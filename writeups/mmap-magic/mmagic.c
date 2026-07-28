#include "mmagic.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>

#ifdef MFD_CLOEXEC
#define HAS_MEMFD 1
#endif

/* Linux 3.17, glibc 2.27 */
#ifdef HAS_MEMFD
static inline bool mkmemfile_memfd(const char *name, int *fd)
{
	bool ret;

	*fd = memfd_create(name, 0);
	ret = *fd >= 0;

	if (ret)
		fprintf(stderr, "backing file: memfd_create()\n");
	return ret;
}
#endif

/* Fallback for other BSDs and legacy Unices */
static inline bool mkmemfile_shm(const char *name, int *fd)
{
	*fd = shm_open(name, O_RDWR|O_CREAT|O_TRUNC, 0000);
	if (*fd >= 0) {
		shm_unlink(name);
		fprintf(stderr, "backing file: shm_open()\n");
		return true;
	}
	return false;
}

int mkmemfile(int flags)
{
	static const size_t NAME_SIZE =
#if	defined(__APPLE__)
		/*
		 * SHM_NAME_MAX is not exposed anywhere on macos. The value is
		 * kept secret in the kernel. The manual is incomplete.
		 *
		 * Apple can go fuck themselves.
		 */
		32;
#else
		256;
#endif
	unsigned long r[2] = { ULONG_MAX, ULONG_MAX, };
	char name[NAME_SIZE];
	int ret;
	int fd;

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
	ret = snprintf(name, sizeof(name), "/mmagic_%08x:%016lx%016lx",
			(unsigned int)getpid(), r[0], r[1]);
	assert(name[0] != 0);

	ret =
#ifdef HAS_MEMFD
		(flags & MEMFILE_SHM_ONLY ? false : mkmemfile_memfd(name, &fd)) ||
#endif
		mkmemfile_shm(name, &fd);

	if (!ret)
		return -1;

	return fd;
}

void *mmemfile(const int fd, off_t ofs, const size_t req, size_t *size, size_t *len)
{
	static long psz = -1;
	int err;
	void *a, *b;
	uintptr_t second;
	int prot, flags;

	if (psz < 0) {
		psz = sysconf(_SC_PAGESIZE);
		assert(psz > 0);
	}

	if (ofs < 0) {
		errno = EINVAL;
		return MAP_FAILED;
	}

	/*
	 * Thread-safety:
	 *
	 * Map the range twice as much initially, replace the the second half
	 * afterwards by mapping it over with the range same as the first half.
	 * This is to reserve the virtual address range first so that the second
	 * half range is not taken by other threads in between mmap() calls.
	 *
	 * The existing address range is replaced by the new mapping if mapped
	 * over. This behaviour is guaranteed in POSIX.
	 */

	*len = (req % psz == 0 ? req / psz : (req / psz + 1)) * psz;
	*size = *len * 2;
	/*
	 * Cater to macos's tantrum: could technically use len here, but then
	 * macos refuses to mmap() it. This is an undocumented behaviour.
	 */
	ofs += *size;
	if (*len < req || *size < *len || ofs < 0) {
		errno = EOVERFLOW;
		return MAP_FAILED;
	}

	err = ftruncate(fd, ofs);
	if (err)
		return MAP_FAILED;
	ofs -= *size;

	prot = PROT_READ|PROT_WRITE;
	flags = MAP_SHARED;
	a = mmap(NULL, *size, prot, flags, fd, ofs);
	if (a == MAP_FAILED)
		return MAP_FAILED;

	flags = MAP_SHARED|MAP_FIXED;
	second = (uintptr_t)a;
	second += *len;
	b = mmap((void*)second, *len, prot, flags, fd, ofs);
	if (b == MAP_FAILED) {
		munmap(a, *size);
		return MAP_FAILED;
	}

	/* Cater to macos's tantrum */
	ftruncate(fd, ofs + *len);

	/* Guard against accidental use after fork() */
	errno = ENOSYS;
	err = -1;
#if defined(MADV_DONTFORK)	/* Linux */
	err = madvise(a, *size, MADV_DONTFORK);
#elif defined(INHERIT_NONE)	/* FreeBSD */
	err = minherit(a, *size, INHERIT_NONE);
#elif defined(VM_INHERIT_NONE)	/* macos */
	err = minherit(a, *size, VM_INHERIT_NONE);
#elif defined(MAP_INHERIT_NONE)	/* the rest */
	err = minherit(a, *size, MAP_INHERIT_NONE);
#endif
	if (err)
		perror("mmagic: mmap() inheritance");

	return a;
}
