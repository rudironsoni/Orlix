/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_PTY_CONSOLE_SMOKE_H
#define ORLIX_TCTI_PTY_CONSOLE_SMOKE_H

static const char tcti_pty_console_stdout_marker[] =
	"ORLIX-PTY-CONSOLE-STDOUT\n";
static const char tcti_pty_console_stderr_marker[] =
	"ORLIX-PTY-CONSOLE-STDERR\n";

typedef long (*tcti_pty_console_smoke_fn)(
	int fd,
	const char *bytes,
	unsigned long length,
	bool *pty_write_entered,
	bool *host_console_mirror_called);

static inline bool tcti_kernel_pty_console_smoke_execute(
	struct pt_regs *regs,
	struct tcti_kernel_pty_console_smoke_result *out,
	tcti_pty_console_smoke_fn write_output)
{
	bool stdout_pty = false;
	bool stderr_pty = false;
	bool stdout_mirror = false;
	bool stderr_mirror = false;
	long stdout_ret;
	long stderr_ret;
	unsigned long stdout_len = sizeof(tcti_pty_console_stdout_marker) - 1;
	unsigned long stderr_len = sizeof(tcti_pty_console_stderr_marker) - 1;

	if (!regs || !out || !write_output)
		return false;

	out->tcti_write_syscall_observed = regs->regs[8] == __NR_write ||
		regs->syscallno == __NR_write;
	out->stdout_fd = 1;
	out->stderr_fd = 2;
	out->stdout_bytes = 0;
	out->stderr_bytes = 0;
	out->mirrored_bytes = 0;
	out->linux_stdout_source_recorded = false;
	out->linux_stderr_source_recorded = false;
	out->linux_pty_write_entered = false;
	out->host_console_mirror_called = false;

	if (!out->tcti_write_syscall_observed)
		return false;

	stdout_ret = write_output(1, tcti_pty_console_stdout_marker,
				  stdout_len, &stdout_pty, &stdout_mirror);
	stderr_ret = write_output(2, tcti_pty_console_stderr_marker,
				  stderr_len, &stderr_pty, &stderr_mirror);

	out->stdout_bytes = stdout_ret > 0 ? (unsigned long)stdout_ret : 0;
	out->stderr_bytes = stderr_ret > 0 ? (unsigned long)stderr_ret : 0;
	out->mirrored_bytes =
		(stdout_mirror ? out->stdout_bytes : 0) +
		(stderr_mirror ? out->stderr_bytes : 0);
	out->linux_stdout_source_recorded = stdout_ret == (long)stdout_len;
	out->linux_stderr_source_recorded = stderr_ret == (long)stderr_len;
	out->linux_pty_write_entered = stdout_pty && stderr_pty;
	out->host_console_mirror_called = stdout_mirror && stderr_mirror;

	return out->linux_stdout_source_recorded &&
		out->linux_stderr_source_recorded &&
		out->linux_pty_write_entered &&
		out->host_console_mirror_called;
}

#endif /* ORLIX_TCTI_PTY_CONSOLE_SMOKE_H */
