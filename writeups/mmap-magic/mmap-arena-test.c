#include "mmagic.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include <unistd.h>
#include <sys/stat.h>

static void fill_bytes(void *addr, size_t len, const int bval)
{
	addr = (void*)((uintptr_t)addr + len);
	/* Intentionally using the second virtual page to catch unusual bugs. */
	memset(addr, bval, len);
}

static void check_bytes(const void *addr, size_t len, const int bval)
{
	void *tmp = malloc(len);
	int ret;

	memset(tmp, bval, len);
	ret = memcmp(addr, tmp, len);
	free(tmp);

	assert(ret == 0);
	(void)ret;
}

static void getfilesize(const int fd, off_t *size, off_t *blkcnt)
{
	struct stat st;
	int ret;

	st.st_size = -1;
	ret = fstat(fd, &st);
	assert(ret == 0 && st.st_size >= 0);

	if (size != NULL)
		*size = st.st_size;
	if (blkcnt != NULL)
		*blkcnt = st.st_blocks;
}

#define myassert(expr) \
	do { if (!(expr)) perror(NULL); assert(expr); } while (0)

struct my_extent {
	void *m;
	size_t len;
	size_t size;
	int flags;
};

int main(int argc, char *argv[])
{
	struct mmem_arena arena;
	struct my_extent extents[3] = { 0, };
	char flags_str[256];
	struct my_extent *e;
	off_t fsize[2], sum;
	bool bret;

	fprintf(stderr, "pagesize: %ld\n", sysconf(_SC_PAGESIZE));

	mmem_arena_init(&arena);
	bret = mmem_arena_open(&arena, NULL);
	myassert(bret);

	/* Tetris */

	for (int i = 0, bval = 0x0A; i < 3; i++, bval++) {
		e = extents + i;

		e->flags = 0;
		e->m = mmem_arena_req(&arena, 1, &e->len, &e->size, &e->flags);
		myassert(e->m != NULL);
		fill_bytes(e->m, e->len, bval);
	}
	assert(arena.cnt == 3);

	e = extents + 1;

	flags_str[0] = 0;
	memfile_flags_name(e->flags, sizeof(flags_str), flags_str);
	fprintf(stderr, "req: %s\n", flags_str);

	getfilesize(arena.fd, NULL, fsize + 0);
	e->flags = 0;
	mmem_arena_rm(&arena, e->m, &e->flags);
	if (!(e->flags & MEMFILE_DEALLOC))
		perror("MEMFILE_DEALLOC tetris");
	getfilesize(arena.fd, NULL, fsize + 1);
	assert(arena.cnt == 2);
	check_bytes(extents[0].m, extents[0].len, 0x0A);
	check_bytes(extents[2].m, extents[2].len, 0x0C);

	flags_str[0] = 0;
	memfile_flags_name(e->flags, sizeof(flags_str), flags_str);
	/*
	 * In FreeBSD, this is not reported in 512-byte units but as the number
	 * of pages. For some reason, this seems to be a deliberate design
	 * decision, not a quirk.
	 *
	 * Link: https://reviews.freebsd.org/D37097
	 *
	 * In NetBSD, this is always zero if the backing file is memfd.
	 */
	fprintf(stderr, "rm: %lld -> %lld %s\n", (long long)fsize[0], (long long)fsize[1],
			flags_str);
	if (e->flags & MEMFILE_DEALLOC)
		assert(fsize[0] > fsize[1]);

	e->flags = 0;
	e->m = mmem_arena_req(&arena, 1, &e->len, &e->size, &e->flags);
	myassert(e->m != NULL);
	fill_bytes(e->m, e->len, 0xFF);

	check_bytes(extents[0].m, extents[0].len, 0x0A);
	check_bytes(extents[1].m, extents[1].len, 0xFF);
	check_bytes(extents[2].m, extents[2].len, 0x0C);

	/* ftruncate() behaviour */

	getfilesize(arena.fd, fsize + 0, NULL);
	mmem_arena_rm(&arena, extents[0].m, NULL);
	mmem_arena_rm(&arena, extents[2].m, NULL);
	getfilesize(arena.fd, fsize + 1, NULL);
	assert(arena.cnt == 1);
	assert(fsize[0] > fsize[1]);
	assert(fsize[1] > 0);

	fsize[0] = fsize[1];
	mmem_arena_clear(&arena, true);
	getfilesize(arena.fd, fsize + 1, NULL);
	assert(arena.cnt == 0 && LIST_EMPTY(&arena.head));
	assert(fsize[0] > fsize[1]);
	assert(fsize[1] == 0);

	/* Grow and remove forwards */

	sum = 0;
	for (int i = 0, bval = 0xA0; i < 3; i++, bval++) {
		e = extents + i;

		e->flags = 0;
		e->m = mmem_arena_req(&arena, 1, &e->len, &e->size, &e->flags);
		myassert(e->m != NULL);

		fill_bytes(e->m, e->len, bval);
		sum += e->len;
	}
	getfilesize(arena.fd, fsize + 0, NULL);
	assert(arena.cnt == 3);
	assert(fsize[0] == sum);
	check_bytes(extents[0].m, extents[0].len, 0xA0);
	check_bytes(extents[1].m, extents[1].len, 0xA1);
	check_bytes(extents[2].m, extents[2].len, 0xA2);

	for (int i = 0, bval = 0xA0; i < 3; i++, bval++) {
		e = extents + i;
		bret = mmem_arena_rm(&arena, e->m, NULL);
		assert(bret);

		for (int j = i + 1, bval1 = bval + 1; j < 3; j++, bval1++)
			check_bytes(extents[j].m, extents[j].len, bval1);
	}
	getfilesize(arena.fd, fsize + 1, NULL);
	assert(arena.cnt == 0 && LIST_EMPTY(&arena.head));
	assert(fsize[1] == 0);

	/* Grow forward, and remove backwards */

	for (int i = 0; i < 3; i++) {
		e = extents + i;

		e->flags = 0;
		e->m = mmem_arena_req(&arena, 1, &e->len, &e->size, &e->flags);
		myassert(e->m != NULL);
	}

	for (int i = 2; i >= 0; i--) {
		e = extents + i;
		bret = mmem_arena_rm(&arena, e->m, &e->flags);
		assert(bret);

		if (!(e->flags & MEMFILE_DEALLOC))
			perror("MEMFILE_DEALLOC backwards");
	}

	mmem_arena_close(&arena, true);
	return 0;
}
