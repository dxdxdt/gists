#!/bin/sh
# Testing FreeBSD's limit.
# Linux should be able to create the tree, but wouldn't be able handle it with
# any of its tree traversal APIs because:
#
#  - glibc's nftw implementation involves recursion which results in stack
#    overflow
#  - musl's nftw() w/ FTW_CHDIR is hopelessly broken
#  - fts_namelen and fts_pathlen is short(signed 16-bit integer): glibc gives up
#    with ENAMETOOLONG
#
# Will FreeBSD handle this? Stay tuned to find out!

set -e

dig_paddedhex () {
        maxdepth="$1"
        for i in $(seq $maxdepth)
        do
                local dname=$(printf '%0255d' "$i")

                mkdir -p "$dname"
                cd "$dname"
                echo $i
        done

        touch gem

        pwd | wc -c
}

# The limit seems to be 512 levels(fails w/ E2BIG).
# Seems to be the kernel-bound limit, rather than userspace API limit.
dig_paddedhex 1000 & wait
# pwd: ~ 256000 characters

du -hs 000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001
