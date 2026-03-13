#define _DEFAULT_SOURCE

#ifdef NTREE_AVX512VL
#include <immintrin.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>

#include <search.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

/* libbsd */
#ifdef LIBBSD_OVERLAY
#include <sys/time.h>
#include <sys/queue.h>
#else
/*
 * Only macro functions are used from these headers. Since they cannot be used
 * with mingw, the macros are copied and pasted in "bsd.h"
 */
#include "bsd.h"
#endif

#define PROGNAME "ntree128"

/*
 * NTREE_MEMCMP
 * NTREE_CHAINCMP
 * NTREE_AVX512VL
 */

struct key {
	LIST_ENTRY(key) entries;

	union {
		uint64_t n[2];
		struct {
			dev_t dev;
			ino_t ino;
		};
	} data;
};

LIST_HEAD(listhead, key);
struct listhead head;

static_assert(sizeof(LIST_FIRST(&head)->data) == 16, "sizeof(struct key) != 16");

static int compf (const void *in_a, const void *in_b)
{
	const struct key *a = in_a;
	const struct key *b = in_b;

#if	defined(NTREE_MEMCMP)

	return memcmp(&a->data, &b->data, sizeof(a->data));

#elif	defined(NTREE_AVX512VL)

	/* Little to no performance impact for such data */

	__m128i n1 = _mm_setr_epi64((__m64)a->data.n[0], (__m64)a->data.n[1]);
	__m128i n2 = _mm_setr_epi64((__m64)b->data.n[0], (__m64)b->data.n[1]);
	__mmask8 ml, mg;

	ml = _mm_cmp_epu64_mask(n1, n2, _MM_CMPINT_LT);
	mg = _mm_cmp_epu64_mask(n1, n2, _MM_CMPINT_GT);
	if (ml & 1) {
		return -1;
	}
	if (mg & 1) {
		return 1;
	}
	if (ml & 2) {
		return -1;
	}
	if (mg & 2) {
		return 1;
	}
	return 0;

#else

#ifndef NTREE_CHAINCMP
#warning "Define one of NTREE_CHAINCMP, NTREE_AVX512VL, or NTREE_MEMCMP. NTREE_CHAINCMP assumed"
#endif

	for (size_t i = 0; i < sizeof(a->data.n) / sizeof(a->data.n[0]); i += 1) {
		if	(a->data.n[i] < b->data.n[i]) {
			return -1;
		}
		else if (a->data.n[i] > b->data.n[i]) {
			return 1;
		}
	}
	return 0;

#endif
}

void *tree;
struct {
	size_t tree;
	size_t input;
} count;

long pagesize;

static struct key * new_key (void)
{
	struct key *ret = calloc(1, sizeof(struct key));

	if (ret == NULL) {
		return NULL;
	}

	LIST_INSERT_HEAD(&head, ret, entries);
	count.tree += 1;

	return ret;
}

static void free_all (void)
{
	struct key *next = LIST_FIRST(&head);;

	while (next != NULL) {
		struct key *cur = next;
		next = LIST_NEXT(next, entries);

		tdelete(cur, &tree, compf);
		free(cur);
	}
	assert(tree == NULL);
	LIST_INIT(&head);
	count.tree = 0;
}

static void takeback (void)
{
	struct key *c = LIST_FIRST(&head);

	assert(count.tree > 0 && LIST_FIRST(&head) != NULL);

	LIST_REMOVE(c, entries);
	free(c);
	count.tree -= 1;
}

void walkf (const void *nodep, VISIT which, int depth)
{
	if (which == postorder || which == leaf) {
		struct key *k = *(struct key **)nodep;
		printf("%"PRIu64" %"PRIu64"\n", (uint64_t)k->data.dev, (uint64_t)k->data.ino);
	}
}

static void loadf (FILE *f, const char *path)
{
	char line[256];
	uint64_t a, b;
	size_t i;
	struct key *newn, **old;

	for (i = 0; fgets(line, sizeof(line), f) != NULL; i += 1) {
		count.input += 1;

		a = b = 0;
		if (sscanf(line, "%"SCNu64" %"SCNu64, &a, &b) != 2) {
			fprintf(stderr, PROGNAME": %s:%zu %s\n", path, i, strerror(EINVAL));
			goto err;
		}

		newn = new_key();
		if (newn == NULL) {
			perror(PROGNAME);
			goto err;
		}
		newn->data.dev = (dev_t)a;
		newn->data.ino = (ino_t)b;

		old = tsearch(newn, &tree, compf);
		if (old == NULL) {
			perror(PROGNAME);
			goto err;
		}
		if (newn != *old) {
			takeback();
		}
	}

	return;
err:
	exit(EXIT_FAILURE);
}

static void load_path (const char *path)
{
	FILE *f;

	f = fopen(path, "r");
	if (f == NULL) {
		fprintf(stderr, PROGNAME": %s: %s\n", path, strerror(errno));
		exit(EXIT_FAILURE);
	}

	loadf(f, path);

	fclose(f);
}

int main (int argc, char *argv[])
{
	static struct timespec tp[3];
	LIST_INIT(&head);

#if defined(sysconf) && defined(_SC_PAGESIZE)
	pagesize = sysconf(_SC_PAGESIZE);
#else
	/* No one uses Windows with hugepages, anyway */
	pagesize = 4096;
#endif
	assert(pagesize > 0);

	clock_gettime(CLOCK_THREAD_CPUTIME_ID, tp + 0);
	if (argc == 1) {
		loadf(stdin, "-");
	}
	else {
		for (int i = 1; i < argc; i += 1) {
			if (strcmp(argv[i], "-") == 0) {
				loadf(stdin, "-");
			}
			else {
				load_path(argv[i]);
			}
		}
	}
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, tp + 1);

	timespecsub(tp + 1, tp + 0, tp + 2);
	fprintf(stderr, PROGNAME": input: %zu, tree: %zu, time: %ld.%09lds\n",
		count.input, count.tree, (long)tp[2].tv_sec, tp[2].tv_nsec);

	twalk(tree, walkf);

	clock_gettime(CLOCK_THREAD_CPUTIME_ID, tp + 0);
	free_all();
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, tp + 1);

	timespecsub(tp + 1, tp + 0, tp + 2);
	fprintf(stderr, PROGNAME": delete time: %ld.%09lds\n",
		(long)tp[2].tv_sec, tp[2].tv_nsec);

	return 0;
}
