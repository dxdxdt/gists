/**
 * @file eyeball.c
 * @author David Timber
 * @brief Demonstrates RFC 8305
 * @see
 * https://datatracker.ietf.org/doc/html/rfc6555
 * https://datatracker.ietf.org/doc/html/rfc8305
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <math.h>

#include <getopt.h>
#include <pthread.h>
#if defined _WIN32 || defined __CYGWIN__
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

#define ARGV0 "eyeball"
// RFC 6555: 300 ms preferential delay
// "Resolution Delay" as per RFC 8305 section 3: 50 ms
#define DEFAULT_RES_DELAY 0.3 // 300 ms


void ts_sub (
	struct timespec *c,
	const struct timespec *a,
	const struct timespec *b)
{
	c->tv_nsec = a->tv_nsec - b->tv_nsec;
	c->tv_sec = a->tv_sec - b->tv_sec;
	if (c->tv_nsec < 0) {
		c->tv_sec -= 1;
		c->tv_nsec += 1000000000;
	}
}

enum happy_result {
	// no error: connection established and handshake complete
	HR_NULL,
	// a function call failed and error set to errno
	HR_ERRNO,
	// getaddrinfo() returned error and error is set to the returned value
	HR_GAI,
	// error set to user-defined value
	HR_OTHER,
};

struct happy_error {
	struct {
		struct timespec resolv;
		struct timespec conn;
		struct timespec total;
	} delay;
	enum happy_result result;
	int error;
	bool ready;
};

typedef void*(*happy_connectf_t)(
	void *ctx,
	const struct addrinfo *aiv,
	struct happy_error *out_err);

/*
 * simple reference connectf implementation that creates a TCP socket and
 * connects to the address
 */
void *happy_tcp_connectf (
		void *ctx,
		const struct addrinfo *ai,
		struct happy_error *out_err)
{
	int ret = -1;
	int fr;

	ret = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if (ret < 0) {
		goto ERR;
	}

	fr = connect(ret, ai->ai_addr, ai->ai_addrlen);
	if (fr != 0) {
		goto ERR;
	}
	goto END;

ERR:
	out_err->result = HR_ERRNO;
	out_err->error = errno;
	if (ret >= 0) {
#if defined _WIN32 || defined __CYGWIN__
		closesocket(ret);
#else
		close(ret);
#endif
		ret = -1;
	}

END:
	*((int*)ctx) = ret;
	return NULL;
}

void *happy_th_main_inner (
		const char *node,
		const char *service,
		const struct addrinfo *hints,
		void *ctx,
		happy_connectf_t connf,
		pthread_mutex_t *lock,
		pthread_cond_t *cond,
		struct happy_error *out_err)
{
	int fr;
	void *ret = NULL;
	struct happy_error err = { 0, };
	struct addrinfo *ai = NULL;
	struct {
		struct timespec start;
		struct timespec resolv;
		struct timespec conn;
		struct timespec end;
	} ts = { 0, };

	clock_gettime(CLOCK_MONOTONIC, &ts.start);

	// do resolve
	fr = getaddrinfo(node, service, hints, &ai);
	clock_gettime(CLOCK_MONOTONIC, &ts.resolv);
	if (fr != 0) {
		if (
#ifdef EAI_SYSTEM
				fr == EAI_SYSTEM
#else
				false
#endif
				)
		{
			err.result = HR_ERRNO;
			err.error = errno;
		}
		else {
			err.result = HR_GAI;
			err.error = fr;
		}

		ts.conn = ts.resolv;
		goto END;
	}
	assert(ai != NULL);

	clock_gettime(CLOCK_MONOTONIC, &ts.conn);
	if (connf != NULL) {
		// call the inner function
		ret = connf(ctx, ai, &err);
	}

END:
	clock_gettime(CLOCK_MONOTONIC, &ts.end);
	ts_sub(&err.delay.resolv, &ts.resolv, &ts.start);
	ts_sub(&err.delay.conn, &ts.end, &ts.conn);
	ts_sub(&err.delay.total, &ts.end, &ts.start);

	if (ai != NULL) {
		freeaddrinfo(ai);
	}

	err.ready = true;

	fr = pthread_mutex_lock(lock);
	assert(fr == 0);
	if (out_err != NULL) {
		*out_err = err;
	}
	pthread_cond_broadcast(cond);
	pthread_mutex_unlock(lock);

	return ret;
}

struct happy_th_args {
	const char *node;
	const char *service;
	const struct addrinfo *hints;
	void *ctx;
	happy_connectf_t connf;
	pthread_mutex_t *lock;
	pthread_cond_t *cond;
	struct happy_error *out_err;
};

/*
 * Happy eyeballs worker thread entry
 */
void *happy_th_main (void *args_in) {
	struct happy_th_args *args = args_in;

	return happy_th_main_inner(
		args->node,
		args->service,
		args->hints,
		args->ctx,
		args->connf,
		args->lock,
		args->cond,
		args->out_err
	);
}

int aihintflags (void) {
	int ret = 0;
#ifdef AI_IDN
	ret |= AI_IDN;
#endif
	return ret;
}

struct {
	const char *node;
	const char *service;
	struct timespec op_timeout;
	double res_delay;
	struct {
		bool help;
		bool both;
	} flags;
} opts;

void init_opts (void) {
	// connection timeout: 10 s
	opts.op_timeout.tv_sec = 10;
	opts.op_timeout.tv_nsec = 0;
	opts.res_delay = DEFAULT_RES_DELAY;
}

bool parse_args (const int argc, const char **argv) {
	int fr;
	double tmpf;

	while (true) {
		fr = getopt(argc, (char *const*)argv, "hd:b");
		if (fr < 0) {
			break;
		}

		switch (fr) {
		case 'h': opts.flags.help = true; break;
		case 'd':
			tmpf = NAN;
			fr = sscanf(optarg, "%lf", &tmpf);
			if (fr != 1 || isnan(tmpf)) {
				fprintf(stderr, ARGV0": invalid option -d: %s\n", optarg);
				return false;
			}
			opts.res_delay = tmpf;
			break;
		case 'b':
			opts.flags.both = true;
			break;
		case '?': return false;
		}
	}

	if (opts.flags.help) {
		return true;
	}

	if (argc < optind + 2) {
		fprintf(stderr, ARGV0": too few arguments\n");
		return false;
	}

	opts.node = argv[optind];
	opts.service = argv[optind + 1];

	return true;
}

void print_help (void) {
	printf("Usage: "ARGV0" [-hb] [-d RES_DELAY] <NODE> <SERVICE>\n");
}

struct {
	pthread_mutex_t lock;
	pthread_cond_t cond;
	struct {
		struct addrinfo hints;
		struct happy_th_args args;
		struct happy_error err;
		pthread_t th;
		int ret;
		bool started;
	} th[2];
} g;

bool init_ctx (void) {
	pthread_mutex_init(&g.lock, NULL);
	pthread_cond_init(&g.cond, NULL);

	// hints
	g.th[0].hints.ai_socktype = g.th[1].hints.ai_socktype = SOCK_STREAM;
	g.th[0].hints.ai_flags = g.th[1].hints.ai_flags = aihintflags();
	g.th[0].hints.ai_family = AF_INET6;
	g.th[1].hints.ai_family = AF_INET;

	// args
	g.th[0].args.node = g.th[1].args.node = opts.node;
	g.th[0].args.service = g.th[1].args.service = opts.service;
	g.th[0].args.connf = g.th[1].args.connf = happy_tcp_connectf;
	g.th[0].args.hints = &g.th[0].hints;
	g.th[0].args.ctx = &g.th[0].ret;
	g.th[0].args.lock = &g.lock;
	g.th[0].args.cond = &g.cond;
	g.th[0].args.out_err = &g.th[0].err;
	g.th[1].args.hints = &g.th[1].hints;
	g.th[1].args.ctx = &g.th[1].ret;
	g.th[1].args.lock = &g.lock;
	g.th[1].args.cond = &g.cond;
	g.th[1].args.out_err = &g.th[1].err;

	g.th[0].ret = g.th[1].ret = -1;

	return true;
}

bool spawn_threads (void) {
	int fr;

	for (size_t i = 0; i < 2; i += 1) {
		fr = pthread_create(&g.th[i].th, NULL, happy_th_main, &g.th[i].args);
		g.th[i].started = fr == 0;
		if (!g.th[i].started) {
			errno = fr;
			perror(ARGV0": pthread_create()");
			return false;
		}
	}

	return true;
}

void despawn_threads (void) {
	for (size_t i = 0; i < 2; i += 1) {
		if (!g.th[i].started) {
			continue;
		}
/*
 * ain't pretty, but canceling getaddrinfo() is not possible. Portability comes
 * first in this demo. This is why cross-platform many apps handcraft their own
 * address or use libraries like c-res.
 */
		pthread_cancel(g.th[i].th);
		pthread_join(g.th[i].th, NULL);

		g.th[i].started = false;
	}
}

void deinit_ctx (void) {
	despawn_threads();
	pthread_mutex_destroy(&g.lock);
	pthread_cond_destroy(&g.cond);
}

void get_leadtime (struct timespec *ts, const struct timespec *amt) {
	clock_gettime(CLOCK_REALTIME, ts);
	ts->tv_sec += amt->tv_sec;
	ts->tv_nsec += amt->tv_nsec;
	ts->tv_sec += ts->tv_nsec / 1000000000;
	ts->tv_nsec %= 1000000000;
}

void get_leadtimef (struct timespec *ts, const double lt) {
	struct timespec translated;

	translated.tv_sec = (time_t)lt;
	translated.tv_nsec = (long)((lt - (time_t)lt) * 1000000000);
	get_leadtime(ts, &translated);
}

ssize_t poll_result (void) {
	struct timespec op_deadline;
	int fr;

	get_leadtime(&op_deadline, &opts.op_timeout);

	while (true) {
		if (g.th[0].err.ready && g.th[0].err.result == HR_NULL) {
			// ipv6 made it already
			return 1;
		}

		if (g.th[1].err.ready && g.th[1].err.result == HR_NULL) {
			// ipv4 made it first
			struct timespec bias_deadline;

			if (g.th[0].err.ready) {
				// and ipv6 failed already. no need to do the waiting
				return 2;
			}

			// ipv6 connection attempt still in progress.
			// let's wait around for a bit to see ipv6 makes it in time
			fprintf(stderr, ARGV0": ipv4 shot first\n");

			get_leadtimef(&bias_deadline, opts.res_delay);
			pthread_cond_timedwait(&g.cond, &g.lock, &bias_deadline);

			if (g.th[0].err.ready && g.th[0].err.result == HR_NULL) {
				// ipv6 made it!
				return 1;
			}

			// ipv6 didn't make it
			return 2;
		}

		if (g.th[0].err.ready && g.th[1].err.ready) {
			// both failed
			break;
		}

		// both still trying
		fr = pthread_cond_timedwait(&g.cond, &g.lock, &op_deadline);
		if (fr != 0) {
			// timed out or interrupted
			break;
		}
	}

	return -1;
}

ssize_t poll_both (void) {
	ssize_t ret = 0;
	struct timespec op_deadline;
	int fr;

	get_leadtime(&op_deadline, &opts.op_timeout);

	while (!(g.th[0].err.ready && g.th[1].err.ready)) {
		fr = pthread_cond_timedwait(&g.cond, &g.lock, &op_deadline);
		if (fr != 0) {
			break;
		}
	}

	if (g.th[0].err.result == HR_NULL) {
		ret |= 1;
	}
	if (g.th[1].err.result == HR_NULL) {
		ret |= 2;
	}
	if (ret == 0) {
		ret = -1;
	}

	return ret;
}

void print_sockname (
		const int fd,
#if defined _WIN32 || defined __CYGWIN__
		int(*namef)(SOCKET, struct sockaddr*, socklen_t*)
#else
		int(*namef)(int, struct sockaddr*, socklen_t*)
#endif
		)
{
	union {
		struct sockaddr_storage ss;
		struct sockaddr sa;
		struct sockaddr_in sin;
		struct sockaddr_in6 sin6;
	} addr;
	socklen_t sl = sizeof(addr);
	char str[INET6_ADDRSTRLEN] = { 0 };
	int fr;
	void *addr_data;
	uint16_t port;

	fr = namef(fd, &addr.sa, &sl);
	assert(fr == 0);

	if (addr.sa.sa_family == AF_INET) {
		addr_data = &addr.sin.sin_addr;
		port = ntohs(addr.sin.sin_port);
	}
	else {
		addr_data = &addr.sin6.sin6_addr;
		port = ntohs(addr.sin6.sin6_port);
	}
	inet_ntop(addr.sa.sa_family, addr_data, str, sizeof(str));

	if (addr.sa.sa_family == AF_INET) {
		printf("%s:%"PRIu16, str, port);
	}
	else {
		printf("[%s]:%"PRIu16, str, port);
	}
}

void do_report (const ssize_t picked) {
	char star;

	for (size_t i = 0; i < 2; i += 1) {
		if (picked > 0 && ((1 << i) & picked)) {
			star =  '*';
		}
		else {
			star = ' ';
		}
		printf("%c%s: ", star, g.th[i].hints.ai_family == AF_INET ? "v4" : "v6");

		if (g.th[i].err.ready) {
			printf("%ld.%03lus %ld.%03lus %ld.%03lus ",
				(long)g.th[i].err.delay.resolv.tv_sec,
				(unsigned long)g.th[i].err.delay.resolv.tv_nsec / 1000000,
				(long)g.th[i].err.delay.conn.tv_sec,
				(unsigned long)g.th[i].err.delay.conn.tv_nsec / 1000000,
				(long)g.th[i].err.delay.total.tv_sec,
				(unsigned long)g.th[i].err.delay.total.tv_nsec / 1000000);


			switch (g.th[i].err.result) {
			case HR_GAI:
				printf("%s", gai_strerror(g.th[i].err.error));
				break;
			case HR_ERRNO:
				printf("%s", strerror(g.th[i].err.error));
				break;
			default:
				print_sockname(g.th[i].ret, getsockname);
				printf(" -> ");
				print_sockname(g.th[i].ret, getpeername);
			}
		}
		printf("\n");
	}
}

int do_pick (void) {
	ssize_t picked;

	pthread_mutex_lock(&g.lock);
	if (opts.flags.both) {
		picked = poll_both();
	}
	else {
		picked = poll_result();
	}
	do_report(picked);
	pthread_mutex_unlock(&g.lock);

	if (picked < 0) {
		return 1;
	}
	return 0;
}

#if defined _WIN32 || defined __CYGWIN__
static void start_wsa (void) {
	int fr;
	WSADATA wsaData = {0};

	fr = WSAStartup(MAKEWORD(2, 2), &wsaData);
	assert(fr == 0);
	(void)fr;
}
#endif

int main (const int argc, const char **argv) {
	static int ec = 0;

	init_opts();

#if defined _WIN32 || defined __CYGWIN__
	start_wsa();
#endif

	if (!parse_args(argc, argv)) {
		ec = 2;
		goto END;
	}
	if (opts.flags.help) {
		print_help();
		goto END;
	}

	if (!init_ctx()) {
		ec = 1;
		goto END;
	}
	if (!spawn_threads()) {
		ec = 1;
		goto END;
	}

	ec = do_pick();

END:
	deinit_ctx();
	return ec;
}
