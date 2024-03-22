CC ?= cc
OBJ = flock_mmap lock

do: build test

clean:
	rm -f $(OBJ)

flock_mmap: flock_mmap.c
	$(CC) -std=c11 -Wall -Wextra -g -O0 -o flock_mmap flock_mmap.c

build: flock_mmap

test: build
	./tests.sh
