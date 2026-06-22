// SPDX-License-Identifier: GPL-2.0
#include "orlix_kselftest_user.h"

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static bool string_equals(const char *actual, const char *expected)
{
	if (!actual || !expected)
		return false;

	return orlix_strlen(actual) == orlix_strlen(expected) &&
	       orlix_memcmp(actual, expected, orlix_strlen(expected)) == 0;
}

static bool exec_defaults_child_observes_linux_state(int argc, char **argv)
{
	char cwd[128];
	const char *value;

	if (argc != 3)
		return false;

	if (!string_equals(argv[0], "exec-defaults-argv0"))
		return false;

	if (!string_equals(argv[2], "argument-one"))
		return false;

	value = getenv("ORLIX_EXEC_ENV");
	if (!string_equals(value, "oci-defaults"))
		return false;

	if (!getcwd(cwd, sizeof(cwd)))
		return false;

	return string_equals(cwd, "/tmp");
}

static bool read_proc_self_status(void)
{
	char buffer[1024];
	size_t size;

	if (orlix_read_file("/proc/self/status", buffer, sizeof(buffer), &size) != 0)
		return false;

	return orlix_contains(buffer, size, "Name:") &&
	       orlix_contains(buffer, size, "State:");
}

static bool child_exit_status_is_reported(void)
{
	pid_t child = fork();
	int status;

	if (child < 0)
		return false;

	if (child == 0)
		_exit(42);

	if (waitpid(child, &status, 0) != child)
		return false;

	return WIFEXITED(status) && WEXITSTATUS(status) == 42;
}

static bool fork_exec_status_is_reported(void)
{
	pid_t child = fork();
	int status;

	if (child < 0)
		return false;

	if (child == 0) {
		execl("/orlix/process_lifecycle_probe",
		      "process_lifecycle_probe", "exec-child", NULL);
		_exit(127);
	}

	if (waitpid(child, &status, 0) != child)
		return false;

	return WIFEXITED(status) && WEXITSTATUS(status) == 77;
}

static bool fork_exec_carries_argv_env_and_cwd(void)
{
	char *const envp[] = {
		"ORLIX_EXEC_ENV=oci-defaults",
		"PATH=/bin:/usr/bin:/sbin:/usr/sbin",
		NULL
	};
	pid_t child = fork();
	int status;

	if (child < 0)
		return false;

	if (child == 0) {
		if (chdir("/tmp") != 0)
			_exit(126);
		execle("/orlix/process_lifecycle_probe",
		       "exec-defaults-argv0", "exec-defaults-child",
		       "argument-one", NULL, envp);
		_exit(127);
	}

	if (waitpid(child, &status, 0) != child)
		return false;

	return WIFEXITED(status) && WEXITSTATUS(status) == 78;
}

static bool waited_child_is_reaped(void)
{
	pid_t child = fork();
	int status;

	if (child < 0)
		return false;

	if (child == 0)
		_exit(0);

	if (waitpid(child, &status, 0) != child)
		return false;

	errno = 0;
	return waitpid(child, &status, WNOHANG) == -1 && errno == ECHILD;
}

static bool live_child_has_proc_status(void)
{
	pid_t child;
	int status;

	child = fork();
	if (child < 0)
		return false;

	if (child == 0)
		_exit(read_proc_self_status() ? 0 : 1);

	if (waitpid(child, &status, 0) != child)
		return false;

	return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static bool signal_termination_status_is_reported(void)
{
	int pipefds[2];
	pid_t child;
	char byte = 'r';
	int status;

	if (pipe(pipefds) != 0)
		return false;

	child = fork();
	if (child < 0) {
		close(pipefds[0]);
		close(pipefds[1]);
		return false;
	}

	if (child == 0) {
		close(pipefds[0]);
		if (write(pipefds[1], &byte, 1) != 1)
			_exit(1);
		for (;;)
			pause();
	}

	close(pipefds[1]);
	if (read(pipefds[0], &byte, 1) != 1) {
		close(pipefds[0]);
		kill(child, SIGKILL);
		waitpid(child, &status, 0);
		return false;
	}
	close(pipefds[0]);

	if (kill(child, SIGTERM) != 0)
		return false;

	if (waitpid(child, &status, 0) != child)
		return false;

	return WIFSIGNALED(status) && WTERMSIG(status) == SIGTERM;
}

int main(int argc, char **argv)
{
	if (argc == 2 && string_equals(argv[1], "exec-child"))
		_exit(read_proc_self_status() ? 77 : 76);

	if (argc >= 2 && string_equals(argv[1], "exec-defaults-child"))
		_exit(exec_defaults_child_observes_linux_state(argc, argv) ? 78 : 75);

	orlix_test_plan(7);
	orlix_write_all("ORLIX-PROCESS-LIFECYCLE-PROBE\n");

	orlix_test_result(child_exit_status_is_reported(),
			  "forked child exit status is reported by waitpid");
	orlix_test_result(fork_exec_status_is_reported(),
			  "forked child exec status is reported by waitpid");
	orlix_test_result(fork_exec_carries_argv_env_and_cwd(),
			  "forked exec observes Linux argv env and cwd");
	orlix_test_result(waited_child_is_reaped(),
			  "waited child is reaped with ECHILD on second wait");
	orlix_test_result(live_child_has_proc_status(),
			  "child reads Linux procfs status before exit");
	orlix_test_result(signal_termination_status_is_reported(),
			  "signal-terminated child reports Linux wait status");
	orlix_test_result(getpid() > 1,
			  "process has Linux PID allocated by the kernel");

	orlix_test_exit();
}
