CC11_D = cc -std=c11 -Wall -Wextra -Wno-unused-parameter -g -O0
CC11_R = cc -std=c11 -Wall -Wextra -Wno-unused-parameter -g -O2
CC11MINGW_D = i686-w64-mingw32-gcc -std=c11 -Wall -Wextra -Wno-unused-parameter -g -O0
CC11MINGW_R = i686-w64-mingw32-gcc -std=c11 -Wall -Wextra -Wno-unused-parameter -g -O2

OBJ = 	ntree128-memcmp \
	ntree128-chaincmp \
	ntree128h \
	ntree128a

OBJ_EXE = ntree128-memcmp.exe ntree128-chaincmp.exe ntree128h.exe ntree128a.exe

ifeq ($(OS),Windows_NT)
OBJ := $(OBJ_EXE)
endif

.PHONY: ntree128 run clean

run: ntree128
	xz -dkc ntree128.testdata.xz | cut -d ' ' -f2 | ./ntree128a > /dev/null
	xz -dkc ntree128.testdata.xz | ./ntree128-memcmp            > /dev/null
	xz -dkc ntree128.testdata.xz | ./ntree128-chaincmp          > /dev/null
	xz -dkc ntree128.testdata.xz | ./ntree128h                  > /dev/null

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

ntree128-memcmp.exe: ntree128.c bsd.h
	$(CC11MINGW_R) -DNTREE_MEMCMP ntree128.c -o ntree128-memcmp
ntree128-chaincmp.exe: ntree128.c bsd.h
	$(CC11MINGW_R) -DNTREE_CHAINCMP ntree128.c -o ntree128-chaincmp
ntree128h.exe: ntree128h.c bsd.h
	$(CC11MINGW_R) ntree128h.c -o ntree128h
ntree128a.exe: ntree128a.c bsd.h
	$(CC11MINGW_R) ntree128a.c -o ntree128a
