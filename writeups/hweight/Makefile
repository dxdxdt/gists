TEST_N=65535

test: hweight
	for (( i = 1; i <= $(TEST_N); i++ )); do echo "$$i"; done | ./hweight -ti > result-i
	for (( i = 1; i <= $(TEST_N); i++ )); do echo "$$i"; done | ./hweight -tl > result-l
	for (( i = 1; i <= $(TEST_N); i++ )); do echo "$$i"; done | ./hweight -tc > result-c
	diff -q result-i result-l
	diff -q result-l result-c

clean:
	rm -f hweight result-*

hweight: hweight.c
	gcc -o hweight hweight.c -Wall -Wextra -O0 -g -fno-inline
