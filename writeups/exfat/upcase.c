// SPDX-License-Identifier: GPL-2.0-or-later

#include "upcase.h"

struct exfat_upcase_range_info def_utbl_ri[EXFAT_UPTBL_ARRSIZE + 1] __initconst;

int exfat_set_upcase_ptable(struct exfat_upcase_ptable *ptbl,
		const __u16 index, const __u16 value)
{
	const size_t page_idx = index / EXFAT_UPTBL_PAGESIZE;
	const size_t idx_in_page = index % EXFAT_UPTBL_PAGESIZE;

	if (value == 0)
		return 0;

	if (ptbl->pages[page_idx] == NULL) {
		void *nm = kvcalloc(EXFAT_UPTBL_PAGESIZE, sizeof(__u16), GFP_KERNEL);

		if (nm == NULL)
			return -ENOMEM;
		ptbl->pages[page_idx] = nm;
		ptbl->cnt++;
	}

	ptbl->pages[page_idx][idx_in_page] = value;

	return 0;
}

void exfat_free_upcase_ptable(struct exfat_upcase_ptable *ptbl)
{
	if (ptbl == NULL)
		return;

	for (size_t i = 0; i < ARRAY_SIZE(ptbl->pages); i ++) {
		kvfree(ptbl->pages[i]);
		ptbl->pages[i] = NULL;
	}
	ptbl->cnt = 0;
}

int __init exfat_populate_upcase_ptable(struct exfat_upcase_ptable *ptbl)
{
	const struct exfat_upcase_range_info *ri;
	int ret;

	for (ri = def_utbl_ri; ri->inc != 0; ri++) {
		/* Memory safety: allow the value to wrap around but not the index */
		const unsigned int step = ri->inc;
		unsigned int index = ri->start;
		__u16 value = ri->value;

		if (index >= ri->end) {
			/* Damaged .rodata */
			ret = -EINVAL;
			goto err;
		}

		while (index < ri->end) {
			ret = exfat_set_upcase_ptable(ptbl, index, value);
			if (ret)
				goto err;

			index += step;
			value += step;
		}
	}

	return 0;
err:
	exfat_free_upcase_ptable(ptbl);
	return ret;
}
