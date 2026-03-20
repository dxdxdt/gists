TESTS = \
	empty \
	whole \
	datahole \
	holedata \
	hole-sandwich \
	data-sandwich \
	data-rainbow \
	hole-rainbow
OBJ = hashole
CMD_TRACE =
ifeq ($(OS),Windows_NT)
	OBJ += lshole.exe hashole.exe
else
	OBJ += lshole

	ifneq ($(shell truss > /dev/null 2> /dev/null; echo -n $$?),127)
		CMD_TRACE = truss
	else ifneq ($(shell dtruss > /dev/null 2> /dev/null; echo -n $$?),127)
		CMD_TRACE = dtruss
	else ifneq ($(shell strace > /dev/null 2> /dev/null; echo -n $$?),127)
		CMD_TRACE = strace
	endif
endif

.PHONY: clean test_files test test-bsd-phantom-data

# The mingw executable outputs CrLf so it can be run on cmd.exe.
# But grep totally ignores Cr, so it has to be matched for the test to work
test: $(OBJ) $(TESTS)
	  ./lshole empty         | tr -d '\r' | grep -Ex  'empty:'
	  ./lshole whole         | tr -d '\r' | grep -Ex  'whole:'
	  ./lshole datahole      | tr -d '\r' | grep -Ex  'datahole: 1048576-2097152'
	  ./lshole holedata      | tr -d '\r' | grep -Ex  'holedata: 0-1048576'
	  ./lshole hole-sandwich | tr -d '\r' | grep -Ex  'hole-sandwich: 1048576-2097152'
	! ./lshole data-sandwich | tr -d '\r' | grep -Exv 'data-sandwich: 0-1048576|data-sandwich: 2097152-3145728'
	! ./lshole data-rainbow  | tr -d '\r' | grep -Exv 'data-rainbow: 1048576-2097152|data-rainbow: 3145728-4194304'
	! ./lshole hole-rainbow  | tr -d '\r' | grep -Exv 'hole-rainbow: 0-1048576|hole-rainbow: 2097152-3145728'
	! ./hashole empty
	! ./hashole whole
	! ./hashole empty whole
	  ./hashole datahole
	  ./hashole holedata
	  ./hashole hole-sandwich
	  ./hashole data-sandwich
	  ./hashole data-rainbow
	  ./hashole hole-rainbow
	  ./hashole datahole holedata hole-sandwich data-sandwich data-rainbow hole-rainbow

test-freebsd-ufs-phantom-data-at-eof: $(OBJ)
# same as data-rainbow
	truncate -s 0  phantom-data && [ -e phantom-data ]
	dd status=none conv=notrunc bs=1M if=/dev/urandom of=phantom-data count=1 seek=0
	$(CMD_TRACE) ./lshole phantom-data 2>&1 | grep -E '^(lseek|phantom-data:)'
	truncate -s 2M phantom-data
	$(CMD_TRACE) ./lshole phantom-data 2>&1 | grep -E '^(lseek|phantom-data:)'
	dd status=none conv=notrunc bs=1M if=/dev/urandom of=phantom-data count=1 seek=2
	$(CMD_TRACE) ./lshole phantom-data 2>&1 | grep -E '^(lseek|phantom-data:)'
	truncate -s 4M phantom-data
	$(CMD_TRACE) ./lshole phantom-data 2>&1 | grep -E '^(lseek|phantom-data:)'

test_files: $(TESTS)

clean:
	rm -f $(OBJ) $(TESTS)

lshole: lshole.c
	cc -std=c11 -Wall -Wextra -g -O2 -o lshole lshole.c
lshole.exe: lshole.c
	i686-w64-mingw32-gcc -std=c11 -Wall -Wextra -g -O2 -mcrtdll=crtdll -o lshole lshole.c
hashole:
	ln -s lshole hashole
hashole.exe: lshole.exe
	rm -f hashole.exe
	ln lshole.exe hashole.exe

# `fallocate -d` is required for Windows
empty:
	truncate -s 0 $@.tmp && [ -e $@.tmp ]
	mv -f $@.tmp $@
whole:
	truncate -s 0 $@.tmp && [ -e $@.tmp ]
	dd status=none conv=notrunc bs=1M if=/dev/urandom of=$@.tmp count=1 seek=0
	mv -f $@.tmp $@
datahole:
	truncate -s 0 $@.tmp && [ -e $@.tmp ]
	dd status=none conv=notrunc bs=1M if=/dev/urandom of=$@.tmp count=1 seek=0
	truncate -s 2M $@.tmp
	fallocate -d $@.tmp 2> /dev/null || true
	mv -f $@.tmp $@
holedata:
	truncate -s 0 $@.tmp && [ -e $@.tmp ]
	dd status=none conv=notrunc bs=1M if=/dev/urandom of=$@.tmp count=1 seek=1
	fallocate -d $@.tmp 2> /dev/null || true
	mv -f $@.tmp $@
# i.e. data sandwiched between holes("data burger")
hole-sandwich:
	truncate -s 0 $@.tmp && [ -e $@.tmp ]
	dd status=none conv=notrunc bs=1M if=/dev/urandom of=$@.tmp count=1 seek=0
	dd status=none conv=notrunc bs=1M if=/dev/urandom of=$@.tmp count=1 seek=2
	fallocate -d $@.tmp 2> /dev/null || true
	mv -f $@.tmp $@
# i.e. hole sandwiched between data("hole burger")
data-sandwich:
	truncate -s 0 $@.tmp && [ -e $@.tmp ]
	truncate -s 3M $@.tmp
	dd status=none conv=notrunc bs=1M if=/dev/urandom of=$@.tmp count=1 seek=1
	fallocate -d $@.tmp 2> /dev/null || true
	mv -f $@.tmp $@
# alternating layout starting with data, ending with hole
data-rainbow:
	truncate -s 0 $@.tmp && [ -e $@.tmp ]
	truncate -s 4M $@.tmp
	dd status=none conv=notrunc bs=1M if=/dev/urandom of=$@.tmp count=1 seek=0
	dd status=none conv=notrunc bs=1M if=/dev/urandom of=$@.tmp count=1 seek=2
	fallocate -d $@.tmp 2> /dev/null || true
	mv -f $@.tmp $@
# alternating layout starting with hole, ending with data
hole-rainbow:
	truncate -s 0 $@.tmp && [ -e $@.tmp ]
	dd status=none conv=notrunc bs=1M if=/dev/urandom of=$@.tmp count=1 seek=1
	dd status=none conv=notrunc bs=1M if=/dev/urandom of=$@.tmp count=1 seek=3
	fallocate -d $@.tmp 2> /dev/null || true
	mv -f $@.tmp $@
