#ifndef MMAGIC_H
#define MMAGIC_H

#ifdef __linux__
/* glibc can shut the fuck up */
#undef _BSD_SOURCE
#define _GNU_SOURCE
#else
#define _BSD_SOURCE
#define _NETBSD_SOURCE
#endif

#include <sys/types.h>

#define MEMFILE_SHM_ONLY 0x01

int mkmemfile(int flags);
void *mmemfile(const int fd, off_t ofs, const size_t req, size_t *size, size_t *len);

#endif
