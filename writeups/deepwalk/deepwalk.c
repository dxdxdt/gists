#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>
#include <ftw.h>
#ifdef HAS_FTS
#include <fts.h>
#endif

#define ARGV0 "deepwalk"
#define DEEPWALK_FTW_NOPENFD (20)


static struct {
	const char **args;
	int(*walkf)(void);
	long maxdepth;
} params = {
	.maxdepth = -1,
};

static bool erred;

#ifdef HAS_FTS

static int walkf_fts_ent (FTS *fts) {
	FTSENT *ent = fts_read(fts);

	if (ent == NULL) {
		if (errno != 0) {
			perror(ARGV0": fts_read()");
			return -1;
		}
		return 0;
	}

	switch (ent->fts_info) {
	case FTS_D: // we're diving into the dir
		// NOTE that ent->fts_level is short!!
		if (params.maxdepth >= 0 && params.maxdepth <= ent->fts_level) {
			const int fr = fts_set(fts, ent, FTS_SKIP);
			(void)fr;
			assert(fr == 0);
		}
		break;
	case FTS_DP: // we're coming out of the dir
		return 1;
	case FTS_DC:
		// ftw doesn't have this. +1 for fts.
		fprintf(
				stderr,
				ARGV0": %s: circular directory structure detected(FTS_DC)\n",
				ent->fts_path);
		return 1;
	case FTS_DNR:
	case FTS_ERR:
	case FTS_NS:
		// syscall failed for some reason:
		// - ENOENT argv
		// - (possibly) EIO due to corrupted fs
		fprintf(stderr, ARGV0": %s: %s\n", ent->fts_path, strerror(ent->fts_errno));
		erred = true;
		return 1;
	}

	if (access(ent->fts_accpath, F_OK) < 0) {
		fprintf(stderr, ARGV0": %s: %s\n", ent->fts_path, strerror(errno));
	}
	else {
		printf("%s\n", ent->fts_path);
	}

	return 1;
}

static int walkf_fts (void) {
	FTS *fts;
	int ret = 0;

	// FTS_XDEV == FTW_MOUNT
	// no user context. thread_local should be used if required
	fts = fts_open((char*const*)params.args, FTS_PHYSICAL, NULL);
	if (fts == NULL) {
		perror(ARGV0": fts_open()");
		return 1;
	}

	while (true) {
		const int fr = walkf_fts_ent(fts);
		if (fr < 0) {
			ret = 1;
			break;
		}
		else if (fr == 0) {
			break;
		}
	}

	fts_close(fts);

	return erred ? 3 : ret;
}

#endif

static int walkf_nftw_cb (
		const char *fpath,
		const struct stat *sb,
		int typeflag,
		struct FTW *ftwbuf)
{
#ifdef FTW_ACTIONRETVAL
	#define RET_CONT (FTW_CONTINUE)
	#define RET_STOP (FTW_STOP)
#else
	#define RET_CONT (0)
	#define RET_STOP (-1)
#endif
	const char *basename;

	switch (typeflag) {
#ifdef FTW_ACTIONRETVAL
	case FTW_D: // we're diving into the dir
		if (params.maxdepth >= 0 && params.maxdepth < ftwbuf->level) {
			return FTW_SKIP_SUBTREE;
		}
		break;
#endif
	case FTW_DP: // we're coming out of the dir
		// not reached when not called with FTW_DEPTH
		return RET_CONT;
	case FTW_DNR:
	case FTW_NS:
		// syscall failed for some reason (probably EIO due to corrupted fs)
		goto err;
	}

#ifndef FTW_ACTIONRETVAL
	/*
	 * Without FTW_ACTIONRETVAL, we've got no choice but to keep getting
	 * callbacks and ignore them.
	 */
	if (params.maxdepth >= 0 && params.maxdepth < ftwbuf->level) {
		return RET_CONT;
	}
#endif

	/*
	 * ftwbuf->base is 32 bit on all arch, limiting fpath to 2^32-1 characters.
	 * Naive libc implementations could let it overflow. Here, we're being
	 * defensive by checking it before use.
	 *
	 * This is a design error similar to 32-bit time_t all over again.
	 */
	if (ftwbuf->base < 0) {
		errno = EOVERFLOW;
		return RET_STOP;
	}
	basename = fpath + ftwbuf->base;
	assert(*basename != 0);
	(void)sb;
	if (access(basename, F_OK) < 0) {
		goto err;
	}
	else {
		printf("%s\n", fpath);
	}

	return RET_CONT;
err:
	fprintf(stderr, ARGV0": %s: %s\n", fpath, strerror(errno));
	return RET_CONT;
#undef RET_CONT
#undef RET_STOP
}

static int walkf_nftw (void) {
	const int flags = FTW_PHYS |
#ifdef FTW_ACTIONRETVAL
		FTW_ACTIONRETVAL |
#endif
		/*
		 * fts default
		 *
		 * BUG: HOWEVER, it's hopelessly broken on both musl and glibc!
		 */
		FTW_CHDIR;
	for (; *params.args != NULL; params.args += 1) {
		// no comparator function
		// FTS_XDEV == FTW_MOUNT
		// no user context. thread_local should be used if required
		const int fr = nftw(*params.args, walkf_nftw_cb, DEEPWALK_FTW_NOPENFD, flags);
		if (fr < 0) {
			const int err = errno;

			erred = true;
			fprintf(stderr, ARGV0": %s: %s\n", *params.args, strerror(err));
			if (err != ENOENT) {
				return 1;
			}
		}
	}

	return erred ? 3 : 0;
}

static int walkf_trad (void) {
	return 0; // TODO
}

static int parse_args (const int argc, char *const *argv) {
	const char *fstr;

	if (argv == NULL || argv[1] == NULL) {
		return -1;
	}

	while (true) {
		char *end;
		const int fr = getopt(argc, argv, "hd:");

		if (fr < 0) {
			break;
		}
		switch (fr) {
		case 'h': return -1;
		case 'd':
			errno = 0;
			end = NULL;
			params.maxdepth = strtol(optarg, &end, 0);
			if (end != NULL && *end != 0) {
				errno = EINVAL;
			}
			if (errno != 0) {
				fprintf(stderr, ARGV0": -d %s: %s\n", optarg, strerror(errno));
				return -1;
			}
			break;
		default:
			return -1;
		}
	}

	if (argc <= optind) {
		fprintf(stderr, ARGV0": too few arguments\n");
		return -1;
	}

	fstr = argv[optind];
	if (strcmp(fstr, "nftw") == 0) {
		params.walkf = walkf_nftw;
	}
	else if (strcmp(fstr, "fts") == 0) {
#ifdef HAS_FTS
		params.walkf = walkf_fts;
#else
		fprintf(stderr, ARGV0": %s: no libc support\n", fstr);
		return 0;
#endif
	}
	else if (strcmp(fstr, "trad") == 0) {
		params.walkf = walkf_trad;
	}
	else {
		fprintf(stderr, ARGV0": %s: invalid func\n", fstr);
		return -1;
	}

	optind += 1;
	if (argc > optind) {
		params.args = (const char**)(argv + optind);
	}
	else {
		static const char *DEFAULT_ARGS[] = { ".", NULL, };
		params.args = DEFAULT_ARGS;
	}

	return 1;
}

int main (const int argc, char *const *argv) {
	const int pr = parse_args(argc, argv);
	if (pr < 0) {
		fprintf(
				stderr,
				"Usage: "ARGV0" [-h] [-d maxdepth] <nftw|fts|trad> [path ...]\n");
		return 2;
	}
	else if (pr == 0) {
		return 1;
	}

	return params.walkf();
}
