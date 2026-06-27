// SPDX-License-Identifier: GPL-2.0

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

#define ORLIX_OCI_RLIMIT_NOFILE_SOFT 32
#define ORLIX_OCI_RLIMIT_NOFILE_HARD 32
#define ORLIX_OCI_UMASK_PROBE_FILE "/tmp/orlix-oci-process-umask"

static bool rlimit_nofile_matches_oci_config(void)
{
	struct rlimit limit;

	if (getrlimit(RLIMIT_NOFILE, &limit) != 0)
		return false;

	return limit.rlim_cur == ORLIX_OCI_RLIMIT_NOFILE_SOFT &&
	       limit.rlim_max == ORLIX_OCI_RLIMIT_NOFILE_HARD;
}

static bool rlimit_nofile_enforces_emfile(void)
{
	int fds[ORLIX_OCI_RLIMIT_NOFILE_HARD + 8];
	size_t count = 0;
	bool saw_emfile = false;

	for (size_t i = 0; i < sizeof(fds) / sizeof(fds[0]); i++)
		fds[i] = -1;

	for (;;) {
		int fd = open("/dev/null", O_RDONLY | O_CLOEXEC);

		if (fd >= 0) {
			if (count < sizeof(fds) / sizeof(fds[0]))
				fds[count] = fd;
			count++;
			continue;
		}

		saw_emfile = errno == EMFILE;
		break;
	}

	for (size_t i = 0; i < sizeof(fds) / sizeof(fds[0]); i++) {
		if (fds[i] >= 0)
			close(fds[i]);
	}

	return saw_emfile;
}

static bool no_new_privileges_matches_oci_config(void)
{
	return prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0) == 1;
}

static bool umask_matches_oci_config(void)
{
	struct stat st;
	int fd;

	(void)unlink(ORLIX_OCI_UMASK_PROBE_FILE);

	fd = open(ORLIX_OCI_UMASK_PROBE_FILE,
		  O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0666);
	if (fd < 0)
		return false;
	if (close(fd) != 0)
		return false;
	if (stat(ORLIX_OCI_UMASK_PROBE_FILE, &st) != 0)
		return false;

	return (st.st_mode & 0777) == 0640;
}

int main(void)
{
	orlix_write_all("ORLIX-OCI-PROCESS-ATTRIBUTES-PROBE\n");
	orlix_test_plan(4);

	orlix_test_result(rlimit_nofile_matches_oci_config(),
			  "OCI process rlimit is visible through getrlimit");
	orlix_test_result(rlimit_nofile_enforces_emfile(),
			  "OCI process rlimit is enforced by Linux");
	orlix_test_result(no_new_privileges_matches_oci_config(),
			  "OCI noNewPrivileges is visible through prctl");
	orlix_test_result(umask_matches_oci_config(),
			  "OCI process user umask controls created file mode");

	orlix_test_exit();
}
