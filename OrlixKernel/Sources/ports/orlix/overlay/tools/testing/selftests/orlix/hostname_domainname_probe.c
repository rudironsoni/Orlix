// SPDX-License-Identifier: GPL-2.0

#include <string.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

int main(void)
{
	char hostname[128];
	char domainname[128];

	orlix_test_plan(2);
	orlix_test_result(gethostname(hostname, sizeof(hostname)) == 0 &&
			  strcmp(hostname, "oci-host") == 0,
			  "hostname boot token is visible through gethostname");
	orlix_test_result(getdomainname(domainname, sizeof(domainname)) == 0 &&
			  strcmp(domainname, "oci.example") == 0,
			  "domainname boot token is visible through getdomainname");
	orlix_test_exit();
}
