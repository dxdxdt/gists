.PHONY: all clean

all: exfat-default-upcase exfat-test-upcase

clean:
	rm -f exfat-default-upcase exfat-test-upcase

exfat-default-upcase: exfat-default-upcase.c default-upcase-table.h upcase.h
	cc -g -O0 -o exfat-default-upcase exfat-default-upcase.c -Wall -Wextra

exfat-test-upcase: exfat-test-upcase.c
	cc -flto -O2 -o exfat-test-upcase exfat-test-upcase.c -Wall -Wextra
