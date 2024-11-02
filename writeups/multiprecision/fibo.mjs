#!/bin/env node

function do_fibo (num_f, str_f) {
	let a, b, c;

	if (typeof str_f === 'undefined') {
		str_f = (n) => n.toString();
	}

	a = num_f('0');
	b = num_f('1');

	for (let i = 0; i < 101; i += 1) {
		c = a + b;
		console.info(str_f(c));
		b = a;
		a = c;
	}
}

const flag = process.argv.length > 2 ? process.argv[2] : 0;

switch (flag) {
case '0': do_fibo(Number); break;
case '1': do_fibo(BigInt); break;
case '2': do_fibo(Number, (n) => n.toFixed()); break;
default: throw new Error(process.argv[1] + ": " + flag + ": unknown flag");
}
