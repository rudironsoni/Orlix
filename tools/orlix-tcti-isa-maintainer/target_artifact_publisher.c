/* SPDX-License-Identifier: GPL-2.0-only */
#define _POSIX_C_SOURCE 200809L
#include "target_artifact_publisher.h"

#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
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

#define ORLIX_TCTI_TARGET_ARTIFACT_MAX_GENERATION 128U
#define ORLIX_TCTI_TARGET_ARTIFACT_MAX_COUNT 4096U
#define ORLIX_TCTI_TARGET_ARTIFACT_MAX_TOTAL_BYTES (512U * 1024U * 1024U)
#define ORLIX_TCTI_TARGET_ARTIFACT_TEMP_ATTEMPTS 256U

struct publisher {
	int root_fd;
	int generation_fd;
	const struct orlix_tcti_target_artifact_publish_fault *fault;
	unsigned long temporary_sequence;
	struct orlix_tcti_target_artifact_publish_result *result;
};

static void set_result(struct publisher *publisher,
			       enum orlix_tcti_target_artifact_publish_error error,
			       enum orlix_tcti_target_artifact_publish_stage stage,
			       int system_error)
{
	if (!publisher->result)
		return;
	publisher->result->error = error;
	publisher->result->stage = stage;
	publisher->result->system_error = system_error;
}

static bool fail_stage(const struct publisher *publisher,
		       enum orlix_tcti_target_artifact_publish_stage stage)
{
	return publisher->fault && publisher->fault->stage == stage;
}

static bool valid_component(const char *text, size_t maximum)
{
	size_t index;

	if (!text || !*text || strlen(text) > maximum)
		return false;
	if (!strcmp(text, ".") || !strcmp(text, ".."))
		return false;
	for (index = 0; text[index]; index++) {
		unsigned char value = (unsigned char)text[index];

		if (!(value >= 'A' && value <= 'Z') &&
		    !(value >= 'a' && value <= 'z') &&
		    !(value >= '0' && value <= '9') &&
		    value != '.' && value != '_' && value != '-')
			return false;
	}
	return true;
}

static bool valid_sha256(const char *text)
{
	size_t index;

	if (!text || strlen(text) != 64)
		return false;
	for (index = 0; index < 64; index++)
		if (!((text[index] >= '0' && text[index] <= '9') ||
		      (text[index] >= 'a' && text[index] <= 'f')))
			return false;
	return true;
}

static bool valid_provenance(
	const struct orlix_tcti_target_artifact_provenance *provenance)
{
	return provenance &&
		valid_component(provenance->schema, ORLIX_TCTI_TARGET_ARTIFACT_MAX_NAME) &&
		valid_component(provenance->generator,
				ORLIX_TCTI_TARGET_ARTIFACT_MAX_NAME) &&
		valid_sha256(provenance->instructions_sha256) &&
		valid_sha256(provenance->features_sha256) &&
		valid_sha256(provenance->registers_sha256);
}

static enum orlix_tcti_target_artifact_publish_error
validate_artifacts(const struct orlix_tcti_target_artifact *artifacts,
		   size_t artifact_count)
{
	size_t index;
	size_t total = 0;

	if (!artifacts || !artifact_count || artifact_count >
	    ORLIX_TCTI_TARGET_ARTIFACT_MAX_COUNT)
		return ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_INVALID_ARGUMENT;
	for (index = 0; index < artifact_count; index++) {
		if (!valid_component(artifacts[index].name,
			     ORLIX_TCTI_TARGET_ARTIFACT_MAX_NAME) ||
		    (!artifacts[index].data && artifacts[index].length))
			return ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_INVALID_NAME;
		if (!strcmp(artifacts[index].name, "manifest") ||
		    !strcmp(artifacts[index].name, "current"))
			return ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_INVALID_NAME;
		if (index && strcmp(artifacts[index - 1].name,
				    artifacts[index].name) >= 0)
			return ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_INVALID_NAME;
		if (artifacts[index].length > ORLIX_TCTI_TARGET_ARTIFACT_MAX_TOTAL_BYTES -
		    total)
			return ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_INVALID_ARGUMENT;
		total += artifacts[index].length;
	}
	return ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_OK;
}

static int write_all(struct publisher *publisher, int fd, const void *data,
		     size_t length)
{
	const unsigned char *bytes = data;

	if (fail_stage(publisher, ORLIX_TCTI_TARGET_ARTIFACT_STAGE_WRITE)) {
		errno = EIO;
		return -1;
	}
	while (length) {
		size_t request = length;
		ssize_t written;

		if (publisher->fault && publisher->fault->maximum_write_bytes &&
		    request > publisher->fault->maximum_write_bytes)
			request = publisher->fault->maximum_write_bytes;
		written = write(fd, bytes, request);

		if (written < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		if (!written) {
			errno = EIO;
			return -1;
		}
		bytes += written;
		length -= (size_t)written;
	}
	return 0;
}

static int open_temporary(struct publisher *publisher, const char *name,
			  char temporary[NAME_MAX + 1])
{
	unsigned int attempt;

	for (attempt = 0; attempt < ORLIX_TCTI_TARGET_ARTIFACT_TEMP_ATTEMPTS;
	     attempt++) {
		int fd;
		int count;

		if (fail_stage(publisher, ORLIX_TCTI_TARGET_ARTIFACT_STAGE_TEMP_OPEN)) {
			errno = EIO;
			return -1;
		}
		count = snprintf(temporary, NAME_MAX + 1, ".%s.%ld.%lu.tmp",
				 name, (long)getpid(), publisher->temporary_sequence++);
		if (count < 0 || count > NAME_MAX) {
			errno = ENAMETOOLONG;
			return -1;
		}
		fd = openat(publisher->generation_fd, temporary,
				O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
				0600);
		if (fd >= 0)
			return fd;
		if (errno != EEXIST)
			return -1;
	}
	errno = EEXIST;
	return -1;
}

static int publish_file(struct publisher *publisher, const char *name,
			const void *data, size_t length,
			enum orlix_tcti_target_artifact_publish_error semantic_failure)
{
	char temporary[NAME_MAX + 1] = { 0 };
	int fd = -1;
	int saved_errno;

	fd = open_temporary(publisher, name, temporary);
	if (fd < 0)
		goto fail;
	if (write_all(publisher, fd, data, length))
		goto fail;
	if (fail_stage(publisher, ORLIX_TCTI_TARGET_ARTIFACT_STAGE_FILE_SYNC)) {
		errno = EIO;
		goto fail;
	}
	if (fsync(fd))
		goto fail;
	if (fchmod(fd, 0444))
		goto fail;
	if (fail_stage(publisher, ORLIX_TCTI_TARGET_ARTIFACT_STAGE_FILE_LOCK_SYNC)) {
		errno = EIO;
		goto fail;
	}
	if (fsync(fd))
		goto fail;
	if (fail_stage(publisher, ORLIX_TCTI_TARGET_ARTIFACT_STAGE_FILE_CLOSE)) {
		errno = EIO;
		goto fail;
	}
	if (close(fd)) {
		fd = -1;
		goto fail;
	}
	fd = -1;
	if (fail_stage(publisher, ORLIX_TCTI_TARGET_ARTIFACT_STAGE_RENAME)) {
		errno = EIO;
		goto fail;
	}
	if (renameat(publisher->generation_fd, temporary,
		     publisher->generation_fd, name))
		goto fail;
	return 0;
fail:
	saved_errno = errno;
	if (fd >= 0)
		close(fd);
	if (temporary[0])
		unlinkat(publisher->generation_fd, temporary, 0);
	if (semantic_failure != ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_OK)
		set_result(publisher, semantic_failure,
			   ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE, saved_errno);
	else if (fail_stage(publisher, ORLIX_TCTI_TARGET_ARTIFACT_STAGE_TEMP_OPEN))
		set_result(publisher, ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_TEMP_OPEN,
			   ORLIX_TCTI_TARGET_ARTIFACT_STAGE_TEMP_OPEN, saved_errno);
	else if (fail_stage(publisher, ORLIX_TCTI_TARGET_ARTIFACT_STAGE_WRITE))
		set_result(publisher, ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_WRITE,
			   ORLIX_TCTI_TARGET_ARTIFACT_STAGE_WRITE, saved_errno);
	else if (fail_stage(publisher, ORLIX_TCTI_TARGET_ARTIFACT_STAGE_FILE_SYNC))
		set_result(publisher, ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_FILE_SYNC,
		   ORLIX_TCTI_TARGET_ARTIFACT_STAGE_FILE_SYNC, saved_errno);
	else if (fail_stage(publisher,
			   ORLIX_TCTI_TARGET_ARTIFACT_STAGE_FILE_LOCK_SYNC))
		set_result(publisher, ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_FILE_LOCK_SYNC,
			   ORLIX_TCTI_TARGET_ARTIFACT_STAGE_FILE_LOCK_SYNC, saved_errno);
	else if (fail_stage(publisher, ORLIX_TCTI_TARGET_ARTIFACT_STAGE_FILE_CLOSE))
		set_result(publisher, ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_FILE_CLOSE,
			   ORLIX_TCTI_TARGET_ARTIFACT_STAGE_FILE_CLOSE, saved_errno);
	else
		set_result(publisher, ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_RENAME,
			   ORLIX_TCTI_TARGET_ARTIFACT_STAGE_RENAME, saved_errno);
	errno = saved_errno;
	return -1;
}

#define ORLIX_TCTI_TARGET_ARTIFACT_MAX_MANIFEST_BYTES (2U * 1024U * 1024U)
#define ORLIX_TCTI_TARGET_ARTIFACT_MAX_SELECTOR_BYTES 256U

struct publisher_sha256 {
	uint32_t words[8];
	uint64_t byte_count;
	unsigned char block[64];
	size_t used;
};

static void publisher_sha256_transform(struct publisher_sha256 *state,
				       const unsigned char block[64]);
static void publisher_sha256_update(struct publisher_sha256 *state,
				    const unsigned char *data, size_t length);

struct verified_artifact {
	char name[ORLIX_TCTI_TARGET_ARTIFACT_MAX_NAME + 1];
	size_t length;
	char digest[65];
};

struct manifest_cursor {
	const char *data;
	size_t length;
	size_t offset;
};

static void set_verify_result(
	struct orlix_tcti_target_artifact_verify_result *result,
	enum orlix_tcti_target_artifact_verify_error error, int system_error,
	const char *artifact)
{
	if (!result)
		return;
	*result = (struct orlix_tcti_target_artifact_verify_result) {
		.error = error,
		.system_error = system_error,
	};
	if (artifact)
		snprintf(result->artifact, sizeof(result->artifact), "%s",
			 artifact);
}

static int read_bounded_regular_at(int directory_fd, const char *name,
				   size_t maximum, char **data, size_t *length,
				   enum orlix_tcti_target_artifact_verify_error open_error,
				   struct orlix_tcti_target_artifact_verify_result *result)
{
	struct stat status;
	char *buffer;
	size_t used = 0;
	int fd;

	fd = openat(directory_fd, name,
		    O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
	if (fd < 0) {
		set_verify_result(result, open_error, errno, name);
		return -1;
	}
	if (fstat(fd, &status) || !S_ISREG(status.st_mode)) {
		int saved_errno = errno ? errno : EINVAL;

		close(fd);
		set_verify_result(result, ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_ARTIFACT_TYPE,
				  saved_errno, name);
		return -1;
	}
	if (status.st_size < 0 || (uintmax_t)status.st_size > maximum) {
		close(fd);
		set_verify_result(result, ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_RESOURCE_LIMIT,
				  EFBIG, name);
		errno = EFBIG;
		return -1;
	}
	buffer = malloc((size_t)status.st_size + 1);
	if (!buffer) {
		close(fd);
		set_verify_result(result, ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_IO, ENOMEM,
				  name);
		return -1;
	}
	while (used < (size_t)status.st_size) {
		ssize_t count = read(fd, buffer + used,
				     (size_t)status.st_size - used);

		if (count < 0 && errno == EINTR)
			continue;
		if (count <= 0) {
			int saved_errno = count ? errno : EIO;

			free(buffer);
			close(fd);
			set_verify_result(result, ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_IO,
					  saved_errno, name);
			errno = saved_errno;
			return -1;
		}
		used += (size_t)count;
	}
	if (close(fd)) {
		int saved_errno = errno;

		free(buffer);
		set_verify_result(result, ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_IO,
				  saved_errno, name);
		return -1;
	}
	buffer[used] = '\0';
	*data = buffer;
	*length = used;
	return 0;
}

static int next_manifest_line(struct manifest_cursor *cursor,
			      const char **line, size_t *line_length,
			      size_t *line_offset)
{
	const char *newline;

	if (cursor->offset >= cursor->length)
		return 0;
	newline = memchr(cursor->data + cursor->offset, '\n',
			 cursor->length - cursor->offset);
	if (!newline)
		return -1;
	*line = cursor->data + cursor->offset;
	*line_length = (size_t)(newline - *line);
	*line_offset = cursor->offset;
	if (memchr(*line, '\0', *line_length))
		return -1;
	cursor->offset += *line_length + 1;
	return 1;
}

static bool line_is(const char *line, size_t length, const char *expected)
{
	return length == strlen(expected) && !memcmp(line, expected, length);
}

static bool line_has_value(const char *line, size_t length,
			   const char *prefix, const char *expected)
{
	size_t prefix_length = strlen(prefix);
	size_t expected_length = strlen(expected);

	return length == prefix_length + expected_length &&
		!memcmp(line, prefix, prefix_length) &&
		!memcmp(line + prefix_length, expected, expected_length);
}

static int parse_size(const char *text, size_t length, size_t *value)
{
	size_t index;
	size_t result = 0;

	if (!length || (length > 1 && text[0] == '0'))
		return -1;
	for (index = 0; index < length; index++) {
		unsigned int digit;

		if (text[index] < '0' || text[index] > '9')
			return -1;
		digit = (unsigned int)(text[index] - '0');
		if (result > (SIZE_MAX - digit) / 10)
			return -1;
		result = result * 10 + digit;
	}
	*value = result;
	return 0;
}

static const char *find_bytes(const char *data, size_t length,
			      const char *needle, size_t needle_length)
{
	size_t index;

	if (!needle_length || needle_length > length)
		return NULL;
	for (index = 0; index <= length - needle_length; index++)
		if (!memcmp(data + index, needle, needle_length))
			return data + index;
	return NULL;
}

static int parse_artifact_line(const char *line, size_t line_length,
			       struct verified_artifact *artifact)
{
	static const char prefix[] = "artifact=";
	static const char digest_marker[] = " sha256=";
	const char *name;
	const char *name_end;
	const char *digest;
	const char *length_end;
	size_t name_length;
	size_t index;

	if (line_length <= sizeof(prefix) - 1 ||
	    memcmp(line, prefix, sizeof(prefix) - 1))
		return -1;
	name = line + sizeof(prefix) - 1;
	name_end = memchr(name, ' ', line_length - (size_t)(name - line));
	if (!name_end)
		return -1;
	name_length = (size_t)(name_end - name);
	if (!name_length || name_length > ORLIX_TCTI_TARGET_ARTIFACT_MAX_NAME)
		return -1;
	memcpy(artifact->name, name, name_length);
	artifact->name[name_length] = '\0';
	if (!valid_component(artifact->name, ORLIX_TCTI_TARGET_ARTIFACT_MAX_NAME) ||
	    !strcmp(artifact->name, "manifest") ||
	    !strcmp(artifact->name, "current"))
		return -1;
	length_end = find_bytes(name_end + 1,
				line_length - (size_t)(name_end + 1 - line),
				digest_marker, sizeof(digest_marker) - 1);
	if (!length_end ||
	    parse_size(name_end + 1, (size_t)(length_end - (name_end + 1)),
		       &artifact->length))
		return -1;
	digest = length_end + sizeof(digest_marker) - 1;
	if ((size_t)(line + line_length - digest) != 64)
		return -1;
	for (index = 0; index < 64; index++)
		if (!((digest[index] >= '0' && digest[index] <= '9') ||
		      (digest[index] >= 'a' && digest[index] <= 'f')))
			return -1;
	memcpy(artifact->digest, digest, 64);
	artifact->digest[64] = '\0';
	return 0;
}

static int hash_artifact(int generation_fd,
			 const struct verified_artifact *artifact,
			 struct orlix_tcti_target_artifact_verify_result *result)
{
	struct publisher_sha256 state = {
		.words = { 0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U,
			   0xa54ff53aU, 0x510e527fU, 0x9b05688cU,
			   0x1f83d9abU, 0x5be0cd19U },
	};
	struct stat status;
	unsigned char buffer[65536];
	unsigned char output[32];
	char digest[65];
	uint64_t bits;
	size_t index;
	int fd;

	fd = openat(generation_fd, artifact->name,
		    O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
	if (fd < 0) {
		set_verify_result(result, ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_ARTIFACT_OPEN,
				  errno, artifact->name);
		return -1;
	}
	if (fstat(fd, &status) || !S_ISREG(status.st_mode)) {
		int saved_errno = errno ? errno : EINVAL;

		close(fd);
		set_verify_result(result, ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_ARTIFACT_TYPE,
				  saved_errno, artifact->name);
		return -1;
	}
	if (status.st_size < 0 || (uintmax_t)status.st_size != artifact->length) {
		close(fd);
		set_verify_result(result, ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_ARTIFACT_SIZE,
				  EINVAL, artifact->name);
		errno = EINVAL;
		return -1;
	}
	for (;;) {
		ssize_t count = read(fd, buffer, sizeof(buffer));

		if (count < 0 && errno == EINTR)
			continue;
		if (count < 0) {
			int saved_errno = errno;

			close(fd);
			set_verify_result(result, ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_IO,
					  saved_errno, artifact->name);
			return -1;
		}
		if (!count)
			break;
		publisher_sha256_update(&state, buffer, (size_t)count);
	}
	if (close(fd)) {
		set_verify_result(result, ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_IO, errno,
				  artifact->name);
		return -1;
	}
	bits = state.byte_count * 8U;
	state.block[state.used++] = 0x80;
	if (state.used > 56) {
		memset(state.block + state.used, 0,
		       sizeof(state.block) - state.used);
		publisher_sha256_transform(&state, state.block);
		state.used = 0;
	}
	memset(state.block + state.used, 0, 56 - state.used);
	for (index = 0; index < 8; index++)
		state.block[63 - index] = (unsigned char)(bits >> (index * 8));
	publisher_sha256_transform(&state, state.block);
	for (index = 0; index < 8; index++) {
		output[index * 4] = (unsigned char)(state.words[index] >> 24);
		output[index * 4 + 1] = (unsigned char)(state.words[index] >> 16);
		output[index * 4 + 2] = (unsigned char)(state.words[index] >> 8);
		output[index * 4 + 3] = (unsigned char)state.words[index];
	}
	for (index = 0; index < sizeof(output); index++) {
		static const char hex[] = "0123456789abcdef";

		digest[index * 2] = hex[output[index] >> 4];
		digest[index * 2 + 1] = hex[output[index] & 0xfU];
	}
	digest[64] = '\0';
	if (strcmp(digest, artifact->digest)) {
		set_verify_result(result, ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_ARTIFACT_DIGEST,
				  EINVAL, artifact->name);
		errno = EINVAL;
		return -1;
	}
	return 0;
}

static bool artifact_is_listed(const struct verified_artifact *artifacts,
			       size_t count, const char *name)
{
	size_t low = 0;
	size_t high = count;

	while (low < high) {
		size_t middle = low + (high - low) / 2;
		int comparison = strcmp(name, artifacts[middle].name);

		if (!comparison)
			return true;
		if (comparison < 0)
			high = middle;
		else
			low = middle + 1;
	}
	return false;
}

const char *orlix_tcti_target_artifact_verify_error_name(
	enum orlix_tcti_target_artifact_verify_error error)
{
	switch (error) {
	case ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_OK: return "success";
	case ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_INVALID_ARGUMENT: return "invalid argument";
	case ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_OPEN_ROOT: return "open publish root";
	case ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_SELECTOR_OPEN: return "open current selector";
	case ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_SELECTOR_FORMAT: return "invalid current selector";
	case ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_GENERATION_OPEN: return "open current generation";
	case ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_MANIFEST_OPEN: return "open generation manifest";
	case ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_MANIFEST_FORMAT: return "invalid generation manifest";
	case ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_PROVENANCE: return "generation provenance mismatch";
	case ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_BUNDLE_DIGEST: return "generation bundle digest mismatch";
	case ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_ARTIFACT_OPEN: return "open generated artifact";
	case ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_ARTIFACT_TYPE: return "generated artifact is not regular";
	case ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_ARTIFACT_SIZE: return "generated artifact size mismatch";
	case ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_ARTIFACT_DIGEST: return "generated artifact digest mismatch";
	case ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_EXTRA_ARTIFACT: return "unmanifested generated artifact";
	case ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_RESOURCE_LIMIT: return "verification resource limit";
	case ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_IO: return "verification I/O failure";
	}
	return "unknown verification error";
}

int orlix_tcti_target_artifact_verify(
	int build_root_fd, const char *publish_name,
	const struct orlix_tcti_target_artifact_provenance *expected_provenance,
	struct orlix_tcti_target_artifact_verify_result *result)
{
	struct verified_artifact *artifacts = NULL;
	struct manifest_cursor cursor;
	char generation[ORLIX_TCTI_TARGET_ARTIFACT_MAX_GENERATION + 1];
	char *selector = NULL;
	char *manifest = NULL;
	size_t selector_length = 0;
	size_t manifest_length = 0;
	size_t artifact_count = 0;
	size_t total_bytes = 0;
	size_t line_length;
	size_t line_offset;
	const char *line;
	int root_fd = -1;
	int generation_fd = -1;
	int line_result;
	int return_value = -1;
	size_t index;

	if (result)
		*result = (struct orlix_tcti_target_artifact_verify_result) {
			.error = ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_OK,
		};
	if (build_root_fd < 0 ||
	    !valid_component(publish_name, ORLIX_TCTI_TARGET_ARTIFACT_MAX_NAME) ||
	    !valid_provenance(expected_provenance)) {
		set_verify_result(result, ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_INVALID_ARGUMENT,
				  EINVAL, NULL);
		errno = EINVAL;
		return -1;
	}
	root_fd = openat(build_root_fd, publish_name,
			 O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	if (root_fd < 0) {
		set_verify_result(result, ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_OPEN_ROOT,
				  errno, NULL);
		goto out;
	}
	if (read_bounded_regular_at(root_fd, "current",
				    ORLIX_TCTI_TARGET_ARTIFACT_MAX_SELECTOR_BYTES,
				    &selector, &selector_length,
				    ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_SELECTOR_OPEN,
				    result))
		goto out;
	cursor = (struct manifest_cursor) {
		.data = selector,
		.length = selector_length,
	};
	line_result = next_manifest_line(&cursor, &line, &line_length,
					 &line_offset);
	if (line_result != 1 ||
	    !line_is(line, line_length, "ORLIX_TCTI_TARGET_ARTIFACT_CURRENT_V1") ||
	    next_manifest_line(&cursor, &line, &line_length, &line_offset) != 1 ||
	    line_length <= strlen("generation=") ||
	    memcmp(line, "generation=", strlen("generation=")) ||
	    line_length - strlen("generation=") > ORLIX_TCTI_TARGET_ARTIFACT_MAX_GENERATION)
		goto selector_format;
	memcpy(generation, line + strlen("generation="),
	       line_length - strlen("generation="));
	generation[line_length - strlen("generation=")] = '\0';
	if (!valid_component(generation, ORLIX_TCTI_TARGET_ARTIFACT_MAX_GENERATION) ||
	    next_manifest_line(&cursor, &line, &line_length, &line_offset) != 1 ||
	    !line_is(line, line_length, "manifest=manifest") ||
	    cursor.offset != cursor.length)
		goto selector_format;
	generation_fd = openat(root_fd, generation,
			       O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	if (generation_fd < 0) {
		set_verify_result(result,
				  ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_GENERATION_OPEN,
				  errno, generation);
		goto out;
	}
	if (read_bounded_regular_at(generation_fd, "manifest",
				    ORLIX_TCTI_TARGET_ARTIFACT_MAX_MANIFEST_BYTES,
				    &manifest, &manifest_length,
				    ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_MANIFEST_OPEN,
				    result))
		goto out;
	cursor = (struct manifest_cursor) {
		.data = manifest,
		.length = manifest_length,
	};
#define EXPECT_MANIFEST_LINE(expression) do { \
	if (next_manifest_line(&cursor, &line, &line_length, &line_offset) != 1 || \
	    !(expression)) \
		goto manifest_format; \
} while (0)
	EXPECT_MANIFEST_LINE(line_is(line, line_length,
				    "ORLIX_TCTI_TARGET_ARTIFACT_SET_V2"));
	EXPECT_MANIFEST_LINE(line_has_value(line, line_length, "generation=",
					   generation));
	if (next_manifest_line(&cursor, &line, &line_length, &line_offset) != 1 ||
	    !line_has_value(line, line_length, "schema=",
			    expected_provenance->schema))
		goto provenance_mismatch;
	if (next_manifest_line(&cursor, &line, &line_length, &line_offset) != 1 ||
	    !line_has_value(line, line_length, "generator=",
			    expected_provenance->generator))
		goto provenance_mismatch;
	if (next_manifest_line(&cursor, &line, &line_length, &line_offset) != 1 ||
	    !line_has_value(line, line_length, "instructions_sha256=",
			    expected_provenance->instructions_sha256))
		goto provenance_mismatch;
	if (next_manifest_line(&cursor, &line, &line_length, &line_offset) != 1 ||
	    !line_has_value(line, line_length, "features_sha256=",
			    expected_provenance->features_sha256))
		goto provenance_mismatch;
	if (next_manifest_line(&cursor, &line, &line_length, &line_offset) != 1 ||
	    !line_has_value(line, line_length, "registers_sha256=",
			    expected_provenance->registers_sha256))
		goto provenance_mismatch;
	artifacts = calloc(ORLIX_TCTI_TARGET_ARTIFACT_MAX_COUNT, sizeof(*artifacts));
	if (!artifacts) {
		set_verify_result(result, ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_IO, ENOMEM,
				  NULL);
		goto out;
	}
	for (;;) {
		char calculated_digest[65];
		const char *recorded_digest;

		line_result = next_manifest_line(&cursor, &line, &line_length,
						 &line_offset);
		if (line_result != 1)
			goto manifest_format;
		if (line_length > strlen("bundle_sha256=") &&
		    !memcmp(line, "bundle_sha256=", strlen("bundle_sha256="))) {
			recorded_digest = line + strlen("bundle_sha256=");
			if ((size_t)(line + line_length - recorded_digest) != 64 ||
			    cursor.offset != cursor.length)
				goto manifest_format;
			orlix_tcti_target_artifact_sha256(manifest, line_offset,
					      calculated_digest);
			if (memcmp(recorded_digest, calculated_digest, 64)) {
				set_verify_result(result,
					ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_BUNDLE_DIGEST,
					EINVAL, NULL);
				errno = EINVAL;
				goto out;
			}
			break;
		}
		if (artifact_count >= ORLIX_TCTI_TARGET_ARTIFACT_MAX_COUNT) {
			set_verify_result(result,
				ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_RESOURCE_LIMIT,
				EFBIG, NULL);
			errno = EFBIG;
			goto out;
		}
		if (parse_artifact_line(line, line_length,
					&artifacts[artifact_count]))
			goto manifest_format;
		if (artifact_count &&
		    strcmp(artifacts[artifact_count - 1].name,
			   artifacts[artifact_count].name) >= 0)
			goto manifest_format;
		if (artifacts[artifact_count].length >
		    ORLIX_TCTI_TARGET_ARTIFACT_MAX_TOTAL_BYTES - total_bytes) {
			set_verify_result(result,
				ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_RESOURCE_LIMIT,
				EFBIG, artifacts[artifact_count].name);
			errno = EFBIG;
			goto out;
		}
		total_bytes += artifacts[artifact_count].length;
		artifact_count++;
	}
	if (!artifact_count)
		goto manifest_format;
	for (index = 0; index < artifact_count; index++)
		if (hash_artifact(generation_fd, &artifacts[index], result))
			goto out;
	{
		DIR *directory = fdopendir(dup(generation_fd));
		struct dirent *entry;

		if (!directory) {
			set_verify_result(result, ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_IO,
					  errno, NULL);
			goto out;
		}
		while ((entry = readdir(directory))) {
			if (!strcmp(entry->d_name, ".") ||
			    !strcmp(entry->d_name, "..") ||
			    !strcmp(entry->d_name, "manifest") ||
			    artifact_is_listed(artifacts, artifact_count,
					       entry->d_name))
				continue;
			set_verify_result(result,
				ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_EXTRA_ARTIFACT,
				EINVAL, entry->d_name);
			closedir(directory);
			errno = EINVAL;
			goto out;
		}
		if (closedir(directory)) {
			set_verify_result(result, ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_IO,
					  errno, NULL);
			goto out;
		}
	}
	return_value = 0;
	goto out;
selector_format:
	set_verify_result(result, ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_SELECTOR_FORMAT,
			  EINVAL, "current");
	errno = EINVAL;
	goto out;
manifest_format:
	set_verify_result(result, ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_MANIFEST_FORMAT,
			  EINVAL, "manifest");
	errno = EINVAL;
	goto out;
provenance_mismatch:
	set_verify_result(result, ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_PROVENANCE,
			  EINVAL, "manifest");
	errno = EINVAL;
out:
#undef EXPECT_MANIFEST_LINE
	free(artifacts);
	free(manifest);
	free(selector);
	if (generation_fd >= 0)
		close(generation_fd);
	if (root_fd >= 0)
		close(root_fd);
	return return_value;
}

static int appendf(char **buffer, size_t *length, size_t *capacity,
		   const char *format, ...)
{
	va_list arguments;
	int count;
	char *replacement;

	va_start(arguments, format);
	count = vsnprintf(NULL, 0, format, arguments);
	va_end(arguments);
	if (count < 0)
		return -1;
	if ((size_t)count > SIZE_MAX - *length - 1) {
		errno = EOVERFLOW;
		return -1;
	}
	if (*length + (size_t)count + 1 > *capacity) {
		size_t required = *length + (size_t)count + 1;
		size_t next = *capacity ? *capacity : 256;

		while (next < required) {
			if (next > SIZE_MAX / 2) {
				next = required;
				break;
			}
			next *= 2;
		}
		replacement = realloc(*buffer, next);
		if (!replacement)
			return -1;
		*buffer = replacement;
		*capacity = next;
	}
	va_start(arguments, format);
	vsnprintf(*buffer + *length, *capacity - *length, format, arguments);
	va_end(arguments);
	*length += (size_t)count;
	return 0;
}

static uint32_t publisher_rotate_right(uint32_t value, unsigned int shift)
{
	return (value >> shift) | (value << (32U - shift));
}

static void publisher_sha256_transform(struct publisher_sha256 *state,
				       const unsigned char block[64])
{
	static const uint32_t constants[64] = {
		0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
		0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
		0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
		0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
		0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
		0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
		0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
		0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
		0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
		0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
		0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
		0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
		0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
		0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
		0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
		0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
	};
	uint32_t schedule[64];
	uint32_t a, b, c, d, e, f, g, h;
	size_t index;

	for (index = 0; index < 16; index++)
		schedule[index] = ((uint32_t)block[index * 4] << 24) |
			((uint32_t)block[index * 4 + 1] << 16) |
			((uint32_t)block[index * 4 + 2] << 8) |
			(uint32_t)block[index * 4 + 3];
	for (index = 16; index < 64; index++) {
		uint32_t s0 = publisher_rotate_right(schedule[index - 15], 7) ^
			publisher_rotate_right(schedule[index - 15], 18) ^
			(schedule[index - 15] >> 3);
		uint32_t s1 = publisher_rotate_right(schedule[index - 2], 17) ^
			publisher_rotate_right(schedule[index - 2], 19) ^
			(schedule[index - 2] >> 10);

		schedule[index] = schedule[index - 16] + s0 +
			schedule[index - 7] + s1;
	}
	a = state->words[0];
	b = state->words[1];
	c = state->words[2];
	d = state->words[3];
	e = state->words[4];
	f = state->words[5];
	g = state->words[6];
	h = state->words[7];
	for (index = 0; index < 64; index++) {
		uint32_t sum1 = publisher_rotate_right(e, 6) ^
			publisher_rotate_right(e, 11) ^
			publisher_rotate_right(e, 25);
		uint32_t choose = (e & f) ^ (~e & g);
		uint32_t temporary1 = h + sum1 + choose + constants[index] +
			schedule[index];
		uint32_t sum0 = publisher_rotate_right(a, 2) ^
			publisher_rotate_right(a, 13) ^
			publisher_rotate_right(a, 22);
		uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
		uint32_t temporary2 = sum0 + majority;

		h = g;
		g = f;
		f = e;
		e = d + temporary1;
		d = c;
		c = b;
		b = a;
		a = temporary1 + temporary2;
	}
	state->words[0] += a;
	state->words[1] += b;
	state->words[2] += c;
	state->words[3] += d;
	state->words[4] += e;
	state->words[5] += f;
	state->words[6] += g;
	state->words[7] += h;
}

static void publisher_sha256_update(struct publisher_sha256 *state,
				    const unsigned char *data, size_t length)
{
	state->byte_count += length;
	while (length) {
		size_t available = sizeof(state->block) - state->used;
		size_t copied = length < available ? length : available;

		memcpy(state->block + state->used, data, copied);
		state->used += copied;
		data += copied;
		length -= copied;
		if (state->used == sizeof(state->block)) {
			publisher_sha256_transform(state, state->block);
			state->used = 0;
		}
	}
}

void orlix_tcti_target_artifact_sha256(const void *data, size_t length,
				 char digest[65])
{
	static const char hex[] = "0123456789abcdef";
	struct publisher_sha256 state = {
		.words = { 0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U,
			   0xa54ff53aU, 0x510e527fU, 0x9b05688cU,
			   0x1f83d9abU, 0x5be0cd19U },
	};
	uint64_t bits;
	unsigned char output[32];
	size_t index;

	if (!data && length) {
		digest[0] = '\0';
		return;
	}
	publisher_sha256_update(&state, data, length);
	bits = state.byte_count * 8U;
	state.block[state.used++] = 0x80;
	if (state.used > 56) {
		memset(state.block + state.used, 0, sizeof(state.block) -
		       state.used);
		publisher_sha256_transform(&state, state.block);
		state.used = 0;
	}
	memset(state.block + state.used, 0, 56 - state.used);
	for (index = 0; index < 8; index++)
		state.block[63 - index] = (unsigned char)(bits >> (index * 8));
	publisher_sha256_transform(&state, state.block);
	for (index = 0; index < 8; index++) {
		output[index * 4] = (unsigned char)(state.words[index] >> 24);
		output[index * 4 + 1] = (unsigned char)(state.words[index] >> 16);
		output[index * 4 + 2] = (unsigned char)(state.words[index] >> 8);
		output[index * 4 + 3] = (unsigned char)state.words[index];
	}
	for (index = 0; index < sizeof(output); index++) {
		digest[index * 2] = hex[output[index] >> 4];
		digest[index * 2 + 1] = hex[output[index] & 0xfU];
	}
	digest[64] = '\0';
}

static int build_manifest(const char *generation,
			  const struct orlix_tcti_target_artifact *artifacts,
			  size_t artifact_count, char **manifest,
			  size_t *manifest_length,
			  const struct orlix_tcti_target_artifact_provenance *provenance)
{
	char *buffer = NULL;
	size_t length = 0;
	size_t capacity = 0;
	size_t index;

	if (appendf(&buffer, &length, &capacity,
		    "ORLIX_TCTI_TARGET_ARTIFACT_SET_V2\ngeneration=%s\nschema=%s\n"
		    "generator=%s\ninstructions_sha256=%s\n"
		    "features_sha256=%s\nregisters_sha256=%s\n", generation,
		    provenance->schema, provenance->generator,
		    provenance->instructions_sha256, provenance->features_sha256,
		    provenance->registers_sha256))
		goto fail;
	for (index = 0; index < artifact_count; index++) {
		char digest[65];

		orlix_tcti_target_artifact_sha256(artifacts[index].data,
					     artifacts[index].length, digest);
		if (appendf(&buffer, &length, &capacity,
			    "artifact=%s %zu sha256=%s\n", artifacts[index].name,
			    artifacts[index].length, digest))
			goto fail;
	}
	{
		char bundle_digest[65];

		orlix_tcti_target_artifact_sha256(buffer, length, bundle_digest);
		if (appendf(&buffer, &length, &capacity, "bundle_sha256=%s\n",
			    bundle_digest))
			goto fail;
	}
	*manifest = buffer;
	*manifest_length = length;
	return 0;
fail:
	free(buffer);
	return -1;
}

/*
 * Failed generations are never selected. Remove our own bounded file set so
 * the same deterministic generation identifier can be retried. This is
 * intentionally descriptor-relative and never follows a generation symlink.
 */
static void discard_generation(int root_fd, const char *generation,
			       const struct orlix_tcti_target_artifact *artifacts,
			       size_t artifact_count)
{
	int generation_fd;
	size_t index;

	generation_fd = openat(root_fd, generation,
		O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	if (generation_fd < 0)
		return;
	(void)fchmod(generation_fd, 0755);
	for (index = 0; index < artifact_count; index++)
		(void)unlinkat(generation_fd, artifacts[index].name, 0);
	(void)unlinkat(generation_fd, "manifest", 0);
	(void)fsync(generation_fd);
	(void)close(generation_fd);
	(void)unlinkat(root_fd, generation, AT_REMOVEDIR);
}

const char *orlix_tcti_target_artifact_publish_error_name(
	enum orlix_tcti_target_artifact_publish_error error)
{
	switch (error) {
	case ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_OK: return "success";
	case ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_INVALID_ARGUMENT: return "invalid argument";
	case ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_INVALID_NAME: return "invalid artifact name";
	case ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_OPEN_ROOT: return "open publish root";
	case ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_CREATE_GENERATION: return "create generation";
	case ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_OPEN_GENERATION: return "open generation";
	case ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_TEMP_OPEN: return "create temporary file";
	case ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_WRITE: return "write temporary file";
	case ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_FILE_SYNC: return "sync temporary file";
	case ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_FILE_LOCK_SYNC: return "sync immutable temporary file";
	case ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_FILE_CLOSE: return "close temporary file";
	case ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_RENAME: return "rename published file";
	case ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_MANIFEST: return "build or publish manifest";
	case ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_GENERATION_SYNC: return "sync generation";
	case ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_LOCK_GENERATION: return "lock generation";
	case ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_LOCK_SYNC: return "sync locked generation";
	case ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_SELECTOR: return "publish current selector";
	case ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_SELECTOR_SYNC: return "sync publish root";
	case ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_ALREADY_EXISTS: return "generation already exists";
	}
	return "unknown publish error";
}

int orlix_tcti_target_artifact_publish(
	int build_root_fd, const char *publish_name, const char *generation,
	const struct orlix_tcti_target_artifact *artifacts, size_t artifact_count,
	const struct orlix_tcti_target_artifact_provenance *provenance,
	const struct orlix_tcti_target_artifact_publish_fault *fault,
	struct orlix_tcti_target_artifact_publish_result *result)
{
	struct publisher publisher = {
		.root_fd = -1,
		.generation_fd = -1,
		.fault = fault,
		.result = result,
	};
	char *manifest = NULL;
	size_t manifest_length = 0;
	char selector[256];
	int selector_length;
	size_t index;
	int saved_errno = 0;
	bool generation_created = false;
	bool selector_published = false;
	enum orlix_tcti_target_artifact_publish_error validation;

	if (result)
		*result = (struct orlix_tcti_target_artifact_publish_result) {
			.error = ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_OK,
		};
	validation = validate_artifacts(artifacts, artifact_count);
	if (build_root_fd < 0 ||
	    !valid_component(publish_name, ORLIX_TCTI_TARGET_ARTIFACT_MAX_NAME) ||
	    !valid_component(generation, ORLIX_TCTI_TARGET_ARTIFACT_MAX_GENERATION) ||
	    !valid_provenance(provenance) ||
	    validation != ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_OK) {
		set_result(&publisher, ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_INVALID_ARGUMENT,
		   ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE, EINVAL);
		if (validation == ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_INVALID_NAME)
			set_result(&publisher, validation,
				   ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE, EINVAL);
		errno = EINVAL;
		return -1;
	}
	if (mkdirat(build_root_fd, publish_name, 0755) && errno != EEXIST) {
		set_result(&publisher, ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_OPEN_ROOT,
			   ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE, errno);
		return -1;
	}
	publisher.root_fd = openat(build_root_fd, publish_name,
		O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	if (publisher.root_fd < 0) {
		set_result(&publisher, ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_OPEN_ROOT,
			   ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE, errno);
		return -1;
	}
	if (mkdirat(publisher.root_fd, generation, 0755)) {
		if (errno == EEXIST)
			set_result(&publisher,
				   ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_ALREADY_EXISTS,
				   ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE, errno);
		else
			set_result(&publisher,
				   ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_CREATE_GENERATION,
				   ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE, errno);
		goto fail;
	}
	generation_created = true;
	publisher.generation_fd = openat(publisher.root_fd, generation,
		O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	if (publisher.generation_fd < 0) {
		set_result(&publisher, ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_OPEN_GENERATION,
			   ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE, errno);
		goto fail;
	}
	for (index = 0; index < artifact_count; index++)
		if (publish_file(&publisher, artifacts[index].name,
				 artifacts[index].data, artifacts[index].length,
				 ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_OK))
			goto fail;
	if (fail_stage(&publisher, ORLIX_TCTI_TARGET_ARTIFACT_STAGE_MANIFEST)) {
		errno = EIO;
		set_result(&publisher, ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_MANIFEST,
			   ORLIX_TCTI_TARGET_ARTIFACT_STAGE_MANIFEST, errno);
		goto fail;
	}
	if (build_manifest(generation, artifacts, artifact_count, &manifest,
			   &manifest_length, provenance) ||
	    publish_file(&publisher, "manifest", manifest, manifest_length,
			 ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_MANIFEST)) {
		if (!publisher.result ||
		    publisher.result->error == ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_OK)
			set_result(&publisher, ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_MANIFEST,
				   ORLIX_TCTI_TARGET_ARTIFACT_STAGE_MANIFEST, errno);
		goto fail;
	}
	free(manifest);
	manifest = NULL;
	if (fail_stage(&publisher, ORLIX_TCTI_TARGET_ARTIFACT_STAGE_GENERATION_SYNC)) {
		errno = EIO;
		set_result(&publisher, ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_GENERATION_SYNC,
			   ORLIX_TCTI_TARGET_ARTIFACT_STAGE_GENERATION_SYNC, errno);
		goto fail;
	}
	if (fsync(publisher.generation_fd)) {
		set_result(&publisher, ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_GENERATION_SYNC,
			   ORLIX_TCTI_TARGET_ARTIFACT_STAGE_GENERATION_SYNC, errno);
		goto fail;
	}
	if (fail_stage(&publisher, ORLIX_TCTI_TARGET_ARTIFACT_STAGE_LOCK_GENERATION)) {
		errno = EIO;
		set_result(&publisher, ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_LOCK_GENERATION,
			   ORLIX_TCTI_TARGET_ARTIFACT_STAGE_LOCK_GENERATION, errno);
		goto fail;
	}
	if (fchmod(publisher.generation_fd, 0555)) {
		set_result(&publisher, ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_LOCK_GENERATION,
			   ORLIX_TCTI_TARGET_ARTIFACT_STAGE_LOCK_GENERATION, errno);
		goto fail;
	}
	if (fail_stage(&publisher, ORLIX_TCTI_TARGET_ARTIFACT_STAGE_LOCK_SYNC)) {
		errno = EIO;
		set_result(&publisher, ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_LOCK_SYNC,
			   ORLIX_TCTI_TARGET_ARTIFACT_STAGE_LOCK_SYNC, errno);
		goto fail;
	}
	if (fsync(publisher.generation_fd)) {
		set_result(&publisher, ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_LOCK_SYNC,
			   ORLIX_TCTI_TARGET_ARTIFACT_STAGE_LOCK_SYNC, errno);
		goto fail;
	}
	selector_length = snprintf(selector, sizeof(selector),
		"ORLIX_TCTI_TARGET_ARTIFACT_CURRENT_V1\ngeneration=%s\nmanifest=manifest\n",
		generation);
	if (selector_length < 0 || (size_t)selector_length >= sizeof(selector)) {
		errno = EOVERFLOW;
		set_result(&publisher, ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_SELECTOR,
			   ORLIX_TCTI_TARGET_ARTIFACT_STAGE_SELECTOR, errno);
		goto fail;
	}
	/* current is published in the root, never inside the immutable generation. */
	close(publisher.generation_fd);
	publisher.generation_fd = publisher.root_fd;
	if (fail_stage(&publisher, ORLIX_TCTI_TARGET_ARTIFACT_STAGE_SELECTOR)) {
		errno = EIO;
		set_result(&publisher, ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_SELECTOR,
			   ORLIX_TCTI_TARGET_ARTIFACT_STAGE_SELECTOR, errno);
		goto fail;
	}
	if (publish_file(&publisher, "current", selector, (size_t)selector_length,
			 ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_SELECTOR))
		goto fail;
	selector_published = true;
	if (fail_stage(&publisher, ORLIX_TCTI_TARGET_ARTIFACT_STAGE_SELECTOR_SYNC)) {
		errno = EIO;
		set_result(&publisher, ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_SELECTOR_SYNC,
			   ORLIX_TCTI_TARGET_ARTIFACT_STAGE_SELECTOR_SYNC, errno);
		goto fail;
	}
	if (fsync(publisher.root_fd)) {
		set_result(&publisher, ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_SELECTOR_SYNC,
			   ORLIX_TCTI_TARGET_ARTIFACT_STAGE_SELECTOR_SYNC, errno);
		goto fail;
	}
	close(publisher.root_fd);
	return 0;
fail:
	saved_errno = errno;
	free(manifest);
	if (publisher.generation_fd >= 0 &&
	    publisher.generation_fd != publisher.root_fd)
		close(publisher.generation_fd);
	if (publisher.root_fd >= 0) {
		if (generation_created && !selector_published)
			discard_generation(publisher.root_fd, generation, artifacts,
					   artifact_count);
		close(publisher.root_fd);
	}
	errno = saved_errno;
	return -1;
}
