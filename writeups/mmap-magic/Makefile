CC ?= cc
RUNTIME ?= 30
SETTLEDOWN ?= 2
DATASOURCE ?= /dev/urandom
BUFSIZE ?= 0
CKSUM ?= sha256sum

OBJS = mmagic.o mmap-*-test mmap-stack sparse-array tcp-echo-server a b
CFLAGS = \
	-std=c11 -Wall -Wextra -Wno-unused-parameter -Wno-unused-result \
	-march=native -mtune=native -O2 -g \
	$(CFLAGS_EXTRA)
CC_CMD = $(CC) $(CFLAGS)
LDADD = -lc
LDADD_NETBSD = -lc -lrt

.PHONY: test test-tcp-echo-server clean

test: mmap-alias-test mmap-arena-test a b
	diff a b
	./mmap-alias-test
	./mmap-arena-test
	# memdev and vfs may behave differently (actually observed w/ Linux)
	! ./mmap-buffer-test -Fw > /dev/null
	! ./mmap-buffer-test -Fw > wrapguard-Fw.tmp

test-tcp-echo-server: tcp-echo-server hashfifo.tmp
	time timeout $$(( $(RUNTIME) + $(SETTLEDOWN) )) ./tcp-echo-server -zs "$(BUFSIZE)" \
		-p "tcp-echo-server.pid" &
	$(CKSUM) "hashfifo.tmp" | cut -d ' ' -f 1 > "hash-out.tmp" &
	timeout $(RUNTIME) tee "hashfifo.tmp" < "$(DATASOURCE)" | nc localhost 7777 | \
		$(CKSUM) | cut -d ' ' -f 1 > "hash-in.tmp"
	diff "hash-in.tmp" "hash-out.tmp"
	kill -TERM $$(cat "tcp-echo-server.pid")

	time timeout $$(( $(RUNTIME) + $(SETTLEDOWN) )) ./tcp-echo-server -s "$(BUFSIZE)" \
		-p "tcp-echo-server.pid" &
	$(CKSUM) "hashfifo.tmp" | cut -d ' ' -f 1 > "hash-out.tmp" &
	timeout $(RUNTIME) tee "hashfifo.tmp" < "$(DATASOURCE)" | nc localhost 7777 | \
		$(CKSUM) | cut -d ' ' -f 1 > "hash-in.tmp"
	diff "hash-in.tmp" "hash-out.tmp"
	kill -TERM $$(cat "tcp-echo-server.pid")

clean:
	rm -f $(OBJS) *.tmp *.pid

hashfifo.tmp:
	[ -p "$@" ] || mkfifo "$@"

mmagic.o: mmagic.c mmagic.h
	$(CC_CMD) -c -o mmagic.o mmagic.c

mmap-buffer-test: mmap-buffer-test.c mmagic.o
	$(CC_CMD) $(LDADD) mmap-buffer-test.c mmagic.o -o mmap-buffer-test || \
		$(CC_CMD) $(LDADD_NETBSD) mmap-buffer-test.c mmagic.o -o mmap-buffer-test
mmap-alias-test: mmap-alias-test.c mmagic.o
	$(CC_CMD) $(LDADD) mmap-alias-test.c mmagic.o -o mmap-alias-test || \
		$(CC_CMD) $(LDADD_NETBSD) mmap-alias-test.c mmagic.o -o mmap-alias-test
mmap-arena-test: mmap-arena-test.c mmagic.o
	$(CC_CMD) $(LDADD) mmap-arena-test.c mmagic.o -o mmap-arena-test || \
		$(CC_CMD) $(LDADD_NETBSD) mmap-arena-test.c mmagic.o -o mmap-arena-test

mmap-stack: mmap-stack.c
	$(CC_CMD) -o mmap-stack mmap-stack.c
sparse-array: mmagic.o sparse-array.c
	$(CC_CMD) $(LDADD) sparse-array.c mmagic.o -o sparse-array || \
		$(CC_CMD) $(LDADD_NETBSD) sparse-array.c mmagic.o -o sparse-array
tcp-echo-server: mmagic.o tcp-echo-server.c
	$(CC_CMD) $(LDADD) tcp-echo-server.c mmagic.o -o tcp-echo-server || \
		$(CC_CMD) $(LDADD_NETBSD) tcp-echo-server.c mmagic.o -o tcp-echo-server

a: mmap-buffer-test
	./mmap-buffer-test > a.tmp
	mv a.tmp a
b: mmap-buffer-test
	./mmap-buffer-test -w > b.tmp
	mv b.tmp b
