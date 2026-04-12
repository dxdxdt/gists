#ifndef _EXFAT_UPCASE_TABLE_H_
#define _EXFAT_UPCASE_TABLE_H_
#include <linux/types.h>
#include <linux/stddef.h>

/* magic values */
#define EXFAT_UPTBL_PAGESIZE	(512)
#define EXFAT_UPTBL_SIZE	(1 << 16)
#define EXFAT_UPTBL_ARRSIZE	(EXFAT_UPTBL_SIZE / EXFAT_UPTBL_PAGESIZE)

struct exfat_upcase_ptable {
	__u16 *pages[EXFAT_UPTBL_ARRSIZE];
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

static inline int exfat_set_upcase_ptable (struct exfat_upcase_ptable *ptbl,
					   const __u16 index, const __u16 value)
{
	const size_t page_idx = index / EXFAT_UPTBL_PAGESIZE;
	const size_t idx_in_page = index % EXFAT_UPTBL_PAGESIZE;

	if (ptbl->pages[page_idx] == NULL) {
		void *nm = kvcalloc(EXFAT_UPTBL_PAGESIZE, sizeof(__u16));

		if (nm == NULL)
			return -ENOMEM;
		ptbl->pages[page_idx] = nm;
	}

	ptbl->pages[page_idx][idx_in_page] = value;

	return 0;
}

static inline __u16 exfat_lookup_upcase_ptable (struct exfat_upcase_ptable *ptbl,
						const __u16 index)
{
	const size_t page_idx = index / EXFAT_UPTBL_PAGESIZE;
	const size_t idx_in_page = index % EXFAT_UPTBL_PAGESIZE;

	return ptbl->pages[page_idx] == NULL ? 0 : ptbl->pages[page_idx][idx_in_page];
}

static inline void exfat_free_upcase_ptable (struct exfat_upcase_ptable *ptbl)
{
	if (ptbl == NULL)
		return;

	for (size_t i = 0; i < ARRAY_SIZE(ptbl->pages); i ++)
		kvfree(ptbl->pages[i]);
}

static inline int exfat_populate_upcase_ptable (struct exfat_upcase_ptable *ptbl,
						const struct exfat_upcase_range_info *ri,
						const size_t cnt)
{
	int err;

	for (__u16 i = 0; i < cnt; i++) {
		/* Memory safety: allow the value to wrap around but not the index */
		const unsigned int step = ri[i].inc;
		unsigned int index = ri[i].start;
		__u16 value = ri[i].value;

		if (step == 0 || index >= ri[i].end)
			/* Damaged .rodata */
			return -EINVAL;

		while (index < ri[i].end) {
			err = exfat_set_upcase_ptable(ptbl, index, value);
			if (err)
				return err;

			index += step;
			value += step;
		}
	}

	return 0;
}

#endif /* !_EXFAT_UPCASE_TABLE_H_ */
