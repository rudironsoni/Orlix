// SPDX-License-Identifier: GPL-2.0
#include <errno.h>
#include <linux/capability.h>
#include <stdbool.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

#define ORLIX_CAP_WORDS _LINUX_CAPABILITY_U32S_3

static bool capget_current(struct __user_cap_data_struct data[ORLIX_CAP_WORDS])
{
	struct __user_cap_header_struct header = {
		.version = _LINUX_CAPABILITY_VERSION_3,
		.pid = 0,
	};

	memset(data, 0, sizeof(struct __user_cap_data_struct) * ORLIX_CAP_WORDS);
	return syscall(SYS_capget, &header, data) == 0;
}

static bool capset_current(const struct __user_cap_data_struct data[ORLIX_CAP_WORDS])
{
	struct __user_cap_header_struct header = {
		.version = _LINUX_CAPABILITY_VERSION_3,
		.pid = 0,
	};

	return syscall(SYS_capset, &header, data) == 0;
}

static bool proc_status_exposes_capability_fields(void)
{
	char status[4096];
	size_t size = 0;

	if (orlix_read_file("/proc/self/status", status, sizeof(status),
			    &size) != 0)
		return false;

	return orlix_contains(status, size, "CapInh:") &&
	       orlix_contains(status, size, "CapPrm:") &&
	       orlix_contains(status, size, "CapEff:") &&
	       orlix_contains(status, size, "CapBnd:") &&
	       orlix_contains(status, size, "CapAmb:");
}

static bool proc_status_effective_caps_are_zero(void)
{
	char status[4096];
	size_t size = 0;

	if (orlix_read_file("/proc/self/status", status, sizeof(status),
			    &size) != 0)
		return false;

	return orlix_contains(status, size, "CapEff:\t0000000000000000");
}

static bool proc_status_ambient_caps_are_zero(void)
{
	char status[4096];
	size_t size = 0;

	if (orlix_read_file("/proc/self/status", status, sizeof(status),
			    &size) != 0)
		return false;

	return orlix_contains(status, size, "CapAmb:\t0000000000000000");
}

static bool capset_clears_effective_caps_and_proc_reports_it(void)
{
	struct __user_cap_data_struct original[ORLIX_CAP_WORDS];
	struct __user_cap_data_struct reduced[ORLIX_CAP_WORDS];
	bool observed;

	if (!capget_current(original))
		return false;

	memcpy(reduced, original, sizeof(reduced));
	for (size_t index = 0; index < ORLIX_CAP_WORDS; index++)
		reduced[index].effective = 0;

	if (!capset_current(reduced))
		return false;

	observed = proc_status_effective_caps_are_zero();
	if (!capset_current(original))
		return false;

	return observed;
}

static bool prctl_reads_bounding_set_for_standard_capability(void)
{
	int value = prctl(PR_CAPBSET_READ, CAP_CHOWN, 0, 0, 0);

	return value == 0 || value == 1;
}

static bool prctl_clears_ambient_set_and_proc_reports_it(void)
{
	if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_CLEAR_ALL, 0, 0, 0) != 0)
		return false;
	return proc_status_ambient_caps_are_zero();
}

int main(void)
{
	struct __user_cap_data_struct data[ORLIX_CAP_WORDS];

	orlix_write_all("ORLIX-PROCESS-CAPABILITY-PROBE\n");
	orlix_test_plan(6);

	orlix_test_result(capget_current(data),
			  "capget reads current Linux capability sets");
	orlix_test_result(proc_status_exposes_capability_fields(),
			  "proc self status exposes Linux capability fields");
	orlix_test_result(capset_current(data),
			  "capset accepts current Linux capability sets");
	orlix_test_result(capset_clears_effective_caps_and_proc_reports_it(),
			  "capset effective changes are visible in proc status");
	orlix_test_result(prctl_reads_bounding_set_for_standard_capability(),
			  "prctl reads Linux capability bounding set");
	orlix_test_result(prctl_clears_ambient_set_and_proc_reports_it(),
			  "prctl ambient clear is visible in proc status");

	orlix_test_exit();
}
