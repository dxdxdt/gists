ifeq ($(OS),Windows_NT)
	CC ?= x86_64-w64-mingw32-gcc
	EXEC_SUFFIX ?= .exe
else
	CC ?= cc
endif

.PHONY: test clean

EXEC = zm
SIZE_HUGE = 1099511627776 # 1TB
SIZE_REASONABLE = 1073741824 # 1GB

test: $(EXEC)$(EXEC_SUFFIX)
	time ./$(EXEC)$(EXEC_SUFFIX) -c -s $(SIZE_REASONABLE) || true
	time ./$(EXEC)$(EXEC_SUFFIX) -c -s $(SIZE_HUGE)       || true
	time ./$(EXEC)$(EXEC_SUFFIX) -m -s $(SIZE_REASONABLE) || true
	time ./$(EXEC)$(EXEC_SUFFIX) -m -s $(SIZE_HUGE)       || true

$(EXEC)$(EXEC_SUFFIX): $(EXEC).c
	$(CC) -Wall -Wextra -std=c17 -O0 -g -o $(EXEC)$(EXEC_SUFFIX) $(EXEC).c $(LD_ADD)

clean:
	rm -f $(EXEC)
