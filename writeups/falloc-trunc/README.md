> In Linux, since st.st_size < new_length, the following will fail because the
> kernel only extends without truncating the allocated blocks. All big 3
> filesystems(ext4, xfs, btrfs) behave the same way by failing this test.

This is not a bug. See https://lore.kernel.org/lkml/20171010184828.110347744@linuxfoundation.org/

## Explanation

`ftruncate()` is the only "POSIXie" way to punch holes. Increasing the file
size(size in inode, aka "isize") using the syscall is referred to as "truncating
up". Linux decided that, for POSIX compat, when the userspace uses `ftruncate()`
to punch holes or skip `write()` for efficiency, blocks(extents) allocated
through `fallocate(... KEEP_SIZE ...)` shouldn't be deallocated.

If the userspace wishes to "finalize" the file size, it should call
`ftruncate()` with the exact isize to request the deallocation of the blocks.
