#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <errno.h>
#include <assert.h>
#include <locale.h>
#include <getopt.h>
#include <unistd.h>

#include "default-upcase-table.h"
#include "upcase.h"

enum outtype {
	OT_NONE,
	OT_TABLE_ERRORS,
	OT_PAGED_TABLE,

	NB_OUTTYPE
};
#define assert_outtype_in_range(x) assert(OT_NONE <= x && x < NB_OUTTYPE)

static struct {
	long pagesize;
	enum outtype ot;
	bool mode_c;
} ui = {
	.pagesize = 512,
};

static void decompress_upcase_table (const uint8_t *in, const size_t len, uint16_t *out)
{
	bool skip = false;
	uint32_t index = 0;

	for (size_t i = 1; i < len; i += 2) {
		const uint16_t uni = in[i - 1] | ((uint16_t)in[i] << 8);

		if (skip) {
			index += uni;
			assert(index <= 0xFFFF);
			skip = false;
		} else if (uni == index) {
			index++;
		} else if (uni == 0xFFFF) {
			skip = true;
		} else { /* uni != index , uni != 0xFFFF */
			out[index] = uni;
			index++;
		}
	}
}

static void print_conv (const wint_t a, const wint_t b, const char *msg)
{
	printf("%04X(%lc) -> %04X(%lc)", a, a, b, b);
	if (msg != NULL)
		printf(": %s", msg);
	putc('\n', stdout);
}

static void print_table_human (const uint16_t *tbl)
{
	ssize_t last_idx_page = -1, entcnt = 0, pagecnt = 0, nzpages = 0;
	bool page_had_contents = false;

	for (size_t i = 0; i <= 0xFFFF; i++) {
		const uint16_t v = tbl[i];
		const ssize_t this_idx_page = (ssize_t)i / ui.pagesize;

		if (v != 0) {
			print_conv((wint_t)i, (wint_t)v, NULL);
			page_had_contents = true;
			entcnt++;
		}

		if (last_idx_page != this_idx_page) {
			if (page_had_contents) {
				nzpages++;
				fprintf(stderr, "========== page %zd ==========\n", pagecnt);
			}
			pagecnt++;
			page_had_contents = false;
		}
		last_idx_page = this_idx_page;
	}

	fprintf(stderr, "entries: %zd (%zu bytes)\n", entcnt, entcnt * sizeof(uint16_t));
	fprintf(stderr, "non-empty pages: %zd (nb_page * pagesize * %zu = %ld * %zd * 2 = %zd bytes)\n",
		nzpages,
		sizeof(uint16_t),
		ui.pagesize,
		nzpages,
		ui.pagesize * nzpages * sizeof(uint16_t));
}

static void print_table_c (const uint16_t *tbl)
{
	bool print_nl = true;

	for (size_t i = 0; i <= 0xFFFF; i++) {
		print_nl = (i + 1) % 8 == 0;

		printf("0x%04"PRIX16", ", tbl[i]);
		if (print_nl)
			putc('\n', stdout);
	}

	if (!print_nl)
		putc('\n', stdout);
}

static inline const char *check_conv_err (const wint_t a, const wint_t b)
{
	if (iswlower(a)) {
		/* should be converted to upper */
		if (!iswupper(b))
			return "lower not converted to upper";
		return NULL;
	}

	if (iswupper(a)) {
		/* upper shouldn't be converted */
		if (a != b)
			return "upper converted";
		return NULL;
	}

	/* caseless shouldn't be converted to anything */
	if (a != b)
		return "caseless converted";

	return NULL;
}

/* find errors in the table */
static void print_table_errors (const uint16_t *tbl)
{
	for (wint_t a = 0; a <= 0xFFFF; a++) {
		const wint_t b = tbl[a];
		/* skip chars not covered in table */
		const char *msg = b ? check_conv_err(a, b) : NULL;

		if (msg != NULL)
			print_conv(a, b, msg);
	}
}

typedef void (*emit_range_info_callback_t) (const uint16_t start, const uint16_t end,
					    const uint16_t value, const uint16_t inc,
					    void *uc);

static inline void do_emit_range_info (uint16_t start, uint16_t end,
				       uint16_t value, uint16_t inc,
				       emit_range_info_callback_t cb, void *uc)
{
	uint16_t len;

	assert(start <= end);
	end += 1;
	len = end - start;

	if (inc == 0)
		inc = 1;
	if (len < inc)
		inc = len;

	cb(start, end, value, inc, uc);
}

/*
 * Group contiguous runs of upcase mapping in the default upcase table.
 * The "compressed" format of exFAT upcase table is not so space-efficient.
 * We compress the table further by converting it into a series of metadata
 * that can be used to populate the default upcase table.
 */
static void emit_range_info (const uint16_t *tbl, emit_range_info_callback_t cb, void *uc)
{
	bool first = false, second = false; /* whether following state values are valid */
	uint16_t start, value, inc;
	unsigned int last_value;
	uint16_t end; /* but not this one */

	for (unsigned int i = 0; i <= 0xFFFF; i++) {
		const uint16_t this_value = tbl[i];

		/* 0xFFFF cannot be mapped due to format limitations */
		assert(!(0xFFFF == i && this_value != 0));

		if (this_value == 0) {
			if (second && end + inc == i) {
				first = false;
				goto emit;
			}
			continue;
		}

		if (first) {
			if (!second)
				inc = (uint16_t)i - start;

			if (last_value + inc != this_value || end + inc != i) {
				if (second)
					end = i - 1;
				goto emit;
			}

			second = true;
		} else {
			/* Found the start of the run. cache it and continue */
			start = i;
			value = this_value;
			inc = 0;
			first = true;
		}

		last_value = this_value;
		end = i;
		continue;
emit:
		do_emit_range_info(start, end, value, inc, cb, uc);

		start = (uint16_t)i;
		last_value = value = this_value;
		end = i;
		inc = 0;
		second = false;
	}
}

static struct {
	size_t len;
	struct exfat_upcase_range_info arr[EXFAT_UPTBL_SIZE];
} ric_ctx;

static void range_info_callback_human (const uint16_t start, const uint16_t end,
				       const uint16_t value, const uint16_t inc,
				       void *)
{
	struct exfat_upcase_range_info *ri = ric_ctx.arr + ric_ctx.len;

	assert(ric_ctx.len < ARRAY_SIZE(ric_ctx.arr));

	ri->start = start;
	ri->end = end;
	ri->value = value;
	ri->inc = inc;

	ric_ctx.len++;
}

static void print_ptable_human (const uint16_t *tbl) {
	static struct exfat_upcase_ptable ptbl;
	uint16_t a, b;
	int err;

	emit_range_info(tbl, range_info_callback_human, NULL);

	err = exfat_populate_upcase_ptable(&ptbl, ric_ctx.arr, ric_ctx.len);
	if (!err)
		for (unsigned int i = 0; i <= 0xFFFF; i++) {
			a = exfat_lookup_upcase_ptable(&ptbl, (uint16_t)i);
			b = tbl[i];

			assert(a == b);

			if (a != 0)
				print_conv((wint_t)i, (wint_t)a, NULL);
		}

	exfat_free_upcase_ptable(&ptbl);
}

static void range_info_callback_c (const uint16_t start, const uint16_t end,
				   const uint16_t value, const uint16_t inc,
				   void *)
{
	printf("	{\n"
		"		/* (index = %zu, len = %d) */\n"
		"		.start = 0x%04X,\n"
		"		.end   = 0x%04X,\n"
		"		.value = 0x%04X,\n"
		"		.inc   = 0x%04X,\n"
		"	},\n",
		ric_ctx.len, end - start, start, end, value, inc);

	ric_ctx.len++;
}

static void usage (const char *argv0)
{
	fprintf(stderr, "Usage: %s [-cep] [-s SIZE]\n", argv0);
	exit(2);
}

static void parse_opts (int argc, const char **argv)
{
	int ret;

	for (;;) {
		ret = getopt(argc, (char *const*)argv, "ceps:");

		switch (ret) {
		case 'c':
			ui.mode_c = true;
			break;
		case 'e':
			ui.ot = OT_TABLE_ERRORS;
			break;
		case 'p':
			ui.ot = OT_PAGED_TABLE;
			break;
		case 's':
			ui.pagesize = strtol(optarg, NULL, 0);
			if (ui.pagesize <= 0) {
				errno = EINVAL;
				perror(optarg);
				exit(2);
			}
			break;
		case -1:
			goto out;
		default:
			usage(argv[0]);
		}
	}

out:
	switch (ui.ot) {
	case OT_TABLE_ERRORS:
		if (ui.mode_c) {
			fprintf(stderr, "-c mode not supported with -e output type.\n");
			exit(2);
		}
		break;
	default:
		break;
	}
}

int main (int argc, const char **argv)
{
	static char *lc;
	static uint16_t table[1 << 16];

	lc = setlocale(LC_ALL, "C.UTF-8");
	assert(lc != NULL);
	(void)lc;

	parse_opts(argc, argv);

	assert_outtype_in_range(ui.ot);
	assert(ui.pagesize > 0);

	decompress_upcase_table(default_upcase_table, sizeof(default_upcase_table), table);

	switch (ui.ot) {
	case OT_NONE:
		if (ui.mode_c)
			print_table_c(table);
		else
			print_table_human(table);
		break;
	case OT_TABLE_ERRORS:
		print_table_errors(table);
		break;
	case OT_PAGED_TABLE:
		if (ui.mode_c)
			emit_range_info(table, range_info_callback_c, NULL);
		else
			print_ptable_human(table);
		break;
	default:
		break; /* unreachable */
	}

	return 0;
}
