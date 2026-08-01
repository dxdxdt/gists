/*
 * Test case for virtual address aliasing
 *
 * On some old architectures where MMU is implemented using VIVT(virtually
 * indexed, virtually tagged), aliasing a physical page(mapping it to multiple
 * virtual pages) may result in cache incoherency. This problem is referred to
 * as the "virtual address aliasing" or "virtual address color" problem.
 *
 * x86 never implemented address translation using VIVT and employed VIPT since
 * the first model of processor with MMU(80386). Therefore, this test is only
 * meaningful on very very old systems like ARMv5 or older and some historic
 * MIPS, SPARC, SH4 processors.
 *
 * Link: https://docs.altera.com/r/docs/683620/current/nios-ii-classic-processor-reference-guide/virtual-address-aliasing
 * Link: https://community.intel.com/t5/Intel-Moderncode-for-Parallel/virtual-address-aliasing/m-p/812652
 * Link: https://stackoverflow.com/questions/19039280/physical-or-virtual-addressing-is-used-in-processors-x86-x86-64-for-caching-in-t
 */

#include "mmagic.h"
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

/* Two cache lines */
struct placeholder {
	int a;
	long long hole[7];
	int b;
};

__attribute__((noinline))
static void do_access(struct placeholder *a, struct placeholder *b)
{
	a->b = -1;
	printf("normal: %d\n", b->b);
	assert(a->b == b->b);
}

__attribute__((noinline))
static void do_access_volatile(volatile struct placeholder *a, volatile struct placeholder *b)
{
	a->b = -1;
	printf("volatile: %d\n", b->b);
	assert(a->b == b->b);
}

int main(void)
{
	int fd;
	void *addr;
	size_t len;
	struct placeholder *a, *b;

	fd = mkmemfile(NULL);
	assert(fd >= 0);

	addr = mmemfile(fd, 0, 1, &len, NULL, NULL);
	assert(addr != NULL);
	assert(len >= sizeof(*a));

	a = addr;
	b = (void*)((uintptr_t)addr + len);

	/*
	 * These two should look identical in asm, regardless of -O level.
	 * This is to show that, if cache incoherency occurs, it is not from how
	 * the compiler produces the code. There's not much that can be done
	 * from userspace(at least in a portable manner whilst staying
	 * unprivileged, anyways).
	 */
	do_access(a, b);
	do_access_volatile(a, b);

	return 0;
}
