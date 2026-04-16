#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#define ARGV0 "zm"
#define ALARM_INTERVAL (5)
static volatile unsigned long long size;

static void on_alarm (const int sn) {
	assert(sn == SIGALRM);
	(void)sn;

	fprintf(stderr, "%llu\n", size);

	alarm(ALARM_INTERVAL);
}

static void do_memcmp (const size_t pagesize, const uint8_t *m) {
	uint8_t zm[pagesize];

	memset(zm, 0, pagesize);

	while (size > 0) {
		const size_t len = pagesize > size ? size : pagesize;
		const int ret = memcmp(m, zm, len);

		assert(ret == 0);

		m += len;
		size -= len;
	}
}

static void with_zm (const size_t pagesize) {
	static const char *DEV_ZERO = "/dev/zero";
	uint8_t *m;
	int fd;

	fd = open(DEV_ZERO, O_RDONLY);
	if (fd < 0) {
		perror(DEV_ZERO);
		exit(1);
	}

	m = mmap(NULL, (size_t)size, PROT_READ, MAP_SHARED, fd, 0);
	if (m == MAP_FAILED) {
		fprintf(stderr, "-s %llu: %s\n", size, strerror(errno));
		exit(1);
	}

	do_memcmp(pagesize, m);

	munmap(m, (size_t)size);
	close(fd);
}

static void with_calloc (const size_t pagesize) {
	uint8_t *m = calloc(1, (size_t)size);

	if (m == NULL) {
		fprintf(stderr, "-s %llu: %s\n", size, strerror(errno));
		exit(1);
	}

	do_memcmp(pagesize, m);

	free(m);
}

static void usage (void) {
	fprintf(stderr, "Usage: "ARGV0" -m|-c -s SIZE\n");
	exit(2);
}

int main (const int argc, const char **argv) {
	const long pagesize = sysconf(_SC_PAGESIZE);
	unsigned int mode = 0;
	int ret;

	assert(pagesize > 0);

	for (;;) {
		ret = getopt(argc, (char *const *)argv, "s:mc");

		switch (ret) {
		case 's':
			errno = 0;
			size = strtoull(optarg, NULL, 0);
			if (size == 0) {
				if (errno == 0) {
					errno = EINVAL;
				}
				goto parse_fail;
			}
			if (size > SIZE_MAX) {
				errno = EOVERFLOW;
				goto parse_fail;
			}
			break;
parse_fail:
			fprintf(stderr, "-s %llu: %s\n", size, strerror(errno));
			exit(2);
		case 'm':
			mode |= 1;
			break;
		case 'c':
			mode |= 2;
			break;
		case -1:
			goto opts_out;
		default:
			usage();
		}
	}

opts_out:
	if (size == 0)
		usage();

	ret = signal(SIGALRM, on_alarm) != SIG_ERR;
	assert(ret);
	alarm(ALARM_INTERVAL);

	switch (mode) {
	case 1:
		with_zm(pagesize);
		break;
	case 2:
		with_calloc(pagesize);
		break;
	default:
		usage();
	}

	alarm(0);
	signal(SIGALRM, SIG_DFL);

	return 0;
}
