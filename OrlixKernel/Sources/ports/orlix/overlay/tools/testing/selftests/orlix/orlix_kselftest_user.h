/* SPDX-License-Identifier: GPL-2.0 */
#ifndef ORLIX_KSELFTEST_USER_H
#define ORLIX_KSELFTEST_USER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>

static int orlix_test_index;
static int orlix_test_failures;

static size_t orlix_strlen(const char *s)
{
	size_t len = 0;

	while (s[len])
		len++;
	return len;
}

static int orlix_memcmp(const void *left, const void *right, size_t len)
{
	const unsigned char *a = left;
	const unsigned char *b = right;
	size_t i;

	for (i = 0; i < len; i++) {
		if (a[i] != b[i])
			return (int)a[i] - (int)b[i];
	}
	return 0;
}

static void orlix_write_all(const char *s)
{
	size_t len = orlix_strlen(s);

	while (len > 0) {
		ssize_t written = write(STDOUT_FILENO, s, len);

		if (written <= 0)
			_exit(125);
		s += written;
		len -= (size_t)written;
	}
}

static void orlix_write_uint(unsigned int value)
{
	char buffer[16];
	size_t pos = sizeof(buffer);

	buffer[--pos] = '\0';
	do {
		buffer[--pos] = (char)('0' + (value % 10));
		value /= 10;
	} while (value);
	orlix_write_all(&buffer[pos]);
}

static void orlix_test_plan(unsigned int count)
{
	orlix_write_all("TAP version 13\n1..");
	orlix_write_uint(count);
	orlix_write_all("\n");
}

static void orlix_test_result(bool passed, const char *name)
{
	orlix_test_index++;
	if (!passed)
		orlix_test_failures++;
	orlix_write_all(passed ? "ok " : "not ok ");
	orlix_write_uint((unsigned int)orlix_test_index);
	orlix_write_all(" - ");
	orlix_write_all(name);
	orlix_write_all("\n");
}

static void orlix_test_exit(void)
{
	_exit(orlix_test_failures ? 1 : 0);
}

static bool orlix_contains(const char *haystack, size_t size, const char *needle)
{
	size_t needle_len = orlix_strlen(needle);
	size_t i;

	if (needle_len == 0)
		return true;
	if (needle_len > size)
		return false;
	for (i = 0; i + needle_len <= size; i++) {
		if (orlix_memcmp(haystack + i, needle, needle_len) == 0)
			return true;
	}
	return false;
}

static int orlix_read_file(const char *path, char *buffer, size_t capacity,
			   size_t *size)
{
	int fd = open(path, O_RDONLY);

	if (fd < 0)
		return -1;

	*size = 0;
	while (*size + 1 < capacity) {
		ssize_t nread = read(fd, buffer + *size, capacity - *size - 1);

		if (nread < 0) {
			close(fd);
			return -1;
		}
		if (nread == 0)
			break;
		*size += (size_t)nread;
	}
	buffer[*size] = '\0';
	close(fd);
	return 0;
}

static uint32_t orlix_read_be32(const unsigned char *data)
{
	return ((uint32_t)data[0] << 24) |
	       ((uint32_t)data[1] << 16) |
	       ((uint32_t)data[2] << 8) |
	       (uint32_t)data[3];
}

#endif /* ORLIX_KSELFTEST_USER_H */
