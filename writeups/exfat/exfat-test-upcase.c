#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <wchar.h>
#include <wctype.h>
#include <assert.h>
#include <locale.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

static void do_test(const size_t i, const wint_t n, const char *a, const char *b)
{
	int fd;
	struct stat st[2] = {
		{ .st_ino = 0 },
		{ .st_ino = 0 }
	};

	fd = open(a, O_RDWR | O_CREAT | O_TRUNC, 0644);
	if (fd < 0 || fstat(fd, &st[0]) != 0) {
		printf("%04zX(%s): %s\n", i, a, strerror(errno));
		exit(1);
	}

	if (stat(b, &st[1]) == 0 && st[0].st_ino == st[1].st_ino) {
		printf("%04zX(%s) -> %"PRIX16"(%s)\n", i, a, (uint16_t)n, b);
		fflush(stdout);
	}

	close(fd);
	unlink(a);
}

static bool should_skip (const wint_t c)
{
	static const unsigned short exfat_bad_uni_chars[] = {
		0x0022, '.',    0x002A, 0x002F, 0x003A,
		0x003C, 0x003E, 0x003F, 0x005C, 0x007C,
		0
	};

	if (!iswprint(c)) {
		return true;
	}

	for (const unsigned short *p = exfat_bad_uni_chars; *p != 0; p += 1) {
		if (c == *p)
			return true;
	}

	return false;
}

int main (void)
{
	/* test output file names in native encoding(probably UTF-8) */
	static char name_a[256];
	static char name_b[256];
	static char *lc;

	lc = setlocale(LC_ALL, "C.UTF-8");
	assert(lc != NULL);
	(void)lc;

	for (wint_t a = 1; a <= 0xFFFF; a += 1) {
		if (should_skip(a) || iswupper(a))
			continue;

		fprintf(stderr, "%04zX ...\n", (size_t)a);

		for (wint_t b = 1; b <= 0xFFFF; b += 1) {
			if (a == b || should_skip(b))
				continue;

			name_a[0] = name_b[0] = 0;
			snprintf(name_a, sizeof(name_a), "%lc", a);
			snprintf(name_b, sizeof(name_b), "%lc", b);
			assert(name_a[0] != 0 && name_b[0] != 0);

			do_test((size_t)a, b, name_a, name_b);
		}
	}

	return 0;
}
