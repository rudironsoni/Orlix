// SPDX-License-Identifier: MIT

#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static const struct option longopts[] = {
	{ "date", required_argument, NULL, 'd' },
	{ NULL, 0, NULL, 0 },
};

int main(void)
{
	char *dash_argv[] = { "touch", "-d", "-1 hour", "file", NULL };
	char *missing_argv[] = { "touch", "-d", NULL };
	int option;

	optind = 0;
	opterr = 0;
	option = getopt_long(4, dash_argv, "acd:fhmr:t:", longopts, NULL);
	if (option != 'd')
		return 2;
	if (optarg == NULL || strcmp(optarg, "-1 hour") != 0)
		return 3;

	optind = 0;
	opterr = 0;
	option = getopt_long(2, missing_argv, "acd:fhmr:t:", longopts, NULL);
	if (option != '?')
		return 4;
	if (optopt != 'd')
		return 5;

	return 0;
}
