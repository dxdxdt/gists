#!/bin/env node

function do_fibo (num_f) {
	let a, b, c;

	a = num_f('0');
	b = num_f('1');

	for (let i = 0; i < 101; i += 1) {
		c = a + b;
		console.info(c.toString());
		b = a;
		a = c;
	}
}

function parse_flag (s) {
	s = s.toLowerCase();

	if (s === 'true') {
		return true;
	}
	else if (s === 'false') {
		return false;
	}

	return Number(s);
}

const flag = process.argv.length > 2 ? parse_flag(process.argv[2]) : true;

if (flag) {
	do_fibo(BigInt);
}
else {
	do_fibo(Number);
}
