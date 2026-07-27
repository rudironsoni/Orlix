/* SPDX-License-Identifier: GPL-2.0-only */
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE

#include "target_refresh.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef O_DIRECTORY
#define O_DIRECTORY 0
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif

#define ARRAY_SIZE(values) (sizeof(values) / sizeof((values)[0]))
#define EXPECT(expression) do { \
	if (!(expression)) { \
		fprintf(stderr, "%s:%d: expectation failed: %s\n", \
			__FILE__, __LINE__, #expression); \
		return -1; \
	} \
} while (0)

static const struct orlix_tcti_target_artifact_provenance pinned_provenance = {
	.schema = "orlix-tcti-aarchmrs-source-v2",
	.generator = "orlix-tcti-isa-maintainer",
	.instructions_sha256 =
		"a1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe",
	.features_sha256 =
		"633259000ffd3da32900bd0c0c1beae4a9eea7095c278f74d62a00c846b41187",
	.registers_sha256 =
		"5bd76c3c3ce90322eb4fd179675dafe82df2fd1cb789beee516e5b29c471b874",
};

static int remove_tree(const char *path)
{
	DIR *directory = opendir(path);
	struct dirent *entry;

	if (!directory)
		return errno == ENOENT ? 0 : -1;
	while ((entry = readdir(directory))) {
		char child[PATH_MAX];
		struct stat status;

		if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
			continue;
		if (snprintf(child, sizeof(child), "%s/%s", path,
			     entry->d_name) >= (int)sizeof(child) ||
		    lstat(child, &status)) {
			closedir(directory);
			return -1;
		}
		if (S_ISDIR(status.st_mode)) {
			if (chmod(child, 0755) || remove_tree(child)) {
				closedir(directory);
				return -1;
			}
		} else if (unlink(child)) {
			closedir(directory);
			return -1;
		}
	}
	if (closedir(directory))
		return -1;
	return rmdir(path);
}

static char *make_root(void)
{
	static const char pattern[] = "/tmp/orlix-tcti-refresh-XXXXXX";
	char *root = malloc(sizeof(pattern));

	if (!root)
		return NULL;
	memcpy(root, pattern, sizeof(pattern));
	if (!mkdtemp(root)) {
		free(root);
		return NULL;
	}
	return root;
}

static int selected_generation(const char *root,
			       char generation[ORLIX_TCTI_TARGET_ARTIFACT_MAX_GENERATION + 1U])
{
	char path[PATH_MAX];
	ssize_t length;

	if (snprintf(path, sizeof(path), "%s/generations/current", root) >=
	    (int)sizeof(path))
		return -1;
	length = readlink(path, generation,
			  ORLIX_TCTI_TARGET_ARTIFACT_MAX_GENERATION);
	if (length < 0 ||
	    length > (ssize_t)ORLIX_TCTI_TARGET_ARTIFACT_MAX_GENERATION)
		return -1;
	generation[length] = '\0';
	return 0;
}

static char *read_file(const char *path, size_t *length)
{
	FILE *file = fopen(path, "rb");
	long size;
	char *data;

	if (!file || fseek(file, 0, SEEK_END) || (size = ftell(file)) < 0 ||
	    fseek(file, 0, SEEK_SET)) {
		if (file)
			fclose(file);
		return NULL;
	}
	data = malloc((size_t)size + 1U);
	if (!data || fread(data, 1, (size_t)size, file) != (size_t)size) {
		free(data);
		fclose(file);
		return NULL;
	}
	if (fclose(file)) {
		free(data);
		return NULL;
	}
	data[size] = '\0';
	*length = (size_t)size;
	return data;
}

static int generation_entry_count(const char *root)
{
	char path[PATH_MAX];
	DIR *directory;
	struct dirent *entry;
	int count = 0;

	if (snprintf(path, sizeof(path), "%s/generations", root) >=
	    (int)sizeof(path))
		return -1;
	directory = opendir(path);
	if (!directory)
		return -1;
	while ((entry = readdir(directory)))
		if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, ".."))
			count++;
	closedir(directory);
	return count;
}

static int seed_prior(int canonical_fd,
		      struct orlix_tcti_target_artifact_publish_result *result)
{
	static const char prior_data[] = "prior immutable artifact\n";
	static const struct orlix_tcti_target_artifact prior_artifact = {
		.name = "prior.def",
		.data = prior_data,
		.length = sizeof(prior_data) - 1U,
	};
	static const struct orlix_tcti_target_artifact_provenance provenance = {
		.schema = "orlix-tcti-aarchmrs-source-v2",
		.generator = "orlix-tcti-isa-maintainer",
		.instructions_sha256 =
			"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
		.features_sha256 =
			"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
		.registers_sha256 =
			"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc",
	};

	return orlix_tcti_target_artifact_publish(
		canonical_fd, "generations", "prior", &prior_artifact, 1U,
		&provenance, NULL, result);
}

static int full_refresh_is_authoritative_and_idempotent(
	const char *instructions, const char *features, const char *registers,
	const char *arm_xml_archive, const char *arm_xml_release)
{
	static const char expected_generation[] =
		"aarchmrs-2026-06-v2-6f9d82e8bcd3aa2471d83caae4dffdd3a43983815aec0843a5581ed05e11a877";
	char *root = make_root();
	char first[ORLIX_TCTI_TARGET_ARTIFACT_MAX_GENERATION + 1U];
	char second[ORLIX_TCTI_TARGET_ARTIFACT_MAX_GENERATION + 1U];
	struct orlix_tcti_target_refresh_result result;
	struct orlix_tcti_target_artifact_verify_result verify_result;
	int fd;
	int refresh_status;

	EXPECT(root);
	fd = open(root, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	EXPECT(fd >= 0);
	refresh_status = orlix_tcti_target_refresh(fd, instructions, features,
		registers, arm_xml_archive, arm_xml_release, &result);
	if (refresh_status)
		fprintf(stderr, "refresh failed: %s / %s / %s\n",
			orlix_tcti_target_refresh_error_name(result.error),
			orlix_tcti_arm_xml_package_error_name(result.arm_xml_error),
			orlix_tcti_target_artifact_publish_error_name(
				result.publish.error));
	EXPECT(!refresh_status);
	EXPECT(result.error == ORLIX_TCTI_TARGET_REFRESH_OK);
	EXPECT(!strcmp(result.publish.generation, expected_generation));
	EXPECT(!selected_generation(root, first));
	EXPECT(!strcmp(first, expected_generation));
	EXPECT(!orlix_tcti_target_artifact_verify(
		fd, "generations", &pinned_provenance, &verify_result));
	EXPECT(!orlix_tcti_target_refresh(fd, instructions, features, registers,
					  arm_xml_archive, arm_xml_release,
					  &result));
	EXPECT(result.error == ORLIX_TCTI_TARGET_REFRESH_OK);
	EXPECT(!selected_generation(root, second));
	EXPECT(!strcmp(first, second));
	EXPECT(generation_entry_count(root) == 2);
	EXPECT(!close(fd));
	EXPECT(!remove_tree(root));
	free(root);
	return 0;
}

static int injected_failure_preserves_prior(
	enum orlix_tcti_target_refresh_fault_stage refresh_stage,
	enum orlix_tcti_target_artifact_publish_stage publish_stage,
	const char *instructions, const char *features, const char *registers,
	const char *arm_xml_archive, const char *arm_xml_release)
{
	char *root = make_root();
	char before[ORLIX_TCTI_TARGET_ARTIFACT_MAX_GENERATION + 1U];
	char after[ORLIX_TCTI_TARGET_ARTIFACT_MAX_GENERATION + 1U];
	char prior_path[PATH_MAX];
	char *before_bytes = NULL;
	char *after_bytes = NULL;
	size_t before_length = 0;
	size_t after_length = 0;
	struct orlix_tcti_target_artifact_publish_result prior;
	struct orlix_tcti_target_refresh_result result;
	struct orlix_tcti_target_refresh_fault fault = {
		.stage = refresh_stage,
		.publication = {
			.stage = publish_stage,
		},
	};
	int fd = -1;
	int status = -1;

	if (!root)
		goto out;
	fd = open(root, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	if (fd < 0 || seed_prior(fd, &prior) ||
	    selected_generation(root, before) ||
	    snprintf(prior_path, sizeof(prior_path),
		     "%s/generations/%s/prior.def", root, prior.generation) >=
		    (int)sizeof(prior_path))
		goto out;
	before_bytes = read_file(prior_path, &before_length);
	if (!before_bytes)
		goto out;
	if (orlix_tcti_target_refresh_with_fault(
		    fd, instructions, features, registers, arm_xml_archive,
		    arm_xml_release, &fault, &result) >= 0)
		goto out;
	if ((refresh_stage == ORLIX_TCTI_TARGET_REFRESH_FAULT_PARSE &&
	     result.error != ORLIX_TCTI_TARGET_REFRESH_PARSE) ||
	    (refresh_stage == ORLIX_TCTI_TARGET_REFRESH_FAULT_VALIDATION &&
	     result.error != ORLIX_TCTI_TARGET_REFRESH_VALIDATION) ||
	    (refresh_stage == ORLIX_TCTI_TARGET_REFRESH_FAULT_PUBLICATION &&
	     result.error != ORLIX_TCTI_TARGET_REFRESH_PUBLISH))
		goto out;
	if (selected_generation(root, after) || strcmp(before, after))
		goto out;
	after_bytes = read_file(prior_path, &after_length);
	if (!after_bytes || before_length != after_length ||
	    memcmp(before_bytes, after_bytes, before_length) ||
	    generation_entry_count(root) != 2)
		goto out;
	status = 0;
out:
	free(after_bytes);
	free(before_bytes);
	if (fd >= 0)
		close(fd);
	if (root)
		remove_tree(root);
	free(root);
	return status;
}

static int all_injected_failures_are_atomic(
	const char *instructions, const char *features, const char *registers,
	const char *arm_xml_archive, const char *arm_xml_release)
{
	size_t artifact;

	EXPECT(!injected_failure_preserves_prior(
		ORLIX_TCTI_TARGET_REFRESH_FAULT_PARSE,
		ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE, instructions, features,
		registers, arm_xml_archive, arm_xml_release));
	for (artifact = 0; artifact < 8U; artifact++) {
		char *root = make_root();
		char before[ORLIX_TCTI_TARGET_ARTIFACT_MAX_GENERATION + 1U];
		char after[ORLIX_TCTI_TARGET_ARTIFACT_MAX_GENERATION + 1U];
		struct orlix_tcti_target_artifact_publish_result prior;
		struct orlix_tcti_target_refresh_result result;
		struct orlix_tcti_target_refresh_fault fault = {
			.stage = ORLIX_TCTI_TARGET_REFRESH_FAULT_VALIDATION,
			.validation_artifact = artifact,
		};
		int fd;

		EXPECT(root);
		fd = open(root, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
		EXPECT(fd >= 0);
		EXPECT(!seed_prior(fd, &prior));
		EXPECT(!selected_generation(root, before));
		EXPECT(orlix_tcti_target_refresh_with_fault(
			fd, instructions, features, registers, arm_xml_archive,
			arm_xml_release, &fault, &result) < 0);
		EXPECT(result.error == ORLIX_TCTI_TARGET_REFRESH_VALIDATION);
		EXPECT(!selected_generation(root, after));
		EXPECT(!strcmp(before, after));
		EXPECT(generation_entry_count(root) == 2);
		EXPECT(!close(fd));
		EXPECT(!remove_tree(root));
		free(root);
	}
	EXPECT(!injected_failure_preserves_prior(
		ORLIX_TCTI_TARGET_REFRESH_FAULT_PUBLICATION,
		ORLIX_TCTI_TARGET_ARTIFACT_STAGE_SELECTOR_SYNC, instructions,
		features, registers, arm_xml_archive, arm_xml_release));
	return 0;
}

static int wrong_source_identity_does_not_publish(
	const char *features, const char *registers,
	const char *arm_xml_archive, const char *arm_xml_release)
{
	char *root = make_root();
	char before[ORLIX_TCTI_TARGET_ARTIFACT_MAX_GENERATION + 1U];
	char after[ORLIX_TCTI_TARGET_ARTIFACT_MAX_GENERATION + 1U];
	struct orlix_tcti_target_artifact_publish_result prior;
	struct orlix_tcti_target_refresh_result result;
	int fd;

	EXPECT(root);
	fd = open(root, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	EXPECT(fd >= 0);
	EXPECT(!seed_prior(fd, &prior));
	EXPECT(!selected_generation(root, before));
	EXPECT(orlix_tcti_target_refresh(fd, "/dev/null", features, registers,
					 arm_xml_archive, arm_xml_release,
					 &result) < 0);
	EXPECT(result.error == ORLIX_TCTI_TARGET_REFRESH_SOURCE_IDENTITY);
	EXPECT(!selected_generation(root, after));
	EXPECT(!strcmp(before, after));
	EXPECT(generation_entry_count(root) == 2);
	EXPECT(!close(fd));
	EXPECT(!remove_tree(root));
	free(root);
	return 0;
}

int main(int argc, char **argv)
{
	if (argc != 6) {
		fprintf(stderr,
			"usage: %s Instructions.json Features.json Registers.json ISA_A64_xml_A_profile-2026-06.tar.gz ISA_A64_xml_A_profile-2026-06\n",
			argv[0]);
		return 2;
	}
	if (full_refresh_is_authoritative_and_idempotent(
		    argv[1], argv[2], argv[3], argv[4], argv[5]) ||
	    all_injected_failures_are_atomic(argv[1], argv[2], argv[3],
					     argv[4], argv[5]) ||
	    wrong_source_identity_does_not_publish(argv[2], argv[3], argv[4],
						   argv[5]))
		return 1;
	puts("PASS target refresh authoritative transaction");
	return 0;
}
