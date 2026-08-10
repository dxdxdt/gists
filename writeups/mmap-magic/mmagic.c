#include "mmagic.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/queue.h>

#ifdef MFD_CLOEXEC
#define HAS_MEMFD 1
#endif

static long pagesize = -1;

#ifdef __NetBSD__
extern int fdiscard(int fd, off_t pos, off_t length) __attribute__((weak));
#endif
extern int posix_fallocate(int fd, off_t pos, off_t length) __attribute__((weak));

static inline void cache_pagesize(void)
{
	if (pagesize > 0)
		return;

	pagesize = sysconf(_SC_PAGESIZE);
	assert(pagesize > 0);
}

static inline bool aligned_val(size_t x, size_t alignment)
{
	alignment -= 1;
	return (x & alignment) == 0;
}

static inline size_t align_up(size_t x, const size_t alignment, bool *out_ovf)
{
	size_t ret;
	bool ovf = false;

	if (aligned_val(x, alignment))
		ret = x;
	else {
		ret = (x & ~(alignment - 1));
		ret += alignment;
		ovf = ret < x;
	}

	if (out_ovf != NULL)
		*out_ovf = ovf;

	return ret;
}

/* Linux 3.17, glibc 2.27 */
#ifdef HAS_MEMFD
static bool mkmemfile_memfd(const char *name, int *fd)
{
	*fd = memfd_create(name, 0);
	return *fd >= 0;
}
#endif

/* Fallback for other BSDs and legacy Unices */
static bool mkmemfile_shm(const char *name, int *fd)
{
	*fd = shm_open(name, O_RDWR|O_CREAT|O_TRUNC, 0000);
	if (*fd >= 0) {
		shm_unlink(name);
		return true;
	}
	return false;
}

struct memfile_impl_tuple {
	bool(*fn)(const char *name, int *fd);
	const char *name;
	int type;
};

static const struct memfile_impl_tuple MKMEMFILE_IMPL_TABLE[] = {
#ifdef HAS_MEMFD
	{ .fn = mkmemfile_memfd, .name = "memfd", .type = MEMFILE_BACKING_MFD, },
#endif
	{ .fn = mkmemfile_shm, .name = "shm", .type = MEMFILE_BACKING_SHM, },
	{ },
};


int mkmemfile(int *flags)
{
	const int in_flags = flags == NULL ? 0 : *flags;
	int target_types = in_flags & MEMFILE_BACKING_TYPE;
	unsigned long r[2] = { ULONG_MAX, ULONG_MAX, };
	char name[256];
	int fd;
	bool tried;

	if (target_types == 0)
		target_types = MEMFILE_BACKING_TYPE;

	/* Quick way to get a 128-bit random number */
	fd = open("/dev/urandom", O_RDONLY);
	if (fd >= 0) {
		read(fd, r, sizeof(r));
		close(fd);
	}
	fd = -1;

	/*
	 * When mkmemfile_shm() is chosen, there's a very slight chance of name
	 * collision leading to undefined behaviour from multiple process using
	 * the same file simultaneously. That's why memfd_create() is safer
	 * alternative.
	 *
	 * shm_open() is used as a fallback when we don't actually need a region
	 * of memory shared across multiple processes. There's no other portable
	 * alternative - mmap() requires a file descriptor. Memory-backed file
	 * systems like tmpfs are only implementations, not spec.
	 */

	name[0] = 0;
	snprintf(name, sizeof(name), "/mmagic_%08x:%016lx%016lx",
			(unsigned int)getpid(), r[0], r[1]);
	assert(name[0] != 0);

	tried = false;
	for (const struct memfile_impl_tuple *tpl = MKMEMFILE_IMPL_TABLE; tpl->fn != NULL; tpl++) {
		if (!(target_types & tpl->type))
			continue;

		tried = true;
		if (tpl->fn(name, &fd)) {
			if (flags != NULL) {
				*flags &= ~MEMFILE_BACKING_TYPE;
				*flags |= tpl->type;
			}
			return fd;
		}
	}

	if (!tried)
		errno = ENOTSUP;
	return -1;
}

void *mmemfile(int fd, off_t ofs, size_t req, size_t *out_len, size_t *out_size, int *in_flags)
{
	size_t len, size;
	void *ret;
	bool ovf = false;

	if (req == 0)
		return NULL;

	cache_pagesize();

	len = align_up(req, pagesize, &ovf);
	/*
	 * Thread-safety:
	 *
	 * Map the range twice as much initially, then replace the the second
	 * half afterwards by mapping it over with the range same as the first
	 * half. This is to reserve the virtual address range first so that:
	 *
	 *   1. the second half range is not taken by other threads in between
	 *      mmap() calls
	 *   2. the new mapping won't inadvertently replace existing mappings
	 *      like libc and the program code and data segments themselves
	 *
	 * The existing address range is replaced by the new mapping if mapped
	 * over. This behaviour is guaranteed in POSIX.
	 */
	size = len * 2;
	if (ovf || size <= len) {
		errno = EOVERFLOW;
		return NULL;
	}

	ret = mmemfile_aligned(fd, ofs, len, in_flags);
	if (ret != NULL) {
		if (out_len != NULL)
			*out_len = len;
		if (out_size != NULL)
			*out_size = size;
	}

	return ret;
}

static inline bool do_clear_inherit(void *addr, size_t size)
{
	bool ret = false;

#if defined(MADV_DONTFORK)	/* Linux */
	ret = madvise(addr, size, MADV_DONTFORK) == 0;
#elif defined(INHERIT_NONE)	/* FreeBSD */
	ret = minherit(addr, size, INHERIT_NONE) == 0;
#elif defined(VM_INHERIT_NONE)	/* macos */
	ret = minherit(addr, size, VM_INHERIT_NONE) == 0;
#elif defined(MAP_INHERIT_NONE)	/* the rest */
	ret = minherit(addr, size, MAP_INHERIT_NONE) == 0;
#else
	errno = ENOSYS;
#endif

	return ret;
}

/*
 * Basically a knock-off implementation of posix_fallocate(), with the
 * assumption that the block size is the system memory page size.
 */
static inline bool do_prefault_fallback(int fd, off_t ofs, off_t len)
{
	ssize_t err;

	assert(pagesize > 0);

	while (len > 0) {
		err = pwrite(fd, "", 1, ofs);
		assert(err != 0);
		if (err < 0)
			return false;

		ofs += pagesize;
		len -= pagesize;
	}

	return true;
}

static inline bool do_prefault(int fd, off_t ofs, off_t len)
{
	/*
	 * If the platform provides an efficient allocation scheme, try to use
	 * that.
	 */
	if (posix_fallocate != NULL && posix_fallocate(fd, ofs, len) == 0)
		return true;
	/*
	 * By the book, posix_fallocate() can fail with EINTR, but the modern
	 * implementations do SA_RESTART on regular files. On a strange platform
	 * where posix_fallocate() can indeed fail with EINTR, the second line
	 * of defence is the following fallback. (basically, we're calling
	 * posix_fallocate() twice)
	 */
	return do_prefault_fallback(fd, ofs, len);
}

void *mmemfile_aligned(int fd, off_t ofs, size_t len, int *in_flags)
{
	int out_flags = in_flags == NULL ? 0 : *in_flags;
	void *a, *b;
	uintptr_t second;
	int prot, flags;
	size_t size;
	off_t flen;
	bool grown;

	cache_pagesize();

	if (ofs < 0 || !aligned_val(ofs, pagesize) || !aligned_val(len, pagesize)) {
		errno = EINVAL;
		return NULL;
	}
	if (len == 0)
		return NULL;

	size = len * 2;
	/*
	 * Cater to tantrums of NetBSD and macos:
	 *
	 * memfd_create() appeared in NetBSD 11. However, its implementation(
	 * memfd_mmap() in sys/kern/sys_memfd.c), the size of the file is
	 * checked to ensure that the region being mmap()'d exists. If not, it
	 * fails with EINVAL. This is undocumented behaviour.
	 *
	 * It is noted that XNU kernel exhibits the same behaviour during the
	 * early stages of development albeit the support has been dropped due
	 * to other various reasons.
	 */
	ofs += size;
	/*
	 * Well, the C standard says the wrap of signed integers is undefined
	 * behaviour, but that's because it has to support non-2's complement
	 * systems. All the systems running today use 2's complement to
	 * represent negative integers.
	 */
	if (size < len || ofs < 0) {
		errno = EOVERFLOW;
		return NULL;
	}

	flen = lseek(fd, 0, SEEK_END);
	if (flen < 0)
		return NULL;

	grown = flen < ofs;
	if (grown) {
		if (ftruncate(fd, ofs))
			return NULL;
	}
	ofs -= size;

	if (!do_prefault(fd, ofs, len)) {
		if (grown)
			ftruncate(fd, flen);
		return NULL;
	}

	prot = PROT_READ|PROT_WRITE;
	flags = MAP_SHARED;
	a = mmap(NULL, size, prot, flags, fd, ofs);
	if (a == MAP_FAILED)
		return NULL;
	assert(a != NULL);

	flags = MAP_SHARED|MAP_FIXED;
	second = (uintptr_t)a;
	second += len;
	b = mmap((void*)second, len, prot, flags, fd, ofs);
	if (b == MAP_FAILED) {
		munmap(a, size);
		if (grown)
			ftruncate(fd, flen);
		return NULL;
	}
	if (grown)
		ftruncate(fd, ofs + len);

	/* Guard against accidental use after fork() */
	if (do_clear_inherit(a, size))
		out_flags |= MEMFILE_NO_INHERIT;
	else
		out_flags &= ~MEMFILE_NO_INHERIT;

	if (in_flags != NULL)
		*in_flags = out_flags;
	return a;
}

void mmem_arena_init(struct mmem_arena *a)
{
	memset(a, 0, sizeof(*a));
	a->fd = -1;
	LIST_INIT(&a->head);
}

bool mmem_arena_open(struct mmem_arena *a, int *flags)
{
	if (a->fd >= 0) {
		errno = EEXIST;
		return false;
	}

	a->fd = mkmemfile(flags);
	return a->fd >= 0;
}

void mmem_arena_close(struct mmem_arena *a, const bool unmap)
{
	mmem_arena_clear(a, unmap);

	if (a->fd >= 0) {
		close(a->fd);
		a->fd = -1;
	}
}

void mmem_arena_clear(struct mmem_arena *a, const bool unmap)
{
	struct mmem_extent *cur, *next;

	cur = LIST_FIRST(&a->head);
	while (cur != NULL) {
		next = LIST_NEXT(cur, entries);

		if (unmap)
			munmap(cur->m, cur->size * 2);
		free(cur);
		cur = next;
	}
	LIST_INIT(&a->head);

	if (a->fd >= 0)
		ftruncate(a->fd, 0);

	a->cnt = 0;
}

void *mmem_arena_req(struct mmem_arena *a, size_t req, size_t *out_len,
		size_t *out_size, int *flags)
{
	struct mmem_extent *cur, *last = NULL, *newm;
	off_t t_ofs = 0, gap;
	size_t len;
	bool ovf = false;

	if (req == 0)
		return NULL;

	cache_pagesize();
	len = align_up(req, pagesize, &ovf);
	if (ovf || a->cnt == UINT_MAX) {
		errno = ENOMEM;
		return NULL;
	}

	newm = calloc(1, sizeof(*newm));
	if (newm == NULL)
		return NULL;

	LIST_FOREACH(cur, &a->head, entries) {
		last = cur;

		gap = cur->ofs - t_ofs;
		assert(gap >= 0);
		if (gap >= (off_t)req)
			break;

		t_ofs = cur->ofs + cur->size;
	}

	if (cur == NULL && last != NULL)
		/* No gap found but there's at least one entry */
		t_ofs = last->ofs + last->size;

	newm->m = mmemfile_aligned(a->fd, t_ofs, len, flags);
	if (newm->m == NULL)
		goto bail;
	newm->ofs = t_ofs;
	newm->size = len;

	if (cur == NULL) {
		if (last == NULL)
			LIST_INSERT_HEAD(&a->head, newm, entries);
		else
			LIST_INSERT_AFTER(last, newm, entries);
	} else
		LIST_INSERT_BEFORE(cur, newm, entries);

	if (out_len != NULL)
		*out_len = len;
	if (out_size != NULL)
		*out_size = len * 2;

	a->cnt++;
	return newm->m;
bail:
	free(newm);
	return NULL;
}

#ifdef __NetBSD__
extern int fdiscard(int fd, off_t pos, off_t length) __attribute__((weak));
#endif

int dealloc_file_range(int fd, off_t ofs, off_t len)
{
	int ret = -1;

#if defined(FALLOC_FL_PUNCH_HOLE) && defined(FALLOC_FL_KEEP_SIZE)
	/*
	 * NOTE: the support for tmpfs was added in Linux 3.5, so the
	 * fact that the macro definitions exist does not necessarily
	 * mean tmpfs actually has the support
	 */
	ret = fallocate(fd, FALLOC_FL_PUNCH_HOLE|FALLOC_FL_KEEP_SIZE, ofs, len);
#elif defined(SPACECTL_DEALLOC)
	const struct spacectl_range rqsr = {
		.r_offset = ofs,
		.r_len = len,
	};

	ret = fspacectl(fd, SPACECTL_DEALLOC, &rqsr, 0, NULL);
#elif defined(__NetBSD__)
	if (fdiscard != NULL)
		ret = fdiscard(fd, ofs, len);
#else
	errno = ENOSYS;
#endif

	return ret;
}

bool mmem_arena_rm(struct mmem_arena *a, void *m, int *flags)
{
	struct mmem_extent *cur, *one_before = NULL, *found = NULL;
	int out_flags = flags == NULL ? 0 : *flags;
	int err;

	(void)err;
	if (m == NULL)
		return true;

	LIST_FOREACH(cur, &a->head, entries) {
		if (cur->m == m) {
			found = cur;
			break;
		}

		one_before = cur;
	}

	if (found == NULL) {
		errno = ENOENT;
		return false;
	}
	assert(a->cnt > 0);

	err = munmap(found->m, found->size * 2);
	assert(err == 0);

	if (LIST_NEXT(found, entries) == NULL) {
		/*
		 * When deleting the last entry, truncate the file to the last
		 * extent.
		 *
		 * Edge case: if it was the very last entry with the offset not
		 * at the beginning of the file, the file should become empty.
		 */
		const off_t trunc = one_before == NULL ?
			0 : one_before->ofs + one_before->size;

		err = ftruncate(a->fd, trunc);
	} else {
		/*
		 * Try deallocating the range of the file to save up the pages.
		 * If there's no support or the underlying syscall fails, well
		 * it is what it is.
		 *
		 * Make sure that errno is left intact after this point so that
		 * the caller can figure out what went wrong by inspecting
		 * errno.
		 */
		err = dealloc_file_range(a->fd, found->ofs, found->size);
	}
	if (err == 0)
		out_flags |= MEMFILE_DEALLOC;
	else
		out_flags &= ~MEMFILE_DEALLOC;

	LIST_REMOVE(found, entries);
	free(found);

	if (flags != NULL)
		*flags = out_flags;

	a->cnt--;
	return true;
}

const char *memfile_backing_name(const int type)
{
	const int in_type = MEMFILE_BACKING_TYPE & type;

	for (const struct memfile_impl_tuple *tpl = MKMEMFILE_IMPL_TABLE; tpl->fn != NULL; tpl++) {
		if (tpl->type == in_type)
			return tpl->name;
	}

	return NULL;
}

int memfile_flags_name(const int flags, const size_t len, char *out)
{
	return snprintf(out, len, "%s%s",
			flags & MEMFILE_NO_INHERIT	? "NO_INHERIT "	: "",
			flags & MEMFILE_DEALLOC		? "DEALLOC "	: "");
}
