#!/bin/env python3
nums = [ i for i in range(1, 46) ]

while True:
	try: line = input()
	except EOFError: break

	a = int(line)
	b = a % len(nums)
	c = nums.pop(b)

	print(c)
