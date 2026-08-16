/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_active_execution_profile_artifact_generator.h"
#include "target_artifact_publisher.h"
#include "../../tests/target_condition_format.h"

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* These are the immutable denominators of the pinned #120 source contract. */
#define ORLIX_TCTI_A64_TARGET_LEAF_COUNT 4350U
#define ORLIX_TCTI_A64_TARGET_MEMBERSHIP_COUNT 5592U
#define ORLIX_TCTI_ACTIVE_EXECUTION_MAX_ROWS 7000U
#define ORLIX_TCTI_SOURCE_CONDITION_MAX_HEX 8192U
#define ORLIX_TCTI_A64_APPLICABILITY_PARAMETER_COUNT 409U
#define ORLIX_TCTI_A64_APPLICABILITY_COMMON_VALUE_COUNT 377U
#define ORLIX_TCTI_A64_APPLICABILITY_OPERAND_VALUE_COUNT 364U
#define ORLIX_TCTI_A64_APPLICABILITY_COMMON_VALUE_BYTES 6032U
#define ORLIX_TCTI_A64_APPLICABILITY_ARCHITECTURE "vFAPA2-A"
#define ORLIX_TCTI_A64_APPLICABILITY_RELEASE "2026-06_rel"
#define ORLIX_TCTI_A64_APPLICABILITY_INSTRUCTIONS_SHA256 \
	"a1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe"
#define ORLIX_TCTI_A64_APPLICABILITY_FEATURES_SHA256 \
	"633259000ffd3da32900bd0c0c1beae4a9eea7095c278f74d62a00c846b41187"
#define ORLIX_TCTI_A64_APPLICABILITY_REGISTERS_SHA256 \
	"5bd76c3c3ce90322eb4fd179675dafe82df2fd1cb789beee516e5b29c471b874"
#define ORLIX_TCTI_A64_APPLICABILITY_RECONCILIATION_IDENTITY \
	"ceb8f8c561a5ecdce1a32bda12c7f33fc1b0433bbf035301f2590b6000ccbfe4"

enum profile_state {
	DISABLED,
	ENABLED,
};

struct profile_row {
	char feature[64];
	enum profile_state state;
	char identity[128];
};

struct promotion_row {
	char feature[64];
	char identity[128];
	char classification[128];
	char proof[128];
	unsigned int ordinal;
};

struct cohort_leaf_row {
	unsigned int index;
	unsigned int first_membership;
	unsigned int membership_count;
};

struct cohort_row {
	unsigned int index;
	unsigned int leaf;
	unsigned int parameter;
	char feature[64];
	unsigned int source_offset;
	unsigned int source_length;
};

struct source_proof_row {
	char proof[128];
	unsigned int ordinal;
};

struct classification_row {
	char name[128];
	char state[32];
	char proof[128];
};

struct registry_row {
	char proof[128];
	unsigned int ordinal;
};

struct source_manifest_row {
	char name[128];
	size_t condition_length;
	char condition_sha256[65];
};

struct applicability_row {
	unsigned int ordinal;
	char name[128];
	unsigned int condition_index;
	unsigned int condition_length;
	unsigned int status;
	size_t condition_offset;
	size_t condition_text_length;
	char condition_sha256[65];
};

struct applicability_summary {
	size_t row_count;
	int source_header;
	int counts_header;
};

struct source_manifest_summary {
	size_t row_count;
	size_t source_byte_length;
};

struct cohort_summary {
	unsigned int leaf_count;
	unsigned int membership_count;
	unsigned int resolved_count;
	unsigned int unresolved_count;
	size_t leaf_rows;
	size_t membership_rows;
};

static const char *next_line(const char *cursor, const char *end,
	const char **line, size_t *length)
{
	const char *newline;

	if (cursor >= end)
		return NULL;
	newline = memchr(cursor, '\n', (size_t)(end - cursor));
	*line = cursor;
	*length = newline ? (size_t)(newline - cursor) :
		(size_t)(end - cursor);
	return newline ? newline + 1U : end;
}

static int row_prefix(const char *line, size_t length, const char *prefix,
	const char **body)
{
	size_t prefix_length = strlen(prefix);
	size_t offset = 0;

	while (offset < length && isspace((unsigned char)line[offset]))
		offset++;
	if (length - offset < prefix_length ||
		memcmp(line + offset, prefix, prefix_length))
		return 0;
	*body = line + offset;
	return 1;
}

static int complete_row(const char *body, size_t line_length, int consumed)
{
	const char *tail;
	const char *end;

	if (consumed < 0 || (size_t)consumed > line_length)
		return 0;
	tail = body + consumed;
	end = body + line_length;
	if (tail >= end || *tail++ != ')')
		return 0;
	while (tail < end && isspace((unsigned char)*tail))
		tail++;
	return tail == end;
}

static int parse_unsigned_u(const char **cursor, const char *end,
	unsigned int *value)
{
	unsigned long parsed = 0;

	while (*cursor < end && isspace((unsigned char)**cursor))
		(*cursor)++;
	if (*cursor >= end || !isdigit((unsigned char)**cursor))
		return -1;
	while (*cursor < end && isdigit((unsigned char)**cursor)) {
		unsigned int digit = (unsigned int)(**cursor - '0');

		if (parsed > (UINT_MAX - digit) / 10U)
			return -1;
		parsed = parsed * 10U + digit;
		(*cursor)++;
	}
	if (*cursor >= end || *(*cursor)++ != 'U')
		return -1;
	*value = (unsigned int)parsed;
	return 0;
}

static int parse_quoted(const char **cursor, const char *end,
	const char **value, size_t *length)
{
	const char *start;
	const char *quote;

	while (*cursor < end && isspace((unsigned char)**cursor))
		(*cursor)++;
	if (*cursor >= end || *(*cursor)++ != '"')
		return -1;
	start = *cursor;
	quote = memchr(start, '"', (size_t)(end - start));
	if (!quote)
		return -1;
	*value = start;
	*length = (size_t)(quote - start);
	*cursor = quote + 1U;
	return 0;
}

static int parse_comma(const char **cursor, const char *end)
{
	while (*cursor < end && isspace((unsigned char)**cursor))
		(*cursor)++;
	if (*cursor >= end || *(*cursor)++ != ',')
		return -1;
	return 0;
}

static int duplicate_profile(const struct profile_row *rows, size_t count,
	const char *feature)
{
	size_t index;

	for (index = 0; index < count; index++)
		if (!strcmp(rows[index].feature, feature))
			return 1;
	return 0;
}

static int parse_profile_rows(const char *text, size_t length,
	struct profile_row *rows, size_t *count)
{
	const char *cursor = text;
	const char *end = text + length;
	const char *line;
	const char *body;
	size_t line_length;

	*count = 0;
	while ((cursor = next_line(cursor, end, &line, &line_length))) {
		char state[16];
		int consumed;

		if (!row_prefix(line, line_length,
			"ORLIX_TCTI_A64_ACTIVE_EXECUTION_FEATURE(", &body))
			continue;
		if (*count == ORLIX_TCTI_ACTIVE_EXECUTION_MAX_ROWS ||
			sscanf(body,
				"ORLIX_TCTI_A64_ACTIVE_EXECUTION_FEATURE(%63[A-Za-z0-9_], %15[A-Z], \"%127[^\"]\"%n",
				rows[*count].feature, state, rows[*count].identity,
				&consumed) != 3 ||
			!rows[*count].identity[0] ||
			!complete_row(body,
				line_length - (size_t)(body - line), consumed) ||
			duplicate_profile(rows, *count, rows[*count].feature))
			return -1;
		if (!strcmp(state, "DISABLED"))
			rows[*count].state = DISABLED;
		else if (!strcmp(state, "ENABLED"))
			rows[*count].state = ENABLED;
		else
			return -1;
		(*count)++;
	}
	return *count ? 0 : -1;
}

static int parse_promotion_rows(const char *text, size_t length,
	struct promotion_row *rows, size_t *count)
{
	const char *cursor = text;
	const char *end = text + length;
	const char *line;
	const char *body;
	size_t line_length;

	*count = 0;
	while ((cursor = next_line(cursor, end, &line, &line_length))) {
		size_t index;
		int consumed;

		if (!row_prefix(line, line_length,
			"ORLIX_TCTI_A64_ACTIVE_EXECUTION_PROMOTION(", &body))
			continue;
		if (*count == ORLIX_TCTI_ACTIVE_EXECUTION_MAX_ROWS ||
			sscanf(body,
				"ORLIX_TCTI_A64_ACTIVE_EXECUTION_PROMOTION(%63[A-Za-z0-9_], \"%127[^\"]\", %uU, \"%127[^\"]\", \"%127[^\"]\"%n",
				rows[*count].feature, rows[*count].identity,
				&rows[*count].ordinal, rows[*count].classification,
				rows[*count].proof, &consumed) != 5 ||
			!rows[*count].identity[0] ||
			!rows[*count].classification[0] ||
			!rows[*count].proof[0] ||
			!complete_row(body,
				line_length - (size_t)(body - line), consumed))
			return -1;
		for (index = 0; index < *count; index++)
			if ((!strcmp(rows[index].feature, rows[*count].feature) &&
				rows[index].ordinal == rows[*count].ordinal) ||
				(!strcmp(rows[index].feature, rows[*count].feature) &&
				 !strcmp(rows[index].identity, rows[*count].identity) &&
				 rows[index].ordinal != rows[*count].ordinal))
				return -1;
		(*count)++;
	}
	return 0;
}

static int parse_source_proof_rows(const char *text, size_t length,
	struct source_proof_row *rows, size_t *count)
{
	const char *cursor = text;
	const char *end = text + length;
	const char *line;
	const char *body;
	size_t line_length;

	*count = 0;
	while ((cursor = next_line(cursor, end, &line, &line_length))) {
		size_t index;
		int consumed;

		if (!row_prefix(line, line_length,
			"ORLIX_TCTI_A64_SOURCE_BOUND_PROOF(", &body))
			continue;
		if (*count == ORLIX_TCTI_ACTIVE_EXECUTION_MAX_ROWS ||
			sscanf(body,
				"ORLIX_TCTI_A64_SOURCE_BOUND_PROOF(%uU, \"%127[^\"]\"%n",
				&rows[*count].ordinal, rows[*count].proof, &consumed) != 2 ||
			!rows[*count].proof[0] ||
			!complete_row(body,
				line_length - (size_t)(body - line), consumed))
			return -1;
		for (index = 0; index < *count; index++)
			if (rows[index].ordinal == rows[*count].ordinal)
				return -1;
		(*count)++;
	}
	return *count ? 0 : -1;
}

static int parse_registry_rows(const char *text, size_t length,
	const struct source_proof_row *source, size_t source_count,
	struct registry_row *rows, size_t *count)
{
	static const char *const required_lines[] = {
		"#ifndef ORLIX_TCTI_A64_PROOF_REGISTRY_BINDING",
		"#define ORLIX_TCTI_A64_PROOF_REGISTRY_PROJECTION_SOURCE \"source_bound_proof.def\"",
		"#define ORLIX_TCTI_A64_PROOF_REGISTRY_PROJECTION_SOURCE_DEFAULT 1",
		"#define ORLIX_TCTI_A64_SOURCE_BOUND_PROOF(ordinal, proof_id) \\",
		"ORLIX_TCTI_A64_PROOF_REGISTRY_BINDING(ordinal, proof_id)",
		"#include ORLIX_TCTI_A64_PROOF_REGISTRY_PROJECTION_SOURCE",
		"#undef ORLIX_TCTI_A64_SOURCE_BOUND_PROOF",
		"#undef ORLIX_TCTI_A64_PROOF_REGISTRY_PROJECTION_SOURCE_DEFAULT",
		"#undef ORLIX_TCTI_A64_PROOF_REGISTRY_PROJECTION_SOURCE",
	};
	const char *cursor = text;
	const char *end = text + length;
	const char *line;
	size_t line_length;
	size_t required_index;
	size_t required_seen[sizeof(required_lines) / sizeof(required_lines[0])] = { 0 };
	size_t binding_rows = 0;

	*count = 0;
	while ((cursor = next_line(cursor, end, &line, &line_length))) {
		const char *body = line;
		size_t body_length = line_length;

		while (body_length && isspace((unsigned char)*body)) {
			body++;
			body_length--;
		}
		while (body_length && isspace((unsigned char)body[body_length - 1U]))
			body_length--;
		for (required_index = 0;
			required_index < sizeof(required_lines) / sizeof(required_lines[0]);
			required_index++)
			if (body_length == strlen(required_lines[required_index]) &&
				!memcmp(body, required_lines[required_index], body_length))
				break;
		if (required_index == sizeof(required_lines) / sizeof(required_lines[0]) &&
			body_length && !memcmp(body,
				"ORLIX_TCTI_A64_PROOF_REGISTRY_BINDING(",
				strlen("ORLIX_TCTI_A64_PROOF_REGISTRY_BINDING(")))
			return -1;
		if (required_index < sizeof(required_lines) / sizeof(required_lines[0]))
			required_seen[required_index]++;
		if (required_index < sizeof(required_lines) / sizeof(required_lines[0]) &&
			!strcmp(required_lines[required_index],
				"ORLIX_TCTI_A64_PROOF_REGISTRY_BINDING(ordinal, proof_id)"))
			binding_rows++;
	}
	for (required_index = 0;
		required_index < sizeof(required_lines) / sizeof(required_lines[0]);
		required_index++)
		if ((required_index == 6U && required_seen[required_index] < 1U) ||
			(required_index != 6U && required_seen[required_index] != 1U))
			return -1;
	if (binding_rows != 1U || source_count > ORLIX_TCTI_ACTIVE_EXECUTION_MAX_ROWS)
		return -1;
	for (*count = 0; *count < source_count; (*count)++) {
		rows[*count].ordinal = source[*count].ordinal;
		memcpy(rows[*count].proof, source[*count].proof,
			sizeof(rows[*count].proof));
	}
	return 0;
}

static int parse_classification_row(const char *body, size_t line_length,
	struct classification_row *row)
{
	const char *cursor;
	const char *end = body + line_length;
	const char *comma;
	unsigned int field;
	int consumed;

	if (sscanf(body,
		"ORLIX_TCTI_A64_TARGET_CLASSIFICATION(%127[A-Za-z0-9_], %31[A-Z0-9_], %n",
		row->name, row->state, &consumed) != 2)
		return -1;
	cursor = body + consumed;
	while (cursor < end && isspace((unsigned char)*cursor))
		cursor++;
	comma = memchr(cursor, ',', (size_t)(end - cursor));
	if (!comma || comma == cursor)
		return -1;
	while (comma > cursor && isspace((unsigned char)comma[-1]))
		comma--;
	if (comma == cursor)
		return -1;
	cursor = memchr(comma + 1U, '"', (size_t)(end - comma - 1U));
	if (!cursor)
		return -1;
	for (field = 0; field < 3U; field++) {
		const char *start;
		const char *quote;
		size_t value_length;

		while (cursor < end && isspace((unsigned char)*cursor))
			cursor++;
		if (cursor >= end || *cursor++ != '"')
			return -1;
		start = cursor;
		quote = memchr(cursor, '"', (size_t)(end - cursor));
		if (!quote)
			return -1;
		value_length = (size_t)(quote - start);
		if (field == 2U) {
			if (value_length >= sizeof(row->proof))
				return -1;
			memcpy(row->proof, start, value_length);
			row->proof[value_length] = '\0';
		}
		cursor = quote + 1U;
		while (cursor < end && isspace((unsigned char)*cursor))
			cursor++;
		if (field < 2U) {
			if (cursor >= end || *cursor++ != ',')
				return -1;
		} else if (cursor >= end || *cursor++ != ')') {
			return -1;
		}
	}
	while (cursor < end && isspace((unsigned char)*cursor))
		cursor++;
	return cursor == end ? 0 : -1;
}

static int parse_classification_rows(const char *text, size_t length,
	struct classification_row *rows, size_t *count)
{
	const char *cursor = text;
	const char *end = text + length;
	const char *line;
	const char *body;
	size_t line_length;

	*count = 0;
	while ((cursor = next_line(cursor, end, &line, &line_length))) {
		size_t index;

		if (!row_prefix(line, line_length,
			"ORLIX_TCTI_A64_TARGET_CLASSIFICATION(", &body))
			continue;
		if (*count == ORLIX_TCTI_ACTIVE_EXECUTION_MAX_ROWS ||
			parse_classification_row(body,
				line_length - (size_t)(body - line), &rows[*count]))
			return -1;
		for (index = 0; index < *count; index++)
			if (!strcmp(rows[index].name, rows[*count].name))
				return -1;
		(*count)++;
	}
	return *count ? 0 : -1;
}

static int hex_value(unsigned char value)
{
	if (value >= '0' && value <= '9')
		return value - '0';
	if (value >= 'a' && value <= 'f')
		return value - 'a' + 10;
	if (value >= 'A' && value <= 'F')
		return value - 'A' + 10;
	return -1;
}

static uint32_t big_endian_u32(const unsigned char *bytes)
{
	return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
		((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

static int tcnd_record_valid(const unsigned char *bytes, size_t length,
	size_t *offset, size_t depth)
{
	unsigned char tag;
	uint32_t payload_length;
	size_t payload_start;
	size_t payload_end;
	size_t child_count;

	if (depth >= 256U || *offset > length || length - *offset < 5U)
		return 0;
	tag = bytes[*offset];
	payload_length = big_endian_u32(bytes + *offset + 1U);
	payload_start = *offset + 5U;
	if ((size_t)payload_length > length - payload_start)
		return 0;
	payload_end = payload_start + payload_length;
	*offset = payload_start;
	if (tag == 1U)
		return payload_length == 1U && bytes[payload_start] <= 1U ?
			(*offset = payload_end, 1) : 0;
	if (tag >= 2U && tag <= 4U) {
		if (payload_length < 4U ||
			big_endian_u32(bytes + payload_start) != payload_length - 4U)
			return 0;
		*offset = payload_end;
		return 1;
	}
	if (tag == 5U) {
		if (payload_length < 4U)
			return 0;
		child_count = big_endian_u32(bytes + payload_start);
		*offset = payload_start + 4U;
		while (child_count--) {
			if (!tcnd_record_valid(bytes, payload_end, offset, depth + 1U))
				return 0;
		}
		return *offset == payload_end;
	}
	if (tag == 6U) {
		if (!tcnd_record_valid(bytes, payload_end, offset, depth + 1U))
			return 0;
		return *offset == payload_end;
	}
	if (tag >= 7U && tag <= 11U) {
		if (!tcnd_record_valid(bytes, payload_end, offset, depth + 1U) ||
			!tcnd_record_valid(bytes, payload_end, offset, depth + 1U))
			return 0;
		return *offset == payload_end;
	}
	return 0;
}

static int tcnd_hex_valid(const char *text)
{
	unsigned char *bytes;
	size_t hex_length;
	size_t byte_length;
	size_t index;
	size_t offset;
	int result = 0;

	if (!text)
		return 0;
	hex_length = strlen(text);
	if (hex_length < 10U || (hex_length & 1U) ||
		hex_length > ORLIX_TCTI_SOURCE_CONDITION_MAX_HEX ||
		strncmp(text, "54434e4401", 10U))
		return 0;
	byte_length = hex_length / 2U;
	bytes = malloc(byte_length);
	if (!bytes)
		return 0;
	for (index = 0; index < byte_length; index++) {
		int high = hex_value((unsigned char)text[index * 2U]);
		int low = hex_value((unsigned char)text[index * 2U + 1U]);

		if (high < 0 || low < 0)
			goto out;
		bytes[index] = (unsigned char)((high << 4) | low);
	}
	offset = 5U;
	result = tcnd_record_valid(bytes, byte_length, &offset, 0U) &&
		offset == byte_length;
out:
	free(bytes);
	return result;
}

static int parse_source_manifest(const char *text, size_t length,
	struct source_manifest_row *rows,
	struct source_manifest_summary *summary)
{
	const char *cursor = text;
	const char *end = text + length;
	const char *line;
	const char *body;
	size_t line_length;
	int source_header = 0;

	*summary = (struct source_manifest_summary) { 0 };
	while ((cursor = next_line(cursor, end, &line, &line_length))) {
		int consumed;

		if (row_prefix(line, line_length,
			"ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(", &body)) {
			char architecture[32];
			char build[32];
			char release[64];
			char schema[32];
			char digest[65];
			char timestamp[64];
			unsigned int row_count;
			int parsed;

			if (source_header++)
				return -1;
			parsed = sscanf(body,
				"ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(\"%31[^\"]\", \"%31[^\"]\", \"%63[^\"]\", \"%31[^\"]\", \"%64[^\"]\", %u, \"%63[^\"]\", %zuU%n",
				architecture, build, release, schema, digest, &row_count,
				timestamp, &summary->source_byte_length, &consumed);
			if (parsed != 8 || !row_count || !summary->source_byte_length ||
				!complete_row(body,
					line_length - (size_t)(body - line), consumed) ||
				row_count != ORLIX_TCTI_A64_TARGET_LEAF_COUNT)
				return -1;
			continue;
		}
		if (!row_prefix(line, line_length,
			"ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(", &body))
			continue;
		{
			char mnemonic[64];
			char operation[128];
			char condition[ORLIX_TCTI_SOURCE_CONDITION_MAX_HEX + 1U];
			unsigned int ordinal;
			int parsed;

			if (summary->row_count >= ORLIX_TCTI_A64_TARGET_LEAF_COUNT)
				return -1;
			parsed = sscanf(body,
				"ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(%u, \"%127[^\"]\", \"%63[^\"]\", \"%127[^\"]\", %*[^,], %*[^,], \"%8192[^\"]\", %*[^,], %*[^)]%n",
				&ordinal, rows[summary->row_count].name, mnemonic,
				operation, condition, &consumed);
			if (parsed != 5 || ordinal != summary->row_count ||
				!rows[summary->row_count].name[0] ||
				!tcnd_hex_valid(condition) ||
				!complete_row(body,
					line_length - (size_t)(body - line), consumed))
				return -1;
			rows[summary->row_count].condition_length = strlen(condition);
			orlix_tcti_target_artifact_sha256(condition,
				rows[summary->row_count].condition_length,
				rows[summary->row_count].condition_sha256);
			summary->row_count++;
		}
	}
	return source_header == 1 &&
		summary->row_count == ORLIX_TCTI_A64_TARGET_LEAF_COUNT ? 0 : -1;
}

static int parse_applicability_row(const char *text, const char *body,
	size_t line_length, struct applicability_row *row)
{
	static const char prefix[] =
		"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_ROW(";
	const char *cursor = body + sizeof(prefix) - 1U;
	const char *end = body + line_length;
	const char *value;
	const char *close;
	char condition[ORLIX_TCTI_SOURCE_CONDITION_MAX_HEX + 1U];
	size_t value_length;
	size_t condition_length;
	unsigned int status;
	const char *tail;

	if (parse_unsigned_u(&cursor, end, &row->ordinal) ||
		parse_comma(&cursor, end) || parse_quoted(&cursor, end, &value,
			&value_length) || !value_length || value_length >= sizeof(row->name))
		return -1;
	memcpy(row->name, value, value_length);
	row->name[value_length] = '\0';
	if (parse_comma(&cursor, end) || parse_quoted(&cursor, end, &value,
		&value_length) || parse_comma(&cursor, end) ||
		parse_quoted(&cursor, end, &value, &value_length) ||
		parse_comma(&cursor, end) ||
		parse_unsigned_u(&cursor, end, &row->condition_index) ||
		parse_comma(&cursor, end) ||
		parse_unsigned_u(&cursor, end, &row->condition_length) ||
		parse_comma(&cursor, end) || parse_unsigned_u(&cursor, end, &status) ||
		parse_comma(&cursor, end) || parse_quoted(&cursor, end, &value,
			&condition_length))
		return -1;
	if (!condition_length || condition_length > ORLIX_TCTI_SOURCE_CONDITION_MAX_HEX ||
		row->condition_length > ORLIX_TCTI_SOURCE_CONDITION_MAX_HEX / 2U ||
		condition_length != (size_t)row->condition_length * 2U)
		return -1;
	memcpy(condition, value, condition_length);
	condition[condition_length] = '\0';
	if (!tcnd_hex_valid(condition))
		return -1;
	row->status = status;
	if (row->status > 1U)
		return -1;
	row->condition_offset = (size_t)(value - text);
	row->condition_text_length = condition_length;
	orlix_tcti_target_artifact_sha256(condition, condition_length,
		row->condition_sha256);
	if (parse_comma(&cursor, end))
		return -1;
	tail = cursor;
	while (tail < end && isspace((unsigned char)*tail))
		tail++;
	close = end;
	while (close > tail && isspace((unsigned char)close[-1]))
		close--;
	if (tail >= close || close[-1] != ')')
		return -1;
	return 0;
}

static int parse_applicability_rows(const char *text, size_t length,
	struct applicability_row *rows, struct applicability_summary *summary)
{
	const char *cursor = text;
	const char *end = text + length;
	const char *line;
	const char *body;
	size_t line_length;

	*summary = (struct applicability_summary) { 0 };
	while ((cursor = next_line(cursor, end, &line, &line_length))) {
		int consumed;

		if (row_prefix(line, line_length,
			"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_SOURCE(", &body)) {
			char architecture[32];
			char release[64];
			char instructions[65];
			char features[65];
			char registers[65];
			char reconciliation[65];
			unsigned int row_count;
			unsigned int parameter_count;
			int parsed;

			if (summary->source_header++)
				return -1;
			parsed = sscanf(body,
				"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_SOURCE(\"%31[^\"]\", \"%63[^\"]\", \"%64[^\"]\", \"%64[^\"]\", \"%64[^\"]\", \"%64[^\"]\", %uU, %uU%n",
				architecture, release, instructions, features, registers,
				reconciliation, &row_count, &parameter_count, &consumed);
			if (parsed != 8 || strcmp(architecture,
				ORLIX_TCTI_A64_APPLICABILITY_ARCHITECTURE) ||
				strcmp(release, ORLIX_TCTI_A64_APPLICABILITY_RELEASE) ||
				strcmp(instructions, ORLIX_TCTI_A64_APPLICABILITY_INSTRUCTIONS_SHA256) ||
				strcmp(features, ORLIX_TCTI_A64_APPLICABILITY_FEATURES_SHA256) ||
				strcmp(registers, ORLIX_TCTI_A64_APPLICABILITY_REGISTERS_SHA256) ||
				strcmp(reconciliation,
					ORLIX_TCTI_A64_APPLICABILITY_RECONCILIATION_IDENTITY) ||
				row_count != ORLIX_TCTI_A64_TARGET_LEAF_COUNT ||
				parameter_count != ORLIX_TCTI_A64_APPLICABILITY_PARAMETER_COUNT ||
				!complete_row(body, line_length - (size_t)(body - line), consumed))
				return -1;
			continue;
		}
		if (row_prefix(line, line_length,
			"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_COUNTS(", &body)) {
			unsigned int common_count;
			unsigned int row_count;
			unsigned int operand_count;
			unsigned int common_bytes;
			int parsed = sscanf(body,
				"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_COUNTS(%uU, %uU, %uU, %uU%n",
				&common_count, &row_count, &operand_count, &common_bytes,
				&consumed);

			if (summary->counts_header++ || parsed != 4 ||
				common_count != ORLIX_TCTI_A64_APPLICABILITY_COMMON_VALUE_COUNT ||
				row_count != ORLIX_TCTI_A64_TARGET_LEAF_COUNT ||
				operand_count != ORLIX_TCTI_A64_APPLICABILITY_OPERAND_VALUE_COUNT ||
				common_bytes != ORLIX_TCTI_A64_APPLICABILITY_COMMON_VALUE_BYTES ||
				!complete_row(body, line_length - (size_t)(body - line), consumed))
				return -1;
			continue;
		}
		if (!row_prefix(line, line_length,
			"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_ROW(", &body))
			continue;
		if (summary->row_count >= ORLIX_TCTI_A64_TARGET_LEAF_COUNT ||
			parse_applicability_row(text, body,
				line_length - (size_t)(body - line), &rows[summary->row_count]) ||
			rows[summary->row_count].ordinal != summary->row_count)
			return -1;
		summary->row_count++;
	}
	return summary->source_header == 1 && summary->counts_header == 1 &&
		summary->row_count == ORLIX_TCTI_A64_TARGET_LEAF_COUNT ? 0 : -1;
}

static int parse_cohort_rows(const char *text, size_t length,
	struct cohort_leaf_row *leaves, struct cohort_row *memberships,
	struct cohort_summary *summary)
{
	const char *cursor = text;
	const char *end = text + length;
	const char *line;
	const char *body;
	size_t line_length;

	*summary = (struct cohort_summary) { 0 };
	while ((cursor = next_line(cursor, end, &line, &line_length))) {
		int consumed;

		if (row_prefix(line, line_length,
			"ORLIX_TCTI_A64_RUNTIME_CAPABILITY_COHORT_COUNTS(", &body)) {
			unsigned int leaf_count;
			unsigned int membership_count;
			unsigned int resolved_count;
			unsigned int unresolved_count;
			int parsed;

			if (summary->leaf_count)
				return -1;
			parsed = sscanf(body,
				"ORLIX_TCTI_A64_RUNTIME_CAPABILITY_COHORT_COUNTS(%uU, %uU, %uU, %uU%n",
				&leaf_count, &membership_count, &resolved_count,
				&unresolved_count, &consumed);
			if (parsed != 4 ||
				!complete_row(body,
					line_length - (size_t)(body - line), consumed) ||
				leaf_count != ORLIX_TCTI_A64_TARGET_LEAF_COUNT ||
				membership_count != ORLIX_TCTI_A64_TARGET_MEMBERSHIP_COUNT ||
				resolved_count != membership_count ||
				unresolved_count != membership_count)
				return -1;
			summary->leaf_count = leaf_count;
			summary->membership_count = membership_count;
			summary->resolved_count = resolved_count;
			summary->unresolved_count = unresolved_count;
			continue;
		}
		if (row_prefix(line, line_length,
			"ORLIX_TCTI_A64_RUNTIME_CAPABILITY_COHORT_LEAF(", &body)) {
			unsigned int index;
			unsigned int first_membership;
			unsigned int membership_count;
			int parsed;

			if (summary->leaf_rows >= ORLIX_TCTI_A64_TARGET_LEAF_COUNT)
				return -1;
			parsed = sscanf(body,
				"ORLIX_TCTI_A64_RUNTIME_CAPABILITY_COHORT_LEAF(%uU, %uU, %uU%n",
				&index, &first_membership, &membership_count, &consumed);
			if (parsed != 3 || index != summary->leaf_rows ||
				!complete_row(body,
					line_length - (size_t)(body - line), consumed))
				return -1;
			leaves[summary->leaf_rows++] = (struct cohort_leaf_row) {
				.index = index,
				.first_membership = first_membership,
				.membership_count = membership_count,
			};
			continue;
		}
		if (row_prefix(line, line_length,
			"ORLIX_TCTI_A64_RUNTIME_CAPABILITY_COHORT_MEMBERSHIP(", &body)) {
			char feature[64];
			char disposition[96];
			unsigned int index;
			unsigned int leaf;
			unsigned int parameter;
			unsigned int source_offset;
			unsigned int source_length;
			int parsed;

			if (summary->membership_rows >=
				ORLIX_TCTI_A64_TARGET_MEMBERSHIP_COUNT)
				return -1;
			parsed = sscanf(body,
				"ORLIX_TCTI_A64_RUNTIME_CAPABILITY_COHORT_MEMBERSHIP(%uU, %uU, %uU, \"%63[^\"]\", %95[^,], %uU, %uU%n",
				&index, &leaf, &parameter,
				feature,
				disposition, &source_offset, &source_length, &consumed);
			if (parsed != 7 || index != summary->membership_rows ||
				leaf >= ORLIX_TCTI_A64_TARGET_LEAF_COUNT ||
				!feature[0] ||
				!source_length || !disposition[0] ||
				!complete_row(body,
					line_length - (size_t)(body - line), consumed))
				return -1;
			memberships[summary->membership_rows] = (struct cohort_row) {
				.index = index,
				.leaf = leaf,
				.parameter = parameter,
				.source_offset = source_offset,
				.source_length = source_length,
			};
			strncpy(memberships[summary->membership_rows].feature, feature,
				sizeof(memberships[summary->membership_rows].feature) - 1U);
			memberships[summary->membership_rows].feature[
				sizeof(memberships[summary->membership_rows].feature) - 1U] = '\0';
			summary->membership_rows++;
		}
	}
	if (summary->leaf_count != ORLIX_TCTI_A64_TARGET_LEAF_COUNT ||
		summary->membership_count != ORLIX_TCTI_A64_TARGET_MEMBERSHIP_COUNT ||
		summary->leaf_rows != ORLIX_TCTI_A64_TARGET_LEAF_COUNT ||
		summary->membership_rows != ORLIX_TCTI_A64_TARGET_MEMBERSHIP_COUNT)
		return -1;
	{
		size_t index;
		size_t membership_sum = 0;

		for (index = 0; index < summary->leaf_rows; index++) {
			const struct cohort_leaf_row *leaf = &leaves[index];
			size_t membership_index;

			if ((size_t)leaf->first_membership + leaf->membership_count >
				ORLIX_TCTI_A64_TARGET_MEMBERSHIP_COUNT)
				return -1;
			membership_sum += leaf->membership_count;
			if (membership_sum !=
				(size_t)leaf->first_membership + leaf->membership_count)
				return -1;
			for (membership_index = 0;
			     membership_index < leaf->membership_count;
			     membership_index++) {
				if (memberships[leaf->first_membership + membership_index].leaf !=
					leaf->index)
					return -1;
			}
		}
		if (membership_sum != ORLIX_TCTI_A64_TARGET_MEMBERSHIP_COUNT)
			return -1;
	}
	return 0;
}

static int feature_known(const char *features, size_t length,
	const char *feature)
{
	size_t feature_length = strlen(feature);
	size_t offset;

	if (!feature_length || feature_length > length)
		return 0;
	for (offset = 0; offset + feature_length < length; offset++)
		if (!memcmp(features + offset, feature, feature_length) &&
			offset > 0U && features[offset - 1U] == '"' &&
			features[offset + feature_length] == '"')
			return 1;
	return 0;
}

static int condition_byte(const char *text, size_t offset,
	unsigned char *value)
{
	int high = hex_value((unsigned char)text[offset]);
	int low = hex_value((unsigned char)text[offset + 1U]);

	if (high < 0 || low < 0)
		return -1;
	*value = (unsigned char)((high << 4) | low);
	return 0;
}

struct condition_feature_state {
	int mentioned;
	int required;
	int unsafe;
	int always_true;
	int always_false;
};

static int condition_u32(const char *text, size_t *cursor, size_t limit,
	unsigned int *value)
{
	unsigned char bytes[4];
	size_t index;

	if (*cursor > limit || limit - *cursor < sizeof(bytes) * 2U)
		return -1;
	for (index = 0; index < sizeof(bytes); index++)
		if (condition_byte(text, *cursor + index * 2U, &bytes[index]))
			return -1;
	*cursor += sizeof(bytes) * 2U;
	*value = ((unsigned int)bytes[0] << 24) |
		((unsigned int)bytes[1] << 16) | ((unsigned int)bytes[2] << 8) |
		(unsigned int)bytes[3];
	return 0;
}

static int condition_text(const char *text, size_t *cursor, size_t limit,
	size_t *start, size_t *length)
{
	unsigned int encoded_length;

	if (condition_u32(text, cursor, limit, &encoded_length) || !encoded_length ||
		(size_t)encoded_length > (limit - *cursor) / 2U)
		return -1;
	*start = *cursor;
	*length = (size_t)encoded_length;
	*cursor += *length * 2U;
	return 0;
}

static int condition_text_is_feature(const char *text, size_t start,
	size_t length, const char *feature)
{
	size_t feature_length = strlen(feature);
	size_t index;

	if (feature_length != length)
		return 0;
	for (index = 0; index < length; index++) {
		unsigned char value;

		if (condition_byte(text, start + index * 2U, &value) ||
			value != (unsigned char)feature[index])
			return 0;
	}
	return 1;
}

static void condition_state_merge(struct condition_feature_state *state,
	const struct condition_feature_state *child)
{
	state->mentioned |= child->mentioned;
	state->required |= child->required;
	state->unsafe |= child->unsafe;
}

static int condition_feature_expression(const char *text, size_t *cursor,
	size_t limit, unsigned int depth, const char *feature, int positive,
	struct condition_feature_state *state)
{
	unsigned char tag;
	unsigned int payload_length;
	size_t payload_end;
	size_t payload_start;

	if (!state || depth >= ORLIX_TCTI_TARGET_CONDITION_MAX_DEPTH ||
		*cursor > limit || limit - *cursor < 10U ||
		condition_byte(text, *cursor, &tag))
		return -1;
	*cursor += 2U;
	if (condition_u32(text, cursor, limit, &payload_length) ||
		(size_t)payload_length > (limit - *cursor) / 2U)
		return -1;
	payload_start = *cursor;
	payload_end = payload_start + (size_t)payload_length * 2U;
	*state = (struct condition_feature_state) { 0 };
	switch (tag) {
	case ORLIX_TCTI_TARGET_CONDITION_BOOL: {
		unsigned char value;

		if (payload_length != 1U || condition_byte(text, *cursor, &value) ||
			value > 1U)
			return -1;
		state->always_true = value != 0U;
		state->always_false = value == 0U;
		*cursor = payload_end;
		return 0;
	}
	case ORLIX_TCTI_TARGET_CONDITION_FEATURE: {
		size_t start;
		size_t length;

		if (condition_text(text, cursor, payload_end, &start, &length) ||
			*cursor != payload_end)
			return -1;
		if (condition_text_is_feature(text, start, length, feature)) {
			state->mentioned = 1;
			state->required = positive;
		}
		return 0;
	}
	case ORLIX_TCTI_TARGET_CONDITION_OPERAND:
	case ORLIX_TCTI_TARGET_CONDITION_VALUE: {
		size_t start;
		size_t length;

		if (condition_text(text, cursor, payload_end, &start, &length) ||
			*cursor != payload_end)
			return -1;
		(void)start;
		(void)length;
		return 0;
	}
	case ORLIX_TCTI_TARGET_CONDITION_SET: {
		unsigned int count;
		unsigned int index;

		if (condition_u32(text, cursor, payload_end, &count))
			return -1;
		for (index = 0; index < count; index++) {
			struct condition_feature_state child;

			if (condition_feature_expression(text, cursor, payload_end,
				depth + 1U, feature, 0, &child))
				return -1;
			condition_state_merge(state, &child);
		}
		if (*cursor != payload_end)
			return -1;
		if (state->mentioned) {
			state->unsafe = 1;
			state->required = 0;
		}
		state->always_true = 0;
		state->always_false = 0;
		return 0;
	}
	case ORLIX_TCTI_TARGET_CONDITION_NOT: {
		struct condition_feature_state child;

		if (condition_feature_expression(text, cursor, payload_end,
			depth + 1U, feature, 0, &child) || *cursor != payload_end)
			return -1;
		*state = child;
		state->always_true = child.always_false;
		state->always_false = child.always_true;
		if (state->mentioned) {
			state->unsafe = 1;
			state->required = 0;
		}
		return 0;
	}
	case ORLIX_TCTI_TARGET_CONDITION_AND:
	case ORLIX_TCTI_TARGET_CONDITION_OR:
	case ORLIX_TCTI_TARGET_CONDITION_EQ:
	case ORLIX_TCTI_TARGET_CONDITION_NE:
	case ORLIX_TCTI_TARGET_CONDITION_IN: {
		struct condition_feature_state left;
		struct condition_feature_state right;
		int child_positive = tag == ORLIX_TCTI_TARGET_CONDITION_AND ?
			positive : 0;

		if (condition_feature_expression(text, cursor, payload_end,
			depth + 1U, feature, child_positive, &left) ||
			condition_feature_expression(text, cursor, payload_end,
				depth + 1U, feature, child_positive, &right) ||
			*cursor != payload_end)
			return -1;
		condition_state_merge(state, &left);
		condition_state_merge(state, &right);
		if (tag != ORLIX_TCTI_TARGET_CONDITION_AND && state->mentioned) {
			state->unsafe = 1;
			state->required = 0;
		} else if (tag == ORLIX_TCTI_TARGET_CONDITION_AND) {
			state->required = positive && !state->unsafe &&
				(left.required || right.required);
		}
		if (tag == ORLIX_TCTI_TARGET_CONDITION_AND) {
			state->always_true = left.always_true && right.always_true;
			state->always_false = left.always_false || right.always_false;
		} else if (tag == ORLIX_TCTI_TARGET_CONDITION_OR) {
			state->always_true = left.always_true || right.always_true;
			state->always_false = left.always_false && right.always_false;
		} else {
			state->always_true = 0;
			state->always_false = 0;
		}
		return 0;
	}
	default:
		return -1;
	}
}

static int applicability_requires_feature(const char *text,
	const struct applicability_row *row, const char *feature)
{
	static const unsigned char header[] = { 'T', 'C', 'N', 'D', 1U };
	struct condition_feature_state state;
	size_t cursor;
	size_t limit;
	size_t index;

	if (!text || !row || !feature || !feature[0] ||
		row->condition_text_length & 1U ||
		row->condition_offset > SIZE_MAX - row->condition_text_length)
		return 0;
	cursor = row->condition_offset;
	limit = cursor + row->condition_text_length;
	if (row->condition_text_length < sizeof(header) * 2U)
		return 0;
	for (index = 0; index < sizeof(header); index++) {
		unsigned char value;

		if (condition_byte(text, cursor + index * 2U, &value) ||
			value != header[index])
			return 0;
	}
	cursor += sizeof(header) * 2U;
	if (condition_feature_expression(text, &cursor, limit, 0U, feature, 1,
		&state) || cursor != limit)
		return 0;
	return state.required && !state.unsafe && !state.always_false;
}

static int source_proof_matches(const struct source_proof_row *rows,
	size_t count, unsigned int ordinal, const char *proof)
{
	size_t index;

	for (index = 0; index < count; index++)
		if (rows[index].ordinal == ordinal && !strcmp(rows[index].proof, proof))
			return 1;
	return 0;
}

static int registry_matches(const struct registry_row *rows, size_t count,
	unsigned int ordinal, const char *proof)
{
	size_t index;

	for (index = 0; index < count; index++)
		if (rows[index].ordinal == ordinal && !strcmp(rows[index].proof, proof))
			return 1;
	return 0;
}

static int classification_matches(const struct classification_row *rows,
	size_t count, const char *name, const char *proof)
{
	size_t index;

	for (index = 0; index < count; index++)
		if (!strcmp(rows[index].state, "REQUIRED") &&
			!strcmp(rows[index].name, name) &&
			!strcmp(rows[index].proof, proof) && rows[index].proof[0])
			return 1;
	return 0;
}

static int source_registry_bijective(const struct source_proof_row *source,
	size_t source_count, const struct registry_row *registry,
	size_t registry_count)
{
	size_t index;

	if (source_count != registry_count)
		return 0;
	for (index = 0; index < source_count; index++)
		if (!registry_matches(registry, registry_count, source[index].ordinal,
			source[index].proof))
			return 0;
	for (index = 0; index < registry_count; index++)
		if (!source_proof_matches(source, source_count, registry[index].ordinal,
			registry[index].proof))
			return 0;
	return 1;
}

static const char *source_name(const struct source_manifest_row *rows,
	size_t count, unsigned int ordinal)
{
	return ordinal < count ? rows[ordinal].name : NULL;
}

enum orlix_tcti_active_execution_profile_artifact_generator_error
orlix_tcti_active_execution_profile_artifact_emit(const char *features,
	size_t features_length, const char *feature_applicability,
	size_t feature_applicability_length, const char *profile,
	size_t profile_length,
	const char *promotion_manifest, size_t promotion_manifest_length,
	const char *generator_schema, size_t generator_schema_length,
	const char *source_bound_proof, size_t source_bound_proof_length,
	const char *runtime_capability_cohort, size_t runtime_capability_cohort_length,
	const char *classification, size_t classification_length,
	const char *proof_registry, size_t proof_registry_length,
	const char *source_manifest, size_t source_manifest_length, FILE *output)
{
	struct profile_row *profiles = NULL;
	struct promotion_row *promotions = NULL;
	struct cohort_leaf_row *cohort_leaves = NULL;
	struct cohort_row *cohorts = NULL;
	struct source_proof_row *proofs = NULL;
	struct classification_row *classes = NULL;
	struct registry_row *registry = NULL;
	struct source_manifest_row *source_rows = NULL;
	struct applicability_row *applicability = NULL;
	struct source_manifest_summary source_summary;
	struct cohort_summary cohort_summary;
	struct applicability_summary applicability_summary;
	size_t profile_count = 0;
	size_t promotion_count = 0;
	size_t proof_count = 0;
	size_t class_count = 0;
	size_t registry_count = 0;
	size_t enabled = 0;
	size_t index;
	char profile_sha256[65];
	char manifest_sha256[65];
	char schema_sha256[65];
	char proof_sha256[65];
	char cohort_sha256[65];
	char classification_sha256[65];
	char registry_sha256[65];
	char source_manifest_sha256[65];
	char applicability_sha256[65];
	enum orlix_tcti_active_execution_profile_artifact_generator_error result =
		ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_INVALID_ARGUMENT;

	if (!features || !features_length || !feature_applicability ||
		!feature_applicability_length || !profile || !profile_length ||
		!promotion_manifest || !promotion_manifest_length || !generator_schema ||
		!generator_schema_length || !source_bound_proof ||
		!source_bound_proof_length || !runtime_capability_cohort ||
		!runtime_capability_cohort_length || !classification ||
		!classification_length || !proof_registry || !proof_registry_length ||
		!source_manifest || !source_manifest_length || !output)
		return result;

	profiles = calloc(ORLIX_TCTI_ACTIVE_EXECUTION_MAX_ROWS, sizeof(*profiles));
	promotions = calloc(ORLIX_TCTI_ACTIVE_EXECUTION_MAX_ROWS, sizeof(*promotions));
	cohort_leaves = calloc(ORLIX_TCTI_A64_TARGET_LEAF_COUNT,
		sizeof(*cohort_leaves));
	cohorts = calloc(ORLIX_TCTI_A64_TARGET_MEMBERSHIP_COUNT,
		sizeof(*cohorts));
	proofs = calloc(ORLIX_TCTI_ACTIVE_EXECUTION_MAX_ROWS, sizeof(*proofs));
	classes = calloc(ORLIX_TCTI_ACTIVE_EXECUTION_MAX_ROWS, sizeof(*classes));
	registry = calloc(ORLIX_TCTI_ACTIVE_EXECUTION_MAX_ROWS, sizeof(*registry));
	source_rows = calloc(ORLIX_TCTI_A64_TARGET_LEAF_COUNT,
		sizeof(*source_rows));
	applicability = calloc(ORLIX_TCTI_A64_TARGET_LEAF_COUNT,
		sizeof(*applicability));
	if (!profiles || !promotions || !cohort_leaves || !cohorts || !proofs ||
		!classes || !registry || !source_rows || !applicability)
		goto out;

	if (parse_source_manifest(source_manifest, source_manifest_length,
		source_rows, &source_summary)) {
		result = ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_SOURCE_MISMATCH;
		goto out;
	}
	if (parse_applicability_rows(feature_applicability,
		feature_applicability_length, applicability, &applicability_summary)) {
		result = ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_SOURCE_MISMATCH;
		goto out;
	}
	for (index = 0; index < ORLIX_TCTI_A64_TARGET_LEAF_COUNT; index++)
		if (strcmp(applicability[index].name, source_rows[index].name) ||
			applicability[index].condition_text_length !=
				source_rows[index].condition_length ||
			strcmp(applicability[index].condition_sha256,
				source_rows[index].condition_sha256)) {
			result = ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_SOURCE_MISMATCH;
			goto out;
		}
	if (parse_cohort_rows(runtime_capability_cohort, runtime_capability_cohort_length,
		cohort_leaves, cohorts, &cohort_summary)) {
		result = ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_SOURCE_MISMATCH;
		goto out;
	}
	if (parse_profile_rows(profile, profile_length, profiles, &profile_count)) {
		result = ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_SOURCE_MISMATCH;
		goto out;
	}
	if (parse_promotion_rows(promotion_manifest, promotion_manifest_length,
		promotions, &promotion_count)) {
		result = ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_SOURCE_MISMATCH;
		goto out;
	}
	if (parse_source_proof_rows(source_bound_proof, source_bound_proof_length,
		proofs, &proof_count)) {
		result = ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_SOURCE_MISMATCH;
		goto out;
	}
	if (parse_classification_rows(classification, classification_length,
		classes, &class_count)) {
		result = ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_SOURCE_MISMATCH;
		goto out;
	}
	if (parse_registry_rows(proof_registry, proof_registry_length, proofs,
		proof_count, registry, &registry_count)) {
		result = ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_SOURCE_MISMATCH;
		goto out;
	}
	if (!source_registry_bijective(proofs, proof_count, registry, registry_count)) {
		result = ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_SOURCE_MISMATCH;
		goto out;
	}
	for (index = 0; index < cohort_summary.membership_rows; index++) {
		const struct cohort_row *cohort = &cohorts[index];

			if (!feature_known(features, features_length, cohort->feature) ||
				(size_t)cohort->source_offset + cohort->source_length >
					source_summary.source_byte_length) {
				result = ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_SOURCE_MISMATCH;
			goto out;
		}
	}
	for (index = 0; index < profile_count; index++) {
		if (!feature_known(features, features_length, profiles[index].feature)) {
			result = ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_UNKNOWN_FEATURE;
			goto out;
		}
		if (profiles[index].state == ENABLED)
			enabled++;
	}
	for (index = 0; index < profile_count; index++) {
		size_t cohort_matches = 0;
		size_t declared = 0;
		size_t cohort_index;
		size_t promotion_index;

		for (cohort_index = 0; cohort_index < cohort_summary.membership_rows;
			cohort_index++)
			if (!strcmp(cohorts[cohort_index].feature,
				profiles[index].feature))
				cohort_matches++;
		for (promotion_index = 0; promotion_index < promotion_count;
			promotion_index++) {
			const struct promotion_row *promotion = &promotions[promotion_index];
			const char *expected_source_name;

			if (strcmp(promotion->feature, profiles[index].feature))
				continue;
			declared++;
			expected_source_name = source_name(source_rows, source_summary.row_count,
				promotion->ordinal);
			if (profiles[index].state != ENABLED ||
				strcmp(promotion->identity, profiles[index].identity) ||
				promotion->ordinal >= ORLIX_TCTI_A64_TARGET_LEAF_COUNT ||
				!expected_source_name ||
				strcmp(expected_source_name, promotion->classification) ||
				applicability[promotion->ordinal].status != 1U ||
				strcmp(applicability[promotion->ordinal].name,
					expected_source_name) ||
				!applicability_requires_feature(feature_applicability,
					&applicability[promotion->ordinal], profiles[index].feature) ||
				!source_proof_matches(proofs, proof_count, promotion->ordinal,
					promotion->proof) ||
				!registry_matches(registry, registry_count, promotion->ordinal,
					promotion->proof) ||
				!classification_matches(classes, class_count,
					promotion->classification, promotion->proof)) {
				result = ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_UNPROMOTED_ENABLE;
				goto out;
			}
			{
				size_t matches = 0;

				for (cohort_index = 0;
				     cohort_index < cohort_summary.membership_rows;
				     cohort_index++)
					if (cohorts[cohort_index].leaf == promotion->ordinal &&
						!strcmp(cohorts[cohort_index].feature,
							profiles[index].feature))
						matches++;
				if (matches != 1U) {
					result = ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_UNPROMOTED_ENABLE;
					goto out;
				}
			}
		}
		if ((profiles[index].state == ENABLED &&
			(cohort_matches == 0U || cohort_matches != declared)) ||
			(profiles[index].state == DISABLED && declared)) {
			result = ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_UNPROMOTED_ENABLE;
			goto out;
		}
	}
	for (index = 0; index < promotion_count; index++) {
		size_t profile_index;

		for (profile_index = 0; profile_index < profile_count; profile_index++)
			if (!strcmp(profiles[profile_index].feature,
				promotions[index].feature))
				break;
		if (profile_index == profile_count ||
			profiles[profile_index].state != ENABLED) {
			result = ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_UNPROMOTED_ENABLE;
			goto out;
		}
	}

	orlix_tcti_target_artifact_sha256(profile, profile_length, profile_sha256);
	orlix_tcti_target_artifact_sha256(promotion_manifest,
		promotion_manifest_length, manifest_sha256);
	orlix_tcti_target_artifact_sha256(generator_schema, generator_schema_length,
		schema_sha256);
	orlix_tcti_target_artifact_sha256(source_bound_proof,
		source_bound_proof_length, proof_sha256);
	orlix_tcti_target_artifact_sha256(runtime_capability_cohort,
		runtime_capability_cohort_length, cohort_sha256);
	orlix_tcti_target_artifact_sha256(classification, classification_length,
		classification_sha256);
	orlix_tcti_target_artifact_sha256(proof_registry, proof_registry_length,
		registry_sha256);
	orlix_tcti_target_artifact_sha256(source_manifest, source_manifest_length,
		source_manifest_sha256);
	orlix_tcti_target_artifact_sha256(feature_applicability,
		feature_applicability_length, applicability_sha256);
	if (fprintf(output,
		"/* SPDX-License-Identifier: GPL-2.0-only */\n"
		"ORLIX_TCTI_A64_ACTIVE_EXECUTION_PROFILE_SOURCE("
		"%zuU, \"%s\", %zuU, \"%s\", %zuU, \"%s\", "
		"%zuU, \"%s\", %zuU, \"%s\", %zuU, \"%s\", "
		"%zuU, \"%s\", %zuU, \"%s\", %zuU, \"%s\")\n"
		"ORLIX_TCTI_A64_ACTIVE_EXECUTION_PROFILE_SOURCE_MANIFEST(%zuU, %zuU, \"%s\")\n"
		"ORLIX_TCTI_A64_ACTIVE_EXECUTION_PROFILE_COUNTS(%zuU, %zuU, %zuU)\n",
		profile_length, profile_sha256, promotion_manifest_length, manifest_sha256,
		generator_schema_length, schema_sha256, source_bound_proof_length,
		proof_sha256, runtime_capability_cohort_length, cohort_sha256,
		classification_length, classification_sha256, proof_registry_length,
		registry_sha256, source_manifest_length, source_manifest_sha256,
		feature_applicability_length, applicability_sha256,
		source_summary.row_count, source_summary.source_byte_length,
		source_manifest_sha256, profile_count, enabled, profile_count - enabled) < 0)
		goto io;
	for (index = 0; index < profile_count; index++)
		if (fprintf(output,
			"ORLIX_TCTI_A64_ACTIVE_EXECUTION_PROFILE_ENTRY(%zuU, \"%s\", %s, \"%s\")\n",
			index, profiles[index].feature,
			profiles[index].state == ENABLED ? "ENABLED" : "DISABLED",
			profiles[index].identity) < 0)
			goto io;
	result = ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_OK;
	goto out;
io:
	result = ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_IO;
out:
	free(source_rows);
	free(registry);
	free(classes);
	free(proofs);
	free(cohorts);
	free(cohort_leaves);
	free(promotions);
	free(profiles);
	free(applicability);
	return result;
}

const char *orlix_tcti_active_execution_profile_artifact_generator_error_name(
	enum orlix_tcti_active_execution_profile_artifact_generator_error error)
{
	static const char *const names[] = {
		"ok",
		"invalid argument",
		"unknown feature",
		"duplicate feature",
		"unpromoted enable",
		"source mismatch",
		"count mismatch",
		"I/O failure",
	};

	return error < sizeof(names) / sizeof(names[0]) ? names[error] : "unknown";
}
