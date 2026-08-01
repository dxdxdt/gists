CC ?= cc
OBJS = mmagic.o mmap-*-test a b
CFLAGS = \
	-std=c11 -Wall -Wextra -Wno-unused-parameter -Wno-unused-result \
	-march=native -mtune=native -O2 -g \
	$(CFLAGS_EXTRA)
CC_CMD = $(CC) $(CFLAGS)
LDADD = -lc
LDADD_NETBSD = -lc -lrt

.PHONY: test clean

test: mmap-alias-test mmap-arena-test a b
	diff a b
	./mmap-alias-test
	./mmap-arena-test

clean:
	rm -f $(OBJS) *.tmp

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

a: mmap-buffer-test
	./mmap-buffer-test > a.tmp
	mv a.tmp a
b: mmap-buffer-test
	./mmap-buffer-test -w > b.tmp
	mv b.tmp b
