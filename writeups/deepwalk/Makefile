.PHONY: all

BINS = deepwalk deepwalk-musl posix-nftw_example

all: $(BINS)
clean:
	rm -f $(BINS)

posix-nftw_example: posix-nftw_example.c
	cc -Wall -Wextra -g -o $@ posix-nftw_example.c

deepwalk: deepwalk.c
	cc -Wall -Wextra -DHAS_FTS -D_GNU_SOURCE -g -o $@ deepwalk.c

deepwalk-musl: deepwalk.c
	musl-gcc -Wall -Wextra -g -o $@ deepwalk.c
