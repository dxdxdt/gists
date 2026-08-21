#if defined(__sun) && !defined(_XOPEN_SOURCE) /* Solaris */
/* getopt() */
#define _XOPEN_SOURCE 500
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>

#include <getopt.h>

#include "normalpath.h"

#define ARGV0 "normalpath"

struct {
	bool help:1;
	bool print0:1;
	bool stop:1;
} param;

int main(int argc, char *argv[])
{
	bool ret;

	for (;;) {
		const int c = getopt(argc, (char *const *)argv, "h0"
#if DEBUG_NORMALPATH
				"S"
#endif
			);

		switch (c) {
		case 'h':
			param.help = true;
			break;
		case '0':
			param.print0 = true;
			break;
		case 'S':
			param.stop = true;
			break;
		default:
			if (c < 0)
				goto done;
			return 2;
		}
	}
done:
	if (param.help) {
		printf("Usage: " ARGV0 " [-h0] <PATH ...>\n");
		return 0;
	}

	ret = optind < argc;

	for (; optind < argc; optind++) {
		char *a = argv[optind];

		normalpath_logical_scrub(a, '/');

		fputs(a, stdout);
		if (param.print0)
			putc(0, stdout);
		else
			putc('\n', stdout);
	}

	if (param.stop)
		raise(SIGSTOP);

	return ret ? 0 : 2;
}
