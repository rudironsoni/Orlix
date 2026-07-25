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
		fprintf(stderr, "%s:%d: expectation failed: %s\n", __FILE__, \
			__LINE__, #expression); \
		return -1; \
	} \
} while (0)

static int remove_tree(const char *path)
{
	DIR *directory;
	struct dirent *entry;

	directory = opendir(path);
	if (!directory)
		return errno == ENOENT ? 0 : -1;
	while ((entry = readdir(directory))) {
		char child[PATH_MAX];
		struct stat status;

		if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
			continue;
		if (snprintf(child, sizeof(child), "%s/%s", path, entry->d_name) >=
		    (int)sizeof(child) || lstat(child, &status)) {
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
	closedir(directory);
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

static int digest_file(const char *path, char digest[65])
{
	FILE *file = fopen(path, "rb");
	long length;
	char *data;

	if (!file || fseek(file, 0, SEEK_END) || (length = ftell(file)) < 0 ||
	    fseek(file, 0, SEEK_SET)) {
		if (file)
			fclose(file);
		return -1;
	}
	data = malloc((size_t)length + 1U);
	if (!data || fread(data, 1, (size_t)length, file) != (size_t)length ||
	    fclose(file)) {
		free(data);
		return -1;
	}
	tcti_target_artifact_sha256(data, (size_t)length, digest);
	free(data);
	return 0;
}

static int write_file(const char *path, const char *data, size_t length)
{
	int fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);

	if (fd < 0)
		return -1;
	if (write(fd, data, length) != (ssize_t)length || close(fd)) {
		close(fd);
		return -1;
	}
	return 0;
}

static char *read_text_file(const char *path)
{
	FILE *file = fopen(path, "rb");
	long length;
	char *text;

	if (!file || fseek(file, 0, SEEK_END) || (length = ftell(file)) < 0 ||
	    fseek(file, 0, SEEK_SET)) {
		if (file)
			fclose(file);
		return NULL;
	}
	text = malloc((size_t)length + 1U);
	if (!text || fread(text, 1, (size_t)length, file) != (size_t)length ||
	    fclose(file)) {
		free(text);
		return NULL;
	}
	text[length] = '\0';
	return text;
}

static size_t count_text(const char *text, const char *needle)
{
	size_t count = 0;
	size_t needle_length = strlen(needle);
	const char *cursor = text;

	while ((cursor = strstr(cursor, needle)) != NULL) {
		count++;
		cursor += needle_length;
	}
	return count;
}

static int published_manifest_has_exact_eight_artifacts(const char *root)
{
	static const char *const expected[] = {
		"source_manifest.def",
		"target_asl_availability.def",
		"target_feature_artifact.def",
		"target_feature_field_domain_binding.def",
		"target_runtime_capability_cohort_artifact.def",
		"target_instruction_artifact_generated.h",
		"target_register_artifact.def",
		"target_system_accessor_reconciliation.def",
	};
	char publish[PATH_MAX];
	char current_path[PATH_MAX];
	char manifest_path[PATH_MAX];
	char *current = NULL;
	char *manifest = NULL;
	char *generation;
	char *newline;
	size_t index;
	int status = -1;

	if (snprintf(publish, sizeof(publish), "%s/aarchmrs-2026-06-v2", root) >=
	    (int)sizeof(publish) ||
	    snprintf(current_path, sizeof(current_path), "%s/current", publish) >=
	    (int)sizeof(current_path))
		return -1;
	current = read_text_file(current_path);
	if (!current || strncmp(current, "TCTI_TARGET_ARTIFACT_CURRENT_V1\n"
				    "generation=aarchmrs-2026-06-v2-",
				    strlen("TCTI_TARGET_ARTIFACT_CURRENT_V1\n"
					   "generation=aarchmrs-2026-06-v2-")))
		goto out;
	generation = current + strlen("TCTI_TARGET_ARTIFACT_CURRENT_V1\n"
				      "generation=");
	newline = strchr(generation, '\n');
	if (!newline)
		goto out;
	*newline = '\0';
	if (snprintf(manifest_path, sizeof(manifest_path), "%s/%s/manifest",
		     publish, generation) >= (int)sizeof(manifest_path))
		goto out;
	manifest = read_text_file(manifest_path);
	if (!manifest || !strstr(manifest, "schema=tcti-aarchmrs-source-v2\n") ||
	    count_text(manifest, "artifact=") != ARRAY_SIZE(expected))
		goto out;
	for (index = 0; index < ARRAY_SIZE(expected); index++) {
		char line[TCTI_TARGET_ARTIFACT_MAX_NAME + sizeof("artifact= \n")];

		if (snprintf(line, sizeof(line), "artifact=%s ", expected[index]) >=
		    (int)sizeof(line) || count_text(manifest, line) != 1U)
			goto out;
	}
	status = 0;
out:
	free(manifest);
	free(current);
	return status;
}

static int copy_text_file(const char *source, const char *destination)
{
	char *text = read_text_file(source);
	int result;

	if (!text)
		return -1;
	result = write_file(destination, text, strlen(text));
	free(text);
	return result;
}

static int eighth_canonical_failure_is_attributed_and_atomic(
	const char *canonical_source, const char *instructions,
	const char *features, const char *registers)
{
	static const char *const required[] = {
		"source_manifest.def",
		"target_asl_availability.def",
		"target_feature_artifact.def",
		"target_feature_field_domain_binding.def",
		"target_runtime_capability_cohort_artifact.def",
		"target_instruction_artifact_generated.h",
		"target_register_artifact.def",
		"target_system_accessor_reconciliation.def",
	};
	static const char eighth[] = "target_runtime_capability_cohort_artifact.def";
	char *root = make_root();
	char *canonical = make_root();
	char path[PATH_MAX];
	char source[PATH_MAX];
	struct tcti_target_refresh_result result;
	int root_fd = -1;
	int canonical_fd = -1;
	size_t index;
	int status = -1;

	if (!root || !canonical)
		goto out;
	for (index = 0; index < ARRAY_SIZE(required); index++) {
		if (!strcmp(required[index], eighth))
			continue;
		if (snprintf(source, sizeof(source), "%s/%s", canonical_source,
			     required[index]) >= (int)sizeof(source) ||
		    snprintf(path, sizeof(path), "%s/%s", canonical, required[index]) >=
		    (int)sizeof(path) || copy_text_file(source, path))
			goto out;
	}
	root_fd = open(root, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	canonical_fd = open(canonical, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	if (root_fd < 0 || canonical_fd < 0)
		goto out;
	if (tcti_target_refresh(root_fd, canonical_fd, instructions, features,
				registers, &result) >= 0 ||
	    result.error != TCTI_TARGET_REFRESH_CANONICAL_MISSING ||
	    !result.canonical_artifact || strcmp(result.canonical_artifact, eighth))
		goto out;
	if (snprintf(path, sizeof(path), "%s/aarchmrs-2026-06-v2", root) >=
	    (int)sizeof(path) || access(path, F_OK) == 0)
		goto out;
	if (close(canonical_fd))
		goto out;
	canonical_fd = -1;
	if (snprintf(path, sizeof(path), "%s/%s", canonical, eighth) >=
	    (int)sizeof(path) || write_file(path, "mismatch", sizeof("mismatch") - 1U))
		goto out;
	canonical_fd = open(canonical, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	if (canonical_fd < 0 ||
	    tcti_target_refresh(root_fd, canonical_fd, instructions, features,
				registers, &result) >= 0 ||
	    result.error != TCTI_TARGET_REFRESH_CANONICAL_MISMATCH ||
	    !result.canonical_artifact || strcmp(result.canonical_artifact, eighth))
		goto out;
	if (snprintf(path, sizeof(path), "%s/aarchmrs-2026-06-v2", root) >=
	    (int)sizeof(path) || access(path, F_OK) == 0)
		goto out;
	status = 0;
out:
	if (canonical_fd >= 0)
		close(canonical_fd);
	if (root_fd >= 0)
		close(root_fd);
	if (canonical)
		remove_tree(canonical);
	if (root)
		remove_tree(root);
	free(canonical);
	free(root);
	return status;
}

static int refresh_is_atomic_and_idempotent(const char *canonical,
		const char *instructions,
		const char *features, const char *registers)
{
	char *root = make_root();
	char instruction_digest[65];
	char feature_digest[65];
	char register_digest[65];
	struct tcti_target_artifact_provenance provenance = {
		.schema = "tcti-aarchmrs-source-v2",
		.generator = "tcti-isa-maintainer",
	};
	struct tcti_target_refresh_result result;
	struct tcti_target_artifact_verify_result verify;
	char invalid_path[PATH_MAX];
	int root_fd;
	int canonical_fd;
	int invalid_fd;

	EXPECT(root);
	EXPECT(!digest_file(instructions, instruction_digest));
	EXPECT(!digest_file(features, feature_digest));
	EXPECT(!digest_file(registers, register_digest));
	provenance.instructions_sha256 = instruction_digest;
	provenance.features_sha256 = feature_digest;
	provenance.registers_sha256 = register_digest;
	root_fd = open(root, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	EXPECT(root_fd >= 0);
	canonical_fd = open(canonical, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	EXPECT(canonical_fd >= 0);
	if (tcti_target_refresh(root_fd, canonical_fd, instructions, features,
				registers, &result)) {
		fprintf(stderr, "initial refresh failed: %s%s%s; publish=%s errno=%d stage=%u\n",
			tcti_target_refresh_error_name(result.error),
			result.canonical_artifact ? ": " : "",
			result.canonical_artifact ? result.canonical_artifact : "",
			tcti_target_artifact_publish_error_name(result.publish.error),
			result.publish.system_error, (unsigned int)result.publish.stage);
		return -1;
	}
	EXPECT(result.error == TCTI_TARGET_REFRESH_OK);
	EXPECT(!tcti_target_refresh(root_fd, canonical_fd, instructions, features, registers,
				    NULL));
	EXPECT(!tcti_target_artifact_verify(root_fd, "aarchmrs-2026-06-v2",
					     &provenance, &verify));
	EXPECT(!published_manifest_has_exact_eight_artifacts(root));
	EXPECT(!tcti_target_refresh(root_fd, canonical_fd, instructions, features, registers,
				    &result));
	EXPECT(result.error == TCTI_TARGET_REFRESH_OK);
	EXPECT(snprintf(invalid_path, sizeof(invalid_path), "%s/not-arm.json", root) <
	       (int)sizeof(invalid_path));
	invalid_fd = open(invalid_path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC,
			  0600);
	EXPECT(invalid_fd >= 0);
	EXPECT(write(invalid_fd, "{}", 2) == 2);
	EXPECT(!close(invalid_fd));
	EXPECT(tcti_target_refresh(root_fd, canonical_fd, invalid_path, features, registers,
				   &result) < 0);
	EXPECT(result.error == TCTI_TARGET_REFRESH_MANIFEST);
	EXPECT(!tcti_target_artifact_verify(root_fd, "aarchmrs-2026-06-v2",
					     &provenance, &verify));
	EXPECT(!close(canonical_fd));
	EXPECT(!close(root_fd));
	EXPECT(!remove_tree(root));
	free(root);
	return 0;
}

static int canonical_failures_do_not_publish(const char *instructions,
		const char *features, const char *registers)
{
	char *root = make_root();
	char *canonical = make_root();
	char manifest[PATH_MAX];
	struct tcti_target_refresh_result result;
	int root_fd;
	int canonical_fd;

	EXPECT(root && canonical);
	root_fd = open(root, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	canonical_fd = open(canonical, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	EXPECT(root_fd >= 0 && canonical_fd >= 0);
	EXPECT(tcti_target_refresh(root_fd, canonical_fd, instructions, features,
				   registers, &result) < 0);
	EXPECT(result.error == TCTI_TARGET_REFRESH_CANONICAL_MISSING);
	EXPECT(!strcmp(result.canonical_artifact, "source_manifest.def"));
	EXPECT(snprintf(manifest, sizeof(manifest), "%s/aarchmrs-2026-06-v2", root) <
	       (int)sizeof(manifest));
	EXPECT(access(manifest, F_OK) < 0 && errno == ENOENT);
	EXPECT(!close(canonical_fd));
	EXPECT(snprintf(manifest, sizeof(manifest), "%s/source_manifest.def", canonical) <
	       (int)sizeof(manifest));
	EXPECT(!write_file(manifest, "mismatch", sizeof("mismatch") - 1U));
	canonical_fd = open(canonical, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	EXPECT(canonical_fd >= 0);
	EXPECT(tcti_target_refresh(root_fd, canonical_fd, instructions, features,
				   registers, &result) < 0);
	EXPECT(result.error == TCTI_TARGET_REFRESH_CANONICAL_MISMATCH);
	EXPECT(!strcmp(result.canonical_artifact, "source_manifest.def"));
	EXPECT(access(manifest, F_OK) == 0);
	EXPECT(snprintf(manifest, sizeof(manifest), "%s/aarchmrs-2026-06-v2", root) <
	       (int)sizeof(manifest));
	EXPECT(access(manifest, F_OK) < 0 && errno == ENOENT);
	EXPECT(!close(canonical_fd));
	EXPECT(!close(root_fd));
	EXPECT(!remove_tree(canonical));
	EXPECT(!remove_tree(root));
	free(canonical);
	free(root);
	return 0;
}

int main(int argc, char **argv)
{
	if (argc != 5) {
		fprintf(stderr, "usage: %s CANONICAL_DIR Instructions.json Features.json Registers.json\n",
			argv[0]);
		return 2;
	}
	if (refresh_is_atomic_and_idempotent(argv[1], argv[2], argv[3], argv[4]) ||
	    canonical_failures_do_not_publish(argv[2], argv[3], argv[4]) ||
	    eighth_canonical_failure_is_attributed_and_atomic(argv[1], argv[2],
						  argv[3], argv[4]))
		return 1;
	puts("PASS target refresh transaction");
	return 0;
}
