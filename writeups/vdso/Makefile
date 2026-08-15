CC ?= cc

.PHONY: clean test

test: it-gettime
	./it-gettime -h
	./it-gettime 1000000

it-gettime: it-gettime.c
	$(CC) -std=c11 -O2 -g -o it-gettime it-gettime.c

clean:
	rm -f it-gettime
