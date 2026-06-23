// SPDX-License-Identifier: GPL-2.0
#define _GNU_SOURCE

#include <sched.h>
#include <stdbool.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

static bool child_exited_success(pid_t child)
{
	int status;

	if (waitpid(child, &status, 0) != child)
		return false;
	return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static bool private_hostname_visible_in_current_uts(void)
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

static bool uts_namespace_allows_private_hostname(void)
{
	pid_t child = fork();

	if (child < 0)
		return false;
	if (child == 0)
		_exit(private_hostname_visible_in_current_uts() ? 0 : 1);

	return child_exited_success(child);
}

static int read_domainname(char *buffer, size_t size)
{
	struct utsname name;
	size_t length;

	if (uname(&name) != 0)
		return -1;

	length = strlen(name.domainname);
	if (length >= size)
		length = size - 1;
	memcpy(buffer, name.domainname, length);
	buffer[length] = '\0';
	return 0;
}

static bool private_domainname_visible_in_current_uts(void)
{
	const char domainname[] = "orlix-domain-probe";
	char observed[64];

	observed[0] = '\0';

	if (unshare(CLONE_NEWUTS) != 0)
		return false;
	if (syscall(SYS_setdomainname, domainname, sizeof(domainname) - 1) != 0)
		return false;
	if (read_domainname(observed, sizeof(observed)) != 0)
		return false;

	observed[sizeof(observed) - 1] = '\0';

	return strcmp(observed, domainname) == 0;
}

static bool uts_namespace_allows_private_domainname(void)
{
	pid_t child = fork();

	if (child < 0)
		return false;
	if (child == 0)
		_exit(private_domainname_visible_in_current_uts() ? 0 : 1);

	return child_exited_success(child);
}

static bool read_current_uts_names(char *hostname, size_t hostname_len,
				   char *domainname, size_t domainname_len)
{
	if (gethostname(hostname, hostname_len) != 0)
		return false;
	if (read_domainname(domainname, domainname_len) != 0)
		return false;

	hostname[hostname_len - 1] = '\0';
	domainname[domainname_len - 1] = '\0';

	return true;
}

static bool child_sets_private_uts_names(void)
{
	const char hostname[] = "orlix-uts-isolated";
	const char domainname[] = "orlix-domain-isolated";
	char observed_hostname[64];
	char observed_domainname[64];

	if (unshare(CLONE_NEWUTS) != 0)
		return false;
	if (sethostname(hostname, sizeof(hostname) - 1) != 0)
		return false;
	if (syscall(SYS_setdomainname, domainname, sizeof(domainname) - 1) != 0)
		return false;
	if (!read_current_uts_names(observed_hostname, sizeof(observed_hostname),
				    observed_domainname,
				    sizeof(observed_domainname)))
		return false;

	return strcmp(observed_hostname, hostname) == 0 &&
	       strcmp(observed_domainname, domainname) == 0;
}

static bool uts_namespace_child_names_do_not_leak_to_parent(void)
{
	char before_hostname[64];
	char before_domainname[64];
	char after_hostname[64];
	char after_domainname[64];
	pid_t child;

	if (!read_current_uts_names(before_hostname, sizeof(before_hostname),
				    before_domainname,
				    sizeof(before_domainname)))
		return false;

	child = fork();
	if (child < 0)
		return false;
	if (child == 0)
		_exit(child_sets_private_uts_names() ? 0 : 1);
	if (!child_exited_success(child))
		return false;

	if (!read_current_uts_names(after_hostname, sizeof(after_hostname),
				    after_domainname,
				    sizeof(after_domainname)))
		return false;

	return strcmp(before_hostname, after_hostname) == 0 &&
	       strcmp(before_domainname, after_domainname) == 0;
}

static bool pid_namespace_first_child_is_init(void)
{
	pid_t child;

	if (unshare(CLONE_NEWPID) != 0)
		return false;

	child = fork();
	if (child < 0)
		return false;
	if (child == 0)
		_exit(getpid() == 1 ? 0 : 1);

	return child_exited_success(child);
}

int main(void)
{
	orlix_test_plan(4);
	orlix_test_result(uts_namespace_allows_private_hostname(),
			  "UTS namespace supports private hostname");
	orlix_test_result(uts_namespace_allows_private_domainname(),
			  "UTS namespace supports private domainname");
	orlix_test_result(uts_namespace_child_names_do_not_leak_to_parent(),
			  "UTS namespace child names do not leak to parent");
	orlix_test_result(pid_namespace_first_child_is_init(),
			  "PID namespace first child is namespace init");
	orlix_test_exit();
}
