/**
 * @file pthread-timedwait.c
 * @brief Demonstrates the effects on pthread_timedwait upon system wall clock
 * change
 */
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>

#define ARG0 "pthread_timedwait"


struct {
	// desired sleep time
	struct timespec t;
} opts;

/**
 * @brief Raise nanosecond fraction part
 * @details "05" -> 50000000, "123" -> 123000000, "005" -> 5000000
 * @note used to preserve precision
 *
 * @param str fraction part, excluding the leading decimal point
 * @param len length of \param str
 * @return long the fraction part raised to nanosecond scale
 */
long raise_nsec_frac (const char *str, const size_t len) {
	char m[] = { '0', '0', '0', '0', '0', '0', '0', '0', '0', 0 };
	char *p;
	long ret = -1;

	memcpy(m, str, len > 9 ? 9 : len);

	for (p = m; *p != 0 && *p == '0'; p += 1);

	if (*p == 0 && m[0] == '0') {
		return 0;
	}
	if (sscanf(p, "%ld", &ret) != 1) {
		return -1;
	}

	return ret;
}

/**
 * @brief Parse timespec from string, preserving precision
 *
 * @param str string to parse
 * @param ts_out (optional) pointer to timespec struct
 * @return true if parsed successfully and \param ts_out is set if non-null
 * @return false otherwise. errno set to EINVAL if \param str is NULL. errno set
 * to EBADMSG on format error
 */
bool parse_ts (const char *str, struct timespec *ts_out) {
	long long sec = 0;
	long nsec = 0;
	int fr;
	const char *frac, *ipart;

	if (str == NULL) {
		errno = EINVAL;
		return false;
	}

	if (str[0] == '.' || str[0] == ',') {
		ipart = NULL;
		frac = str + 1;
	}
	else {
		ipart = str;
		frac = strchr(str, '.');
		if (frac == NULL) {
			frac = strchr(str, ',');
		}
	}

	if (ipart != NULL) {
		fr = sscanf(ipart, "%lld", &sec);
		if (fr < 1) {
			errno = EBADMSG;
			return false;
		}
	}
	if (frac != NULL) {
		frac += 1;
		nsec = raise_nsec_frac(frac, strlen(frac));
		if (nsec < 0) {
			errno = EBADMSG;
			return false;
		}
	}

	if (ts_out != NULL) {
		ts_out->tv_sec = sec;
		ts_out->tv_nsec = nsec;
	}

	return true;
}

bool parse_args (const int argc, const char **argv) {
	if (argc <= 1) {
		fprintf(stderr, ARG0" <TIME>\n");
		return false;
	}

	if (!parse_ts(argv[1], &opts.t)) {
		perror(ARG0);
		return false;
	}

	return true;
}

int main (const int argc, const char **argv) {
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
	struct timespec ts[2];
	struct timespec tp_deadline;

	if (!parse_args(argc, argv)) {
		return 1;
	}

	clock_gettime(CLOCK_REALTIME, &tp_deadline);
	tp_deadline.tv_nsec += opts.t.tv_nsec;
	tp_deadline.tv_sec += opts.t.tv_sec + (tp_deadline.tv_nsec / 1000000000);
	tp_deadline.tv_nsec %= 1000000000;

	clock_gettime(CLOCK_MONOTONIC, ts);
	pthread_mutex_lock(&mtx);
	pthread_cond_timedwait(&cond, &mtx, &tp_deadline);
	clock_gettime(CLOCK_MONOTONIC, ts + 1);

	ts[1].tv_nsec -= ts[0].tv_nsec;
	ts[1].tv_sec -= ts[0].tv_sec;
	if (ts[1].tv_nsec < 0) {
		ts[1].tv_sec -= 1;
		ts[1].tv_nsec += 1000000000;
	}

	printf("%lld.%03ld\n", (long long)ts[1].tv_sec, ts[1].tv_nsec / 1000000);

	return 0;
}
