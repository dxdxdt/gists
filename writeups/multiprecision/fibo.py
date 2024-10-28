#!/bin/env python3
a = 0
b = 1

for i in range(101):
	c = a + b
	print(c)
	b = a
	a = c
