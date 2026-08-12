#include "mmagic.h"
#if defined(__sun) && !defined(_XOPEN_SOURCE) /* Solaris */
/* getopt() */
#define _XOPEN_SOURCE 500
#endif
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>

#define ARGV0 "sparse-array"

enum cmdline_opcode {
	CO_NONE,
	CO_READ,
	CO_WRITE,
	CO_COUNT,
	CO_DUMP_ALL,
	CO_GC,
	CO_CLEAR,
	CO_TIMESTAMP,
	CO_HELP,
	CO_QUIT,
	NB_CO
};

struct op {
	union {
		size_t at;
		size_t cnt;
	};
	long val;
	enum cmdline_opcode opcode;
};

static bool parse_cmdline(const char *line, struct op *o, ssize_t *err_at)
{
	const char *start, *ptr;
	const int saved_errno = errno;

	errno = 0;

	switch (line[0]) {
	case 'a':
	case 'g':
	case 'x':
	case 'c':
	case 't':
	case 'h':
	case 'q':
		start = line + 0;
		ptr = start + 1;
		/* Skip to the next non-space or null terminator */
		for (; *ptr != 0 && isspace(*ptr); ptr++);
		/* Make sure it's not rubbish */
		if (*ptr != 0)
			goto err;

		switch (line[0]) {
		case 'a':
			o->opcode = CO_DUMP_ALL;
			break;
		case 'g':
			o->opcode = CO_GC;
			break;
		case 'x':
			o->opcode = CO_CLEAR;
			break;
		case 'c':
			o->opcode = CO_COUNT;
			break;
		case 't':
			o->opcode = CO_TIMESTAMP;
			break;
		case 'h':
			o->opcode = CO_HELP;
			break;
		case 'q':
			o->opcode = CO_QUIT;
			break;
		default:
			goto err;
		}
		goto out;
	}

	/* Read the first number as unsigned integer */
	start = line;
	o->at = strtoul(start, (char**)&ptr, 0);
	if (errno || start == ptr)
		goto err;

	/* Skip to the next non-space or null terminator */
	for (; *ptr != 0 && isspace(*ptr); ptr++);
	switch (*ptr) {
	case 0: /* Just a number. This is a read op. */
		o->opcode = CO_READ;
		goto out;
	case '=': /* Has to be a '=' sign */
		break;
	default:
		goto err;
	}

	start = ptr + 1;
	o->val = strtol(start, (char**)&ptr, 0);
	if (errno || start == ptr)
		goto err;

	/* Skip to the next non-space or null terminator */
	for (; *ptr != 0 && isspace(*ptr); ptr++);
	/* Make sure it's not rubbish */
	if (*ptr != 0)
		goto err;

	o->opcode = CO_WRITE;
out:
	errno = saved_errno;
	return true;
err:
	if (errno == 0)
		errno = EBADMSG;
	*err_at = (ssize_t)(ptr - line);
	return false;
}

static inline size_t psz_round_up(size_t a, size_t b, bool *out_ovf)
{
	bool ovf;
	const long psz = sysconf(_SC_PAGESIZE);
	const size_t mask = (size_t)psz - 1;

	assert(psz > 0);

	ovf = __builtin_mul_overflow(a, b, &a);
	if (a & mask) {
		a &= ~mask;
		ovf |= __builtin_add_overflow(a, (size_t)psz, &a);
	}

	if (out_ovf)
		*out_ovf = ovf;
	return a;
}

static int fd = -1;

static void *mmap_array(size_t *size)
{
	void *ret = MAP_FAILED;
	int prot, flags, err;
	bool ovf = false;

	*size = psz_round_up(*size, sizeof(long), &ovf);
	if (ovf) {
		errno = ENOMEM;
		return NULL;
	}

	assert(fd < 0);

	fd = mkmemfile(NULL);
	if (fd < 0)
		return NULL;
	err = ftruncate(fd, *size);
	if (err)
		goto err;

	prot = PROT_READ | PROT_WRITE;
	flags = MAP_SHARED;
#if defined(MAP_NORESERVE) && 0
	/* You need this for MAP_PRIVATE */
	flags |= MAP_NORESERVE;
#endif
	ret = mmap(NULL, *size, prot, flags, fd, 0);
	if (ret == MAP_FAILED)
		goto err;

	return ret;
err:
	if (fd >= 0) {
		close(fd);
		fd = -1;
	}
	return NULL;
}

static void unmap_array(void *addr, size_t size)
{
	if (addr == NULL)
		return;

	munmap(addr, size);
	close(fd);
	fd = -1;
}

static bool lseek_array(void *addr, size_t size, void *uctx,
		bool (*callback)(void *start, size_t len, void *uctx))
{
	struct {
		off_t start;
		off_t end;
		off_t len;
	} d = {0};
	int saved_errno;
	bool flag = true;
	const long psz = sysconf(_SC_PAGESIZE);
	char *ptr;

	assert(psz > 0);

	do {
		saved_errno = errno;
		d.start = lseek(fd, d.end, SEEK_DATA);
		if (d.start < 0) {
			if (errno == ENXIO) {
				/* no more data */
				errno = saved_errno;
				return true;
			}
			return false;
		}

		d.end = lseek(fd, d.start, SEEK_HOLE);
		if (d.end < 0)
			return false;

		d.len = d.end - d.start;
		if (d.len <= 0)
			/*
			 * This check is thanks to the bug in Linux kernel. See
			 *
			 * Link: https://github.com/util-linux/util-linux/pull/4132
			 */
			break;

		/* Align to psz */
		d.start &= ~((off_t)psz - 1);
		d.end = d.start + psz;
		d.len = psz;
		ptr = (char*)addr + d.start;

		if (callback != NULL)
			flag = callback(ptr, (size_t)d.len, uctx);
	} while (flag);

	return true;
}

struct gc_ctx {
	void *base;
	void *zm;
	size_t zl;
};

static bool dealloc_callback(void *start, size_t len, void *uctx)
{
	struct gc_ctx *ctx = uctx;
	const off_t ofs = (char*)start - (char*)ctx->base;

	assert(len <= ctx->zl);

	if (memcmp(ctx->zm, start, len) == 0)
		dealloc_file_range(fd, ofs, len);

	return true;
}

static bool gc_array(void *addr, size_t size)
{
	const long psz = sysconf(_SC_PAGESIZE);
	struct gc_ctx ctx = {
		.base = addr,
		.zl = (size_t)psz,
	};
	bool ret;

	assert(psz > 0);
	ctx.zm = calloc(1, ctx.zl);
	if (ctx.zm == NULL)
		return false;

	ret = lseek_array(addr, size, &ctx, dealloc_callback);

	free(ctx.zm);
	return ret;
}

static void dealloc_array(void *addr, size_t size)
{
	if (false)
		/*
		 * For platforms without a file range deallocation support, we
		 * don't do this. The prime example of such implementation is
		 * OpenBSD.
		 */
		dealloc_file_range(fd, 0, size);
	else {
		/*
		 * Instead, just nuke the whole file and then truncate it back
		 * up to the original size. Most UNIX implementations allow
		 * truncation of a file with existing mappings.
		 *
		 * One major implementation that won't allow this is XNU kernel,
		 * as of writing of this. (Apple can go shove it)
		 */
		const bool res =
				ftruncate(fd, 0) == 0 &&
				ftruncate(fd, size) == 0;

		/* Oh, no! This OS doesn't implement memfile as sparse tmpfs. */
		assert(res);
		(void)res;
	}
}

static void *calloc_array(size_t *size)
{
	bool ovf = false;
	*size = psz_round_up(*size, sizeof(long), &ovf);

	if (ovf) {
		errno = ENOMEM;
		return NULL;
	}
	return calloc(1, *size);
}

static void free_array(void *addr, size_t size)
{
	free(addr);
}

static bool gc_noop(void *addr, size_t size)
{
	errno = ENOTSUP;
	return false;
}

static bool lsearch_array(void *addr, size_t size, void *uctx,
		bool (*callback)(void *start, size_t len, void *uctx))
{
	bool flag = true;
	const long psz = sysconf(_SC_PAGESIZE);
	char *ptr;

	assert(psz > 0);

	for (size_t i = 0; flag && i < size; i += (size_t)psz) {
		ptr = addr + i;
		if (callback != NULL)
			flag = callback(ptr, (size_t)psz, uctx);
	}

	return true;
}

static void zero_array(void *addr, size_t size)
{
	memset(addr, 0, size);
}

static struct {
	void *(*alloc_array)(size_t *size);
	void (*free_array)(void *addr, size_t size);
	bool (*dump_array)(void *addr, size_t size, void *uctx,
			bool (*callback)(void *start, size_t len, void *uctx));
	bool (*gc_array)(void *addr, size_t size);
	void (*clear_array)(void *addr, size_t size);
	size_t size;
	struct {
		bool help:1;
	} f;
} parm = {
	.alloc_array = mmap_array,
	.free_array = unmap_array,
	.dump_array = lseek_array,
	.gc_array = gc_array,
	.clear_array = dealloc_array,
	.size = 1,
};

static bool parse_opts(int argc, char *argv[])
{
	const char *str;
	char *trail;
	bool noarg = false;

	for(;;) {
		const int c = getopt(argc, (char *const *)argv, "cmh");

		switch (c) {
		case 'c':
			parm.alloc_array = calloc_array;
			parm.free_array = free_array;
			parm.dump_array = lsearch_array;
			parm.gc_array = gc_noop;
			parm.clear_array = zero_array;
			break;
		case 'm':
			parm.alloc_array = mmap_array;
			parm.free_array = unmap_array;
			parm.dump_array = lseek_array;
			parm.gc_array = gc_array;
			parm.clear_array = dealloc_array;
			break;
		case 'h':
			noarg = parm.f.help = true;
			break;
		default:
			if (c < 0)
				goto done;
			return false;
		}
	}
done:
	if (noarg)
		return true;

	if (optind >= argc) {
		fprintf(stderr, ARGV0 ": too few arguments\n");
		return false;
	}
	if (optind + 1 < argc) {
		fprintf(stderr, ARGV0 ": too many arguments\n");
		return false;
	}

	errno = 0;
	str = argv[optind];
	parm.size = strtoul(str, &trail, 0);
	if (*trail != 0 || parm.size == 0)
		errno = EINVAL;
	if (errno) {
		fprintf(stderr, ARGV0 ": size %s: %s\n", str, strerror(errno));
		return false;
	}

	return true;
}

static void cmd_help(FILE *s)
{
	fprintf(s,
			"Commands:\n"
			"  <INDEX> = <VALUE>: assign the value at the index\n"
			"  <INDEX>: print the value at the index\n"
			"  x: clear everything\n"
			"  g: do garbage collection(free empty pages)\n"
			"  c: print the count, total pages in use(pages) and non-empty pages(npz)\n"
			"  a: print all values and the counts\n"
			"  t: print timestamp (see CLOCK_MONOTONIC from clock_gettime(2))\n"
			"  h: print this message\n"
			"  q: quit\n"
		);
}

static void usage(FILE *s)
{
	fprintf(s, "Usage: " ARGV0 " [-cm] <SIZE>\n");
	fprintf(s, "Usage: " ARGV0 " [-h]\n");
}

static bool skip_line(const char *line)
{
	/* Skip to the next non-space or null terminator */
	for (; *line != 0 && isspace(*line); line++);

	switch (*line) {
	case 0:
	case '#':
		return true;
	}
	return false;
}

struct dump_ctx {
	void *base;
	void *end;
	struct {
		size_t in;
		size_t out;
		size_t pages;
		size_t npz;
	} cnt;
	FILE *f;
};

static bool dump_callback(void *start, size_t len, void *uctx)
{
	struct dump_ctx *ctx = uctx;
	const long *base = ctx->base;
	size_t idx;
	bool nzp = false;

	assert((size_t)start % sizeof(long) == 0);

	for (const long *p = start, *end = p + (len / sizeof(long)); p < end; p++) {
		if (ctx->cnt.in == 0 || (ctx->end != NULL && (long*)ctx->end <= p))
			return false;
		if (*p == 0)
			continue;

		idx = p - base;
		if (ctx->f != NULL)
			fprintf(ctx->f, "%zu = %ld\n", idx, *p);

		ctx->cnt.in--;
		ctx->cnt.out++;
		nzp = true;
	}

	if (nzp)
		ctx->cnt.npz++;
	ctx->cnt.pages++;

	return true;
}

static bool istermout(void)
{
	return isatty(STDOUT_FILENO) && isatty(STDERR_FILENO);
}

int main(int argc, char *argv[])
{
	int saved_errno;
	long *arr;
	size_t len, size, linelen, lineidx = 0;
	ssize_t err_at = -1;
	char linebuf[256];
	struct op cmd = {0};
	struct {
		bool err:1;
		bool ok:1;
	} dirty = {0};
	struct dump_ctx dctx = {0};
	struct timespec ts = {0};

	if (!parse_opts(argc, argv)) {
		usage(stderr);
		return 2;
	}
	if (parm.f.help) {
		usage(stdout);
		cmd_help(stdout);
		return 0;
	}

	len = size = parm.size;
	arr = parm.alloc_array(&size);
	if (arr == NULL) {
		perror(ARGV0);
		return 1;
	}

	if (istermout())
		fprintf(stderr, ARGV0 ": %zu bytes of VSZ ready\n", size);

	for (;; lineidx++) {
		err_at = -1;

		if (lineidx == SSIZE_MAX) {
			errno = EOVERFLOW;
			goto it_err;
		}

		if (istermout()) {
			fprintf(stderr, "> ");
			fflush(stderr);
		}
		if (fgets(linebuf, sizeof(linebuf), stdin) == NULL)
			goto loop_out;
		linelen = strlen(linebuf);
		if (linelen + 1 >= sizeof(linebuf)) {
			errno = EMSGSIZE;
			goto it_err;
		}
		if (skip_line(linebuf))
			continue;

		if (!parse_cmdline(linebuf, &cmd, &err_at))
			goto it_err;
		if (cmd.at >= len) {
			errno = EFAULT;
			goto it_err;
		}

		switch (cmd.opcode) {
		case CO_WRITE:
			arr[cmd.at] = cmd.val;
			/* fall-through */
		case CO_READ:
			printf("%zu = %ld\n", cmd.at, arr[cmd.at]);
			break;
		case CO_COUNT:
			dctx.f = NULL;
			goto do_dump;
		case CO_DUMP_ALL:
			dctx.f = stdout;
do_dump:
			dctx.base = arr;
			dctx.end = NULL;
			dctx.cnt.in = len;
			dctx.cnt.out = 0;
			dctx.cnt.pages = 0;
			dctx.cnt.npz = 0;
			if (!parm.dump_array(arr, size, &dctx, dump_callback))
				goto it_err;
			printf("count = %zu, npz = %zu, pages = %zu\n",
					dctx.cnt.out, dctx.cnt.npz, dctx.cnt.pages);
			break;
		case CO_GC:
			if (!parm.gc_array(arr, size))
				goto it_err;
			break;
		case CO_CLEAR:
			parm.clear_array(arr, size);
			break;
		case CO_TIMESTAMP:
			clock_gettime(CLOCK_MONOTONIC, &ts);
			printf("%lld.%06ld\n", (long long)ts.tv_sec, ts.tv_nsec / 1000);
			break;
		case CO_HELP:
			cmd_help(stdout);
			break;
		case CO_QUIT:
			goto loop_out;
		default:
			errno = ENOTSUP;
			goto it_err;
		}
		dirty.ok = true;
		continue;
it_err:
		dirty.err = true;
		saved_errno = errno;
		if (err_at >= 0 && istermout()) {
			for (size_t i = 0; i < (size_t)err_at; i++)
				fprintf(stderr, " ");
			fprintf(stderr, "  ^\n");
		}
		fprintf(stderr, ARGV0 ": line %zu: %s\n", lineidx, strerror(saved_errno));

		if (errno == EMSGSIZE)
			goto loop_out;
	}
loop_out:
	parm.free_array(arr, size);

	if (dirty.err)
		return dirty.ok ? 3 : 1;
	return 0;
}
