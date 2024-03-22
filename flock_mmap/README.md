# Mixing flock() and close() on Various Unices
https://stackoverflow.com/questions/70045539/file-retains-lock-after-mmap-and-close

This turned out to be a bigger problem than I thought. Well, too late to fix the
kernel now.

Added `fcntl()` as control.

## INSTALL
```sh
mkdir flock_mmap
cd flock_mmap

wget \
	https://github.com/dxdxdt/gists/raw/master/flock_mmap/tests.sh \
	https://github.com/dxdxdt/gists/raw/master/flock_mmap/Makefile \
	https://github.com/dxdxdt/gists/raw/master/flock_mmap/flock_mmap.c \
	https://github.com/dxdxdt/gists/raw/master/flock_mmap/README.md

chmod 755 tests.sh
```

## Run it
```sh
make 2> /dev/null
```

### Result A
- Linux

```
./tests.sh
[TEST]fcntl(fd, F_SETLK, ...) and close() unlocks the file: YES
[TEST]fcntl(fd, F_SETLK, ...) and holding unlocks the file: NO
[TEST]flock() and close() unlocks the file*: NO
[TEST]flock() and holding unlocks the file : NO
```

### Result B
- FreeBSD
- OpenBSD
- mac

```
./tests.sh
[TEST]fcntl(fd, F_SETLK, ...) and close() unlocks the file: YES
[TEST]fcntl(fd, F_SETLK, ...) and holding unlocks the file: NO
[TEST]flock() and close() unlocks the file*: YES
[TEST]flock() and holding unlocks the file : NO
```
