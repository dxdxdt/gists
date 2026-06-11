#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#define COLS (72)
static unsigned int cols_cnt;

static void put_hex(const uint8_t b)
{
	if (b == 0)
		return;

	if (cols_cnt >= COLS) {
		printf("\n");
		cols_cnt = 0;
	}

	printf("%02x", b);
	cols_cnt += 2;
}

static bool dump_hex(const int fd, uint8_t *buf, const size_t bufsize)
{
	const char *errmsg = NULL;
	off_t cur = 0, next;
	bool has_next;

	assert(sizeof(off_t) >= sizeof(size_t));

	for (;;) {
		/* skip hole */

		next = lseek(fd, cur, SEEK_DATA);
		if (next < 0) {
			switch (errno) {
			case EINVAL: /* no kernel support */
			case ESPIPE: /* not a regular file */
				next = cur;
				break;
			case ENXIO: /* EOF */
				goto eof;
			default: /* EIO */
				errmsg = "lseek()";
				goto err;
			}
		}

		/* process data */
		for (off_t i = cur; i < next; i += 1)
			put_hex(0);
		cur = next;

		/* find the end of this data range */
		next = lseek(fd, cur, SEEK_HOLE);
		if (next < 0) {
			switch (errno) {
			case EINVAL:
			case ESPIPE: /* not a regular file */
				break;
			case ENXIO: /* EOF... somehow? (TOCTOU) */
				goto eof;
			default:
				goto err;
			}

			has_next = false;
		} else
			has_next = true;
		lseek(fd, cur, SEEK_SET);

		/* do actual I/O */
		do {
			off_t toread = has_next ? next - cur : (off_t)bufsize;
			ssize_t rlen;

			if (toread > (off_t)bufsize)
				toread = (off_t)bufsize;

			rlen = read(fd, buf, (size_t)toread);
			if (rlen == 0)
				goto eof;
			else if (rlen < 0) {
				errmsg = "read()";
				goto err;
			} else
				assert(rlen <= toread);

			/* process data */
			for (ssize_t i = 0; i < rlen; i += 1)
				put_hex(buf[i]);
			cur += (size_t)rlen;
		} while (!has_next || cur < next);
	}

eof:
	/* edge case: hole at the end */
	next = lseek(fd, 0, SEEK_END);
	for (; cur < next; cur += 1)
		put_hex(0);

	/* terminate the line that was being written */
	if (cols_cnt > 0)
		printf("\n");
	return true;
err:
	perror(errmsg);
	return false;
}

int main(void)
{
	const long pagesize = sysconf(_SC_PAGESIZE);
	void *buf;

	assert(pagesize > 0);
	buf = malloc((size_t)pagesize);
	if (buf == NULL) {
		perror("malloc()");
		return EXIT_FAILURE;
	}

	return dump_hex(STDIN_FILENO, buf, (size_t)pagesize) ? EXIT_SUCCESS : EXIT_FAILURE;
}
