// SPDX-License-Identifier: GPL-2.0
#define _GNU_SOURCE

#include <sched.h>
#include <stdbool.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

static bool uts_namespace_allows_private_hostname(void)
{
	const char hostname[] = "orlix-uts-probe";
	char observed[64];

	if (unshare(CLONE_NEWUTS) != 0)
		return false;
	if (sethostname(hostname, sizeof(hostname) - 1) != 0)
		return false;
	if (gethostname(observed, sizeof(observed)) != 0)
		return false;
	observed[sizeof(observed) - 1] = '\0';
	return strcmp(observed, hostname) == 0;
}

static bool uts_namespace_allows_private_domainname(void)
{
	const char domainname[] = "orlix-domain-probe";
	char observed[64];

	observed[0] = '\0';

	if (unshare(CLONE_NEWUTS) != 0)
		return false;
	if (setdomainname(domainname, sizeof(domainname) - 1) != 0)
		return false;
	if (getdomainname(observed, sizeof(observed)) != 0)
		return false;

	observed[sizeof(observed) - 1] = '\0';

	return strcmp(observed, domainname) == 0;
}

static bool pid_namespace_first_child_is_init(void)
{
	pid_t child;
	int status;

	if (unshare(CLONE_NEWPID) != 0)
		return false;

	child = fork();
	if (child < 0)
		return false;
	if (child == 0)
		_exit(getpid() == 1 ? 0 : 1);

	if (waitpid(child, &status, 0) != child)
		return false;
	return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

int main(void)
{
	orlix_test_plan(3);
	orlix_test_result(uts_namespace_allows_private_hostname(),
			  "UTS namespace supports private hostname");
	orlix_test_result(uts_namespace_allows_private_domainname(),
			  "UTS namespace supports private domainname");
	orlix_test_result(pid_namespace_first_child_is_init(),
			  "PID namespace first child is namespace init");
	orlix_test_exit();
}
