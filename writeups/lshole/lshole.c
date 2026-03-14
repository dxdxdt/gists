#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <assert.h>
#else
#define _GNU_SOURCE
#endif
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>

#define PROGNAME_LSHOLE "lshole"
#define PROGNAME_HASHOLE "hashole"
static const char *progname;

#ifdef _WIN32

#ifndef SEEK_DATA
#define SEEK_DATA (3)
#endif
#ifndef SEEK_HOLE
#define SEEK_HOLE (4)
#endif

static inline bool check_offovf (const LONGLONG ofs)
{
	const off_t cast = (off_t)ofs;

	if (cast < ofs) {
		errno = EOVERFLOW;
		return true;
	}

	return false;
}

static off_t __lseek_mingw (int fd, off_t offset, int whence)
{
	DWORD nor = 0;
	FILE_ALLOCATED_RANGE_BUFFER qr, or;
	HANDLE h = (HANDLE)_get_osfhandle(fd);

	if (h == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return -1;
	}

	qr.FileOffset.QuadPart = offset;
	qr.Length.QuadPart = LLONG_MAX - offset;
	or.FileOffset.QuadPart = or.Length.QuadPart = -1;
	DeviceIoControl(h,
			FSCTL_QUERY_ALLOCATED_RANGES,
			&qr,
			sizeof(qr),
			&or,
			sizeof(or),
			&nor,
			NULL);
	/*
	 * The return value is not used because data is returned even if the
	 * function returns FALSE in case of ERROR_MORE_DATA. This is not a
	 * documented feature but an observed one.
	 *
	 * You gotta love MS folks!
	 */
	if (false) {
		fprintf(stderr,
			"qr=%lld,%lld nor=%lu ",
			qr.FileOffset.QuadPart, qr.Length.QuadPart, nor);
		if (nor == sizeof(or)) {
			fprintf(stderr, "or=%lld,%lld", or.FileOffset.QuadPart, or.Length.QuadPart);
		}
		fprintf(stderr, "\n");
	}
	nor /= sizeof(or);

	switch (whence) {
	case SEEK_DATA:
		if (nor == 0) {
			errno = ENXIO;
			return -1;
		}

		assert(or.FileOffset.QuadPart >= 0 && or.Length.QuadPart >= 0);
		if (check_offovf(or.FileOffset.QuadPart)) {
			return -1;
		}

		return lseek(fd, (off_t)or.FileOffset.QuadPart, SEEK_SET);
	case SEEK_HOLE:
		if (nor == 0) {
			return lseek(fd, 0, SEEK_END);
		}

		assert(or.FileOffset.QuadPart >= 0 && or.Length.QuadPart >= 0);
		if (check_offovf(or.FileOffset.QuadPart)) {
			return -1;
		}

		if (or.FileOffset.QuadPart + or.Length.QuadPart <= offset ||
			offset < or.FileOffset.QuadPart)
		{
			/* offset is in a hole */
			return lseek(fd, offset, SEEK_SET);
		}
		return lseek(fd, (off_t)(or.FileOffset.QuadPart + or.Length.QuadPart), SEEK_SET);
	}

	abort();
}

static off_t lseek_mingw (int fd, off_t offset, int whence)
{
	switch (whence) {
	case SEEK_DATA:
	case SEEK_HOLE:
		return __lseek_mingw(fd, offset, whence);
	}
	return lseek(fd, offset, whence);
}

static const char *geterrmsg (void) {
	static char buf[256];
	snprintf(buf, sizeof(buf), "errno=%d, LastError=%lu", errno, GetLastError());
	return buf;
}

#define LSEEK lseek_mingw

#else /* _WIN32 */

#define _GNU_SOURCE
#define LSEEK lseek

static const char *geterrmsg (void) {
	return strerror(errno);
}

#endif /* _WIN32 */

static bool print_holes (const char *path, const int fd)
{
	off_t a, b = 0, end;
	bool out = false;

	do {
		a = LSEEK(fd, b, SEEK_HOLE);
		if (a < 0) {
			if (b == 0 && errno == ENXIO) {
				break;
			}
			return false;
		}

		b = LSEEK(fd, a, SEEK_DATA);
		if (b < 0 && errno != ENXIO) {
			return false;
		}

		end = b < 0 ? LSEEK(fd, 0, SEEK_END) : b;

		if (end < 0) {
			return false;
		}

		if (a != end) {
			out = true;
			printf("%s: %"PRIuMAX"-%"PRIuMAX"\n",
				path, (uintmax_t)a, (uintmax_t)end);
		}
	} while (b >= 0);

	if (!out) {
		printf("%s:\n", path);
	}

	return true;
}

static int has_holes (const int fd)
{
	off_t h, e;

	h = LSEEK(fd, 0, SEEK_HOLE);
	if (h < 0) {
		if (errno == ENXIO) {
			return 0;
		}
		return -1;
	}

	e = LSEEK(fd, 0, SEEK_END);
	if (e < 0) {
		return -1;
	}

	/*
	 * The "virtual hole" at the EOF or the file is truncated between the
	 * lseek calls(TOCTOU), but that's okay.
	 */
	if (e <= h) {
		return 0;
	}

	return 1;
}

static bool is_hasholemode (const char *s)
{
	static const char PATHSEP =
#if defined(_WIN32)
		'\\';
#else
		'/';
#endif
	const char *sep, *dot;

	sep = strrchr(s, PATHSEP);
	if (sep != NULL) {
		s = sep + 1;
	}

	dot = strrchr(s, '.');
	if (dot != NULL) {
		return strncmp(s, PROGNAME_HASHOLE, dot - s) == 0;
	}
	return strcmp(s, PROGNAME_HASHOLE) == 0;
}

int main (int argc, char *argv [])
{
	static int fails, oks;
	static bool hasholemode;

	assert(argc > 0 && argv[0] != NULL);
	hasholemode = is_hasholemode(argv[0]);

	progname = hasholemode ? PROGNAME_HASHOLE : PROGNAME_LSHOLE;

	for (int i = 1; i < argc; i += 1) {
		const char *path = argv[i];
		const int fd = open(path, O_RDONLY);

		if (fd < 0) {
			goto err;
		}

		errno = 0;
		if (hasholemode) {
			const int ret = has_holes(fd);

			if (ret > 0) {
				return 0;
			}
			if (ret < 0) {
				goto err;
			}
			fails += 1;
			close(fd);
			continue;
		}
		else {
			if (print_holes(path, fd)) {
				oks += 1;
				close(fd);
				continue;
			}
		}

err:
		fprintf(stderr, "%s: %s: %s\n", progname, path, geterrmsg());
		if (fd >= 0) {
			close(fd);
		}
		fails += 1;
		if (hasholemode) {
			return 3;
		}
	}

	if (fails > 0) {
		return 1;
	}
	if (oks == 0) {
		fprintf(stderr, "Usage: %s FILE...\n", progname);
		return 2;
	}
	return 0;
}
