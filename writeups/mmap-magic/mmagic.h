#ifdef __APPLE__
/*
 * It is decided that it is not possible to implement this in macos because:
 *
 *   1. SHM_NAME_MAX is not exposed anywhere on macos userspace. The value is
 *      kept secret in the kernel(the manual is incomplete)
 *   2. After a successful call to mmap(), all the subsequent ftruncate() with
 *      the file fails with EINVAL. This is probably to preemptively prevent
 *      potential bugs from SIGBUS(this is an undocument feature)
 *   3. If the target region of mmap() in the file is not yet allocated or the
 *      length no set using ftruncate(), the syscall fails with EINVAL(again,
 *      also an undocument feature)
 *
 * As evident from 2 and 3, this is not only a limitation in the implementation
 * but also deliberate design decisions.
 *
 * Apple can go fuck themselves. So defensive and greedy as always have been.
 */
#error "The XNU implementation is so restrictive, the implementation is "\
	"simply not possible. Apple can go fuck themselves."
#endif

#ifndef MMAGIC_H
#define MMAGIC_H
#ifdef __linux__
/* glibc can shut the fuck up */
#undef _BSD_SOURCE
#define _GNU_SOURCE
#else
#define _BSD_SOURCE
#define _NETBSD_SOURCE
#endif
#include <stdbool.h>

#include <sys/types.h>
#include <sys/queue.h>

#define MEMFILE_BACKING_TYPE	0xFF00
#define MEMFILE_BACKING_SHM	0x0100
#define MEMFILE_BACKING_MFD	0x0200
#define MEMFILE_NO_INHERIT	0x1
#define MEMFILE_DEALLOC		0x2

struct mmem_extent {
	size_t ofs;
	size_t size;
	void *m;
	/* Ordered as they appear in the memfile */
	LIST_ENTRY(mmem_extent) entries;
};

LIST_HEAD(mmem_arena_head, mmem_extent);

struct mmem_arena {
	struct mmem_arena_head head;
	unsigned int cnt;
	int fd;
};

int mkmemfile(int *flags);
void *mmemfile(int fd, off_t ofs, size_t req, size_t *len, size_t *size, int *flags);
void *mmemfile_aligned(int fd, off_t ofs, size_t len, int *flags);

/*
 * Simple O(N) page allocator implementation
 *
 * This should scale up to the size of the locality of reference(ie. the
 * processor cache size), usually entries in few thousands. If you wish to scale
 * it up, you'll probably have to make your own implementation or improve this
 * implementation.
 */

void mmem_arena_init(struct mmem_arena *a);
bool mmem_arena_open(struct mmem_arena *a, int *flags);
void mmem_arena_close(struct mmem_arena *a, const bool unmap);
void mmem_arena_clear(struct mmem_arena *a, const bool unmap);
void *mmem_arena_req(struct mmem_arena *a, size_t req, size_t *len, size_t *size, int *flags);
bool mmem_arena_rm(struct mmem_arena *a, void *m, int *flags);

/* misc. */
const char *memfile_backing_name(const int type);
int memfile_flags_name(const int flags, const size_t len, char *out);

#endif
