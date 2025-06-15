#define _GNU_SOURCE
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#define ARGV0 "fmemmem"

static bool load_stdin (void **out_m, size_t *out_l) {
	const long psz = sysconf(_SC_PAGESIZE);
	assert(psz > 0);
	size_t m_size = psz;
	void *m = realloc(NULL, m_size);
	size_t l = 0;
	ssize_t rsz;

	if (m == NULL) {
		goto ERR;
	}

	while (true) {
		rsz = read(STDIN_FILENO, (char*)m + l, m_size - l);
		if (rsz < 0) {
			goto ERR;
		}
		if (rsz == 0) {
			break;
		}

		l += rsz;
		assert(l <= m_size);
		if (m_size == l) {
			m_size += psz;

			void *nm = realloc(m, m_size);
			if (nm == NULL) {
				goto ERR;
			}
			m = nm;
		}
	}

	*out_m = m;
	*out_l = l;
	return true;
ERR:
	free(m);
	return false;
}

static size_t memmem_all (
		const void *haystack_in,
		const size_t haystacklen_in,
		const void *needle,
		const size_t needlelen,
		bool(*cb_f)(size_t ofs, void *ctx),
		void *ctx)
{
	size_t ret = 0;
	const void *haystack = haystack_in;
	size_t haystacklen = haystacklen_in;

	if (haystacklen_in == 0 || needlelen == 0) {
		return 0;
	}

	while (haystacklen >= needlelen) {
		void *found = memmem(haystack, haystacklen, needle, needlelen);
		if (found == NULL) {
			break;
		}
		ret += 1;

		const size_t ofs = (char*)found - (char*)haystack_in;
		if (cb_f != NULL && !cb_f(ofs, ctx)) {
			break;
		}

		haystack = (char*)found + needlelen;
		haystacklen = haystacklen_in - ofs - needlelen;
	}

	return ret;
}

static bool report_found_ofs (size_t ofs, void *unused) {
	printf("%zu\n", ofs);
	return true;
}

int main (const int argc, const char **argv) {
	const char *fpath = argv[1]; // should be NULL if argc == 1
	const int fd = open(fpath, O_RDONLY);
	void *m = MAP_FAILED;
	void *needle = NULL;
	size_t needle_len = 0;
	int ret = 1;

	if (argc <= 1) {
		fprintf(stderr, ARGV0": too few arguments. Usage: "ARGV0" <file>\n");
		return 2;
	}
	if (fd < 0) {
		goto FILE_ERR;
	}

	const off_t flen = lseek(fd, 0, SEEK_END);
	if (flen < 0) {
		goto FILE_ERR;
	}
	if (flen == 0) {
		// mmap() does not like zero flen
		ret = 3;
		goto END;
	}

	m = mmap(NULL, (size_t)flen, PROT_READ, MAP_PRIVATE, fd, 0);
	if (m == MAP_FAILED) {
		goto FILE_ERR;
	}

	if (!load_stdin(&needle, &needle_len)) {
		goto END;
	}

	if (memmem_all(m, (size_t)flen, needle, needle_len, report_found_ofs, NULL) > 0) {
		ret = 0;
	}
	else {
		ret = 3;
	}
	goto END;

FILE_ERR:
	fprintf(stderr, ARGV0": ");
	perror(fpath);
END:
	free(needle);
	if (m != MAP_FAILED) {
		munmap(m, (size_t)flen);
	}
	close(fd);
	return ret;
}
