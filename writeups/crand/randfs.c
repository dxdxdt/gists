#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <threads.h>
#include <stdatomic.h>
#define ARGV0 "lottofs" // "Lottery Find Seed"
#define LCG_M (214013)
#define LCG_A (2531011)
#define LCG_S (1)
#define LCG_OP &
#define LCG_BITMASK (32767)

#define DIST_STAT false

atomic_uintmax_t *st;

mtx_t stdlock;

static inline int32_t genrand (uint32_t *seed) {
	*seed = *seed * LCG_M + LCG_A;
	return (*seed >> 16) LCG_OP LCG_BITMASK;
}

struct {
	struct {
		uint32_t start;
		uint32_t mod;
	} range;
	struct {
		uint32_t bits;
		uint32_t mask;
	} hash;
	struct {
		uint64_t hash;
		const uint32_t *arr;
		size_t cnt;
	} needle;
} parm;

struct wctx {
	uint32_t seed_start;
	uint32_t seed_step;
};

static inline bool in_set (
		const uint32_t needle,
		const uint32_t *arr,
		const size_t cnt)
{
	for (size_t i = 0; i < cnt; i += 1) {
		if (arr[i] == needle) {
			return true;
		}
	}
	return false;
}

static inline bool uniq_set (const uint32_t *arr, const size_t cnt) {
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
// 	const uint32_t a = *(const uint32_t*)in_a;
// 	const uint32_t b = *(const uint32_t*)in_b;
// 	return a == b ? 0 : a < b ? -1 : 1;
// }

/*
 * This is the good old selection sort. This faster than calling qsort().
 */
static inline void sort (uint32_t *arr, const size_t cnt) {
	for (size_t i = 0; i < cnt; i += 1) {
		for (size_t j = i + 1; j < cnt; j += 1) {
			if (arr[i] > arr[j]) {
				const uint32_t tmp = arr[i];
				arr[i] = arr[j];
				arr[j] = tmp;
			}
		}
	}
}

static inline uint64_t sanitize (uint32_t *arr) {
	uint64_t ret = 0;

	// qsort(arr, parm.needle.cnt, sizeof(uint32_t), compf);
	sort(arr, parm.needle.cnt);
	for (size_t i = 0; i < parm.needle.cnt; i += 1) {
		ret = (ret << parm.hash.bits) | (arr[i] & parm.hash.mask);
	}

	return ret;
}

static inline void inc_stat (const uint32_t n) {
	const uintmax_t prev = atomic_fetch_add_explicit(
			st + n,
			1,
			memory_order_relaxed);
	assert(prev != UINTMAX_MAX); // oi! overflow!
	(void)prev;
}

static inline bool do_hunt (uint32_t seed) {
	uint32_t arr[parm.needle.cnt];
	uint64_t hash;
	uint32_t x, y;

	for (size_t i = 0; i < parm.needle.cnt; i += 1) {
		do {
			x = genrand(&seed) % parm.range.mod;
			y = x + parm.range.start;
		} while (in_set(y, arr, i));

		if (DIST_STAT) {
			inc_stat(x);
		}
		arr[i] = y;
	}
	hash = sanitize(arr);

	return
		hash == parm.needle.hash &&
		memcmp(arr, parm.needle.arr, parm.needle.cnt * sizeof(*arr)) == 0;
}

static int work_main (void *ctx_in) {
	struct wctx *ctx = ctx_in;

	for (uint64_t i = ctx->seed_start; i < UINT32_MAX; i += ctx->seed_step) {
		if (do_hunt(i)) {
			const int fr = mtx_lock(&stdlock);
			assert(fr == thrd_success);
			(void)fr;

			printf("%"PRIu64"\n", i);

			mtx_unlock(&stdlock);
		}
	}

	return 0;
}

static int main_inner (const unsigned long n) {
	struct wctx wc[n];
	thrd_t t[n];
	int fr;

	assert(n != ULONG_MAX);

	for (unsigned long i = 0; i < n; i += 1) {
		wc[i].seed_start = i;
		wc[i].seed_step = n;

		fr = thrd_create(t + i, work_main, wc + i);
		if (fr != thrd_success) {
			perror(ARGV0": thrd_create()");
			abort();
		}
	}

	for (unsigned long i = 0; i < n; i += 1) {
		fr = thrd_join(t[i], NULL);
		if (fr != thrd_success) {
			perror(ARGV0": thrd_join()");
			abort();
		}
	}

	return 0;
}

int main (const int argc, const char **argv) {
	static int fr;
	int ret;

	if (argc < 1 + 6) {
		fprintf(stderr, "Usage: "ARGV0" <A> <B> <C> <D> <E> <F> \n");
		return 2;
	}

	fr = mtx_init(&stdlock, mtx_plain);
	assert(fr == thrd_success);
	(void)fr;

	// FIXME: don't use sscanf()
	// TODO: parameterize everything and observe the performance impact
	static uint32_t arr[6];
	for (size_t i = 0; i < 6; i += 1) {
		sscanf(argv[i + 1], "%"SCNu32, arr + i);
		assert(0 < arr[i] && arr[i] <= 45);
	}
	parm.range.start = 1;
	parm.range.mod = 45;
	parm.hash.bits = 6;
	parm.hash.mask = 0x3F;
	parm.needle.arr = arr;
	parm.needle.cnt = sizeof(arr) / sizeof(*arr);
	parm.needle.hash = sanitize(arr);
	assert(uniq_set(arr, parm.needle.cnt));

	if (DIST_STAT) {
		st = calloc(parm.range.mod, sizeof(atomic_uintmax_t));
		for (size_t i = 0; i < parm.range.mod; i += 1) {
			atomic_init(st + i, 0);
			// just not feasible if atomic variables are not lock-free
			assert(atomic_is_lock_free(st + i));
		}
	}

	// TODO: 자동으로 나온 1등

	ret = main_inner(8);
	if (DIST_STAT && ret == 0) {
		for (size_t i = 0; i < parm.range.mod; i += 1) {
			fprintf(stderr, "%zu: %"PRIuMAX"\n", i, atomic_load(st + i));
		}
	}
	return ret;
}
