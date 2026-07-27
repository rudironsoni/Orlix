/* SPDX-License-Identifier: GPL-2.0-only */
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "target_arm_xml_package.h"

#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define EXPECT(expression) do { \
	if (!(expression)) { \
		fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, \
			#expression); \
		return 1; \
	} \
} while (0)

static int copy_file(const char *source, const char *destination)
{
	char buffer[65536];
	FILE *input = fopen(source, "rb");
	FILE *output = NULL;
	size_t count;
	int result = -1;

	if (!input)
		goto out;
	output = fopen(destination, "wb");
	if (!output)
		goto out;
	while ((count = fread(buffer, 1, sizeof(buffer), input)) != 0U) {
		if (fwrite(buffer, 1, count, output) != count)
			goto out;
	}
	if (!ferror(input) && !fflush(output) && !fclose(output)) {
		output = NULL;
		result = 0;
	}
out:
	if (output)
		fclose(output);
	if (input)
		fclose(input);
	return result;
}

static int mutate_last_byte(const char *path)
{
	unsigned char byte;
	int fd = open(path, O_RDWR);

	if (fd < 0 || lseek(fd, -1, SEEK_END) < 0 || read(fd, &byte, 1) != 1 ||
	    lseek(fd, -1, SEEK_END) < 0) {
		if (fd >= 0)
			close(fd);
		return -1;
	}
	byte ^= 1U;
	return write(fd, &byte, 1) == 1 && !close(fd) ? 0 : -1;
}

static int mutation_tests(const char *archive, const char *release)
{
	static const char pattern[] = "/tmp/orlix-arm-xml-package-XXXXXX";
	char root[sizeof(pattern)];
	char archive_copy[PATH_MAX] = { 0 };
	struct orlix_tcti_arm_xml_package package;
	int result = -1;

	memcpy(root, pattern, sizeof(pattern));
	if (!mkdtemp(root) ||
	    snprintf(archive_copy, sizeof(archive_copy), "%s/%s", root,
		     ORLIX_TCTI_ARM_XML_ARCHIVE_NAME) >= (int)sizeof(archive_copy) ||
	    copy_file(archive, archive_copy))
		goto out;
	if (orlix_tcti_arm_xml_package_validate(archive_copy, release,
						&package) !=
						ORLIX_TCTI_ARM_XML_PACKAGE_OK)
		goto out;
	orlix_tcti_arm_xml_package_destroy(&package);
	if (mutate_last_byte(archive_copy) ||
	    orlix_tcti_arm_xml_package_validate(archive_copy, release,
						&package) !=
						ORLIX_TCTI_ARM_XML_PACKAGE_IDENTITY)
		goto out;
	result = 0;
out:
	unlink(archive_copy);
	rmdir(root);
	return result;
}

static int copy_release(const char *source, const char *destination)
{
	DIR *directory = NULL;
	struct dirent *entry;
	char source_path[PATH_MAX];
	char destination_path[PATH_MAX];
	struct stat status;
	int result = -1;

	if (mkdir(destination, 0700) || !(directory = opendir(source)))
		goto out;
	while ((entry = readdir(directory))) {
		if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
			continue;
		if (snprintf(source_path, sizeof(source_path), "%s/%s", source,
			     entry->d_name) >= (int)sizeof(source_path) ||
		    snprintf(destination_path, sizeof(destination_path), "%s/%s",
			     destination, entry->d_name) >= (int)sizeof(destination_path) ||
		    lstat(source_path, &status))
			goto out;
		if (S_ISREG(status.st_mode)) {
			if (copy_file(source_path, destination_path))
				goto out;
		} else if (S_ISDIR(status.st_mode)) {
			if (mkdir(destination_path, 0700))
				goto out;
		} else {
			goto out;
		}
	}
	result = closedir(directory);
	directory = NULL;
out:
	if (directory)
		closedir(directory);
	return result;
}

static void remove_copied_release(const char *release)
{
	DIR *directory = opendir(release);
	struct dirent *entry;
	char path[PATH_MAX];
	struct stat status;

	if (!directory)
		return;
	while ((entry = readdir(directory))) {
		if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
			continue;
		if (snprintf(path, sizeof(path), "%s/%s", release, entry->d_name) >=
				(int)sizeof(path) || lstat(path, &status))
			continue;
		if (S_ISDIR(status.st_mode))
			rmdir(path);
		else
			unlink(path);
	}
	closedir(directory);
	rmdir(release);
}

static int release_mutation_tests(const char *archive, const char *release)
{
	static const char pattern[] = "/tmp/orlix-arm-xml-release-XXXXXX";
	char root[sizeof(pattern)];
	char release_copy[PATH_MAX] = { 0 };
	char copied_index[PATH_MAX] = { 0 };
	struct orlix_tcti_arm_xml_package package;
	int result = -1;

	memcpy(root, pattern, sizeof(pattern));
	if (!mkdtemp(root) ||
	    snprintf(release_copy, sizeof(release_copy), "%s/%s", root,
		     ORLIX_TCTI_ARM_XML_RELEASE_NAME) >= (int)sizeof(release_copy) ||
	    snprintf(copied_index, sizeof(copied_index), "%s/index.xml",
		     release_copy) >= (int)sizeof(copied_index) ||
	    copy_release(release, release_copy) ||
	    orlix_tcti_arm_xml_package_validate(archive, release_copy, &package) !=
			ORLIX_TCTI_ARM_XML_PACKAGE_OK)
		goto out;
	orlix_tcti_arm_xml_package_destroy(&package);
	if (mutate_last_byte(copied_index) ||
	    orlix_tcti_arm_xml_package_validate(archive, release_copy, &package) !=
			ORLIX_TCTI_ARM_XML_PACKAGE_IDENTITY)
		goto out;
	result = 0;
out:
	remove_copied_release(release_copy);
	rmdir(root);
	return result;
}

int main(int argc, char **argv)
{
	static const char valid[] =
		"<?xml version=\"1.0\"?><root value=\"a&amp;b\"><child/></root>";
	static const char malformed[] = "<root><child></root>";
	static const char bad_entity[] = "<root>&arm_private_entity;</root>";
	static const char doctype[] = "<!DOCTYPE root [<!ENTITY x 'y'>]><root/>";
	static const char duplicate_helper[] =
		"<instructionsection><ps name=\"shared/functions/F\"><pstext/>"
		"</ps><ps name=\"shared/functions/F\"><pstext/></ps>"
		"</instructionsection>";
	static const char missing_helper[] =
		"<instructionsection><ps><pstext/></ps></instructionsection>";
	struct orlix_tcti_arm_xml_package package;
	const struct orlix_tcti_arm_xml_semantic_entry *entry;
	enum orlix_tcti_arm_xml_package_error error;

	if (argc != 3) {
		fprintf(stderr, "usage: %s ISA_A64_xml_A_profile-2026-06.tar.gz "
			"ISA_A64_xml_A_profile-2026-06\n", argv[0]);
		return 2;
	}
	EXPECT(orlix_tcti_arm_xml_document_validate(valid, sizeof(valid) - 1U) ==
	       ORLIX_TCTI_ARM_XML_PACKAGE_OK);
	EXPECT(orlix_tcti_arm_xml_document_validate(malformed,
						    sizeof(malformed) - 1U) ==
	       ORLIX_TCTI_ARM_XML_PACKAGE_MALFORMED_XML);
	EXPECT(orlix_tcti_arm_xml_document_validate(bad_entity,
						    sizeof(bad_entity) - 1U) ==
	       ORLIX_TCTI_ARM_XML_PACKAGE_MALFORMED_XML);
	EXPECT(orlix_tcti_arm_xml_document_validate(doctype,
						    sizeof(doctype) - 1U) ==
	       ORLIX_TCTI_ARM_XML_PACKAGE_MALFORMED_XML);
	EXPECT(orlix_tcti_arm_xml_document_validate(duplicate_helper,
					    sizeof(duplicate_helper) - 1U) ==
	       ORLIX_TCTI_ARM_XML_PACKAGE_DUPLICATE_HELPER);
	EXPECT(orlix_tcti_arm_xml_document_validate(missing_helper,
					    sizeof(missing_helper) - 1U) ==
	       ORLIX_TCTI_ARM_XML_PACKAGE_MISSING_HELPER);
	error = orlix_tcti_arm_xml_package_validate(argv[1], argv[2], &package);
	if (error)
		fprintf(stderr, "official package: %s (ps=%zu functions=%zu forms=%zu)\n",
			orlix_tcti_arm_xml_package_error_name(error),
			package.shared_ps_count, package.shared_anchor_count,
			package.index_form_count);
	EXPECT(error == ORLIX_TCTI_ARM_XML_PACKAGE_OK);
	EXPECT(package.shared_ps_count == ORLIX_TCTI_ARM_XML_SHARED_PS_COUNT);
	EXPECT(package.shared_anchor_count ==
	       ORLIX_TCTI_ARM_XML_SHARED_ANCHOR_COUNT);
	EXPECT(package.index_form_count == ORLIX_TCTI_ARM_XML_INDEX_FORM_COUNT);
	EXPECT(package.entry_count == ORLIX_TCTI_ARM_XML_ENCODING_COUNT);
	entry = orlix_tcti_arm_xml_package_entry(&package, "ABS_32_dp_1src");
	EXPECT(entry != NULL);
	EXPECT(!strcmp(entry->relative_file, "abs.xml"));
	EXPECT(entry->decode.section_count == 1U);
	EXPECT(entry->operation.section_count == 1U);
	EXPECT(strlen(entry->decode.normalized_sha256) == 64U);
	EXPECT(strlen(entry->operation.shared_helpers_sha256) == 64U);
	EXPECT(orlix_tcti_arm_xml_package_entry(&package,
						"SETGOEN_memset_go") == NULL);
	orlix_tcti_arm_xml_package_destroy(&package);
	EXPECT(orlix_tcti_arm_xml_package_validate("/dev/null", argv[2], &package) ==
	       ORLIX_TCTI_ARM_XML_PACKAGE_RELEASE_PATH);
	EXPECT(orlix_tcti_arm_xml_package_validate(argv[1], "/dev/null", &package) ==
	       ORLIX_TCTI_ARM_XML_PACKAGE_RELEASE_PATH);
	EXPECT(!mutation_tests(argv[1], argv[2]));
	EXPECT(!release_mutation_tests(argv[1], argv[2]));
	puts("PASS official Arm A64 XML package validation");
	return 0;
}
