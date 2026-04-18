ifeq ($(OS),Windows_NT)
	CC ?= x86_64-w64-mingw32-gcc
else
	CC ?= cc
endif

.PHONY: test clean

EXEC = lseek
TESTFILE = testfile
TESTFILE_DATA_OFS = 32767

test: $(EXEC)$(EXEC_SUFFIX)
	truncate -s 0 $(TESTFILE)
	dd if=/dev/urandom iflag=fullblock count=1 seek=$(TESTFILE_DATA_OFS) of=$(TESTFILE)
	./$(EXEC)$(EXEC_SUFFIX) /dev/null /dev/zero "$(TESTFILE)" || true

$(EXEC)$(EXEC_SUFFIX): $(EXEC).c
	$(CC) -Wall -Wextra -Wno-unused-parameter -std=c17 -O0 -g -o $(EXEC)$(EXEC_SUFFIX) $(EXEC).c $(LD_ADD)

clean:
	rm -f $(EXEC) $(TESTFILE)
