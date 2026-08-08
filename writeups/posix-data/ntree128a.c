#define _DEFAULT_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>

#include <search.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef _POSIX_MAPPED_FILES /* Unavailable on Windows */
#include <signal.h>
#include <sys/mman.h>
#endif

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

#define PROGNAME "ntree128a"

intmax_t *arr;

static int compf(const void *in_a, const void *in_b)
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

static ssize_t new_element(void)
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

static void free_all(void)
{
	free(arr);
	arr = NULL;
	count.size = count.len = 0;
}

static void loadf(FILE *f, const char *path)
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

static void load_path(const char *path)
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

#ifdef _POSIX_MAPPED_FILES
static long pagesize;
static void *alt_stack;
static size_t alt_stack_size;
static volatile sig_atomic_t cnt_pages;
static void *stack_addr;
static size_t stack_len;

static inline size_t pagesize_round_up(size_t x)
{
	if (x % pagesize == 0)
		x /= pagesize;
	else
		x = (x / pagesize) + 1;

	return x == 0 ? (size_t)pagesize : x * pagesize;
}

static void clear_sighandlers(void)
{
	struct sigaction sa = { .sa_handler = SIG_DFL, };

	sigaction(SIGSEGV, &sa, NULL);
}

static void handle_stackgrowth(int sig, siginfo_t *info, void *ucontext)
{
	const uintptr_t faulted = (uintptr_t)info->si_addr & ~((uintptr_t)pagesize - 1);
	const uintptr_t stack_base = (uintptr_t)stack_addr;
	const uintptr_t stack_end = stack_base + stack_len;
	const uintptr_t stack_end_cur = stack_end - pagesize * cnt_pages;
	const size_t fault_dt = stack_end - faulted;
	int prot, err;

	if (!(stack_base <= faulted && faulted < stack_end) || /* Not our range */
			(stack_end_cur <= faulted && faulted < stack_end)) /* Already handled */
		/*
		 * This is not our fault. Remove the handler so that the default
		 * action(crash) is triggered.
		 */
		 goto disable;

	prot = PROT_READ | PROT_WRITE;
	err = mprotect((void*)faulted, fault_dt, prot);
	if (err == 0) {
		cnt_pages = (sig_atomic_t)(fault_dt / pagesize);
		return;
	}
disable:
	clear_sighandlers();
}

static void setup_sighandlers(void)
{
	struct sigaction sa = {0};
	stack_t ss = {
		.ss_sp = alt_stack,
		.ss_size = alt_stack_size,
	};

	assert(alt_stack != NULL && alt_stack_size > 0);
	sigaltstack(&ss, NULL);

	sa.sa_sigaction = handle_stackgrowth;
	sa.sa_flags = SA_RESTART | SA_SIGINFO | SA_ONSTACK;
	sigaction(SIGSEGV, &sa, NULL);
}

static void *alloc_stack(size_t *size)
{
	const size_t initial_size = pagesize_round_up(PTHREAD_STACK_MIN);
	void *ret = NULL;
	uintptr_t initial_base;
	int prot, flags;
	int err;

	(void)err;

	if (*size == 0) {
		errno = EINVAL;
		goto err;
	}
	*size = pagesize_round_up(*size);

	prot = PROT_NONE;
	flags = MAP_PRIVATE | MAP_ANONYMOUS;
#ifdef MAP_NORESERVE
	flags |= MAP_NORESERVE;
#endif
#ifdef MAP_STACK
	flags |= MAP_STACK;
#endif
#ifdef MAP_GROWSDOWN
	flags |= MAP_GROWSDOWN;
#endif
	ret = mmap(NULL, *size, prot, flags, -1, 0);
	if (ret == MAP_FAILED)
		return NULL;
	assert(ret != NULL);

	initial_base = (uintptr_t)ret + *size - initial_size;

	prot = PROT_READ | PROT_WRITE;
	err = mprotect((void*)initial_base, initial_size, prot);
	if (err)
		goto err;

	cnt_pages = initial_size / pagesize;

	return ret;
err:
	if (ret != NULL)
		munmap(ret, *size);
	return NULL;
}

static unsigned long spike_stack_usage(unsigned long a, bool *ovf)
{
	unsigned long b, c;

	if (a <= 1)
		return a;

	b = spike_stack_usage(a - 1, ovf);
	if (__builtin_mul_overflow(a, b, &c)) {
		if (ovf != NULL)
			*ovf = true;
	}

	return c;
}

static bool should_spike_stack(unsigned long *x)
{
	const char *str = getenv("STACK_SPIKE_TEST");

	if (str == NULL)
		return false;
	return sscanf(str, "%ld", x) == 1;
}
#endif

struct arg_vec {
	char **argv;
	int argc;
	int ret;
};

static void *th_mainf(void *th_arg)
{
	static struct timespec tp[3];
	struct arg_vec *a = th_arg;

#ifdef _POSIX_MAPPED_FILES
	/*
	 * 100000: 1286144 bytes on x86_64 w/ -O2
	 * 1000000: (crashes)
	 */
	unsigned long spike_depth = 0;

	setup_sighandlers();

	if (should_spike_stack(&spike_depth)) {
		bool ovf = false;
		const unsigned long y = spike_stack_usage(spike_depth, &ovf);

		fprintf(stderr, "spike_stack_usage(): %lu! is %lu%s\n",
				spike_depth, y, ovf ? " (overflow)" : "");
	}
#endif

	if (a->argc == 1) {
		loadf(stdin, "-");
	}
	else {
		for (int i = 1; i < a->argc; i += 1) {
			if (strcmp(a->argv[i], "-") == 0) {
				loadf(stdin, "-");
			}
			else {
				load_path(a->argv[i]);
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
#ifdef _POSIX_MAPPED_FILES
	clear_sighandlers();
#endif

	a->ret = 0;
	return NULL;
}

int main(int argc, char *argv[])
{
	struct arg_vec arg = { .argc = argc, .argv = argv, };
	pthread_t th;
	pthread_attr_t th_attr;
	int err;

	pthread_attr_init(&th_attr);

#ifdef _POSIX_MAPPED_FILES
	pagesize = sysconf(_SC_PAGESIZE);
	assert(pagesize > 0);

	pthread_attr_getstacksize(&th_attr, &stack_len);
	fprintf(stderr, "pthread_attr_getstacksize(): %zu\n", stack_len);
	alt_stack_size = PTHREAD_STACK_MIN;
	alt_stack = NULL;
	err = posix_memalign(&alt_stack, alt_stack_size, pagesize);
	/*
	 * POSIX says nothing about posix_memalign() setting errno although
	 * implementations may set it nontheless.
	 */
	if (err)
		errno = err;
	stack_addr = alloc_stack(&stack_len);
	if (alt_stack == NULL || stack_addr == NULL) {
		perror(PROGNAME);
		exit(1);
	}
	fprintf(stderr, "actual stack size allocated: %zu\n", stack_len);

	pthread_attr_setstack(&th_attr, stack_addr, stack_len);
#endif
	err = pthread_create(&th, &th_attr, th_mainf, &arg);
	if (err) {
		errno = err;
		perror(PROGNAME ": pthread_create()");
		exit(1);
	}

	pthread_join(th, NULL);
	pthread_attr_destroy(&th_attr);
#ifdef _POSIX_MAPPED_FILES
	free(alt_stack);
	munmap(stack_addr, stack_len);

	fprintf(stderr, "Stack usage: %llu bytes\n", (unsigned long long)pagesize * cnt_pages);
#endif
	return arg.ret;
}
