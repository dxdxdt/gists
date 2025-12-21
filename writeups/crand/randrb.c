#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>
#ifdef _WIN32
#include <windows.h>
#include <memoryapi.h>
#else
#include <sys/mman.h>
#endif
#define ARGV0 "randrb"


#define OUR_RAND_MAX (32767)
#define ARR_SIZE ((size_t)4294967296)
static_assert(ARR_SIZE > 0, "no 32-bit machine support");
uint16_t *the_arr;

#define ARR_BYTE_SIZE (ARR_SIZE * sizeof(*the_arr))
static_assert(
	ARR_SIZE < ARR_BYTE_SIZE && ARR_BYTE_SIZE < SIZE_MAX,
	"no 32-bit machine support");

#define DEFAULT_M 214013
#define DEFAULT_A 2531011
#define DEFAULT_S 1

static struct {
	unsigned int m;
	unsigned int a;
	unsigned int s;
	const char *mode;
} parm = {
	// https://gitlab.winehq.org/wine/wine/-/blob/master/dlls/msvcrt/misc.c
	// these are msvc default
	.m = DEFAULT_M,
	.a = DEFAULT_A,
	.s = DEFAULT_S,
	.mode = "M",
};

static int rand_masked (void) {
	parm.s = parm.s * parm.m + parm.a;
	return (parm.s >> 16) & OUR_RAND_MAX;
}

static int rand_wrap (void) {
	parm.s = parm.s * parm.m + parm.a;
	return (parm.s >> 16) % (OUR_RAND_MAX + 1);
}

static void (*seedf)(unsigned int) = NULL;
/*
 * Use of function pointers causes loss of -O3/-flto inline efficiency. Making
 * the function inline would increase efficiency by 300%.
 */
static int (*genrand)(void) = rand_masked;

static int do_dump (void) {
	ssize_t l;
	size_t rem = ARR_BYTE_SIZE;
	const uint8_t *p = (void*)the_arr;

	fprintf(stderr, ARGV0": dumping the array to stdout ...\n");

	while (rem > 0) {
#ifdef _WIN32
		// fuck you Microsoft
		const unsigned int outl = rem < INT_MAX ? rem : INT_MAX;
		l = write(STDOUT_FILENO, p, outl);
#else
		l = write(STDOUT_FILENO, p, rem);
#endif
		assert(l != 0);
		if (l < 0) {
			perror(ARGV0": write(STDOUT_FILENO, ...)");
			return 1;
		}
		fprintf(
			stderr,
			ARGV0": %zu (%.1f%%) ...\n",
			rem,
			(double)rem / (double)ARR_BYTE_SIZE * 100.0);

		p += l;
		rem -= l;
	}
	// fsync(STDOUT_FILENO); // modern programs are not responsible for EIO

	return 0;
}

static int do_iteration (void) {
	int r;

	if (seedf != NULL) {
		seedf(parm.s);
	}

	for (size_t i = 0; i < ARR_SIZE; i += 1) {
		r = (uint16_t)genrand();
		the_arr[i] = r;
	}

	return do_dump();
}

static void print_help (void) {
	printf(
		"Usage: "ARGV0" [-h] [-m M] [-a A] [-s SEED] [mode]\n"
		"Options:\n"
		"  -m: multiplier (default: %u)\n"
		"  -a: increment (default: %u)\n"
		"  -s: seed (default: %u)\n"
		"mode:\n"
		"  M: use MSVC's variant of rand() (default)\n"
		"  W: use the ISO C recommended example of rand()\n"
		"  N: use rand() from the libc that's been linked against the executable\n",
		DEFAULT_M,
		DEFAULT_A,
		DEFAULT_S
	);
}

static int parse_args (const int argc, const char **argv) {
	while (true) {
		const int fr = getopt(argc, (char*const*)argv, "hm:a:s:");

		if (fr < 0) {
			break;
		}
		switch (fr) {
		case 'h': return 0;
		// I'm too lazy to use strtol(). Security is not of concern
		case 'm': sscanf(optarg, "%u", &parm.m); break;
		case 'a': sscanf(optarg, "%u", &parm.a); break;
		case 's': sscanf(optarg, "%u", &parm.s); break;
		default: return -1;
		}
	}

	if (argc > optind) {
		parm.mode = argv[optind];
		assert(parm.mode != NULL);

		if (strcmp(parm.mode, "M") == 0) {}
		else if (strcmp(parm.mode, "W") == 0) {
			genrand = rand_wrap;
			parm.m = 1103515245;
			parm.a = 12345;
		}
		else if (strcmp(parm.mode, "N") == 0) {
			genrand = rand;
			seedf = srand;
			parm.m = parm.a = parm.s = 0;
		}
		else {
			fprintf(stderr, ARGV0": %s: %s\n", parm.mode, strerror(EINVAL));
			return -1;
		}
	}

	return 1;
}

int main (const int argc, const char **argv) {
	static int pr;

	pr = parse_args(argc, argv);
	if (pr == 0) {
		print_help();
		return 0;
	}
	else if (pr < 0) {
		return 2;
	}

#ifndef _WIN32
	if (isatty(STDOUT_FILENO)) {
		fprintf(stderr, ARGV0": stdout is a terminal\n");
		return 2;
	}
#endif

	fprintf(stderr, ARGV0": using %zu bytes of memory\n", ARR_BYTE_SIZE);
	fprintf(
		stderr,
		 ARGV0": mode = %s, M = %u, A = %u, S = %u\n",
		parm.mode,
		parm.m,
		parm.a,
		parm.s);

	the_arr = calloc(ARR_SIZE, sizeof(*the_arr));
	if (the_arr == NULL) {
		perror(ARGV0": calloc()");
		goto err;
	}
#ifdef _WIN32
	// XXX: this is useless without (Get|Set)ProcessWorkingSetSize()
	// I'm not doing that. Fuck Microsoft
	if (false && !VirtualLock(the_arr, ARR_BYTE_SIZE)) {
		fprintf(stderr, ARGV0": VirtualLock(): 0x%lx\n", GetLastError());
		// fprintf(stderr, ARGV0": this is only to prevent OOM freeze/crash\n");
		// fprintf(stderr, ARGV0": run the program as admin if you want\n");
	}
#else
	/*
	 * https://man7.org/linux/man-pages/man2/mlock.2.html
	 * > Memory locking APIs will trigger the necessary page-faults, to bring in
	 * > the pages being locked, to physical memory. Consequently first access
	 * > to a locked-memory (following an mlock*() call) will already have
	 * > physical memory assigned and will not page fault (in RT-critical path).
	 * > This removes the need to explicitly pre-fault these memory.
	 *
	 * Try faulting all the pages right away to boost things up. Page fault
	 * handling is so efficient that there won't be any performance gain. Still
	 * a good protection mechanism against OOM.
	 */
	if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
		perror(ARGV0": mlockall()");
		// fprintf(stderr, ARGV0": this is only to prevent OOM freeze/crash\n");
		// fprintf(stderr, ARGV0": run the program as root if you want\n");
	}
#endif

	return do_iteration();
err:
	free(the_arr);
	return 1;
}
