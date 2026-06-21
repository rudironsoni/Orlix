// SPDX-License-Identifier: GPL-2.0
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define UMASK_PROBE_FILE "orlix-umask-probe-file"
#define UMASK_PROBE_DIR "orlix-umask-probe-dir"
#define UMASK_PROBE_CHILD_FILE "orlix-umask-probe-child-file"

static void write_literal(const char *message)
{
	size_t length = 0;

	while (message[length] != '\0')
		length++;
	(void)write(STDOUT_FILENO, message, length);
}

static int mode_matches(const char *path, mode_t expected)
{
	struct stat st;

	if (stat(path, &st) != 0)
		return 0;
	return (st.st_mode & 0777) == expected;
}

static int create_file_with_mode(const char *path, mode_t mode)
{
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, mode);

	if (fd < 0)
		return -1;
	return close(fd);
}

static int prove_file_and_dir_modes(void)
{
	(void)unlink(UMASK_PROBE_FILE);
	(void)rmdir(UMASK_PROBE_DIR);
	umask(0027);
	if (create_file_with_mode(UMASK_PROBE_FILE, 0666) != 0) {
		write_literal("not ok - umask file create failed\n");
		return 1;
	}
	if (!mode_matches(UMASK_PROBE_FILE, 0640)) {
		write_literal("not ok - umask did not mask file mode\n");
		return 1;
	}
	if (mkdir(UMASK_PROBE_DIR, 0777) != 0) {
		write_literal("not ok - umask mkdir failed\n");
		return 1;
	}
	if (!mode_matches(UMASK_PROBE_DIR, 0750)) {
		write_literal("not ok - umask did not mask directory mode\n");
		return 1;
	}
	write_literal("ok - umask masks file and directory creation modes\n");
	return 0;
}

static int prove_exec_inheritance(char *self)
{
	pid_t child;
	int status;

	(void)unlink(UMASK_PROBE_CHILD_FILE);
	umask(0077);
	child = fork();
	if (child < 0) {
		write_literal("not ok - umask fork failed\n");
		return 1;
	}
	if (child == 0) {
		execl(self, self, "--exec-child", NULL);
		_exit(127);
	}
	if (waitpid(child, &status, 0) != child) {
		write_literal("not ok - umask waitpid failed\n");
		return 1;
	}
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		write_literal("not ok - umask did not survive exec\n");
		return 1;
	}
	write_literal("ok - umask survives exec\n");
	return 0;
}

static int prove_process_isolation(void)
{
	pid_t child;
	int status;
	mode_t previous;

	umask(0022);
	child = fork();
	if (child < 0) {
		write_literal("not ok - umask isolation fork failed\n");
		return 1;
	}
	if (child == 0) {
		umask(0000);
		_exit(0);
	}
	if (waitpid(child, &status, 0) != child) {
		write_literal("not ok - umask isolation waitpid failed\n");
		return 1;
	}
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		write_literal("not ok - umask isolation child failed\n");
		return 1;
	}
	previous = umask(0022);
	if (previous != 0022) {
		write_literal("not ok - child umask leaked to parent\n");
		return 1;
	}
	write_literal("ok - child umask changes stay process-local\n");
	return 0;
}

static int exec_child(void)
{
	if (create_file_with_mode(UMASK_PROBE_CHILD_FILE, 0666) != 0) {
		write_literal("not ok - exec child umask file create failed\n");
		return 1;
	}
	if (!mode_matches(UMASK_PROBE_CHILD_FILE, 0600)) {
		write_literal("not ok - exec child did not inherit umask\n");
		return 1;
	}
	write_literal("ok - exec child inherited umask\n");
	return 0;
}

int main(int argc, char **argv)
{
	if (argc == 2 && strcmp(argv[1], "--exec-child") == 0)
		return exec_child();

	if (prove_file_and_dir_modes() != 0)
		return 1;
	if (prove_exec_inheritance(argv[0]) != 0)
		return 1;
	if (prove_process_isolation() != 0)
		return 1;
	return 0;
}
