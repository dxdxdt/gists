#define _GNU_SOURCE
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#define ARGV0 "lseek"

#if defined(SEEK_DATA) && defined(SEEK_HOLE)
#define HAS_SEEK_DH
#endif

#ifdef HAS_SEEK_DH

static off_t do_lseek (const int fd, off_t ofs, const int whence) {
	ofs = lseek(fd, ofs, whence);
	if (ofs < 0) {
		return -errno;
	}
	return ofs;
}

/* Open the file, find the first data segment range */
static bool find_one_data_seg (const char *path, const off_t from, off_t *a, off_t *b) {
	const int fd = open(path, O_RDONLY);
	int saved_errno;
	bool ret = false;

	if (fd < 0) {
		return false;
	}

	*a = do_lseek(fd, from, SEEK_DATA);
	if (*a < 0) {
		goto out;
	}
	else {
		*b = do_lseek(fd, *a, SEEK_HOLE);
	}

	ret = true;
out:
	saved_errno = errno;
	close(fd);
	errno = saved_errno;

	return ret;
}

#endif

static void usage (void) {
	fprintf(stderr, "Usage: "ARGV0" FILE ...\n");
#ifdef HAS_SEEK_DH
	fprintf(stderr, "Tests SEEK_DATA and SEEK_HOLE\n");
#else
	fprintf(stderr, "Tests lseek(..., 3)\n");
#endif
	exit(2);
}

int main (int argc, const char **argv) {
	unsigned int succ = 0, fail = 0;

	if (argc <= 1) {
		usage();
	}

	/* Scan for any option */
	for (int i = 1; i < argc; i += 1) {
		if (argv[i][0] == '-') {
			usage();
		}
	}

#ifdef HAS_SEEK_DH
	for (int i = 1; i < argc; i += 1) {
		const char *path = argv[i];
		off_t a = 0, b = 0;

		if (find_one_data_seg(path, 0, &a, &b)) {
			succ += 1;
			printf("%s: %lld %lld\n", path, (long long)a, (long long)b);
		}
		else {
			fail += 1;
			fprintf(stderr, ARGV0": %s: %s\n", path, strerror(errno));
		}
	}
#else
	/* Just test test invalid whence */
	for (int i = 1; i < argc; i += 1) {
		const char *path = argv[i];
		const int fd = open(path, O_RDONLY);

		if (fd < 0) {
			fail += 1;
			fprintf(stderr, ARGV0": %s: %s\n", path, strerror(errno));
		}
		else {
			off_t ret;
			const char *msg;

			errno = 0;
			ret = lseek(fd, 0, 3);
			msg = strerror(errno);
			fprintf(stderr, ARGV0": %s: %lld %s\n",
				path, (long long)ret, msg == NULL ? "(null)" : msg);

			close(fd);

			succ += 1;
		}
	}
#endif

	if (fail > 0) {
		return succ > 0 ? 3 : 1;
	}
	return 0;
}
