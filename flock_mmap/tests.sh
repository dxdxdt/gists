#!/bin/sh
LOCK_F="lock"

setyes () {
	echo "[TEST]$TEST_NAME: YES"
}

setno () {
	echo "[TEST]$TEST_NAME: NO"
}

cleanup () {
	kill -TERM $!
}

TEST_NAME="fcntl(fd, F_SETLK, ...) and close() unlocks the file"
./flock_mmap -fw "$LOCK_F" & sleep 1
if ./flock_mmap -f "$LOCK_F"
then
	setyes
else
	setno
fi
cleanup

TEST_NAME="fcntl(fd, F_SETLK, ...) and holding unlocks the file"
./flock_mmap -fwn "$LOCK_F" & sleep 1
if ./flock_mmap -f "$LOCK_F"
then
	setyes
else
	setno
fi
cleanup

TEST_NAME="flock() and close() unlocks the file*"
./flock_mmap -w "$LOCK_F" & sleep 1
if ./flock_mmap "$LOCK_F"
then
	setyes
else
	setno
fi
cleanup

TEST_NAME="flock() and holding unlocks the file "
./flock_mmap -wn "$LOCK_F" & sleep 1
if ./flock_mmap "$LOCK_F"
then
	setyes
else
	setno
fi
cleanup
