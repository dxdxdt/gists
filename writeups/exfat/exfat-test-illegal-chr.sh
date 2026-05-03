#/bin/bash

for (( i = 1; i <= 0xFFFF; i += 1 ))
do
	hex=$(printf '%04X' $i)
	c=$(printf "\\u$hex")

	tune2fs -L "$c" $1 > /dev/null
	if [ $? -ne 0 ]; then
		echo "$hex"
	fi
done
