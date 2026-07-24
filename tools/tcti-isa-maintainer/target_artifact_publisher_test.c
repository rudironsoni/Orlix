/* SPDX-License-Identifier: GPL-2.0-only */
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "target_artifact_publisher.c"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define EXPECT(expression) do { \
	if (!(expression)) { \
		fprintf(stderr, "%s:%d: expectation failed: %s\n", __FILE__, \
			__LINE__, #expression); \
		return -1; \
	} \
} while (0)

static const struct tcti_target_artifact artifacts[] = {
	{ .name = "features.h", .data = "features-v1\n", .length = 12 },
	{ .name = "instructions.h", .data = "instructions-v1\n", .length = 16 },
	{ .name = "registers.h", .data = "registers-v1\n", .length = 13 },
};
static const struct tcti_target_artifact_provenance provenance = {
	.schema = "tcti-target-artifact-v2",
	.generator = "target-artifact-generator",
	.instructions_sha256 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
	.features_sha256 = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
	.registers_sha256 = "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc",
};

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
		if (snprintf(child, sizeof(child), "%s/%s", path,
			     entry->d_name) >= (int)sizeof(child)) {
			closedir(directory);
			return -1;
		}
		if (lstat(child, &status)) {
			closedir(directory);
			return -1;
		}
		if (S_ISDIR(status.st_mode)) {
			if (chmod(child, 0755)) {
				closedir(directory);
				return -1;
			}
			if (remove_tree(child)) {
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
	static const char pattern[] = "/tmp/orlix-tcti-publisher-XXXXXX";
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

static char *read_file(const char *path, size_t *length)
{
	FILE *file;
	long size;
	char *data;

	file = fopen(path, "rb");
	if (!file || fseek(file, 0, SEEK_END) || (size = ftell(file)) < 0 ||
	    fseek(file, 0, SEEK_SET)) {
		if (file)
			fclose(file);
		return NULL;
	}
	data = malloc((size_t)size + 1);
	if (!data || fread(data, 1, (size_t)size, file) != (size_t)size) {
		free(data);
		fclose(file);
		return NULL;
	}
	fclose(file);
	data[size] = '\0';
	*length = (size_t)size;
	return data;
}

static int selector_is(const char *root, const char *generation)
{
	char path[PATH_MAX];
	char expected[256];
	char *contents;
	size_t length;
	int result;

	if (snprintf(path, sizeof(path), "%s/target/current", root) >=
	    (int)sizeof(path) ||
	    snprintf(expected, sizeof(expected),
		     "TCTI_TARGET_ARTIFACT_CURRENT_V1\ngeneration=%s\nmanifest=manifest\n",
		     generation) >= (int)sizeof(expected))
		return -1;
	contents = read_file(path, &length);
	result = contents && length == strlen(expected) &&
		!memcmp(contents, expected, length) ? 0 : -1;
	free(contents);
	return result;
}

static int file_matches(const char *root, const char *generation,
			const struct tcti_target_artifact *artifact)
{
	char path[PATH_MAX];
	char *contents;
	size_t length;
	int result;

	if (snprintf(path, sizeof(path), "%s/target/%s/%s", root, generation,
		     artifact->name) >= (int)sizeof(path))
		return -1;
	contents = read_file(path, &length);
	result = contents && length == artifact->length &&
		!memcmp(contents, artifact->data, length) ? 0 : -1;
	free(contents);
	return result;
}

static int publish(const char *root, const char *generation,
		   enum tcti_target_artifact_publish_stage stage,
		   struct tcti_target_artifact_publish_result *result)
{
	struct tcti_target_artifact_publish_fault fault = { .stage = stage };
	int root_fd;
	int published;

	root_fd = open(root, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	if (root_fd < 0)
		return -1;
	published = tcti_target_artifact_publish(root_fd, "target", generation, artifacts,
		sizeof(artifacts) / sizeof(artifacts[0]),
		&provenance, stage == TCTI_TARGET_ARTIFACT_STAGE_NONE ? NULL : &fault,
		result);
	close(root_fd);
	return published;
}

static int publish_bytewise(const char *root, const char *generation,
			    struct tcti_target_artifact_publish_result *result)
{
	struct tcti_target_artifact_publish_fault fault = {
		.maximum_write_bytes = 1,
	};
	int root_fd;
	int published;

	root_fd = open(root, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	if (root_fd < 0)
		return -1;
	published = tcti_target_artifact_publish(root_fd, "target", generation,
		artifacts, sizeof(artifacts) / sizeof(artifacts[0]), &provenance,
		&fault, result);
	close(root_fd);
	return published;
}

static int verify(const char *root,
		  const struct tcti_target_artifact_provenance *expected,
		  struct tcti_target_artifact_verify_result *result)
{
	int root_fd;
	int verified;

	root_fd = open(root, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	if (root_fd < 0)
		return -1;
	verified = tcti_target_artifact_verify(root_fd, "target", expected,
					       result);
	close(root_fd);
	return verified;
}

static int overwrite_file(const char *path, const void *data, size_t length)
{
	const unsigned char *bytes = data;
	int fd;

	if (chmod(path, 0644))
		return -1;
	fd = open(path, O_WRONLY | O_TRUNC | O_CLOEXEC);
	if (fd < 0)
		return -1;
	while (length) {
		ssize_t written = write(fd, bytes, length);

		if (written < 0 && errno == EINTR)
			continue;
		if (written <= 0) {
			close(fd);
			return -1;
		}
		bytes += written;
		length -= (size_t)written;
	}
	return close(fd);
}

static int deterministic_immutable_publication(void)
{
	char *left = make_root();
	char *right = make_root();
	struct tcti_target_artifact_publish_result result;
	size_t index;
	char left_path[PATH_MAX];
	char right_path[PATH_MAX];
	char *left_data;
	char *right_data;
	size_t left_length;
	size_t right_length;
	struct stat status;

	EXPECT(left && right);
	EXPECT(!publish(left, "2026-06-a1", TCTI_TARGET_ARTIFACT_STAGE_NONE,
			&result));
	EXPECT(result.error == TCTI_TARGET_ARTIFACT_PUBLISH_OK);
	EXPECT(!publish(right, "2026-06-a1", TCTI_TARGET_ARTIFACT_STAGE_NONE,
			&result));
	for (index = 0; index < sizeof(artifacts) / sizeof(artifacts[0]); index++) {
		EXPECT(!file_matches(left, "2026-06-a1", &artifacts[index]));
		EXPECT(snprintf(left_path, sizeof(left_path), "%s/target/%s/%s", left,
				"2026-06-a1", artifacts[index].name) <
			(int)sizeof(left_path));
		EXPECT(snprintf(right_path, sizeof(right_path), "%s/target/%s/%s", right,
				"2026-06-a1", artifacts[index].name) <
			(int)sizeof(right_path));
		left_data = read_file(left_path, &left_length);
		right_data = read_file(right_path, &right_length);
		EXPECT(left_data && right_data);
		EXPECT(left_length == right_length && !memcmp(left_data, right_data,
			left_length));
		free(left_data);
		free(right_data);
		EXPECT(!stat(left_path, &status));
		EXPECT((status.st_mode & 0222) == 0);
	}
	EXPECT(snprintf(left_path, sizeof(left_path), "%s/target/2026-06-a1/manifest",
			left) < (int)sizeof(left_path));
	left_data = read_file(left_path, &left_length);
	EXPECT(left_data);
	EXPECT(strstr(left_data, "schema=tcti-target-artifact-v2\n"));
	EXPECT(strstr(left_data, "generator=target-artifact-generator\n"));
	EXPECT(strstr(left_data, provenance.instructions_sha256));
	EXPECT(strstr(left_data, provenance.features_sha256));
	EXPECT(strstr(left_data, provenance.registers_sha256));
	EXPECT(strstr(left_data, "bundle_sha256="));
	free(left_data);
	EXPECT(!selector_is(left, "2026-06-a1"));
	EXPECT(!selector_is(right, "2026-06-a1"));
	EXPECT(publish(left, "2026-06-a1", TCTI_TARGET_ARTIFACT_STAGE_NONE,
		       &result) < 0);
	EXPECT(result.error == TCTI_TARGET_ARTIFACT_PUBLISH_ALREADY_EXISTS);
	EXPECT(!remove_tree(left));
	EXPECT(!remove_tree(right));
	free(left);
	free(right);
	return 0;
}

static enum tcti_target_artifact_publish_error expected_error(
	enum tcti_target_artifact_publish_stage stage)
{
	switch (stage) {
	case TCTI_TARGET_ARTIFACT_STAGE_TEMP_OPEN:
		return TCTI_TARGET_ARTIFACT_PUBLISH_TEMP_OPEN;
	case TCTI_TARGET_ARTIFACT_STAGE_WRITE:
		return TCTI_TARGET_ARTIFACT_PUBLISH_WRITE;
	case TCTI_TARGET_ARTIFACT_STAGE_FILE_SYNC:
		return TCTI_TARGET_ARTIFACT_PUBLISH_FILE_SYNC;
	case TCTI_TARGET_ARTIFACT_STAGE_FILE_LOCK_SYNC:
		return TCTI_TARGET_ARTIFACT_PUBLISH_FILE_LOCK_SYNC;
	case TCTI_TARGET_ARTIFACT_STAGE_FILE_CLOSE:
		return TCTI_TARGET_ARTIFACT_PUBLISH_FILE_CLOSE;
	case TCTI_TARGET_ARTIFACT_STAGE_RENAME:
		return TCTI_TARGET_ARTIFACT_PUBLISH_RENAME;
	case TCTI_TARGET_ARTIFACT_STAGE_MANIFEST:
		return TCTI_TARGET_ARTIFACT_PUBLISH_MANIFEST;
	case TCTI_TARGET_ARTIFACT_STAGE_GENERATION_SYNC:
		return TCTI_TARGET_ARTIFACT_PUBLISH_GENERATION_SYNC;
	case TCTI_TARGET_ARTIFACT_STAGE_LOCK_GENERATION:
		return TCTI_TARGET_ARTIFACT_PUBLISH_LOCK_GENERATION;
	case TCTI_TARGET_ARTIFACT_STAGE_LOCK_SYNC:
		return TCTI_TARGET_ARTIFACT_PUBLISH_LOCK_SYNC;
	case TCTI_TARGET_ARTIFACT_STAGE_SELECTOR:
		return TCTI_TARGET_ARTIFACT_PUBLISH_SELECTOR;
	case TCTI_TARGET_ARTIFACT_STAGE_SELECTOR_SYNC:
		return TCTI_TARGET_ARTIFACT_PUBLISH_SELECTOR_SYNC;
	case TCTI_TARGET_ARTIFACT_STAGE_NONE:
		break;
	}
	return TCTI_TARGET_ARTIFACT_PUBLISH_INVALID_ARGUMENT;
}

static int failures_preserve_prior_selector(void)
{
	static const enum tcti_target_artifact_publish_stage stages[] = {
		TCTI_TARGET_ARTIFACT_STAGE_TEMP_OPEN,
		TCTI_TARGET_ARTIFACT_STAGE_WRITE,
		TCTI_TARGET_ARTIFACT_STAGE_FILE_SYNC,
		TCTI_TARGET_ARTIFACT_STAGE_FILE_LOCK_SYNC,
		TCTI_TARGET_ARTIFACT_STAGE_FILE_CLOSE,
		TCTI_TARGET_ARTIFACT_STAGE_RENAME,
		TCTI_TARGET_ARTIFACT_STAGE_MANIFEST,
		TCTI_TARGET_ARTIFACT_STAGE_GENERATION_SYNC,
		TCTI_TARGET_ARTIFACT_STAGE_LOCK_GENERATION,
		TCTI_TARGET_ARTIFACT_STAGE_LOCK_SYNC,
		TCTI_TARGET_ARTIFACT_STAGE_SELECTOR,
	};
	char *root = make_root();
	struct tcti_target_artifact_publish_result result;
	size_t index;
	char selected[64] = "prior";

	EXPECT(root);
	EXPECT(!publish(root, "prior", TCTI_TARGET_ARTIFACT_STAGE_NONE, &result));
	for (index = 0; index < sizeof(stages) / sizeof(stages[0]); index++) {
		char generation[64];

		EXPECT(snprintf(generation, sizeof(generation), "failed-%zu", index) <
			(int)sizeof(generation));
		EXPECT(publish(root, generation, stages[index], &result) < 0);
		EXPECT(result.error == expected_error(stages[index]));
		EXPECT(!selector_is(root, selected));
		/* The unselected partial set was discarded, so retry is clean. */
		EXPECT(!publish(root, generation, TCTI_TARGET_ARTIFACT_STAGE_NONE,
				&result));
		EXPECT(!selector_is(root, generation));
		memcpy(selected, generation, strlen(generation) + 1);
	}
	EXPECT(!remove_tree(root));
	free(root);
	return 0;
}

static int short_writes_publish_complete_generation(void)
{
	char *root = make_root();
	struct tcti_target_artifact_publish_result result;
	size_t index;

	EXPECT(root);
	EXPECT(!publish_bytewise(root, "bytewise", &result));
	EXPECT(!selector_is(root, "bytewise"));
	for (index = 0; index < sizeof(artifacts) / sizeof(artifacts[0]); index++)
		EXPECT(!file_matches(root, "bytewise", &artifacts[index]));
	EXPECT(!remove_tree(root));
	free(root);
	return 0;
}

static int selector_sync_is_explicit_post_publish_boundary(void)
{
	char *root = make_root();
	struct tcti_target_artifact_publish_result result;

	EXPECT(root);
	EXPECT(!publish(root, "prior", TCTI_TARGET_ARTIFACT_STAGE_NONE, &result));
	EXPECT(publish(root, "new", TCTI_TARGET_ARTIFACT_STAGE_SELECTOR_SYNC,
		       &result) < 0);
	EXPECT(result.error == TCTI_TARGET_ARTIFACT_PUBLISH_SELECTOR_SYNC);
	EXPECT(!selector_is(root, "new"));
	EXPECT(!remove_tree(root));
	free(root);
	return 0;
}

static int names_are_not_paths_or_implicit_order(void)
{
	char *root = make_root();
	struct tcti_target_artifact_publish_result result;
	struct tcti_target_artifact invalid[] = {
		{ .name = "../source", .data = "x", .length = 1 },
	};
	struct tcti_target_artifact duplicate[] = {
		{ .name = "a", .data = "x", .length = 1 },
		{ .name = "a", .data = "y", .length = 1 },
	};
	struct tcti_target_artifact reserved[] = {
		{ .name = "manifest", .data = "x", .length = 1 },
	};

	EXPECT(root);
	{
		int root_fd = open(root, O_RDONLY | O_DIRECTORY | O_CLOEXEC);

		EXPECT(root_fd >= 0);
		EXPECT(tcti_target_artifact_publish(root_fd, "target", "bad", invalid,
		       1, &provenance, NULL, &result) < 0);
		EXPECT(result.error == TCTI_TARGET_ARTIFACT_PUBLISH_INVALID_NAME);
		EXPECT(tcti_target_artifact_publish(root_fd, "target", "bad", reserved,
		       1, &provenance, NULL, &result) < 0);
		EXPECT(result.error == TCTI_TARGET_ARTIFACT_PUBLISH_INVALID_NAME);
		EXPECT(tcti_target_artifact_publish(root_fd, "target", "bad", duplicate,
		       2, &provenance, NULL, &result) < 0);
		EXPECT(result.error == TCTI_TARGET_ARTIFACT_PUBLISH_INVALID_NAME);
		EXPECT(tcti_target_artifact_publish(root_fd, "../relative", "bad", artifacts,
		       sizeof(artifacts) / sizeof(artifacts[0]), &provenance, NULL,
		       &result) < 0);
		close(root_fd);
	}
	EXPECT(result.error == TCTI_TARGET_ARTIFACT_PUBLISH_INVALID_ARGUMENT);
	EXPECT(!remove_tree(root));
	free(root);
	return 0;
}

static int symlink_publish_root_is_rejected(void)
{
	char *root = make_root();
	char outside[PATH_MAX];
	char link[PATH_MAX];
	struct tcti_target_artifact_publish_result result;
	int root_fd;

	EXPECT(root);
	EXPECT(snprintf(outside, sizeof(outside), "%s/outside", root) <
	       (int)sizeof(outside));
	EXPECT(snprintf(link, sizeof(link), "%s/linked", root) < (int)sizeof(link));
	EXPECT(!mkdir(outside, 0700));
	EXPECT(!symlink(outside, link));
	root_fd = open(root, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	EXPECT(root_fd >= 0);
	EXPECT(tcti_target_artifact_publish(root_fd, "linked", "bad", artifacts,
	       sizeof(artifacts) / sizeof(artifacts[0]), &provenance, NULL,
	       &result) < 0);
	EXPECT(result.error == TCTI_TARGET_ARTIFACT_PUBLISH_OPEN_ROOT);
	close(root_fd);
	EXPECT(snprintf(link, sizeof(link), "%s/target/current", root) <
	       (int)sizeof(link));
	EXPECT(access(link, F_OK) < 0 && errno == ENOENT);
	EXPECT(!remove_tree(root));
	free(root);
	return 0;
}

static int valid_generation_verifies(void)
{
	char *root = make_root();
	struct tcti_target_artifact_publish_result publish_result;
	struct tcti_target_artifact_verify_result verify_result;

	EXPECT(root);
	EXPECT(!publish(root, "verified", TCTI_TARGET_ARTIFACT_STAGE_NONE,
			&publish_result));
	EXPECT(!verify(root, &provenance, &verify_result));
	EXPECT(verify_result.error == TCTI_TARGET_ARTIFACT_VERIFY_OK);
	EXPECT(!remove_tree(root));
	free(root);
	return 0;
}

static int artifact_content_tampering_is_rejected(void)
{
	char *root = make_root();
	char path[PATH_MAX];
	char replacement[] = "Features-v1\n";
	struct tcti_target_artifact_publish_result publish_result;
	struct tcti_target_artifact_verify_result verify_result;

	EXPECT(root);
	EXPECT(!publish(root, "tampered", TCTI_TARGET_ARTIFACT_STAGE_NONE,
			&publish_result));
	EXPECT(snprintf(path, sizeof(path), "%s/target/tampered/features.h", root) <
	       (int)sizeof(path));
	EXPECT(!overwrite_file(path, replacement, sizeof(replacement) - 1));
	EXPECT(verify(root, &provenance, &verify_result) < 0);
	EXPECT(verify_result.error ==
	       TCTI_TARGET_ARTIFACT_VERIFY_ARTIFACT_DIGEST);
	EXPECT(!strcmp(verify_result.artifact, "features.h"));
	EXPECT(!remove_tree(root));
	free(root);
	return 0;
}

static int missing_and_extra_artifacts_are_rejected(void)
{
	char *missing_root = make_root();
	char *extra_root = make_root();
	char path[PATH_MAX];
	int fd;
	struct tcti_target_artifact_publish_result publish_result;
	struct tcti_target_artifact_verify_result verify_result;

	EXPECT(missing_root && extra_root);
	EXPECT(!publish(missing_root, "missing", TCTI_TARGET_ARTIFACT_STAGE_NONE,
			&publish_result));
	EXPECT(snprintf(path, sizeof(path), "%s/target/missing", missing_root) <
	       (int)sizeof(path));
	EXPECT(!chmod(path, 0755));
	EXPECT(snprintf(path, sizeof(path), "%s/target/missing/features.h",
			missing_root) < (int)sizeof(path));
	EXPECT(!unlink(path));
	EXPECT(verify(missing_root, &provenance, &verify_result) < 0);
	EXPECT(verify_result.error == TCTI_TARGET_ARTIFACT_VERIFY_ARTIFACT_OPEN);

	EXPECT(!publish(extra_root, "extra", TCTI_TARGET_ARTIFACT_STAGE_NONE,
			&publish_result));
	EXPECT(snprintf(path, sizeof(path), "%s/target/extra", extra_root) <
	       (int)sizeof(path));
	EXPECT(!chmod(path, 0755));
	EXPECT(snprintf(path, sizeof(path), "%s/target/extra/unmanifested.h",
			extra_root) < (int)sizeof(path));
	fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
	EXPECT(fd >= 0);
	EXPECT(!close(fd));
	EXPECT(verify(extra_root, &provenance, &verify_result) < 0);
	EXPECT(verify_result.error == TCTI_TARGET_ARTIFACT_VERIFY_EXTRA_ARTIFACT);
	EXPECT(!strcmp(verify_result.artifact, "unmanifested.h"));

	EXPECT(!remove_tree(missing_root));
	EXPECT(!remove_tree(extra_root));
	free(missing_root);
	free(extra_root);
	return 0;
}

static int artifact_symlink_is_rejected(void)
{
	char *root = make_root();
	char directory[PATH_MAX];
	char artifact_path[PATH_MAX];
	struct tcti_target_artifact_publish_result publish_result;
	struct tcti_target_artifact_verify_result verify_result;

	EXPECT(root);
	EXPECT(!publish(root, "linked-artifact",
			TCTI_TARGET_ARTIFACT_STAGE_NONE, &publish_result));
	EXPECT(snprintf(directory, sizeof(directory), "%s/target/linked-artifact",
			root) < (int)sizeof(directory));
	EXPECT(!chmod(directory, 0755));
	EXPECT(snprintf(artifact_path, sizeof(artifact_path), "%s/features.h",
			directory) < (int)sizeof(artifact_path));
	EXPECT(!unlink(artifact_path));
	EXPECT(!symlink("instructions.h", artifact_path));
	EXPECT(verify(root, &provenance, &verify_result) < 0);
	EXPECT(verify_result.error == TCTI_TARGET_ARTIFACT_VERIFY_ARTIFACT_OPEN);
	EXPECT(!remove_tree(root));
	free(root);
	return 0;
}

static int manifest_provenance_and_bundle_tampering_are_rejected(void)
{
	char *provenance_root = make_root();
	char *bundle_root = make_root();
	char path[PATH_MAX];
	char *manifest;
	char *where;
	size_t length;
	struct tcti_target_artifact_publish_result publish_result;
	struct tcti_target_artifact_verify_result verify_result;

	EXPECT(provenance_root && bundle_root);
	EXPECT(!publish(provenance_root, "provenance",
			TCTI_TARGET_ARTIFACT_STAGE_NONE, &publish_result));
	EXPECT(snprintf(path, sizeof(path), "%s/target/provenance/manifest",
			provenance_root) < (int)sizeof(path));
	manifest = read_file(path, &length);
	EXPECT(manifest);
	where = strstr(manifest, "instructions_sha256=");
	EXPECT(where);
	where[strlen("instructions_sha256=")] =
		where[strlen("instructions_sha256=")] == 'a' ? 'b' : 'a';
	EXPECT(!overwrite_file(path, manifest, length));
	free(manifest);
	EXPECT(verify(provenance_root, &provenance, &verify_result) < 0);
	EXPECT(verify_result.error == TCTI_TARGET_ARTIFACT_VERIFY_PROVENANCE);

	EXPECT(!publish(bundle_root, "bundle", TCTI_TARGET_ARTIFACT_STAGE_NONE,
			&publish_result));
	EXPECT(snprintf(path, sizeof(path), "%s/target/bundle/manifest",
			bundle_root) < (int)sizeof(path));
	manifest = read_file(path, &length);
	EXPECT(manifest);
	where = strstr(manifest, "bundle_sha256=");
	EXPECT(where);
	where[strlen("bundle_sha256=")] =
		where[strlen("bundle_sha256=")] == 'a' ? 'b' : 'a';
	EXPECT(!overwrite_file(path, manifest, length));
	free(manifest);
	EXPECT(verify(bundle_root, &provenance, &verify_result) < 0);
	EXPECT(verify_result.error == TCTI_TARGET_ARTIFACT_VERIFY_BUNDLE_DIGEST);

	EXPECT(!remove_tree(provenance_root));
	EXPECT(!remove_tree(bundle_root));
	free(provenance_root);
	free(bundle_root);
	return 0;
}

static int selected_generation_symlink_is_rejected(void)
{
	char *root = make_root();
	char original[PATH_MAX];
	char renamed[PATH_MAX];
	struct tcti_target_artifact_publish_result publish_result;
	struct tcti_target_artifact_verify_result verify_result;

	EXPECT(root);
	EXPECT(!publish(root, "selected", TCTI_TARGET_ARTIFACT_STAGE_NONE,
			&publish_result));
	EXPECT(snprintf(original, sizeof(original), "%s/target/selected", root) <
	       (int)sizeof(original));
	EXPECT(snprintf(renamed, sizeof(renamed), "%s/target/actual", root) <
	       (int)sizeof(renamed));
	EXPECT(!rename(original, renamed));
	EXPECT(!symlink("actual", original));
	EXPECT(verify(root, &provenance, &verify_result) < 0);
	EXPECT(verify_result.error == TCTI_TARGET_ARTIFACT_VERIFY_GENERATION_OPEN);
	EXPECT(!remove_tree(root));
	free(root);
	return 0;
}

int main(void)
{
	static const struct {
		const char *name;
		int (*run)(void);
	} tests[] = {
		{ "deterministic_immutable_publication", deterministic_immutable_publication },
		{ "failures_preserve_prior_selector", failures_preserve_prior_selector },
		{ "short_writes_publish_complete_generation", short_writes_publish_complete_generation },
		{ "selector_sync_is_explicit_post_publish_boundary", selector_sync_is_explicit_post_publish_boundary },
		{ "names_are_not_paths_or_implicit_order", names_are_not_paths_or_implicit_order },
		{ "symlink_publish_root_is_rejected", symlink_publish_root_is_rejected },
		{ "valid_generation_verifies", valid_generation_verifies },
		{ "artifact_content_tampering_is_rejected", artifact_content_tampering_is_rejected },
		{ "missing_and_extra_artifacts_are_rejected", missing_and_extra_artifacts_are_rejected },
		{ "artifact_symlink_is_rejected", artifact_symlink_is_rejected },
		{ "manifest_provenance_and_bundle_tampering_are_rejected", manifest_provenance_and_bundle_tampering_are_rejected },
		{ "selected_generation_symlink_is_rejected", selected_generation_symlink_is_rejected },
	};
	size_t index;

	for (index = 0; index < sizeof(tests) / sizeof(tests[0]); index++) {
		if (tests[index].run()) {
			fprintf(stderr, "FAIL %s\n", tests[index].name);
			return 1;
		}
		printf("PASS %s\n", tests[index].name);
	}
	return 0;
}
