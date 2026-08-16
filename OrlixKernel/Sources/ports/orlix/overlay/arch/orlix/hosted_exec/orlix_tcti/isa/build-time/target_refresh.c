/* SPDX-License-Identifier: GPL-2.0-only */
#define _POSIX_C_SOURCE 200809L
#include "target_asl_availability.h"
#include "target_feature_artifact_generator.h"
#include "target_feature_applicability_generator.h"
#include "target_feature_field_domain_binding_artifact_generator.h"
#include "target_feature_sat.h"
#include "target_runtime_capability_cohort_artifact_generator.h"
#include "target_active_execution_profile_artifact_generator.h"
#include "target_instruction_artifact_generator.h"
#include "target_manifest_generator.h"
#include "target_refresh.h"
#include "target_register_artifact_generator.h"
#include "target_system_accessor_reconciliation.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
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

#define ORLIX_TCTI_TARGET_REFRESH_MAX_SOURCE (128U * 1024U * 1024U)
#define ORLIX_TCTI_TARGET_REFRESH_PUBLISH_NAME "generations"
#define ORLIX_TCTI_TARGET_REFRESH_GENERATION_PREFIX "aarchmrs-2026-06-v3"
#define ORLIX_TCTI_TARGET_REFRESH_INSTRUCTIONS_SHA256 \
	"a1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe"
#define ORLIX_TCTI_TARGET_REFRESH_FEATURES_SHA256 \
	"633259000ffd3da32900bd0c0c1beae4a9eea7095c278f74d62a00c846b41187"
#define ORLIX_TCTI_TARGET_REFRESH_REGISTERS_SHA256 \
	"5bd76c3c3ce90322eb4fd179675dafe82df2fd1cb789beee516e5b29c471b874"
#define ORLIX_TCTI_TARGET_REFRESH_RECONCILIATION_IDENTITY \
	"ceb8f8c561a5ecdce1a32bda12c7f33fc1b0433bbf035301f2590b6000ccbfe4"
#define ORLIX_TCTI_TARGET_REFRESH_OPERAND_CERTIFICATES 364U
#define ORLIX_TCTI_TARGET_REFRESH_BOOLEAN_IDENTIFIERS 5U
#define ORLIX_TCTI_TARGET_REFRESH_ARCHITECTURE "vFATAp1-A"
#define ORLIX_TCTI_TARGET_REFRESH_BUILD "818"
#define ORLIX_TCTI_TARGET_REFRESH_RELEASE "2026-06_rel"
#define ORLIX_TCTI_TARGET_REFRESH_SOURCE_SCHEMA "2.9.5"
#define ORLIX_TCTI_TARGET_REFRESH_TIMESTAMP "2026-06-24 17:12:14"
#define ORLIX_TCTI_TARGET_REFRESH_INSTRUCTIONS_BYTE_LENGTH 115441429U
#define ORLIX_TCTI_TARGET_REFRESH_FEATURES_BYTE_LENGTH 1243621U
#define ORLIX_TCTI_TARGET_REFRESH_REGISTERS_BYTE_LENGTH 96016602U
#define ORLIX_TCTI_TARGET_REFRESH_SCHEMA "orlix-tcti-aarchmrs-source-v3"
#define ORLIX_TCTI_TARGET_REFRESH_GENERATOR "orlix-tcti-target-refresh-active-execution-profile-v3"
#define ORLIX_TCTI_TARGET_REFRESH_FIELD_DOMAIN_OCCURRENCES 605U
#define ORLIX_TCTI_TARGET_REFRESH_FIELD_DOMAIN_GROUPS 362U
#define ORLIX_TCTI_TARGET_REFRESH_FIELD_DOMAIN_MAPPED 605U
#define ORLIX_TCTI_TARGET_REFRESH_FIELD_DOMAIN_AMBIGUOUS 0U
#define ORLIX_TCTI_TARGET_REFRESH_FIELD_DOMAIN_ALTERNATIVES 606U
#define ORLIX_TCTI_TARGET_REFRESH_FIELD_DOMAIN_OCCURRENCE_V3 \
	"ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_OCCURRENCE_V3("

struct source_bytes {
	char *data;
	size_t length;
};

struct artifact_bytes {
	char *data;
	size_t length;
};

static int emit_active_execution_profile(const struct source_bytes *features,
	const struct artifact_bytes *feature_applicability,
	const struct source_bytes *profile,
	const struct source_bytes *promotion_manifest,
	const struct source_bytes *generator_schema,
	const struct source_bytes *proof,
	const struct source_bytes *cohort,
	const struct source_bytes *classification,
	const struct source_bytes *registry,
	const struct artifact_bytes *source_manifest,
	struct artifact_bytes *artifact);

#define ARRAY_SIZE(values) (sizeof(values) / sizeof((values)[0]))

static size_t count_token(const struct artifact_bytes *artifact,
			  const char *token)
{
	const char *cursor = artifact->data;
	size_t count = 0;
	size_t token_length = strlen(token);

	while (cursor && (cursor = strstr(cursor, token))) {
		count++;
		cursor += token_length;
	}
	return count;
}

static int has_exact_line(const struct artifact_bytes *artifact, const char *line)
{
	const char *cursor = artifact->data;
	const char *end = artifact->data + artifact->length;
	size_t expected = strlen(line);

	while (cursor < end) {
		const char *line_end = memchr(cursor, '\n', (size_t)(end - cursor));
		size_t length = line_end ? (size_t)(line_end - cursor) :
			(size_t)(end - cursor);

		if (length == expected && !memcmp(cursor, line, expected))
			return 1;
		cursor = line_end ? line_end + 1 : end;
	}
	return 0;
}

static int has_token(const struct artifact_bytes *artifact, const char *token)
{
	return count_token(artifact, token) != 0;
}

static size_t count_macro_rows(const struct artifact_bytes *artifact,
			       const char *token)
{
	const char *cursor = artifact->data;
	const char *end = artifact->data + artifact->length;
	size_t token_length = strlen(token);
	size_t count = 0;

	while (cursor < end) {
		const char *line_end = memchr(cursor, '\n', (size_t)(end - cursor));
		size_t line_length = line_end ? (size_t)(line_end - cursor) :
			(size_t)(end - cursor);

		if (line_length >= token_length &&
		    !memcmp(cursor, token, token_length)) {
			if (line_length == token_length || cursor[line_length - 1U] != ')')
				return SIZE_MAX;
			count++;
		}
		cursor = line_end ? line_end + 1 : end;
	}
	return count;
}

static int parse_counts(const struct artifact_bytes *artifact,
			const char *marker, size_t *values, size_t count)
{
	const char *cursor = strstr(artifact->data, marker);
	size_t index;

	if (!cursor)
		return -1;
	cursor += strlen(marker);
	for (index = 0; index < count; index++) {
		char *end;
		unsigned long long value;

		while (*cursor == ' ' || *cursor == '\t')
			cursor++;
		errno = 0;
		value = strtoull(cursor, &end, 10);
		if (errno || end == cursor || value > SIZE_MAX)
			return -1;
		values[index] = (size_t)value;
		cursor = end;
		if (*cursor == 'U')
			cursor++;
		while (*cursor == ' ' || *cursor == '\t')
			cursor++;
		if (index + 1U < count) {
			if (*cursor++ != ',')
				return -1;
		} else if (*cursor != ')') {
			return -1;
		}
	}
	return 0;
}

enum feature_field_domain_count_index {
	FIELD_DOMAIN_OCCURRENCES,
	FIELD_DOMAIN_GROUPS,
	FIELD_DOMAIN_MAPPED,
	FIELD_DOMAIN_AMBIGUOUS,
	FIELD_DOMAIN_ALTERNATIVES,
	FIELD_DOMAIN_RANGES,
	FIELD_DOMAIN_VALUESETS,
	FIELD_DOMAIN_DOMAINS,
	FIELD_DOMAIN_LINKS,
	FIELD_DOMAIN_EXPRESSIONS,
	FIELD_DOMAIN_EXPRESSION_CHILDREN,
	FIELD_DOMAIN_VALUE_CANDIDATES,
	FIELD_DOMAIN_CONSTRAINTS,
	FIELD_DOMAIN_CONSTRAINT_ITEMS,
	FIELD_DOMAIN_CONSTRAINT_CANDIDATES,
	FIELD_DOMAIN_COUNT
};

struct feature_field_domain_row_contract {
	const char *token;
	enum feature_field_domain_count_index count_index;
};

static int validate_feature_field_domain_artifact(
	const struct artifact_bytes *artifact)
{
	static const struct feature_field_domain_row_contract rows[] = {
		{ ORLIX_TCTI_TARGET_REFRESH_FIELD_DOMAIN_OCCURRENCE_V3,
		  FIELD_DOMAIN_OCCURRENCES },
		{ "ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_ALTERNATIVE_V3(",
		  FIELD_DOMAIN_ALTERNATIVES },
		{ "ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_RANGE_V3(",
		  FIELD_DOMAIN_RANGES },
		{ "ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_VALUESET_V3(",
		  FIELD_DOMAIN_VALUESETS },
		{ "ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_DOMAIN_V3(",
		  FIELD_DOMAIN_DOMAINS },
		{ "ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_LINK_V3(",
		  FIELD_DOMAIN_LINKS },
		{ "ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_EXPRESSION_V3(",
		  FIELD_DOMAIN_EXPRESSIONS },
		{ "ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_EXPRESSION_CHILD_V3(",
		  FIELD_DOMAIN_EXPRESSION_CHILDREN },
		{ "ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_VALUE_CANDIDATE_V3(",
		  FIELD_DOMAIN_VALUE_CANDIDATES },
		{ "ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_CONSTRAINT_V3(",
		  FIELD_DOMAIN_CONSTRAINTS },
		{ "ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_CONSTRAINT_ITEM_V3(",
		  FIELD_DOMAIN_CONSTRAINT_ITEMS },
		{ "ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_CONSTRAINT_CANDIDATE_V3(",
		  FIELD_DOMAIN_CONSTRAINT_CANDIDATES },
	};
	size_t counts[FIELD_DOMAIN_COUNT];
	size_t index;

	if (!artifact->data || artifact->length != strlen(artifact->data) ||
	    count_macro_rows(artifact,
		"ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_SOURCE(") != 1U ||
	    !has_token(artifact, ORLIX_TCTI_TARGET_REFRESH_FEATURES_SHA256) ||
	    !has_token(artifact, ORLIX_TCTI_TARGET_REFRESH_REGISTERS_SHA256) ||
	    count_macro_rows(artifact,
		"ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_COUNTS_V3(") != 1U ||
	    count_macro_rows(artifact,
		"ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_IDENTITY(") != 1U ||
	    count_token(artifact,
		"ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_COUNTS(") != 0U ||
	    count_token(artifact,
		"ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_OCCURRENCE_V2(") != 0U ||
	    parse_counts(artifact,
		"ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_COUNTS_V3(", counts,
		ARRAY_SIZE(counts)) ||
	    counts[FIELD_DOMAIN_OCCURRENCES] !=
		ORLIX_TCTI_TARGET_REFRESH_FIELD_DOMAIN_OCCURRENCES ||
	    counts[FIELD_DOMAIN_GROUPS] !=
		ORLIX_TCTI_TARGET_REFRESH_FIELD_DOMAIN_GROUPS ||
	    counts[FIELD_DOMAIN_MAPPED] !=
		ORLIX_TCTI_TARGET_REFRESH_FIELD_DOMAIN_MAPPED ||
	    counts[FIELD_DOMAIN_AMBIGUOUS] !=
		ORLIX_TCTI_TARGET_REFRESH_FIELD_DOMAIN_AMBIGUOUS ||
	    counts[FIELD_DOMAIN_ALTERNATIVES] !=
		ORLIX_TCTI_TARGET_REFRESH_FIELD_DOMAIN_ALTERNATIVES)
		return -1;

	for (index = 0; index < ARRAY_SIZE(rows); index++) {
		size_t row_count = count_macro_rows(artifact, rows[index].token);

		if (row_count == SIZE_MAX ||
		    row_count != counts[rows[index].count_index] ||
		    count_token(artifact, rows[index].token) != row_count)
			return -1;
	}
	return 0;
}

static int applicability_c_string(const char **cursor, const char *end,
	int witness, int allow_empty, size_t *content_count)
{
	const char *position = *cursor;
	const char *content_start;
	size_t count = 0;

	if (position >= end || *position++ != '"')
		return -1;
	content_start = position;
	while (position < end && *position != '"') {
		if (*position != '\\') {
			if (witness)
				return -1;
			position++;
			continue;
		}
		if (!witness) {
			position++;
			if (position >= end)
				return -1;
			position++;
			continue;
		}
		if (end - position < 4 ||
		    !((position[1] == '0' && position[2] == '0' &&
		       position[3] == '1') ||
		      (position[1] == '3' && position[2] == '7' &&
		       position[3] == '7')))
			return -1;
		position += 4;
		count++;
	}
	if ((!allow_empty && position == content_start) ||
	    position >= end || *position++ != '"')
		return -1;
	*cursor = position;
	if (content_count)
		*content_count = witness ? count : (size_t)(position - content_start - 1U);
	return 0;
}

static int applicability_hex_string(const char **cursor, const char *end,
	size_t byte_count)
{
	const char *position = *cursor;
	size_t digits;
	size_t index;

	if (byte_count > SIZE_MAX / 2U)
		return -1;
	digits = byte_count * 2U;
	if (position >= end || *position++ != '"' ||
	    (size_t)(end - position) < digits + 1U)
		return -1;
	for (index = 0; index < digits; index++)
		if (!((position[index] >= '0' && position[index] <= '9') ||
		      (position[index] >= 'a' && position[index] <= 'f')))
			return -1;
	position += digits;
	if (*position++ != '"')
		return -1;
	*cursor = position;
	return 0;
}

static int applicability_literal(const char **cursor, const char *end,
				 const char *literal)
{
	size_t length = strlen(literal);

	if ((size_t)(end - *cursor) < length ||
	    memcmp(*cursor, literal, length))
		return -1;
	*cursor += length;
	return 0;
}

static int applicability_unsigned(const char **cursor, const char *end,
	size_t *value)
{
	const char *position = *cursor;
	size_t parsed = 0;

	if (position >= end || *position < '0' || *position > '9')
		return -1;
	while (position < end && *position >= '0' && *position <= '9') {
		if (parsed > (SIZE_MAX - (size_t)(*position - '0')) / 10U)
			return -1;
		parsed = parsed * 10U + (size_t)(*position++ - '0');
	}
	if (position >= end || *position++ != 'U')
		return -1;
	*cursor = position;
	*value = parsed;
	return 0;
}

static int applicability_u64_hex(const char **cursor, const char *end,
	uint64_t *value)
{
	const char *position = *cursor;
	uint64_t parsed = 0;
	size_t digits = 0;

	if ((size_t)(end - position) < 4U || position[0] != '0' ||
	    position[1] != 'x')
		return -1;
	position += 2;
	while (position < end) {
		unsigned int nibble;

		if (*position >= '0' && *position <= '9')
			nibble = (unsigned int)(*position - '0');
		else if (*position >= 'a' && *position <= 'f')
			nibble = (unsigned int)(*position - 'a' + 10);
		else
			break;
		if (digits == 16U)
			return -1;
		parsed = (parsed << 4) | nibble;
		digits++;
		position++;
	}
	if (!digits || (size_t)(end - position) < 3U ||
	    memcmp(position, "ULL", 3U))
		return -1;
	*cursor = position + 3U;
	*value = parsed;
	return 0;
}

static int applicability_integer_valid(size_t width, uint64_t low, uint64_t high)
{
	uint64_t high_mask;

	if (!width || width > 128U)
		return 0;
	if (width <= 64U)
		return !high && (width == 64U || !(low >> width));
	high_mask = width == 128U ? UINT64_MAX :
		(UINT64_C(1) << (width - 64U)) - 1U;
	return !(high & ~high_mask);
}

#if 0
static int validate_feature_applicability_artifact(
	const struct artifact_bytes *artifact)
{
	static const char row_prefix[] =
		"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_ROW(";
	static const char value_prefix[] =
		"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_VALUE(";
	static const char source[] =
		"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_SOURCE(\"vFAPA2-A\", "
		"\"2026-06_rel\", \"" ORLIX_TCTI_TARGET_REFRESH_INSTRUCTIONS_SHA256
		"\", \"" ORLIX_TCTI_TARGET_REFRESH_FEATURES_SHA256 "\", \""
		ORLIX_TCTI_TARGET_REFRESH_REGISTERS_SHA256 "\", \""
		ORLIX_TCTI_TARGET_REFRESH_RECONCILIATION_IDENTITY
		"\", 4350U, 409U)";
	const char *cursor;
	const char *end;
	size_t certificate_counts[4];
	size_t row_value_counts[4350];
	size_t common_kind[1024];
	size_t common_numeric[1024];
	size_t common_feature_node[1024];
	size_t common_identity_group[1024];
	size_t common_width[1024];
	size_t expected_first_value = 0;
	size_t ordinal;

	if (!artifact || !artifact->data || !artifact->length ||
	    memchr(artifact->data, '\0', artifact->length) ||
	    count_macro_rows(artifact,
		"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_SOURCE(") != 1U ||
	    count_macro_rows(artifact,
		"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_COUNTS(") != 1U ||
	    count_token(artifact, row_prefix) != 4350U ||
	    !has_exact_line(artifact, source) ||
	    parse_counts(artifact,
		"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_COUNTS(",
		certificate_counts, ARRAY_SIZE(certificate_counts)) ||
	    !certificate_counts[0] || !certificate_counts[1] ||
	    certificate_counts[0] > 1024U ||
	    certificate_counts[0] > certificate_counts[1] ||
	    certificate_counts[2] > certificate_counts[1] ||
	    !certificate_counts[3] ||
	    count_macro_rows(artifact, value_prefix) != certificate_counts[1])
		return -1;

	cursor = strstr(artifact->data, row_prefix);
	end = artifact->data + artifact->length;
	if (!cursor)
		return -1;
	for (ordinal = 0; ordinal < 4350U; ordinal++) {
		const char *line_end = memchr(cursor, '\n', (size_t)(end - cursor));
		size_t parsed_ordinal;
		size_t condition;
		size_t condition_length;
		size_t status;
		size_t witness_count;
		size_t declared_witness_count;
		size_t first_certificate_value;
		size_t certificate_value_count;
		uint64_t formula_identity;

		if (!line_end || applicability_literal(&cursor, line_end, row_prefix) ||
		    applicability_unsigned(&cursor, line_end, &parsed_ordinal) ||
		    parsed_ordinal != ordinal ||
		    applicability_literal(&cursor, line_end, ", ") ||
		    applicability_c_string(&cursor, line_end, 0, 0, NULL) ||
		    applicability_literal(&cursor, line_end, ", ") ||
		    applicability_c_string(&cursor, line_end, 0, 0, NULL) ||
		    applicability_literal(&cursor, line_end, ", ") ||
		    applicability_c_string(&cursor, line_end, 0, 0, NULL) ||
		    applicability_literal(&cursor, line_end, ", ") ||
		    applicability_unsigned(&cursor, line_end, &condition) ||
		    applicability_literal(&cursor, line_end, ", ") ||
		    applicability_unsigned(&cursor, line_end, &condition_length) ||
		    !condition_length ||
		    applicability_literal(&cursor, line_end, ", ") ||
		    applicability_unsigned(&cursor, line_end, &status) ||
		    applicability_literal(&cursor, line_end, ", ") ||
		    applicability_hex_string(&cursor, line_end, condition_length) ||
		    applicability_literal(&cursor, line_end, ", ") ||
		    applicability_c_string(&cursor, line_end, 1, 0, &witness_count) ||
		    applicability_literal(&cursor, line_end, ", ") ||
		    applicability_unsigned(&cursor, line_end,
			&declared_witness_count) ||
		    applicability_literal(&cursor, line_end, ", ") ||
		    applicability_unsigned(&cursor, line_end,
			&first_certificate_value) ||
		    first_certificate_value != expected_first_value ||
		    applicability_literal(&cursor, line_end, ", ") ||
		    applicability_unsigned(&cursor, line_end,
			&certificate_value_count) ||
		    certificate_value_count < certificate_counts[0] ||
		    certificate_value_count - certificate_counts[0] > 64U ||
		    first_certificate_value > certificate_counts[1] ||
		    certificate_value_count > certificate_counts[1] -
			first_certificate_value ||
		    applicability_literal(&cursor, line_end, ", ") ||
		    applicability_u64_hex(&cursor, line_end, &formula_identity) ||
		    !formula_identity ||
		    applicability_literal(&cursor, line_end, ")") ||
		    cursor != line_end || status != 1U || witness_count != 409U ||
		    declared_witness_count != witness_count)
			return -1;
		expected_first_value += certificate_value_count;
		row_value_counts[ordinal] = certificate_value_count;
		(void)condition;
		cursor = line_end + 1;
		if (ordinal + 1U < 4350U &&
		    ((size_t)(end - cursor) < sizeof(row_prefix) - 1U ||
		     memcmp(cursor, row_prefix, sizeof(row_prefix) - 1U)))
			return -1;
	}
	if (expected_first_value != certificate_counts[1])
		return -1;

	{
		size_t value_index;
		size_t row_index = 0;
		size_t row_first = 0;
		size_t operand_count = 0;
		size_t common_bytes = 0;
		const char *operand_names[64];
		size_t operand_name_lengths[64];

		for (value_index = 0; value_index < certificate_counts[1]; value_index++) {
			const char *line_end = memchr(cursor, '\n', (size_t)(end - cursor));
			const char *name_start;
			size_t parsed_index;
			size_t kind;
			size_t numeric;
			size_t feature_node;
			size_t identity_group;
			size_t leaf;
			size_t name_length;
			size_t width;
			size_t offset;
			size_t prior;
			uint64_t low;
			uint64_t high;

			while (row_index < 4350U &&
			       value_index >= row_first + row_value_counts[row_index]) {
				row_first += row_value_counts[row_index++];
			}
			if (row_index >= 4350U || !line_end ||
			    applicability_literal(&cursor, line_end, value_prefix) ||
			    applicability_unsigned(&cursor, line_end, &parsed_index) ||
			    parsed_index != value_index ||
			    applicability_literal(&cursor, line_end, ", ") ||
			    applicability_unsigned(&cursor, line_end, &kind) || kind > 2U ||
			    applicability_literal(&cursor, line_end, ", ") ||
			    applicability_unsigned(&cursor, line_end, &numeric) || numeric > 1U ||
			    applicability_literal(&cursor, line_end, ", ") ||
			    applicability_unsigned(&cursor, line_end, &feature_node) ||
			    applicability_literal(&cursor, line_end, ", ") ||
			    applicability_unsigned(&cursor, line_end, &identity_group) ||
			    applicability_literal(&cursor, line_end, ", ") ||
			    applicability_unsigned(&cursor, line_end, &leaf) ||
			    applicability_literal(&cursor, line_end, ", "))
				return -1;
			name_start = cursor + 1;
			if (applicability_c_string(&cursor, line_end, 0, 1, &name_length) ||
			    applicability_literal(&cursor, line_end, ", ") ||
			    applicability_unsigned(&cursor, line_end, &width) ||
			    applicability_literal(&cursor, line_end, ", ") ||
			    applicability_u64_hex(&cursor, line_end, &low) ||
			    applicability_literal(&cursor, line_end, ", ") ||
			    applicability_u64_hex(&cursor, line_end, &high) ||
			    applicability_literal(&cursor, line_end, ")") || cursor != line_end ||
			    !applicability_integer_valid(width, low, high))
				return -1;

			offset = value_index - row_first;
			if (offset < certificate_counts[0]) {
				if (kind == 0U) {
					if (feature_node >= 8955U || identity_group != UINT32_MAX ||
					    leaf != UINT32_MAX || name_length)
						return -1;
				} else if (kind == 1U) {
					if (feature_node != UINT32_MAX || identity_group >= 362U ||
					    leaf != UINT32_MAX || name_length)
						return -1;
				} else {
					return -1;
				}
				if (!row_index) {
					common_kind[offset] = kind;
					common_numeric[offset] = numeric;
					common_feature_node[offset] = feature_node;
					common_identity_group[offset] = identity_group;
					common_width[offset] = width;
					common_bytes += (width + 7U) / 8U;
				} else if (kind != common_kind[offset] ||
					   numeric != common_numeric[offset] ||
					   feature_node != common_feature_node[offset] ||
					   identity_group != common_identity_group[offset] ||
					   width != common_width[offset]) {
					return -1;
				}
			} else {
				size_t operand_index = offset - certificate_counts[0];

				if (kind != 2U || numeric != 0U ||
				    feature_node != UINT32_MAX || identity_group != UINT32_MAX ||
				    leaf != row_index || !name_length || width > 32U ||
				    operand_index >= 64U)
					return -1;
				for (prior = 0; prior < operand_index; prior++)
					if (operand_name_lengths[prior] == name_length &&
					    !memcmp(operand_names[prior], name_start, name_length))
						return -1;
				operand_names[operand_index] = name_start;
				operand_name_lengths[operand_index] = name_length;
				operand_count++;
			}
			cursor = line_end + 1;
		}
		if (operand_count != certificate_counts[2] ||
		    common_bytes != certificate_counts[3])
			return -1;
	}
	return cursor == end ? 0 : -1;
}

#endif

static int packed_hex_line(const char *cursor, const char *line_end,
	size_t expected_digits, int comma)
{
	size_t index;

	if ((size_t)(line_end - cursor) != expected_digits + 3U + (size_t)comma ||
	    cursor[0] != '\t' || cursor[1] != '"' ||
	    cursor[expected_digits + 2U] != '"' ||
	    (comma && cursor[expected_digits + 3U] != ','))
		return -1;
	for (index = 0; index < expected_digits; index++)
		if (!((cursor[index + 2U] >= '0' && cursor[index + 2U] <= '9') ||
		      (cursor[index + 2U] >= 'a' && cursor[index + 2U] <= 'f')))
			return -1;
	return 0;
}

static int validate_feature_applicability_artifact_packed(
	const struct artifact_bytes *artifact)
{
	static const char row_prefix[] =
		"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_ROW(";
	static const char symbol_prefix[] =
		"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_SYMBOL(";
	static const char common_prefix[] =
		"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_COMMON_VALUES(";
	static const char operand_prefix[] =
		"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_OPERAND_VALUE(";
	static const char source[] =
		"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_SOURCE(\"vFAPA2-A\", "
		"\"2026-06_rel\", \"" ORLIX_TCTI_TARGET_REFRESH_INSTRUCTIONS_SHA256
		"\", \"" ORLIX_TCTI_TARGET_REFRESH_FEATURES_SHA256 "\", \""
		ORLIX_TCTI_TARGET_REFRESH_REGISTERS_SHA256 "\", \""
		ORLIX_TCTI_TARGET_REFRESH_RECONCILIATION_IDENTITY
		"\", 4350U, 409U)";
	const char *cursor;
	const char *end;
	size_t counts[4];
	size_t operand_owners[ORLIX_TCTI_TARGET_REFRESH_OPERAND_CERTIFICATES];
	const char *operand_names[ORLIX_TCTI_TARGET_REFRESH_OPERAND_CERTIFICATES];
	size_t operand_name_lengths[
		ORLIX_TCTI_TARGET_REFRESH_OPERAND_CERTIFICATES];
	const char *boolean_names[ORLIX_TCTI_TARGET_REFRESH_BOOLEAN_IDENTIFIERS];
	size_t boolean_name_lengths[ORLIX_TCTI_TARGET_REFRESH_BOOLEAN_IDENTIFIERS];
	size_t boolean_count = 0;
	size_t expected_operand = 0;
	size_t ordinal;

	if (!artifact || !artifact->data || !artifact->length ||
	    memchr(artifact->data, '\0', artifact->length) ||
	    count_macro_rows(artifact,
		"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_SOURCE(") != 1U ||
	    count_macro_rows(artifact,
		"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_COUNTS(") != 1U ||
	    !has_exact_line(artifact, source) ||
	    parse_counts(artifact,
		"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_COUNTS(",
		counts, ARRAY_SIZE(counts)) || counts[0] != 377U ||
	 counts[1] != 4350U ||
	 counts[2] != ORLIX_TCTI_TARGET_REFRESH_OPERAND_CERTIFICATES ||
	 counts[3] != 6032U ||
	    count_macro_rows(artifact, row_prefix) != counts[1] ||
	    count_macro_rows(artifact, symbol_prefix) != counts[0] ||
	    count_token(artifact, common_prefix) != counts[1] ||
	    count_macro_rows(artifact, operand_prefix) != counts[2])
		return -1;
	end = artifact->data + artifact->length;
	cursor = strstr(artifact->data, row_prefix);
	if (!cursor)
		return -1;
	for (ordinal = 0; ordinal < counts[1]; ordinal++) {
		const char *line_end = memchr(cursor, '\n', (size_t)(end - cursor));
		size_t parsed, condition, condition_length, status, witness_count;
		size_t declared_witness, first_operand, operand_count;
		uint64_t formula;

		if (!line_end || applicability_literal(&cursor, line_end, row_prefix) ||
		    applicability_unsigned(&cursor, line_end, &parsed) || parsed != ordinal ||
		    applicability_literal(&cursor, line_end, ", ") ||
		    applicability_c_string(&cursor, line_end, 0, 0, NULL) ||
		    applicability_literal(&cursor, line_end, ", ") ||
		    applicability_c_string(&cursor, line_end, 0, 0, NULL) ||
		    applicability_literal(&cursor, line_end, ", ") ||
		    applicability_c_string(&cursor, line_end, 0, 0, NULL) ||
		    applicability_literal(&cursor, line_end, ", ") ||
		    applicability_unsigned(&cursor, line_end, &condition) ||
		    applicability_literal(&cursor, line_end, ", ") ||
		    applicability_unsigned(&cursor, line_end, &condition_length) ||
		    !condition_length || applicability_literal(&cursor, line_end, ", ") ||
		    applicability_unsigned(&cursor, line_end, &status) || status != 1U ||
		    applicability_literal(&cursor, line_end, ", ") ||
		    applicability_hex_string(&cursor, line_end, condition_length) ||
		    applicability_literal(&cursor, line_end, ", ") ||
		    applicability_c_string(&cursor, line_end, 1, 0, &witness_count) ||
		    applicability_literal(&cursor, line_end, ", ") ||
		    applicability_unsigned(&cursor, line_end, &declared_witness) ||
		    witness_count != 409U || declared_witness != witness_count ||
		    applicability_literal(&cursor, line_end, ", ") ||
		    applicability_unsigned(&cursor, line_end, &first_operand) ||
		    first_operand != expected_operand ||
		    applicability_literal(&cursor, line_end, ", ") ||
		    applicability_unsigned(&cursor, line_end, &operand_count) ||
		    first_operand > counts[2] || operand_count > 64U ||
		    operand_count > counts[2] - first_operand ||
		    applicability_literal(&cursor, line_end, ", ") ||
		    applicability_u64_hex(&cursor, line_end, &formula) || !formula ||
		    applicability_literal(&cursor, line_end, ")") || cursor != line_end)
			return -1;
		{
			size_t owner_index;

			for (owner_index = 0; owner_index < operand_count; owner_index++)
				operand_owners[first_operand + owner_index] = ordinal;
		}
		expected_operand += operand_count;
		(void)condition;
		cursor = line_end + 1U;
	}
	if (expected_operand != counts[2])
		return -1;
	for (ordinal = 0; ordinal < counts[0]; ordinal++) {
		const char *line_start = cursor;
		const char *line_end = memchr(cursor, '\n', (size_t)(end - cursor));
		size_t parsed, kind, numeric, node, group, name_length, width;

		if (!line_end || applicability_literal(&cursor, line_end, symbol_prefix) ||
		    applicability_unsigned(&cursor, line_end, &parsed) || parsed != ordinal ||
		    applicability_literal(&cursor, line_end, ", ") ||
		    applicability_unsigned(&cursor, line_end, &kind) ||
		    (kind != 0U && kind != 1U && kind != 3U) ||
		    applicability_literal(&cursor, line_end, ", ") ||
		    applicability_unsigned(&cursor, line_end, &numeric) || numeric > 1U ||
		    applicability_literal(&cursor, line_end, ", ") ||
		    applicability_unsigned(&cursor, line_end, &node) ||
		    applicability_literal(&cursor, line_end, ", ") ||
		    applicability_unsigned(&cursor, line_end, &group) ||
		    applicability_literal(&cursor, line_end, ", ") ||
		    applicability_c_string(&cursor, line_end, 0, 1, &name_length) ||
		    applicability_literal(&cursor, line_end, ", ") ||
		    applicability_unsigned(&cursor, line_end, &width) || !width || width > 128U ||
		    applicability_literal(&cursor, line_end, ")") || cursor != line_end ||
		    (kind == 0U && (node >= 8955U || group != UINT32_MAX || name_length)) ||
		    (kind == 1U &&
		     (node != UINT32_MAX || group >= 362U || name_length)) ||
		    (kind == 3U &&
		     (node >= 8955U || group != UINT32_MAX || numeric != 0U ||
		      width != 1U || !name_length || !memchr(line_start, '"',
			(size_t)(line_end - line_start)))))
			return -1;
		if (kind == 3U) {
			const char *name_quote = memchr(line_start, '"',
				(size_t)(line_end - line_start));
			size_t prior;

			if (!name_quote ||
			    boolean_count >= ORLIX_TCTI_TARGET_REFRESH_BOOLEAN_IDENTIFIERS)
				return -1;
			for (prior = 0; prior < boolean_count; prior++)
				if (boolean_name_lengths[prior] == name_length &&
				    !memcmp(boolean_names[prior], name_quote + 1U, name_length))
					return -1;
			boolean_names[boolean_count] = name_quote + 1U;
			boolean_name_lengths[boolean_count++] = name_length;
		}
		cursor = line_end + 1U;
	}
	if (boolean_count != ORLIX_TCTI_TARGET_REFRESH_BOOLEAN_IDENTIFIERS)
		return -1;
	for (ordinal = 0; ordinal < counts[1]; ordinal++) {
		const char *line_end = memchr(cursor, '\n', (size_t)(end - cursor));
		size_t parsed;
		size_t remaining = counts[0] * 32U;

		if (!line_end || applicability_literal(&cursor, line_end, common_prefix) ||
		    applicability_unsigned(&cursor, line_end, &parsed) || parsed != ordinal ||
		    applicability_literal(&cursor, line_end, ",") || cursor != line_end)
			return -1;
		cursor = line_end + 1U;
		while (remaining) {
			size_t digits = remaining > 512U ? 512U : remaining;
			line_end = memchr(cursor, '\n', (size_t)(end - cursor));
			if (!line_end || packed_hex_line(cursor, line_end, digits,
				remaining > digits))
				return -1;
			remaining -= digits;
			cursor = line_end + 1U;
		}
		if ((size_t)(end - cursor) < 2U || cursor[0] != ')' || cursor[1] != '\n')
			return -1;
		cursor += 2U;
	}
	for (ordinal = 0; ordinal < counts[2]; ordinal++) {
		const char *line_start = cursor;
		const char *line_end = memchr(cursor, '\n', (size_t)(end - cursor));
		const char *name_quote;
		size_t parsed, leaf, name_length, width;
		size_t prior;
		uint64_t low, high;

		if (!line_end || applicability_literal(&cursor, line_end, operand_prefix) ||
		    applicability_unsigned(&cursor, line_end, &parsed) || parsed != ordinal ||
		    applicability_literal(&cursor, line_end, ", ") ||
		    applicability_unsigned(&cursor, line_end, &leaf) || leaf >= 4350U ||
		    leaf != operand_owners[ordinal] ||
		    applicability_literal(&cursor, line_end, ", ") ||
		    applicability_c_string(&cursor, line_end, 0, 0, &name_length) ||
		    !name_length || applicability_literal(&cursor, line_end, ", ") ||
		    applicability_unsigned(&cursor, line_end, &width) || !width || width > 32U ||
		    applicability_literal(&cursor, line_end, ", ") ||
		    applicability_u64_hex(&cursor, line_end, &low) ||
		    applicability_literal(&cursor, line_end, ", ") ||
		    applicability_u64_hex(&cursor, line_end, &high) ||
		    !applicability_integer_valid(width, low, high) ||
		    applicability_literal(&cursor, line_end, ")") || cursor != line_end)
			return -1;
		name_quote = memchr(line_start, '"', (size_t)(line_end - line_start));
		if (!name_quote)
			return -1;
		operand_names[ordinal] = name_quote + 1U;
		operand_name_lengths[ordinal] = name_length;
		for (prior = 0; prior < ordinal; prior++)
			if (operand_owners[prior] == leaf &&
			    operand_name_lengths[prior] == name_length &&
			    !memcmp(operand_names[prior], operand_names[ordinal],
				    name_length))
				return -1;
		cursor = line_end + 1U;
	}
	return cursor == end ? 0 : -1;
}

static int validate_artifact_bundle(const struct artifact_bytes *manifest,
				    const struct artifact_bytes *asl_availability,
				    const struct artifact_bytes *instruction_artifact,
				    const struct artifact_bytes *feature_artifact,
				    const struct artifact_bytes *feature_applicability,
	const struct artifact_bytes *feature_field_domains,
	const struct artifact_bytes *runtime_capability_cohort,
	const struct artifact_bytes *active_execution_profile,
	const struct artifact_bytes *register_artifact,
	const struct artifact_bytes *system_accessors,
	const struct source_bytes *features,
	const struct source_bytes *profile,
	const struct source_bytes *promotion_manifest,
	const struct source_bytes *generator_schema,
	const struct source_bytes *proof,
	const struct source_bytes *generated_runtime_capability_cohort,
	const struct source_bytes *classification,
	const struct source_bytes *registry)
{
	size_t feature_counts[6], cohort_counts[4], profile_counts[3];
	size_t register_counts[24], accessor_counts[8], accessor_semantic_counts[9];
	size_t index;
	size_t accessor_outcomes = 0;

	/* Every instruction-derived artifact binds the same pinned source. */
	if (!has_token(manifest, "ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(") ||
	    !has_token(manifest, ORLIX_TCTI_TARGET_REFRESH_INSTRUCTIONS_SHA256) ||
	    count_token(manifest, "ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(") != 4350U ||
	    count_token(asl_availability,
			"ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE(") != 1U ||
	    !has_token(asl_availability,
		       ORLIX_TCTI_TARGET_REFRESH_INSTRUCTIONS_SHA256) ||
	    count_token(asl_availability,
			"ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW(") != 4332U ||
	    count_token(asl_availability,
			"ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW(") != 18U ||
	    count_token(asl_availability,
			"ORLIX_TCTI_A64_ASL_AVAILABILITY") != 0U ||
	    count_token(asl_availability, "ORLIX_TCTI_A64_ASL_BODY_") != 0U ||
	    count_token(asl_availability, "ORLIX_TCTI_A64_ASL_DECODE_") != 0U ||
	    count_token(asl_availability, "ORLIX_TCTI_A64_ASL_CORPUS_") != 0U ||
	    count_token(asl_availability, "ORLIX_TCTI_A64_ASL_HELPERS_") != 0U ||
	    !has_token(instruction_artifact,
		       "ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_SOURCE_SHA256 \"") ||
	    !has_token(instruction_artifact,
		       ORLIX_TCTI_TARGET_REFRESH_INSTRUCTIONS_SHA256) ||
	    !has_token(instruction_artifact,
		       "ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_LEAF_COUNT 4350U") ||
	    !has_token(instruction_artifact,
		       "orlix_tcti_a64_instruction_artifact_leaves[4350]") ||
	    !has_token(runtime_capability_cohort,
		       ORLIX_TCTI_TARGET_REFRESH_INSTRUCTIONS_SHA256))
		return -1;

	/* Feature identity is shared by its model, applicability, field bindings, and cohorts. */
	if (validate_feature_applicability_artifact_packed(feature_applicability))
		return -1;
	if (!has_token(feature_artifact,
		       "ORLIX_TCTI_A64_FEATURE_ARTIFACT_SOURCE(") ||
	    !has_token(feature_artifact, ORLIX_TCTI_TARGET_REFRESH_FEATURES_SHA256) ||
	    !has_token(feature_field_domains,
		       "ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_SOURCE(") ||
	    !has_token(feature_field_domains,
		       ORLIX_TCTI_TARGET_REFRESH_FEATURES_SHA256) ||
	    !has_token(runtime_capability_cohort,
		       "ORLIX_TCTI_A64_RUNTIME_CAPABILITY_COHORT_SOURCE(") ||
	    !has_token(runtime_capability_cohort,
		       ORLIX_TCTI_TARGET_REFRESH_FEATURES_SHA256))
		return -1;

	if (parse_counts(feature_artifact,
			 "ORLIX_TCTI_A64_FEATURE_ARTIFACT_COUNTS(",
			 feature_counts, ARRAY_SIZE(feature_counts)) ||
	    count_token(feature_artifact,
			"ORLIX_TCTI_A64_FEATURE_PARAMETER(") != feature_counts[0] ||
	    count_token(feature_artifact,
			"ORLIX_TCTI_A64_FEATURE_CONSTRAINT(") != feature_counts[1] ||
	    count_token(feature_artifact,
			"ORLIX_TCTI_A64_FEATURE_CHILD(") != feature_counts[4] ||
	    feature_counts[2] + feature_counts[5] != feature_counts[1])
		return -1;

	if (validate_feature_field_domain_artifact(feature_field_domains))
		return -1;

	if (parse_counts(runtime_capability_cohort,
			 "ORLIX_TCTI_A64_RUNTIME_CAPABILITY_COHORT_COUNTS(",
			 cohort_counts, ARRAY_SIZE(cohort_counts)) ||
	    cohort_counts[0] != 4350U || cohort_counts[1] != cohort_counts[2] ||
	    cohort_counts[1] != cohort_counts[3] ||
	    count_token(runtime_capability_cohort,
			"ORLIX_TCTI_A64_RUNTIME_CAPABILITY_COHORT_LEAF(") !=
		cohort_counts[0] ||
	    count_token(runtime_capability_cohort,
			"ORLIX_TCTI_A64_RUNTIME_CAPABILITY_COHORT_MEMBERSHIP(") !=
		cohort_counts[1])
		return -1;

	/* The active profile is a closed, generated policy table. HWCAP is not an
	 * input. Re-emit it here from the exact promotion, cohort, source-bound
	 * proof, classification, and #120-projected registry inputs before the
	 * bundle can publish. That admits a future ENABLED row only when every
	 * binding verifies, while leaving unproven rows fail-closed. */
	if (!has_token(active_execution_profile,
		       "ORLIX_TCTI_A64_ACTIVE_EXECUTION_PROFILE_SOURCE(") ||
	    parse_counts(active_execution_profile,
		"ORLIX_TCTI_A64_ACTIVE_EXECUTION_PROFILE_COUNTS(",
		profile_counts, ARRAY_SIZE(profile_counts)) ||
	    profile_counts[0] != profile_counts[1] + profile_counts[2] ||
	    count_token(active_execution_profile,
		"ORLIX_TCTI_A64_ACTIVE_EXECUTION_PROFILE_ENTRY(") != profile_counts[0] ||
	    count_token(active_execution_profile, ", ENABLED,") != profile_counts[1] ||
	    count_token(active_execution_profile, ", DISABLED,") != profile_counts[2])
		return -1;
	{
		struct artifact_bytes expected_profile = { 0 };
		int invalid = emit_active_execution_profile(features,
			feature_applicability, profile,
			promotion_manifest, generator_schema, proof,
			generated_runtime_capability_cohort,
			classification, registry, manifest, &expected_profile) ||
			expected_profile.length != active_execution_profile->length ||
			memcmp(expected_profile.data, active_execution_profile->data,
			       active_execution_profile->length);

		free(expected_profile.data);
		if (invalid)
			return -1;
	}

	/* Both register-derived artifacts bind one register source and reconcile
	 * every emitted system accessor into exactly one outcome bucket. */
	if (!has_token(register_artifact, "TREG_SRC(") ||
	    !has_token(register_artifact,
		       ORLIX_TCTI_TARGET_REFRESH_REGISTERS_SHA256) ||
	    parse_counts(register_artifact, "TREG_COUNTS(", register_counts,
			 ARRAY_SIZE(register_counts)) ||
	    count_token(register_artifact, "TREG_REG(") != register_counts[0] ||
	    !has_token(system_accessors,
		       "ORLIX_TCTI_A64_SYSTEM_ACCESSOR_SOURCE(") ||
	    !has_token(system_accessors,
		       ORLIX_TCTI_TARGET_REFRESH_REGISTERS_SHA256) ||
	    parse_counts(system_accessors,
			 "ORLIX_TCTI_A64_SYSTEM_ACCESSOR_COUNTS(",
			 accessor_counts, ARRAY_SIZE(accessor_counts)) ||
	    parse_counts(system_accessors,
			 "ORLIX_TCTI_A64_SYSTEM_ACCESSOR_SEMANTIC_COUNTS(",
			 accessor_semantic_counts,
			 ARRAY_SIZE(accessor_semantic_counts)) ||
	    accessor_semantic_counts[0] != accessor_counts[0] ||
	    accessor_semantic_counts[1] + accessor_semantic_counts[2] !=
		    accessor_counts[0] ||
	    accessor_semantic_counts[3] + accessor_semantic_counts[4] +
		    accessor_semantic_counts[5] != accessor_counts[0] ||
	    accessor_semantic_counts[6] + accessor_semantic_counts[7] !=
		    accessor_counts[0] ||
	    accessor_semantic_counts[8] != accessor_counts[0] ||
	    count_token(system_accessors,
			"ORLIX_TCTI_A64_SYSTEM_ACCESSOR(") != accessor_counts[0] ||
	    count_token(system_accessors,
			"ORLIX_TCTI_A64_SYSTEM_ACCESSOR_IDENTITY(") != 1U)
		return -1;
	for (index = 1; index < ARRAY_SIZE(accessor_counts); index++)
		accessor_outcomes += accessor_counts[index];
	return accessor_outcomes == accessor_counts[0] ? 0 : -1;
}

static void set_result(struct orlix_tcti_target_refresh_result *result,
			       enum orlix_tcti_target_refresh_error error)
{
	if (result)
		result->error = error;
}

const char *orlix_tcti_target_refresh_error_name(enum orlix_tcti_target_refresh_error error)
{
	switch (error) {
	case ORLIX_TCTI_TARGET_REFRESH_OK: return "success";
	case ORLIX_TCTI_TARGET_REFRESH_INVALID_ARGUMENT: return "invalid argument";
	case ORLIX_TCTI_TARGET_REFRESH_SOURCE_IO: return "source I/O failure";
	case ORLIX_TCTI_TARGET_REFRESH_SOURCE_LIMIT: return "source exceeds refresh limit";
	case ORLIX_TCTI_TARGET_REFRESH_SOURCE_IDENTITY: return "source SHA-256 does not match pinned Arm release";
	case ORLIX_TCTI_TARGET_REFRESH_ARM_XML_PACKAGE: return "official Arm A64 XML package validation failed";
	case ORLIX_TCTI_TARGET_REFRESH_PARSE: return "injected parse failure";
	case ORLIX_TCTI_TARGET_REFRESH_VALIDATION: return "injected cross-artifact validation failure";
	case ORLIX_TCTI_TARGET_REFRESH_MANIFEST: return "manifest generation failed";
	case ORLIX_TCTI_TARGET_REFRESH_SEMANTIC_PROVENANCE: return "external semantic provenance generation failed";
	case ORLIX_TCTI_TARGET_REFRESH_INSTRUCTIONS: return "instruction artifact generation failed";
	case ORLIX_TCTI_TARGET_REFRESH_FEATURES: return "feature artifact generation failed";
	case ORLIX_TCTI_TARGET_REFRESH_FEATURE_FIELD_DOMAINS:
		return "feature field-domain binding artifact generation failed";
	case ORLIX_TCTI_TARGET_REFRESH_FEATURE_APPLICABILITY:
		return "feature applicability artifact generation failed";
	case ORLIX_TCTI_TARGET_REFRESH_RUNTIME_CAPABILITY_COHORT:
		return "runtime capability cohort artifact generation failed";
	case ORLIX_TCTI_TARGET_REFRESH_ACTIVE_EXECUTION_PROFILE:
		return "active execution profile artifact generation failed";
	case ORLIX_TCTI_TARGET_REFRESH_REGISTERS: return "register artifact generation failed";
	case ORLIX_TCTI_TARGET_REFRESH_SYSTEM_ACCESSORS: return "system accessor reconciliation failed";
	case ORLIX_TCTI_TARGET_REFRESH_PUBLISH: return "transactional publication failed";
	}
	return "unknown refresh failure";
}

static int read_source(const char *path, struct source_bytes *source,
		       enum orlix_tcti_target_refresh_error *error)
{
	FILE *file;
	long length;
	size_t count;

	file = fopen(path, "rb");
	if (!file) {
		*error = ORLIX_TCTI_TARGET_REFRESH_SOURCE_IO;
		return -1;
	}
	if (fseek(file, 0, SEEK_END) || (length = ftell(file)) < 0) {
		*error = ORLIX_TCTI_TARGET_REFRESH_SOURCE_IO;
		fclose(file);
		return -1;
	}
	if ((uintmax_t)length > ORLIX_TCTI_TARGET_REFRESH_MAX_SOURCE) {
		*error = ORLIX_TCTI_TARGET_REFRESH_SOURCE_LIMIT;
		fclose(file);
		return -1;
	}
	if ((uintmax_t)length > SIZE_MAX - 1U || fseek(file, 0, SEEK_SET)) {
		*error = ORLIX_TCTI_TARGET_REFRESH_SOURCE_IO;
		fclose(file);
		return -1;
	}
	source->data = malloc((size_t)length + 1U);
	if (!source->data) {
		*error = ORLIX_TCTI_TARGET_REFRESH_SOURCE_IO;
		fclose(file);
		return -1;
	}
	count = fread(source->data, 1, (size_t)length, file);
	if (count != (size_t)length || fclose(file)) {
		free(source->data);
		*source = (struct source_bytes) { 0 };
		*error = ORLIX_TCTI_TARGET_REFRESH_SOURCE_IO;
		return -1;
	}
	source->data[length] = '\0';
	source->length = (size_t)length;
	return 0;
}

static int read_source_at(int root_fd, const char *path,
	struct source_bytes *source, enum orlix_tcti_target_refresh_error *error)
{
	char fd_path[64];
	int fd;

	fd = openat(root_fd, path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
	if (fd < 0 || snprintf(fd_path, sizeof(fd_path), "/dev/fd/%d", fd) >=
		(int)sizeof(fd_path)) {
		if (fd >= 0)
			close(fd);
		*error = ORLIX_TCTI_TARGET_REFRESH_SOURCE_IO;
		return -1;
	}
	if (read_source(fd_path, source, error)) {
		close(fd);
		return -1;
	}
	if (close(fd)) {
		*error = ORLIX_TCTI_TARGET_REFRESH_SOURCE_IO;
		return -1;
	}
	return 0;
}

static int capture(FILE *output, struct artifact_bytes *artifact)
{
	long length;

	if (fflush(output) || fseek(output, 0, SEEK_END) ||
	    (length = ftell(output)) < 0 || (uintmax_t)length > SIZE_MAX ||
	    fseek(output, 0, SEEK_SET))
		return -1;
	artifact->data = malloc((size_t)length + 1U);
	if (!artifact->data)
		return -1;
	if (fread(artifact->data, 1, (size_t)length, output) != (size_t)length) {
		free(artifact->data);
		*artifact = (struct artifact_bytes) { 0 };
		return -1;
	}
	artifact->data[length] = '\0';
	artifact->length = (size_t)length;
	return 0;
}

static int emit_manifest(const struct source_bytes *source,
			 struct artifact_bytes *artifact)
{
	FILE *output = tmpfile();
	int result;

	if (!output)
		return -1;
	result = target_manifest_generator_emit(source->data, source->length,
					output) == ORLIX_TCTI_TARGET_MANIFEST_GENERATOR_OK &&
		!capture(output, artifact) ? 0 : -1;
	fclose(output);
	return result;
}

static int emit_semantic_provenance(const struct source_bytes *source,
				    const struct orlix_tcti_arm_xml_package *package,
		struct artifact_bytes *artifact)
{
	FILE *output = tmpfile();
	int result;

	if (!output)
		return -1;
	result = orlix_tcti_target_semantic_provenance_emit(
			source->data, source->length, package, output) ==
		ORLIX_TCTI_TARGET_SEMANTIC_PROVENANCE_OK &&
		!capture(output, artifact) ? 0 : -1;
	fclose(output);
	return result;
}

static int emit_instructions(const struct source_bytes *source,
			     struct artifact_bytes *artifact)
{
	FILE *output = tmpfile();
	int result;

	if (!output)
		return -1;
	result = orlix_tcti_target_instruction_artifact_emit(source->data, source->length,
			output) == ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_OK &&
		!capture(output, artifact) ? 0 : -1;
	fclose(output);
	return result;
}

static int emit_features(const struct source_bytes *source,
			 struct artifact_bytes *artifact)
{
	FILE *output = tmpfile();
	int result;

	if (!output)
		return -1;
	result = orlix_tcti_target_feature_artifact_emit(source->data, source->length,
			output) == ORLIX_TCTI_FEATURE_ARTIFACT_OK &&
		!capture(output, artifact) ? 0 : -1;
	fclose(output);
	return result;
}

static int emit_registers(const struct source_bytes *source,
			  struct artifact_bytes *artifact)
{
	FILE *output = tmpfile();
	int result;

	if (!output)
		return -1;
	result = orlix_tcti_target_register_artifact_emit(source->data, source->length,
			output) == ORLIX_TCTI_REGISTER_ARTIFACT_OK &&
		!capture(output, artifact) ? 0 : -1;
	fclose(output);
	return result;
}

static int emit_feature_field_domains(const struct source_bytes *features,
				      const struct source_bytes *registers,
				      struct artifact_bytes *artifact)
{
	FILE *output = tmpfile();
	int result;

	if (!output)
		return -1;
	result = orlix_tcti_target_feature_field_domain_binding_artifact_emit(
		features->data, features->length, registers->data, registers->length,
		output) == ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_OK &&
		!capture(output, artifact) ? 0 : -1;
	fclose(output);
	return result;
}

static int emit_runtime_capability_cohort(const struct source_bytes *instructions,
	const struct source_bytes *features, struct artifact_bytes *artifact)
{
	FILE *output = tmpfile();
	int result;

	if (!output)
		return -1;
	result = orlix_tcti_runtime_capability_cohort_artifact_emit(
		instructions->data, instructions->length, features->data,
		features->length, output) ==
		ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_OK &&
		!capture(output, artifact) ? 0 : -1;
	fclose(output);
	return result;
}

static int emit_active_execution_profile(const struct source_bytes *features,
	const struct artifact_bytes *feature_applicability,
	const struct source_bytes *profile, const struct source_bytes *promotion_manifest,
	const struct source_bytes *generator_schema, const struct source_bytes *proof,
	const struct source_bytes *cohort, const struct source_bytes *classification,
	const struct source_bytes *registry, const struct artifact_bytes *source_manifest,
	struct artifact_bytes *artifact)
{
	FILE *output = tmpfile();
	int result;

	if (!output)
		return -1;
	result = orlix_tcti_active_execution_profile_artifact_emit(
		features->data, features->length, feature_applicability->data,
		feature_applicability->length, profile->data, profile->length,
		promotion_manifest->data, promotion_manifest->length,
		generator_schema->data, generator_schema->length, proof->data, proof->length,
		cohort->data, cohort->length,
		classification->data, classification->length, registry->data, registry->length,
		source_manifest->data, source_manifest->length,
		output) ==
		ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_OK &&
		!capture(output, artifact) ? 0 : -1;
	fclose(output);
	return result;
}

static int emit_feature_applicability(const struct source_bytes *instructions,
				      const struct source_bytes *features,
				      const struct source_bytes *registers,
				      struct artifact_bytes *artifact)
{
#ifdef ORLIX_TCTI_TARGET_REFRESH_TEST_CACHE
	static struct artifact_bytes cached;
#endif
	struct orlix_tcti_feature_model feature_model = { 0 };
	struct orlix_tcti_feature_error feature_error = { 0 };
	struct orlix_tcti_target_inventory inventory = { 0 };
	struct orlix_tcti_target_import_error inventory_error = { 0 };
	struct orlix_tcti_register_model register_model = { 0 };
	struct orlix_tcti_register_model_error register_error = { 0 };
	struct orlix_tcti_feature_field_domain_bindings field_domains = { 0 };
	struct orlix_tcti_feature_field_domain_binding_error field_error = { 0 };
	struct orlix_tcti_target_feature_sat_audit audit = { 0 };
	struct orlix_tcti_target_feature_sat_error sat_error = { 0 };
	FILE *output = NULL;
	int status = -1;

	if (!instructions || !features || !registers || !artifact)
		return -1;
#ifdef ORLIX_TCTI_TARGET_REFRESH_TEST_CACHE
	if (cached.data) {
		artifact->data = malloc(cached.length + 1U);
		if (!artifact->data)
			return -1;
		memcpy(artifact->data, cached.data, cached.length);
		artifact->data[cached.length] = '\0';
		artifact->length = cached.length;
		return 0;
	}
#endif
	if (orlix_tcti_target_feature_model_import(features->data, features->length,
						    &feature_model, &feature_error) ||
	    orlix_tcti_target_inventory_import(instructions->data,
					       instructions->length, &inventory,
					       &inventory_error) ||
	    orlix_tcti_register_model_import(registers->data, registers->length,
					     &register_model, &register_error) ||
	    orlix_tcti_target_feature_field_domain_bindings_build(
		&feature_model, &register_model, &field_domains, &field_error) ||
	    orlix_tcti_target_feature_sat_audit(
		&feature_model, &inventory,
		ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_BRANCH_LIMIT,
		ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_ALLOCATION_LIMIT,
		&field_domains, &audit, &sat_error))
		goto out;
	output = tmpfile();
	if (!output ||
	    orlix_tcti_target_feature_applicability_emit(
		&feature_model, &inventory, &audit, output) !=
		ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_OK ||
	    capture(output, artifact))
		goto out;
#ifdef ORLIX_TCTI_TARGET_REFRESH_TEST_CACHE
	cached.data = malloc(artifact->length + 1U);
	if (!cached.data)
		goto out;
	memcpy(cached.data, artifact->data, artifact->length);
	cached.data[artifact->length] = '\0';
	cached.length = artifact->length;
#endif
	status = 0;

out:
	if (output)
		fclose(output);
	orlix_tcti_target_feature_sat_audit_destroy(&audit);
	orlix_tcti_target_feature_field_domain_bindings_destroy(&field_domains);
	orlix_tcti_register_model_destroy(&register_model);
	orlix_tcti_target_inventory_destroy(&inventory);
	orlix_tcti_target_feature_model_destroy(&feature_model);
	return status;
}

static int emit_system_accessors(const struct source_bytes *source,
				 struct artifact_bytes *artifact)
{
	struct orlix_tcti_register_model model = { 0 };
	struct orlix_tcti_register_model_error import_error = { 0 };
	FILE *output = tmpfile();
	int result = -1;

	if (!output)
		return -1;
	if (!orlix_tcti_register_model_import(source->data, source->length, &model,
					&import_error) &&
	    orlix_tcti_system_accessor_reconciliation_emit(&model, output) ==
		ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_OK && !capture(output, artifact))
		result = 0;
	orlix_tcti_register_model_destroy(&model);
	fclose(output);
	return result;
}

int orlix_tcti_target_refresh_with_fault(
	int publish_root_fd, int source_tcti_root_fd, const char *instructions_path,
	const char *features_path, const char *registers_path,
	const char *arm_xml_archive_path, const char *arm_xml_release_path,
	const struct orlix_tcti_target_refresh_fault *fault,
	struct orlix_tcti_target_refresh_result *result)
{
	struct source_bytes instructions = { 0 }, features = { 0 }, registers = { 0 };
	struct source_bytes profile = { 0 }, promotion_manifest = { 0 };
	struct source_bytes generator_schema = { 0 }, proof = { 0 };
	struct source_bytes classification = { 0 }, registry = { 0 };
	struct source_bytes generated_runtime_capability_cohort = { 0 };
	struct artifact_bytes manifest = { 0 }, asl_availability = { 0 };
	struct artifact_bytes instruction_artifact = { 0 };
	struct artifact_bytes feature_artifact = { 0 }, register_artifact = { 0 };
	struct artifact_bytes feature_applicability = { 0 };
	struct artifact_bytes feature_field_domains = { 0 };
	struct artifact_bytes runtime_capability_cohort = { 0 };
	struct artifact_bytes active_execution_profile = { 0 };
	struct artifact_bytes system_accessors = { 0 };
	struct orlix_tcti_target_artifact artifacts[] = {
#define ORLIX_TCTI_TARGET_REFRESH_ARTIFACT(identifier, artifact_name, bytes) \
		{ .name = #artifact_name, .data = bytes.data, .length = bytes.length },
#include "target_refresh_artifacts.def"
#undef ORLIX_TCTI_TARGET_REFRESH_ARTIFACT
	};
	struct orlix_tcti_target_artifact_provenance provenance = {
		.schema = ORLIX_TCTI_TARGET_REFRESH_SCHEMA,
		.generator = ORLIX_TCTI_TARGET_REFRESH_GENERATOR,
		.source_architecture = ORLIX_TCTI_TARGET_REFRESH_ARCHITECTURE,
		.source_build = ORLIX_TCTI_TARGET_REFRESH_BUILD,
		.source_release = ORLIX_TCTI_TARGET_REFRESH_RELEASE,
		.source_schema = ORLIX_TCTI_TARGET_REFRESH_SOURCE_SCHEMA,
		.source_timestamp = ORLIX_TCTI_TARGET_REFRESH_TIMESTAMP,
	};
	struct orlix_tcti_target_artifact_publish_result publish_result = { 0 };
	struct orlix_tcti_arm_xml_package arm_xml_package = { 0 };
	enum orlix_tcti_arm_xml_package_error arm_xml_error;
	char instruction_digest[65];
	char feature_digest[65];
	char register_digest[65];
	char reconciliation_identity[65];
	enum orlix_tcti_target_refresh_error error = ORLIX_TCTI_TARGET_REFRESH_OK;
	size_t artifact_index;
	int source_isa_fd = -1;

	if (result)
		*result = (struct orlix_tcti_target_refresh_result) { 0 };
	if (publish_root_fd < 0 || source_tcti_root_fd < 0 || !instructions_path || !features_path ||
	    !registers_path || !arm_xml_archive_path || !arm_xml_release_path) {
		set_result(result, ORLIX_TCTI_TARGET_REFRESH_INVALID_ARGUMENT);
		errno = EINVAL;
		return -1;
	}
	/* Keep ISA and #120 proof-projection reads below one TCTI source descriptor.
	 * The publish root may be a hermetic test destination, but it must never
	 * select a second source tree or escape the ISA descriptor with "..". */
	source_isa_fd = openat(source_tcti_root_fd, "isa",
			       O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	if (source_isa_fd < 0) {
		set_result(result, ORLIX_TCTI_TARGET_REFRESH_INVALID_ARGUMENT);
		errno = EINVAL;
		return -1;
	}
	if (read_source(instructions_path, &instructions, &error) ||
	    read_source(features_path, &features, &error) ||
	    read_source(registers_path, &registers, &error))
		goto out;
	if (read_source_at(source_isa_fd, "runtime_profile.def", &profile, &error) ||
	    read_source_at(source_isa_fd, "active_execution_promotion_manifest.def", &promotion_manifest, &error) ||
	    read_source_at(source_isa_fd, "build-time/target_active_execution_profile_artifact_generator.c", &generator_schema, &error) ||
	    read_source_at(source_isa_fd, "source_bound_proof.def", &proof, &error) ||
	    read_source_at(source_isa_fd, "target_classification.def", &classification, &error) ||
	    read_source_at(source_isa_fd, "proof_registry_projection.def", &registry, &error))
		goto out;
	orlix_tcti_target_artifact_sha256(instructions.data, instructions.length,
					 instruction_digest);
	orlix_tcti_target_artifact_sha256(features.data, features.length,
					 feature_digest);
	orlix_tcti_target_artifact_sha256(registers.data, registers.length,
					 register_digest);
	if (strcmp(instruction_digest,
		   ORLIX_TCTI_TARGET_REFRESH_INSTRUCTIONS_SHA256) ||
	    strcmp(feature_digest, ORLIX_TCTI_TARGET_REFRESH_FEATURES_SHA256) ||
	    strcmp(register_digest, ORLIX_TCTI_TARGET_REFRESH_REGISTERS_SHA256)) {
		error = ORLIX_TCTI_TARGET_REFRESH_SOURCE_IDENTITY;
		errno = EINVAL;
		goto out;
	}
	provenance.instructions_sha256 = instruction_digest;
	provenance.instructions_byte_length = instructions.length;
	provenance.features_sha256 = feature_digest;
	provenance.features_byte_length = features.length;
	provenance.registers_sha256 = register_digest;
	provenance.registers_byte_length = registers.length;
	if (instructions.length != ORLIX_TCTI_TARGET_REFRESH_INSTRUCTIONS_BYTE_LENGTH ||
	    features.length != ORLIX_TCTI_TARGET_REFRESH_FEATURES_BYTE_LENGTH ||
	    registers.length != ORLIX_TCTI_TARGET_REFRESH_REGISTERS_BYTE_LENGTH ||
	    orlix_tcti_target_artifact_reconciliation_identity(
		&provenance, reconciliation_identity)) {
		error = ORLIX_TCTI_TARGET_REFRESH_SOURCE_IDENTITY;
		errno = EINVAL;
		goto out;
	}
	provenance.reconciliation_identity = reconciliation_identity;
	arm_xml_error = orlix_tcti_arm_xml_package_validate(
		arm_xml_archive_path, arm_xml_release_path, &arm_xml_package);
	if (result)
		result->arm_xml_error = arm_xml_error;
	if (arm_xml_error != ORLIX_TCTI_ARM_XML_PACKAGE_OK) {
		error = ORLIX_TCTI_TARGET_REFRESH_ARM_XML_PACKAGE;
		errno = EINVAL;
		goto out;
	}
	if (fault && fault->stage == ORLIX_TCTI_TARGET_REFRESH_FAULT_PARSE) {
		error = ORLIX_TCTI_TARGET_REFRESH_PARSE;
		errno = EIO;
		goto out;
	}

	if (emit_manifest(&instructions, &manifest)) {
		error = ORLIX_TCTI_TARGET_REFRESH_MANIFEST;
		goto out;
	}
	if (emit_semantic_provenance(&instructions, &arm_xml_package,
				  &asl_availability)) {
		error = ORLIX_TCTI_TARGET_REFRESH_SEMANTIC_PROVENANCE;
		goto out;
	}
	if (emit_instructions(&instructions, &instruction_artifact)) {
		error = ORLIX_TCTI_TARGET_REFRESH_INSTRUCTIONS;
		goto out;
	}
	if (emit_features(&features, &feature_artifact)) {
		error = ORLIX_TCTI_TARGET_REFRESH_FEATURES;
		goto out;
	}
	if (emit_feature_field_domains(&features, &registers,
				       &feature_field_domains)) {
		error = ORLIX_TCTI_TARGET_REFRESH_FEATURE_FIELD_DOMAINS;
		goto out;
	}
	if (emit_feature_applicability(&instructions, &features, &registers,
				       &feature_applicability)) {
		error = ORLIX_TCTI_TARGET_REFRESH_FEATURE_APPLICABILITY;
		goto out;
	}
	if (emit_runtime_capability_cohort(&instructions, &features,
					   &runtime_capability_cohort)) {
		error = ORLIX_TCTI_TARGET_REFRESH_RUNTIME_CAPABILITY_COHORT;
		goto out;
	}
	generated_runtime_capability_cohort.data = runtime_capability_cohort.data;
	generated_runtime_capability_cohort.length = runtime_capability_cohort.length;
	if (emit_active_execution_profile(&features, &feature_applicability, &profile,
						  &promotion_manifest,
					  &generator_schema, &proof,
					  &generated_runtime_capability_cohort,
					  &classification, &registry, &manifest,
					  &active_execution_profile)) {
		error = ORLIX_TCTI_TARGET_REFRESH_ACTIVE_EXECUTION_PROFILE;
		goto out;
	}
	if (emit_registers(&registers, &register_artifact)) {
		error = ORLIX_TCTI_TARGET_REFRESH_REGISTERS;
		goto out;
	}
	if (emit_system_accessors(&registers, &system_accessors)) {
		error = ORLIX_TCTI_TARGET_REFRESH_SYSTEM_ACCESSORS;
		goto out;
	}
	artifact_index = 0;
#define ORLIX_TCTI_TARGET_REFRESH_ARTIFACT(identifier, artifact_name, bytes) \
	do { \
		artifacts[artifact_index].data = bytes.data; \
		artifacts[artifact_index++].length = bytes.length; \
	} while (0);
#include "target_refresh_artifacts.def"
#undef ORLIX_TCTI_TARGET_REFRESH_ARTIFACT

	if (fault && fault->stage == ORLIX_TCTI_TARGET_REFRESH_FAULT_VALIDATION) {
		char *corruption;

		if (fault->validation_artifact >= ARRAY_SIZE(artifacts) ||
		    !artifacts[fault->validation_artifact].length) {
			error = ORLIX_TCTI_TARGET_REFRESH_INVALID_ARGUMENT;
			errno = EINVAL;
			goto out;
		}
		/* Deterministically corrupt one emitted artifact so tests exercise
		 * the real validator rather than bypassing it synthetically. The
		 * field-domain fixture preserves its V3 header and removes the rows. */
		corruption = (char *)artifacts[fault->validation_artifact].data;
		if (fault->validation_artifact == 4U) {
			char *occurrence = strstr(
				corruption,
				ORLIX_TCTI_TARGET_REFRESH_FIELD_DOMAIN_OCCURRENCE_V3);

			if (occurrence)
				corruption = occurrence;
		}
		*corruption = '\0';
	}
	if (validate_artifact_bundle(&manifest, &asl_availability,
				     &instruction_artifact, &feature_artifact,
				     &feature_applicability,
			     &feature_field_domains,
			     &runtime_capability_cohort,
			     &active_execution_profile,
			     &register_artifact, &system_accessors,
			     &features, &profile, &promotion_manifest,
			     &generator_schema, &proof,
			     &generated_runtime_capability_cohort, &classification,
			     &registry)) {
		error = ORLIX_TCTI_TARGET_REFRESH_VALIDATION;
		errno = EINVAL;
		goto out;
	}
	if (orlix_tcti_target_artifact_publish(
		    publish_root_fd, ORLIX_TCTI_TARGET_REFRESH_PUBLISH_NAME,
		    ORLIX_TCTI_TARGET_REFRESH_GENERATION_PREFIX, artifacts,
		    sizeof(artifacts) / sizeof(artifacts[0]), &provenance,
		    fault && fault->stage == ORLIX_TCTI_TARGET_REFRESH_FAULT_PUBLICATION ?
			    &fault->publication : NULL,
		    result ? &result->publish : &publish_result)) {
		error = ORLIX_TCTI_TARGET_REFRESH_PUBLISH;
		goto out;
	}

	/*
	 * The publisher validates existing generations before selection and
	 * returns immediately after its single atomic selector rename. Nothing
	 * after publication can manufacture a failure.
	 */
out:
	orlix_tcti_arm_xml_package_destroy(&arm_xml_package);
	if (source_isa_fd >= 0)
		close(source_isa_fd);
	free(instructions.data);
	free(features.data);
	free(registers.data);
	free(profile.data); free(promotion_manifest.data); free(generator_schema.data);
	free(proof.data); free(classification.data); free(registry.data);
	free(manifest.data);
	free(asl_availability.data);
	free(instruction_artifact.data);
	free(feature_artifact.data);
	free(feature_applicability.data);
	free(feature_field_domains.data);
	free(runtime_capability_cohort.data);
	free(active_execution_profile.data);
	free(register_artifact.data);
	free(system_accessors.data);
	set_result(result, error);
	return error == ORLIX_TCTI_TARGET_REFRESH_OK ? 0 : -1;
}

int orlix_tcti_target_refresh(
	int publish_root_fd, int source_tcti_root_fd, const char *instructions_path,
	const char *features_path, const char *registers_path,
	const char *arm_xml_archive_path, const char *arm_xml_release_path,
	struct orlix_tcti_target_refresh_result *result)
{
	return orlix_tcti_target_refresh_with_fault(
		publish_root_fd, source_tcti_root_fd, instructions_path, features_path, registers_path,
		arm_xml_archive_path, arm_xml_release_path, NULL, result);
}

#ifndef ORLIX_TCTI_TARGET_REFRESH_NO_MAIN
int main(int argc, char **argv)
{
	struct orlix_tcti_target_refresh_result result;
	int publish_fd;
	int source_fd;
	int status;

	if (argc != 7) {
		fprintf(stderr,
			"usage: %s TCTI_DIR Instructions.json Features.json Registers.json ISA_A64_xml_A_profile-2026-06.tar.gz ISA_A64_xml_A_profile-2026-06\n",
			argv[0]);
		return EXIT_FAILURE;
	}
	source_fd = open(argv[1],
			    O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	if (source_fd < 0) {
		fprintf(stderr, "%s: %s\n", argv[1], strerror(errno));
		return EXIT_FAILURE;
	}
	publish_fd = openat(source_fd, "isa",
			    O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	if (publish_fd < 0) {
		fprintf(stderr, "%s/isa: %s\n", argv[1], strerror(errno));
		close(source_fd);
		return EXIT_FAILURE;
	}
	status = orlix_tcti_target_refresh(publish_fd, source_fd, argv[2], argv[3],
				   argv[4], argv[5], argv[6], &result);
	close(publish_fd);
	close(source_fd);
	if (status) {
		fprintf(stderr, "OrlixTCTI ISA refresh: %s",
			orlix_tcti_target_refresh_error_name(result.error));
		if (result.arm_xml_error != ORLIX_TCTI_ARM_XML_PACKAGE_OK)
			fprintf(stderr, ": %s",
				orlix_tcti_arm_xml_package_error_name(
					result.arm_xml_error));
		if (result.publish.error != ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_OK)
			fprintf(stderr, ": %s",
				orlix_tcti_target_artifact_publish_error_name(
					result.publish.error));
		fputc('\n', stderr);
	}
	return status ? EXIT_FAILURE : EXIT_SUCCESS;
}
#endif
