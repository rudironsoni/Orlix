/* SPDX-License-Identifier: GPL-2.0-only */
#define _POSIX_C_SOURCE 200809L

#include "target_arm_xml_package.h"
#include "target_artifact_publisher.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef O_DIRECTORY
#define O_DIRECTORY 0
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif

#define ARRAY_SIZE(values) (sizeof(values) / sizeof((values)[0]))
#define MAX_XML_BYTES (8U * 1024U * 1024U)
#define MAX_ARCHIVE_BYTES (64U * 1024U * 1024U)
#define MAX_XML_DEPTH 64U
#define MAX_XML_NAME 128U
#define MAX_HELPERS 3000U
#define MAX_FORMS 600U

struct xml_name_set {
	char **values;
	size_t count;
	size_t capacity;
};

struct xml_scan {
	const char *xml;
	size_t length;
	size_t cursor;
	char stack[MAX_XML_DEPTH][MAX_XML_NAME];
	size_t depth;
	size_t ps_count;
	size_t pstext_count;
	size_t anchor_count;
	size_t iform_count;
	struct xml_name_set ps_names;
	struct xml_name_set function_links;
	struct xml_name_set iform_ids;
	struct xml_name_set iform_files;
	bool require_unique_ps_names;
	bool record_iforms;
};

static bool xml_name_start(unsigned char byte)
{
	return isalpha(byte) || byte == '_' || byte == ':';
}

static bool xml_name_byte(unsigned char byte)
{
	return xml_name_start(byte) || isdigit(byte) || byte == '-' || byte == '.';
}

static int copy_xml_name(const char *source, size_t length,
			 char destination[MAX_XML_NAME])
{
	if (!length || length >= MAX_XML_NAME)
		return -1;
	memcpy(destination, source, length);
	destination[length] = '\0';
	return 0;
}

static void name_set_destroy(struct xml_name_set *set)
{
	size_t index;

	for (index = 0; index < set->count; index++)
		free(set->values[index]);
	free(set->values);
	*set = (struct xml_name_set) { 0 };
}

static int name_set_add(struct xml_name_set *set, const char *value,
			 size_t length, size_t maximum)
{
	size_t index;
	char *copy;

	if (!length || set->count >= maximum)
		return -1;
	for (index = 0; index < set->count; index++) {
		if (strlen(set->values[index]) == length &&
		    !memcmp(set->values[index], value, length))
			return 1;
	}
	if (set->count == set->capacity) {
		size_t capacity = set->capacity ? set->capacity * 2U : 64U;
		char **values;

		if (capacity > maximum)
			capacity = maximum;
		values = realloc(set->values, capacity * sizeof(*values));
		if (!values)
			return -1;
		set->values = values;
		set->capacity = capacity;
	}
	copy = malloc(length + 1U);
	if (!copy)
		return -1;
	memcpy(copy, value, length);
	copy[length] = '\0';
	set->values[set->count++] = copy;
	return 0;
}

static bool name_set_contains(const struct xml_name_set *set,
			      const char *value, size_t length)
{
	size_t index;

	for (index = 0; index < set->count; index++) {
		if (strlen(set->values[index]) == length &&
		    !memcmp(set->values[index], value, length))
			return true;
	}
	return false;
}

static int compare_strings(const void *left, const void *right)
{
	const char *const *a = left;
	const char *const *b = right;

	return strcmp(*a, *b);
}

static bool valid_entity(const char *entity, size_t length)
{
	size_t index;

	if ((length == 2U && !memcmp(entity, "lt", 2U)) ||
	    (length == 2U && !memcmp(entity, "gt", 2U)) ||
	    (length == 3U && !memcmp(entity, "amp", 3U)) ||
	    (length == 4U && !memcmp(entity, "apos", 4U)) ||
	    (length == 4U && !memcmp(entity, "quot", 4U)))
		return true;
	if (length < 2U || entity[0] != '#')
		return false;
	index = 1U;
	if (index < length && (entity[index] == 'x' || entity[index] == 'X')) {
		if (++index == length)
			return false;
		for (; index < length; index++) {
			if (!isxdigit((unsigned char)entity[index]))
				return false;
		}
		return true;
	}
	for (; index < length; index++) {
		if (!isdigit((unsigned char)entity[index]))
			return false;
	}
	return true;
}

static int scan_entities(const char *text, size_t length)
{
	size_t index;

	for (index = 0; index < length; index++) {
		size_t end;

		if (text[index] != '&')
			continue;
		for (end = index + 1U; end < length && end - index <= 16U;
		     end++) {
			if (text[end] == ';')
				break;
			if (text[end] == '<' || text[end] == '&' ||
			    isspace((unsigned char)text[end]))
				return -1;
		}
		if (end >= length || text[end] != ';' ||
		    !valid_entity(text + index + 1U, end - index - 1U))
			return -1;
		index = end;
	}
	return 0;
}

static void skip_space(struct xml_scan *scan)
{
	while (scan->cursor < scan->length &&
	       isspace((unsigned char)scan->xml[scan->cursor]))
		scan->cursor++;
}

static int parse_name(struct xml_scan *scan, const char **name, size_t *length)
{
	size_t start = scan->cursor;

	if (start >= scan->length ||
	    !xml_name_start((unsigned char)scan->xml[start]))
		return -1;
	scan->cursor++;
	while (scan->cursor < scan->length &&
	       xml_name_byte((unsigned char)scan->xml[scan->cursor]))
		scan->cursor++;
	*name = scan->xml + start;
	*length = scan->cursor - start;
	return 0;
}

static int record_attribute(struct xml_scan *scan, const char *element,
			    const char *attribute, const char *value,
			    size_t value_length, bool *has_ps_name,
			    bool *has_function, bool *has_iform_id,
			    bool *has_iform_file)
{
	int status;

	if (!strcmp(element, "ps") && !strcmp(attribute, "name")) {
		status = name_set_add(&scan->ps_names, value, value_length,
				      MAX_HELPERS);
		if (status < 0 || (status > 0 && scan->require_unique_ps_names))
			return status > 0 ? 1 : -1;
		*has_ps_name = true;
	} else if (!strcmp(element, "anchor") &&
		   !strcmp(attribute, "link") && value_length) {
		/*
		 * Arm deliberately repeats a small number of overload anchors in
		 * distinct, uniquely named ps entries.  The ps locator is the helper
		 * identity; an anchor alone is not.
		 */
		status = name_set_add(&scan->function_links, value, value_length,
				      MAX_HELPERS);
		if (status < 0)
			return -1;
		*has_function = true;
	} else if (scan->record_iforms && !strcmp(element, "iform") &&
		   !strcmp(attribute, "id")) {
		status = name_set_add(&scan->iform_ids, value, value_length, MAX_FORMS);
		if (status)
			return status > 0 ? 1 : -1;
		*has_iform_id = true;
	} else if (scan->record_iforms && !strcmp(element, "iform") &&
		   !strcmp(attribute, "iformfile")) {
		status = name_set_add(&scan->iform_files, value, value_length,
				      MAX_FORMS);
		if (status)
			return status > 0 ? 1 : -1;
		*has_iform_file = true;
	}
	return 0;
}

static int parse_start_tag(struct xml_scan *scan)
{
	const char *name;
	size_t name_length;
	char element[MAX_XML_NAME];
	bool self_closing = false;
	bool has_ps_name = false, has_function = false;
	bool has_iform_id = false, has_iform_file = false;

	if (parse_name(scan, &name, &name_length) ||
	    copy_xml_name(name, name_length, element))
		return -1;
	for (;;) {
		const char *attribute, *value;
		size_t attribute_length, value_length;
		char attribute_name[MAX_XML_NAME];
		char quote;
		int status;

		skip_space(scan);
		if (scan->cursor >= scan->length)
			return -1;
		if (scan->xml[scan->cursor] == '>') {
			scan->cursor++;
			break;
		}
		if (scan->xml[scan->cursor] == '/' &&
		    scan->cursor + 1U < scan->length &&
		    scan->xml[scan->cursor + 1U] == '>') {
			scan->cursor += 2U;
			self_closing = true;
			break;
		}
		if (parse_name(scan, &attribute, &attribute_length) ||
		    copy_xml_name(attribute, attribute_length, attribute_name))
			return -1;
		skip_space(scan);
		if (scan->cursor >= scan->length || scan->xml[scan->cursor++] != '=')
			return -1;
		skip_space(scan);
		if (scan->cursor >= scan->length ||
		    (scan->xml[scan->cursor] != '\'' &&
		     scan->xml[scan->cursor] != '"'))
			return -1;
		quote = scan->xml[scan->cursor++];
		value = scan->xml + scan->cursor;
		while (scan->cursor < scan->length &&
		       scan->xml[scan->cursor] != quote) {
			if (scan->xml[scan->cursor] == '<')
				return -1;
			scan->cursor++;
		}
		if (scan->cursor >= scan->length)
			return -1;
		value_length = scan->xml + scan->cursor - value;
		if (scan_entities(value, value_length))
			return -1;
		scan->cursor++;
		status = record_attribute(scan, element, attribute_name, value,
					  value_length, &has_ps_name,
					  &has_function, &has_iform_id,
					  &has_iform_file);
		if (status)
			return status > 0 ? -2 : -1;
	}
	if (!strcmp(element, "ps")) {
		if (!has_ps_name)
			return -3;
		scan->ps_count++;
	} else if (!strcmp(element, "pstext")) {
		scan->pstext_count++;
	} else if (!strcmp(element, "anchor") && has_function) {
		scan->anchor_count++;
	} else if (scan->record_iforms && !strcmp(element, "iform")) {
		if (!has_iform_id || !has_iform_file)
			return -4;
		scan->iform_count++;
	}
	if (!self_closing) {
		if (scan->depth >= MAX_XML_DEPTH)
			return -1;
		memcpy(scan->stack[scan->depth++], element, strlen(element) + 1U);
	}
	return 0;
}

static int parse_end_tag(struct xml_scan *scan)
{
	const char *name;
	size_t name_length;

	if (parse_name(scan, &name, &name_length))
		return -1;
	skip_space(scan);
	if (scan->cursor >= scan->length || scan->xml[scan->cursor++] != '>' ||
	    !scan->depth || strlen(scan->stack[scan->depth - 1U]) != name_length ||
	    memcmp(scan->stack[scan->depth - 1U], name, name_length))
		return -1;
	scan->depth--;
	return 0;
}

static int skip_until(struct xml_scan *scan, const char *terminator)
{
	const char *end = strstr(scan->xml + scan->cursor, terminator);

	if (!end || end + strlen(terminator) > scan->xml + scan->length)
		return -1;
	scan->cursor = (size_t)(end - scan->xml) + strlen(terminator);
	return 0;
}

static int skip_external_doctype(struct xml_scan *scan)
{
	char quote = '\0';

	if (strncmp(scan->xml + scan->cursor, "!DOCTYPE", 8U))
		return -1;
	scan->cursor += 8U;
	while (scan->cursor < scan->length) {
		char byte = scan->xml[scan->cursor++];

		if (quote) {
			if (byte == quote)
				quote = '\0';
			continue;
		}
		if (byte == '\'' || byte == '"') {
			quote = byte;
			continue;
		}
		/* Internal subsets and custom entity declarations are forbidden. */
		if (byte == '[')
			return -1;
		if (byte == '>')
			return 0;
	}
	return -1;
}

static enum orlix_tcti_arm_xml_package_error scan_xml(struct xml_scan *scan)
{
	while (scan->cursor < scan->length) {
		size_t text_start = scan->cursor;
		int status;

		while (scan->cursor < scan->length && scan->xml[scan->cursor] != '<')
			scan->cursor++;
		if (scan_entities(scan->xml + text_start, scan->cursor - text_start))
			return ORLIX_TCTI_ARM_XML_PACKAGE_MALFORMED_XML;
		if (scan->cursor == scan->length)
			break;
		scan->cursor++;
		if (scan->cursor >= scan->length)
			return ORLIX_TCTI_ARM_XML_PACKAGE_MALFORMED_XML;
		if (!strncmp(scan->xml + scan->cursor, "!--", 3U)) {
			scan->cursor += 3U;
			if (skip_until(scan, "-->"))
				return ORLIX_TCTI_ARM_XML_PACKAGE_MALFORMED_XML;
			continue;
		}
		if (scan->xml[scan->cursor] == '?') {
			scan->cursor++;
			if (skip_until(scan, "?>"))
				return ORLIX_TCTI_ARM_XML_PACKAGE_MALFORMED_XML;
			continue;
		}
		if (scan->xml[scan->cursor] == '!') {
			if (skip_external_doctype(scan))
				return ORLIX_TCTI_ARM_XML_PACKAGE_MALFORMED_XML;
			continue;
		}
		if (scan->xml[scan->cursor] == '/') {
			scan->cursor++;
			status = parse_end_tag(scan);
		} else {
			status = parse_start_tag(scan);
		}
		if (status == -2)
			return ORLIX_TCTI_ARM_XML_PACKAGE_DUPLICATE_HELPER;
		if (status == -3)
			return ORLIX_TCTI_ARM_XML_PACKAGE_MISSING_HELPER;
		if (status == -4)
			return ORLIX_TCTI_ARM_XML_PACKAGE_INDEX;
		if (status)
			return ORLIX_TCTI_ARM_XML_PACKAGE_MALFORMED_XML;
	}
	return scan->depth ? ORLIX_TCTI_ARM_XML_PACKAGE_MALFORMED_XML :
		ORLIX_TCTI_ARM_XML_PACKAGE_OK;
}

enum orlix_tcti_arm_xml_package_error
orlix_tcti_arm_xml_document_validate(const char *xml, size_t length)
{
	struct xml_scan scan = {
		.xml = xml,
		.length = length,
		.require_unique_ps_names = true,
	};
	enum orlix_tcti_arm_xml_package_error error;

	if (!xml || !length)
		return ORLIX_TCTI_ARM_XML_PACKAGE_INVALID_ARGUMENT;
	error = scan_xml(&scan);
	name_set_destroy(&scan.ps_names);
	name_set_destroy(&scan.function_links);
	name_set_destroy(&scan.iform_ids);
	name_set_destroy(&scan.iform_files);
	return error;
}

static int read_regular_file_at(int directory_fd, const char *name,
				char **data, size_t *length)
{
	struct stat status;
	int fd;
	ssize_t count;

	fd = openat(directory_fd, name, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
	if (fd < 0 || fstat(fd, &status) || !S_ISREG(status.st_mode) ||
	    status.st_size <= 0 || (unsigned long long)status.st_size > MAX_XML_BYTES) {
		if (fd >= 0)
			close(fd);
		return -1;
	}
	*length = (size_t)status.st_size;
	*data = malloc(*length + 1U);
	if (!*data) {
		close(fd);
		return -1;
	}
	count = read(fd, *data, *length);
	if (count < 0 || (size_t)count != *length || close(fd)) {
		free(*data);
		*data = NULL;
		return -1;
	}
	(*data)[*length] = '\0';
	return 0;
}

static int file_sha256(const char *path, char digest[65])
{
	struct stat status;
	char *data = NULL;
	size_t length;
	FILE *file;
	long size;
	int result = -1;

	file = fopen(path, "rb");
	if (!file || fstat(fileno(file), &status) || !S_ISREG(status.st_mode) ||
	    fseek(file, 0, SEEK_END) || (size = ftell(file)) <= 0 ||
	    (unsigned long long)size > MAX_ARCHIVE_BYTES || fseek(file, 0, SEEK_SET))
		goto out;
	length = (size_t)size;
	data = malloc(length);
	if (!data || fread(data, 1, length, file) != length)
		goto out;
	orlix_tcti_target_artifact_sha256(data, length, digest);
	result = 0;
out:
	free(data);
	if (file)
		fclose(file);
	return result;
}

static const char *path_basename(const char *path)
{
	const char *end;
	const char *slash;

	if (!path || !*path)
		return NULL;
	end = path + strlen(path);
	while (end > path && end[-1] == '/')
		end--;
	if (end == path)
		return NULL;
	slash = end;
	while (slash > path && slash[-1] != '/')
		slash--;
	return strlen(slash) == (size_t)(end - slash) ? slash : NULL;
}

static int release_files(int release_fd, struct xml_name_set *files)
{
	DIR *directory;
	struct dirent *entry;
	struct stat status;

	directory = fdopendir(dup(release_fd));
	if (!directory)
		return -1;
	while ((entry = readdir(directory))) {
		if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
			continue;
		if (fstatat(release_fd, entry->d_name, &status, AT_SYMLINK_NOFOLLOW)) {
			closedir(directory);
			return -1;
		}
		if (S_ISDIR(status.st_mode)) {
			if (strcmp(entry->d_name, "xhtml") &&
			    strcmp(entry->d_name,
				   "diff-from-ISA_A64_xml_A_profile-2026-03")) {
				closedir(directory);
				return -1;
			}
			continue;
		}
		if (!S_ISREG(status.st_mode) ||
		    name_set_add(files, entry->d_name, strlen(entry->d_name),
				 ORLIX_TCTI_ARM_XML_RELEASE_FILE_COUNT)) {
			closedir(directory);
			return -1;
		}
	}
	closedir(directory);
	if (files->count != ORLIX_TCTI_ARM_XML_RELEASE_FILE_COUNT)
		return -1;
	qsort(files->values, files->count, sizeof(*files->values), compare_strings);
	return 0;
}

static const char *bounded_find(const char *start, const char *end,
				const char *needle)
{
	size_t length = strlen(needle);
	const char *cursor;

	if ((size_t)(end - start) < length)
		return NULL;
	for (cursor = start; cursor + length <= end; cursor++) {
		if (!memcmp(cursor, needle, length))
			return cursor;
	}
	return NULL;
}

static char *tag_attribute(const char *start, const char *end,
			   const char *attribute)
{
	char marker[MAX_XML_NAME + 3U];
	const char *value;
	const char *close;
	size_t marker_length;
	char *copy;

	if (snprintf(marker, sizeof(marker), "%s=\"", attribute) >=
			(int)sizeof(marker))
		return NULL;
	marker_length = strlen(marker);
	value = bounded_find(start, end, marker);
	if (!value)
		return NULL;
	value += marker_length;
	close = memchr(value, '"', (size_t)(end - value));
	if (!close || close == value || memchr(value, '&', (size_t)(close - value)))
		return NULL;
	copy = malloc((size_t)(close - value) + 1U);
	if (!copy)
		return NULL;
	memcpy(copy, value, (size_t)(close - value));
	copy[close - value] = '\0';
	return copy;
}

static void normalized_sha256(const char *data, size_t length, char digest[65])
{
	char *normalized = malloc(length ? length : 1U);
	size_t source = 0, destination = 0;

	if (!normalized) {
		digest[0] = '\0';
		return;
	}
	while (source < length) {
		if (data[source] == '\r') {
			normalized[destination++] = '\n';
			source += source + 1U < length && data[source + 1U] == '\n' ?
				2U : 1U;
		} else {
			normalized[destination++] = data[source++];
		}
	}
	orlix_tcti_target_artifact_sha256(normalized, destination, digest);
	free(normalized);
}

static struct orlix_tcti_arm_xml_semantic_entry *package_entry_mutable(
	struct orlix_tcti_arm_xml_package *package, const char *encoding)
{
	size_t index;

	for (index = 0; index < package->entry_count; index++) {
		if (!strcmp(package->entries[index].encoding_name, encoding))
			return &package->entries[index];
	}
	return NULL;
}

static int add_encoding(struct orlix_tcti_arm_xml_package *package,
			const char *encoding, const char *file)
{
	struct orlix_tcti_arm_xml_semantic_entry *entries;
	struct orlix_tcti_arm_xml_semantic_entry *entry;

	if (package_entry_mutable(package, encoding) ||
	    package->entry_count >= ORLIX_TCTI_ARM_XML_ENCODING_COUNT)
		return -1;
	entries = realloc(package->entries,
			  (package->entry_count + 1U) * sizeof(*entries));
	if (!entries)
		return -1;
	package->entries = entries;
	entry = &package->entries[package->entry_count++];
	*entry = (struct orlix_tcti_arm_xml_semantic_entry) { 0 };
	entry->encoding_name = strdup(encoding);
	entry->relative_file = strdup(file);
	if (!entry->encoding_name || !entry->relative_file)
		return -1;
	return 0;
}

static int helper_digest(const char *start, const char *end,
			 const struct xml_name_set *shared_links,
			 size_t *count, char digest[65])
{
	struct xml_name_set helpers = { 0 };
	const char *cursor = start;
	char *manifest = NULL;
	size_t manifest_length = 0;
	size_t index;
	int result = -1;

	while ((cursor = bounded_find(cursor, end, "<a "))) {
		const char *tag_end = memchr(cursor, '>', (size_t)(end - cursor));
		char *file;
		char *link;
		int status;

		if (!tag_end)
			goto out;
		file = tag_attribute(cursor, tag_end, "file");
		link = tag_attribute(cursor, tag_end, "link");
		if (file && !strcmp(file, "shared_pseudocode.xml") && !link) {
			free(file);
			goto out;
		}
		if (file && link && !strcmp(file, "shared_pseudocode.xml")) {
			if (!name_set_contains(shared_links, link, strlen(link))) {
				free(file);
				free(link);
				goto out;
			}
			status = name_set_add(&helpers, link, strlen(link), MAX_HELPERS);
			if (status < 0) {
				free(file);
				free(link);
				goto out;
			}
		}
		free(file);
		free(link);
		cursor = tag_end + 1U;
	}
	qsort(helpers.values, helpers.count, sizeof(*helpers.values), compare_strings);
	for (index = 0; index < helpers.count; index++)
		manifest_length += strlen(helpers.values[index]) + 1U;
	manifest = malloc(manifest_length ? manifest_length : 1U);
	if (!manifest)
		goto out;
	manifest_length = 0;
	for (index = 0; index < helpers.count; index++) {
		size_t length = strlen(helpers.values[index]);

		memcpy(manifest + manifest_length, helpers.values[index], length);
		manifest_length += length;
		manifest[manifest_length++] = '\n';
	}
	orlix_tcti_target_artifact_sha256(manifest, manifest_length, digest);
	*count = helpers.count;
	result = 0;
out:
	free(manifest);
	name_set_destroy(&helpers);
	return result;
}

static int set_semantic_section(
	struct orlix_tcti_arm_xml_semantic_section *section,
	const char *file, const char *ps_name, const char *kind,
	const char *start, const char *end,
	const struct xml_name_set *shared_links)
{
	char *locator;
	char body_digest[65];
	char helpers_digest[65];
	size_t helper_count;
	size_t locator_length;

	locator_length = strlen(file) + strlen(ps_name) + strlen(kind) + 6U;
	locator = malloc(locator_length);
	if (!locator ||
	    snprintf(locator, locator_length, "%s#%s/%s", file, ps_name,
		     kind) >= (int)locator_length)
		goto error;
	normalized_sha256(start, (size_t)(end - start), body_digest);
	if (!body_digest[0] ||
	    helper_digest(start, end, shared_links, &helper_count, helpers_digest))
		goto error;
	if (!section->locator) {
		section->locator = locator;
		section->section_count = 1U;
		memcpy(section->normalized_sha256, body_digest, sizeof(body_digest));
		section->shared_helper_count = helper_count;
		memcpy(section->shared_helpers_sha256, helpers_digest,
		       sizeof(helpers_digest));
		return 0;
	} else {
		/*
		 * A class decode and file-wide postdecode can both apply to one
		 * encoding.  Bind their ordered locators and normalized body digests
		 * into one deterministic semantic-section manifest digest.
		 */
		size_t old_locator_length = strlen(section->locator);
		size_t combined_length = old_locator_length + 1U + strlen(locator) + 1U;
		char *combined = malloc(combined_length);
		char body_manifest[2U * (65U + 1U) + 512U];
		char helper_manifest[2U * 66U];
		int body_length;

		if (!combined)
			goto error;
		if (snprintf(combined, combined_length, "%s;%s", section->locator,
			     locator) >= (int)combined_length) {
			free(combined);
			goto error;
		}
		body_length = snprintf(body_manifest, sizeof(body_manifest),
			"%s\n%s\n%s\n%s\n", section->locator,
			section->normalized_sha256, locator, body_digest);
		if (body_length < 0 || body_length >= (int)sizeof(body_manifest)) {
			free(combined);
			goto error;
		}
		orlix_tcti_target_artifact_sha256(body_manifest, (size_t)body_length,
						  section->normalized_sha256);
		body_length = snprintf(helper_manifest, sizeof(helper_manifest),
			"%s\n%s\n", section->shared_helpers_sha256, helpers_digest);
		if (body_length < 0 || body_length >= (int)sizeof(helper_manifest)) {
			free(combined);
			goto error;
		}
		orlix_tcti_target_artifact_sha256(helper_manifest,
						  (size_t)body_length,
						  section->shared_helpers_sha256);
		free(section->locator);
		section->locator = combined;
		section->section_count++;
		section->shared_helper_count += helper_count;
		free(locator);
		return 0;
	}

	error:
	free(locator);
	return -1;
}

static int parse_instruction_file(
	const char *xml, size_t length, const char *file,
	const struct xml_name_set *shared_links,
	struct orlix_tcti_arm_xml_package *package, size_t *ps_count)
{
	struct encoding_position {
		size_t entry_index;
		const char *tag;
	};
	struct encoding_position *encodings = NULL;
	size_t encoding_count = 0;
	const char *end = xml + length;
	const char *cursor = xml;
	int result = -1;

	while ((cursor = bounded_find(cursor, end, "<encoding "))) {
		const char *tag_end = memchr(cursor, '>', (size_t)(end - cursor));
		struct encoding_position *grown;
		char *name;

		if (!tag_end || !(name = tag_attribute(cursor, tag_end, "name")))
			goto out;
		if (add_encoding(package, name, file)) {
			free(name);
			goto out;
		}
		grown = realloc(encodings,
				(encoding_count + 1U) * sizeof(*encodings));
		if (!grown) {
			free(name);
			goto out;
		}
		encodings = grown;
		encodings[encoding_count].entry_index = package->entry_count - 1U;
		encodings[encoding_count++].tag = cursor;
		free(name);
		cursor = tag_end + 1U;
	}
	if (!encoding_count) {
		result = 0;
		goto out;
	}
	cursor = xml;
	while ((cursor = bounded_find(cursor, end, "<ps_section "))) {
		const char *section_tag_end = memchr(cursor, '>', (size_t)(end - cursor));
		const char *section_end;
		const char *ps;
		const char *ps_tag_end;
		const char *ps_end;
		const char *pstext;
		const char *pstext_tag_end;
		const char *last_iclass = NULL;
		const char *iclass_cursor = xml;
		const char *scope_start = xml;
		const char *scope_end = end;
		char *kind;
		char *ps_name;
		bool decode_kind;
		size_t index;
		size_t assigned = 0;

		if (!section_tag_end ||
		    !(section_end = bounded_find(section_tag_end, end, "</ps_section>")))
			goto out;
		ps = bounded_find(section_tag_end, section_end, "<ps ");
		if (!ps || !(ps_tag_end = memchr(ps, '>',
							(size_t)(section_end - ps))) ||
		    !(ps_end = bounded_find(ps_tag_end, section_end, "</ps>")) ||
		    !(ps_name = tag_attribute(ps, ps_tag_end, "name")) ||
		    !(pstext = bounded_find(ps_tag_end, ps_end, "<pstext")) ||
		    !(pstext_tag_end = memchr(pstext, '>', (size_t)(ps_end - pstext))) ||
		    !(kind = tag_attribute(pstext, pstext_tag_end, "rep_section")))
			goto out;
		(*ps_count)++;
		decode_kind = !strcmp(kind, "decode") || !strcmp(kind, "postdecode");
		while ((iclass_cursor = bounded_find(iclass_cursor, cursor, "<iclass "))) {
			last_iclass = iclass_cursor;
			iclass_cursor += strlen("<iclass ");
		}
		if (!strcmp(kind, "decode") && last_iclass) {
			const char *close = bounded_find(last_iclass, end, "</iclass>");

			if (close && cursor < close) {
				scope_start = last_iclass;
				scope_end = close;
			}
		} else if (!decode_kind && strcmp(kind, "execute")) {
			free(kind);
			free(ps_name);
			goto out;
		}
		for (index = 0; index < encoding_count; index++) {
			struct orlix_tcti_arm_xml_semantic_section *semantic;

			if (encodings[index].tag < scope_start ||
			    encodings[index].tag >= scope_end)
				continue;
			semantic = decode_kind ?
				&package->entries[encodings[index].entry_index].decode :
				&package->entries[encodings[index].entry_index].operation;
			if (set_semantic_section(semantic, file, ps_name, kind, ps,
						 ps_end + strlen("</ps>"), shared_links)) {
				free(kind);
				free(ps_name);
				goto out;
			}
			assigned++;
		}
		if (!assigned) {
			free(kind);
			free(ps_name);
			goto out;
		}
		free(kind);
		free(ps_name);
		cursor = section_end + strlen("</ps_section>");
	}
	result = 0;
out:
	free(encodings);
	return result;
}

static enum orlix_tcti_arm_xml_package_error validate_scanned_documents(
	const char *shared, size_t shared_length, const char *notice,
	size_t notice_length, const char *index, size_t index_length,
	struct orlix_tcti_arm_xml_package *package)
{
	struct xml_scan shared_scan = {
		.xml = shared,
		.length = shared_length,
		.require_unique_ps_names = true,
	};
	struct xml_scan index_scan = {
		.xml = index,
		.length = index_length,
		.record_iforms = true,
	};
	enum orlix_tcti_arm_xml_package_error error;

	error = scan_xml(&shared_scan);
	if (error)
		goto out;
	error = orlix_tcti_arm_xml_document_validate(notice, notice_length);
	if (error)
		goto out;
	error = scan_xml(&index_scan);
	if (error)
		goto out;
	package->shared_ps_count = shared_scan.ps_count;
	package->shared_anchor_count = shared_scan.anchor_count;
	package->index_form_count = index_scan.iform_count;
	if (shared_scan.ps_count != ORLIX_TCTI_ARM_XML_SHARED_PS_COUNT ||
	    shared_scan.pstext_count != ORLIX_TCTI_ARM_XML_SHARED_PS_COUNT ||
	    shared_scan.anchor_count != ORLIX_TCTI_ARM_XML_SHARED_ANCHOR_COUNT) {
		error = ORLIX_TCTI_ARM_XML_PACKAGE_MISSING_HELPER;
		goto out;
	}
	if (index_scan.iform_count != ORLIX_TCTI_ARM_XML_INDEX_FORM_COUNT) {
		error = ORLIX_TCTI_ARM_XML_PACKAGE_INDEX;
		goto out;
	}
out:
	name_set_destroy(&shared_scan.ps_names);
	name_set_destroy(&shared_scan.function_links);
	name_set_destroy(&shared_scan.iform_ids);
	name_set_destroy(&shared_scan.iform_files);
	name_set_destroy(&index_scan.ps_names);
	name_set_destroy(&index_scan.function_links);
	name_set_destroy(&index_scan.iform_ids);
	name_set_destroy(&index_scan.iform_files);
	return error;
}

static bool has_suffix(const char *text, const char *suffix)
{
	size_t text_length = strlen(text);
	size_t suffix_length = strlen(suffix);

	return text_length >= suffix_length &&
		!strcmp(text + text_length - suffix_length, suffix);
}

static enum orlix_tcti_arm_xml_package_error validate_release_corpus(
	int release_fd, const struct xml_name_set *files,
	const struct xml_name_set *shared_links,
	struct orlix_tcti_arm_xml_package *package)
{
	char *manifest = NULL;
	size_t manifest_length = 0;
	size_t manifest_capacity = 0;
	size_t xml_count = 0;
	size_t ps_count = 0;
	size_t index;
	char release_digest[65];
	enum orlix_tcti_arm_xml_package_error error =
		ORLIX_TCTI_ARM_XML_PACKAGE_IO;

	for (index = 0; index < files->count; index++) {
		const char *name = files->values[index];
		char *data = NULL;
		size_t length = 0;
		char digest[65];
		size_t required;

		if (read_regular_file_at(release_fd, name, &data, &length))
			goto out;
		orlix_tcti_target_artifact_sha256(data, length, digest);
		required = manifest_length + strlen(name) + 1U + 64U + 1U;
		if (required > manifest_capacity) {
			size_t capacity = manifest_capacity ? manifest_capacity * 2U :
				256U * 1024U;
			char *grown;

			while (capacity < required)
				capacity *= 2U;
			grown = realloc(manifest, capacity);
			if (!grown) {
				free(data);
				goto out;
			}
			manifest = grown;
			manifest_capacity = capacity;
		}
		memcpy(manifest + manifest_length, name, strlen(name));
		manifest_length += strlen(name);
		manifest[manifest_length++] = '\0';
		memcpy(manifest + manifest_length, digest, 64U);
		manifest_length += 64U;
		manifest[manifest_length++] = '\n';
		if (has_suffix(name, ".xml")) {
			struct xml_scan instruction_scan = {
				.xml = data,
				.length = length,
			};
			enum orlix_tcti_arm_xml_package_error scan_error;

			xml_count++;
			scan_error = scan_xml(&instruction_scan);
			name_set_destroy(&instruction_scan.ps_names);
			name_set_destroy(&instruction_scan.function_links);
			name_set_destroy(&instruction_scan.iform_ids);
			name_set_destroy(&instruction_scan.iform_files);
			if (scan_error) {
				free(data);
				error = scan_error;
				goto out;
			}
			if (strcmp(name, "shared_pseudocode.xml") &&
			    strcmp(name, "notice.xml") && strcmp(name, "index.xml") &&
			    parse_instruction_file(data, length, name, shared_links,
						   package, &ps_count)) {
				free(data);
				error = ORLIX_TCTI_ARM_XML_PACKAGE_MALFORMED_XML;
				goto out;
			}
		}
		free(data);
	}
	orlix_tcti_target_artifact_sha256(manifest, manifest_length, release_digest);
	if (strcmp(release_digest, ORLIX_TCTI_ARM_XML_RELEASE_DIGEST)) {
		error = ORLIX_TCTI_ARM_XML_PACKAGE_IDENTITY;
		goto out;
	}
	if (xml_count != ORLIX_TCTI_ARM_XML_INSTRUCTION_FILE_COUNT ||
	    package->entry_count != ORLIX_TCTI_ARM_XML_ENCODING_COUNT ||
	    ps_count != ORLIX_TCTI_ARM_XML_INSTRUCTION_PS_COUNT) {
		error = ORLIX_TCTI_ARM_XML_PACKAGE_INDEX;
		goto out;
	}
	error = ORLIX_TCTI_ARM_XML_PACKAGE_OK;
out:
	free(manifest);
	return error;
}

enum orlix_tcti_arm_xml_package_error
orlix_tcti_arm_xml_package_validate(
	const char *archive_path, const char *release_path,
	struct orlix_tcti_arm_xml_package *package)
{
	char archive_digest[65], shared_digest[65], notice_digest[65], index_digest[65];
	char *shared = NULL, *notice = NULL, *index = NULL;
	size_t shared_length = 0, notice_length = 0, index_length = 0;
	const char *basename;
	int release_fd = -1;
	struct xml_name_set files = { 0 };
	struct xml_scan shared_scan = { 0 };
	enum orlix_tcti_arm_xml_package_error error = ORLIX_TCTI_ARM_XML_PACKAGE_IO;

	if (!archive_path || !release_path || !package)
		return ORLIX_TCTI_ARM_XML_PACKAGE_INVALID_ARGUMENT;
	*package = (struct orlix_tcti_arm_xml_package) { 0 };
	basename = path_basename(release_path);
	if (!basename || strcmp(basename, ORLIX_TCTI_ARM_XML_RELEASE_NAME))
		return ORLIX_TCTI_ARM_XML_PACKAGE_RELEASE_PATH;
	basename = path_basename(archive_path);
	if (!basename || strcmp(basename, ORLIX_TCTI_ARM_XML_ARCHIVE_NAME))
		return ORLIX_TCTI_ARM_XML_PACKAGE_RELEASE_PATH;
	if (file_sha256(archive_path, archive_digest) ||
	    strcmp(archive_digest, ORLIX_TCTI_ARM_XML_ARCHIVE_SHA256))
		return ORLIX_TCTI_ARM_XML_PACKAGE_IDENTITY;
	release_fd = open(release_path, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	if (release_fd < 0)
		return ORLIX_TCTI_ARM_XML_PACKAGE_IO;
	if (release_files(release_fd, &files)) {
		error = ORLIX_TCTI_ARM_XML_PACKAGE_FILE_SET;
		goto out;
	}
	if (read_regular_file_at(release_fd, "shared_pseudocode.xml", &shared,
				 &shared_length) ||
	    read_regular_file_at(release_fd, "notice.xml", &notice, &notice_length) ||
	    read_regular_file_at(release_fd, "index.xml", &index, &index_length))
		goto out;
	orlix_tcti_target_artifact_sha256(shared, shared_length, shared_digest);
	orlix_tcti_target_artifact_sha256(notice, notice_length, notice_digest);
	orlix_tcti_target_artifact_sha256(index, index_length, index_digest);
	if (strcmp(shared_digest, ORLIX_TCTI_ARM_XML_SHARED_SHA256) ||
	    strcmp(notice_digest, ORLIX_TCTI_ARM_XML_NOTICE_SHA256) ||
	    strcmp(index_digest, ORLIX_TCTI_ARM_XML_INDEX_SHA256)) {
		error = ORLIX_TCTI_ARM_XML_PACKAGE_IDENTITY;
		goto out;
	}
	error = validate_scanned_documents(shared, shared_length, notice, notice_length,
					   index, index_length, package);
	if (error)
		goto out;
	shared_scan = (struct xml_scan) {
		.xml = shared,
		.length = shared_length,
		.require_unique_ps_names = true,
	};
	error = scan_xml(&shared_scan);
	if (error)
		goto out;
	error = validate_release_corpus(release_fd, &files,
					&shared_scan.function_links, package);
out:
	free(shared);
	free(notice);
	free(index);
	if (release_fd >= 0)
		close(release_fd);
	name_set_destroy(&files);
	name_set_destroy(&shared_scan.ps_names);
	name_set_destroy(&shared_scan.function_links);
	name_set_destroy(&shared_scan.iform_ids);
	name_set_destroy(&shared_scan.iform_files);
	if (error)
		orlix_tcti_arm_xml_package_destroy(package);
	return error;
}

void orlix_tcti_arm_xml_package_destroy(
	struct orlix_tcti_arm_xml_package *package)
{
	size_t index;

	if (!package)
		return;
	for (index = 0; index < package->entry_count; index++) {
		free(package->entries[index].encoding_name);
		free(package->entries[index].relative_file);
		free(package->entries[index].decode.locator);
		free(package->entries[index].operation.locator);
	}
	free(package->entries);
	*package = (struct orlix_tcti_arm_xml_package) { 0 };
}

const struct orlix_tcti_arm_xml_semantic_entry *
orlix_tcti_arm_xml_package_entry(
	const struct orlix_tcti_arm_xml_package *package, const char *encoding_name)
{
	size_t index;

	if (!package || !encoding_name)
		return NULL;
	for (index = 0; index < package->entry_count; index++) {
		if (!strcmp(package->entries[index].encoding_name, encoding_name))
			return &package->entries[index];
	}
	return NULL;
}

const char *orlix_tcti_arm_xml_package_error_name(
	enum orlix_tcti_arm_xml_package_error error)
{
	switch (error) {
	case ORLIX_TCTI_ARM_XML_PACKAGE_OK: return "success";
	case ORLIX_TCTI_ARM_XML_PACKAGE_INVALID_ARGUMENT: return "invalid argument";
	case ORLIX_TCTI_ARM_XML_PACKAGE_RELEASE_PATH: return "release path mismatch";
	case ORLIX_TCTI_ARM_XML_PACKAGE_IO: return "source I/O failure";
	case ORLIX_TCTI_ARM_XML_PACKAGE_FILE_SET: return "unexpected release file set";
	case ORLIX_TCTI_ARM_XML_PACKAGE_IDENTITY: return "source identity mismatch";
	case ORLIX_TCTI_ARM_XML_PACKAGE_MALFORMED_XML: return "malformed XML or entity";
	case ORLIX_TCTI_ARM_XML_PACKAGE_DUPLICATE_HELPER: return "duplicate helper";
	case ORLIX_TCTI_ARM_XML_PACKAGE_MISSING_HELPER: return "missing helper";
	case ORLIX_TCTI_ARM_XML_PACKAGE_INDEX: return "invalid instruction index";
	}
	return "unknown error";
}
