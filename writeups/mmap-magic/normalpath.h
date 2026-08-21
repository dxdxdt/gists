#ifndef NORMALPATH_H_
#define NORMALPATH_H_
#ifndef DEBUG_NORMALPATH
#define DEBUG_NORMALPATH (0)
#endif
#include <stdio.h>
#include <string.h>
#include <assert.h>

static inline void normalpath_logical(char *path, const char sep)
{
	size_t len = strlen(path);
	char *parent;
#define DEBUG_TRACE()							\
	if (DEBUG_NORMALPATH)						\
		fprintf(stderr, "%s:%d: %s\n", __PRETTY_FUNCTION__, __LINE__, path);
#define DEBUG_TRACE_START()						\
	if (DEBUG_NORMALPATH)						\
		fprintf(stderr, "%s:%d: START\n", __PRETTY_FUNCTION__, __LINE__);
#define DEBUG_TRACE_END()						\
	if (DEBUG_NORMALPATH)						\
		fprintf(stderr, "%s:%d: RETURN\n", __PRETTY_FUNCTION__, __LINE__);

	DEBUG_TRACE_START();

	/*
	 * This looks like a mess in terms of CPU cycles, but you can't really
	 * win without sacrificing memory footprint. Yes, it's O(N^2) in the
	 * worse case scenario where the input string looks like this:
	 *
	 *    "/../../../../../../../../../../../../../../../.." (15 restarts)
	 *
	 * But I'm sure it's a good trade-off because memory allocation or
	 * separator(/) scanning is way more expensive.
	 */
restart:
	path[len] = 0;
	DEBUG_TRACE();
	if (len < 2) {
		DEBUG_TRACE_END();
		return;
	}
	parent = path;

	for (size_t i = 1; i < len;) {
		if (path[i - 1] == sep && path[i] == sep) {
			/* double sep(//) */
			memmove(&path[i - 1], &path[i], len - i);
			len -= 1;
			goto newsize;
		} else if (path[i - 1] == sep && path[i] == '.') {
			if (path[i + 1] == 0 || path[i + 1] == sep) {
				/* single dot */
				memmove(&path[i], &path[i + 1], len - i);
				len -= 1;
				goto restart;
			}
			if (i + 1 < len && path[i + 1] == '.' &&
					(path[i + 2] == sep || path[i + 2] == 0)) {
				/* double dot */
				memmove(parent, &path[i + 2], len - i - 2);
				len -= &path[i + 2] - parent;
				if (len == 0)
					/* Don't cut the root! */
					len = 1;
				goto restart;
			}
			goto uneventful;
		}
uneventful:
		if (path[i - 1] == sep)
			parent = &path[i - 1];
		i++;
		continue;
newsize:
		path[len] = 0;
		DEBUG_TRACE();
	}

	/* Remove trailing sep */
	while (len > 1 && path[len - 1] == sep)
		path[--len] = 0;
	DEBUG_TRACE();
	DEBUG_TRACE_END();
#undef DEBUG_TRACE
#undef DEBUG_TRACE_START
#undef DEBUG_TRACE_END
}

static inline void normalpath_logical_scrub(char *path, const char sep)
{
	size_t a, b;

	a = strlen(path);
	normalpath_logical(path, sep);
	b = strlen(path);

	assert(a >= b);
	if (a > b) {
		if (true)
			memset(path + b, 0, a - b);
		else
			/* XXX: the bug */
			memset(path + a, 0, a - b);
	}
}

#endif
