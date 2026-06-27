// SPDX-License-Identifier: GPL-2.0
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

#define ORLIX_OCI_EXPECTED_HOSTNAME "oci-time-host"
#define ORLIX_OCI_EXPECTED_MONOTONIC_SECS 5
#define ORLIX_OCI_EXPECTED_MONOTONIC_NANOSECS 123456789
#define ORLIX_OCI_EXPECTED_BOOTTIME_SECS 7
#define ORLIX_OCI_EXPECTED_BOOTTIME_NANOSECS 987654321

static bool hostname_matches_oci_config(void)
{
	char hostname[128];

	if (gethostname(hostname, sizeof(hostname)) != 0)
		return false;
	hostname[sizeof(hostname) - 1] = '\0';
	return strcmp(hostname, ORLIX_OCI_EXPECTED_HOSTNAME) == 0;
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

static bool parse_long_field(char **cursor, long *value)
{
	char *end;

	errno = 0;
	*value = strtol(*cursor, &end, 10);
	if (errno != 0 || end == *cursor)
		return false;
	*cursor = end;
	return true;
}

static bool timens_offsets_contains(const char *clock_name,
				    long expected_secs,
				    long expected_nanosecs)
{
	char buffer[512];
	ssize_t nread;
	char *cursor;
	char *line;
	int fd;

	errno = 0;
	fd = open("/proc/self/timens_offsets", O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		orlix_test_comment_uint("open timens_offsets errno ",
					(unsigned int)errno);
		return false;
	}

	nread = read(fd, buffer, sizeof(buffer) - 1);
	close(fd);
	if (nread <= 0) {
		orlix_test_comment_uint("read timens_offsets errno ",
					(unsigned int)errno);
		return false;
	}
	buffer[nread] = '\0';

	cursor = buffer;
	while ((line = strsep(&cursor, "\n")) != NULL) {
		char *fields = line;
		char *name = strsep(&fields, " \t");
		long secs;
		long nanosecs;

		if (name == NULL || fields == NULL || strcmp(name, clock_name) != 0)
			continue;
		if (!parse_long_field(&fields, &secs))
			continue;
		if (!parse_long_field(&fields, &nanosecs))
			continue;
		if (secs == expected_secs && nanosecs == expected_nanosecs)
			return true;
	}

	return false;
}

static bool clock_reads(clockid_t clock_id)
{
	struct timespec value;

	return clock_gettime(clock_id, &value) == 0;
}

int main(void)
{
	orlix_write_all("ORLIX-OCI-TIME-NAMESPACE-PROBE\n");
	orlix_write_all("1..6\n");
	orlix_test_result(hostname_matches_oci_config(),
			  "OCI time proof hostname visible through gethostname");
	orlix_test_result(proc_namespace_entry_exists("/proc/self/ns/time"),
			  "OCI time namespace exposes Linux procfs namespace entry");
	orlix_test_result(timens_offsets_contains(
				  "monotonic",
				  ORLIX_OCI_EXPECTED_MONOTONIC_SECS,
				  ORLIX_OCI_EXPECTED_MONOTONIC_NANOSECS),
			  "OCI monotonic timeOffset visible through timens_offsets");
	orlix_test_result(timens_offsets_contains(
				  "boottime",
				  ORLIX_OCI_EXPECTED_BOOTTIME_SECS,
				  ORLIX_OCI_EXPECTED_BOOTTIME_NANOSECS),
			  "OCI boottime timeOffset visible through timens_offsets");
	orlix_test_result(clock_reads(CLOCK_MONOTONIC),
			  "Linux CLOCK_MONOTONIC reads in OCI time namespace");
	orlix_test_result(clock_reads(CLOCK_BOOTTIME),
			  "Linux CLOCK_BOOTTIME reads in OCI time namespace");
	orlix_test_exit();
}
