CC ?= cc

all: a b c d e sxsdiff-a-c sxsdiff-c-e fibo-crude-mp

.PHONY: clean doplot all test

clean:
	rm -f a b c d e fibo-crude-mp sxsdiff-*

a: fibo.py
	./fibo.py | cat -n > a.tmp
	mv a.tmp a

b: fibo.mjs
	./fibo.mjs 1 | cat -n > b.tmp
	mv b.tmp b

c: fibo.mjs
	./fibo.mjs 0 | cat -n > c.tmp
	mv c.tmp c

d: fibo-crude-mp
	./fibo-crude-mp 1 | cat -n > d.tmp
	mv d.tmp d

e: fibo-crude-mp
	./fibo-crude-mp 0 | cat -n > e.tmp
	mv e.tmp e

test: a b d
	diff a b
	diff b d

sxsdiff-a-c: a c
	! diff --suppress-common-lines -y a c > sxsdiff-a-c.tmp
	mv sxsdiff-a-c.tmp sxsdiff-a-c

sxsdiff-c-e: c e
	! diff --suppress-common-lines -y c e > sxsdiff-c-e.tmp
	mv sxsdiff-c-e.tmp sxsdiff-c-e

doplot: a c e
	gnuplot plot

fibo-crude-mp: fibo-crude-mp.c
	$(CC) -std=c99 -Wall -Wextra -g -O0 -o fibo-crude-mp fibo-crude-mp.c
