#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <dlfcn.h>

#include <blkid/blkid.h>

static void *lib;

static struct {
	int (*get_cache)(blkid_cache *cache, const char *filename);
	blkid_dev (*get_dev)(blkid_cache cache, const char *devname, int flags);
	int (*probe_all)(blkid_cache cache);
	blkid_dev_iterate (*dev_iterate_begin)(blkid_cache cache);
	void (*dev_iterate_end)(blkid_dev_iterate iterate);
	int (*dev_next)(blkid_dev_iterate iterate, blkid_dev *dev);
	blkid_dev (*verify)(blkid_cache cache, blkid_dev dev);
	blkid_tag_iterate (*tag_iterate_begin)(blkid_dev dev);
	void (*tag_iterate_end)(blkid_tag_iterate iterate);
	const char *(*dev_devname)(blkid_dev dev);
	int (*tag_next)(blkid_tag_iterate iterate, const char **type, const char **value);
	int (*get_library_version)(const char **ver_string, const char **date_string);

	const void *end;
} blkid;

static bool do_load_blkid(void)
{
	static const struct {
		const char *name;
		void **target;
	} SYMTBL[] = {
		{ .name = "blkid_get_cache", .target = (void**)&blkid.get_cache },
		{ .name = "blkid_get_dev", .target = (void**)&blkid.get_dev },
		{ .name = "blkid_probe_all", .target = (void**)&blkid.probe_all },
		{ .name = "blkid_dev_iterate_begin", .target = (void**)&blkid.dev_iterate_begin },
		{ .name = "blkid_dev_iterate_end", .target = (void**)&blkid.dev_iterate_end },
		{ .name = "blkid_dev_next", .target = (void**)&blkid.dev_next },
		{ .name = "blkid_verify", .target = (void**)&blkid.verify },
		{ .name = "blkid_tag_iterate_begin", .target = (void**)&blkid.tag_iterate_begin },
		{ .name = "blkid_tag_iterate_end", .target = (void**)&blkid.tag_iterate_end },
		{ .name = "blkid_dev_devname", .target = (void**)&blkid.dev_devname },
		{ .name = "blkid_tag_next", .target = (void**)&blkid.tag_next },
		{ .name = "blkid_get_library_version", .target = (void**)&blkid.get_library_version },
		{}
	};
	const char *mod = getenv("BLKID_LIB");

	if (mod == NULL) {
		fprintf(stderr, "BLKID_LIB env var not set.\n");
		return false;
	}

	lib = dlopen(mod, RTLD_LAZY);
	if (lib == NULL) {
		fprintf(stderr, "%s: %s\n", mod, dlerror());
		return false;
	}

	for (size_t i = 0; SYMTBL[i].name != NULL; i++) {
		*SYMTBL[i].target = dlsym(lib, SYMTBL[i].name);
		if (*SYMTBL[i].target == NULL) {
			fprintf(stderr, "%s: %s\n", SYMTBL[i].name, dlerror());
			return false;
		}
	}

	return true;
}

/*
 * This function does "safe" printing.  It will convert non-printable
 * ASCII characters using '^' and M- notation.
 */
static void safe_print(const char *cp, int len)
{
	unsigned char	ch;

	if (len < 0)
		len = strlen(cp);

	while (len--) {
		ch = *cp++;
		if (ch > 128) {
			fputs("M-", stdout);
			ch -= 128;
		}
		if ((ch < 32) || (ch == 0x7f)) {
			fputc('^', stdout);
			ch ^= 0x40; /* ^@, ^A, ^B; ^? for DEL */
		}
		if (ch != '"') {
			fputc(ch, stdout);
		}
	}
}

static void print_tags(blkid_dev dev)
{
	blkid_tag_iterate	iter;
	const char		*type, *value;
	int 			first = 1;

	if (!dev)
		return;

	iter = blkid.tag_iterate_begin(dev);
	while (blkid.tag_next(iter, &type, &value) == 0) {
		if (first) {
			printf("%s: ", blkid.dev_devname(dev));
			first = 0;
		}
		fputs(type, stdout);
		fputs("=\"", stdout);
		safe_print(value, -1);
		fputs("\" ", stdout);
	}
	blkid.tag_iterate_end(iter);

	if (!first)
		printf("\n");
}

int main(int argc, char **argv)
{
	const char *libver = "", *libdate = "";
	blkid_cache cache = NULL;
	blkid_dev dev;

	if (!do_load_blkid())
		return 1;

	blkid.get_library_version(&libver, &libdate);
	fprintf(stderr, "Modver: %s (%s)\n", libver, libdate);

	if (blkid.get_cache(&cache, NULL) < 0 ||
			(dev = blkid.get_dev(cache, argv[1], BLKID_DEV_CREATE | BLKID_DEV_VERIFY)) == NULL) {
		perror(argv[1]);
		return 1;
	}

	print_tags(dev);

	return 0;
}
