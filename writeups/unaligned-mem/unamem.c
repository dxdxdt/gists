#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <error.h>
#include <assert.h>

#include <unistd.h>
#include <wait.h>

#define ARGV0 "unamem"
#define DONT_FORK 1


static void report_wait (const int status) {
	if (WIFEXITED(status)) {
		printf("exit %d\n", WEXITSTATUS(status));
	}
	else if (WIFSIGNALED(status)) {
		printf("signal %d\n", WTERMSIG(status));
	}
	else {
		printf("?\n");
	}
}

int main (void) {
	static char m[9]; // bss address is nicer than stack
	char *c;
	uintptr_t *p;
	pid_t pid;
	int status;

	printf(
		"CHAR_BIT:  %d\n"
		"word:      %zd\n"
		"uintptr_t: %zd\n"
		"size_t:    %zd\n"
		"short:     %zd\n"
		"int:       %zd\n"
		"long:      %zd\n"
		"long long: %zd\n"
		,
		CHAR_BIT,
		CHAR_BIT * sizeof(uintptr_t),
		sizeof(uintptr_t),
		sizeof(size_t),
		sizeof(short),
		sizeof(int),
		sizeof(long),
		sizeof(long long));

	c = m;
	p = (void*)(m + 1);

	printf(
		"malloc:    %zx", (uintptr_t)m);
	for (size_t i = sizeof(uintptr_t); i > 1; i /= 2) {
		if ((uintptr_t)m % i == 0) {
			printf(" {%zu}", i);
		}
	}
	printf("\n");

	printf(
		"c:         %zx\n"
		"p:         %zx\n"
		,
		(uintptr_t)c,
		(uintptr_t)p);

	pid = DONT_FORK ? 0 : fork();
	if (pid > 0) {
		printf(
		"access:    "
		);
		wait(&status);
		report_wait(status);
	}
	else if (pid == 0) {
		*p = UINTPTR_MAX;
		*c = 0;
		assert(*p == UINTPTR_MAX);
		assert(*c == 0);

		*c = 0;
		*p = UINTPTR_MAX;
		assert(*p == UINTPTR_MAX);
		assert(*c == 0);
	}
	else {
		perror(ARGV0": fork()");
		return 1;
	}

	pid = DONT_FORK ? 0 : fork();
	if (pid > 0) {
		printf(
		"float:     "
		);
		wait(&status);
		report_wait(status);
	}
	else if (pid == 0) {
		volatile float a, b, c;

		a = 0.1;
		b = 1.0;
		c = b / a;
		assert(c > 1.0);
	}
	else {
		perror(ARGV0": fork()");
		return 1;
	}

	return 0;
}
