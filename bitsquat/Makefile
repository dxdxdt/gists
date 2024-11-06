CC ?= cc
CC_OPTS ?= -Wall -Wextra -g
CC_OPTS_BT ?= -O0

CC_CMD = $(CC) $(CC_OPTS) $(CC_OPTS_BT) $(CC_OPTS_EXTRA)

all: bitsquat

clean:
	rm -f bitsquat

bitsquat: bitsquat.c
	$(CC_CMD) -o bitsquat bitsquat.c
