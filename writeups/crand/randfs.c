#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <threads.h>
#include <stdatomic.h>
#include <getopt.h>
#include <unistd.h>
#define ARGV0 "randfs" // "rand() Find Seed"
#define LCG_M (214013)
#define LCG_A (2531011)
#define LCG_S (1)
#define LCG_OP &
#define LCG_BITMASK (32767)

#define DIST_STAT false
#define PRINT_ROWS false

static atomic_uintmax_t *st;

/*
 * glibc's stdio functions are thread-safe, but that's not necessarily POSIX. We
 * need to fflush() anyways, so [f]printf() and fflush() are called from the
 * critical section. fflush() shouldn't really be necessary but somehow, glibc
 * seems to be using thread-local variables in stdio functions.
 *
 * GNUism at its finest.
 */
static mtx_t stdlock;

static inline int32_t genrand (uint32_t *seed) {
	*seed = *seed * LCG_M + LCG_A;
	return (*seed >> 16) LCG_OP LCG_BITMASK;
}

typedef uint8_t val_t;
#define PRIval PRIu8
#define SCNval SCNu8

struct {
	long nproc;
	struct {
		uint32_t start;
		uint32_t mod;
		size_t rows;
	} range;
	struct {
		const val_t *arr;
		size_t cols;
	} needle;
} parm = {
	.range.start = 1,
	.range.mod = 45,
	// 6 numbers a row, 5 rows per sheet
	.needle.cols = 6,
	.range.rows = 5,
};

struct wctx {
	uint32_t seed_start;
	uint32_t seed_step;
};

static inline bool in_set (
		const val_t needle,
		const val_t *arr,
		const size_t cnt)
{
	for (size_t i = 0; i < cnt; i += 1) {
		if (arr[i] == needle) {
			return true;
		}
	}
	return false;
}

static bool uniq_set (const val_t *arr, const size_t cnt) {
	for (size_t i = 0; i < cnt; i += 1) {
		for (size_t j = i + 1; j < cnt; j += 1) {
			if (arr[i] == arr[j]) {
				return false;
			}
		}
	}
	return true;
}

// static int compf (const void *in_a, const void *in_b) {
// 	const val_t a = *(const val_t*)in_a;
// 	const val_t b = *(const val_t*)in_b;
// 	return a == b ? 0 : a < b ? -1 : 1;
// }

/*
 * This is the good old selection sort. This faster than calling qsort().
 */
static inline void sort (val_t *arr, const size_t cnt) {
	for (size_t i = 0; i < cnt; i += 1) {
		for (size_t j = i + 1; j < cnt; j += 1) {
			if (arr[i] > arr[j]) {
				const val_t tmp = arr[i];
				arr[i] = arr[j];
				arr[j] = tmp;
			}
		}
	}
}

static inline void inc_stat (const uint32_t n) {
	const uintmax_t prev = atomic_fetch_add_explicit(
			st + n,
			1,
			memory_order_relaxed);
	// oi! overflow!
	// assert(prev != UINTMAX_MAX);
	(void)prev;
}

static inline bool arrcomp (const val_t *a, const val_t *b, const size_t l) {
	return memcmp(a, b, l * sizeof(*a)) == 0;
}

static void report_row (const val_t *arr, const size_t cnt) {
	const int fr = mtx_lock(&stdlock);
	assert(fr == thrd_success);
	(void)fr;

	for (size_t i = 0; i < cnt; i += 1) {
		fprintf(stderr, "%02"PRIval" ", arr[i]);
	}
	fprintf(stderr, "\n");
	fflush(stderr);

	mtx_unlock(&stdlock);
}

static inline bool do_hunt_sel (uint32_t seed) {
	val_t deck[parm.range.mod];
	val_t arr[parm.needle.cols];

	for (size_t y = 0; y < parm.range.rows; y += 1) {
		val_t v;
		uint32_t n;
		size_t r = parm.range.mod;

		v = parm.range.start;
		for (size_t i = 0; i < parm.range.mod; i += 1) {
			deck[i] = v;
			v += 1;
		}

		for (size_t x = 0; x < parm.needle.cols; x += 1) {
			n = genrand(&seed);
			v = n % r;
			if (DIST_STAT) {
				inc_stat(n % parm.range.mod);
			}

			arr[x] = deck[v];
			r -= 1;
			// this could be better
			memmove(deck + v, deck + v + 1, r - v);
		}
		sort(arr, parm.needle.cols);
		// assert(uniq_set(arr, parm.needle.cols));

		if (PRINT_ROWS) {
			report_row(arr, parm.needle.cols);
		}

		if (arrcomp(arr, parm.needle.arr, parm.needle.cols)) {
			return true;
		}
	}

	return false;
}

// https://www.ilyo.co.kr/?ac=article_view&entry_id=30730
// no correlation. this was not the algo used for the POS software
static inline bool do_hunt_mod (uint32_t seed) {
	val_t arr[parm.needle.cols];

	for (size_t y = 0; y < parm.range.rows; y += 1) {
		for (size_t x = 0; x < parm.needle.cols; x += 1) {
			val_t a, b;
			do {
				a = genrand(&seed) % parm.range.mod;
				b = a + parm.range.start;
				if (DIST_STAT) {
					inc_stat(a);
				}
			} while (in_set(b, arr, x));
			arr[x] = b;
		}
		sort(arr, parm.needle.cols);

		if (PRINT_ROWS) {
			report_row(arr, parm.needle.cols);
		}

		if (arrcomp(arr, parm.needle.arr, parm.needle.cols)) {
			return true;
		}
	}

	return false;
}

static inline void report_seed (const uint32_t s) {
	const int fr = mtx_lock(&stdlock);
	assert(fr == thrd_success);
	(void)fr;

	printf("%"PRIu32"\n", s);
	fflush(stdout);

	mtx_unlock(&stdlock);
}

static int work_main_sel (void *ctx_in) {
	struct wctx *ctx = ctx_in;

	for (uint64_t i = ctx->seed_start; i < UINT32_MAX; i += ctx->seed_step) {
		if (do_hunt_sel(i)) {
			report_seed(i);
		}
	}

	return 0;
}

static int work_main_mod (void *ctx_in) {
	struct wctx *ctx = ctx_in;

	for (uint64_t i = ctx->seed_start; i < UINT32_MAX; i += ctx->seed_step) {
		if (do_hunt_mod(i)) {
			report_seed(i);
		}
	}

	return 0;
}

static int (*work_main)(void*) = work_main_sel;

static int main_inner (void) {
	struct wctx wc[parm.nproc];
	thrd_t t[parm.nproc];
	int fr;

	assert((unsigned long)parm.nproc != ULONG_MAX);

	for (unsigned long i = 0; i < (unsigned long)parm.nproc; i += 1) {
		wc[i].seed_start = i;
		wc[i].seed_step = parm.nproc;

		fr = thrd_create(t + i, work_main, wc + i);
		if (fr != thrd_success) {
			perror(ARGV0": thrd_create()");
			abort();
		}
	}

	for (unsigned long i = 0; i < (unsigned long)parm.nproc; i += 1) {
		fr = thrd_join(t[i], NULL);
		if (fr != thrd_success) {
			perror(ARGV0": thrd_join()");
			abort();
		}
	}

	return 0;
}

static int parse_args (const int argc, const char **argv) {
	int fr;

	// FIXME: don't use sscanf()

	while (true) {
		fr = getopt(argc, (char*const*)argv, "ha:c:r:");
		if (fr < 0) {
			break;
		}
		switch (fr) {
		case 'h':
			return 0;
		case 'a':
			if (strcmp("sel", optarg) == 0) {
				work_main = work_main_sel;
			}
			else if (strcmp("mod", optarg) == 0) {
				work_main = work_main_mod;
			}
			else {
				goto err_inv;
			}
			break;
		case 'c':
			parm.nproc = -1;
			sscanf(optarg, "%ld", &parm.nproc);
			if (parm.nproc < 0) {
				goto err_inv;
			}
			break;
		case 'r':
			sscanf(optarg, "%zu", &parm.range.rows);
			if (parm.range.rows == 0) {
				goto err_inv;
			}
			break;
		default:
			return -1;
		}
	}

	if (optind + 6 == argc) {
		static val_t arr[6];

		for (size_t i = 0; i < 6; i += 1) {
			const char *arg = argv[optind + i];
			sscanf(arg, "%"SCNu8, arr + i);
			if (!(0 < arr[i] && arr[i] <= 45)) {
				fprintf(stderr, ARGV0": %s: %s\n", arg, strerror(EINVAL));
				return -1;
			}
		}
		sort(arr, parm.needle.cols);
		parm.needle.arr = arr;
		assert(sizeof(arr) / sizeof(*arr) == parm.needle.cols);

		if (!uniq_set(arr, parm.needle.cols)) {
			fprintf(stderr, ARGV0": not unique set\n");
			return -1;
		}
	}
	else if (optind + 6 > argc) {
		fprintf(stderr, ARGV0": too few arguments\n");
		return -1;
	}
	else {
		fprintf(stderr, ARGV0": too many arguments\n");
		return -1;
	}

	return 1;
err_inv:
	fprintf(stderr, ARGV0": -%c %s: %s\n", fr, optarg, strerror(EINVAL));
	return -1;
}

static unsigned long getnproc (void) {
	const long ret = sysconf(_SC_NPROCESSORS_ONLN);
	if (ret <= 0) {
		return 1;
	}
	return ret;
}

int main (const int argc, const char **argv) {
	static int fr;
	int ret;

	fr = parse_args(argc, argv);
	if (fr == 0) {
		fprintf(
			stderr,
			"Usage: "ARGV0" [-h] [-c threads] [-a algo] [-r rows] <A> <B> <C> <D> <E> <F>\n"
			"Algo: sel(default), mod\n");
		return 0;
	}
	else if (fr < 0) {
		return 2;
	}
	assert(parm.range.mod > parm.needle.cols);

	if (parm.nproc == 0) {
		parm.nproc = getnproc();
	}

	fr = mtx_init(&stdlock, mtx_plain);
	assert(fr == thrd_success);
	(void)fr;

	if (DIST_STAT) {
		st = calloc(parm.range.mod, sizeof(atomic_uintmax_t));
		for (size_t i = 0; i < parm.range.mod; i += 1) {
			atomic_init(st + i, 0);
			// just not feasible if atomic variables are not lock-free
			assert(atomic_is_lock_free(st + i));
		}
	}

	ret = main_inner();
	if (DIST_STAT && ret == 0) {
		for (size_t i = 0; i < parm.range.mod; i += 1) {
			fprintf(stderr, "%zu: %"PRIuMAX"\n", i, atomic_load(st + i));
		}
	}

	free(st);
	mtx_destroy(&stdlock);

	return ret;
}
