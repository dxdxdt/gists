#include <stdio.h>
#include <stdbool.h>

#if 0
#pragma pack(1)
#endif
struct {
	unsigned a;
	bool b;
	unsigned short c;
	unsigned d;
} a;

int main(void) {
	a.a = 0xAAAAAAAA;
	a.b = true;
	a.c = 0xFFFF;
	a.d = 0x55555555;

	for (unsigned i = 1; i <= sizeof(a); i += 1) {
		printf("%02x ", ((unsigned char*)&a)[i - 1]);
		if (i % 4 == 0) {
			printf("\n");
		}
	}

	return 0;
}
