#include <stdio.h>
#include <stdbool.h>
#include <wchar.h>
#include <wctype.h>
#include <locale.h>

int main (void) {
	static unsigned int col = 0;

	setlocale(LC_ALL, "C.utf-8");

	for (wint_t i = 0x20; i <= 0xFFFF; i += 1) {
		if (!iswprint(i) || iswblank(i))
			continue;
		switch (i) {
		case 0x0022:
		case 0x002A:
		case 0x002E:
		case 0x002F:
		case 0x003A:
		case 0x003C:
		case 0x003E:
		case 0x003F:
		case 0x005C:
		case 0x007C:
			continue;
		}

		putwc(i, stdout);
		col += 1;

		if (col % 64 == 0) {
			putc('\n', stdout);
			fflush(stdout);
		}
	}

	if (col % 64 != 0)
		putc('\n', stdout);

	return 0;
}
