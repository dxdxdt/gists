CC ?= cc

.PHONY: clean test

test: eyeball$(EXEC_SUFFIX)
	./eyeball$(EXEC_SUFFIX) github.com 443 || true
	./eyeball$(EXEC_SUFFIX) example.com 443 || true
	./eyeball$(EXEC_SUFFIX) google.com 443 || true
	./eyeball$(EXEC_SUFFIX) ipv6.google.com 443 || true
	./eyeball$(EXEC_SUFFIX) v6.ipv6-test.com 443 || true
	./eyeball$(EXEC_SUFFIX) 8.8.8.8 53 || true
	./eyeball$(EXEC_SUFFIX) 1.1.1.1 53 || true
	./eyeball$(EXEC_SUFFIX) 2001:4860:4860::8888 53 || true
	./eyeball$(EXEC_SUFFIX) 2606:4700:4700::1111 53 || true
	./eyeball$(EXEC_SUFFIX) nxdomain.test 80 || true

eyeball$(EXEC_SUFFIX): eyeball.c
	$(CC) -std=c17 -g -O0 -Wall -Wextra -Wno-unused-parameter -o eyeball$(EXEC_SUFFIX) eyeball.c $(LD_ADD)

clean:
	rm -f eyeball
