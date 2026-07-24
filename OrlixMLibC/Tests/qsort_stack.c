// SPDX-License-Identifier: MIT

#include <stddef.h>
#include <stdlib.h>
#include <sys/resource.h>

#define ELEMENT_COUNT	4096
#define STACK_LIMIT	(128 * 1024)

static int values[ELEMENT_COUNT];

static int compare_ints(const void *left, const void *right)
{
	const int lhs = *(const int *)left;
	const int rhs = *(const int *)right;

	return (lhs > rhs) - (lhs < rhs);
}

int main(void)
{
	const struct rlimit limit = {
		.rlim_cur = STACK_LIMIT,
		.rlim_max = STACK_LIMIT,
	};
	size_t index;

	for (index = 0; index < ELEMENT_COUNT; index++)
		values[index] = (int)index;

	if (setrlimit(RLIMIT_STACK, &limit))
		return 1;

	qsort(values, ELEMENT_COUNT, sizeof(values[0]), compare_ints);

	for (index = 0; index < ELEMENT_COUNT; index++)
		if (values[index] != (int)index)
			return 1;

	return 0;
}
