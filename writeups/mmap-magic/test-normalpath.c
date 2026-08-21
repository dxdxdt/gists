#include "mmagic.h"
#include "normalpath.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <locale.h>
#include <ctype.h>

#include <unistd.h>
#include <fcntl.h>

#define PATHSEP '/'

static bool do_test_inner(size_t actual, const char *input,
		const char *expected, void *near, void *far)
{
	size_t a, b, c;

	a = strlen(input);

	/* induce overrun on malplaced null-terminator */
	memset(near, 0xCD, actual);
	strcpy(near, input);
	normalpath_logical(near, PATHSEP);
	if (expected != NULL)
		assert(strcmp(near, expected) == 0);
	b = strlen(near);

	memset(near, 0xCD, actual);
	strcpy(far, input);
	normalpath_logical_scrub(far, PATHSEP);
	if (expected != NULL)
		assert(strcmp(far, expected) == 0);
	c = strlen(far);

	assert(b == c);
	assert(a >= b);

	return a != b;
}

static bool do_test0(const char *input, const char *expected)
{
	struct mmem_guarded *mg;
	const size_t size = strlen(input) + 1;
	void *near, *far;
	size_t actual;
	bool ret;

	mg = mmem_alloc_guarded(size, &actual, &near, &far, NULL);
	if (mg == NULL)
		perror("mmem_alloc_guarded()");
	assert(mg != NULL);

	ret = do_test_inner(actual, input, expected, near, far);

	mmem_free_guarded(mg);

	return ret;
}

static void do_test1(void)
{
	static const size_t BUFSIZE = 65536;
	struct mmem_guarded *mg;
	void *near, *far;
	int fd;
	char *buf;
	ssize_t rsize;
	size_t actual;
	unsigned long shrunk = 0;
	unsigned short trunc;

	buf = malloc(BUFSIZE);

	mg = mmem_alloc_guarded(BUFSIZE, &actual, &near, &far, NULL);
	if (mg == NULL)
		perror("mmem_alloc_guarded()");
	assert(mg != NULL);

	fd = open("/dev/urandom", O_RDONLY);
	assert(fd >= 0);
	assert(buf != NULL);

	for (unsigned long i = 0; i < 2048; i++) {
		rsize = read(fd, buf, BUFSIZE);
		if (rsize < 0)
			perror(NULL);
		assert(rsize == BUFSIZE);

		trunc = (unsigned short)buf[0];
		trunc |= (unsigned short)buf[1] << 8;
		trunc %= BUFSIZE;

		for (size_t j = 0; j < trunc; j++) {
			if ((unsigned int)buf[j] & 0x80 || buf[j] == 0)
				goto scrub;
			if (isblank(buf[j]))
				continue;
			if (isspace(buf[j]) || !isprint(buf[j]))
				goto scrub;
			continue;
scrub:
			buf[j] = '_';
		}
		buf[0] = '/';
		buf[trunc] = 0;

		shrunk += do_test_inner(actual, buf, NULL, near, far);
	}

	free(buf);
	close(fd);
	mmem_free_guarded(mg);

	printf("%lu\n", shrunk);
}

int main(void)
{
	setlocale(LC_ALL, "");

	do_test0("/.",					"/");
	do_test0("/..",					"/");
	do_test0("/./.",				"/");
	do_test0("/././.",				"/");
	do_test0("/./",					"/");
	do_test0("/././",				"/");
	do_test0("/./././",				"/");
	do_test0("/",					"/");
	do_test0("//",					"/");
	do_test0("///",					"/");
	do_test0("////",				"/");

	do_test0("/a/bb/cccccc/dddddd/../../..",	"/a");
	do_test0("/a/bb/cccccc/dddddd/../../../..",	"/");
	do_test0("/a/bb/cccccc/dddddd/../../../../..",	"/");
	do_test0("/a/bb/cccccc/dddddd/../../../",	"/a");
	do_test0("/a/bb/cccccc/dddddd/../../../../",	"/");
	do_test0("/a/bb/cccccc/dddddd/../../../../../",	"/");

	do_test0("/./a/bb/cccccc/dddddd/../../..",		"/a");
	do_test0("/a/./bb/cccccc/dddddd/../../../..",		"/");
	do_test0("/a/bb/./cccccc/dddddd/../../../../..",	"/");
	do_test0("/a/bb/cccccc/./dddddd/../../../",		"/a");
	do_test0("/a/bb/cccccc/dddddd/./../../../../",		"/");
	do_test0("/a/bb/cccccc/dddddd/../../.././../../",	"/");

	do_test0("/aaa/bb/c",				"/aaa/bb/c");
	do_test0("/./aaa/bb/c",				"/aaa/bb/c");
	do_test0("/aaa/./bb/c",				"/aaa/bb/c");
	do_test0("/aaa/bb/./c",				"/aaa/bb/c");
	do_test0("/aaa/bb/c/.",				"/aaa/bb/c");
	do_test0("/aaa/bb/c/./",			"/aaa/bb/c");

	do_test0("/aaaa/../bbb/cc/d",			"/bbb/cc/d");
	do_test0("/aaaa/../bbb/./cc/d",			"/bbb/cc/d");
	do_test0("/aaaa/../bbb/cc/./d",			"/bbb/cc/d");
	do_test0("/aaaa/../bbb/cc/d/",			"/bbb/cc/d");
	do_test0("/aaaa/../bbb/cc/d/.",			"/bbb/cc/d");
	do_test0("/aaaa/../bbb/cc/d/./",		"/bbb/cc/d");
	do_test0("/aaaa/../bbb/cc/d/././",		"/bbb/cc/d");

	do_test0("/aaaa/bbb/cc/d/..",			"/aaaa/bbb/cc");
	do_test0("/aaaa/bbb/cc/d/../",			"/aaaa/bbb/cc");
	do_test0("/aaaa/bbb/cc/d/../.",			"/aaaa/bbb/cc");
	do_test0("/aaaa/bbb/cc/d/.././",		"/aaaa/bbb/cc");

	do_test0("/../aaaa/bbb/cc/d",			"/aaaa/bbb/cc/d");
	do_test0("/aaaa/../bbb/cc/d",			"/bbb/cc/d");
	do_test0("/aaaa/bbb/cc/../d",			"/aaaa/bbb/d");

	do_test0("/aaaa/../cc/../0000",			"/0000");
	do_test0("/aaaa/../../d/0000",			"/d/0000");
	do_test0("/aaaa/bbb/cc/../..",			"/aaaa");

	do_test1();
}
