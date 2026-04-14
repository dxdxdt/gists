#!/bin/bash

declare -r ARGV0="exfat-profile-upcase.sh"
declare -r WD_PREFIX="/tmp/exfat-profile"
declare -r LOCKFILE="lock"
declare -r MKTEMP_TEMPLATE_PREFIX="$WD_PREFIX/exfat."
declare -r MKTEMP_TEMPLATE="${MKTEMP_TEMPLATE_PREFIX}XXXXXXXXXX"
declare -r NB_MOUNTS="$1"
declare -r IMAGE_SIZE="64M"

declare -a filenames
declare -a mounts

set -e

pstderr () {
	echo "$ARGV0: $1" >&2
}

die () {
	pstderr "assertion failed: $1"
	exit 1
}

cleanup_all () {
	cut -d' ' -f 2 /proc/mounts | grep "^$MKTEMP_TEMPLATE_PREFIX" |
		while read mp
	do
		umount "$mp" || die "failed to clean up mounts." \
			"You'll have to resolve this manually."
	done

	rm -rf "${MKTEMP_TEMPLATE_PREFIX}"*
}

report_vmstat () {
	vmstat -m
	vmstat -w
}

# prep the dir

mkdir -p "$WD_PREFIX"
cd "$WD_PREFIX"

# Do flock

exec {LCOKFILE_FD}> "$LOCKFILE"
if flock -w 0 "$LCOKFILE_FD"; then
	truncate -s 0 "$LOCKFILE" && echo $$ >> "$LOCKFILE"
else
	die "process $(cat "$LOCKFILE") is holding the lock"
fi

# Clean up mounts and images from the previous run if any
pstderr "cleaning up rubbish from the previous run ..."
cleanup_all

# Read unicode file names for the test. A BMP-0 character could be encoded to up
# to 3 bytes. Assert that the UTF-8 lines read are no longer than 255 bytes
# which UNIX interfaces allow.
pstderr "loading file names from stdin ..."

i=0
cnt=0
while read line
do
	let 'i += 1'
	bytelen=$(echo -n "$line" | wc -c)
	mblen=$(echo -n "$line" | wc -m)

	if [ "$mblen" -le 0 ]; then
		continue
	fi
	let 'cnt += 1'

	if [ "$bytelen" -gt 255 ]; then
		die "line $i: $line: a multi-byte(UTF-8) file name longer than 255 bytes"
	fi

	filenames+=("$line")
done

[ $cnt -le 0 ] && die "no lines read!"
pstderr "$cnt lines read"

# Generate and mount exFAT images
pstderr "generating and mounting images ..."

for (( i = 0; i < NB_MOUNTS; i += 1 ))
do
	base=$(mktemp "$MKTEMP_TEMPLATE")
	image="${base}.image"
	mp="${base}.mount"
	mkfs_exfat_out="${base}.mkfs.exfat.out"

	fallocate -l "$IMAGE_SIZE" "$image"
	mkfs.exfat "$image" > "$mkfs_exfat_out"

	mkdir "$mp"
	mount -t exfat "$image" "$mp"

	mounts+=("$base")
done

# Pre-test mem report
echo "----------- pre-run ----------"
report_vmstat

# Finally! Run the test

pstderr "running the test ..."

for mp in "${mounts[@]}"
do
	pushd "${mp}.mount" > /dev/null
	for f in "${filenames[@]}"
	do
		touch "$f"
	done
	popd > /dev/null
done

pstderr "done!"

# Report
echo "---------- post-run ----------"
report_vmstat
echo "------------------------------"

# Clean up
pstderr "cleaning up ..."
cleanup_all
