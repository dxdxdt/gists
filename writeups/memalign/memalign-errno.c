/*
 * Shows whether the platform provided posix_memalign() sets errno in ENOMEM
 * conditions. POSIX says nothing about the function setting errno and
 * implementations may or may not set it.
 *
 * Just note: in case of Musl, there's a case where EINVAL is only returned
 * whilst errno is not set. We're not testing that. We're only testing ENOMEM
 * behaviour because a smart userspace program would never call posix_memalign()
 * in a way that would result in EINVAL.
 */
#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <unistd.h>

int main (void)
{
	const long pagesize = sysconf(_SC_PAGESIZE);
	/*
	 * (two thirds of the whole address space)
	 *
	 * In some cases, this could be actually a reasonable amount of memory
	 * available to userspace in 32-bit address space...
	 * Welp, it is what it is.
	 */
	const size_t req_len = (SIZE_MAX / (size_t)pagesize / 3 * 2) * (size_t)pagesize;
	void *m = NULL;
	int saved, err;

	assert(pagesize > 0);

	errno = 0;
	err = posix_memalign(&m, (size_t)pagesize, req_len);
	saved = errno;

	printf("posix_memalign(0x%" PRIxPTR ", %zu, %zu): %d(%s)\n",
			(uintptr_t)&m, (size_t)pagesize, (size_t)req_len, err, strerror(err));
	printf("errno: %d(%s)\n", saved, strerror(saved));
	printf("m: 0x%" PRIxPTR "\n", (uintptr_t)m);

	free(m);
	return 0;
}
