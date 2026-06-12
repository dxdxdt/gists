#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <getopt.h>
#include <windows.h>

#define PROGNAME "deathbylfs"

static BOOL set_sparse_len (HANDLE f, LARGE_INTEGER *size)
{
	return	/* `fsutil sparse setFlag ...` */
		DeviceIoControl(f, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, NULL, NULL) &&
		/* `fsutil file setEOF ...` */
		SetFilePointer(f, size->LowPart, &size->HighPart, FILE_BEGIN) != INVALID_SET_FILE_POINTER &&
		SetEndOfFile(f);
}

int main (int argc, char *argv[])
{
	int c;
	int ret = 2;
	LARGE_INTEGER size = { 0, };

	while ((c = getopt(argc, argv, "l:")) != -1) {
		switch (c) {
		case 'l':
			if (sscanf(optarg, "%lld", &size.QuadPart) != 1 || size.QuadPart < 0) {
				fprintf(stderr, PROGNAME": -l %s: %s\n", optarg, strerror(EINVAL));
				return ret;
			}
			break;
		default:
			return ret;
		}
	}

	printf(PROGNAME": length = %lX %lX\n", size.HighPart, size.LowPart);

	for (int i = optind; i < argc; i += 1) {
		const char *path = argv[i];
		const HANDLE f = CreateFileA(path,
					     GENERIC_WRITE,
					     0,
					     NULL,
					     OPEN_ALWAYS,
					     FILE_ATTRIBUTE_NORMAL,
					     NULL);

		if (f == INVALID_HANDLE_VALUE) {
			goto err;
		}

		if (set_sparse_len(f, &size)) {
			ret = ret == 1 ? 3 : 0;
			goto cont;
		}
		else {
			ret = ret == 3 ? 3 : 1;
		}

err:
		fprintf(stderr, PROGNAME": %s: LastError=%ld\n", path, GetLastError());
cont:
		if (f != INVALID_HANDLE_VALUE) {
			CloseHandle(f);
		}
	}

	return ret;
}
