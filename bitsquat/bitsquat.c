#define _POSIX_C_SOURCE 199309L
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>

#define ARGV0 "bitsquat"


static struct {
	union {
		bool help:1;
	} flags;
	size_t size;
	double report_int;
	uint32_t fault_p;
} opts;

static struct {
	int fd;
} g;

static void init_opts (void) {
	opts.report_int = 1.0;
}

static void init_g (void) {
	g.fd = -1;
}

static bool parse_opts (const int argc, const char **argv) {
	int fr;
	double tmp_f;

	while (true) {
		fr = getopt(argc, (char*const*)argv, "hs:F:i:");
		if (fr == -1) {
			break;
		}

		switch (fr) {
		case 'h': opts.flags.help = true; break;
		case 's':
			opts.size = 0;
			fr = sscanf(optarg, "%zu", &opts.size);
			if (fr != 1 || opts.size == 0) {
				fprintf(stderr, ARGV0 ": invalid -s option: %s\n", optarg);
				return false;
			}
			break;
		case 'F':
			tmp_f = 0.0;
			fr = sscanf(optarg, "%lf", &tmp_f);
			if (fr != 1) {
				fprintf(stderr, ARGV0 ": invalid -F option: %s\n", optarg);
				return false;
			}

			opts.fault_p = (uint32_t)(tmp_f * UINT32_MAX);
			break;
		case 'i':
			opts.report_int = NAN;
			fr = sscanf(optarg, "%lf", &opts.report_int);
			// accept "inf" for disabling report
			if (fr != 1 || isnan(opts.report_int)) {
				fprintf(stderr, ARGV0 ": invalid -i option: %s\n", optarg);
				return false;
			}
			break;
		case '?':
			return false;
		}
	}

	if (opts.size == 0) {
		fprintf(stderr, ARGV0 ": -s option required\n");
		return false;
	}

	return true;
}

static void print_help (void) {
	// TODO
}

static bool alloc_g (void) {
	g.fd = open("/dev/urandom", O_RDONLY);

	if (g.fd < 0) {
		perror(ARGV0 ": /dev/urandom");
		return false;
	}

	return true;
}

static void dealloc_g (void) {
	if (g.fd >= 0) {
		close(g.fd);
		g.fd = -1;
	}
}

static void do_report (const struct timespec *ts, const size_t itc) {
	fprintf(
		stderr, ARGV0 ": %10zu.%06ld %20zu\n",
		(size_t)ts->tv_sec,
		ts->tv_nsec / 1000,
		itc);
}

static void ts_sub (
	struct timespec *c,
	const struct timespec *a,
	const struct timespec *b)
{
	c->tv_nsec = a->tv_nsec - b->tv_nsec;
	c->tv_sec = a->tv_sec - b->tv_sec;
	if (c->tv_nsec < 0) {
		c->tv_sec -= 1;
		c->tv_nsec += 1000000000;
	}
}

static double ts2d (const struct timespec *ts) {
	return ts->tv_nsec / 1000000000 + ts->tv_sec;
}

static void dump_error (
	const size_t itc,
	const struct timespec *ts,
	const uint8_t *a,
	const uint8_t *b,
	const size_t l)
{
	printf(
		"iteration: %10zu.%06ld %zu\n",
		(size_t)ts->tv_sec,
		ts->tv_nsec / 1000,
		itc);

	for (size_t i = 0; i < l; i += 1) {
		if (a[i] == b[i]) {
			continue;
		}

		printf("  - offset: %zu\n", i);
		printf("    a: 0x%X\n", a[i]);
		printf("    b: 0x%X\n", b[i]);
	}
}

static void getrnd (void *out, const size_t l) {
	const ssize_t fr = read(g.fd, out, l);

	if (fr < 0) {
		perror(ARGV0 ": getrnd()");
		abort();
	}
	if ((size_t)fr != l) {
		abort();
	}
}

static bool cointoss (void) {
	uint32_t rnd;

	if (opts.fault_p == 0) {
		return false;
	}

	getrnd(&rnd, sizeof(rnd));

	return opts.fault_p >= rnd;
}

static void inject_fault (uint8_t *m, const size_t l) {
	size_t rnd;

	getrnd(&rnd, sizeof(rnd));
	rnd %= l;

	m[rnd] = ~m[rnd];
}

static char *getctime (void) {
	const time_t t = time(NULL);
	return ctime(&t);
}

static int do_main (void) {
	int ec = 0;
	int fr;
	void *m1 = NULL;
	void *m2 = NULL;
	const size_t s = opts.size;
	struct {
		struct timespec tp_start;
		struct timespec tp_now;
		struct timespec tp_report;
		struct timespec d_start;
		struct timespec d_report;
	} ts;
	size_t itc = 0; // iteration counter
	int bp; // bit pattern

	fprintf(stderr, ARGV0 ": allocating (%zu * 2) bytes of memory ...\n", s);

	m1 = malloc(s);
	m2 = malloc(s);
	if (m1 == NULL || m2 == NULL) {
		perror(ARGV0);
		goto END;
	}

	bp = 0xCD;
	memset(m1, bp, s);
	memset(m2, bp, s);

	fprintf(stderr, ARGV0 ": loop start: %s\n", getctime());

	clock_gettime(CLOCK_MONOTONIC, &ts.tp_start);
	ts.tp_report = ts.tp_start;

	while (true) {
		switch (itc % 2) {
		case 0: bp = 0x55; break;
		case 1: bp = 0xAA; break;
		}

		memset(m1, bp, s);
		memset(m2, bp, s);
		if (cointoss()) {
			inject_fault((uint8_t*)m1, s);
		}
		if (cointoss()) {
			inject_fault((uint8_t*)m2, s);
		}
		fr = memcmp(m1, m2, s);
		itc += 1;

		clock_gettime(CLOCK_MONOTONIC, &ts.tp_now);
		ts_sub(&ts.d_start, &ts.tp_now, &ts.tp_start);
		ts_sub(&ts.d_report, &ts.tp_now, &ts.tp_report);

		if (fr != 0) {
			dump_error(
				itc,
				&ts.d_start,
				(const uint8_t*)m1,
				(const uint8_t*)m2,
				s);
			ec = 1;
			goto END;
		}

		if (ts2d(&ts.d_report) >= opts.report_int) {
			do_report(&ts.d_start, itc);
			ts.tp_report = ts.tp_now;
		}
	}

END:
	free(m1);
	free(m2);
	return ec;
}

int main (const int argc, const char **argv) {
	int ec = 0;

	init_opts();
	init_g();

	if (!parse_opts(argc, argv)) {
		ec = 2;
		goto END;
	}

	if (opts.flags.help) {
		print_help();
		goto END;
	}

	if (!alloc_g()) {
		ec = 1;
		goto END;
	}

	ec = do_main();

END:
	dealloc_g();
	return ec;
}
