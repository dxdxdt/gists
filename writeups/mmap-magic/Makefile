CC ?= cc
RUNTIME ?= 30
SETTLEDOWN ?= 2
DATASOURCE ?= /dev/urandom
BUFSIZE ?= 0
CKSUM ?= cksum -a sha256
NC_OPTS ?=
BASELINE_OPTS ?= -m

OBJS = \
	mmagic.o \
	mmap-buffer-test \
	mmap-alias-test \
	mmap-arena-test \
	mmap-stack \
	sparse-array \
	tcp-echo-server \
	normalpath \
	test-normalpath \
	a \
	b
CFLAGS = \
	-std=c11 -Wall -Wextra -Wno-unused-parameter -Wno-unused-result \
	-march=native -mtune=native -O2 -g \
	$(CFLAGS_EXTRA)
CC_CMD = $(CC) $(CFLAGS)
LDADD = -lc
LDADD_NETBSD = -lc -lrt

.PHONY: test test-tcp-echo-server clean

test: mmap-alias-test mmap-arena-test test-normalpath a b
	./test-normalpath
	diff a b
	./mmap-alias-test
	./mmap-arena-test
	# memdev and vfs may behave differently (actually observed w/ Linux)
	! ./mmap-buffer-test -Fw > /dev/null
	! ./mmap-buffer-test -Fw > wrapguard-Fw.tmp

all: $(OBJS)

test-tcp-echo-server: tcp-echo-server
	rm -f hashfifo.tmp lenfifo-in.tmp lenfifo-out.tmp
	mkfifo hashfifo.tmp lenfifo-in.tmp lenfifo-out.tmp
	timeout $$(( $(RUNTIME) + $(SETTLEDOWN) )) $(CKSUM) < "hashfifo.tmp" > "hash-out.tmp" &
	timeout $$(( $(RUNTIME) + $(SETTLEDOWN) )) wc -c < "lenfifo-in.tmp" > "len-in.tmp" &
	timeout $$(( $(RUNTIME) + $(SETTLEDOWN) )) wc -c < "lenfifo-out.tmp" > "len-out.tmp" &

	./tcp-echo-server $(BASELINE_OPTS) -s "$(BUFSIZE)" \
		-D -b "tcp-echo-server.bound.tmp" -p "tcp-echo-server.pid" localhost 0
	timeout $(RUNTIME) cat "$(DATASOURCE)" | tee "hashfifo.tmp" | tee "lenfifo-in.tmp" | \
		nc $(NC_OPTS) localhost $$(grep -Eo '[0-9]+$$' "tcp-echo-server.bound.tmp" | head -n1) | \
		tee "lenfifo-out.tmp" | $(CKSUM) > "hash-in.tmp"
	sync "hash-in.tmp" "hash-out.tmp"
	diff "hash-in.tmp" "hash-out.tmp"
	kill $$(cat "tcp-echo-server.pid") || true
	rm -f "tcp-echo-server.pid"

	rm -f hashfifo.tmp lenfifo-in.tmp lenfifo-out.tmp
	mkfifo hashfifo.tmp lenfifo-in.tmp lenfifo-out.tmp
	timeout $$(( $(RUNTIME) + $(SETTLEDOWN) )) $(CKSUM) < "hashfifo.tmp" > "hash-out.tmp" &
	timeout $$(( $(RUNTIME) + $(SETTLEDOWN) )) wc -c < "lenfifo-in.tmp" > "len-in.tmp" &
	timeout $$(( $(RUNTIME) + $(SETTLEDOWN) )) wc -c < "lenfifo-out.tmp" > "len-out.tmp" &

	./tcp-echo-server -s "$(BUFSIZE)" \
		-D -b "tcp-echo-server.bound.tmp" -p "tcp-echo-server.pid" localhost 0
	timeout $(RUNTIME) cat "$(DATASOURCE)" | tee "hashfifo.tmp" | tee "lenfifo-in.tmp" | \
		nc $(NC_OPTS) localhost $$(grep -Eo '[0-9]+$$' "tcp-echo-server.bound.tmp" | head -n1) | \
		tee "lenfifo-out.tmp" | $(CKSUM) > "hash-in.tmp"
	sync "hash-in.tmp" "hash-out.tmp"
	diff "hash-in.tmp" "hash-out.tmp"
	kill $$(cat "tcp-echo-server.pid") || true
	rm -f "tcp-echo-server.pid"

benchmark-tcp-echo-server: tcp-echo-server
	time timeout $$(( $(RUNTIME) + $(SETTLEDOWN) )) ./tcp-echo-server \
		$(BASELINE_OPTS) -p "tcp-echo-server.pid" -s "$(BUFSIZE)" 7777 &
	! timeout $(RUNTIME) nc $(NC_OPTS) localhost 7777 < "$(DATASOURCE)" > /dev/null
	kill $$(cat "tcp-echo-server.pid") || true
	rm -f "tcp-echo-server.pid"

	time timeout $$(( $(RUNTIME) + $(SETTLEDOWN) )) ./tcp-echo-server \
		-p "tcp-echo-server.pid" -s "$(BUFSIZE)" 7778 &
	! timeout $(RUNTIME) nc $(NC_OPTS) localhost 7778 < "$(DATASOURCE)" > /dev/null
	kill $$(cat "tcp-echo-server.pid") || true
	rm -f "tcp-echo-server.pid"

clean:
	rm -f $(OBJS) *.tmp *.pid

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
	$(CC_CMD) -lpthread -o mmap-stack mmap-stack.c
sparse-array: mmagic.o sparse-array.c
	$(CC_CMD) $(LDADD) sparse-array.c mmagic.o -o sparse-array || \
		$(CC_CMD) $(LDADD_NETBSD) sparse-array.c mmagic.o -o sparse-array
tcp-echo-server: mmagic.o tcp-echo-server.c
	$(CC_CMD) $(LDADD) tcp-echo-server.c mmagic.o -o tcp-echo-server || \
		$(CC_CMD) $(LDADD_NETBSD) tcp-echo-server.c mmagic.o -o tcp-echo-server
normalpath: normalpath.h normalpath.c
	$(CC_CMD) $(LDADD) normalpath.c -o normalpath
test-normalpath: normalpath.h test-normalpath.c mmagic.h mmagic.o
	$(CC_CMD) $(LDADD) test-normalpath.c mmagic.o -o test-normalpath || \
		$(CC_CMD) $(LDADD_NETBSD) test-normalpath.c mmagic.o -o test-normalpath

a: mmap-buffer-test
	./mmap-buffer-test > a.tmp
	mv a.tmp a
b: mmap-buffer-test
	./mmap-buffer-test -w > b.tmp
	mv b.tmp b
