// SPDX-License-Identifier: MIT

#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

struct locale_file {
	const char *category;
	const char *path;
	uint32_t expected_magic;
	uint32_t expected_count;
	const uint32_t *expected_offsets;
	size_t expected_offset_count;
};

struct locale_category {
	const char *name;
	int category;
};

static const uint32_t lc_messages_offsets[] = { 28, 38, 46, 49, 54 };
static const uint32_t lc_numeric_offsets[] = { 32, 34, 36, 40, 44, 48 };

static const struct locale_file locale_files[] = {
	{ "LC_ADDRESS", "/usr/lib/locale/de_DE.utf8/LC_ADDRESS", 0, 0, NULL, 0 },
	{ "LC_COLLATE", "/usr/lib/locale/de_DE.utf8/LC_COLLATE", 0, 0, NULL, 0 },
	{ "LC_CTYPE", "/usr/lib/locale/de_DE.utf8/LC_CTYPE", 0, 0, NULL, 0 },
	{ "LC_IDENTIFICATION", "/usr/lib/locale/de_DE.utf8/LC_IDENTIFICATION", 0, 0, NULL, 0 },
	{ "LC_MEASUREMENT", "/usr/lib/locale/de_DE.utf8/LC_MEASUREMENT", 0, 0, NULL, 0 },
	{ "LC_MESSAGES", "/usr/lib/locale/de_DE.utf8/LC_MESSAGES/SYS_LC_MESSAGES", 0x20031110, 5, lc_messages_offsets, sizeof(lc_messages_offsets) / sizeof(lc_messages_offsets[0]) },
	{ "LC_MONETARY", "/usr/lib/locale/de_DE.utf8/LC_MONETARY", 0, 0, NULL, 0 },
	{ "LC_NAME", "/usr/lib/locale/de_DE.utf8/LC_NAME", 0, 0, NULL, 0 },
	{ "LC_NUMERIC", "/usr/lib/locale/de_DE.utf8/LC_NUMERIC", 0x20031114, 6, lc_numeric_offsets, sizeof(lc_numeric_offsets) / sizeof(lc_numeric_offsets[0]) },
	{ "LC_PAPER", "/usr/lib/locale/de_DE.utf8/LC_PAPER", 0, 0, NULL, 0 },
	{ "LC_TELEPHONE", "/usr/lib/locale/de_DE.utf8/LC_TELEPHONE", 0, 0, NULL, 0 },
	{ "LC_TIME", "/usr/lib/locale/de_DE.utf8/LC_TIME", 0, 0, NULL, 0 },
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

	struct stat fd_st;
	if (fstat(fd, &fd_st) != 0) {
		printf("# locale_probe fstat %s errno=%d %s\n",
				entry->category, errno, strerror(errno));
		close(fd);
		return 1;
	}

	printf("# LP FSTAT %s size=%lld mode=%o\n",
			entry->category, (long long)fd_st.st_size,
			(unsigned int)(fd_st.st_mode & 07777));

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

	if (entry->expected_magic) {
		uint32_t magic = (uint32_t)header[0]
			| ((uint32_t)header[1] << 8)
			| ((uint32_t)header[2] << 16)
			| ((uint32_t)header[3] << 24);
		uint32_t count = (uint32_t)header[4]
			| ((uint32_t)header[5] << 8)
			| ((uint32_t)header[6] << 16)
			| ((uint32_t)header[7] << 24);
		printf("# LP HEADER %s magic=0x%08x count=%u\n",
				entry->category, magic, count);
		if (magic != entry->expected_magic || count != entry->expected_count) {
			printf("# locale_probe header mismatch %s\n", entry->category);
			close(fd);
			return 1;
		}
	}

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
		for (size_t i = 0; i < entry->expected_offset_count; i++) {
			uint32_t offset = entry->expected_offsets[i];
			unsigned char via_pread = 0;
			ssize_t pread_n = pread(fd, &via_pread, 1, (off_t)offset);
			if (pread_n != 1) {
				printf("# locale_probe pread %s offset=%u n=%zd errno=%d %s\n",
						entry->category, offset, pread_n, errno, strerror(errno));
				munmap(mapping, (size_t)st.st_size);
				close(fd);
				return 1;
			}
			printf("# LP OFFSET %s index=%zu off=%u mmap=%02x pread=%02x\n",
					entry->category, i, offset, mapped[offset], via_pread);
			if (mapped[offset] != via_pread) {
				printf("# LP BAD %s I%zu O%u M%02x P%02x\n",
						entry->category, i, offset, mapped[offset],
						via_pread);
				printf("# locale_probe offset mismatch %s index=%zu\n",
						entry->category, i);
				munmap(mapping, (size_t)st.st_size);
				close(fd);
				return 1;
			}
		}
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
