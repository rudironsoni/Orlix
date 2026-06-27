// SPDX-License-Identifier: GPL-2.0
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

#define ORLIX_OCI_EXPECTED_HOSTNAME "oci-namespace-host"
#define ORLIX_OCI_EXPECTED_DOMAINNAME "oci.example"

static bool hostname_matches_oci_config(void)
{
	char hostname[128];

	if (gethostname(hostname, sizeof(hostname)) != 0)
		return false;
	hostname[sizeof(hostname) - 1] = '\0';
	return strcmp(hostname, ORLIX_OCI_EXPECTED_HOSTNAME) == 0;
}

static bool domainname_matches_oci_config(void)
{
	struct utsname name;

	if (uname(&name) != 0)
		return false;
	return strcmp(name.domainname, ORLIX_OCI_EXPECTED_DOMAINNAME) == 0;
}

static bool proc_namespace_entry_exists(const char *path)
{
	struct stat st;

	errno = 0;
	if (stat(path, &st) != 0) {
		orlix_test_comment_uint("stat namespace errno ",
					(unsigned int)errno);
		return false;
	}

	return true;
}

static bool parse_unsigned_field(char **cursor, unsigned long *value)
{
	char *end;

	errno = 0;
	*value = strtoul(*cursor, &end, 10);
	if (errno != 0 || end == *cursor)
		return false;
	*cursor = end;
	return true;
}

static bool parse_id_map_line(char *line, unsigned long *container_id,
			      unsigned long *host_id, unsigned long *size)
{
	if (!parse_unsigned_field(&line, container_id))
		return false;
	if (!parse_unsigned_field(&line, host_id))
		return false;
	if (!parse_unsigned_field(&line, size))
		return false;
	return true;
}

static bool proc_id_map_contains(const char *path,
				 unsigned long expected_container_id,
				 unsigned long expected_host_id,
				 unsigned long expected_size)
{
	char buffer[512];
	ssize_t nread;
	char *cursor;
	char *line;
	int fd;

	errno = 0;
	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		orlix_test_comment_uint("open proc file errno ",
					(unsigned int)errno);
		return false;
	}

	nread = read(fd, buffer, sizeof(buffer) - 1);
	close(fd);
	if (nread <= 0) {
		orlix_test_comment_uint("read proc file errno ",
					(unsigned int)errno);
		return false;
	}
	buffer[nread] = '\0';

	cursor = buffer;
	while ((line = strsep(&cursor, "\n")) != NULL) {
		unsigned long container_id;
		unsigned long host_id;
		unsigned long size;

		if (!parse_id_map_line(line, &container_id, &host_id, &size))
			continue;
		if (container_id == expected_container_id &&
		    host_id == expected_host_id && size == expected_size)
			return true;
	}

	return false;
}

int main(void)
{
	orlix_write_all("ORLIX-OCI-NAMESPACE-IDENTITY-PROBE\n");
	orlix_write_all("1..6\n");
	orlix_test_result(hostname_matches_oci_config(),
			  "OCI hostname visible through gethostname");
	orlix_test_result(domainname_matches_oci_config(),
			  "OCI domainname visible through uname");
	orlix_test_result(proc_namespace_entry_exists("/proc/self/ns/uts"),
			  "OCI UTS namespace exposes Linux procfs namespace entry");
	orlix_test_result(proc_namespace_entry_exists("/proc/self/ns/user"),
			  "OCI user namespace exposes Linux procfs namespace entry");
	orlix_test_result(proc_id_map_contains("/proc/self/uid_map", 0, 0, 1),
			  "OCI uidMappings visible through Linux uid_map");
	orlix_test_result(proc_id_map_contains("/proc/self/gid_map", 0, 0, 1),
			  "OCI gidMappings visible through Linux gid_map");
	orlix_test_exit();
}
