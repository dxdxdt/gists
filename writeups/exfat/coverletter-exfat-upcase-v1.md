Reroll v1:

 - Mark the volume read-only if the up-case table seems damaged
 - Inline exfat_lookup_upcase_ptable()
 - Fix uninitialised variable(ret) in exfat_load_upcase_table()

Revised coverletter:

This round of patches introduces efficient upcase table implementation
to exFAT as well as more stringent check against the upcase table when
mounting the volume.

Link: https://github.com/exfatprogs/exfatprogs/pull/341

**Theoretical trade offs**

The use of "paged upcase table" saw runtime memory footprint reduced
from 131KB to 8KB(+some slab overhead). Other trade offs include:

 - Reduced .rodata usage from 5KB to 2KB at the cost of added delay in
   module initialisation for populating the default upcase table
 - Slight performance increase from reduced cache misses when traversing
   directories with entries with a wide range of unicode code points

**Tests**

Upcase table test cases are available in my
repo(https://github.com/dxdxdt/gists/tree/master/writeups/exfat):

 - exfat-default-upcase:
   - -c option: uncompresses and prints the default compressed upcase
     table
   - -p option: tests if exfat_populate_upcase_ptable() produces the
     same default upcase table included in the kernel prior to the
     patches
   - -pc option: generates .rodata included in fs/exfat/tables.c
 - exfat-test-upcase: brute forces combinations(2^32) of unicode upcase
   conversion to ensure that no regression is introduced. The test
   finishes within 24 hours ;)
 - exfat-profile-upcase.sh and exfat-print-all-allowed: profile kernel
   memory use per mount and the worse-case directory traversal, entries
   filled with a range of unicode code points. Run with `make profile`

**Profiling**

When tested with 16 volumes(`make profile`), ~1MB saving in memory
usage per volume is observed(available mem 122528 -> 140884).

**NOTES**

Errors found in the "recommended" upcase table are outlined in
fs/exfat/tables.c.

The value of EXFAT_UPTBL_PAGESIZE(512 __u16 entries or 1024 bytes) is
determined to be the best based on the data:

```
  $ ./exfat-default-upcase > test-control
  ...
  entries: 874 (1748 bytes)
  non-empty pages: 8 (nb_page * pagesize * 2 = 512 * 8 * 2 = 8192 bytes)
```

The program reports that the total of 8 "pages" of 1KiB are used by the
table.

Regression test:

```

  (with the mainline exfat module)
  $ ./exfat-test-upcase /PATH/TO/EXFAT/MOUNTPOINT > a
  ...

  (with the patched exfat module)
  $ ./exfat-test-upcase /PATH/TO/EXFAT/MOUNTPOINT > b
  ...

  $ diff a b
```

Let me know if the test programs need to be added in the source tree
(perhaps in /tools/testing/selftests/filesystems/exfat/ ??)
