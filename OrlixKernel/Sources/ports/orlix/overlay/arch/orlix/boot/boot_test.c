// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <asm/boot.h>
#include <asm/page.h>

static void orlix_boot_handoff_records_params(struct kunit *test)
{
	const struct boot_params *previous = arch_boot_params();
	struct boot_params previous_params = {};
	int previous_count = arch_boot_handoff_count();
	struct boot_params params = {
		.cmdline = "console=ttyS0 root=/dev/vda rootfstype=ext4 ro orlix.profile=release",
		.memory_base = 0x100000,
		.memory_size = 0x4000000,
		.dtb_base = "dtb",
		.dtb_size = 3,
		.root_device = "/dev/vda",
		.console_device = "ttyS0",
	};
	const struct boot_params *recorded;
	int replay;

	if (!!previous != !!previous_count) {
		KUNIT_FAIL(test, "inconsistent prior boot handoff state");
		return;
	}

	if (previous)
		previous_params = *previous;

	arch_boot_reset_handoff();
	KUNIT_EXPECT_EQ(test, 0, arch_boot_handoff_count());
	KUNIT_EXPECT_PTR_EQ(test, NULL, arch_boot_last_params());

	arch_boot_test_record_handoff(&params);

	KUNIT_EXPECT_EQ(test, 1, arch_boot_handoff_count());
	recorded = arch_boot_last_params();
	KUNIT_EXPECT_NOT_NULL(test, recorded);
	if (recorded) {
#if defined(ORLIX_APP_HOSTED_BOOT)
		KUNIT_EXPECT_PTR_NE(test, &params, recorded);
		KUNIT_EXPECT_MEMEQ(test, &params, recorded, sizeof(params));
#else
		KUNIT_EXPECT_PTR_EQ(test, &params, recorded);
#endif
		KUNIT_EXPECT_STREQ(test, params.cmdline, recorded->cmdline);
		KUNIT_EXPECT_EQ(test, params.memory_base,
				recorded->memory_base);
		KUNIT_EXPECT_EQ(test, params.memory_size,
				recorded->memory_size);
		KUNIT_EXPECT_PTR_EQ(test, params.dtb_base,
				recorded->dtb_base);
		KUNIT_EXPECT_EQ(test, params.dtb_size, recorded->dtb_size);
		KUNIT_EXPECT_STREQ(test, params.root_device,
				recorded->root_device);
		KUNIT_EXPECT_STREQ(test, params.console_device,
				recorded->console_device);
	}
	KUNIT_EXPECT_PTR_EQ(test, recorded, arch_boot_params());

	arch_boot_reset_handoff();
	if (previous_count) {
		KUNIT_EXPECT_NOT_NULL(test, previous);
		if (previous) {
			for (replay = 0; replay < previous_count; replay++) {
#if defined(ORLIX_APP_HOSTED_BOOT)
				arch_boot_test_record_handoff(&previous_params);
#else
				arch_boot_test_record_handoff(previous);
#endif
			}
		}
	}
	KUNIT_EXPECT_EQ(test, previous_count, arch_boot_handoff_count());
#if defined(ORLIX_APP_HOSTED_BOOT)
	recorded = arch_boot_last_params();
	if (previous_count) {
		KUNIT_EXPECT_NOT_NULL(test, recorded);
		if (recorded)
			KUNIT_EXPECT_MEMEQ(test, &previous_params, recorded,
					   sizeof(previous_params));
	} else {
		KUNIT_EXPECT_PTR_EQ(test, NULL, recorded);
	}
#else
	KUNIT_EXPECT_PTR_EQ(test, previous, arch_boot_last_params());
#endif
}

static void orlix_boot_handoff_rejects_missing_dtb(struct kunit *test)
{
	const struct boot_params *previous = arch_boot_last_params();
	int previous_count = arch_boot_handoff_count();
	struct boot_params params = {
		.cmdline = "console=ttyS0 root=/dev/vda rootfstype=ext4 ro orlix.profile=release",
		.root_device = "/dev/vda",
		.console_device = "ttyS0",
	};

	KUNIT_EXPECT_EQ(test, ORLIX_ARCH_BOOT_INVALID_CONFIG,
			arch_boot_prepare_entry(&params));
	KUNIT_EXPECT_EQ(test, previous_count, arch_boot_handoff_count());
	KUNIT_EXPECT_PTR_EQ(test, previous, arch_boot_last_params());
}

static void orlix_boot_host_page_size_requires_linux_granule(struct kunit *test)
{
	KUNIT_EXPECT_FALSE(test, arch_boot_host_page_size_supported(0));
	KUNIT_EXPECT_FALSE(test, arch_boot_host_page_size_supported(PAGE_SIZE - 1));
	KUNIT_EXPECT_TRUE(test, arch_boot_host_page_size_supported(PAGE_SIZE));
	KUNIT_EXPECT_FALSE(test, arch_boot_host_page_size_supported(PAGE_SIZE * 2));

	if (PAGE_SIZE > 4096)
		KUNIT_EXPECT_TRUE(test, arch_boot_host_page_size_supported(4096));
}

static struct kunit_case orlix_boot_test_cases[] = {
	KUNIT_CASE(orlix_boot_handoff_records_params),
	KUNIT_CASE(orlix_boot_handoff_rejects_missing_dtb),
	KUNIT_CASE(orlix_boot_host_page_size_requires_linux_granule),
	{}
};

static struct kunit_suite orlix_boot_test_suite = {
	.name = "orlix-boot",
	.test_cases = orlix_boot_test_cases,
};

kunit_test_suite(orlix_boot_test_suite);

MODULE_LICENSE("GPL");
