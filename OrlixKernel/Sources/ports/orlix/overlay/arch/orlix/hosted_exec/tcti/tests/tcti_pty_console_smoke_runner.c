// SPDX-License-Identifier: GPL-2.0-only
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <linux/types.h>

#define __NR_write 64
#define NO_SYSCALL (-1)
#define PSR_MODE_EL0t 0x00000000UL
#define STACK_TOP 0x0000800000000000UL

struct pt_regs {
	u64 regs[31];
	u64 sp;
	u64 pc;
	u64 pstate;
	u64 orig_x0;
	s32 syscallno;
	u32 unused;
};

struct tcti_kernel_pty_console_smoke_result {
	bool tcti_write_syscall_observed;
	bool linux_stdout_source_recorded;
	bool linux_stderr_source_recorded;
	bool linux_pty_write_entered;
	bool host_console_mirror_called;
	int stdout_fd;
	int stderr_fd;
	unsigned long stdout_bytes;
	unsigned long stderr_bytes;
	unsigned long mirrored_bytes;
};

#include "../pty_console_smoke.h"

static unsigned char console_mirror[256];
static unsigned long console_mirror_length;

static long linux_pty_console_stub(int fd, const char *bytes,
				   unsigned long length,
				   bool *pty_write_entered,
				   bool *host_console_mirror_called)
{
	if ((fd != 1 && fd != 2) || !bytes || !length)
		return -1;
	if (console_mirror_length + length > sizeof(console_mirror))
		return -1;

	memcpy(&console_mirror[console_mirror_length], bytes, (size_t)length);
	console_mirror_length += length;
	*pty_write_entered = true;
	*host_console_mirror_called = true;
	return (long)length;
}

static bool mirror_contains(const char *needle)
{
	size_t needle_length = strlen(needle);
	unsigned long index;

	if (needle_length == 0 || needle_length > console_mirror_length)
		return false;
	for (index = 0; index + needle_length <= console_mirror_length;
	     index++) {
		if (memcmp(&console_mirror[index], needle, needle_length) == 0)
			return true;
	}
	return false;
}

static bool smoke_result_passed(
	const struct tcti_kernel_pty_console_smoke_result *result)
{
	return result->tcti_write_syscall_observed &&
	       result->linux_stdout_source_recorded &&
	       result->linux_stderr_source_recorded &&
	       result->linux_pty_write_entered &&
	       result->host_console_mirror_called &&
	       result->stdout_fd == 1 &&
	       result->stderr_fd == 2 &&
	       result->stdout_bytes ==
		       sizeof(tcti_pty_console_stdout_marker) - 1 &&
	       result->stderr_bytes ==
		       sizeof(tcti_pty_console_stderr_marker) - 1 &&
	       result->mirrored_bytes == result->stdout_bytes +
		       result->stderr_bytes &&
	       mirror_contains(tcti_pty_console_stdout_marker) &&
	       mirror_contains(tcti_pty_console_stderr_marker);
}

int main(void)
{
	const char *test_name =
		"tcti_kernel_pty_console_smoke_reports_output";
	struct tcti_kernel_pty_console_smoke_result result;
	struct pt_regs regs;
	bool ok;

	memset(&result, 0, sizeof(result));
	memset(&regs, 0, sizeof(regs));
	regs.pc = 0x210128;
	regs.sp = STACK_TOP - 16;
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;
	regs.regs[8] = __NR_write;

	ok = tcti_kernel_pty_console_smoke_execute(
		&regs, &result, linux_pty_console_stub);
	ok = ok && smoke_result_passed(&result);

	printf("KTAP version 1\n");
	printf("1..1\n");
	if (ok)
		printf("ok 1 - orlix-tcti-decode.%s\n", test_name);
	else
		printf("not ok 1 - orlix-tcti-decode.%s\n", test_name);
	printf("%s=%s\n", test_name, ok ? "pass" : "fail");
	printf("ORLIX-PTY-CONSOLE-RUNNER-BEGIN\n");
	printf("ORLIX-PTY-CONSOLE-RUNNER workload_hook_executed=%s\n",
	       ok ? "true" : "false");
	printf("ORLIX-PTY-CONSOLE-RUNNER tcti_write_syscall_observed=%s\n",
	       result.tcti_write_syscall_observed ? "true" : "false");
	printf("ORLIX-PTY-CONSOLE-RUNNER linux_stdout_source_recorded=%s\n",
	       result.linux_stdout_source_recorded ? "true" : "false");
	printf("ORLIX-PTY-CONSOLE-RUNNER linux_stderr_source_recorded=%s\n",
	       result.linux_stderr_source_recorded ? "true" : "false");
	printf("ORLIX-PTY-CONSOLE-RUNNER linux_pty_write_entered=%s\n",
	       result.linux_pty_write_entered ? "true" : "false");
	printf("ORLIX-PTY-CONSOLE-RUNNER host_console_mirror_called=%s\n",
	       result.host_console_mirror_called ? "true" : "false");
	printf("ORLIX-PTY-CONSOLE-RUNNER stdout_fd=%d\n",
	       result.stdout_fd);
	printf("ORLIX-PTY-CONSOLE-RUNNER stderr_fd=%d\n",
	       result.stderr_fd);
	printf("ORLIX-PTY-CONSOLE-RUNNER stdout_bytes=%lu\n",
	       result.stdout_bytes);
	printf("ORLIX-PTY-CONSOLE-RUNNER stderr_bytes=%lu\n",
	       result.stderr_bytes);
	printf("ORLIX-PTY-CONSOLE-RUNNER mirrored_bytes=%lu\n",
	       result.mirrored_bytes);
	printf("ORLIX-PTY-CONSOLE-RUNNER stdout_marker=%s",
	       tcti_pty_console_stdout_marker);
	printf("ORLIX-PTY-CONSOLE-RUNNER stderr_marker=%s",
	       tcti_pty_console_stderr_marker);
	printf("ORLIX-PTY-CONSOLE-RUNNER-END status=%s\n",
	       ok ? "pass" : "fail");

	return ok ? 0 : 1;
}
