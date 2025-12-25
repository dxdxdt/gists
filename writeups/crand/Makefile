fmemmem: ../../fmemmem/fmemmem.c
	clang -std=c11 -Wall -Wextra -g -O3 -o fmemmem ../../fmemmem/fmemmem.c
randrb: randrb.c
	clang -std=c11 -Wall -Wextra -g -O3 -flto -o randrb randrb.c
randrb.x64.exe: randrb.c
	x86_64-w64-mingw32-gcc -std=c11 -Wall -Wextra -g -O3 -flto -o randrb.x64.exe randrb.c

randrb.crtdll.exe: randrb.c
	i686-w64-mingw32-gcc -std=c11 -Wall -Wextra -g -O3 -flto -mcrtdll=crtdll -o randrb.crtdll.exe randrb.c
randrb.msvcrt20.exe: randrb.c
	i686-w64-mingw32-gcc -std=c11 -Wall -Wextra -g -O3 -flto -mcrtdll=msvcrt20 -o randrb.msvcrt20.exe randrb.c
randrb.x86.exe: randrb.c
	i686-w64-mingw32-gcc -std=c11 -Wall -Wextra -g -O3 -flto -o randrb.x86.exe randrb.c

randfs: randfs.c
	clang -std=c11 -Wall -Wextra -g -O3 -flto -march=native -mtune=native -o randfs randfs.c
