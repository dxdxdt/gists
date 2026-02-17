#!/bin/sh
cp -f /dev/null test

fallocate --keep-size -l 1M test
echo $(du -h test) $(stat -c '%s' test)

echo "hello, world!" | dd conv=notrunc,nocreat of=test
echo $(du -h test) $(stat -c '%s' test)

truncate -s 512 test
hexdump -C test
echo $(du -h test) $(stat -c '%s' test)

truncate -s 5 test
echo $(du -h test) $(stat -c '%s' test)
hexdump -C test
echo
