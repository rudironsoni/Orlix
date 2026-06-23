// SPDX-License-Identifier: MIT

#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

struct locale_file {
	const char *category;
	const char *path;
};

struct locale_category {
	const char *name;
	int category;
};

static const struct locale_file locale_files[] = {
	{ "LC_ADDRESS", "/usr/lib/locale/de_DE.utf8/LC_ADDRESS" },
	{ "LC_COLLATE", "/usr/lib/locale/de_DE.utf8/LC_COLLATE" },
	{ "LC_CTYPE", "/usr/lib/locale/de_DE.utf8/LC_CTYPE" },
	{ "LC_IDENTIFICATION", "/usr/lib/locale/de_DE.utf8/LC_IDENTIFICATION" },
	{ "LC_MEASUREMENT", "/usr/lib/locale/de_DE.utf8/LC_MEASUREMENT" },
	{ "LC_MESSAGES", "/usr/lib/locale/de_DE.utf8/LC_MESSAGES/SYS_LC_MESSAGES" },
	{ "LC_MONETARY", "/usr/lib/locale/de_DE.utf8/LC_MONETARY" },
	{ "LC_NAME", "/usr/lib/locale/de_DE.utf8/LC_NAME" },
	{ "LC_NUMERIC", "/usr/lib/locale/de_DE.utf8/LC_NUMERIC" },
	{ "LC_PAPER", "/usr/lib/locale/de_DE.utf8/LC_PAPER" },
	{ "LC_TELEPHONE", "/usr/lib/locale/de_DE.utf8/LC_TELEPHONE" },
	{ "LC_TIME", "/usr/lib/locale/de_DE.utf8/LC_TIME" },
};

static const struct locale_category locale_categories[] = {
	{ "LC_COLLATE", LC_COLLATE },
	{ "LC_CTYPE", LC_CTYPE },
	{ "LC_MESSAGES", LC_MESSAGES },
	{ "LC_MONETARY", LC_MONETARY },
	{ "LC_NUMERIC", LC_NUMERIC },
	{ "LC_TIME", LC_TIME },
};

static int check_file(const struct locale_file *entry) {
	struct stat st;
	if (stat(entry->path, &st) != 0) {
		printf("# locale_probe stat %s %s errno=%d %s\n",
				entry->category, entry->path, errno, strerror(errno));
		return 1;
	}

	printf("# LP STAT %s size=%lld\n", entry->category, (long long)st.st_size);

	int fd = open(entry->path, O_RDONLY);
	if (fd < 0) {
		printf("# locale_probe open %s errno=%d %s\n",
				entry->category, errno, strerror(errno));
		return 1;
	}

	unsigned char header[8] = {0};
	ssize_t nread = read(fd, header, sizeof(header));
	if (nread < 0) {
		printf("# locale_probe read %s errno=%d %s\n",
				entry->category, errno, strerror(errno));
		close(fd);
		return 1;
	}

	printf("# LP READ %s n=%zd h=%02x%02x%02x%02x%02x%02x%02x%02x\n",
			entry->category, nread,
			header[0], header[1], header[2], header[3],
			header[4], header[5], header[6], header[7]);

	if (st.st_size > 0) {
		void *mapping = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
		if (mapping == MAP_FAILED) {
			printf("# locale_probe mmap %s errno=%d %s\n",
					entry->category, errno, strerror(errno));
			close(fd);
			return 1;
		}

		const unsigned char *mapped = mapping;
		printf("# LP MMAP %s h=%02x%02x%02x%02x\n",
				entry->category, mapped[0], mapped[1], mapped[2], mapped[3]);
		if (munmap(mapping, (size_t)st.st_size) != 0) {
			printf("# locale_probe munmap %s errno=%d %s\n",
					entry->category, errno, strerror(errno));
			close(fd);
			return 1;
		}
	}

	if (close(fd) != 0) {
		printf("# locale_probe close %s errno=%d %s\n",
				entry->category, errno, strerror(errno));
		return 1;
	}

	return 0;
}

static int check_category(const struct locale_category *entry) {
	if (!setlocale(LC_ALL, "C")) {
		printf("# locale_probe reset C failed before %s\n", entry->name);
		return 1;
	}

	errno = 0;
	char *result = setlocale(entry->category, "de_DE.utf8");
	if (!result) {
		printf("# LP SET %s FAIL errno=%d %s\n", entry->name, errno, strerror(errno));
		return 1;
	}

	printf("# LP SET %s OK %s\n", entry->name, result);
	return 0;
}

int main(void) {
	int failures = 0;

	for (size_t i = 0; i < sizeof(locale_files) / sizeof(locale_files[0]); i++)
		failures += check_file(&locale_files[i]);

	for (size_t i = 0; i < sizeof(locale_categories) / sizeof(locale_categories[0]); i++)
		failures += check_category(&locale_categories[i]);

	if (!setlocale(LC_ALL, "C")) {
		printf("# locale_probe reset C failed before LC_ALL\n");
		failures++;
	}

	errno = 0;
	char *all = setlocale(LC_ALL, "de_DE.utf8");
	if (!all) {
		printf("# LP SET LC_ALL FAIL errno=%d %s\n", errno, strerror(errno));
		failures++;
	} else {
		printf("# LP SET LC_ALL OK %s\n", all);
	}

	printf("# LP FAILURES %d\n", failures);
	return failures ? 1 : 0;
}
