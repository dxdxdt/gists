#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#if defined _WIN32 || defined __CYGWIN__
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#endif

#define ARGV0 "inet-pnp"

static bool do_pn (const char *in, const int af) {
	uint8_t buf[16];
	char out[INET6_ADDRSTRLEN];

	out[0] = 0;
	errno = EBADMSG;
	return
		inet_pton(af, in, buf) > 0 &&
		inet_ntop(af, buf, out, sizeof(out)) != NULL &&
		printf("%s", out);
}

static bool do_gn (const char *in) {
	static const struct addrinfo hints = {
		.ai_flags = AI_NUMERICHOST
	};
	union {
		struct sockaddr_storage storage;
		struct sockaddr a;
		struct sockaddr_in in;
		struct sockaddr_in6 in6;
	} s;
	struct addrinfo *ai = NULL;
	char out[INET6_ADDRSTRLEN];
	bool ret = false;
	const void *addrbuf;
	const int fr = getaddrinfo(in, NULL, &hints, &ai);

	if (fr != 0) {
		const char *errmsg = gai_strerror(fr);
		printf("%s", errmsg);
		ret = true;
		goto END;
	}

	memcpy(&s, ai->ai_addr, ai->ai_addrlen);
	switch (ai->ai_family) {
	case AF_INET: addrbuf = &s.in.sin_addr; break;
	case AF_INET6: addrbuf = &s.in6.sin6_addr; break;
	default:
		errno = EAFNOSUPPORT;
		goto END;
	}
	out[0] = 0;

	errno = EBADMSG;
	if (inet_ntop(ai->ai_family, addrbuf, out, sizeof(out)) == NULL) {
		goto END;
	}

	printf("%s", out);
	ret = true;

END:
	if (ai != NULL) {
		freeaddrinfo(ai);
	}
	return ret;
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
	char line[2][256];
	bool err = false;
	bool proc = false;

#if defined _WIN32 || defined __CYGWIN__
	start_wsa();
#endif

	while (fgets(line[0], sizeof(line[0]), stdin) != NULL) {
		unsigned int cnt = 0;
		// trim
		line[1][0] = 0;
		if (sscanf(line[0], "%255s", line[1]) != 1) {
			// empty line. all whitespace string cannot be captured using %s
			putc('\n', stdout);
			continue;
		}

		printf("pton: ");
		if (do_pn(line[1], AF_INET) || do_pn(line[1], AF_INET6)) {
			cnt += 1;
		}
		else {
			printf("%s", strerror(errno));
		}
		printf(", gai: ");
		if (do_gn(line[1])) {
			cnt += 1;
		}
		else {
			printf("%s", strerror(errno));
		}

		if (cnt > 0) {
			proc = true;
		}
		else {
			err = true;
		}
		putc('\n', stdout);
	}

	if (err) {
		if (proc) {
			return 3;
		}
		return 1;
	}
	return 0;
}