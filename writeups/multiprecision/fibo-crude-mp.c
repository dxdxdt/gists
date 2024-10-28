#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#define N 101
#define MAX_CHAR 256

void do_ieee754 (void) {
	double a = 0.0;
	double b = 1.0;
	double c;

	for (int i = 0; i < N; i += 1) {
		c = a + b;
		printf("%.0lf\n", c);
		b = a;
		a = c;
	}
}

void add (char *out, const char *a, const char *b) {
	int i;
	int n;
	int carry = 0;
	bool flag;

	for (i = 0; i < MAX_CHAR - 1; i += 1) {
		n = carry;
		flag = false;
		if (a[i] != 0) {
			n += a[i] - '0';
			flag = true;
		}
		if (b[i] != 0) {
			n += b[i] - '0';
			flag = true;
		}
		carry = n / 10;
		if (!flag && carry == 0 && n == 0) {
			out[i] = 0;
			return;
		}

		out[i] = (n % 10) + '0';
	}

	assert(carry == 0); // detect "overflow"
}

void print_reversed (const char *s) {
	const size_t l = strlen(s);
	const char *p = s + l;

	for (size_t i = 0; i < l; i += 1) {
		p -= 1;
		putchar(*p);
	}
	putchar('\n');
}

void do_crude_mp (void) {
	static char a[MAX_CHAR];
	static char b[MAX_CHAR];
	static char c[MAX_CHAR];

	strcpy(a, "0");
	strcpy(b, "1");
	memset(c, 0, MAX_CHAR);

	for (int i = 0; i < N; i += 1) {
		add(c, a, b); // c = a + b;
		print_reversed(c);
		memcpy(b, a, MAX_CHAR); // b = a;
		memcpy(a, c, MAX_CHAR); // a = c;
	}
}

bool parse_flag (const char *s) {
	if (strcmp(s, "true") == 0) {
		return true;
	}
	else if (strcmp(s, "false") == 0) {
		return false;
	}
	else {
		double tmp = 0.0;

		sscanf(s, "%lf", &tmp);
		return tmp != 0.0;
	}
}

int main (const int argc, const char **argv) {
	if (argc == 1 || (argc > 1 && parse_flag(argv[1]))) {
		do_crude_mp();
	}
	else {
		do_ieee754();
	}

	return 0;
}
