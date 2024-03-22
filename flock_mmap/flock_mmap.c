#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/types.h>

static const size_t M_SIZE = 1;
static bool use_fcntl = false;
static bool close_fd = true;
static bool should_wait = false;
static bool verbose = false;
static const char *path;

static bool parse_args (const int argc, const char **argv) {
	int opt;

	while ((opt = getopt(argc, (char *const*)argv, "fnwV")) != -1) {
		switch (opt) {
		case 'f': use_fcntl = true; break;
		case 'n': close_fd = false; break;
		case 'w': should_wait = true; break;
		case 'V': verbose = true; break;
		default: return false;
		}
	}

	if (optind >= argc) {
		return false;
	}
	else {
		path = argv[optind];
	}

	return true;
}

int main (const int argc, const char **argv) {
	int ec = 0;
	int fd = -1;
	int f_ret;
	void *m = MAP_FAILED;

	if (!parse_args(argc, argv)) {
		ec = 2;
		fprintf(stderr, "Usage: %s -fnw <lock file path>\n", argv[0]);
		goto END;
	}

	if (!verbose) {
		close(STDOUT_FILENO);
	}

	fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0755);
	if (fd < 0) {
		ec = 1;
		perror(path);
		goto END;
	}

	if (use_fcntl) {
		struct flock fl;

		memset(&fl, 0, sizeof(fl));
		fl.l_type = F_WRLCK;
		fl.l_whence = SEEK_SET;
		fl.l_len = M_SIZE;

		f_ret = fcntl(fd, F_SETLK, &fl);
	}
	else {
		f_ret = flock(fd, LOCK_EX | LOCK_NB);
	}

	if (f_ret != 0) {
		ec = 1;
		perror(path);
		goto END;
	}

	f_ret = ftruncate(fd, M_SIZE);
	if (f_ret != 0) {
		ec = 1;
		perror(path);
		goto END;
	}

	m = mmap(NULL, M_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (m == MAP_FAILED) {
		ec = 1;
		perror(path);
		goto END;
	}

	// This block
	if (close_fd) {
		close(fd);
		fd = -1;
	}

	printf("Lock holding\n");
	if (should_wait) {
		pause();
	}

END:
	if (fd >= 0) {
		close(fd);
	}
	if (m != MAP_FAILED) {
		munmap(m, M_SIZE);
	}
	return ec;
}
