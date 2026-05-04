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

#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/vfs.h>

#include <linux/magic.h>

static struct {
	const char *path;
	uint16_t start;
	uint16_t step;
	bool mode_u:1;
	bool mode_l:1;
} ui = {
	.step = 1,
};

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
		printf("%04zX(%s) -> %04"PRIX16"(%s)\n", i, a, (uint16_t)n, b);
		fflush(stdout);
	}

	close(fd);
	unlink(a);
}

static bool should_skip (const wint_t c)
{
	switch (c) {
	case 0x0022: case '.':    case 0x002A: case 0x002F: case 0x003A:
	case 0x003C: case 0x003E: case 0x003F: case 0x005C: case 0x007C:
		return true;
	}

	return !iswprint(c);
}

static void usage (const char *argv0)
{
	fprintf(stderr, "Usage: %s [-ul] [-S START] [-C STEP] [PATH]\n", argv0);
	exit(2);
}

static uint16_t read_num_opt (const char *opt, const char *str)
{
	int saved_errno;
	unsigned long tmp;
	char *end = NULL;

	saved_errno = errno;
	errno = 0;

	tmp = strtoul(optarg, &end, 0);
	if (tmp > UINT16_MAX)
		errno = EOVERFLOW;
	if (tmp == 0)
		errno = EINVAL;

	if (errno != 0) {
		fprintf(stderr, "%s %s: %s\n", opt, str, strerror(errno));
		exit(2);
	}
	if (end != NULL && *end != 0) {
		fprintf(stderr, "%s %s: garbage at the end of uint16 value\n", opt, str);
		exit(2);
	}

	errno = saved_errno;
	return (uint16_t)tmp;
}

static void parse_opts (int argc, const char **argv)
{
	int ret;


	for (;;) {
		ret = getopt(argc, (char *const*)argv, "ulS:C:");

		switch (ret) {
		case 'u':
			ui.mode_u = true;
			break;
		case 'l':
			ui.mode_l = true;
			break;
		case 'S':
			ui.start = read_num_opt("-S", optarg);
			break;
		case 'C':
			ui.step = read_num_opt("-C", optarg);
			break;
		case -1:
			goto out;
		default:
			usage(argv[0]);
		}
	}
out:
	if (optind < argc)
		ui.path = argv[optind];
}

/*
 * test every character in the entire BMP excluding upper-case chars. By doing
 * so, the contents of the upcase table in the kernel can be "extracted".
 */
static void run_test (void)
{
	char name_a[256];
	char name_b[256];

	for (wint_t a = ui.start; a <= 0xFFFF; a += ui.step) {
		if (should_skip(a))
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
}

/* test isw(FILTER) to tow(CONV) */
static void run_test_f (int(*filter)(wint_t), wint_t(*conv)(wint_t))
{
	char name_a[256];
	char name_b[256];

	for (wint_t a = ui.start; a <= 0xFFFF; a += ui.step) {
		wint_t b;

		if (should_skip(a) || !filter(a))
			continue;

		b = conv(a);
		if (a == b)
			/* not all chars are necessarily lower and upper pair */
			continue;

		name_a[0] = name_b[0] = 0;
		snprintf(name_a, sizeof(name_a), "%lc", a);
		snprintf(name_b, sizeof(name_b), "%lc", b);
		assert(name_a[0] != 0 && name_b[0] != 0);

		do_test((size_t)a, b, name_a, name_b);
	}
}

int main (int argc, const char **argv)
{
	static char *lc;
	static struct statfs sf;
	static char cwd[4096];

	lc = setlocale(LC_ALL, "C.UTF-8");
	assert(lc != NULL);
	(void)lc;

	parse_opts(argc, argv);
	fprintf(stderr, "start=0x%04"PRIX16", step=0x%04"PRIX16"\n", ui.start, ui.step);

	if (ui.path != NULL)
		if (chdir(ui.path) != 0) {
			perror(ui.path);
			return EXIT_FAILURE;
		}

	getcwd(cwd, sizeof(cwd));

	if (statfs(".", &sf) != 0) {
		perror(cwd);
		return EXIT_FAILURE;
	}
	if (sf.f_type != EXFAT_SUPER_MAGIC) {
		fprintf(stderr, "%s: fstype not exFAT (actual f_type: %lX)\n",
			cwd, sf.f_type);
		return EXIT_FAILURE;
	}

	if (ui.mode_u)
		run_test_f(iswupper, towlower);
	else if (ui.mode_l)
		run_test_f(iswlower, towupper);
	else
		run_test();

	return 0;
}
