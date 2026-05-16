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

static void expect_file_not_contains(const char *name, const char *base,
				     const char *relative_path,
				     const char *unexpected)
{
	char *path = join_path(base, relative_path);

	ksft_test_result(!file_contains(path, unexpected), "%s\n", name);
	free(path);
}

static void expect_profile_probe_shape(const char *port_dir, const char *profile)
{
	char dts[128];

	snprintf(dts, sizeof(dts), "arch/orlix/boot/dts/%s.dts", profile);
	expect_file_contains("profile has base virtio-mmio node", port_dir, dts,
			     "virtio_base: virtio@10001000 {");
	expect_file_contains("profile has state virtio-mmio node", port_dir, dts,
			     "virtio_state: virtio@10001200 {");
	expect_file_contains("profile virtio nodes use upstream compatible", port_dir, dts,
			     "compatible = \"virtio,mmio\";");
	expect_file_contains("profile has base virtio-mmio register range", port_dir, dts,
			     "reg = <0x0 0x10001000 0x0 0x200>;");
	expect_file_contains("profile has state virtio-mmio register range", port_dir, dts,
			     "reg = <0x0 0x10001200 0x0 0x200>;");
	expect_file_contains("profile has base virtio interrupt", port_dir, dts,
			     "interrupts = <32>;");
	expect_file_contains("profile has state virtio interrupt", port_dir, dts,
			     "interrupts = <33>;");
}

static void expect_profile_config(const char *root, const char *profile)
{
	(void)profile;
	expect_file_contains("profile config enables OF", root,
			     "arch/orlix/configs/defconfig", "CONFIG_OF=y");
	expect_file_contains("profile config enables virtio", root,
			     "arch/orlix/configs/defconfig", "CONFIG_VIRTIO=y");
	expect_file_contains("profile config enables virtio-mmio", root,
			     "arch/orlix/configs/defconfig", "CONFIG_VIRTIO_MMIO=y");
	expect_file_contains("profile config enables virtio-blk", root,
			     "arch/orlix/configs/defconfig", "CONFIG_VIRTIO_BLK=y");
	expect_file_not_contains("profile config does not enable Orlix block", root,
				 "arch/orlix/configs/defconfig",
				 "CONFIG_ORLIX_BLOCK=y");
}

int main(void)
{
	char *root = kernel_root();

	ksft_print_header();
	ksft_set_plan(29);

	expect_profile_probe_shape(root, "appstore");
	expect_profile_probe_shape(root, "development");
	expect_profile_probe_shape(root, "enterprise");

	expect_profile_config(root, "selected");

	expect_file_contains("arch/orlix selects OF", root,
			     "arch/orlix/Kconfig", "select OF");
	expect_file_contains("arch/orlix selects HAS_IOMEM", root,
			     "arch/orlix/Kconfig", "select HAS_IOMEM");
	expect_file_contains("arch/orlix selects HAS_DMA", root,
			     "arch/orlix/Kconfig", "select HAS_DMA");
	free(root);
	ksft_finished();
}
