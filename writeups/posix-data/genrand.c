#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>

#include <getopt.h>

#define ARGV0 "genrand"

int main(int argc, char *argv[])
{
	int c;
	unsigned long n = 0;
	int r0, r1, r2;

	for (;;) {
		c = getopt(argc, (char *const *)argv, "n:");

		switch (c) {
		case 'n':
			if (sscanf(optarg, "%lu", &n) != 1) {
				errno = EINVAL;
				perror(ARGV0 ": -n");
				exit(2);
			}
			break;
		default:
			if (c < 0)
				goto getopt_done;
			else
				exit(2);
		}
	}
getopt_done:

	for (unsigned long i = 0; i < n; i++) {
		r0 = rand();
		r1 = rand();
		r2 = r0 * r1;

		printf("%d\n", r2);
	}

	return 0;
}
