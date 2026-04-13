.PHONY: all clean

all: exfat-default-upcase exfat-test-upcase

clean:
	rm -f exfat-default-upcase exfat-test-upcase

upcase.o: upcase.c
	cc -g -O2 -o upcase.o -c upcase.c -Wall -Wextra
exfat-default-upcase: exfat-default-upcase.c default-upcase-table.h upcase.h upcase.o
	cc -g -O2 -o exfat-default-upcase exfat-default-upcase.c -Wall -Wextra upcase.o

exfat-test-upcase: exfat-test-upcase.c
	cc -flto -O2 -o exfat-test-upcase exfat-test-upcase.c -Wall -Wextra
