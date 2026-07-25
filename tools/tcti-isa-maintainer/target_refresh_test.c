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

static int refresh_is_atomic_and_idempotent(const char *canonical,
		const char *instructions,
		const char *features, const char *registers)
{
	char *root = make_root();
	char instruction_digest[65];
	char feature_digest[65];
	char register_digest[65];
	struct tcti_target_artifact_provenance provenance = {
		.schema = "tcti-aarchmrs-source-v1",
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
	EXPECT(!tcti_target_refresh(root_fd, canonical_fd, instructions, features, registers,
				    &result));
	EXPECT(result.error == TCTI_TARGET_REFRESH_OK);
	EXPECT(!tcti_target_refresh(root_fd, canonical_fd, instructions, features, registers,
				    NULL));
	EXPECT(!tcti_target_artifact_verify(root_fd, "aarchmrs-2026-06",
					     &provenance, &verify));
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
	EXPECT(!tcti_target_artifact_verify(root_fd, "aarchmrs-2026-06",
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
	EXPECT(snprintf(manifest, sizeof(manifest), "%s/aarchmrs-2026-06", root) <
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
	EXPECT(snprintf(manifest, sizeof(manifest), "%s/aarchmrs-2026-06", root) <
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
	    canonical_failures_do_not_publish(argv[2], argv[3], argv[4]))
		return 1;
	puts("PASS target refresh transaction");
	return 0;
}
