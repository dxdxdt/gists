.PHONY: all clean profile

all: exfat-default-upcase exfat-test-upcase exfat-print-all-allowed

clean:
	rm -f \
		*.tmp \
		*.o \
		exfat-default-upcase \
		exfat-test-upcase \
		exfat-print-all-allowed

upcase.o: upcase.c
	cc -g -O2 -o upcase.o -c upcase.c -Wall -Wextra
exfat-default-upcase: exfat-default-upcase.c default-upcase-table.h upcase.h upcase.o
	cc -g -O2 -o exfat-default-upcase exfat-default-upcase.c -Wall -Wextra upcase.o

exfat-test-upcase: exfat-test-upcase.c
	cc -flto -O2 -o exfat-test-upcase exfat-test-upcase.c -Wall -Wextra
exfat-print-all-allowed: exfat-print-all-allowed.c
	cc -O2 -o exfat-print-all-allowed exfat-print-all-allowed.c -Wall -Wextra

profile: exfat-print-all-allowed exfat-profile-upcase.sh
	/bin/time -vo exfat-profile-upcase.time.tmp -- ./exfat-print-all-allowed | \
		./exfat-profile-upcase.sh 16 > exfat-profile-upcase.out.tmp
	cat exfat-profile-upcase.out.tmp exfat-profile-upcase.time.tmp > \
		exfat-profile-upcase.tmp
	mv exfat-profile-upcase.tmp exfat-profile-upcase.report
