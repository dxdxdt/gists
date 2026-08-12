/*
 * Massively-multiconnection-capable portable TCP echo server.
 */

#include "mmagic.h"
#if defined(__sun) && !defined(_XOPEN_SOURCE) /* Solaris */
/* getopt() */
#define _XOPEN_SOURCE 500
#endif
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>
#include <locale.h>
#include <math.h>
#include <time.h>

#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#if defined(__linux__)
#include <sys/epoll.h>
#else
#include <sys/event.h>
#endif

#define ARGV0 "tcp-echo-server"

static const char *BOOLSTR[2] = { "false", "true" };
static const char *OVFSTR[2] = { "", "OVF" };

/* timespec macro functions copied-and-pasted from libbsd */

#ifndef timespecsub
#define	timespecsub(tsp, usp, vsp)					\
	do {								\
		(vsp)->tv_sec = (tsp)->tv_sec - (usp)->tv_sec;		\
		(vsp)->tv_nsec = (tsp)->tv_nsec - (usp)->tv_nsec;	\
		if ((vsp)->tv_nsec < 0) {				\
			(vsp)->tv_sec--;				\
			(vsp)->tv_nsec += 1000000000L;			\
		}							\
	} while (0)
#endif
#ifndef timespeccmp
#define	timespeccmp(tsp, usp, cmp)					\
	(((tsp)->tv_sec == (usp)->tv_sec) ?				\
	    ((tsp)->tv_nsec cmp (usp)->tv_nsec) :			\
	    ((tsp)->tv_sec cmp (usp)->tv_sec))
#endif

enum sig_opcode {
	SOPC_NONE,
	SOPC_QUIT,
	SOPC_STAT,
	NB_SOPC,
};

#define NB_RSV_FD		(3)
#define NB_AF			(2)
#define PFD_AF_START		(1)
#define PFD_TERMPIPE_IDX	(0)
#define MAX_CONN		(INT_MAX - NB_RSV_FD)

static struct {
	size_t bufsize;
	const char *node;
	const char *service;
	unsigned int maxconn;		/* (0, MAX_CONN] */
	int af[NB_AF];
	struct timespec timeout;
	int evtpt;			/* events per tick (0, INT_MAX] */
	bool help:1;
	bool nocirc:1;
	bool reuseport:1;
} param = {
	.bufsize = 64 * 1024,		/* 64KB: highest pagesize (AArch64) */
	.node = NULL,
	.service = "7777",
	.maxconn = MAX_CONN,
	.af = { AF_INET, AF_INET6 },
	.timeout = { .tv_sec = 60, },	/* 1 min */
	.evtpt = 1024,
};

struct cbuffer {
	void *m;
	size_t size;
	size_t ofs;
	size_t len;
};

#define CCTX_FIN		(0x01)
#define CCTX_STAT_OVF_IN	(0x02)
#define CCTX_STAT_OVF_OUT	(0x04)
#define CCTX_ERR		(0x08)
#define CCTX_TIMEDOUT		(0x10)

#define KEVENT_REG_DEL		(0x00)
#define KEVENT_REG_IN		(0x01)
#define KEVENT_REG_OUT		(0x02)

struct evtcb_ctx {
	void *ctx;
	bool (*callback)(void *ctx, int trig, int *out);
	int fd;
};

struct client_ctx {
	TAILQ_ENTRY(client_ctx) entries;
	struct cbuffer buf;
	int fd;
	int err;
	int flags;
	struct evtcb_ctx cb;
	union {
		struct sockaddr a;
		struct sockaddr_in in;
		struct sockaddr_in6 in6;
	} s;
	struct {
		struct timespec since;
		struct timespec last;
	} ts;
	uintmax_t total_in;
	uintmax_t total_out;
};

TAILQ_HEAD(client_entry_head, client_ctx);

static struct {
	/*
	 * The client list in the LRU order
	 *
	 * The client that has done I/O most recently appears first.
	 *
	 * When an element gets added/removed/operated, it will be moved to the
	 * start of the list. When handling connection timeout in
	 * cull_timedout(), keep removing the last element until the last
	 * element is the one that hasn't timed out.
	 */
	struct {
		struct client_entry_head head;
		size_t cnt;
	} clist;
	struct mmem_arena arena;
	struct evtcb_ctx termpipe_cb;
	int termpipe[2];
	int sck[2];
	struct evtcb_ctx sck_cb[2];
	int evt;
	struct {
		struct timespec now;
		struct timespec ts_wait_timeout;
		int ms_wait_timeout;
		bool breather:1;
		bool exiting:1;
	} loop_ctx;
	struct {
		uintmax_t accepted;
		uintmax_t timedout;
		uintmax_t erred;
		uintmax_t total_in;
		uintmax_t total_out;
		bool ovf_accepted:1;
		bool ovf_total_in:1;
		bool ovf_total_out:1;
		bool wrapped:1;
		bool breather:1;
	} stat;
	struct {
		int size;
		int cnt;
#if defined(__linux__)
		struct epoll_event *arr;
#else
		struct kevent *arr;
#endif
	} eobj;
	_Alignas(64) char sa_buf[INET6_ADDRSTRLEN - 1 + sizeof("[]:65535")];
} server = {
	.termpipe = { -1, -1 },
	.sck = { -1, -1 },
	.evt = -1,
};

static inline bool timespecms(const struct timespec *ts, int *out)
{
	bool ret = false;
	const int ms = ts->tv_nsec / 1000000;

	*out = ts->tv_sec * 1000;
	ret |= *out < 0 || *out < ts->tv_sec;
	*out += ms;
	ret |= *out < 0;

	return ret;
}

static void mksockaddrstr(const struct sockaddr *a)
{
	const char *ret;
	const void *addr = NULL;
	uint16_t port = 0;
	union {
		const struct sockaddr *a;
		const struct sockaddr_in *in;
		const struct sockaddr_in6 *in6;
	} s = { .a = a };
	char tmp[INET6_ADDRSTRLEN];
	const char *b[2];

	server.sa_buf[0] = 0;

	switch (s.a->sa_family) {
	case AF_INET:
		addr = &s.in->sin_addr;
		port = s.in->sin_port;
		b[0] = b[1] = "";
		break;
	case AF_INET6:
		addr = &s.in6->sin6_addr;
		port = s.in6->sin6_port;
		b[0] = "[";
		b[1] = "]";
		break;
	default:
		assert(s.a->sa_family == AF_INET || s.a->sa_family == AF_INET6);
	}
	port = ntohs(port);

	tmp[0] = 0;
	ret = inet_ntop(s.a->sa_family, addr, tmp, sizeof(tmp));
	if (ret == NULL)
		goto err;

	if (snprintf(server.sa_buf, sizeof(server.sa_buf), "%s%s%s:%" PRIu16,
			b[0], tmp, b[1], port) < 0)
		goto err;

	return;
err:
	snprintf(server.sa_buf, sizeof(server.sa_buf), "-%d", errno);
}

/* Kernel fd object event abstraction layer */

#ifdef __linux__
#else
#endif

static int kevent_aton(const int app)
{
#ifdef __linux__
	switch (app) {
	case KEVENT_REG_IN:	return EPOLLIN;
	case KEVENT_REG_OUT:	return EPOLLOUT;
	}
#else
	switch (app) {
	case KEVENT_REG_IN:	return EVFILT_READ;
	case KEVENT_REG_OUT:	return EVFILT_WRITE;
	}
#endif
	return INT_MIN;
}

static int kevent_ntoa(const int native)
{
#ifdef __linux__
	if (native & EPOLLOUT)
		return KEVENT_REG_OUT;
	if (native & EPOLLIN)
		return KEVENT_REG_IN;
#else
	switch (native) {
	case EVFILT_READ:	return KEVENT_REG_IN;
	case EVFILT_WRITE:	return KEVENT_REG_OUT;
	}
#endif
	return KEVENT_REG_DEL;
}

static bool setup_kevent(void)
{
	assert(server.evt < 0);

#ifdef __linux__
	server.evt = epoll_create1(0);
#else
	server.evt = kqueue1(0);
#endif
	if (server.evt < 0)
		return false;
	return true;
}

static bool kevent_register(int fd, struct evtcb_ctx *ctx, int event)
{
	bool ret;

#ifdef __linux__
	struct epoll_event ke = {
		.events = kevent_aton(event),
		.data.ptr = ctx,
	};

	ret = epoll_ctl(server.evt, EPOLL_CTL_MOD, fd, &ke) == 0;
	if (!ret && errno == ENOENT)
		ret = epoll_ctl(server.evt, EPOLL_CTL_ADD, fd, &ke) == 0;
#else
	/*
	 * kqueue allows multiple filters for one fd. Disable all the other
	 * filters first before adding/updating the one desired.
	 */
	static const int ARR[2] = { EVFILT_READ, EVFILT_WRITE };
	const int filter = kevent_aton(event);
	int flags;
	struct kevent ke;

	ret = false;

	flags = EV_ADD | EV_DISABLE;
	for (size_t i = 0; i < sizeof(ARR) / sizeof(*ARR); i++) {
		if (ARR[i] == filter)
			continue;

		EV_SET(&ke, fd, ARR[i], flags, 0, 0, ctx);
		ret |= kevent(server.evt, &ke, 1, NULL, 0, NULL) < 0;
	}

	flags = EV_ADD | EV_ENABLE | EV_CLEAR;
	EV_SET(&ke, fd, filter, flags, 0, 0, ctx);
	ret |= kevent(server.evt, &ke, 1, NULL, 0, NULL) < 0;
	ret = !ret;
#endif

	return ret;
}

static bool kevent_unregister(int fd)
{
	bool ret = true;

#ifdef __linux__
	struct epoll_event ke = {};

	ret = epoll_ctl(server.evt, EPOLL_CTL_DEL, fd, &ke) == 0;
#else
	struct kevent ke;

	EV_SET(&ke, fd, 0, EV_DELETE, 0, 0, NULL);
	ke.flags = EVFILT_READ;
	ret |= kevent(server.evt, &ke, 1, NULL, 0, NULL) > 0;
	ke.flags = EVFILT_WRITE;
	ret |= kevent(server.evt, &ke, 1, NULL, 0, NULL) > 0;
#endif

	return ret;
}

static bool kevent_wait(void)
{
#ifdef __linux__
	server.eobj.cnt = epoll_wait(server.evt, server.eobj.arr, server.eobj.size,
				     server.loop_ctx.ms_wait_timeout);
#else
	struct timespec *timeout;

	if (server.loop_ctx.ms_wait_timeout < 0)
		timeout = NULL;
	else
		timeout = &server.loop_ctx.ts_wait_timeout;

	server.eobj.cnt = kevent(server.evt, NULL, 0, server.eobj.arr,
				 server.eobj.size, timeout);
#endif

	return server.eobj.cnt >= 0;
}

static void kevent_iterate(void)
{
	int revents, req;
	struct evtcb_ctx *ctx;
	bool valid, reg_result;

	for (int i = 0; i < server.eobj.cnt; i++) {
#ifdef __linux__
		revents = server.eobj.arr[i].events;
		ctx = server.eobj.arr[i].data.ptr;
#else
		revents = server.eobj.arr[i].filter;
		ctx = server.eobj.arr[i].udata;
#endif

		req = revents = kevent_ntoa(revents);
		assert(revents != 0);
		valid = ctx->callback(ctx->ctx, revents, &req);
		if (valid && revents != req) {
			if (req == KEVENT_REG_DEL)
				reg_result = kevent_unregister(ctx->fd);
			else
				reg_result = kevent_register(ctx->fd, ctx, req);
			assert(reg_result);
		}
	}
}

static bool serve_client(void *ctx_in, int trig, int *out);

static struct client_ctx *add_conn_ctx(const int fd)
{
	struct client_ctx *ret;
	int req_flags = 0;

	if (server.clist.cnt >= param.maxconn) {
		errno = EUSERS;
		return NULL;
	}

	ret = calloc(1, sizeof(*ret));
	if (ret == NULL)
		return NULL;
	if (param.nocirc)
		req_flags |= MEMFILE_WRAPGUARD;
	ret->buf.m = mmem_arena_req(&server.arena, param.bufsize,
			&ret->buf.size, NULL, &req_flags);
	if (ret->buf.m == NULL)
		goto err;

	ret->cb.ctx = ret;
	ret->cb.callback = serve_client;
	ret->cb.fd = ret->fd = fd;
	ret->ts.since = ret->ts.last = server.loop_ctx.now;
	if (!kevent_register(fd, &ret->cb, KEVENT_REG_IN))
		goto err;

	TAILQ_INSERT_HEAD(&server.clist.head, ret, entries);
	server.clist.cnt++;

	server.stat.accepted++;
	if (server.stat.accepted == 0)
		server.stat.ovf_accepted = true;

	return ret;
err:
	mmem_arena_rm(&server.arena, ret->buf.m, NULL);
	free(ret);
	return NULL;
}

static void rm_conn_ctx(struct client_ctx *c)
{
	const char *brackets[2], *errmsg;

	assert(server.clist.cnt > 0);

	mksockaddrstr(&c->s.a);
	if (c->flags & CCTX_TIMEDOUT)
		server.stat.timedout++;
	if (c->flags & CCTX_ERR) {
		server.stat.erred++;

		errmsg = strerror(c->err);
		brackets[0] = "(";
		brackets[1] = ")";
	} else
		brackets[0] = brackets[1] = errmsg = "";
	fprintf(stderr, ARGV0 ": %s: closed%s%s%s%s%s: in=%" PRIuMAX " %s out=%" PRIuMAX " %s\n",
			server.sa_buf,
			c->flags & CCTX_TIMEDOUT ?	" TIMEDOUT" : "",
			c->flags & CCTX_ERR ?		" ERROR" : "",
			brackets[0], errmsg, brackets[1],
			c->total_in, OVFSTR[!!(c->flags & CCTX_STAT_OVF_IN)],
			c->total_out, OVFSTR[!!(c->flags & CCTX_STAT_OVF_OUT)]);

	TAILQ_REMOVE(&server.clist.head, c, entries);
	server.clist.cnt--;
	mmem_arena_rm(&server.arena, c->buf.m, NULL);
	kevent_unregister(c->fd);
	close(c->fd);

	free(c);
}

static const struct addrinfo *select_addr(const struct addrinfo *ai, const int af)
{
	if (af < 0)
		return NULL;

	for (const struct addrinfo *p = ai; p != NULL; p = p->ai_next) {
		if (af == p->ai_family)
			return p;
	}

	return NULL;
}

static void setnonblock(int fd, const bool v)
{
	int flags = fcntl(fd, F_GETFL);

	if (v)
		flags |= O_NONBLOCK;
	else
		flags &= ~(int)O_NONBLOCK;

	fcntl(fd, F_SETFL, flags);
}

static bool setup_server_socket(int *gai_err)
{
	bool ret = false, tried = false;
	int err;
	int sov;
	struct addrinfo hints = {0};
	struct addrinfo *ai = NULL;
	const struct addrinfo *selected;

	(void)sov;

	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	err = getaddrinfo(param.node, param.service, &hints, &ai);
	if (err) {
		*gai_err = err;
		goto out;
	}

	for (size_t i = 0; i < NB_AF; i++) {
		selected = select_addr(ai, param.af[i]);
		if (selected == NULL)
			continue;
		tried = true;

		server.sck[i] = socket(selected->ai_family, selected->ai_socktype, selected->ai_protocol);
		if (server.sck[i] < 0)
			goto out;

#ifdef IPV6_V6ONLY
		if (selected->ai_family == AF_INET6) {
			sov = 1;
			setsockopt(server.sck[i], IPPROTO_IPV6, IPV6_V6ONLY, &sov, sizeof(sov));
		}
#endif
#ifdef SO_REUSEADDR
		sov = 1;
		setsockopt(server.sck[i], SOL_SOCKET, SO_REUSEADDR, &sov, sizeof(sov));
#endif
		sov = param.reuseport;
#if	defined(SO_REUSEPORT_LB)
		setsockopt(server.sck[i], SOL_SOCKET, SO_REUSEPORT_LB, &sov, sizeof(sov));
#elif	defined(SO_REUSEPORT)
		setsockopt(server.sck[i], SOL_SOCKET, SO_REUSEPORT, &sov, sizeof(sov));
#endif

		if (bind(server.sck[i], selected->ai_addr, selected->ai_addrlen) != 0 ||
				listen(server.sck[i], param.evtpt) != 0)
			goto out;
		setnonblock(server.sck[i], true);
	}

	if (!tried) {
		errno = EAFNOSUPPORT;
		return false;
	}
	return true;
out:
	if (ai != NULL)
		freeaddrinfo(ai);
	for (size_t i = 0; i < NB_AF; i++) {
		if (server.sck[i] >= 0) {
			close(server.sck[i]);
			server.sck[i] = -1;
		}
	}

	return ret;
}

static void handle_user_sig(int signum)
{
	enum sig_opcode o;

	switch (signum) {
	case SIGTERM:
	case SIGINT:
		o = SOPC_QUIT;
		break;
	case SIGUSR1:
		o = SOPC_STAT;
		break;
	case SIGUSR2:
	default:
		o = SOPC_NONE;
	}

	write(server.termpipe[1], &o, sizeof(o));
}

static void setup_sighandlers(void)
{
	struct sigaction sa = {0};

	sa.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sa, NULL);

	sa.sa_handler = handle_user_sig;
	sa.sa_flags = SA_RESETHAND | SA_RESTART;
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);

	sa.sa_flags = SA_RESTART;
	sigaction(SIGUSR1, &sa, NULL);
	sigaction(SIGUSR2, &sa, NULL);
}

static void print_stats(void)
{
	printf(ARGV0 " stats:\n"
		"  accepted: %" PRIuMAX " %s\n"
		"  active: %zu\n"
		"  timedout: %" PRIuMAX "\n"
		"  erred: %" PRIuMAX "\n"
		"  bytes-in: %" PRIuMAX " %s\n"
		"  bytes-out: %" PRIuMAX " %s\n"
		"  wrapped: %s\n"
		"  breather: %s\n",
		server.stat.accepted, OVFSTR[server.stat.ovf_accepted],
		server.clist.cnt,
		server.stat.timedout,
		server.stat.erred,
		server.stat.total_in, OVFSTR[server.stat.ovf_total_in],
		server.stat.total_out, OVFSTR[server.stat.ovf_total_out],
		BOOLSTR[server.stat.wrapped],
		BOOLSTR[server.stat.breather]);
}

static bool proc_termpipe(void *ctx, int trig, int *out)
{
	ssize_t rsize;
	enum sig_opcode opcode;

	for (;;) {
		rsize = read(server.termpipe[0], &opcode, sizeof(opcode));
		if (rsize == 0) {
			*out = KEVENT_REG_DEL;
			break;
		} else if (rsize < 0) {
			assert(errno == EAGAIN || errno == EWOULDBLOCK);
			break;
		}
		assert(rsize == sizeof(opcode));

		switch (opcode) {
		case SOPC_NONE:
			fprintf(stderr, ARGV0 ": pong\n");
			break;
		case SOPC_QUIT:
			fprintf(stderr, ARGV0 ": exiting ...\n");
			server.loop_ctx.exiting = true;
			break;
		case SOPC_STAT:
			print_stats();
			break;
		default:
			fprintf(stderr, ARGV0 ": signal opcode %d: %s\n",
					(int)opcode, strerror(ENOSYS));
		}
	}

	return true;
}

static bool serve_incoming(void *ctx_in, int trig, int *out)
{
	int fd = *((int*)ctx_in);
	int newconn;
	int saved_errno;
	struct client_ctx *c;
	union {
		struct sockaddr a;
		struct sockaddr_in in;
		struct sockaddr_in6 in6;
	} s;
	socklen_t sl = sizeof(s);

	for (;;) {
		newconn = accept(fd, &s.a, &sl);
		if (newconn < 0) {
			saved_errno = errno;

			switch (saved_errno) {
			case ENOBUFS:
			case ENOMEM:
				server.loop_ctx.breather = true;
				/* fall-through */
			case EINTR:
			case EAGAIN:
#if EAGAIN != EWOULDBLOCK
			case EWOULDBLOCK:
#endif
				break;
			default:
				assert(saved_errno == ENOMEM ||
						saved_errno == EINTR ||
						saved_errno == EAGAIN ||
						saved_errno == EWOULDBLOCK);
			}

			return true;
		}
		mksockaddrstr(&s.a);

		c = add_conn_ctx(newconn);
		if (c == NULL) {
			if (errno == ENOMEM)
				server.loop_ctx.breather = true;
			fprintf(stderr, ARGV0 ": %s: %s\n",
					server.sa_buf, strerror(errno));
			close(newconn);
		} else {
			fprintf(stderr, ARGV0 ": %s: accepted\n", server.sa_buf);
			memcpy(&c->s, &s, sl);
		}
	}

	return true;
}

static ssize_t do_rw_nocirc(int fd, ssize_t (*rw)(int fd, void *buf, size_t len),
		uint8_t *buf, size_t size, size_t ofs, size_t len)
{
	ssize_t ret;

	if (ofs + len > size) {
		ssize_t rwsize;

		ret = rw(fd, buf + ofs, size - ofs);
		if (ret <= 0)
			return ret;

		ofs += (size_t)ret;
		len -= (size_t)ret;
		if (ofs >= size)
			ofs -= size;

		rwsize = rw(fd, buf + ofs, len);
		if (rwsize > 0)
			ret += (size_t)rwsize;
	} else
		ret = rw(fd, buf + ofs, len);

	return ret;
}

static ssize_t do_rw(int fd, ssize_t (*rw)(int fd, void *buf, size_t len),
		uint8_t *buf, size_t size, size_t ofs, size_t len)
{
	if (param.nocirc)
		return do_rw_nocirc(fd, rw, buf, size, ofs, len);

	if (ofs + len >= size)
		server.stat.wrapped = true;
	return rw(fd, buf + ofs, len);
}

static bool serve_client(void *ctx_in, int trig, int *out)
{
	struct client_ctx *c = ctx_in;
	size_t avail;
	ssize_t rwsize;

	if (trig == KEVENT_REG_IN) {
		size_t ofs = c->buf.ofs + c->buf.len;

		avail = c->buf.size - c->buf.len;
		if (ofs >= c->buf.size)
			/* Don't use mod(%) because division is slow! */
			ofs -= c->buf.size;

		rwsize = do_rw(c->fd, read, c->buf.m, c->buf.size, ofs, avail);
		if (rwsize == 0)
			c->flags |= CCTX_FIN;
		else if (rwsize < 0)
			goto rwerr;
		else {
			assert(avail >= (size_t)rwsize);

			c->buf.len += (size_t)rwsize;
			avail -= (size_t)rwsize; /* will get optimised away */

			c->total_in += (size_t)rwsize;
			if (c->total_in < (size_t)rwsize)
				c->flags |= CCTX_STAT_OVF_IN;

			server.stat.total_in += (size_t)rwsize;
			if (server.stat.total_in < (size_t)rwsize)
				server.stat.ovf_total_in = true;
		}
	} else if (trig == KEVENT_REG_OUT) {
		rwsize = do_rw(c->fd, (ssize_t(*)(int fd, void *buf, size_t len))write,
				c->buf.m, c->buf.size, c->buf.ofs, c->buf.len);
		if (rwsize == 0) {
			errno = EIO;
			rwsize = -1;
		}
		if (rwsize < 0)
			goto rwerr;
		else {
			assert(c->buf.len >= (size_t)rwsize);

			c->buf.ofs += (size_t)rwsize;
			if (c->buf.ofs >= c->buf.size)
				/* Don't use mod(%) because division is slow! */
				c->buf.ofs -= c->buf.size;
			c->buf.len -= (size_t)rwsize;

			c->total_out += (size_t)rwsize;
			if (c->total_out < (size_t)rwsize)
				c->flags |= CCTX_STAT_OVF_OUT;

			server.stat.total_out += (size_t)rwsize;
			if (server.stat.total_out < (size_t)rwsize)
				server.stat.ovf_total_out = true;
		}
	}

	c->ts.last = server.loop_ctx.now;
	TAILQ_REMOVE(&server.clist.head, c, entries);
	TAILQ_INSERT_HEAD(&server.clist.head, c, entries);

	if (c->buf.len > 0)
		*out = KEVENT_REG_OUT;
	else if (c->flags & CCTX_FIN)
		goto remove;
	else
		*out = KEVENT_REG_IN;

	return true;
rwerr:
	c->err = errno;
	switch (c->err) {
	case ENOBUFS:
	case ENOMEM:
		server.loop_ctx.breather = true;
		/* fall-through */
	case EINTR:
		return true;
	}
	c->flags |= CCTX_ERR;
remove:
	rm_conn_ctx(c);
	return false;
}

static void cull_timedout(void)
{
	struct client_ctx *c;
	struct timespec dt;

	for (;;) {
		c = TAILQ_LAST(&server.clist.head, client_entry_head);
		if (c == NULL)
			return;

		timespecsub(&server.loop_ctx.now, &c->ts.last, &dt);
		if (timespeccmp(&dt, &param.timeout, <))
			return;

		c->flags |= CCTX_TIMEDOUT;
		rm_conn_ctx(c);
	}
}

static void update_timeout(void)
{
	struct timespec dt;
	struct client_ctx *c = TAILQ_LAST(&server.clist.head, client_entry_head);

	if (param.timeout.tv_sec < 0 || c == NULL) {
		server.loop_ctx.ms_wait_timeout = -1;
		server.loop_ctx.ts_wait_timeout.tv_sec = -1;
		server.loop_ctx.ts_wait_timeout.tv_nsec = 0;
		return;
	}

	timespecsub(&server.loop_ctx.now, &c->ts.last, &dt);
	if (timespeccmp(&dt, &param.timeout, <)) {
		int ms;

		timespecsub(&param.timeout, &dt, &server.loop_ctx.ts_wait_timeout);
		if (timespecms(&server.loop_ctx.ts_wait_timeout, &ms))
			ms = INT_MAX; /* around 24 days */
		server.loop_ctx.ms_wait_timeout = ms;
	} else {
		server.loop_ctx.ms_wait_timeout = 0;
		server.loop_ctx.ts_wait_timeout.tv_sec = 0;
		server.loop_ctx.ts_wait_timeout.tv_nsec = 0;
	}
}

static void server_loop(void)
{
	bool b;
	int saved_errno;

	do {
		if (server.loop_ctx.breather) {
			static const struct timespec yieldtime = { /* 25 ms */
				.tv_sec = 0,
				.tv_nsec = 25000000,
			};

			nanosleep(&yieldtime, NULL);
			server.loop_ctx.breather = false;
			server.stat.breather = true;
		}

		update_timeout();
		b = kevent_wait();
		saved_errno = errno;
		clock_gettime(CLOCK_MONOTONIC, &server.loop_ctx.now);
		if (b)
			kevent_iterate();
		else {
			switch (saved_errno) {
			case ENOMEM:
				server.loop_ctx.breather = true;
				/* fall-through */
			case EINTR:
				continue;
			default:
				assert(saved_errno == EINTR || saved_errno == ENOMEM);
			}
		}

		cull_timedout();
	} while (!server.loop_ctx.exiting);
}

static void destroy_all_conn(void)
{
	struct client_ctx *c;

	while (!TAILQ_EMPTY(&server.clist.head)) {
		c = TAILQ_FIRST(&server.clist.head);
		rm_conn_ctx(c);
	}
}

static void usage(FILE *f, const bool full)
{
	fprintf(f, "Usage: " ARGV0 " [-h] [-46Rz] [-s SIZE] [-M MAX] [-T TIMEOUT] [HOST] [PORT]\n");
	if (!full)
		return;
	fprintf(f, "Options:\n"
			"  -h: print this message and exit\n"
			"  -4: bind to IPv4 only\n"
			"  -6: bind to IPv6 only\n"
			"  -R: enable SO_REUSEPORT(_LB)\n"
			"  -z: treat buffer as non-circular\n"
			"  -s SIZE: desired buffer size\n"
			"           (actual size is rounded up to system page size)\n"
			"  -M MAX: max connections\n"
			"  -T TIMEOUT: connection inactivity timeout in seconds\n");
}

static void parse_opts(int argc, char *argv[])
{
	bool noarg = false;

	for (;;) {
		const int c = getopt(argc, (char *const *)argv, "h46Rzs:M:T:");
		int err;
		double tmpf;

		switch (c) {
		case 'h':
			noarg = param.help = true;
			break;
		case '4':
			param.af[0] = AF_INET;
			param.af[1] = -1;
			break;
		case '6':
			param.af[0] = AF_INET6;
			param.af[1] = -1;
			break;
		case 'R':
			param.reuseport = true;
			break;
		case 'z':
			param.nocirc = true;
			break;
		case 's':
			errno = EINVAL;
			err = sscanf(optarg, "%zu", &param.bufsize);
			if (err != 1 || param.bufsize == 0)
				goto inval;
			break;
		case 'M':
			errno = EINVAL;
			err = sscanf(optarg, "%u", &param.maxconn);
			if (err != 1)
				goto inval;

			errno = ERANGE;
			if (param.maxconn == 0)
				param.maxconn = MAX_CONN;
			else if (param.maxconn > MAX_CONN)
				goto inval;
			break;
		case 'T':
			/* Same as what the sleep command accepts including "inf" or "infinity" */
			errno = EINVAL;
			err = sscanf(optarg, "%lf", &tmpf);
			if (err != 1)
				goto inval;

			errno = ERANGE;
			if (isnan(tmpf) || tmpf < 0.0)
				goto inval;
			if (isinf(tmpf)) {
				param.timeout.tv_sec = -1;
				param.timeout.tv_nsec = 0;
			} else {
				param.timeout.tv_sec = (time_t)tmpf;
				tmpf -= (intmax_t)tmpf;
				param.timeout.tv_nsec = (long)(tmpf * 1000000000.0);
			}
			break;
		default:
			if (c < 0)
				goto done;
			goto do_exit;
		}
	}
done:
	if (noarg)
		return;
	if (optind + 2 < argc) {
		fprintf(stderr, ARGV0 ": too many arguments\n");
		goto do_exit;
	}

	if (optind + 1 < argc) {
		param.node = argv[optind];
		param.service = argv[optind + 1];
		if (param.node[0] == 0)
			param.node = NULL;
	} else if (optind < argc)
		param.service = argv[optind];

	return;
inval:
	fprintf(stderr, ARGV0 ": %s: %s\n", optarg, strerror(errno));
do_exit:
	usage(stderr, false);
	exit(2);
}

int main(int argc, char *argv[])
{
	const char *errmsg;
	int ret = 1, gai_err = 0;

	setlocale(LC_ALL, "");

	/* some arch checks */
	assert(NB_RSV_FD < INT_MAX);
	assert((int)UINT_MAX == -1); /* processor represents signed integers in 2's complement */

	parse_opts(argc, argv);
	if (param.help) {
		usage(stdout, true);
		exit(0);
	}

	TAILQ_INIT(&server.clist.head);
	mmem_arena_init(&server.arena);

	if (!mmem_arena_open(&server.arena, NULL)) {
		errmsg = "mmem_arena_open()";
		goto err;
	}

	if (param.maxconn > UINT_MAX - NB_RSV_FD) {
		errmsg = "max number of connection";
		errno = EOVERFLOW;
		goto err;
	}

	server.eobj.size = param.evtpt + NB_RSV_FD;
	assert(server.eobj.size > 0);
	server.eobj.arr = calloc(server.eobj.size, sizeof(*server.eobj.arr));
	if (server.eobj.arr == NULL) {
		errmsg = "init";
		goto err;
	}

	if (!setup_kevent()) {
		errmsg = "setup_kevent()";
		goto err;
	}

	if (pipe(server.termpipe) != 0) {
		errmsg = "pipe()";
		goto err;
	}
	setnonblock(server.termpipe[0], true);
	setnonblock(server.termpipe[1], true);
	server.termpipe_cb.callback = proc_termpipe;
	server.termpipe_cb.fd = server.termpipe[0];
	if (!kevent_register(server.termpipe[0], &server.termpipe_cb, KEVENT_REG_IN)) {
		errmsg = "kevent_register()";
		goto err;
	}

	setup_sighandlers();

	if (!setup_server_socket(&gai_err)) {
		errmsg = "setup_server_socket()";
		goto err;
	}
	for (size_t i = 0; i < NB_AF; i++) {
		union {
			struct sockaddr a;
			struct sockaddr_in in;
			struct sockaddr_in6 in6;
		} s;
		socklen_t sl = sizeof(s);

		if (server.sck[i] < 0)
			continue;

		server.sck_cb[i].ctx = &server.sck[i];
		server.sck_cb[i].callback = serve_incoming;
		server.sck_cb[i].fd = server.sck[i];
		if (!kevent_register(server.sck[i], &server.sck_cb[i], KEVENT_REG_IN)) {
			errmsg = "kevent_register()";
			goto err;
		}

		memset(&s, 0, sizeof(s));
		getsockname(server.sck_cb[i].fd, &s.a, &sl);
		mksockaddrstr(&s.a);
		fprintf(stderr, ARGV0 ": bound to %s\n", server.sa_buf);
	}

	server_loop();
	destroy_all_conn();
	print_stats();

	ret = 0;
	goto out;
err:
	if (gai_err == 0 || gai_err == EAI_SYSTEM)
		fprintf(stderr, ARGV0 ": %s: %s\n", errmsg, strerror(errno));
	else
		fprintf(stderr, ARGV0 ": %s: %s\n", errmsg, gai_strerror(gai_err));
out:
	close(server.evt);

	for (size_t i = 0; i < NB_AF; i++)
		close(server.sck[i]);
	close(server.termpipe[0]);
	close(server.termpipe[1]);

	free(server.eobj.arr);
	mmem_arena_close(&server.arena, true);

	return ret;
}
