// SPDX-License-Identifier: GPL-2.0

#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

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

int main(void)
{
	char hostname[128];
	char domainname[128];

	orlix_test_plan(2);
	orlix_test_result(gethostname(hostname, sizeof(hostname)) == 0 &&
			  strcmp(hostname, "oci-host") == 0,
			  "hostname boot token is visible through gethostname");
	orlix_test_result(read_domainname(domainname, sizeof(domainname)) == 0 &&
			  strcmp(domainname, "oci.example") == 0,
			  "domainname boot token is visible through uname");
	orlix_test_exit();
}
