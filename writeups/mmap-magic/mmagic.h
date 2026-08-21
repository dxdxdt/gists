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
#define MEMFILE_NO_INHERIT	0x01
#define MEMFILE_DEALLOC		0x02
#define MEMFILE_WRAPGUARD	0x04
#define MEMFILE_NORESERVE	0x08
#define MEMFILE_EXEC		0x10

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

struct mmem_guarded;

int mkmemfile(int *flags);
void *mmemfile(int fd, off_t ofs, size_t req, size_t *len, size_t *size, int *flags);
void *mmemfile_aligned(int fd, off_t ofs, size_t len, int *flags);
bool mmemfile_align_size(size_t req, size_t *out);

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

/*
 * Set up a Guarded Memory Area(GMA) for testing and debugging memory errors
 *
 * Set up pages in the layout of: |---|rw-|---|, the 1st and 3rd region as
 * guard regions, the 2nd region read-write. The function returns a pointer to
 * an opaque object that can be freed with mmem_free_guarded() on success. The
 * pointer `near` is set to the start of the 2nd region and the pointer `far` is
 * set to the address offset from the start of the 3rd region by `size`. Both
 * pointers are nullable and the allocation of pages will be done regardless of
 * whether both pointers are given NULL or not. If `region` is not NULL, it is
 * set to the `size` rounded up to the system page size which is the actual size
 * of the 2nd region.
 *
 * The idea is that if the memory is accessed past or below the allocated size
 * (either by reading or writing), the process will hit the guard region(ie.
 * buffer overrun or underrun) and SIGSEGV will be raised. In order to catch
 * both underrun and overrun cases, run the functionality to test twice using
 * `far` and `near`, respectively.
 *
 * Other than the system page size and page boundaries, the function does not
 * care about the alignment. The `near` pointer will always be aligned because
 * it's at a page boundary. But in case of the pointer `far`, the caller will
 * have to take extra care to not cause any unaligned memory access. Generally,
 * strings types including char, wchar_t, wint_t, UTF-16 and UTF-32 do not
 * require alignment and C structures are naturally aligned by default. However,
 * for packed structures, the caller will have to take extra care so that the
 * size is aligned to at least the system word size. Always use alignas for
 * structures to guard the code against change if available(C11 or later), or
 * use the convenience macro function MMEM_ALIGN_DEF().
 *
 * On failure, the function returns NULL and errno is set to indicate the error.
 *
 * NOTE:
 *
 * The guard regions(1st and 3rd) will be mapped with the flag MAP_NORESERVE if
 * available on the platform. Unfortunately, if the flag is not available, total
 * of `PAGESIZE * 2 + actual` bytes of memory will be used.
 *
 * On Linux: effectively, only the memory space for the 2nd region is reserved
 * and the rest of the pages will point to the same PFN(physical page)
 *
 * See also:
 *
 *   - mmap(2)
 *   - pthread_attr_setguardsize(3)
 *   - "Linux Memory Types" in top(1)
 *   - The Linux kernel documentation on vm.overcommit sysctl
 *   - https://lwn.net/Articles/1011366/
 */
struct mmem_guarded *mmem_alloc_guarded(size_t size, size_t *region,
		void **near, void **far, int *flags);
/*
 * Free the GMA object
 *
 * The function is no-op if `obj` is NULL.
 */
void mmem_free_guarded(struct mmem_guarded *obj);

#define _MMEM_DEF_ALIGN (sizeof(long long))
/* Align the size to the natural system alignment(`sizeof(long long)`) */
#define MMEM_ALIGN_DEF(SIZE) \
	((SIZE) % _MMEM_DEF_ALIGN == 0 ? \
		(SIZE) : \
		((SIZE) / _MMEM_DEF_ALIGN + 1) * _MMEM_DEF_ALIGN)

/* misc. */
const char *memfile_backing_name(const int type);
int memfile_flags_name(const int flags, const size_t len, char *out);
int dealloc_file_range(int fd, off_t ofs, off_t len);

#endif
