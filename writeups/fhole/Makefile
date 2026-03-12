TESTS = \
	empty \
	whole \
	datahole \
	holedata \
	hole-sandwich \
	data-sandwich \
	data-rainbow \
	hole-rainbow
ifeq ($(OS),Windows_NT)
	OBJ = fhole.exe
else
	OBJ = fhole fhole.exe
endif

.PHONY: test_files test clean

# The mingw executable outputs CrLf so it can be run on cmd.exe.
# But grep totally ignores Cr, so it has to be matched for the test to work
test: $(OBJ) $(TESTS)
	./fhole empty         | grep -xP 'empty: \r?'
	./fhole whole         | grep -xP 'whole: \r?'
	./fhole datahole      | grep -xP 'datahole: 1048576-2097152 \r?'
	./fhole holedata      | grep -xP 'holedata: 0-1048576 \r?'
	./fhole hole-sandwich | grep -xP 'hole-sandwich: 1048576-2097152 \r?'
	./fhole data-sandwich | grep -xP 'data-sandwich: 0-1048576 2097152-3145728 \r?'
	./fhole data-rainbow  | grep -xP 'data-rainbow: 1048576-2097152 3145728-4194304 \r?'
	./fhole hole-rainbow  | grep -xP 'hole-rainbow: 0-1048576 2097152-3145728 \r?'

test_files: $(TESTS)

clean:
	rm -f $(OBJ) $(TESTS)

fhole: fhole.c
	cc -std=c11 -Wall -Wextra -g -O2 -o fhole fhole.c
fhole.exe: fhole.c
	i686-w64-mingw32-gcc -std=c11 -Wall -Wextra -g -O2 -o fhole fhole.c

# `fallocate -d` is required for Windows
empty:
	truncate -s 0 $@.tmp
	mv -f $@.tmp $@
whole:
	truncate -s 0 $@.tmp
	dd status=none conv=nocreat,notrunc bs=1M if=/dev/urandom of=$@.tmp count=1 seek=0
	mv -f $@.tmp $@
datahole:
	truncate -s 0 $@.tmp
	dd status=none conv=nocreat,notrunc bs=1M if=/dev/urandom of=$@.tmp count=1 seek=0
	truncate -s 2M $@.tmp
	fallocate -d $@.tmp
	mv -f $@.tmp $@
holedata:
	truncate -s 0 $@.tmp
	dd status=none conv=nocreat,notrunc bs=1M if=/dev/urandom of=$@.tmp count=1 seek=1
	fallocate -d $@.tmp
	mv -f $@.tmp $@
hole-sandwich:
	truncate -s 0 $@.tmp
	dd status=none conv=nocreat,notrunc bs=1M if=/dev/urandom of=$@.tmp count=1 seek=0
	dd status=none conv=nocreat,notrunc bs=1M if=/dev/urandom of=$@.tmp count=1 seek=2
	fallocate -d $@.tmp
	mv -f $@.tmp $@
data-sandwich:
	truncate -s 0 $@.tmp
	truncate -s 3M $@.tmp
	dd status=none conv=nocreat,notrunc bs=1M if=/dev/urandom of=$@.tmp count=1 seek=1
	fallocate -d $@.tmp
	mv -f $@.tmp $@
data-rainbow:
	truncate -s 0 $@.tmp
	truncate -s 4M $@.tmp
	dd status=none conv=nocreat,notrunc bs=1M if=/dev/urandom of=$@.tmp count=1 seek=0
	dd status=none conv=nocreat,notrunc bs=1M if=/dev/urandom of=$@.tmp count=1 seek=2
	fallocate -d $@.tmp
	mv -f $@.tmp $@
hole-rainbow:
	truncate -s 0 $@.tmp
	dd status=none conv=nocreat,notrunc bs=1M if=/dev/urandom of=$@.tmp count=1 seek=1
	dd status=none conv=nocreat,notrunc bs=1M if=/dev/urandom of=$@.tmp count=1 seek=3
	fallocate -d $@.tmp
	mv -f $@.tmp $@
