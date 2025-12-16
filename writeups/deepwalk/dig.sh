#!/bin/sh
set -e

if base64 - < /dev/null > /dev/null 2> /dev/null; then
	b64exec="base64 -w0"
else
	b64exec="openssl base64 -A"
fi

dig_base64 () {
	maxdepth="$1"
	for i in $(seq $maxdepth)
	do
		local dname=$(
				printf '%02x' "$i" |
				xxd -ps -r |
				$b64exec |
				sed -E 's/[=\/]//g')

		mkdir -p "$dname"
		cd "$dname"
	done

	touch gem

	pwd
	pwd | wc -c
}

dig_hex () {
	maxdepth="$1"
	for i in $(seq $maxdepth)
	do
		local dname=$(printf '%x' "$i")

		mkdir -p "$dname"
		cd "$dname"
	done

	touch gem

	pwd
	pwd | wc -c
}

dig_base64 1000 & wait || exit $?
dig_hex 1500 & wait || exit $?

du -hs AQ 1
