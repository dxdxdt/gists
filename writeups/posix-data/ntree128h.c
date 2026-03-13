#define _DEFAULT_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <limits.h>
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

#define PROGNAME "ntree128h"

struct key {
	union {
		uint64_t n[2];
		struct {
			dev_t dev;
			ino_t ino;
		};
	} data;
};

struct key *arr;

static int compf (const void *in_a, const void *in_b)
{
	const struct key *a = &arr[(size_t)in_a];
	const struct key *b = &arr[(size_t)in_b];

	for (size_t i = 0; i < sizeof(a->data.n) / sizeof(a->data.n[0]); i += 1) {
		if	(a->data.n[i] < b->data.n[i]) {
			return -1;
		}
		else if (a->data.n[i] > b->data.n[i]) {
			return 1;
		}
	}
	return 0;
}

void *tree;

struct {
	size_t input;
	size_t len;
	size_t size;
} count;
#define ALLOC_SIZE (2 * 1024 * 1024)
static_assert(ALLOC_SIZE % 4096 == 0, "ALLOC_SIZE not aligned to 4k");
static_assert(ALLOC_SIZE % sizeof(struct key) == 0, "ALLOC_SIZE not aligned to struct key");

static ssize_t new_element (void)
{
	if (count.size <= count.len) {
		const size_t newsize = count.size + ALLOC_SIZE / sizeof(struct key);
		const size_t newalloc = newsize * sizeof(struct key);
		void *newm;

		if (newsize > newalloc || newsize > SSIZE_MAX) {
			errno = ENOMEM;
			return -1;
		}
		newm = realloc(arr, newalloc);
		if (newm == NULL) {
			return -1;
		}

		arr = newm;
		count.size = newsize;
	}

	return count.len++;
}

static void free_all (void)
{
	for (size_t i = 0; i < count.len; i += 1) {
		tdelete((const void*)i, &tree, compf);
	}
	assert(tree == NULL);
	free(arr);
	arr = NULL;
	count.size = count.len = 0;
}

static void takeback (void)
{
	assert(count.len > 0 && arr != NULL);
	count.len -= 1;
}

void walkf (const void *nodep, VISIT which, int depth)
{
	if (which == postorder || which == leaf) {
		const struct key *k = &arr[(size_t)(*(void **)nodep)];
		printf("%"PRIu64" %"PRIu64"\n", (uint64_t)k->data.dev, (uint64_t)k->data.ino);
	}
}

static void loadf (FILE *f, const char *path)
{
	char line[256];
	uint64_t a, b;
	size_t i;
	ssize_t newn, *old;
	struct key *newk;

	for (i = 0; fgets(line, sizeof(line), f) != NULL; i += 1) {
		count.input += 1;

		a = b = 0;
		if (sscanf(line, "%"SCNu64" %"SCNu64, &a, &b) != 2) {
			fprintf(stderr, PROGNAME": %s:%zu %s\n", path, i, strerror(EINVAL));
			goto err;
		}

		newn = new_element();
		if (newn < 0) {
			perror(PROGNAME);
			goto err;
		}

		newk = &arr[newn];
		newk->data.dev = (dev_t)a;
		newk->data.ino = (ino_t)b;

		old = tsearch((const void*)newn, &tree, compf);
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
		count.input, count.len, (long)tp[2].tv_sec, tp[2].tv_nsec);

	twalk(tree, walkf);

	clock_gettime(CLOCK_THREAD_CPUTIME_ID, tp + 0);
	free_all();
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, tp + 1);

	timespecsub(tp + 1, tp + 0, tp + 2);
	fprintf(stderr, PROGNAME": delete time: %ld.%09lds\n",
		(long)tp[2].tv_sec, tp[2].tv_nsec);

	return 0;
}
