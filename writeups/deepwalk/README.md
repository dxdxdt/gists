# Deepwalk
**tl;dr:** it's in your best interest to keep path strings under 4096
bytes(`PATH_MAX`)

## nftw() in glibc is hopeless broken

```
$ ./dig.sh
...

$ ./deepwalk nftw
...
...
Fatal glibc error: ../sysdeps/wordsize-64/../../io/ftw.c:540 (ftw_dir): assertion failed: startp != data->dirbuf
Aborted                    (core dumped) ./deepwalk nftw
```

### "Good Overflow hunting"

```
(gdb) bt
#0  __pthread_kill_implementation (threadid=<optimized out>, signo=signo@entry=6, no_tid=no_tid@entry=0)
    at pthread_kill.c:44
#1  0x00007ffff7e0f493 in __pthread_kill_internal (threadid=<optimized out>, signo=6) at pthread_kill.c:89
#2  0x00007ffff7db518e in __GI_raise (sig=sig@entry=6) at ../sysdeps/posix/raise.c:26
#3  0x00007ffff7d9c6d0 in __GI_abort () at abort.c:77
#4  0x00007ffff7d9d73b in __libc_message_impl (
    fmt=fmt@entry=0x7ffff7f51f90 "Fatal glibc error: %s:%s (%s): assertion failed: %s\n")
    at ../sysdeps/posix/libc_fatal.c:138
#5  0x00007ffff7dacd07 in __libc_assert_fail (assertion=assertion@entry=0x7ffff7f4ed2d "startp != data->dirbuf",
    file=file@entry=0x7ffff7f54be0 "../sysdeps/wordsize-64/../../io/ftw.c", line=line@entry=540,
    function=function@entry=0x7ffff7f54c08 <__PRETTY_FUNCTION__.0> "ftw_dir") at __libc_assert_fail.c:31
#6  0x00007ffff7e81b8e in ftw_dir (data=data@entry=0x7fffffffcf10, st=st@entry=0x7ffffff92650,
    old_dir=old_dir@entry=0x7ffffff92760) at ../sysdeps/wordsize-64/../../io/ftw.c:540
#7  0x00007ffff7e81491 in process_entry (data=data@entry=0x7fffffffcf10, dir=dir@entry=0x7ffffff92760,
    name=name@entry=0x4359c3 "443", namlen=<optimized out>, d_type=<optimized out>)
    at ../sysdeps/wordsize-64/../../io/ftw.c:469
#8  0x00007ffff7e8171b in ftw_dir (data=data@entry=0x7fffffffcf10, st=st@entry=0x7ffffff927e0,
    old_dir=old_dir@entry=0x7ffffff928f0) at ../sysdeps/wordsize-64/../../io/ftw.c:551
#9  0x00007ffff7e81491 in process_entry (data=data@entry=0x7fffffffcf10, dir=dir@entry=0x7ffffff928f0,
    name=name@entry=0x42d983 "442", namlen=<optimized out>, d_type=<optimized out>)
    at ../sysdeps/wordsize-64/../../io/ftw.c:469
#10 0x00007ffff7e8171b in ftw_dir (data=data@entry=0x7fffffffcf10, st=st@entry=0x7ffffff92970, 
    old_dir=old_dir@entry=0x7ffffff92a80) at ../sysdeps/wordsize-64/../../io/ftw.c:551
#11 0x00007ffff7e81491 in process_entry (data=data@entry=0x7fffffffcf10, dir=dir@entry=0x7ffffff92a80, 
    name=name@entry=0x42592b "441", namlen=<optimized out>, d_type=<optimized out>)
    at ../sysdeps/wordsize-64/../../io/ftw.c:469
...
#2186 0x00007ffff7e8171b in ftw_dir (data=data@entry=0x7fffffffcf10, st=st@entry=0x7fffffffcd70,
    old_dir=old_dir@entry=0x7fffffffce80) at ../sysdeps/wordsize-64/../../io/ftw.c:551
#2187 0x00007ffff7e81491 in process_entry (data=data@entry=0x7fffffffcf10, dir=dir@entry=0x7fffffffce80,
    name=0x49dce8 "1", namlen=<optimized out>, d_type=0) at ../sysdeps/wordsize-64/../../io/ftw.c:469
#2188 0x00007ffff7e81756 in ftw_dir (data=data@entry=0x7fffffffcf10, st=st@entry=0x7fffffffcf70, 
    old_dir=old_dir@entry=0x0) at ../sysdeps/wordsize-64/../../io/ftw.c:582
#2189 0x00007ffff7e8201d in ftw_startup (dir=<optimized out>, is_nftw=<optimized out>, func=<optimized out>,
    descriptors=<optimized out>, flags=21) at ../sysdeps/wordsize-64/../../io/ftw.c:771
#2190 0x0000000000400919 in walkf_nftw () at deepwalk.c:202
#2191 0x0000000000400c60 in main (argc=2, argv=0x7fffffffd1b8) at deepwalk.c:303

(gdb) info proc mapping
process 427727
Mapped address spaces:

Start Addr         End Addr           Size               Offset             Perms File
...
0x00007ffffff92000 0x00007ffffffff000 0x6d000            0x0                rw-p  [stack]
...
(gdb) info thr
  Id   Target Id                                     Frame
* 1    Thread 0x7ffff7d98740 (LWP 427727) "deepwalk" __pthread_kill_implementation (threadid=<optimized out>,
    signo=signo@entry=6, no_tid=no_tid@entry=0) at pthread_kill.c:44
```

```
  st=st@entry=0x7ffffff92650
dir=dir@entry=0x7ffffff92760
  st=st@entry=0x7ffffff927e0
dir=dir@entry=0x7ffffff928f0
```

Turns out, it was just a good'ol stack overflow. Well, I should've checked the
stack pointer and the memory map before deciding to read the code line by line.
My instinct was that there was some sort of logical error in the walk algo.

There's recursive code path in `ftw.c`. When processing directory entries and
callbacks in `process_entry()`, `ftw_dir()` is called for each directory in turn
calls `ftw_dir()`, completing the whole circle.


### Musl's nftw() is also hopelessly broken
Link the POC against Musl and try running with the `FTW_CHDIR` code path. You'll
see that it can't even go deeper than the first level:

```
$ make deepwalk-musl
$ ./dig.sh
$ ./deepwalk-musl nftw
.
./AQ
deepwalk: ./AQ/Ag: No such file or directory
deepwalk: ./AQ/Ag/Aw: No such file or directory
deepwalk: ./AQ/Ag/Aw/BA: No such file or directory
deepwalk: ./AQ/Ag/Aw/BA/BQ: No such file or directory
deepwalk: ./AQ/Ag/Aw/BA/BQ/Bg: No such file or directory
deepwalk: ./AQ/Ag/Aw/BA/BQ/Bg/Bw: No such file or directory
deepwalk: ./AQ/Ag/Aw/BA/BQ/Bg/Bw/CA: No such file or directory
...
```

At least in Musl's case, the API is just not functional. It doesn't die with
`SIGABRT` like glibc's `nftw()` does.

Lightweight Linux distro devs have been using the BSD style fts for a while now.
Musl devs make it clear that they won't provide fts. Then why the hell don't
they even provide a functional `nftw()`? Nobody knows...

### Remove nftw(): just wrap it in fts

 - https://sourceware.org/bugzilla/show_bug.cgi?id=33085
 - https://sourceware.org/bugzilla/show_bug.cgi?id=28831

At this point, there's no point in maintaining two separate versions of
directory walk API. `nftw()` and fts are basically the same API with only
slightly different semantics.

https://github.com/lattera/openbsd/blob/master/lib/libc/gen/nftw.c

BSD's have already been doing this. I know it's really not a good excuse, but it
does seem to be the reasonable course of action at this point. `nftw()` is not
well thought out - that's on POSIX people, not Linux and BSD folks. That's why
we invented fts in the first place.

## The hardcoded kernel limit
The ultimate limit is the size of `environ`. Both Linux and BSD have their own
hardcoded limit in the kernel as a security measure.

### E2BIG
```
[pid 406187] execve("/usr/bin/mkdir", ["mkdir", "-p", "00000000000000000000000000000000"...], 0x55611623b700 /* 104 vars */ <unfinished ...>
...
[pid 406187] <... execve resumed>)      = -1 E2BIG (Argument list too long)
```

Where does that come from?

fs/exec.c:

```c
/*
 * 'copy_strings()' copies argument/environment strings from the old
 * processes's memory to the new process's stack.  The call to get_user_pages()
 * ensures the destination page is created and not swapped out.
 */
static int copy_strings(int argc, struct user_arg_ptr argv,
			struct linux_binprm *bprm)
{
	struct page *kmapped_page = NULL;
	char *kaddr = NULL;
	unsigned long kpos = 0;
	int ret;

	while (argc-- > 0) {
		const char __user *str;
		int len;
		unsigned long pos;

		ret = -EFAULT;
		str = get_user_arg_ptr(argv, argc);
		if (IS_ERR(str))
			goto out;

		len = strnlen_user(str, MAX_ARG_STRLEN);
		if (!len)
			goto out;

		ret = -E2BIG;
		if (!valid_arg_len(bprm, len))
			goto out;
```

include/uapi/linux/binfmts.h:

```c
/*
 * These are the maximum length and maximum number of strings passed to the
 * execve() system call.  MAX_ARG_STRLEN is essentially random but serves to
 * prevent the kernel from being unduly impacted by misaddressed pointers.
 * MAX_ARG_STRINGS is chosen to fit in a signed 32-bit integer.
 */
#define MAX_ARG_STRLEN (PAGE_SIZE * 32)
#define MAX_ARG_STRINGS 0x7FFFFFFF
```

`PAGE_SIZE * 32 = 4096 * 32 = 131072`

So the limit is somewhere around 131K depending on the other env vars passed to
`execve()` call. Note that this is much bigger on systems running on larger page
sizes(`CONFIG_PAGE_SIZE_*KB`). Also, some niche archs like UltraSPARC has 8K
page size, which makes the limit twice that number.

If we run the process without `PWD`, may be we possible to go even deeper?

No, you don't wanna do that. Many of libc functions probably references
PWD(`system()` for example). In other words: no practical application

## getcwd() and the role of PWD
According to the POSIX specs, there are two types of PWD: logical and physical.
The PWD env var keeps the track of the "logical" path. This the cost of having
symlinks in the system design. The logical path is only a matter of setting PWD
to the right value.

However, in the case of physical paths, `getcwd()` - the Linux kernel provides a
syscall version of it. Linux got you covered in one single syscall ... until
`PATH_MAX`, which is usually 4096 on typical 4KB page systems. This macro is
often misinterpreted as the hard system/API limit while in fact it's meant to be
the max length of path string parameter ie. function call limit.

If the syscall can't handle PWD resolution, the userland(libc) has to fall back
to the good old traversal to the root. Not very efficient!

sysdeps/posix/getcwd.c:376

```c
            /* Compute size needed for this file name, or for the file
               name ".." in the same directory, whichever is larger.
               Room for ".." might be needed the next time through
               the outer loop.  */
            size_t name_alloc = _D_ALLOC_NAMLEN (d);
            size_t filesize = dotlen + MAX (sizeof "..", name_alloc);

            if (filesize < dotlen)
              goto memory_exhausted;

            if (dotsize < filesize)
              {
                /* My, what a deep directory tree you have, Grandma.  */
                size_t newsize = MAX (filesize, dotsize * 2);
                size_t i;
                if (newsize < dotsize)
                  goto memory_exhausted;
                if (dotlist != dots)
                  free (dotlist);
                dotlist = malloc (newsize);
```

> My, what a deep directory tree you have, Grandma.

That would be a reference to *Little Red Riding Hood*.

It's safe to say that `getcwd()` scales to infinity as long as the memory
allows if you don't mind it being O(N).
