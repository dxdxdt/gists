#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

static struct stat st;
#define L (1024 * 1024 * 1024)

static void do_stat(const int fd) {
	if (fstat(fd, &st) != 0) {
		perror("fstat()");
		exit(1);
	}
}

int main (void) {
	static const char DATA[] = { 'D' };
	const int fd = open("test", O_TRUNC | O_CREAT | O_RDWR, 0644);

	if (fd < 0) {
		perror("test");
		return 1;
	}

	write(fd, DATA, sizeof(DATA));

	// just a little sanity check
	do_stat(fd);
	assert(st.st_size == 1);

	fallocate(fd, FALLOC_FL_KEEP_SIZE, 0, L);
	do_stat(fd);
	assert(st.st_size == 1);
	assert(st.st_blocks == L / 512);

	lseek(fd, L, SEEK_SET);
	write(fd, DATA, sizeof(DATA));
	do_stat(fd);

	return 0;
}