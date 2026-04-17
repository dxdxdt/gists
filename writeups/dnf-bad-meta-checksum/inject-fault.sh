#!/bin/bash
set -e

for f in "$@"
do
	# calc the target offset to fault
	filesize=$(stat -c '%s' "$f")
	rnd="$RANDOM$RANDOM"
	let 'ofs = rnd % filesize'
	printf "$f: injecting fault at offset 0x%08X\n" $ofs >&2

	# read from the target offset
	org=$(dd bs=1 count=1 skip=$ofs iflag=fullblock if="$f" | xxd -ps)

	# generate a random byte that's different from the original byte
	while true
	do
		v=$(dd bs=1 count=1 iflag=fullblock if=/dev/urandom | xxd -ps)
		if [ "$v" != "$org" ]
		then
			break
		fi
	done

	# inject
	echo "$v" | xxd -ps -r | dd bs=1 count=1 of="$f" seek=$ofs conv=notrunc,nocreat
done
