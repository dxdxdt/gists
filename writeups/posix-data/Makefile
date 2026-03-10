CC11_D = gcc -std=c11 -Wall -Wextra -Wno-unused-parameter -g -O0
CC11_R = gcc -std=c11 -Wall -Wextra -Wno-unused-parameter -g -O2
CC11MINGW_D = i686-w64-mingw32-gcc -std=c11 -Wall -Wextra -Wno-unused-parameter -g -O0
CC11MINGW_R = i686-w64-mingw32-gcc -std=c11 -Wall -Wextra -Wno-unused-parameter -g -O2

OBJ = 	ntree128-memcmp \
	ntree128-chaincmp \
	ntree128-avx512vl \
	ntree128-memcmp.exe \
	ntree128-chaincmp.exe \
	ntree128-avx512vl.exe \

.PHONY: ntree128 run clean

run: ntree128
	xz -dkc ntree128.testdata.xz | ./ntree128-memcmp > /dev/null
	xz -dkc ntree128.testdata.xz | ./ntree128-chaincmp > /dev/null
	xz -dkc ntree128.testdata.xz | ./ntree128-avx512vl > /dev/null

ntree128: $(OBJ)

clean:
	rm -f $(OBJ)

ntree128-memcmp: ntree128.c bsd.h
	$(CC11_R) -DNTREE_MEMCMP ntree128.c -o ntree128-memcmp
ntree128-chaincmp: ntree128.c bsd.h
	$(CC11_R) -DNTREE_CHAINCMP ntree128.c -o ntree128-chaincmp
ntree128-avx512vl: ntree128.c bsd.h
	$(CC11_R) -mavx512vl -DNTREE_AVX512VL ntree128.c -o ntree128-avx512vl
ntree128-memcmp.exe: ntree128.c bsd.h
	$(CC11MINGW_R) -DNTREE_MEMCMP ntree128.c -o ntree128-memcmp
ntree128-chaincmp.exe: ntree128.c bsd.h
	$(CC11MINGW_R) -DNTREE_CHAINCMP ntree128.c -o ntree128-chaincmp
ntree128-avx512vl.exe: ntree128.c bsd.h
	$(CC11MINGW_R) -mavx512vl -DNTREE_AVX512VL ntree128.c -o ntree128-avx512vl
