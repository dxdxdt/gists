#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <limits.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>

#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>

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
	if (alt_stack != NULL && alt_stack_size > 0) {
		stack_t ss = {
			.ss_sp = alt_stack,
			.ss_size = alt_stack_size,
		};

		sigaltstack(&ss, NULL);
	}

	if (stack_addr != NULL && stack_len > 0) {
		struct sigaction sa = {0};

		sa.sa_sigaction = handle_stackgrowth;
		sa.sa_flags = SA_RESTART | SA_SIGINFO | SA_ONSTACK;
		sigaction(SIGSEGV, &sa, NULL);
	}
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

static uint64_t fact(uint64_t a, bool *ovf)
{
	uint64_t c;

	if (a <= 1)
		return a;

	if (__builtin_mul_overflow(a, fact(a - 1, ovf), &c)) {
		if (ovf != NULL)
			*ovf = true;
	}

	return c;
}

struct args {
	uint64_t x;
	uint64_t y;
	bool ovf;
	struct {
		bool track_stack:1;
		bool stop:1;
	} flags;
};

static void *th_run(void *th_arg)
{
	struct args *a = th_arg;

	setup_sighandlers();

	a->y = fact(a->x, &a->ovf);

	clear_sighandlers();

	if (a->flags.stop)
		raise(SIGSTOP);

	return NULL;
}

int main(int argc, char *argv[])
{
	struct args a = {0};
	pthread_attr_t th_attr;
	pthread_t th;
	int err;

	for (;;) {
		const int c = getopt(argc, (char * const*)argv, "tS");

		switch (c) {
		case 't':
			a.flags.track_stack = true;
			break;
		case 'S':
			a.flags.stop = true;
			break;
		default:
			if (c < 0)
				goto opt_done;
			goto opt_err;
		}
	}
opt_done:
	if (argc <= optind || sscanf(argv[optind], "%" SCNu64, &a.x) != 1)
		goto opt_err;

	pthread_attr_init(&th_attr);
	pagesize = sysconf(_SC_PAGESIZE);
	assert(pagesize > 0);

	if (a.flags.track_stack) {
		pthread_attr_getstacksize(&th_attr, &stack_len);
		fprintf(stderr, "pthread_attr_getstacksize(): %zu\n", stack_len);
		alt_stack_size = PTHREAD_STACK_MIN;
		alt_stack = NULL;
		err = posix_memalign(&alt_stack, alt_stack_size, pagesize);
		/*
		 * POSIX says nothing about posix_memalign() setting errno
		 * although implementations may set it nontheless.
		 */
		if (err)
			errno = err;
		stack_addr = alloc_stack(&stack_len);
		if (alt_stack == NULL || stack_addr == NULL) {
			perror(NULL);
			exit(1);
		}
		fprintf(stderr, "Stack allocated: %zu at 0x%" PRIxPTR "\n",
				stack_len, (uintptr_t)stack_addr);
		fprintf(stderr, "Alt stack allocated: %zu at 0x%" PRIxPTR "\n",
				alt_stack_size, (uintptr_t)alt_stack);

		pthread_attr_setstack(&th_attr, stack_addr, stack_len);
	}
	fprintf(stderr, "PID: %d\n", (int)getpid());

	err = pthread_create(&th, &th_attr, th_run, &a);
	if (err) {
		errno = err;
		perror("pthread_create()");
		exit(1);
	}

	pthread_join(th, NULL);

	if (a.flags.track_stack)
		fprintf(stderr, "Stack usage: %llu bytes\n",
				(unsigned long long)pagesize * cnt_pages);
	printf("%" PRIu64 "! = %" PRIu64 "%s\n", a.x, a.y, a.ovf ? " (overflow)" : "");

	pthread_attr_destroy(&th_attr);
	free(alt_stack);
	munmap(stack_addr, stack_len);

	return a.ovf ? 1 : 0;
opt_err:
	fprintf(stderr, "Usage: %s <NUM>\n", argv[0]);
	return 2;
}
