.PHONY: benchmark

benchmark: iterative-div
	./iterative-div -t5    < /dev/urandom > /dev/null || echo $$?
	./iterative-div -t5 -c < /dev/urandom > /dev/null || echo $$?
	./iterative-div -t5    < /dev/zero >    /dev/null || echo $$?
	./iterative-div -t5 -c < /dev/zero >    /dev/null || echo $$?

iterative-div: iterative-div.c libdivide.h
	cc -mtune=native -march=native -g -O2 -Wall -Wextra \
		-o iterative-div iterative-div.c

libdivide.h:
	curl -sSLO 'https://raw.githubusercontent.com/ridiculousfish/libdivide/master/libdivide.h'

clean:
	rm -f iterative-div libdivide.h
