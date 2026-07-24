// SPDX-License-Identifier: GPL-2.0-only
/*
 * The pinned Arm JSON archive is an explicit refresh/audit input.  Ordinary
 * host tests, KUnit, and the kernel TCTI objects consume generated C data and
 * must never acquire a runtime dependency on that archive or its importers.
 */

#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_FILE_BYTES (256U * 1024U * 1024U)
#define MAX_INCLUDE_FILES 4096U
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static int failures;

struct include_scan {
	char *visited[MAX_INCLUDE_FILES];
	size_t visited_count;
};

static void fail(const char *path, const char *reason)
{
	fprintf(stderr, "host-lane boundary: %s: %s\n", path, reason);
	failures++;
}

static void fail_target(const char *path, const char *target,
			const char *reason)
{
	fprintf(stderr, "host-lane boundary: %s: target %s: %s\n", path,
		target ? target : "<none>", reason);
	failures++;
}

static char *join_path(const char *left, const char *right)
{
	size_t left_length = strlen(left);
	size_t right_length = strlen(right);
	char *result = malloc(left_length + 1 + right_length + 1);

	if (!result)
		return NULL;
	memcpy(result, left, left_length);
	result[left_length] = '/';
	memcpy(result + left_length + 1, right, right_length + 1);
	return result;
}

static char *read_file(const char *path, size_t *length_out)
{
	FILE *file;
	long size;
	char *text;

	file = fopen(path, "rb");
	if (!file) {
		fail(path, strerror(errno));
		return NULL;
	}
	if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0 ||
		fseek(file, 0, SEEK_SET) != 0) {
		fail(path, "cannot determine file length");
		fclose(file);
		return NULL;
	}
	if ((unsigned long)size > MAX_FILE_BYTES) {
		fail(path, "file exceeds host-lane scanner limit");
		fclose(file);
		return NULL;
	}
	text = malloc((size_t)size + 1);
	if (!text) {
		fail(path, "out of memory");
		fclose(file);
		return NULL;
	}
	if (fread(text, 1, (size_t)size, file) != (size_t)size) {
		fail(path, "cannot read file");
		free(text);
		fclose(file);
		return NULL;
	}
	fclose(file);
	text[size] = '\0';
	*length_out = (size_t)size;
	return text;
}

static bool contains_case_insensitive(const char *text, const char *needle)
{
	size_t needle_length = strlen(needle);
	const char *cursor;

	if (!needle_length)
		return true;
	for (cursor = text; *cursor; cursor++) {
		size_t index;
		for (index = 0; index < needle_length; index++) {
			unsigned char a = (unsigned char)cursor[index];
			unsigned char b = (unsigned char)needle[index];

			if (!a)
				break;
			if (a >= 'A' && a <= 'Z')
				a = (unsigned char)(a - 'A' + 'a');
			if (b >= 'A' && b <= 'Z')
				b = (unsigned char)(b - 'A' + 'a');
			if (a != b)
				break;
		}
		if (index == needle_length)
			return true;
	}
	return false;
}

static bool contains_forbidden(const char *text, const char **match)
{
	static const char *const forbidden[] = {
		".json", "ORLIX_AARCHMRS_", "target_inventory_import",
		"target_feature_model", "target_register_model",
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(forbidden); index++) {
		if (contains_case_insensitive(text, forbidden[index])) {
			*match = forbidden[index];
			return true;
		}
	}
	return false;
}

static bool contains_raw_json_consumer(const char *text)
{
	static const char *const consumers[] = {
		"target_inventory_import.c", "target_feature_model.c",
		"target_feature_sat.c", "target_feature_typed_ir.c",
		"target_register_model.c", "target_hwcap_cohort_catalog.c",
		"target_manifest_generator.c", "audit_inventory.c",
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(consumers); index++) {
		if (strstr(text, consumers[index]) != NULL)
			return true;
	}
	return false;
}

static void check_json_free_file(const char *path)
{
	char *text;
	const char *match;
	size_t length;

	text = read_file(path, &length);
	if (!text)
		return;
	(void)length;
	if (contains_forbidden(text, &match))
		fail(path, match);
	free(text);
}

static bool source_file_name(const char *name)
{
	size_t length = strlen(name);

	return (length > 2 && !strcmp(name + length - 2, ".c")) ||
		(length > 2 && !strcmp(name + length - 2, ".h")) ||
		!strcmp(name, "Makefile") || !strcmp(name, "Kbuild");
}

static bool c_source_file_name(const char *name)
{
	size_t length = strlen(name);

	return length > 2 && !strcmp(name + length - 2, ".c");
}

static const char *base_name(const char *path)
{
	const char *slash = strrchr(path, '/');

	return slash ? slash + 1 : path;
}

static bool raw_arm_source_name(const char *name)
{
	static const char *const exact[] = {
		"audit_inventory.c",
		"target_classification_generator.c",
		"target_classification_generator.h",
		"target_manifest_generator.c",
		"target_manifest_generator.h",
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(exact); index++) {
		if (!strcmp(name, exact[index]))
			return true;
	}
	return !strncmp(name, "target_inventory_import", 23) ||
		!strncmp(name, "target_feature_model", 20) ||
		!strncmp(name, "target_feature_sat", 18) ||
		!strncmp(name, "target_feature_typed_ir", 23) ||
		!strncmp(name, "target_register_model", 21) ||
		(strstr(name, "_artifact_generator") != NULL);
}

static bool raw_arm_source_content(const char *text)
{
	static const char *const signatures[] = {
		"tcti_target_inventory_import(",
		"tcti_target_feature_model_import(",
		"tcti_register_model_import(",
		"#include \"target_inventory_import.h\"",
		"#include \"target_feature_model.h\"",
		"#include \"target_register_model.h\"",
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(signatures); index++) {
		if (strstr(text, signatures[index]))
			return true;
	}
	return false;
}

static bool include_visited(struct include_scan *scan, const char *path)
{
	size_t index;

	for (index = 0; index < scan->visited_count; index++) {
		if (!strcmp(scan->visited[index], path))
			return true;
	}
	if (scan->visited_count == ARRAY_SIZE(scan->visited)) {
		fail(path, "C include closure exceeds boundary-test limit");
		return true;
	}
	scan->visited[scan->visited_count] = strdup(path);
	if (!scan->visited[scan->visited_count]) {
		fail(path, "out of memory");
		return true;
	}
	scan->visited_count++;
	return false;
}

static char *include_path(const char *source, const char *include)
{
	const char *slash = strrchr(source, '/');
	size_t directory_length = slash ? (size_t)(slash - source) : 0;
	size_t include_length = strlen(include);
	char *path;

	if (!directory_length)
		return strdup(include);
	path = malloc(directory_length + 1 + include_length + 1);
	if (!path)
		return NULL;
	memcpy(path, source, directory_length);
	path[directory_length] = '/';
	memcpy(path + directory_length + 1, include, include_length + 1);
	return path;
}

static void scan_include_closure(const char *path, struct include_scan *scan)
{
	const char *name = base_name(path);
	char *text;
	char *line;
	size_t length;

	if (include_visited(scan, path))
		return;
	text = read_file(path, &length);
	if (!text)
		return;
	(void)length;
	if (strcmp(name, "host_lane_boundary_test.c") &&
	    (raw_arm_source_name(name) || raw_arm_source_content(text)))
		fail(path, "normal C include closure reaches raw Arm source machinery");
	line = text;
	while (line && *line) {
		char *next = strchr(line, '\n');
		char *cursor = line;

		if (next)
			*next++ = '\0';
		while (*cursor == ' ' || *cursor == '\t')
			cursor++;
		if (*cursor == '#') {
			char *quote;
			char *end;

			cursor++;
			while (*cursor == ' ' || *cursor == '\t')
				cursor++;
			if (!strncmp(cursor, "include", 7) &&
			    (cursor[7] == ' ' || cursor[7] == '\t')) {
				quote = strchr(cursor + 7, '"');
				end = quote ? strchr(quote + 1, '"') : NULL;
				if (quote && end) {
					struct stat status;
					char *resolved;

					*end = '\0';
					resolved = include_path(path, quote + 1);
					if (!resolved)
						fail(path, "out of memory");
					else {
						if (!lstat(resolved, &status) &&
						    S_ISREG(status.st_mode))
							scan_include_closure(resolved, scan);
						free(resolved);
					}
				}
			}
		}
		line = next;
	}
	free(text);
}

static void scan_overlay_tree(const char *directory, struct include_scan *scan)
{
	DIR *stream;
	struct dirent *entry;

	stream = opendir(directory);
	if (!stream) {
		fail(directory, strerror(errno));
		return;
	}
	while ((entry = readdir(stream)) != NULL) {
		char *path;
		struct stat status;

		if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
			continue;
		path = join_path(directory, entry->d_name);
		if (!path) {
			fail(directory, "out of memory");
			break;
		}
		if (lstat(path, &status) != 0) {
			fail(path, strerror(errno));
		} else if (S_ISDIR(status.st_mode)) {
			scan_overlay_tree(path, scan);
		} else if (S_ISREG(status.st_mode) && source_file_name(entry->d_name)) {
			bool raw_source = false;
			char *text;
			size_t length;

			text = read_file(path, &length);
			if (text) {
				(void)length;
				raw_source =
					strcmp(entry->d_name, "host_lane_boundary_test.c") &&
					(raw_arm_source_name(entry->d_name) ||
					 raw_arm_source_content(text));
				if (raw_source)
					fail(path, "raw Arm source machinery must live outside OrlixKernel overlay");
				free(text);
			}
			if (c_source_file_name(entry->d_name) && !raw_source)
				scan_include_closure(path, scan);
		}
		free(path);
	}
	closedir(stream);
}

struct make_target {
	char *name;
	char *dependency_lines[16];
	size_t dependency_count;
	bool raw_json_consumer;
	bool visited;
};

#define MAX_MAKE_TARGETS 512U

static struct make_target *find_target(struct make_target *targets,
					       size_t target_count, const char *name)
{
	size_t index;

	for (index = 0; index < target_count; index++) {
		if (!strcmp(targets[index].name, name))
			return &targets[index];
	}
	return NULL;
}

static struct make_target *record_target(struct make_target *targets,
						 size_t *target_count, char *name,
						 char *dependencies, const char *path)
{
	struct make_target *target;

	target = find_target(targets, *target_count, name);
	if (target) {
		if (target->dependency_count == ARRAY_SIZE(target->dependency_lines)) {
			fail(path, "Make target has too many dependency declarations");
			return NULL;
		}
		target->dependency_lines[target->dependency_count++] = dependencies;
		return target;
	}
	if (*target_count == MAX_MAKE_TARGETS) {
		fail(path, "Make target graph exceeds boundary-test limit");
		return NULL;
	}
	target = &targets[(*target_count)++];
	*target = (struct make_target) {
		.name = name,
	};
	target->dependency_lines[target->dependency_count++] = dependencies;
	return target;
}

static struct make_target *record_target_definition(struct make_target *targets,
							     size_t *target_count, char *line,
							     const char *path)
{
	char *colon;
	char *assignment;
	char *name;
	char *cursor;
	struct make_target *first = NULL;

	if (*line == '\t' || *line == ' ' || *line == '#' || !*line)
		return NULL;
	colon = strchr(line, ':');
	assignment = strchr(line, '=');
	if (!colon || (assignment && (assignment < colon || assignment == colon + 1)))
		return NULL;
	*colon++ = '\0';
	name = line;
	while (*name) {
		while (*name == ' ' || *name == '\t')
			name++;
		if (!*name)
			break;
		cursor = name;
		while (*cursor && *cursor != ' ' && *cursor != '\t')
			cursor++;
		if (*cursor)
			*cursor++ = '\0';
		if (strcmp(name, ".PHONY") && strcmp(name, ".DEFAULT_GOAL")) {
			struct make_target *target = record_target(targets, target_count,
									 name, colon, path);
			if (!first)
				first = target;
		}
		name = cursor;
	}
	return first;
}

static void check_host_dependency_closure(struct make_target *target,
						  struct make_target *targets,
						  size_t target_count, const char *path)
{
	size_t dependency_line;
	char *name;

	if (!target || target->visited)
		return;
	target->visited = true;
	if (!strcmp(target->name, "tcti-isa-maintainer-source-check") ||
	    !strcmp(target->name, "tcti-isa-refresh")) {
		fail(path, "ordinary host lane depends on a raw Arm JSON lane");
		return;
	}
	if (target->raw_json_consumer) {
		fail(path, "ordinary host dependency closure reaches a raw Arm JSON consumer");
		return;
	}
	for (dependency_line = 0; dependency_line < target->dependency_count;
	     dependency_line++) {
		char *dependencies = target->dependency_lines[dependency_line];

		while (dependencies && *dependencies) {
			struct make_target *dependency;
			char saved;

			while (*dependencies == ' ' || *dependencies == '\t' ||
				*dependencies == '|')
				dependencies++;
			if (!*dependencies || *dependencies == '#')
				break;
			name = dependencies;
			while (*dependencies && *dependencies != ' ' &&
				*dependencies != '\t' && *dependencies != '#')
				dependencies++;
			saved = *dependencies;
			*dependencies = '\0';
			if (strstr(name, "$") != NULL)
				fail(path, "ordinary host dependency closure is not statically auditable");
			else {
				dependency = find_target(targets, target_count, name);
				if (dependency)
					check_host_dependency_closure(dependency, targets,
								      target_count, path);
			}
			*dependencies = saved;
			if (!saved)
				break;
		}
	}
}

static void check_makefile_lanes(const char *path)
{
	char *text;
	char *line;
	char *next;
	struct make_target targets[MAX_MAKE_TARGETS] = { 0 };
	struct make_target *current_target = NULL;
	size_t target_count = 0;
	size_t length;
	size_t target_index;

	text = read_file(path, &length);
	if (!text)
		return;
	(void)length;
	line = text;
	while (line && *line) {
		char *definition;

		next = strchr(line, '\n');
		if (next)
			*next++ = '\0';
		definition = strdup(line);
		if (!definition) {
			fail(path, "out of memory");
			break;
		}
		{
			struct make_target *defined_target = record_target_definition(
				targets, &target_count, definition, path);
			if (defined_target)
				current_target = defined_target;
			else if (*line && *line != '\t' && *line != ' ' &&
				 !strchr(line, '='))
				current_target = NULL;
		}

		if (strstr(line, "ORLIX_AARCHMRS_")) {
			bool declaration = !strncmp(line, "ORLIX_AARCHMRS_", 17) &&
				strstr(line, "?=") != NULL;

			if (!declaration && current_target != NULL &&
			    strcmp(current_target->name,
				   "tcti-isa-maintainer-source-check"))
				fail_target(path, current_target->name,
					    "Arm JSON environment escaped maintainer lane");
		}
		if (contains_raw_json_consumer(line) && current_target) {
			current_target->raw_json_consumer = true;
			if (strcmp(current_target->name,
				   "tcti-isa-maintainer-source-check"))
				fail_target(path, current_target->name,
					    "raw Arm JSON consumer escaped maintainer lane");
		} else if (contains_raw_json_consumer(line)) {
			fail_target(path, current_target ? current_target->name : NULL,
				    "raw Arm JSON consumer escaped maintainer lane");
		}
		if (current_target && !strcmp(current_target->name,
					     "tcti-isa-host-tests")) {
			const char *match;
			if (contains_forbidden(line, &match))
				fail(path, match);
		}
		line = next;
	}
	check_host_dependency_closure(find_target(targets, target_count,
						  "tcti-isa-host-tests"), targets,
				      target_count, path);
	for (target_index = 0; target_index < target_count; target_index++)
		targets[target_index].visited = false;
	check_host_dependency_closure(find_target(targets, target_count,
						  "tcti-isa-audit"), targets,
				      target_count, path);
	free(text);
}

static void check_generated_artifacts(const char *tcti_root)
{
	static const char *const artifacts[] = {
		"isa/inventory.def", "isa/source_manifest.def",
		"isa/target_classification.def",
		"isa/target_instruction_artifact_generated.h",
		"isa/target_feature_artifact.def",
		"isa/target_register_artifact.def",
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(artifacts); index++) {
		char *path = join_path(tcti_root, artifacts[index]);
		if (!path) {
			fail(tcti_root, "out of memory");
			return;
		}
		check_json_free_file(path);
		free(path);
	}
}

static void check_kunit_makefiles(const char *tcti_root)
{
	static const char *const makefiles[] = {
		"Makefile", "tests/Makefile", "../Makefile", "../../Makefile",
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(makefiles); index++) {
		char *path = join_path(tcti_root, makefiles[index]);

		if (!path)
			fail(tcti_root, "out of memory");
		else {
			check_json_free_file(path);
			free(path);
		}
	}
}

int main(int argc, char **argv)
{
	struct include_scan include_scan = { 0 };
	char *overlay_root;
	char *tcti_root;
	char *makefile;
	const char *variables[] = {
		"ORLIX_AARCHMRS_INSTRUCTIONS", "ORLIX_AARCHMRS_FEATURES",
		"ORLIX_AARCHMRS_REGISTERS",
	};
	size_t index;

	if (argc != 2) {
		fprintf(stderr, "usage: %s repository-root\n", argv[0]);
		return 2;
	}
	for (index = 0; index < ARRAY_SIZE(variables); index++) {
		if (getenv(variables[index]) != NULL)
			fail(variables[index], "ordinary host lane inherited Arm JSON input");
	}
	tcti_root = join_path(argv[1],
		"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti");
	overlay_root = join_path(argv[1],
		"OrlixKernel/Sources/ports/orlix/overlay");
	makefile = join_path(argv[1], "Makefile");
	if (!overlay_root || !tcti_root || !makefile) {
		fail(argv[1], "out of memory");
		free(makefile);
		free(tcti_root);
		free(overlay_root);
		return 1;
	}
	check_makefile_lanes(makefile);
	check_kunit_makefiles(tcti_root);
	check_generated_artifacts(tcti_root);
	scan_overlay_tree(overlay_root, &include_scan);
	for (index = 0; index < include_scan.visited_count; index++)
		free(include_scan.visited[index]);
	free(makefile);
	free(tcti_root);
	free(overlay_root);
	if (failures) {
		fprintf(stderr, "host-lane boundary test: %d failure(s)\n", failures);
		return 1;
	}
	puts("TCTI host-lane boundary test: PASS");
	return 0;
}
