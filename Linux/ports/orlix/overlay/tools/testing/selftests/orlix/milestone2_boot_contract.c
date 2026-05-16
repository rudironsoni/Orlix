// SPDX-License-Identifier: GPL-2.0
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../kselftest.h"

static bool file_contains(const char *path, const char *expected)
{
	FILE *file = fopen(path, "r");
	char buffer[4096];

	if (!file)
		ksft_exit_fail_msg("missing file %s: %s\n", path, strerror(errno));

	while (fgets(buffer, sizeof(buffer), file)) {
		if (strstr(buffer, expected)) {
			fclose(file);
			return true;
		}
	}

	fclose(file);
	return false;
}

static char *join_path(const char *left, const char *right)
{
	char *path;
	int len = snprintf(NULL, 0, "%s/%s", left, right);

	if (len < 0)
		ksft_exit_fail_msg("snprintf failed\n");
	path = malloc((size_t)len + 1);
	if (!path)
		ksft_exit_fail_msg("malloc failed\n");
	snprintf(path, (size_t)len + 1, "%s/%s", left, right);
	return path;
}

static char *kernel_root(void)
{
	char cwd[4096];
	char *root;
	int len;

	if (!getcwd(cwd, sizeof(cwd)))
		ksft_exit_fail_msg("getcwd failed: %s\n", strerror(errno));

	len = snprintf(NULL, 0, "%s/../../../..", cwd);
	if (len < 0)
		ksft_exit_fail_msg("snprintf failed\n");
	root = malloc((size_t)len + 1);
	if (!root)
		ksft_exit_fail_msg("malloc failed\n");
	snprintf(root, (size_t)len + 1, "%s/../../../..", cwd);

	return root;
}

static void expect_file_contains(const char *name, const char *base,
					 const char *relative_path,
					 const char *expected)
{
	char *path = join_path(base, relative_path);

	ksft_test_result(file_contains(path, expected), "%s\n", name);
	free(path);
}

int main(void)
{
	char *root = kernel_root();

	ksft_print_header();
	ksft_set_plan(6);

	expect_file_contains("appstore DTS has compatible string", root,
			     "arch/orlix/boot/dts/appstore.dts",
			     "compatible = \"orlix,appstore\", \"orlix\";");
	expect_file_contains("appstore DTS has profile cmdline", root,
			     "arch/orlix/boot/dts/appstore.dts",
			     "orlix.profile=appstore");
	expect_file_contains("development DTS has compatible string", root,
			     "arch/orlix/boot/dts/development.dts",
			     "compatible = \"orlix,development\", \"orlix\";");
	expect_file_contains("development DTS has profile cmdline", root,
			     "arch/orlix/boot/dts/development.dts",
			     "orlix.profile=development");
	expect_file_contains("enterprise DTS has compatible string", root,
			     "arch/orlix/boot/dts/enterprise.dts",
			     "compatible = \"orlix,enterprise\", \"orlix\";");
	expect_file_contains("enterprise DTS has profile cmdline", root,
			     "arch/orlix/boot/dts/enterprise.dts",
			     "orlix.profile=enterprise");

	free(root);
	ksft_finished();
}
