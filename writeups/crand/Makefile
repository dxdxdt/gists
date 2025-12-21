fmemmem: ../../fmemmem/fmemmem.c
	clang -std=c11 -Wall -Wextra -g -O3 -o fmemmem ../../fmemmem/fmemmem.c
randrb: randrb.c
	clang -std=c11 -Wall -Wextra -g -O3 -flto -o randrb randrb.c
randrb.exe: randrb.c
	x86_64-w64-mingw32-gcc -std=c11 -Wall -Wextra -g -O3 -flto -o randrb.exe randrb.c

randfs: randfs.c
	clang -std=c11 -Wall -Wextra -g -O3 -flto -march=native -mtune=native -o randfs randfs.c
