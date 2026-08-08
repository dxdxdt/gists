CC ?= cc
CC11_D = $(CC) -std=c11 -Wall -Wextra -Wno-unused-parameter -g -O0
CC11_R = $(CC) -std=c11 -Wall -Wextra -Wno-unused-parameter -g -O2
CC11MINGW_D = i686-w64-mingw32-gcc -std=c11 -Wall -Wextra -Wno-unused-parameter -g -O0
CC11MINGW_R = i686-w64-mingw32-gcc -std=c11 -Wall -Wextra -Wno-unused-parameter -g -O2

OBJ = 	ntree128-memcmp \
	ntree128-chaincmp \
	ntree128h \
	ntree128a \
	genrand

OBJ_EXE = ntree128-memcmp.exe ntree128-chaincmp.exe ntree128h.exe ntree128a.exe genrand.exe

ifeq ($(OS),Windows_NT)
OBJ := $(OBJ_EXE)
endif

.PHONY: ntree128 run clean

run: ntree128
	xz -dkc ntree128.testdata.xz | cut -d ' ' -f2 | ./ntree128a > /dev/null
	xz -dkc ntree128.testdata.xz | ./ntree128-memcmp            > /dev/null
	xz -dkc ntree128.testdata.xz | ./ntree128-chaincmp          > /dev/null
	xz -dkc ntree128.testdata.xz | ./ntree128h                  > /dev/null

deep_tree_test: ntree128a genrand
# 2GB of memory required on a 64-bit machine
	./genrand -n 134217728 | /bin/time -v ./ntree128a > /dev/null
stack_spike_test: ntree128a
	STACK_SPIKE_TEST=100000 ./ntree128a < /dev/null > /dev/null

ntree128: $(OBJ)

clean:
	rm -f $(OBJ)

ntree128-memcmp: ntree128.c bsd.h
	$(CC11_R) -DNTREE_MEMCMP ntree128.c -o ntree128-memcmp
ntree128-chaincmp: ntree128.c bsd.h
	$(CC11_R) -DNTREE_CHAINCMP ntree128.c -o ntree128-chaincmp
ntree128h: ntree128h.c bsd.h
	$(CC11_R) ntree128h.c -o ntree128h
ntree128a: ntree128a.c bsd.h
	$(CC11_R) ntree128a.c -o ntree128a
genrand: genrand.c
	$(CC11_R) genrand.c -o genrand

ntree128-memcmp.exe: ntree128.c bsd.h
	$(CC11MINGW_R) -DNTREE_MEMCMP ntree128.c -o ntree128-memcmp
ntree128-chaincmp.exe: ntree128.c bsd.h
	$(CC11MINGW_R) -DNTREE_CHAINCMP ntree128.c -o ntree128-chaincmp
ntree128h.exe: ntree128h.c bsd.h
	$(CC11MINGW_R) ntree128h.c -o ntree128h
ntree128a.exe: ntree128a.c bsd.h
	$(CC11MINGW_R) ntree128a.c -o ntree128a
genrand.exe: genrand.c
	$(CC11MINGW_R) genrand.c -o genrand
