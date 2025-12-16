#!/bin/sh
set -e

dig_base64 () {
	maxdepth="$1"
	for (( i = 1; i < maxdepth; i += 1 ))
	do
		local dname=$(printf '%02x' "$i" | xxd -ps -r | base64 -w 0)
		dname=${dname//\//_}
		dname=${dname//=/}

		mkdir -p "$dname"
		cd "$dname"
	done

	touch gem

	pwd
	pwd | wc -c
}

dig_hex () {
	maxdepth="$1"
	for (( i = 1; i < maxdepth; i += 1 ))
	do
		local dname=$(printf '%x' "$i")

		mkdir -p "$dname"
		cd "$dname"
	done

	touch gem

	pwd
	pwd | wc -c
}

pushd .
dig_base64 1000
popd

pushd .
dig_hex 1500
popd

du -hs AQ 1
