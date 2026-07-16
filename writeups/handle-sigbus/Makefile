EXEC = handle-sigbus
TMPFILE = file.tmp
BLKCNT= 2
LOOPNAME = loop42
DUSTNAME = dust42

.PHONY: test-truncate test-dust packup-dust

$(EXEC)$(EXEC_SUFFIX): $(EXEC).c
	$(CC) -Wall -Wextra -std=c11 -O0 -g -o $(EXEC)$(EXEC_SUFFIX) $(EXEC).c $(LD_ADD)

$(TMPFILE):
	dd if=/dev/urandom of=$(TMPFILE).tmp bs=$$(getconf PAGESIZE) count=$(BLKCNT)
	mv $(TMPFILE).tmp $(TMPFILE)

test-truncate: $(EXEC)$(EXEC_SUFFIX) $(TMPFILE)
	./$(EXEC)$(EXEC_SUFFIX) -t $(TMPFILE) || true

test-dust: packup-dust $(TMPFILE) $(EXEC)$(EXEC_SUFFIX)
	losetup /dev/$(LOOPNAME) $(TMPFILE)
	blockdev --report /dev/$(LOOPNAME)
	dmsetup create $(DUSTNAME) --table \
		"0 $$(blockdev --getsz /dev/$(LOOPNAME)) dust /dev/$(LOOPNAME) 0 512"
	dmsetup message $(DUSTNAME) \
		0 addbadblock $$(($$(blockdev --getsz /dev/$(LOOPNAME)) - 1))
	dmsetup message $(DUSTNAME) 0 enable
	dmsetup status $(DUSTNAME)
	./$(EXEC)$(EXEC_SUFFIX) /dev/mapper/$(DUSTNAME) || true

packup-dust:
	dmsetup remove $(DUSTNAME) 2> /dev/null || true
	losetup -d /dev/$(LOOPNAME) 2> /dev/null || true

clean: packup-dust
	rm -f $(EXEC) *.tmp
