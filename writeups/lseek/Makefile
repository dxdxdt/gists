ifeq ($(OS),Windows_NT)
	CC ?= x86_64-w64-mingw32-gcc
else
	CC ?= cc
endif

.PHONY: test clean

EXEC = lseek
TESTFILE_PREFIX = testfile-
TESTFILE_DATA_OFS = 32767
TESTFILE_DATA_SIZE = 16776704

test: $(EXEC)$(EXEC_SUFFIX)
	truncate -s $(TESTFILE_DATA_SIZE) $(TESTFILE_PREFIX)0 || true
	dd if=/dev/urandom count=1 seek=$(TESTFILE_DATA_OFS) of=$(TESTFILE_PREFIX)0
	dd if=/dev/urandom count=1 seek=$(TESTFILE_DATA_OFS) of=$(TESTFILE_PREFIX)1
	./$(EXEC)$(EXEC_SUFFIX) /dev/null /dev/zero || true
	./$(EXEC)$(EXEC_SUFFIX) "$(TESTFILE_PREFIX)0" "$(TESTFILE_PREFIX)1" || true

$(EXEC)$(EXEC_SUFFIX): $(EXEC).c
	$(CC) -Wall -Wextra -Wno-unused-parameter -std=c17 -O0 -g -o $(EXEC)$(EXEC_SUFFIX) $(EXEC).c $(LD_ADD)

clean:
	rm -f $(EXEC) $(TESTFILE_PREFIX)*
