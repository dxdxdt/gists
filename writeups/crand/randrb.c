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
#else
#include <sys/mman.h>
#endif
#define ARGV0 "randrb"


#define OUR_RAND_MAX (32767)
#define DEFAULT_M 214013
#define DEFAULT_A 2531011
#define DEFAULT_S 1
#define DEFAULT_CNT ((uint_fast64_t)UINT32_MAX * 2)

static struct {
	unsigned int m;
	unsigned int a;
	unsigned int s;
	int ofmt; // -1: little, 0: text, 1: big
	uint_fast64_t cnt;
	const char *mode;
} parm = {
	// https://gitlab.winehq.org/wine/wine/-/blob/master/dlls/msvcrt/misc.c
	// these are msvc default
	.m = DEFAULT_M,
	.a = DEFAULT_A,
	.s = DEFAULT_S,
	.mode = "M",
	.cnt = DEFAULT_CNT,
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

static bool do_binary_dump (const void *m, size_t len) {
	const uint8_t *p = m;

	while (len > 0) {
#ifdef _WIN32
		// fuck you Microsoft
		const unsigned int outl = len < INT_MAX ? len : INT_MAX;
		const ssize_t fr = write(STDOUT_FILENO, p, outl);
#else
		const ssize_t fr = write(STDOUT_FILENO, p, len);
#endif
		assert(fr != 0);
		if (fr < 0) {
			perror(ARGV0": write(STDOUT_FILENO, ...)");
			return false;
		}
		p += (size_t)fr;
		len -= (size_t)fr;
	}
	// fsync(STDOUT_FILENO); // modern programs are not responsible for EIO

	return true;
}

static int do_iteration_bin (const size_t iosize) {
	uint8_t buf[iosize];
	size_t l = 0;
	uint16_t r;

	assert(parm.ofmt != 0);
	assert(iosize >= (int)sizeof(uint16_t) && iosize % sizeof(uint16_t) == 0);

	for (uint_fast64_t i = 0; i < parm.cnt; i += 1) {
		r = (uint16_t)genrand();
		if (parm.ofmt < 0) {
			buf[l + 0] = (uint8_t)((r & 0x00FF));
			buf[l + 1] = (uint8_t)((r & 0xFF00) >> 8);
		}
		else {
			buf[l + 0] = (uint8_t)((r & 0xFF00) >> 8);
			buf[l + 1] = (uint8_t)((r & 0x00FF));
		}
		l += 2;
		if (l >= iosize) {
			if (!do_binary_dump(buf, l)) {
				return 1;
			}
			l = 0;
		}
	}

	return do_binary_dump(buf, l) ? 0 : 1;
}

static int do_iteration_txt (void) {
	uint16_t r;
	for (uint_fast64_t i = 0; i < parm.cnt; i += 1) {
		r = (uint16_t)genrand();
		if (printf("%"PRIu16"\n", r) <= 0) {
			return 1;
		}
	}
	return 0;
}

static void print_help (void) {
	printf(
		"Usage: "ARGV0" [-hlb] [-m M] [-a A] [-s SEED] [-n COUNT] [mode]\n"
		"Options:\n"
		"  -m: multiplier (default: %u)\n"
		"  -a: increment (default: %u)\n"
		"  -s: seed (default: %u)\n"
		"  -n: number of random numbers to generate (default: %"PRIuFAST64")\n"
		"  -l: output in little endian binary\n"
		"  -b: output in big endian binary\n"
		"mode:\n"
		"  M: use MSVC's variant of rand() (default)\n"
		"  W: use the ISO C recommended example of rand()\n"
		"  N: use rand() from the libc that's been linked against the executable\n",
		DEFAULT_M,
		DEFAULT_A,
		DEFAULT_S,
		DEFAULT_CNT
	);
}

static int parse_args (const int argc, const char **argv) {
	while (true) {
		const int fr = getopt(argc, (char*const*)argv, "hm:a:s:n:lb");

		if (fr < 0) {
			break;
		}
		switch (fr) {
		case 'h': return 0;
		case 'l': parm.ofmt = -1; break;
		case 'b': parm.ofmt = +1; break;
		// I'm too lazy to use strtol(). Security is not of concern
		case 'm': sscanf(optarg, "%u", &parm.m); break;
		case 'a': sscanf(optarg, "%u", &parm.a); break;
		case 's': sscanf(optarg, "%u", &parm.s); break;
		case 'n': sscanf(optarg, "%"PRIuFAST64, &parm.cnt); break;
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
			parm.m = parm.a = 0;
		}
		else {
			fprintf(stderr, ARGV0": %s: %s\n", parm.mode, strerror(EINVAL));
			return -1;
		}
	}

	return 1;
}

static size_t getpsz (void) {
#ifdef _WIN32
	static bool fire_once = true;
	SYSTEM_INFO si = { 0, };
	GetSystemInfo(&si);
	assert(si.dwPageSize > 0);
	if (fire_once && si.dwPageSize != 4096) {
		fprintf(stderr, ARGV0": interesting page size: %lu\n", si.dwPageSize);
		fire_once = false;
	}
	return si.dwPageSize;
#else
	const long ret = sysconf(_SC_PAGESIZE);
	assert(ret > 0);
	return (size_t)ret;
#endif
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
	if (parm.ofmt != 0 && isatty(STDOUT_FILENO)) {
		fprintf(stderr, ARGV0": stdout is a terminal\n");
		return 2;
	}
#endif

	fprintf(
		stderr,
		 ARGV0": mode = %s, M = %u, A = %u, S = %u, ofmt = %d, cnt = %"PRIuFAST64"\n",
		parm.mode,
		parm.m,
		parm.a,
		parm.s,
		parm.ofmt,
		parm.cnt
	);

	if (seedf != NULL) {
		seedf(parm.s);
	}

	if (parm.ofmt == 0) {
		return do_iteration_txt();
	}
	return do_iteration_bin(getpsz() * 10);
}
