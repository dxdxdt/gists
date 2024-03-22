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
- Linux: glibc, ulibc, musl

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

## WHY?
Not sure. Just don't mix `mmap()` with `flock()`, I guess.

```
HISTORY
       4.4BSD (the flock() call first  appeared  in  4.2BSD).   A  version  of
       flock(),  possibly  implemented  in  terms of fcntl(2), appears on most
       UNIX systems.
```
```
VERSIONS
       Since  Linux  2.0,  flock()  is implemented as a system call in its own
       right rather than being emulated in the GNU C library as a call to  fc‐
       ntl(2).   With this implementation, there is no interaction between the
       types of lock placed by flock() and fcntl(2), and flock() does not  de‐
       tect  deadlock.  (Note, however, that on some systems, such as the mod‐
       ern BSDs, flock() and fcntl(2) locks do interact with one another.)
```
