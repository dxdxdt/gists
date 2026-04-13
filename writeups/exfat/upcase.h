#ifndef _EXFAT_UPCASE_TABLE_H_
#define _EXFAT_UPCASE_TABLE_H_

#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <linux/types.h>

/* polyfill kernel facilities */
#define GFP_KERNEL (0)
#define kvcalloc(a, b, c)	calloc(a, b)
#define kvfree		free
#define ARRAY_SIZE(arr)	(sizeof(arr) / sizeof((arr)[0]))

/*
 * exfat upcase paged table
 */
/* magic values */
#define EXFAT_UPTBL_PAGESIZE	(512)
#define EXFAT_UPTBL_SIZE	(1 << 16)
#define EXFAT_UPTBL_ARRSIZE	(EXFAT_UPTBL_SIZE / EXFAT_UPTBL_PAGESIZE)

struct exfat_upcase_ptable {
	__u16 *pages[EXFAT_UPTBL_ARRSIZE];
	size_t cnt;
};

struct exfat_upcase_range_info {
	__u16 start;
	__u16 end;
	__u16 value;
	__u16 inc;
};

/* some safety checks */
static_assert(EXFAT_UPTBL_SIZE % EXFAT_UPTBL_PAGESIZE == 0);
static_assert(0xFFFF / EXFAT_UPTBL_PAGESIZE < EXFAT_UPTBL_ARRSIZE);

/* exfat/upcase.c */
int exfat_set_upcase_ptable (struct exfat_upcase_ptable *ptbl,
		const __u16 index, const __u16 value);
__u16 exfat_lookup_upcase_ptable (const struct exfat_upcase_ptable *ptbl,
		const __u16 index);
void exfat_free_upcase_ptable (struct exfat_upcase_ptable *ptbl);
int exfat_populate_upcase_ptable (struct exfat_upcase_ptable *ptbl,
		const struct exfat_upcase_range_info *ri,
		const size_t cnt);

#endif /* !_EXFAT_UPCASE_TABLE_H_ */
