#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

#include <getopt.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>

#define ARGV0 "handle-sigbus"

static struct {
	const char *path;
	bool truncate:1;
} param;

static struct {
	void *addr;
	size_t len;
	off_t offset;
	long pagesize;
	pid_t sentry;
	int channel;
} g = {
	.addr = MAP_FAILED,
	.sentry = -1,
	.channel = -1,
};

static struct {
	size_t offset;
	void *addr;
	int code;
} msg;

static void usage(void)
{
	printf("SIGBUS handling demo(map a file and read the last page)\n"
		"Usage: "ARGV0" [-th] <FILE>\n"
		"Options:\n"
		"  -t  truncate the last page before accessing it\n"
		"  -h  print this message and exit");
}

static void parse_opts(int argc, char *argv[])
{
	for (;;) {
		const int c = getopt(argc, (char *const *)argv, "th");

		switch (c) {
		case 't':
			param.truncate = true;
			break;
		case 'h':
			usage();
			exit(0);
		case '?':
			usage();
			exit(2);
		default:
			if (c < 0)
				goto out;
		}
	}

out:
	if (optind + 1 != argc) {
		usage();
		exit(2);
	}
	param.path = argv[optind];
}

static void handler(int signo, siginfo_t *info, void *context)
{
	uintptr_t a, b, c;

	(void)context;
	assert(signo == SIGBUS);

	/* Is the address known? */
	a = (uintptr_t)g.addr;
	b = (uintptr_t)info->si_addr;
	c = (uintptr_t)a + g.len;
	if (a <= b && b < c)
		goto yes;

	/* We don't know about the page that caused the bus error. */
	return;
yes:
	msg.offset = b - a;
	msg.addr = info->si_addr;
	msg.code = info->si_code;

	write(g.channel, &msg, sizeof(msg));
	close(g.channel);
	waitpid(g.sentry, NULL, 0);
}

static void sentry_main(int fd)
{
	ssize_t rlen;
	const char *err_type;

	rlen = read(fd, &msg, sizeof(msg));
	if (rlen == 0)
		return;
	else
		assert(rlen == sizeof(msg));

	switch (msg.code) {
	case BUS_ADRALN:
		err_type = "(BUS_ADRALN)";
		break;
	case BUS_ADRERR:
		err_type = "(BUS_ADRERR)";
		break;
	case BUS_OBJERR:
		err_type = "(BUS_OBJERR)";
		break;
#ifdef BUS_MCEERR_AR
	case BUS_MCEERR_AR:
		err_type = "(BUS_MCEERR_AR)";
		break;
#endif
#ifdef BUS_MCEERR_AO
	case BUS_MCEERR_AO:
		err_type = "(BUS_MCEERR_AO)";
		break;
#endif
	default:
		err_type = "";
	}

	/* Don't use PRIxPTR here because Solaris seems to be allergic to it. */
	fprintf(stderr, "\n"ARGV0": SIGBUS received: offset=%zu, addr=0x%llx, code=%d%s\n",
		msg.offset, (unsigned long long)msg.addr, msg.code, err_type);
}

int main(int argc, char *argv[])
{
	int ret = 1;
	int fd = -1;
	int channel[2] = { -1, -1 };
	struct sigaction sa = {
		.sa_flags = SA_SIGINFO | SA_RESETHAND,
		.sa_sigaction = handler,
	};
	int oflag;
	int err;
	off_t flen;
	size_t aligned;
	char *p;

	g.pagesize = sysconf(_SC_PAGESIZE);
	assert(g.pagesize > 0);

	parse_opts(argc, argv);

	/* Spawn sentry. */

	fflush(stdout);
	fflush(stderr);

	if (pipe(channel) < 0)
		goto g_err;
	g.sentry = fork();
	if (g.sentry < 0)
		goto g_err;
	else if (g.sentry == 0) {
		close(channel[1]);
		sentry_main(channel[0]);
		close(channel[0]);
		exit(0);
	}

	close(channel[0]);
	channel[0] = -1;
	g.channel = channel[1];

	/* Map file. */

	if (param.truncate)
		oflag = O_RDWR;
	else
		oflag = O_RDONLY;

	fd = open(param.path, oflag);
	if (fd < 0)
		goto f_err;

	flen = lseek(fd, 0, SEEK_END);
	if (flen < 0)
		goto f_err;
	else if (flen == 0) {
		errno = ENXIO;
		goto f_err;
	} else if (SSIZE_MAX < flen) {
		errno = ENOMEM;
		goto f_err;
	}
	aligned = g.len = (size_t)flen;
	aligned -= 1;
	aligned &= ~((size_t)g.pagesize - 1);

	p = g.addr = mmap(NULL, g.len, PROT_READ, MAP_SHARED, fd, g.offset);
	if (g.addr == MAP_FAILED)
		goto f_err;

	err = sigaction(SIGBUS, &sa, NULL);
	assert(err == 0);
	(void)err;

	/* Cause fault. */

	if (param.truncate) {
		/* Truncate one page off the file. */
		err = ftruncate(fd, (off_t)aligned);
		if (err < 0)
			goto f_err;

		unlink(param.path);
	}

	/* Access the contents of the last page. */
	for (size_t i = aligned; i < g.len; i++)
		printf("%02x", p[i]);
	printf("\n");

	ret = 0;
	goto out;
f_err:
	fprintf(stderr, ARGV0": %s: %s\n", param.path, strerror(errno));
	goto out;
g_err:
	perror(ARGV0);
	goto out;
out:
	if (g.addr != MAP_FAILED) {
		msync(g.addr, g.len, MS_SYNC);
		munmap(g.addr, g.len);

		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = SIG_DFL;
		sigaction(SIGBUS, &sa, NULL);
	}
	if (fd >= 0)
		close(fd);

	for (size_t i = 0; i < 2; i++) {
		if (channel[i] >= 0)
			close(channel[i]);
	}
	if (g.sentry > 0)
		waitpid(g.sentry, NULL, 0);

	return ret;
}
