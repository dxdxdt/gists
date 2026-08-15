#ifdef __linux__
/* glibc can shut the fuck up */
#define _GNU_SOURCE
#else
#define _BSD_SOURCE
#define _NETBSD_SOURCE
#define _XOPEN_SOURCE 500
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <getopt.h>

#define ARGV0 "it-gettime"

struct clock_table {
	const char *name;
	clockid_t cid;
};

static const struct clock_table CLKTBL[] = {
#ifdef CLOCK_REALTIME
	{ .name = "realtime",		.cid = CLOCK_REALTIME, },
#endif
#ifdef CLOCK_REALTIME_ALARM
	{ .name = "realtime_alarm",	.cid = CLOCK_REALTIME_ALARM, },
#endif
#ifdef CLOCK_REALTIME_COARSE
	{ .name = "realtime_coarse",	.cid = CLOCK_REALTIME_COARSE, },
#endif
#ifdef CLOCK_TAI
	{ .name = "tai",		.cid = CLOCK_TAI, },
#endif
#ifdef CLOCK_MONOTONIC
	{ .name = "monotonic",		.cid = CLOCK_MONOTONIC, },
#endif
#ifdef CLOCK_MONOTONIC_COARSE
	{ .name = "monotonic_coarse",	.cid = CLOCK_MONOTONIC_COARSE, },
#endif
#ifdef CLOCK_MONOTONIC_RAW
	{ .name = "monotonic_raw",	.cid = CLOCK_MONOTONIC_RAW, },
#endif
#ifdef CLOCK_BOOTTIME
	{ .name = "boottime",		.cid = CLOCK_BOOTTIME, },
#endif
#ifdef CLOCK_BOOTTIME_ALARM
	{ .name = "boottime_alarm",	.cid = CLOCK_BOOTTIME_ALARM, },
#endif
#ifdef CLOCK_PROCESS_CPUTIME_ID
	{ .name = "process_cputime_id",	.cid = CLOCK_PROCESS_CPUTIME_ID, },
#endif
#ifdef CLOCK_THREAD_CPUTIME_ID
	{ .name = "thread_cputime_id",	.cid = CLOCK_THREAD_CPUTIME_ID, },
#endif
	{}
};

static bool parse_clockid(const char *str, clockid_t *out)
{
	const struct clock_table *c;

	if (str == NULL) {
		errno = EINVAL;
		return false;
	}

	for (c = CLKTBL; c->name != NULL; c++) {
		if (strcmp(str, c->name) == 0) {
			if (out != NULL)
				*out = c->cid;
			return true;
		}
	}

	errno = ENOENT;
	return false;
}

static void usage(void)
{
	const struct clock_table *c;

	printf("Usage: " ARGV0 " [-h] [-t CLOCK] <ITERATION>\n");

	printf("CLOCK:");
	for (c = CLKTBL; c->name != NULL; c++)
		printf(" %s", c->name);
	printf("\n");
}

int main(int argc, char *argv[])
{
	clockid_t cid = CLOCK_MONOTONIC;
	unsigned long n = 1;
	struct timespec ts[2] = {0}, sum = {0};

	for (;;) {
		const int c = getopt(argc, (char * const*)argv, "t:h");

		switch (c) {
		case 't':
			if (!parse_clockid(optarg, &cid)) {
				fprintf(stderr, ARGV0 ": -t %s: %s\n", optarg, strerror(errno));
				goto usage_err;
			}
			break;
		case 'h':
			usage();
			exit(0);
			break;
		default:
			if (c < 0)
				goto done;
			goto usage_err;
		}
	}
done:
	if (optind >= argc) {
		fprintf(stderr, ARGV0 ": too few arguments\n");
		goto usage_err;
	} else if (optind + 1 < argc) {
		fprintf(stderr, ARGV0 ": too many arguments\n");
		goto usage_err;
	} else {
		char *endptr = NULL;

		errno = 0;
		n = strtoul(argv[optind], &endptr, 0);
		if (argv[optind] == endptr || n == 0)
			errno = EINVAL;
		if (errno != 0) {
			fprintf(stderr, ARGV0 ": -- %s: %s\n", optarg, strerror(errno));
			goto usage_err;
		}
	}

	for (unsigned long i = 0; i < n; i++) {
		long a;

		for (size_t i = 0; i < 2; i++) {
			if (clock_gettime(cid, ts + i)) {
				fprintf(stderr, ARGV0 ": clock_gettime(%d, ...): %s\n",
							(int)cid, strerror(errno));
				return 1;
			}
		}

		/* Carry branchlessly for time invariance  */
		ts[1].tv_sec -= ts[0].tv_sec;
		ts[1].tv_nsec -= ts[0].tv_nsec;
		a = ts[1].tv_nsec < 0;
		ts[1].tv_sec -= (time_t)a;
		ts[1].tv_nsec += a * 1000000000;

		sum.tv_sec += ts[1].tv_sec;
		sum.tv_nsec += ts[1].tv_nsec;
		sum.tv_sec += sum.tv_nsec / 1000000000;
		sum.tv_nsec %= 1000000000;
	}

	printf("%" PRIuMAX ".%09ld\n", (uintmax_t)sum.tv_sec, sum.tv_nsec);

	return 0;
usage_err:
	return 2;
}
