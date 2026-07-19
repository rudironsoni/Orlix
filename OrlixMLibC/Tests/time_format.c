// SPDX-License-Identifier: MIT
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static int check_format(const char *name, const char *format,
		const struct tm *value, const char *expected)
{
	char output[128];
	size_t length = strftime(output, sizeof(output), format, value);

	if (!length) {
		printf("# %s strftime returned zero\n", name);
		return 1;
	}
	if (strcmp(output, expected)) {
		printf("# %s actual='%s' expected='%s'\n", name, output, expected);
		return 1;
	}
	if (length != strlen(expected)) {
		printf("# %s length=%zu expected_length=%zu\n",
				name, length, strlen(expected));
		return 1;
	}
	return 0;
}

int main(void)
{
	const struct tm january = {
		.tm_sec = 48,
		.tm_min = 17,
		.tm_hour = 8,
		.tm_mday = 19,
		.tm_mon = 0,
		.tm_year = 97,
		.tm_wday = 0,
		.tm_yday = 18,
	};
	const struct tm december = {
		.tm_min = 30,
		.tm_hour = 7,
		.tm_mday = 8,
		.tm_mon = 11,
		.tm_year = 99,
		.tm_wday = 3,
		.tm_yday = 341,
	};
	int failures = 0;

	if (!setlocale(LC_ALL, "C")) {
		printf("# setlocale C failed\n");
		return 1;
	}

	failures += check_format("composite_date_time",
			"prefix:%x:%X:%y:%Y:suffix", &january,
			"prefix:01/19/97:08:17:48:97:1997:suffix");
	failures += check_format("leading_space_composite",
			" %c", &january, " Sun Jan 19 08:17:48 1997");
	failures += check_format("surrounded_composite",
			"prefix:%c:suffix", &december,
			"prefix:Wed Dec  8 07:30:00 1999:suffix");

	return failures != 0;
}
