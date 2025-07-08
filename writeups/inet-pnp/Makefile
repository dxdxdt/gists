CC ?= cc

.PHONY: test clean

test: inet-pnp
	echo '1.2.3.4' | ./inet-pnp || true
	echo '127.1' | ./inet-pnp || true
	echo '2130706433' | ./inet-pnp || true
	echo '0177.1' | ./inet-pnp || true
	echo '0x7F000001' | ./inet-pnp || true
	echo '0177.0.0.0x1' | ./inet-pnp || true
	echo '9.8.7.6 trailing rubbish' | ./inet-pnp || true
	echo 'leading-rubbish 9.8.7.6' | ./inet-pnp || true
	echo 'both 9.8.7.6 both' | ./inet-pnp || true
	echo '100. 10.1.1' | ./inet-pnp || true

inet-pnp: inet-pnp.c
	$(CC) -Wall -Wextra -std=c17 -O0 -g -o inet-pnp$(EXEC_SUFFIX) inet-pnp.c $(LD_ADD)

clean:
	rm -f inet-pnp
