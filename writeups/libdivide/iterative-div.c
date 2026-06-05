#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>

/* Deduces integer division op to faster ops including multiply and bitshift */
#include "libdivide.h"

/* Useful libbsd macros */

#define	timespecsub(tsp, usp, vsp)					\
	do {								\
		(vsp)->tv_sec = (tsp)->tv_sec - (usp)->tv_sec;		\
		(vsp)->tv_nsec = (tsp)->tv_nsec - (usp)->tv_nsec;	\
		if ((vsp)->tv_nsec < 0) {				\
			(vsp)->tv_sec--;				\
			(vsp)->tv_nsec += 1000000000L;			\
		}							\
	} while (0)
#define	timespeccmp(tsp, usp, cmp)					\
	(((tsp)->tv_sec == (usp)->tv_sec) ?				\
	    ((tsp)->tv_nsec cmp (usp)->tv_nsec) :			\
	    ((tsp)->tv_sec cmp (usp)->tv_sec))

/* Report interval: 1 second */
static const struct timespec REPORT_INTV = { .tv_sec = 1, .tv_nsec = 0, };

/* picture dimension times bytes per pixel(RGB32: 3 bytes) */
static size_t frame_size = 640 * 360 * 3;
static uint8_t *frame = NULL;
static uint32_t *frame_avg = NULL; /* Sum of recent frames, capped at `divisor` */
static uint8_t divisor = 180;
/* Number of currently accumulated frames */
static uint8_t acc_cnt = 1;
/* Iteration count since the last report */
static unsigned long iter_cnt;
/* Set to use fast division. Clear to use DIV instruction of the host CPU */
static bool cache_op = false;
/* Use branch-free variant of libdivide  */
static bool branch_free = false;
/* Set to skip calculation of `frame_avg` */
static bool passthrough = false;
// /* Execution timeout */
static unsigned int timeout;
static bool help;

#define ARGV0 "iterative-div"

static __attribute__((noinline)) void parse_args(int argc, char **argv)
{
	for (;;) {
		const int c = getopt(argc, (char *const *)argv, "s:d:cpt:bh");

		if (c < 0)
			break;
		switch (c) {
		case 's':
			if (sscanf(optarg, "%zu", &frame_size) != 1 || frame_size == 0) {
				fprintf(stderr, ARGV0": -s %s: %s\n", optarg, strerror(EINVAL));
				exit(2);
			}
			break;
		case 'd':
			if (sscanf(optarg, "%"SCNu8, &divisor) != 1 || divisor == 0) {
				fprintf(stderr, ARGV0": -d %s: %s\n", optarg, strerror(EINVAL));
				exit(2);
			}
			if (divisor + 1 == 0 || divisor == 1) {
				fprintf(stderr, ARGV0": -d %s: %s\n", optarg, strerror(ERANGE));
				exit(2);
			}
			break;
		case 'c':
			cache_op = true;
			break;
		case 'p':
			passthrough = true;
			break;
		case 't':
			if (sscanf(optarg, "%u", &timeout) != 1) {
				fprintf(stderr, ARGV0": -t %s: %s\n", optarg, strerror(EINVAL));
				exit(2);
			}
			break;
		case 'b':
			branch_free = true;
			break;
		case 'h':
			help = true;
			break;
		default:
			exit(2);
		}
	}

	if (optind < argc) {
		fprintf(stderr, ARGV0": too many arguments\n");
		exit(2);
	}
}

static __attribute__((noinline)) int do_full_io(const int fd, const int what, void *buf, size_t *len)
{
	ssize_t ioret;
	char *p = buf;

	while (*len > 0) {
		switch (what) {
		case 0:
			ioret = read(fd, p, *len);
			break;
		case 1:
			ioret = write(fd, p, *len);
			break;
		default:
			abort();
		}

		if (ioret < 0)
			return -1;
		if (ioret == 0) {
			if (what == 0)
				return 0;
			errno = EIO;
			return -1;
		}

		p += ioret;
		*len -= (size_t)ioret;
	}

	return 1;
}

static void do_average_frame(void)
{
	const bool ovf = __builtin_add_overflow(acc_cnt, 1, &acc_cnt);

	assert(!ovf);
	(void)ovf;

	if (cache_op) {
		if (branch_free) {
			struct libdivide_u32_branchfree_t d = libdivide_u32_branchfree_gen(acc_cnt);

			for (size_t i = 0; i < frame_size; i += 1) {
				uint32_t avg = frame_avg[i];

				avg += frame[i];
				frame[i] = libdivide_u32_branchfree_do(avg, &d);
				frame_avg[i] = avg;
			}
			if (acc_cnt > divisor) {
				for (size_t i = 0; i < frame_size; i += 1)
					frame_avg[i] -= libdivide_u32_branchfree_do(frame_avg[i],
										&d);

				acc_cnt = divisor;
			}
		} else {
			struct libdivide_u32_t d = libdivide_u32_gen(acc_cnt);

			for (size_t i = 0; i < frame_size; i += 1) {
				uint32_t avg = frame_avg[i];

				avg += frame[i];
				frame[i] = libdivide_u32_do(avg, &d);
				frame_avg[i] = avg;
			}
			if (acc_cnt > divisor) {
				for (size_t i = 0; i < frame_size; i += 1)
					frame_avg[i] -= libdivide_u32_do(frame_avg[i], &d);

				acc_cnt = divisor;
			}
		}
	} else {
		for (size_t i = 0; i < frame_size; i += 1) {
			uint32_t avg = frame_avg[i];

			avg += frame[i];
			frame[i] = avg / acc_cnt;
			frame_avg[i] = avg;
		}
		if (acc_cnt > divisor) {
			for (size_t i = 0; i < frame_size; i += 1)
				frame_avg[i] -= frame_avg[i] / acc_cnt;

			acc_cnt = divisor;
		}
	}
}

static __attribute__((noinline)) void do_iteration(void)
{
	int fr;
	size_t l;

	l = frame_size;
	fr = do_full_io(STDIN_FILENO, 0, frame, &l);
	if (fr <= 0) {
		if (fr < 0)
			perror(ARGV0": stdin");
		else
			fprintf(stderr, ARGV0": stdin: read fell short(read = %zu, buf = %zu)\n",
				l, frame_size);

		exit(1);
	}

	if (!passthrough)
		do_average_frame();

	l = frame_size;
	fr = do_full_io(STDOUT_FILENO, 1, frame, &l);
	if (fr < 0) {
		perror(ARGV0": stdout");
		exit(1);
	}
}

int main(int argc, char **argv)
{
	struct {
		struct timespec now;
		struct timespec last_report;
		struct timespec dt;
	} ts;
	bool ovf = false;
	const char *divide_type = NULL;

	parse_args(argc, argv);

	if (help) {
		printf("Usage: "ARGV0" [-cpbh] [-s FRAME_SIZE] [-d DIVISOR] [-t TIMEOUT]\n");
		exit(0);
	}

	if (isatty(STDOUT_FILENO)) {
		fprintf(stderr, ARGV0": refusing to clobber stdout\n");
		exit(2);
	}

	if (timeout != 0)
		alarm(timeout);

	if (cache_op) {
		if (branch_free)
			divide_type = "branchless magic";
		else
			divide_type = "branchy magic";
	} else
		divide_type = "processor";

	fprintf(stderr, ARGV0": frame_size = %zu\n", frame_size);
	fprintf(stderr, ARGV0": divisor    = %"PRIu32"\n", divisor);
	fprintf(stderr, ARGV0": divide     = %s\n", divide_type);

	frame = malloc(frame_size);
	frame_avg = calloc(frame_size, sizeof(uint32_t));
	if (frame == NULL || frame_avg == NULL) {
		perror(ARGV0);
		exit(1);
	}

	clock_gettime(CLOCK_MONOTONIC, &ts.last_report);

	for (;;) {
		do_iteration();
		ovf |= __builtin_add_overflow(iter_cnt, 1, &iter_cnt);

		clock_gettime(CLOCK_MONOTONIC, &ts.now);
		timespecsub(&ts.now, &ts.last_report, &ts.dt);
		if (timespeccmp(&REPORT_INTV, &ts.dt, <=)) {
			fprintf(stderr, "[%10lu.%09lu] %10lu %s\n", (unsigned long)ts.dt.tv_sec,
					(unsigned long)ts.dt.tv_nsec, iter_cnt,
					ovf ? "(invalid: overflow occurred)" : "");

			ts.last_report = ts.now;
			iter_cnt = 0;
			ovf = false;
		}
	}
}
