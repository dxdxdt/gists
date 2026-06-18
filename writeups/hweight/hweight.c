#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <limits.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>

static unsigned int popcnt_intr(const void *bitmap, const size_t bitmap_len)
{
	const size_t lc = bitmap_len / sizeof(unsigned long);
	unsigned int ret = 0;

	for (size_t i = 0; i < lc; i++)
		ret += __builtin_popcountl(((unsigned long*)bitmap)[i]);
	for (size_t i = lc * sizeof(unsigned long); i < bitmap_len; i++)
		ret += __builtin_popcountl(((unsigned char*)bitmap)[i]);

	return ret;
}

static const unsigned char used_bit[] = {
	0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 1, 2, 2, 3,/*  0 ~  19*/
	2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 1, 2, 2, 3, 2, 3, 3, 4,/* 20 ~  39*/
	2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5,/* 40 ~  59*/
	4, 5, 5, 6, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,/* 60 ~  79*/
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 2, 3, 3, 4,/* 80 ~  99*/
	3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6,/*100 ~ 119*/
	4, 5, 5, 6, 5, 6, 6, 7, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4,/*120 ~ 139*/
	3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,/*140 ~ 159*/
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5,/*160 ~ 179*/
	4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 2, 3, 3, 4, 3, 4, 4, 5,/*180 ~ 199*/
	3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6,/*200 ~ 219*/
	5, 6, 6, 7, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,/*220 ~ 239*/
	4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8             /*240 ~ 255*/
};

static unsigned int popcnt_lu(const void *bitmap, const size_t bitmap_len)
{
	unsigned int ret = 0;
	unsigned char bits;
	unsigned char v;

	for (size_t i = 0; i < bitmap_len; i++) {
		v = ((unsigned char*)bitmap)[i];
		bits = used_bit[v];
		ret += bits;
	}

	return ret;
}

static unsigned int popcnt_c(const void *bitmap, const size_t bitmap_len)
{
	const unsigned char *c = bitmap;
	unsigned int ret = 0;

	/* For each char */
	for (size_t i = 0; i < bitmap_len; i++, c++) {
		unsigned char n = *c;

		/* For each bit in char */
		/*
		 * Counting branchlessly(not exiting the loop early when n becomes zero) for
		 * constant execution time.
		 */
		for (unsigned int j = 0; j < CHAR_BIT; j += 1) {
			ret += n & 1;
			n >>= 1;
		}
	}

	return ret;
}

int main (int argc, char **argv)
{
	bool textmode = false;
	int c;
	unsigned int cnt = 0;
	unsigned int (*popcnt_f)(const void *bitmap, const size_t bitmap_len) = NULL;

	while ((c = getopt(argc, argv, "ilct")) != -1) {
		switch (c) {
		case 'i':
			popcnt_f = popcnt_intr;
			break;
		case 'l':
			popcnt_f = popcnt_lu;
			break;
		case 'c':
			popcnt_f = popcnt_c;
			break;
		case 't':
			textmode = true;
			break;
		default:
			exit(2);
		}
	}

	if (popcnt_f == NULL) {
		fprintf(stderr, "-i, -l or -c required\n");
		exit(2);
	}

	if (textmode) {
		char line[256];
		unsigned int n;

		while (fgets(line, sizeof(line), stdin) != NULL) {
			n = 0;

			errno = EINVAL;
			if (sscanf(line, "%u", &n) != 1) {
				if (errno != 0)
					perror(NULL);
				continue;
			}

			cnt = popcnt_f(&n, sizeof(n));
			printf("%u: %u\n", n, cnt);
		}
	} else {
		const size_t len = 0xFFFFFFFF / 8;
		ssize_t rw;
		void *bitmap;

		bitmap = malloc(len);
		if (bitmap == NULL) {
			perror(NULL);
			exit(1);
		}

		rw = read(STDIN_FILENO, bitmap, len);
		if (rw < 0) {
			perror(NULL);
			exit(1);
		}

		cnt = popcnt_f(bitmap, (size_t)rw);
		printf("%u\n", cnt);
	}

	return 0;
}
