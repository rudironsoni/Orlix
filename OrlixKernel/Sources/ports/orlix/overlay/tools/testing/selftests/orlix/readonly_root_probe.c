// SPDX-License-Identifier: GPL-2.0

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

static bool field_equals(const char *start, const char *end, const char *value)
{
	size_t field_len = (size_t)(end - start);
	size_t value_len = strlen(value);

	return field_len == value_len && memcmp(start, value, value_len) == 0;
}

static bool options_have_ro(const char *start, const char *end)
{
	const char *cursor = start;

	while (cursor < end) {
		const char *next = cursor;

		while (next < end && *next != ',')
			next++;
		if (field_equals(cursor, next, "ro"))
			return true;
		cursor = next + 1;
	}

	return false;
}

static bool mountinfo_root_is_ro(void)
{
	char buffer[16384];
	ssize_t nread;
	char *line;
	int fd;

	fd = open("/proc/self/mountinfo", O_RDONLY);
	if (fd < 0)
		return false;

	nread = read(fd, buffer, sizeof(buffer) - 1);
	close(fd);
	if (nread <= 0)
		return false;
	buffer[nread] = '\0';

	for (line = buffer; *line != '\0';) {
		char *line_end = strchr(line, '\n');
		char *cursor = line;
		char *field_start;
		char *field_end;
		int field;

		if (line_end == NULL)
			line_end = buffer + nread;
		*line_end = '\0';

		for (field = 1; field <= 6; field++) {
			while (*cursor == ' ')
				cursor++;
			field_start = cursor;
			while (*cursor != '\0' && *cursor != ' ')
				cursor++;
			field_end = cursor;

			if (field == 5 &&
			    !field_equals(field_start, field_end, "/"))
				break;
			if (field == 6)
				return options_have_ro(field_start, field_end);
		}

		line = line_end + 1;
	}

	return false;
}

static bool root_write_fails_erofs(void)
{
	int fd;

	errno = 0;
	fd = open("/orlix-readonly-root-probe", O_WRONLY | O_CREAT | O_TRUNC,
		  0600);
	if (fd >= 0) {
		close(fd);
		unlink("/orlix-readonly-root-probe");
		return false;
	}

	return errno == EROFS;
}

int main(void)
{
	orlix_test_plan(2);
	orlix_test_result(mountinfo_root_is_ro(),
			  "root mount is read-only in /proc/self/mountinfo");
	orlix_test_result(root_write_fails_erofs(),
			  "creating a file under / fails with EROFS");
	orlix_test_exit();
}
