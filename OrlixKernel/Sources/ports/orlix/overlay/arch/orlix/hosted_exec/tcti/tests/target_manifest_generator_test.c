/* SPDX-License-Identifier: GPL-2.0-only */
#define TARGET_MANIFEST_GENERATOR_NO_MAIN
#include "target_manifest_generator.c"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(expression) do { \
	if (!(expression)) { \
		fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
		return -1; \
	} \
} while (0)

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

static char *write_temporary_file(const char *data, size_t length)
{
	static const char template[] = "/tmp/orlix-target-manifest-XXXXXX";
	char *path;
	int fd;
	size_t written = 0;

	path = malloc(sizeof(template));
	if (!path)
		return NULL;
	memcpy(path, template, sizeof(template));
	fd = mkstemp(path);
	if (fd < 0)
		goto fail;
	while (written < length) {
		ssize_t result = write(fd, data + written, length - written);

		if (result <= 0) {
			close(fd);
			goto fail;
		}
		written += (size_t)result;
	}
	if (close(fd))
		goto fail;
	return path;
fail:
	unlink(path);
	free(path);
	return NULL;
}

static int files_are_identical(const char *left_path, const char *right_path)
{
	char *left;
	char *right;
	size_t left_length;
	size_t right_length;
	int same;

	left = read_file(left_path, &left_length);
	right = read_file(right_path, &right_length);
	if (!left || !right) {
		free(left);
		free(right);
		return -1;
	}
	same = left_length == right_length &&
		!memcmp(left, right, left_length);
	free(left);
	free(right);
	return same ? 0 : -1;
}

static int file_is_empty(const char *path)
{
	char *data;
	size_t length;
	int empty;

	data = read_file(path, &length);
	empty = data && !length;
	free(data);
	return empty ? 0 : -1;
}

static char *read_descriptor(int fd)
{
	char *data = NULL;
	size_t capacity = 0;
	size_t length = 0;

	for (;;) {
		char buffer[256];
		ssize_t count = read(fd, buffer, sizeof(buffer));

		if (count < 0)
			goto fail;
		if (!count)
			break;
		if (length + (size_t)count + 1 > capacity) {
			size_t new_capacity = capacity ? capacity * 2 : 512;
			char *replacement;

			while (new_capacity < length + (size_t)count + 1)
				new_capacity *= 2;
			replacement = realloc(data, new_capacity);
			if (!replacement)
				goto fail;
			data = replacement;
			capacity = new_capacity;
		}
		memcpy(data + length, buffer, (size_t)count);
		length += (size_t)count;
	}
	if (!data) {
		data = malloc(1);
		if (!data)
			return NULL;
	}
	data[length] = '\0';
	return data;
fail:
	free(data);
	return NULL;
}

static int run_generator(const char *generator, const char *source,
			 const char *expected, const char *stdout_path,
			 char **stderr_text, int *exit_status)
{
	int diagnostics[2] = { -1, -1 };
	int stdout_fd = -1;
	pid_t child;
	int status;

	if (pipe(diagnostics))
		goto fail;
	stdout_fd = open(stdout_path, O_WRONLY | O_TRUNC);
	if (stdout_fd < 0)
		goto fail;
	child = fork();
	if (child < 0)
		goto fail;
	if (!child) {
		if (dup2(stdout_fd, STDOUT_FILENO) < 0 ||
		    dup2(diagnostics[1], STDERR_FILENO) < 0)
			_exit(127);
		close(stdout_fd);
		close(diagnostics[0]);
		close(diagnostics[1]);
		if (expected)
			execl(generator, generator, "--check", source, expected,
			      (char *)NULL);
		else
			execl(generator, generator, source, (char *)NULL);
		_exit(127);
	}
	close(stdout_fd);
	close(diagnostics[1]);
	*stderr_text = read_descriptor(diagnostics[0]);
	close(diagnostics[0]);
	if (!*stderr_text || waitpid(child, &status, 0) != child)
		return -1;
	*exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
	return 0;
fail:
	if (diagnostics[0] >= 0)
		close(diagnostics[0]);
	if (diagnostics[1] >= 0)
		close(diagnostics[1]);
	if (stdout_fd >= 0)
		close(stdout_fd);
	return -1;
}

static char *replace_once(const char *source, size_t source_length,
			  const char *from, const char *to, size_t *length)
{
	const char *where = strstr(source, from);
	size_t before;
	size_t from_length = strlen(from);
	size_t to_length = strlen(to);
	char *copy;

	if (!where)
		return NULL;
	before = (size_t)(where - source);
	copy = malloc(source_length - from_length + to_length + 1);
	if (!copy)
		return NULL;
	memcpy(copy, source, before);
	memcpy(copy + before, to, to_length);
	memcpy(copy + before + to_length, where + from_length,
	       source_length - before - from_length);
	*length = source_length - from_length + to_length;
	copy[*length] = '\0';
	return copy;
}

static const char *matching_delimiter(const char *open, char left, char right)
{
	const char *cursor;
	size_t depth = 0;
	bool quoted = false;
	bool escaped = false;

	for (cursor = open; *cursor; cursor++) {
		if (quoted) {
			if (escaped)
				escaped = false;
			else if (*cursor == '\\')
				escaped = true;
			else if (*cursor == '"')
				quoted = false;
			continue;
		}
		if (*cursor == '"') {
			quoted = true;
			continue;
		}
		if (*cursor == left)
			depth++;
		else if (*cursor == right && --depth == 0)
			return cursor;
	}
	return NULL;
}

static const char *json_value_after_key(const char *key, const char *limit,
					 const char *value)
{
	const char *cursor = key + strlen("\"name\"");
	size_t value_length = strlen(value);

	while (cursor < limit && (*cursor == ' ' || *cursor == '\t' ||
				  *cursor == '\n' || *cursor == '\r'))
		cursor++;
	if (cursor == limit || *cursor++ != ':')
		return NULL;
	while (cursor < limit && (*cursor == ' ' || *cursor == '\t' ||
				  *cursor == '\n' || *cursor == '\r'))
		cursor++;
	if (cursor == limit || *cursor++ != '"' ||
	    (size_t)(limit - cursor) < value_length + 1 ||
	    memcmp(cursor, value, value_length) || cursor[value_length] != '"')
		return NULL;
	return cursor;
}

static const char *object_start_at(const char *start, const char *inside)
{
	const char *objects[64];
	const char *cursor;
	size_t depth = 0;
	bool quoted = false;
	bool escaped = false;

	for (cursor = start; cursor < inside; cursor++) {
		if (quoted) {
			if (escaped)
				escaped = false;
			else if (*cursor == '\\')
				escaped = true;
			else if (*cursor == '"')
				quoted = false;
			continue;
		}
		if (*cursor == '"')
			quoted = true;
		else if (*cursor == '{') {
			if (depth == sizeof(objects) / sizeof(objects[0]))
				return NULL;
			objects[depth++] = cursor;
		} else if (*cursor == '}' && depth) {
			depth--;
		}
	}
	return depth ? objects[depth - 1] : NULL;
}

static const char *a64_children_open(const char *source)
{
	const char *instructions = strstr(source, "\"instructions\"");
	const char *sets;
	const char *sets_end;
	const char *cursor;

	if (!instructions || !(sets = strchr(instructions, '[')) ||
	    !(sets_end = matching_delimiter(sets, '[', ']')))
		return NULL;
	for (cursor = sets; cursor < sets_end;) {
		const char *name = strstr(cursor, "\"name\"");
		const char *object;
		const char *object_end;
		const char *children;

		if (!name || name >= sets_end)
			return NULL;
		cursor = name + strlen("\"name\"");
		if (!json_value_after_key(name, sets_end, "A64"))
			continue;
		object = object_start_at(sets, name);
		if (!object || !(object_end = matching_delimiter(object, '{', '}')))
			return NULL;
		children = strstr(object, "\"children\"");
		if (!children || children >= object_end)
			return NULL;
		return strchr(children, '[');
	}
	return NULL;
}

static char *add_leaf(const char *source, size_t source_length, size_t *length)
{
	static const char leaf[] =
		"{\"_type\":\"Instruction.Instruction\","
		"\"condition\":{\"_type\":\"AST.Bool\",\"value\":true},"
		"\"name\":\"target_manifest_added_leaf\","
		"\"operation_id\":\"target_manifest_added_leaf\","
		"\"assembly\":{\"symbols\":[{\"_type\":"
		"\"Instruction.Symbols.Literal\",\"value\":\"ADDED\"}]},"
		"\"encoding\":{\"width\":32,\"values\":[{\"_type\":"
		"\"Instruction.Encodeset.Bits\",\"range\":{\"start\":0,"
		"\"width\":32},\"value\":{\"value\":"
		"\"'00000000000000000000000000000000'\"}}]}}";
	const char *open = a64_children_open(source);
	const char *close;
	size_t before;
	char *copy;

	if (!open || !(close = matching_delimiter(open, '[', ']')))
		return NULL;
	before = (size_t)(close - source);
	copy = malloc(source_length + 1 + sizeof(leaf) - 1 + 1);
	if (!copy)
		return NULL;
	memcpy(copy, source, before);
	copy[before] = ',';
	memcpy(copy + before + 1, leaf, sizeof(leaf) - 1);
	memcpy(copy + before + 1 + sizeof(leaf) - 1, close,
	       source_length - before + 1);
	*length = source_length + sizeof(leaf);
	return copy;
}

static char *remove_a64_leaves(const char *source, size_t source_length,
			       size_t *length)
{
	const char *open = a64_children_open(source);
	const char *close;
	char *copy;
	size_t before;

	if (!open || !(close = matching_delimiter(open, '[', ']')))
		return NULL;
	before = (size_t)(open - source);
	copy = malloc(before + 3 + (source_length - (size_t)(close - source)));
	if (!copy)
		return NULL;
	memcpy(copy, source, before);
	copy[before] = '[';
	memcpy(copy + before + 1, close,
	       source_length - (size_t)(close - source) + 1);
	*length = before + 1 + source_length - (size_t)(close - source);
	return copy;
}

static int output_is_empty_after_failure(
	const char *source, size_t length,
	enum target_manifest_generator_error expected_error)
{
	FILE *output = tmpfile();
	enum target_manifest_generator_error result;

	CHECK(output != NULL);
	result = target_manifest_generator_emit(source, length, output);
	CHECK(result == expected_error);
	CHECK(fflush(output) == 0);
	CHECK(ftell(output) == 0);
	fclose(output);
	return 0;
}

static int valid_pinned_source_emits(const char *source, size_t length)
{
	FILE *output = tmpfile();
	char header[1024] = { 0 };

	CHECK(output != NULL);
	CHECK(target_manifest_generator_emit(source, length, output) ==
	      TCTI_TARGET_MANIFEST_GENERATOR_OK);
	CHECK(fflush(output) == 0);
	CHECK(fseek(output, 0, SEEK_SET) == 0);
	CHECK(fread(header, 1, sizeof(header) - 1, output) != 0);
	CHECK(strstr(header, "TCTI_A64_SOURCE_MANIFEST_SOURCE") != NULL);
	CHECK(strstr(header, "vFATAp1-A") != NULL);
	fclose(output);
	return 0;
}

static int metadata_mutations_fail_without_output(const char *source,
					 size_t source_length)
{
	static const struct {
		const char *from;
		const char *to;
	} mutations[] = {
		{ "vFATAp1-A", "vFATAp1-B" },
		{ "\"build\": \"818\"", "\"build\": \"819\"" },
		{ "2026-06_rel", "2026-06_bad" },
		{ "\"schema\": \"2.9.5\"", "\"schema\": \"2.9.6\"" },
	};
	size_t index;

	for (index = 0; index < sizeof(mutations) / sizeof(mutations[0]); index++) {
		char *mutation;
		size_t mutation_length;

		mutation = replace_once(source, source_length, mutations[index].from,
					mutations[index].to, &mutation_length);
		CHECK(mutation != NULL);
		CHECK(output_is_empty_after_failure(
			mutation, mutation_length,
			TCTI_TARGET_MANIFEST_GENERATOR_METADATA) == 0);
		free(mutation);
	}
	return 0;
}

static int semantic_and_leaf_count_mutations_fail_without_output(
	const char *source, size_t source_length)
{
	char *mutation;
	size_t mutation_length;

	CHECK(output_is_empty_after_failure(
		"{\"instructions\": [", strlen("{\"instructions\": ["),
		TCTI_TARGET_MANIFEST_GENERATOR_PARSE) == 0);

	mutation = replace_once(source, source_length, "ADD_32_addsub_imm",
				"BDD_32_addsub_imm", &mutation_length);
	CHECK(mutation != NULL);
	CHECK(output_is_empty_after_failure(
		mutation, mutation_length,
		TCTI_TARGET_MANIFEST_GENERATOR_DIGEST) == 0);
	free(mutation);

	mutation = add_leaf(source, source_length, &mutation_length);
	CHECK(mutation != NULL);
	CHECK(output_is_empty_after_failure(
		mutation, mutation_length,
		TCTI_TARGET_MANIFEST_GENERATOR_COUNT) == 0);
	free(mutation);

	mutation = remove_a64_leaves(source, source_length, &mutation_length);
	CHECK(mutation != NULL);
	CHECK(output_is_empty_after_failure(
		mutation, mutation_length,
		TCTI_TARGET_MANIFEST_GENERATOR_COUNT) == 0);
	free(mutation);
	return 0;
}

static int cli_subprocess_tests(const char *generator, const char *source_path,
				const char *manifest_path, const char *source,
				size_t source_length)
{
	char *stdout_path = NULL;
	char *expected_copy = NULL;
	char *mutation_path = NULL;
	char *missing_path = NULL;
	char *stderr_text = NULL;
	char *mutation = NULL;
	char *manifest = NULL;
	size_t manifest_length;
	size_t mutation_length;
	int exit_status;
	int result = -1;

	stdout_path = write_temporary_file("", 0);
	CHECK(stdout_path != NULL);
	CHECK(run_generator(generator, source_path, NULL, stdout_path,
				    &stderr_text, &exit_status) == 0);
	CHECK(exit_status == EXIT_SUCCESS);
	CHECK(stderr_text[0] == '\0');
	CHECK(files_are_identical(stdout_path, manifest_path) == 0);
	free(stderr_text);
	stderr_text = NULL;

	manifest = read_file(manifest_path, &manifest_length);
	CHECK(manifest != NULL);
	expected_copy = write_temporary_file(manifest, manifest_length);
	CHECK(expected_copy != NULL);
	mutation = replace_once(source, source_length, "ADD_32_addsub_imm",
				"BDD_32_addsub_imm", &mutation_length);
	CHECK(mutation != NULL);
	mutation_path = write_temporary_file(mutation, mutation_length);
	CHECK(mutation_path != NULL);
	CHECK(run_generator(generator, mutation_path, expected_copy, stdout_path,
				    &stderr_text, &exit_status) == 0);
	CHECK(exit_status != EXIT_SUCCESS);
	CHECK(file_is_empty(stdout_path) == 0);
	CHECK(files_are_identical(manifest_path, expected_copy) == 0);
	CHECK(strstr(stderr_text,
		     "target manifest generator: digest failure") != NULL);
	free(stderr_text);
	stderr_text = NULL;

	missing_path = write_temporary_file("", 0);
	CHECK(missing_path != NULL);
	CHECK(unlink(missing_path) == 0);
	CHECK(run_generator(generator, missing_path, NULL, stdout_path, &stderr_text,
				    &exit_status) == 0);
	CHECK(exit_status != EXIT_SUCCESS);
	CHECK(file_is_empty(stdout_path) == 0);
	CHECK(strstr(stderr_text,
		     "target manifest generator: I/O failure") != NULL);
	result = 0;

	free(stderr_text);
	free(manifest);
	free(mutation);
	if (stdout_path) {
		unlink(stdout_path);
		free(stdout_path);
	}
	if (expected_copy) {
		unlink(expected_copy);
		free(expected_copy);
	}
	if (mutation_path) {
		unlink(mutation_path);
		free(mutation_path);
	}
	free(missing_path);
	return result;
}

int main(int argc, char **argv)
{
	char *source;
	size_t source_length;

	if (argc != 4) {
		fprintf(stderr, "usage: %s generator Instructions.json manifest.def\n",
			argv[0]);
		return 2;
	}
	source = read_file(argv[2], &source_length);
	if (!source) {
		perror(argv[2]);
		return 1;
	}
	if (valid_pinned_source_emits(source, source_length) ||
	    metadata_mutations_fail_without_output(source, source_length) ||
	    semantic_and_leaf_count_mutations_fail_without_output(source,
						 source_length) ||
	    cli_subprocess_tests(argv[1], argv[2], argv[3], source,
				 source_length)) {
		free(source);
		return 1;
	}
	free(source);
	puts("PASS target manifest generator pin and no-output checks");
	return 0;
}
