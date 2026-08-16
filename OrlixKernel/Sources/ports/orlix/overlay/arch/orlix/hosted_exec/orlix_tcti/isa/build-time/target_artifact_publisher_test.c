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

static const struct orlix_tcti_target_artifact artifacts[] = {
	{ .name = "features.h", .data = "features-v1\n", .length = 12 },
	{ .name = "instructions.h", .data = "instructions-v1\n", .length = 16 },
	{ .name = "registers.h", .data = "registers-v1\n", .length = 13 },
};
static struct orlix_tcti_target_artifact_provenance provenance = {
	.schema = "orlix-tcti-target-artifact-v3",
	.generator = "target-artifact-generator",
	.producer_sha256 = "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd",
	.source_architecture = "vFATAp1-A",
	.source_build = "818",
	.source_release = "2026-06_rel",
	.source_schema = "2.9.5",
	.source_timestamp = "2026-06-24 17:12:14",
	.instructions_byte_length = 115441429U,
	.instructions_sha256 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
	.features_byte_length = 1243621U,
	.features_sha256 = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
	.registers_byte_length = 96016602U,
	.registers_sha256 = "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc",
};
static char reconciliation_identity[65];

static int initialize_provenance(void)
{
	if (orlix_tcti_target_artifact_reconciliation_identity(
		    &provenance, reconciliation_identity))
		return -1;
	provenance.reconciliation_identity = reconciliation_identity;
	return 0;
}

static int sha256_known_answers(void)
{
	static const struct {
		const char *input;
		const char *digest;
	} vectors[] = {
		{ "", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" },
		{ "abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" },
		{ "The quick brown fox jumps over the lazy dog", "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592" },
	};
	char digest[65];
	size_t index;

	for (index = 0; index < sizeof(vectors) / sizeof(vectors[0]); index++) {
		orlix_tcti_target_artifact_sha256(vectors[index].input,
			strlen(vectors[index].input), digest);
		EXPECT(!strcmp(digest, vectors[index].digest));
	}
	return 0;
}

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
	char selected[ORLIX_TCTI_TARGET_ARTIFACT_MAX_GENERATION + 1U];
	ssize_t length;

	if (snprintf(path, sizeof(path), "%s/target/current", root) >=
	    (int)sizeof(path))
		return -1;
	length = readlink(path, selected, sizeof(selected) - 1U);
	if (length < 0 || (size_t)length >= sizeof(selected))
		return -1;
	selected[length] = '\0';
	return (!strcmp(selected, generation) ||
		(!strncmp(selected, generation, strlen(generation)) &&
		 selected[strlen(generation)] == '-')) ? 0 : -1;
}

static int staging_entry_count(const char *root)
{
	char path[PATH_MAX];
	DIR *directory;
	struct dirent *entry;
	int count = 0;

	if (snprintf(path, sizeof(path), "%s/target", root) >= (int)sizeof(path))
		return -1;
	directory = opendir(path);
	if (!directory)
		return -1;
	while ((entry = readdir(directory)))
		if (!strncmp(entry->d_name, ".staging.", strlen(".staging.")))
			count++;
	if (closedir(directory))
		return -1;
	return count;
}

static int file_matches(const char *root, const char *generation,
			const struct orlix_tcti_target_artifact *artifact)
{
	char path[PATH_MAX];
	char selector[PATH_MAX];
	char selected[ORLIX_TCTI_TARGET_ARTIFACT_MAX_GENERATION + 1U];
	char *contents;
	size_t length;
	int result;

	if (snprintf(path, sizeof(path), "%s/target/%s/%s", root, generation,
		     artifact->name) >= (int)sizeof(path) ||
	    (access(path, F_OK) &&
	     (snprintf(selector, sizeof(selector), "%s/target/current", root) >=
		      (int)sizeof(selector) ||
	      (length = (size_t)readlink(selector, selected,
					 sizeof(selected) - 1U)) >= sizeof(selected) ||
	      (selected[length] = '\0',
	       strncmp(selected, generation, strlen(generation))) ||
	      snprintf(path, sizeof(path), "%s/target/%s/%s", root, selected,
		       artifact->name) >= (int)sizeof(path))))
		return -1;
	contents = read_file(path, &length);
	result = contents && length == artifact->length &&
		!memcmp(contents, artifact->data, length) ? 0 : -1;
	free(contents);
	return result;
}

static int publish(const char *root, const char *generation,
		   enum orlix_tcti_target_artifact_publish_stage stage,
		   struct orlix_tcti_target_artifact_publish_result *result)
{
	struct orlix_tcti_target_artifact_publish_fault fault = { .stage = stage };
	int root_fd;
	int published;

	root_fd = open(root, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	if (root_fd < 0)
		return -1;
	published = orlix_tcti_target_artifact_publish(root_fd, "target", generation, artifacts,
		sizeof(artifacts) / sizeof(artifacts[0]),
		&provenance, stage == ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE ? NULL : &fault,
		result);
	close(root_fd);
	return published;
}

static int publish_bytewise(const char *root, const char *generation,
			    struct orlix_tcti_target_artifact_publish_result *result)
{
	struct orlix_tcti_target_artifact_publish_fault fault = {
		.maximum_write_bytes = 1,
	};
	int root_fd;
	int published;

	root_fd = open(root, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	if (root_fd < 0)
		return -1;
	published = orlix_tcti_target_artifact_publish(root_fd, "target", generation,
		artifacts, sizeof(artifacts) / sizeof(artifacts[0]), &provenance,
		&fault, result);
	close(root_fd);
	return published;
}

static int verify(const char *root,
		  const struct orlix_tcti_target_artifact_provenance *expected,
		  struct orlix_tcti_target_artifact_verify_result *result)
{
	int root_fd;
	int verified;

	root_fd = open(root, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	if (root_fd < 0)
		return -1;
	verified = orlix_tcti_target_artifact_verify(root_fd, "target", expected,
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
	struct orlix_tcti_target_artifact_publish_result result;
	size_t index;
	char left_path[PATH_MAX];
	char right_path[PATH_MAX];
	char left_generation[ORLIX_TCTI_TARGET_ARTIFACT_MAX_GENERATION + 1U];
	char right_generation[ORLIX_TCTI_TARGET_ARTIFACT_MAX_GENERATION + 1U];
	char *left_data;
	char *right_data;
	size_t left_length;
	size_t right_length;
	struct stat status;

	EXPECT(left && right);
	EXPECT(!publish(left, "2026-06-a1", ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE,
			&result));
	EXPECT(result.error == ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_OK);
	memcpy(left_generation, result.generation, sizeof(left_generation));
	EXPECT(!publish(right, "2026-06-a1", ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE,
			&result));
	memcpy(right_generation, result.generation, sizeof(right_generation));
	EXPECT(!strcmp(left_generation, right_generation));
	for (index = 0; index < sizeof(artifacts) / sizeof(artifacts[0]); index++) {
		EXPECT(!file_matches(left, "2026-06-a1", &artifacts[index]));
		EXPECT(snprintf(left_path, sizeof(left_path), "%s/target/%s/%s", left,
				  left_generation, artifacts[index].name) <
			(int)sizeof(left_path));
		EXPECT(snprintf(right_path, sizeof(right_path), "%s/target/%s/%s", right,
				  right_generation, artifacts[index].name) <
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
	EXPECT(snprintf(left_path, sizeof(left_path), "%s/target/%s/manifest",
			left, left_generation) < (int)sizeof(left_path));
	left_data = read_file(left_path, &left_length);
	EXPECT(left_data);
	EXPECT(strstr(left_data, "ORLIX_TCTI_TARGET_ARTIFACT_SET_V3\n"));
	EXPECT(strstr(left_data, "schema=orlix-tcti-target-artifact-v3\n"));
	EXPECT(strstr(left_data, "generator=target-artifact-generator\n"));
	EXPECT(strstr(left_data, "source_architecture=vFATAp1-A\n"));
	EXPECT(strstr(left_data, "source_build=818\n"));
	EXPECT(strstr(left_data, "source_release=2026-06_rel\n"));
	EXPECT(strstr(left_data, "source_schema=2.9.5\n"));
	EXPECT(strstr(left_data, "source_timestamp=2026-06-24 17:12:14\n"));
	EXPECT(strstr(left_data, "instructions_byte_length=115441429\n"));
	EXPECT(strstr(left_data, "features_byte_length=1243621\n"));
	EXPECT(strstr(left_data, "registers_byte_length=96016602\n"));
	EXPECT(strstr(left_data, provenance.instructions_sha256));
	EXPECT(strstr(left_data, provenance.features_sha256));
	EXPECT(strstr(left_data, provenance.registers_sha256));
	EXPECT(strstr(left_data, provenance.reconciliation_identity));
	EXPECT(strstr(left_data, " source_architecture=vFATAp1-A"));
	EXPECT(strstr(left_data, " reconciliation_identity="));
	EXPECT(strstr(left_data, "bundle_sha256="));
	free(left_data);
	EXPECT(!selector_is(left, "2026-06-a1"));
	EXPECT(!selector_is(right, "2026-06-a1"));
	EXPECT(!publish(left, "2026-06-a1", ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE,
			&result));
	EXPECT(result.error == ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_OK);
	EXPECT(!strcmp(result.generation, left_generation));
	for (index = 0; index < sizeof(artifacts) / sizeof(artifacts[0]); index++)
		EXPECT(!file_matches(left, "2026-06-a1", &artifacts[index]));
	EXPECT(!selector_is(left, "2026-06-a1"));
	EXPECT(!remove_tree(left));
	EXPECT(!remove_tree(right));
	free(left);
	free(right);
	return 0;
}

static enum orlix_tcti_target_artifact_publish_error expected_error(
	enum orlix_tcti_target_artifact_publish_stage stage)
{
	switch (stage) {
	case ORLIX_TCTI_TARGET_ARTIFACT_STAGE_TEMP_OPEN:
		return ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_TEMP_OPEN;
	case ORLIX_TCTI_TARGET_ARTIFACT_STAGE_WRITE:
		return ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_WRITE;
	case ORLIX_TCTI_TARGET_ARTIFACT_STAGE_FILE_SYNC:
		return ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_FILE_SYNC;
	case ORLIX_TCTI_TARGET_ARTIFACT_STAGE_FILE_LOCK_SYNC:
		return ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_FILE_LOCK_SYNC;
	case ORLIX_TCTI_TARGET_ARTIFACT_STAGE_FILE_CLOSE:
		return ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_FILE_CLOSE;
	case ORLIX_TCTI_TARGET_ARTIFACT_STAGE_RENAME:
		return ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_RENAME;
	case ORLIX_TCTI_TARGET_ARTIFACT_STAGE_MANIFEST:
		return ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_MANIFEST;
	case ORLIX_TCTI_TARGET_ARTIFACT_STAGE_GENERATION_SYNC:
		return ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_GENERATION_SYNC;
	case ORLIX_TCTI_TARGET_ARTIFACT_STAGE_LOCK_GENERATION:
		return ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_LOCK_GENERATION;
	case ORLIX_TCTI_TARGET_ARTIFACT_STAGE_LOCK_SYNC:
		return ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_LOCK_SYNC;
	case ORLIX_TCTI_TARGET_ARTIFACT_STAGE_PUBLISH_GENERATION:
		return ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_RENAME;
	case ORLIX_TCTI_TARGET_ARTIFACT_STAGE_SELECTOR:
		return ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_SELECTOR;
	case ORLIX_TCTI_TARGET_ARTIFACT_STAGE_SELECTOR_SYNC:
		return ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_SELECTOR_SYNC;
	case ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE:
		break;
	}
	return ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_INVALID_ARGUMENT;
}

static int failures_preserve_prior_selector(void)
{
	static const enum orlix_tcti_target_artifact_publish_stage stages[] = {
		ORLIX_TCTI_TARGET_ARTIFACT_STAGE_TEMP_OPEN,
		ORLIX_TCTI_TARGET_ARTIFACT_STAGE_WRITE,
		ORLIX_TCTI_TARGET_ARTIFACT_STAGE_FILE_SYNC,
		ORLIX_TCTI_TARGET_ARTIFACT_STAGE_FILE_LOCK_SYNC,
		ORLIX_TCTI_TARGET_ARTIFACT_STAGE_FILE_CLOSE,
		ORLIX_TCTI_TARGET_ARTIFACT_STAGE_RENAME,
		ORLIX_TCTI_TARGET_ARTIFACT_STAGE_MANIFEST,
		ORLIX_TCTI_TARGET_ARTIFACT_STAGE_GENERATION_SYNC,
		ORLIX_TCTI_TARGET_ARTIFACT_STAGE_LOCK_GENERATION,
		ORLIX_TCTI_TARGET_ARTIFACT_STAGE_LOCK_SYNC,
		ORLIX_TCTI_TARGET_ARTIFACT_STAGE_PUBLISH_GENERATION,
		ORLIX_TCTI_TARGET_ARTIFACT_STAGE_SELECTOR,
		ORLIX_TCTI_TARGET_ARTIFACT_STAGE_SELECTOR_SYNC,
	};
	char *root = make_root();
	struct orlix_tcti_target_artifact_publish_result result;
	size_t index;
	char selected[64] = "prior";

	EXPECT(root);
	EXPECT(!publish(root, "prior", ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE, &result));
	for (index = 0; index < sizeof(stages) / sizeof(stages[0]); index++) {
		char generation[64];
		char final_path[PATH_MAX];

		EXPECT(snprintf(generation, sizeof(generation), "failed-%zu", index) <
			(int)sizeof(generation));
		EXPECT(publish(root, generation, stages[index], &result) < 0);
		EXPECT(result.error == expected_error(stages[index]));
		EXPECT(!selector_is(root, selected));
		EXPECT(staging_entry_count(root) == 0);
		EXPECT(snprintf(final_path, sizeof(final_path), "%s/target/%s",
				root, result.generation) < (int)sizeof(final_path));
		EXPECT(access(final_path, F_OK) < 0 && errno == ENOENT);
		/* The unselected partial set was discarded, so retry is clean. */
		EXPECT(!publish(root, generation, ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE,
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
	struct orlix_tcti_target_artifact_publish_result result;
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

static int selector_sync_preserves_prior_selector(void)
{
	char *root = make_root();
	struct orlix_tcti_target_artifact_publish_result result;

	EXPECT(root);
	EXPECT(!publish(root, "prior", ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE, &result));
	EXPECT(publish(root, "new", ORLIX_TCTI_TARGET_ARTIFACT_STAGE_SELECTOR_SYNC,
		       &result) < 0);
	EXPECT(result.error == ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_SELECTOR_SYNC);
	EXPECT(!selector_is(root, "prior"));
	EXPECT(!publish(root, "new", ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE,
			&result));
	EXPECT(!selector_is(root, "new"));
	EXPECT(!remove_tree(root));
	free(root);
	return 0;
}

static int names_are_not_paths_or_implicit_order(void)
{
	char *root = make_root();
	struct orlix_tcti_target_artifact_publish_result result;
	struct orlix_tcti_target_artifact invalid[] = {
		{ .name = "../source", .data = "x", .length = 1 },
	};
	struct orlix_tcti_target_artifact duplicate[] = {
		{ .name = "a", .data = "x", .length = 1 },
		{ .name = "a", .data = "y", .length = 1 },
	};
	struct orlix_tcti_target_artifact reserved[] = {
		{ .name = "manifest", .data = "x", .length = 1 },
	};

	EXPECT(root);
	{
		int root_fd = open(root, O_RDONLY | O_DIRECTORY | O_CLOEXEC);

		EXPECT(root_fd >= 0);
		EXPECT(orlix_tcti_target_artifact_publish(root_fd, "target", "bad", invalid,
		       1, &provenance, NULL, &result) < 0);
		EXPECT(result.error == ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_INVALID_NAME);
		EXPECT(orlix_tcti_target_artifact_publish(root_fd, "target", "bad", reserved,
		       1, &provenance, NULL, &result) < 0);
		EXPECT(result.error == ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_INVALID_NAME);
		EXPECT(orlix_tcti_target_artifact_publish(root_fd, "target", "bad", duplicate,
		       2, &provenance, NULL, &result) < 0);
		EXPECT(result.error == ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_INVALID_NAME);
		EXPECT(orlix_tcti_target_artifact_publish(root_fd, "../relative", "bad", artifacts,
		       sizeof(artifacts) / sizeof(artifacts[0]), &provenance, NULL,
		       &result) < 0);
		close(root_fd);
	}
	EXPECT(result.error == ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_INVALID_ARGUMENT);
	EXPECT(!remove_tree(root));
	free(root);
	return 0;
}

static int symlink_publish_root_is_rejected(void)
{
	char *root = make_root();
	char outside[PATH_MAX];
	char link[PATH_MAX];
	struct orlix_tcti_target_artifact_publish_result result;
	int root_fd;

	EXPECT(root);
	EXPECT(snprintf(outside, sizeof(outside), "%s/outside", root) <
	       (int)sizeof(outside));
	EXPECT(snprintf(link, sizeof(link), "%s/linked", root) < (int)sizeof(link));
	EXPECT(!mkdir(outside, 0700));
	EXPECT(!symlink(outside, link));
	root_fd = open(root, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	EXPECT(root_fd >= 0);
	EXPECT(orlix_tcti_target_artifact_publish(root_fd, "linked", "bad", artifacts,
	       sizeof(artifacts) / sizeof(artifacts[0]), &provenance, NULL,
	       &result) < 0);
	EXPECT(result.error == ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_OPEN_ROOT);
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
	struct orlix_tcti_target_artifact_publish_result publish_result;
	struct orlix_tcti_target_artifact_verify_result verify_result;

	EXPECT(root);
	EXPECT(!publish(root, "verified", ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE,
			&publish_result));
	EXPECT(!verify(root, &provenance, &verify_result));
	EXPECT(verify_result.error == ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_OK);
	EXPECT(!remove_tree(root));
	free(root);
	return 0;
}

static int artifact_content_tampering_is_rejected(void)
{
	char *root = make_root();
	char path[PATH_MAX];
	char replacement[] = "Features-v1\n";
	struct orlix_tcti_target_artifact_publish_result publish_result;
	struct orlix_tcti_target_artifact_verify_result verify_result;

	EXPECT(root);
	EXPECT(!publish(root, "tampered", ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE,
			&publish_result));
	EXPECT(snprintf(path, sizeof(path), "%s/target/%s/features.h", root,
			publish_result.generation) <
		       (int)sizeof(path));
	EXPECT(!overwrite_file(path, replacement, sizeof(replacement) - 1));
	EXPECT(verify(root, &provenance, &verify_result) < 0);
	EXPECT(verify_result.error ==
	       ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_ARTIFACT_DIGEST);
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
	struct orlix_tcti_target_artifact_publish_result publish_result;
	struct orlix_tcti_target_artifact_verify_result verify_result;

	EXPECT(missing_root && extra_root);
	EXPECT(!publish(missing_root, "missing", ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE,
			&publish_result));
	EXPECT(snprintf(path, sizeof(path), "%s/target/%s", missing_root,
			publish_result.generation) <
		       (int)sizeof(path));
	EXPECT(!chmod(path, 0755));
	EXPECT(snprintf(path, sizeof(path), "%s/target/%s/features.h",
			missing_root, publish_result.generation) < (int)sizeof(path));
	EXPECT(!unlink(path));
	EXPECT(verify(missing_root, &provenance, &verify_result) < 0);
	EXPECT(verify_result.error == ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_ARTIFACT_OPEN);

	EXPECT(!publish(extra_root, "extra", ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE,
			&publish_result));
	EXPECT(snprintf(path, sizeof(path), "%s/target/%s", extra_root,
			publish_result.generation) <
		       (int)sizeof(path));
	EXPECT(!chmod(path, 0755));
	EXPECT(snprintf(path, sizeof(path), "%s/target/%s/unmanifested.h",
			extra_root, publish_result.generation) < (int)sizeof(path));
	fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
	EXPECT(fd >= 0);
	EXPECT(!close(fd));
	EXPECT(verify(extra_root, &provenance, &verify_result) < 0);
	EXPECT(verify_result.error == ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_EXTRA_ARTIFACT);
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
	struct orlix_tcti_target_artifact_publish_result publish_result;
	struct orlix_tcti_target_artifact_verify_result verify_result;

	EXPECT(root);
	EXPECT(!publish(root, "linked-artifact",
			ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE, &publish_result));
	EXPECT(snprintf(directory, sizeof(directory), "%s/target/%s", root,
			publish_result.generation) < (int)sizeof(directory));
	EXPECT(!chmod(directory, 0755));
	EXPECT(snprintf(artifact_path, sizeof(artifact_path), "%s/features.h",
			directory) < (int)sizeof(artifact_path));
	EXPECT(!unlink(artifact_path));
	EXPECT(!symlink("instructions.h", artifact_path));
	EXPECT(verify(root, &provenance, &verify_result) < 0);
	EXPECT(verify_result.error == ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_ARTIFACT_OPEN);
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
	struct orlix_tcti_target_artifact_publish_result publish_result;
	struct orlix_tcti_target_artifact_verify_result verify_result;

	EXPECT(provenance_root && bundle_root);
	EXPECT(!publish(provenance_root, "provenance",
			ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE, &publish_result));
	EXPECT(snprintf(path, sizeof(path), "%s/target/%s/manifest",
			provenance_root, publish_result.generation) < (int)sizeof(path));
	manifest = read_file(path, &length);
	EXPECT(manifest);
	where = strstr(manifest, "instructions_sha256=");
	EXPECT(where);
	where[strlen("instructions_sha256=")] =
		where[strlen("instructions_sha256=")] == 'a' ? 'b' : 'a';
	EXPECT(!overwrite_file(path, manifest, length));
	free(manifest);
	EXPECT(verify(provenance_root, &provenance, &verify_result) < 0);
	EXPECT(verify_result.error == ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_PROVENANCE);

	EXPECT(!publish(bundle_root, "bundle", ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE,
			&publish_result));
	EXPECT(snprintf(path, sizeof(path), "%s/target/%s/manifest", bundle_root,
			publish_result.generation) < (int)sizeof(path));
	manifest = read_file(path, &length);
	EXPECT(manifest);
	where = strstr(manifest, "bundle_sha256=");
	EXPECT(where);
	where[strlen("bundle_sha256=")] =
		where[strlen("bundle_sha256=")] == 'a' ? 'b' : 'a';
	EXPECT(!overwrite_file(path, manifest, length));
	free(manifest);
	EXPECT(verify(bundle_root, &provenance, &verify_result) < 0);
	EXPECT(verify_result.error == ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_BUNDLE_DIGEST);

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
	struct orlix_tcti_target_artifact_publish_result publish_result;
	struct orlix_tcti_target_artifact_verify_result verify_result;

	EXPECT(root);
	EXPECT(!publish(root, "selected", ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE,
			&publish_result));
	EXPECT(snprintf(original, sizeof(original), "%s/target/%s", root,
			publish_result.generation) < (int)sizeof(original));
	EXPECT(snprintf(renamed, sizeof(renamed), "%s/target/actual", root) <
	       (int)sizeof(renamed));
	EXPECT(!rename(original, renamed));
	EXPECT(!symlink("actual", original));
	EXPECT(verify(root, &provenance, &verify_result) < 0);
	EXPECT(verify_result.error == ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_GENERATION_OPEN);
	EXPECT(!remove_tree(root));
	free(root);
	return 0;
}

static int stale_same_identity_generation_is_rejected(void)
{
	static const char replacement[] = "different bytes\n";
	char *root = make_root();
	char current[PATH_MAX];
	char generation_path[PATH_MAX];
	char artifact_path[PATH_MAX];
	char prior_generation[ORLIX_TCTI_TARGET_ARTIFACT_MAX_GENERATION + 1U];
	struct orlix_tcti_target_artifact_publish_result result;

	EXPECT(root);
	EXPECT(!publish(root, "prior", ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE,
			&result));
	memcpy(prior_generation, result.generation, sizeof(prior_generation));
	EXPECT(!publish(root, "new", ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE,
			&result));
	EXPECT(snprintf(generation_path, sizeof(generation_path), "%s/target/%s",
			root, result.generation) < (int)sizeof(generation_path));
	EXPECT(snprintf(artifact_path, sizeof(artifact_path), "%s/features.h",
			generation_path) < (int)sizeof(artifact_path));
	EXPECT(snprintf(current, sizeof(current), "%s/target/current", root) <
		       (int)sizeof(current));
	EXPECT(!unlink(current));
	EXPECT(!symlink(prior_generation, current));
	EXPECT(!chmod(generation_path, 0755));
	EXPECT(!overwrite_file(artifact_path, replacement,
			       sizeof(replacement) - 1U));
	EXPECT(publish(root, "new", ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE,
		       &result) < 0);
	EXPECT(result.error ==
	       ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_STALE_GENERATION);
	EXPECT(!selector_is(root, prior_generation));
	EXPECT(!remove_tree(root));
	free(root);
	return 0;
}

static int coherent_tamper_with_stale_generation_name_is_rejected(void)
{
	static const char replacement[] = "features-v2\n";
	struct orlix_tcti_target_artifact changed[sizeof(artifacts) /
						       sizeof(artifacts[0])];
	char *root = make_root();
	char generation_path[PATH_MAX];
	char artifact_path[PATH_MAX];
	char manifest_path[PATH_MAX];
	char *manifest = NULL;
	size_t manifest_length = 0;
	struct orlix_tcti_target_artifact_publish_result publish_result;
	struct orlix_tcti_target_artifact_verify_result verify_result;

	EXPECT(root);
	EXPECT(!publish(root, "coherent", ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE,
			&publish_result));
	memcpy(changed, artifacts, sizeof(changed));
	changed[0].data = replacement;
	changed[0].length = sizeof(replacement) - 1U;
	EXPECT(!build_manifest(publish_result.generation, changed,
			       sizeof(changed) / sizeof(changed[0]), &manifest,
			       &manifest_length, &provenance));
	EXPECT(snprintf(generation_path, sizeof(generation_path), "%s/target/%s",
			root, publish_result.generation) <
	       (int)sizeof(generation_path));
	EXPECT(!chmod(generation_path, 0755));
	EXPECT(snprintf(artifact_path, sizeof(artifact_path), "%s/features.h",
			generation_path) < (int)sizeof(artifact_path));
	EXPECT(snprintf(manifest_path, sizeof(manifest_path), "%s/manifest",
			generation_path) < (int)sizeof(manifest_path));
	EXPECT(!overwrite_file(artifact_path, replacement,
			       sizeof(replacement) - 1U));
	EXPECT(!overwrite_file(manifest_path, manifest, manifest_length));
	free(manifest);
	EXPECT(verify(root, &provenance, &verify_result) < 0);
	EXPECT(verify_result.error ==
	       ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_GENERATION_IDENTITY);
	EXPECT(!remove_tree(root));
	free(root);
	return 0;
}

static int fresh_checkout_modes_and_unselected_generation_are_reused(void)
{
	char *root = make_root();
	char current[PATH_MAX];
	char generation_path[PATH_MAX];
	char artifact_path[PATH_MAX];
	char reusable[ORLIX_TCTI_TARGET_ARTIFACT_MAX_GENERATION + 1U];
	char prior[ORLIX_TCTI_TARGET_ARTIFACT_MAX_GENERATION + 1U];
	struct orlix_tcti_target_artifact_publish_result result;
	struct stat status;
	size_t index;

	EXPECT(root);
	EXPECT(!publish(root, "prior", ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE,
			&result));
	memcpy(prior, result.generation, sizeof(prior));
	EXPECT(!publish(root, "reusable", ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE,
			&result));
	memcpy(reusable, result.generation, sizeof(reusable));
	EXPECT(snprintf(current, sizeof(current), "%s/target/current", root) <
	       (int)sizeof(current));
	EXPECT(!unlink(current));
	EXPECT(!symlink(prior, current));
	EXPECT(snprintf(generation_path, sizeof(generation_path), "%s/target/%s",
			root, reusable) < (int)sizeof(generation_path));
	EXPECT(!chmod(generation_path, 0755));
	for (index = 0; index < sizeof(artifacts) / sizeof(artifacts[0]); index++) {
		EXPECT(snprintf(artifact_path, sizeof(artifact_path), "%s/%s",
				generation_path, artifacts[index].name) <
		       (int)sizeof(artifact_path));
		EXPECT(!chmod(artifact_path, 0644));
	}
	EXPECT(snprintf(artifact_path, sizeof(artifact_path), "%s/manifest",
			generation_path) < (int)sizeof(artifact_path));
	EXPECT(!chmod(artifact_path, 0644));
	EXPECT(!publish(root, "reusable", ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE,
			&result));
	EXPECT(!selector_is(root, reusable));
	EXPECT(!stat(generation_path, &status));
	EXPECT((status.st_mode & 0222) == 0);
	for (index = 0; index < sizeof(artifacts) / sizeof(artifacts[0]); index++) {
		EXPECT(snprintf(artifact_path, sizeof(artifact_path), "%s/%s",
				generation_path, artifacts[index].name) <
		       (int)sizeof(artifact_path));
		EXPECT(!stat(artifact_path, &status));
		EXPECT((status.st_mode & 0222) == 0);
	}
	EXPECT(snprintf(artifact_path, sizeof(artifact_path), "%s/manifest",
			generation_path) < (int)sizeof(artifact_path));
	EXPECT(!stat(artifact_path, &status));
	EXPECT((status.st_mode & 0222) == 0);
	EXPECT(!remove_tree(root));
	free(root);
	return 0;
}

static int orphaned_hidden_staging_directory_does_not_block_retry(void)
{
	char *root = make_root();
	char target[PATH_MAX];
	char orphan[PATH_MAX];
	char partial[PATH_MAX];
	struct orlix_tcti_target_artifact_publish_result result;
	int fd;

	EXPECT(root);
	EXPECT(snprintf(target, sizeof(target), "%s/target", root) <
	       (int)sizeof(target));
	EXPECT(!mkdir(target, 0755));
	EXPECT(snprintf(orphan, sizeof(orphan), "%s/.staging.crash.tmp", target) <
	       (int)sizeof(orphan));
	EXPECT(!mkdir(orphan, 0755));
	EXPECT(snprintf(partial, sizeof(partial), "%s/partial", orphan) <
	       (int)sizeof(partial));
	fd = open(partial, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
	EXPECT(fd >= 0);
	EXPECT(write(fd, "partial", 7) == 7);
	EXPECT(!close(fd));
	EXPECT(!publish(root, "retry", ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE,
			&result));
	EXPECT(!selector_is(root, result.generation));
	EXPECT(staging_entry_count(root) == 1);
	EXPECT(!remove_tree(root));
	free(root);
	return 0;
}

static int artifact_source_identity_tampering_is_rejected(void)
{
	char *root = make_root();
	char path[PATH_MAX];
	char *manifest;
	char *where;
	size_t length;
	struct orlix_tcti_target_artifact_publish_result publish_result;
	struct orlix_tcti_target_artifact_verify_result verify_result;

	EXPECT(root);
	EXPECT(!publish(root, "artifact-source-identity",
			ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE, &publish_result));
	EXPECT(snprintf(path, sizeof(path), "%s/target/%s/manifest", root,
			publish_result.generation) < (int)sizeof(path));
	manifest = read_file(path, &length);
	EXPECT(manifest);
	where = strstr(manifest, " source_architecture=");
	EXPECT(where);
	where += strlen(" source_architecture=");
	where[0] = where[0] == 'a' ? 'b' : 'a';
	EXPECT(!overwrite_file(path, manifest, length));
	free(manifest);
	EXPECT(verify(root, &provenance, &verify_result) < 0);
	EXPECT(verify_result.error ==
	       ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_MANIFEST_FORMAT);
	EXPECT(!remove_tree(root));
	free(root);
	return 0;
}

static int every_three_source_field_is_verified(void)
{
	char *root = make_root();
	struct orlix_tcti_target_artifact_publish_result publish_result;
	struct orlix_tcti_target_artifact_verify_result verify_result;
	struct orlix_tcti_target_artifact_provenance mutated;
	char identity[65];

	EXPECT(root);
	EXPECT(!publish(root, "source-fields",
			ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE, &publish_result));

#define EXPECT_PROVENANCE_MUTATION_REJECTED(mutation) do { \
	mutated = provenance; \
	mutation; \
	EXPECT(!orlix_tcti_target_artifact_reconciliation_identity( \
		&mutated, identity)); \
	mutated.reconciliation_identity = identity; \
	EXPECT(verify(root, &mutated, &verify_result) < 0); \
	EXPECT(verify_result.error == ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_PROVENANCE); \
} while (0)
	EXPECT_PROVENANCE_MUTATION_REJECTED(
		mutated.schema = "orlix-tcti-aarchmrs-source-v4");
	EXPECT_PROVENANCE_MUTATION_REJECTED(
		mutated.generator = "orlix-tcti-target-refresh-system-accessor-v2");
	EXPECT_PROVENANCE_MUTATION_REJECTED(
		mutated.producer_sha256 =
		"eddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd");
	EXPECT_PROVENANCE_MUTATION_REJECTED(
		mutated.source_architecture = "vFATAp1-B");
	EXPECT_PROVENANCE_MUTATION_REJECTED(mutated.source_build = "819");
	EXPECT_PROVENANCE_MUTATION_REJECTED(
		mutated.source_release = "2026-09_rel");
	EXPECT_PROVENANCE_MUTATION_REJECTED(mutated.source_schema = "2.9.6");
	EXPECT_PROVENANCE_MUTATION_REJECTED(
		mutated.source_timestamp = "2026-06-24 17:12:15");
	EXPECT_PROVENANCE_MUTATION_REJECTED(mutated.instructions_byte_length++);
	EXPECT_PROVENANCE_MUTATION_REJECTED(
		mutated.instructions_sha256 =
		"daaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
	EXPECT_PROVENANCE_MUTATION_REJECTED(mutated.features_byte_length++);
	EXPECT_PROVENANCE_MUTATION_REJECTED(
		mutated.features_sha256 =
		"dbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
	EXPECT_PROVENANCE_MUTATION_REJECTED(mutated.registers_byte_length++);
	EXPECT_PROVENANCE_MUTATION_REJECTED(
		mutated.registers_sha256 =
		"dccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc");
#undef EXPECT_PROVENANCE_MUTATION_REJECTED

	mutated = provenance;
	mutated.reconciliation_identity =
		"dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd";
	EXPECT(verify(root, &mutated, &verify_result) < 0);
	EXPECT(verify_result.error ==
	       ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_INVALID_ARGUMENT);

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
		{ "sha256_known_answers", sha256_known_answers },
		{ "deterministic_immutable_publication", deterministic_immutable_publication },
		{ "every_three_source_field_is_verified",
		  every_three_source_field_is_verified },
		{ "artifact_source_identity_tampering_is_rejected",
		  artifact_source_identity_tampering_is_rejected },
		{ "failures_preserve_prior_selector", failures_preserve_prior_selector },
		{ "short_writes_publish_complete_generation", short_writes_publish_complete_generation },
		{ "selector_sync_preserves_prior_selector", selector_sync_preserves_prior_selector },
		{ "names_are_not_paths_or_implicit_order", names_are_not_paths_or_implicit_order },
		{ "symlink_publish_root_is_rejected", symlink_publish_root_is_rejected },
		{ "valid_generation_verifies", valid_generation_verifies },
		{ "artifact_content_tampering_is_rejected", artifact_content_tampering_is_rejected },
		{ "missing_and_extra_artifacts_are_rejected", missing_and_extra_artifacts_are_rejected },
		{ "artifact_symlink_is_rejected", artifact_symlink_is_rejected },
		{ "manifest_provenance_and_bundle_tampering_are_rejected", manifest_provenance_and_bundle_tampering_are_rejected },
		{ "selected_generation_symlink_is_rejected", selected_generation_symlink_is_rejected },
		{ "stale_same_identity_generation_is_rejected", stale_same_identity_generation_is_rejected },
		{ "coherent_tamper_with_stale_generation_name_is_rejected", coherent_tamper_with_stale_generation_name_is_rejected },
		{ "fresh_checkout_modes_and_unselected_generation_are_reused", fresh_checkout_modes_and_unselected_generation_are_reused },
		{ "orphaned_hidden_staging_directory_does_not_block_retry", orphaned_hidden_staging_directory_does_not_block_retry },
	};
	size_t index;

	if (initialize_provenance()) {
		fprintf(stderr, "FAIL initialize_provenance\n");
		return 1;
	}

	for (index = 0; index < sizeof(tests) / sizeof(tests[0]); index++) {
		if (tests[index].run()) {
			fprintf(stderr, "FAIL %s\n", tests[index].name);
			return 1;
		}
		printf("PASS %s\n", tests[index].name);
	}
	return 0;
}
