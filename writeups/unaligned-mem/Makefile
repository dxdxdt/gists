CC ?= cc
CC_FLAGS = -g -O0 -Wall -Wextra
CC_CMD = $(CC) $(CC_FLAGS) $(CC_EXTRA_CLAGS)
OBJ = \
	unamem \
	unamem-armv4t \
	unamem-armv7 \
	unamem-aarch64 \
	unamem-arc \
	unamem-arceb \
	unamem-i686 \
	unamem-x86_64 \
	unamem-m68k \
	unamem-mips \
	unamem-mpsl \
	unamem-ppc \
	unamem-sh4

.PHONY: clean test

all: $(OBJ) test

clean:
	rm -f $(OBJ)


unamem: unamem.c
	$(CC_CMD) -o unamem unamem.c
unamem-armv4t: unamem.c
	xcomp linux-app armv4t arm-linux-cc $(CC_FLAGS) $(CC_EXTRA_CLAGS) -static -o unamem-armv4t unamem.c
unamem-armv7: unamem.c
	xcomp linux-app armv7 arm-linux-cc $(CC_FLAGS) $(CC_EXTRA_CLAGS) -static -o unamem-armv7 unamem.c
unamem-aarch64: unamem.c
	xcomp linux-app aarch64 aarch64-linux-cc $(CC_FLAGS) $(CC_EXTRA_CLAGS) -static -o unamem-aarch64 unamem.c
unamem-arc: unamem.c
	xcomp linux-app arc arc-linux-cc $(CC_FLAGS) $(CC_EXTRA_CLAGS) -static -o unamem-arc unamem.c
unamem-arceb: unamem.c
	xcomp linux-app arceb arceb-linux-cc $(CC_FLAGS) $(CC_EXTRA_CLAGS) -static -o unamem-arceb unamem.c
unamem-i686: unamem.c
	xcomp linux-app i686 i686-linux-cc $(CC_FLAGS) $(CC_EXTRA_CLAGS) -static -o unamem-i686 unamem.c
unamem-x86_64: unamem.c
	xcomp linux-app x86_64 x86_64-linux-cc $(CC_FLAGS) $(CC_EXTRA_CLAGS) -static -o unamem-x86_64 unamem.c
unamem-m68k: unamem.c
	xcomp linux-app m68k m68k-linux-cc $(CC_FLAGS) $(CC_EXTRA_CLAGS) -static -o unamem-m68k unamem.c
unamem-mips: unamem.c
	xcomp linux-app mips mips-linux-cc $(CC_FLAGS) $(CC_EXTRA_CLAGS) -static -o unamem-mips unamem.c
unamem-mpsl: unamem.c
	xcomp linux-app mpsl mipsel-linux-cc $(CC_FLAGS) $(CC_EXTRA_CLAGS) -static -o unamem-mpsl unamem.c
unamem-ppc: unamem.c
	xcomp linux-app ppc powerpc-linux-cc $(CC_FLAGS) $(CC_EXTRA_CLAGS) -static -o unamem-ppc unamem.c
unamem-sh4: unamem.c
	xcomp linux-app sh4 sh4-linux-cc $(CC_FLAGS) $(CC_EXTRA_CLAGS) -static -o unamem-sh4 unamem.c

test: run-all $(OBJ)
	./run-all
