/*
 * Demonstrates/tests mmap()'ing the same range of pages twice to construct a
 * circular buffer(ring buffer). Both kernel and userspace should be able to use
 * the memory past the first range of page. This way, memory access doesn't need
 * to be broken into two rounds when it goes past the buffer size bound.
 * It is especially useful when libc memory functions like memcpy() and/or
 * syscalls like read()/write() are involved in handling the circular buffer.
 *
 * Memory functions including memcpy() and memmove() have some overhead upon
 * entry because they have to handle and optimise unaligned memory access.
 * Syscalls incur user-kernel context switching. With this hack, the use of
 * those functions can be reduced to only once.
 *
 * One caveat is that this hack doesn't mix well with fork(). There's no
 * semantics in POSIX for making a memory-file-backed shared mapping that
 * wouldn't survive fork(). There are some guards that can be placed, though.
 *
 * Another nuisance is that it leverages heavily on the power of MMU and virtual
 * address space. MMU and virtual address space are not requirement of UNIX
 * systems. On such systems, MAP_SHARED would not be supported.
 */

#include "mmagic.h"
#if defined(__sun) && !defined(_XOPEN_SOURCE) /* Solaris */
/* getopt() */
#define _XOPEN_SOURCE 500
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>

#include <getopt.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define ARGV0 "mmap-buffer-test"

static struct {
	size_t size;
	int memfile_flags;
	bool wrap:1;
	bool stop:1;
	bool help:1;
} param = {
	.size = 1,
};

typedef char pagedata_t;

static void parse_opts(int argc, char *argv[])
{
	for (;;) {
		const int c = getopt(argc, (char *const *)argv, "s:wmSh");

		switch (c) {
		case 's':
			if (sscanf(optarg, "%zu", &param.size) != 1) {
				fprintf(stderr, ARGV0": -s %s: %s", optarg, strerror(EINVAL));
				goto err;
			}
			break;
		case 'w':
			param.wrap = true;
			break;
		case 'm':
			param.memfile_flags |= MEMFILE_BACKING_SHM;
			break;
		case 'S':
			param.stop = true;
			break;
		case 'h':
			param.help = true;
			break;
		default:
			if (c < 0)
				goto done;
			goto err;
		}
	}
done:
	return;
err:
	exit(2);
}

static char charn(unsigned long n)
{
	static const char ARR[] = {
		'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
		'M', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
		'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
		'm', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '\n'
	};
	static const size_t ARR_LEN = sizeof(ARR) / sizeof(ARR[0]);

	return ARR[n % ARR_LEN];
}

int main(int argc, char *argv[])
{
	int ret = 0;
	const char *errmsg = NULL;
	size_t size, len, cnt;
	void *addr = NULL;
	pagedata_t *arr;
	int fd;
	int out_flags;
	int saved_errno;
	char flags_str[256];
	struct stat st;

	parse_opts(argc, argv);

	if (param.help) {
		printf("Usage: " ARGV0 " [-s SIZE] [-wmSh]\n");
		exit(0);
	}

	out_flags = param.memfile_flags;
	fd = mkmemfile(&out_flags);
	if (fd < 0) {
		errmsg = "mkmemfile()";
		goto err;
	}
	fprintf(stderr, "backing: %s\n", memfile_backing_name(out_flags));

	addr = mmemfile(fd, 0, param.size, &len, &size, &out_flags);
	saved_errno = errno;
	if (addr == NULL) {
		errmsg = "mmemfile()";
		goto err;
	}
	arr = addr;
	cnt = len / sizeof(pagedata_t);

	for (size_t i = 0; i < cnt; i++)
		arr[i] = charn(i);

	st.st_blocks = -1;
	fstat(fd, &st);
	flags_str[0] = 0;
	memfile_flags_name(out_flags, sizeof(flags_str), flags_str);
	fprintf(stderr, "flags: %lld %s\n", (long long)st.st_blocks, flags_str);
	if (!(out_flags & MEMFILE_NO_INHERIT))
		fprintf(stderr, ARGV0 ": inherit: %s\n", strerror(saved_errno));

	assert(memcmp(addr, (void*)((uintptr_t)addr + len), len) == 0);

	if (param.wrap) {
		const void *wofs = (const void*)((uintptr_t)addr + len / 2);

		/* The kernel should see the same data */
		write(STDOUT_FILENO, wofs, len);
	} else {
		const void *wofs = (const void*)((uintptr_t)addr + len / 2);
		const size_t wlen = len / 2;

		/* Traditional buffer wrap around for test control */
		write(STDOUT_FILENO, wofs, wlen);
		write(STDOUT_FILENO, addr, wlen);
	}

	if (param.stop) {
		fprintf(stderr, "Stopping (PID: %d)\n ...", (int)getpid());
		raise(SIGSTOP);
	}

	goto out;
err:
	fprintf(stderr, ARGV0": %s: %s\n", errmsg, strerror(errno));
	ret = 1;
out:
	if (addr != NULL)
		munmap(addr, size);
	if (fd >= 0)
		close(fd);
	return ret;
}
