// SPDX-License-Identifier: MIT
#include <errno.h>
#include <string.h>

int main(void) {
	static const char expected[] = "No such file or directory";
	char buffer[sizeof(expected)];
	char *result;

	if (strcmp(strerror(ENOENT), expected))
		return 1;

	result = strerror_r(ENOENT, buffer, sizeof(buffer));
	if (result != buffer || strcmp(result, expected))
		return 1;

	return 0;
}
