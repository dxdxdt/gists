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
#else
/*
 * Only macro functions are used from these headers. Since they cannot be used
 * with mingw, the macros are copied and pasted in "bsd.h"
 */
#include "bsd.h"
#endif

#define PROGNAME "nsort"

intmax_t *arr;

static int compf (const void *in_a, const void *in_b)
{
	const intmax_t a = *((const intmax_t*)in_a);
	const intmax_t b = *((const intmax_t*)in_b);

	if (a < b) {
		return -1;
	}
	if (a > b) {
		return 1;
	}
	return 0;
}

struct {
	size_t len;
	size_t size;
} count;
#define ALLOC_SIZE (2 * 1024 * 1024)
static_assert(ALLOC_SIZE % 4096 == 0, "ALLOC_SIZE not aligned to 4k");
static_assert(ALLOC_SIZE % sizeof(arr[0]) == 0, "ALLOC_SIZE not aligned to struct key");

static ssize_t new_element (void)
{
	if (count.size <= count.len) {
		const size_t newsize = count.size + ALLOC_SIZE / sizeof(arr[0]);
		const size_t newalloc = newsize * sizeof(arr[0]);
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
	free(arr);
	arr = NULL;
	count.size = count.len = 0;
}

static void loadf (FILE *f, const char *path)
{
	char line[256];
	intmax_t n;
	size_t i;
	ssize_t newn;

	for (i = 0; fgets(line, sizeof(line), f) != NULL; i += 1) {
		n = 0;
		if (sscanf(line, "%"SCNdMAX, &n) != 1) {
			fprintf(stderr, PROGNAME": %s:%zu %s\n", path, i, strerror(EINVAL));
			goto err;
		}

		newn = new_element();
		if (newn < 0) {
			perror(PROGNAME);
			goto err;
		}

		arr[newn] = n;
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

	clock_gettime(CLOCK_THREAD_CPUTIME_ID, tp + 0);
	qsort(arr, count.len, sizeof(arr[0]), compf);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, tp + 1);
	timespecsub(tp + 1, tp + 0, tp + 2);
	fprintf(stderr, PROGNAME": len: %zu, time: %ld.%09lds\n",
		count.len, (long)tp[2].tv_sec, tp[2].tv_nsec);

	for (size_t i = 0; i < count.len; i += 1) {
		printf("%"PRIdMAX"\n", arr[i]);
	}

	free_all();

	return 0;
}
