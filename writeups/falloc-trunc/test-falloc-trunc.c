#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

static struct stat st;

static void do_stat(const int fd) {
	if (fstat(fd, &st) != 0) {
		perror("fstat()");
		exit(1);
	}
}

int main (void) {
	static const char DATA[] = "hello, world!\n";
	const int fd = open("test", O_TRUNC | O_CREAT | O_RDWR, 0644);

	if (fd < 0) {
		perror("test");
		return 1;
	}

	// just a little sanity check
	do_stat(fd);
	assert(st.st_size == 0);

	fallocate(fd, FALLOC_FL_KEEP_SIZE, 0, 1024 * 1024);
	do_stat(fd);
	assert(st.st_size == 0);
	assert(st.st_blocks == 1024 * 1024 / 512);

	write(fd, DATA, sizeof(DATA));
	do_stat(fd);
	assert(st.st_size == sizeof(DATA));
	assert(st.st_blocks == 1024 * 1024 / 512);

	// In Linux, since st.st_size < new_length, the following will fail because
	// the kernel only extends without truncating the allocated blocks. All big
	// 3 filesystems(ext4, xfs, btrfs) behave the same way by failing this test.

	ftruncate(fd, sizeof(DATA) + 1);
	do_stat(fd);
	assert(st.st_size == sizeof(DATA) + 1);
	assert(st.st_blocks != 1024 * 1024 / 512);

	return 0;
}