CC ?= cc
OBJS = mmagic.o mmap-buffer-test a b
CFLAGS = \
	-std=c11 -Wall -Wextra -Wno-unused-parameter -Wno-unused-result \
	-march=native -mtune=native -O2 -g

.PHONY: test clean

test: mmap-buffer-test a b
	diff a b

clean:
	rm -f $(OBJS) *.tmp

mmagic.o: mmagic.c mmagic.h
	$(CC) $(CFLAGS) -c -o mmagic.o mmagic.c

mmap-buffer-test: mmap-buffer-test.c mmagic.o
	$(CC) $(CFLAGS) mmap-buffer-test.c mmagic.o -o mmap-buffer-test -lc || \
		$(CC) $(CFLAGS) mmap-buffer-test.c mmagic.o -o mmap-buffer-test -lc -lrt # NetBSD

a:
	./mmap-buffer-test > a.tmp
	mv a.tmp a
b:
	./mmap-buffer-test -w > b.tmp
	mv b.tmp b
