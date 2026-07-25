// SPDX-License-Identifier: GPL-2.0-only
/*
 * Import the complete pinned Arm AARCHMRS A64 target without applying a
 * runtime HWCAP profile. The imported condition tree remains symbolic so a
 * later audit can model the union of applicable feature configurations.
 */
#include "target_inventory_import.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The pinned 2026-06 Instructions.json is 110.1 MiB. */
#define TCTI_TARGET_MAX_INPUT_BYTES (128U * 1024U * 1024U)
/* The pinned tree exceeds four million JSON tokens near EOF. */
#define TCTI_TARGET_MAX_TOKENS 5000000U
#define TCTI_TARGET_MAX_JSON_DEPTH 256U
#define TCTI_TARGET_MAX_AST_DEPTH 256U
#define TCTI_TARGET_MAX_TREE_DEPTH 256U
/* Tokens for the 110.1 MiB pinned tree require more than 128 MiB. */
#define TCTI_TARGET_MAX_ALLOCATION (256U * 1024U * 1024U)
/* A 32-bit encoding can contain no more than 32 non-overlapping fields. */
#define TCTI_TARGET_MAX_OPERANDS (TCTI_A64_TARGET_LEAF_COUNT * 32U)
#define TCTI_TARGET_MAX_OPERAND_NAME_BYTES (8U * 1024U * 1024U)

enum json_kind {
	JSON_OBJECT,
	JSON_ARRAY,
	JSON_STRING,
	JSON_PRIMITIVE,
};

struct json_token {
	enum json_kind kind;
	size_t start;
	size_t end;
	size_t next;
	size_t size;
};

struct parser {
	const char *data;
	size_t length;
	size_t position;
	struct json_token *tokens;
	size_t count;
	size_t capacity;
	size_t depth;
	struct tcti_target_import_error *error;
	size_t expression_depth;
};

struct importer {
	const char *json;
	const struct json_token *tokens;
	size_t token_count;
	size_t expression_depth;
	size_t tree_depth;
	struct tcti_target_inventory *inventory;
	struct tcti_target_import_error *error;
};

static const char source_architecture[] = "vFATAp1-A";
static const char source_build[] = "818";
static const char source_ref[] = "2026-06_rel";
static const char source_schema[] = "2.9.5";
static const char source_sha256[] =
	"a1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe";

static void set_error(struct tcti_target_import_error *error,
		      enum tcti_target_import_error_code code, size_t offset,
		      const char *format, ...)
{
	va_list arguments;

	if (!error || error->code != TCTI_TARGET_IMPORT_OK)
		return;
	error->code = code;
	error->offset = offset;
	va_start(arguments, format);
	vsnprintf(error->message, sizeof(error->message), format, arguments);
	va_end(arguments);
}

static int reserve(void **memory, size_t *capacity, size_t needed,
		   size_t element_size)
{
	size_t next = *capacity ? *capacity : 64;
	void *replacement;

	if (needed <= *capacity)
		return 0;
	while (next < needed) {
		if (next > SIZE_MAX / 2)
			return -1;
		next *= 2;
	}
	if (next > SIZE_MAX / element_size || next * element_size >
	    TCTI_TARGET_MAX_ALLOCATION)
		return -1;
	replacement = realloc(*memory, next * element_size);
	if (!replacement)
		return -1;
	*memory = replacement;
	*capacity = next;
	return 0;
}

static int parser_token(struct parser *parser, enum json_kind kind,
			size_t start)
{
	struct json_token *token;

	if (parser->count >= TCTI_TARGET_MAX_TOKENS) {
		set_error(parser->error, TCTI_TARGET_IMPORT_INPUT_LIMIT,
			  parser->position, "JSON token limit exceeded");
		return -1;
	}
	if (reserve((void **)&parser->tokens, &parser->capacity,
		    parser->count + 1, sizeof(*parser->tokens))) {
		set_error(parser->error, TCTI_TARGET_IMPORT_NO_MEMORY,
			  parser->position, "cannot allocate JSON tokens");
		return -1;
	}
	token = &parser->tokens[parser->count];
	*token = (struct json_token) {
		.kind = kind,
		.start = start,
	};
	return (int)parser->count++;
}

static void skip_space(struct parser *parser)
{
	while (parser->position < parser->length) {
		char byte = parser->data[parser->position];

		if (byte != ' ' && byte != '\t' && byte != '\r' && byte != '\n')
			break;
		parser->position++;
	}
}

static int parse_value(struct parser *parser);

static int parser_string_equal(const struct parser *parser, int left,
			       int right);

static int parse_string(struct parser *parser)
{
	size_t quote = parser->position++;
	int index = parser_token(parser, JSON_STRING, parser->position);

	if (index < 0)
		return -1;
	while (parser->position < parser->length) {
		unsigned char byte = parser->data[parser->position++];

		if (byte == '"') {
			parser->tokens[index].end = parser->position - 1;
			parser->tokens[index].next = parser->count;
			return index;
		}
		if (byte < 0x20) {
			set_error(parser->error, TCTI_TARGET_IMPORT_INVALID_JSON,
				  parser->position - 1,
				  "control byte in JSON string");
			return -1;
		}
		if (byte == '\\') {
			char escape;
			size_t i;

			if (parser->position >= parser->length)
				break;
			escape = parser->data[parser->position++];
			if (strchr("\"\\/bfnrt", escape))
				continue;
			if (escape != 'u' ||
			    parser->length - parser->position < 4) {
				set_error(parser->error,
					  TCTI_TARGET_IMPORT_INVALID_JSON,
					  parser->position - 1,
					  "invalid JSON string escape");
				return -1;
			}
			for (i = 0; i < 4; i++) {
				char digit = parser->data[parser->position++];

				if (!((digit >= '0' && digit <= '9') ||
				      (digit >= 'a' && digit <= 'f') ||
				      (digit >= 'A' && digit <= 'F'))) {
					set_error(parser->error,
						  TCTI_TARGET_IMPORT_INVALID_JSON,
						  parser->position - 1,
						  "invalid JSON unicode escape");
					return -1;
				}
			}
		}
	}
	set_error(parser->error, TCTI_TARGET_IMPORT_INVALID_JSON, quote,
		  "unterminated JSON string");
	return -1;
}

static bool primitive_delimiter(char byte)
{
	return byte == ' ' || byte == '\t' || byte == '\r' || byte == '\n' ||
	       byte == ',' || byte == ']' || byte == '}';
}

static int parse_primitive(struct parser *parser)
{
	size_t start = parser->position;
	int index;
	const char *text;
	size_t length;

	while (parser->position < parser->length &&
	       !primitive_delimiter(parser->data[parser->position]))
		parser->position++;
	if (parser->position == start) {
		set_error(parser->error, TCTI_TARGET_IMPORT_INVALID_JSON, start,
			  "empty JSON primitive");
		return -1;
	}
	text = parser->data + start;
	length = parser->position - start;
	if (!((length == 4 && !memcmp(text, "true", 4)) ||
	      (length == 5 && !memcmp(text, "false", 5)) ||
	      (length == 4 && !memcmp(text, "null", 4)))) {
		size_t i = 0;

		if (text[i] == '-')
			i++;
		if (i == length || (text[i] != '0' &&
			(text[i] < '1' || text[i] > '9')))
			goto invalid;
		if (text[i] == '0')
			i++;
		else
			while (i < length && text[i] >= '0' && text[i] <= '9')
				i++;
		if (i < length && text[i] == '.') {
			i++;
			if (i == length || text[i] < '0' || text[i] > '9')
				goto invalid;
			while (i < length && text[i] >= '0' && text[i] <= '9')
				i++;
		}
		if (i < length && (text[i] == 'e' || text[i] == 'E')) {
			i++;
			if (i < length && (text[i] == '+' || text[i] == '-'))
				i++;
			if (i == length || text[i] < '0' || text[i] > '9')
				goto invalid;
			while (i < length && text[i] >= '0' && text[i] <= '9')
				i++;
		}
		if (i != length)
			goto invalid;
	}
	index = parser_token(parser, JSON_PRIMITIVE, start);
	if (index < 0)
		return -1;
	parser->tokens[index].end = parser->position;
	parser->tokens[index].next = parser->count;
	return index;
invalid:
	set_error(parser->error, TCTI_TARGET_IMPORT_INVALID_JSON, start,
		  "invalid RFC 8259 JSON primitive");
	return -1;
}

static int parse_array(struct parser *parser)
{
	int index = parser_token(parser, JSON_ARRAY, parser->position++);

	if (index < 0)
		return -1;
	if (++parser->depth > TCTI_TARGET_MAX_JSON_DEPTH) {
		set_error(parser->error, TCTI_TARGET_IMPORT_DEPTH_LIMIT,
			  parser->position - 1, "JSON nesting depth exceeded");
		return -1;
	}
	skip_space(parser);
	if (parser->position < parser->length &&
	    parser->data[parser->position] == ']') {
		parser->position++;
		parser->depth--;
		parser->tokens[index].end = parser->position;
		parser->tokens[index].next = parser->count;
		return index;
	}
	for (;;) {
		if (parse_value(parser) < 0)
			return -1;
		parser->tokens[index].size++;
		skip_space(parser);
		if (parser->position >= parser->length)
			break;
		if (parser->data[parser->position] == ']') {
			parser->position++;
			parser->depth--;
			parser->tokens[index].end = parser->position;
			parser->tokens[index].next = parser->count;
			return index;
		}
		if (parser->data[parser->position++] != ',') {
			set_error(parser->error, TCTI_TARGET_IMPORT_INVALID_JSON,
				  parser->position - 1,
				  "expected comma in JSON array");
			goto fail;
		}
		skip_space(parser);
	}
	set_error(parser->error, TCTI_TARGET_IMPORT_INVALID_JSON,
		  parser->position, "unterminated JSON array");
	parser->depth--;
	return -1;
fail:
	parser->depth--;
	return -1;
}

static int parse_object(struct parser *parser)
{
	int index = parser_token(parser, JSON_OBJECT, parser->position++);

	if (index < 0)
		return -1;
	if (++parser->depth > TCTI_TARGET_MAX_JSON_DEPTH) {
		set_error(parser->error, TCTI_TARGET_IMPORT_DEPTH_LIMIT,
			  parser->position - 1, "JSON nesting depth exceeded");
		return -1;
	}
	skip_space(parser);
	if (parser->position < parser->length &&
	    parser->data[parser->position] == '}') {
		parser->position++;
		parser->depth--;
		parser->tokens[index].end = parser->position;
		parser->tokens[index].next = parser->count;
		return index;
	}
	for (;;) {
		if (parser->position >= parser->length ||
		    parser->data[parser->position] != '"' ||
		    parse_string(parser) < 0)
			break;
		skip_space(parser);
		if (parser->position >= parser->length ||
		    parser->data[parser->position++] != ':') {
			set_error(parser->error, TCTI_TARGET_IMPORT_INVALID_JSON,
				  parser->position, "expected colon in JSON object");
			return -1;
		}
		{
			int key = (int)parser->count - 1;
			size_t prior;

			for (prior = (size_t)index + 1; prior + 1 <
			     (size_t)key; prior = parser->tokens[prior + 1].next) {
				if (parser_string_equal(parser, (int)prior, key)) {
					set_error(parser->error,
						  TCTI_TARGET_IMPORT_DUPLICATE_KEY,
						  parser->tokens[key].start,
						  "duplicate JSON object key");
					goto fail;
				}
			}
		}
		if (parse_value(parser) < 0)
			goto fail;
		parser->tokens[index].size++;
		skip_space(parser);
		if (parser->position >= parser->length)
			break;
		if (parser->data[parser->position] == '}') {
			parser->position++;
			parser->depth--;
			parser->tokens[index].end = parser->position;
			parser->tokens[index].next = parser->count;
			return index;
		}
		if (parser->data[parser->position++] != ',') {
			set_error(parser->error, TCTI_TARGET_IMPORT_INVALID_JSON,
				  parser->position - 1,
				  "expected comma in JSON object");
			goto fail;
		}
		skip_space(parser);
	}
	if (!parser->error->code)
		set_error(parser->error, TCTI_TARGET_IMPORT_INVALID_JSON,
			  parser->position, "unterminated JSON object");
	parser->depth--;
	return -1;
fail:
	parser->depth--;
	return -1;
}

static int parse_value(struct parser *parser)
{
	skip_space(parser);
	if (parser->position >= parser->length) {
		set_error(parser->error, TCTI_TARGET_IMPORT_INVALID_JSON,
			  parser->position, "missing JSON value");
		return -1;
	}
	switch (parser->data[parser->position]) {
	case '{':
		return parse_object(parser);
	case '[':
		return parse_array(parser);
	case '"':
		return parse_string(parser);
	default:
		return parse_primitive(parser);
	}
}

static int hex_value(unsigned char byte)
{
	if (byte >= '0' && byte <= '9')
		return byte - '0';
	if (byte >= 'a' && byte <= 'f')
		return byte - 'a' + 10;
	return byte - 'A' + 10;
}

static char *decode_json_string(const char *json, const struct json_token *token)
{
	char *decoded;
	size_t input;
	size_t output = 0;

	if (token->kind != JSON_STRING || token->end - token->start >=
	    TCTI_TARGET_MAX_ALLOCATION)
		return NULL;
	decoded = malloc(token->end - token->start + 1);
	if (!decoded)
		return NULL;
	for (input = token->start; input < token->end; input++) {
		unsigned char byte = json[input];

		if (byte != '\\') {
			decoded[output++] = (char)byte;
			continue;
		}
		byte = (unsigned char)json[++input];
		if (strchr("\"\\/", byte))
			decoded[output++] = (char)byte;
		else if (byte == 'b')
			decoded[output++] = '\b';
		else if (byte == 'f')
			decoded[output++] = '\f';
		else if (byte == 'n')
			decoded[output++] = '\n';
		else if (byte == 'r')
			decoded[output++] = '\r';
		else if (byte == 't')
			decoded[output++] = '\t';
		else {
			unsigned int code = 0;
			size_t digit;

			if (byte != 'u' || token->end - input < 5)
				goto invalid;
			for (digit = 0; digit < 4; digit++)
				code = (code << 4) | hex_value((unsigned char)json[++input]);
			if (code >= 0xd800 && code <= 0xdbff) {
				unsigned int low = 0;

				if (token->end - input < 7 || json[++input] != '\\' ||
				    json[++input] != 'u')
					goto invalid;
				for (digit = 0; digit < 4; digit++)
					low = (low << 4) |
					      hex_value((unsigned char)json[++input]);
				if (low < 0xdc00 || low > 0xdfff)
					goto invalid;
				code = 0x10000 + ((code - 0xd800) << 10) +
				       (low - 0xdc00);
			} else if (code >= 0xdc00 && code <= 0xdfff) {
				goto invalid;
			}
			if (code < 0x80)
				decoded[output++] = (char)code;
			else if (code < 0x800) {
				decoded[output++] = (char)(0xc0 | (code >> 6));
				decoded[output++] = (char)(0x80 | (code & 0x3f));
			} else if (code < 0x10000) {
				decoded[output++] = (char)(0xe0 | (code >> 12));
				decoded[output++] = (char)(0x80 | ((code >> 6) & 0x3f));
				decoded[output++] = (char)(0x80 | (code & 0x3f));
			} else {
				decoded[output++] = (char)(0xf0 | (code >> 18));
				decoded[output++] = (char)(0x80 | ((code >> 12) & 0x3f));
				decoded[output++] = (char)(0x80 | ((code >> 6) & 0x3f));
				decoded[output++] = (char)(0x80 | (code & 0x3f));
			}
		}
	}
	decoded[output] = '\0';
	return decoded;
invalid:
	free(decoded);
	return NULL;
}

static int parser_string_equal(const struct parser *parser, int left, int right)
{
	char *first = decode_json_string(parser->data, &parser->tokens[left]);
	char *second = decode_json_string(parser->data, &parser->tokens[right]);
	int equal = first && second && !strcmp(first, second);

	free(first);
	free(second);
	return equal;
}

static bool token_equals(const char *json, const struct json_token *token,
			 const char *text)
{
	char *decoded;
	bool equal;

	if (token->kind != JSON_STRING)
		return false;
	decoded = decode_json_string(json, token);
	if (!decoded)
		return false;
	equal = !strcmp(decoded, text);
	free(decoded);
	return equal;
}

static int object_find(const struct importer *importer, int object_index,
		       const char *key)
{
	const struct json_token *object;
	size_t cursor;
	size_t member;

	if (object_index < 0 || (size_t)object_index >= importer->token_count)
		return -1;
	object = &importer->tokens[object_index];
	if (object->kind != JSON_OBJECT)
		return -1;
	cursor = (size_t)object_index + 1;
	for (member = 0; member < object->size; member++) {
		size_t value;

		if (cursor >= importer->token_count ||
		    importer->tokens[cursor].kind != JSON_STRING)
			return -1;
		value = importer->tokens[cursor].next;
		if (value >= importer->token_count)
			return -1;
		if (token_equals(importer->json, &importer->tokens[cursor], key))
			return (int)value;
		cursor = importer->tokens[value].next;
	}
	return -1;
}

static int array_element(const struct importer *importer, int array_index,
			 size_t element)
{
	const struct json_token *array;
	size_t cursor;
	size_t i;

	if (array_index < 0 || (size_t)array_index >= importer->token_count)
		return -1;
	array = &importer->tokens[array_index];
	if (array->kind != JSON_ARRAY || element >= array->size)
		return -1;
	cursor = (size_t)array_index + 1;
	for (i = 0; i < element; i++)
		cursor = importer->tokens[cursor].next;
	return cursor < importer->token_count ? (int)cursor : -1;
}

static char *copy_token(const struct importer *importer, int token_index)
{
	const struct json_token *token;

	if (token_index < 0 ||
	    (size_t)token_index >= importer->token_count)
		return NULL;
	token = &importer->tokens[token_index];
	if (token->kind != JSON_STRING)
		return NULL;
	return decode_json_string(importer->json, token);
}

static int add_expression(struct importer *importer,
			  struct tcti_target_expr expression, uint32_t *index)
{
	struct tcti_target_inventory *inventory = importer->inventory;

	if (inventory->expression_count >= UINT32_MAX ||
	    reserve((void **)&inventory->expressions,
		    &inventory->expression_capacity,
		    inventory->expression_count + 1,
		    sizeof(*inventory->expressions))) {
		free(expression.text);
		set_error(importer->error, TCTI_TARGET_IMPORT_NO_MEMORY, 0,
			  "cannot allocate condition expressions");
		return -1;
	}
	*index = (uint32_t)inventory->expression_count;
	inventory->expressions[inventory->expression_count++] = expression;
	return 0;
}

static int parse_expression(struct importer *importer, int object_index,
			    uint32_t *result);

static int parse_identifier(struct importer *importer, int object_index,
			    bool feature, uint32_t *result)
{
	int value = object_find(importer, object_index, "value");
	char *text = copy_token(importer, value);

	if (!text) {
		set_error(importer->error, TCTI_TARGET_IMPORT_INVALID_SOURCE,
			  importer->tokens[object_index].start,
			  "condition identifier lacks a string value");
		return -1;
	}
	if (feature && strncmp(text, "FEAT_", 5)) {
		set_error(importer->error,
			  TCTI_TARGET_IMPORT_UNSUPPORTED_GRAMMAR,
			  importer->tokens[value].start,
			  "feature name does not begin with FEAT_");
		free(text);
		return -1;
	}
	return add_expression(importer, (struct tcti_target_expr) {
		.kind = feature ? TCTI_TARGET_EXPR_FEATURE :
				 TCTI_TARGET_EXPR_OPERAND,
		.left = TCTI_TARGET_EXPR_NONE,
		.right = TCTI_TARGET_EXPR_NONE,
		.first_item = TCTI_TARGET_EXPR_NONE,
		.text = text,
	}, result);
}

static int parse_set(struct importer *importer, int object_index,
		     uint32_t *result)
{
	struct tcti_target_inventory *inventory = importer->inventory;
	int values = object_find(importer, object_index, "values");
	uint32_t first;
	size_t i;

	if (values < 0 || importer->tokens[values].kind != JSON_ARRAY ||
	    importer->tokens[values].size > UINT32_MAX) {
		set_error(importer->error, TCTI_TARGET_IMPORT_INVALID_SOURCE,
			  importer->tokens[object_index].start,
			  "condition set lacks an array of values");
		return -1;
	}
	if (inventory->set_item_count > UINT32_MAX -
		    importer->tokens[values].size ||
	    reserve((void **)&inventory->set_items,
		    &inventory->set_item_capacity,
		    inventory->set_item_count + importer->tokens[values].size,
		    sizeof(*inventory->set_items))) {
		set_error(importer->error, TCTI_TARGET_IMPORT_NO_MEMORY, 0,
			  "cannot allocate condition set");
		return -1;
	}
	first = (uint32_t)inventory->set_item_count;
	for (i = 0; i < importer->tokens[values].size; i++) {
		uint32_t item;

		if (parse_expression(importer,
				     array_element(importer, values, i), &item))
			return -1;
		inventory->set_items[inventory->set_item_count++] = item;
	}
	return add_expression(importer, (struct tcti_target_expr) {
		.kind = TCTI_TARGET_EXPR_SET,
		.left = TCTI_TARGET_EXPR_NONE,
		.right = TCTI_TARGET_EXPR_NONE,
		.first_item = first,
		.item_count = (uint32_t)importer->tokens[values].size,
	}, result);
}

static int parse_function(struct importer *importer, int object_index,
			  uint32_t *result)
{
	int name = object_find(importer, object_index, "name");
	int arguments = object_find(importer, object_index, "arguments");
	int argument;
	int type;

	if (name < 0 ||
	    !token_equals(importer->json, &importer->tokens[name],
			  "IsFeatureImplemented") ||
	    arguments < 0 ||
	    importer->tokens[arguments].kind != JSON_ARRAY ||
	    importer->tokens[arguments].size != 1) {
		set_error(importer->error,
			  TCTI_TARGET_IMPORT_UNSUPPORTED_GRAMMAR,
			  importer->tokens[object_index].start,
			  "unsupported condition function");
		return -1;
	}
	argument = array_element(importer, arguments, 0);
	type = object_find(importer, argument, "_type");
	if (type < 0 ||
	    !token_equals(importer->json, &importer->tokens[type],
			  "AST.Identifier")) {
		set_error(importer->error,
			  TCTI_TARGET_IMPORT_UNSUPPORTED_GRAMMAR,
			  importer->tokens[argument].start,
			  "feature argument is not an identifier");
		return -1;
	}
	return parse_identifier(importer, argument, true, result);
}

static int parse_expression_inner(struct importer *importer, int object_index,
				  uint32_t *result)
{
	int type;

	if (object_index < 0 ||
	    (size_t)object_index >= importer->token_count ||
	    importer->tokens[object_index].kind != JSON_OBJECT) {
		set_error(importer->error, TCTI_TARGET_IMPORT_INVALID_SOURCE,
			  object_index < 0 ? 0 :
			  importer->tokens[object_index].start,
			  "condition expression is not an object");
		return -1;
	}
	type = object_find(importer, object_index, "_type");
	if (type < 0) {
		set_error(importer->error, TCTI_TARGET_IMPORT_INVALID_SOURCE,
			  importer->tokens[object_index].start,
			  "condition expression lacks _type");
		return -1;
	}
	if (token_equals(importer->json, &importer->tokens[type],
			 "AST.Bool")) {
		int value = object_find(importer, object_index, "value");
		bool boolean;

		if (value < 0 ||
		    importer->tokens[value].kind != JSON_PRIMITIVE) {
			set_error(importer->error,
				  TCTI_TARGET_IMPORT_INVALID_SOURCE,
				  importer->tokens[object_index].start,
				  "boolean expression lacks a primitive value");
			return -1;
		}
		if (importer->tokens[value].end -
			    importer->tokens[value].start == 4 &&
		    !memcmp(importer->json + importer->tokens[value].start,
			    "true", 4))
			boolean = true;
		else if (importer->tokens[value].end -
				 importer->tokens[value].start == 5 &&
			 !memcmp(importer->json +
				 importer->tokens[value].start, "false", 5))
			boolean = false;
		else {
			set_error(importer->error,
				  TCTI_TARGET_IMPORT_INVALID_SOURCE,
				  importer->tokens[value].start,
				  "invalid boolean expression");
			return -1;
		}
		return add_expression(importer, (struct tcti_target_expr) {
			.kind = TCTI_TARGET_EXPR_BOOL,
			.left = TCTI_TARGET_EXPR_NONE,
			.right = TCTI_TARGET_EXPR_NONE,
			.first_item = TCTI_TARGET_EXPR_NONE,
			.boolean = boolean,
		}, result);
	}
	if (token_equals(importer->json, &importer->tokens[type],
			 "AST.Function"))
		return parse_function(importer, object_index, result);
	if (token_equals(importer->json, &importer->tokens[type],
			 "AST.Identifier"))
		return parse_identifier(importer, object_index, false, result);
	if (token_equals(importer->json, &importer->tokens[type],
			 "Values.Value")) {
		int value = object_find(importer, object_index, "value");
		char *text = copy_token(importer, value);

		if (!text) {
			set_error(importer->error,
				  TCTI_TARGET_IMPORT_INVALID_SOURCE,
				  importer->tokens[object_index].start,
				  "condition value lacks a string value");
			return -1;
		}
		return add_expression(importer, (struct tcti_target_expr) {
			.kind = TCTI_TARGET_EXPR_VALUE,
			.left = TCTI_TARGET_EXPR_NONE,
			.right = TCTI_TARGET_EXPR_NONE,
			.first_item = TCTI_TARGET_EXPR_NONE,
			.text = text,
		}, result);
	}
	if (token_equals(importer->json, &importer->tokens[type],
			 "AST.Set"))
		return parse_set(importer, object_index, result);
	if (token_equals(importer->json, &importer->tokens[type],
			 "AST.UnaryOp")) {
		int operation = object_find(importer, object_index, "op");
		int expression = object_find(importer, object_index, "expr");
		uint32_t child;

		if (operation < 0 ||
		    !token_equals(importer->json,
				  &importer->tokens[operation], "!") ||
		    parse_expression(importer, expression, &child)) {
			if (!importer->error->code)
				set_error(importer->error,
					  TCTI_TARGET_IMPORT_UNSUPPORTED_GRAMMAR,
					  importer->tokens[object_index].start,
					  "unsupported unary condition");
			return -1;
		}
		return add_expression(importer, (struct tcti_target_expr) {
			.kind = TCTI_TARGET_EXPR_NOT,
			.left = child,
			.right = TCTI_TARGET_EXPR_NONE,
			.first_item = TCTI_TARGET_EXPR_NONE,
		}, result);
	}
	if (token_equals(importer->json, &importer->tokens[type],
			 "AST.BinaryOp")) {
		int operation = object_find(importer, object_index, "op");
		int left_object = object_find(importer, object_index, "left");
		int right_object = object_find(importer, object_index, "right");
		enum tcti_target_expr_kind kind;
		uint32_t left;
		uint32_t right;

		if (operation < 0)
			goto unsupported_binary;
		if (token_equals(importer->json, &importer->tokens[operation],
				 "&&"))
			kind = TCTI_TARGET_EXPR_AND;
		else if (token_equals(importer->json,
				      &importer->tokens[operation], "||"))
			kind = TCTI_TARGET_EXPR_OR;
		else if (token_equals(importer->json,
				      &importer->tokens[operation], "=="))
			kind = TCTI_TARGET_EXPR_EQ;
		else if (token_equals(importer->json,
				      &importer->tokens[operation], "!="))
			kind = TCTI_TARGET_EXPR_NE;
		else if (token_equals(importer->json,
				      &importer->tokens[operation], "IN"))
			kind = TCTI_TARGET_EXPR_IN;
		else
			goto unsupported_binary;
		if (parse_expression(importer, left_object, &left) ||
		    parse_expression(importer, right_object, &right))
			return -1;
		return add_expression(importer, (struct tcti_target_expr) {
			.kind = kind,
			.left = left,
			.right = right,
			.first_item = TCTI_TARGET_EXPR_NONE,
		}, result);
unsupported_binary:
		set_error(importer->error,
			  TCTI_TARGET_IMPORT_UNSUPPORTED_GRAMMAR,
			  importer->tokens[object_index].start,
			  "unsupported binary condition operator");
		return -1;
	}
	set_error(importer->error, TCTI_TARGET_IMPORT_UNSUPPORTED_GRAMMAR,
		  importer->tokens[type].start,
		  "unsupported condition expression type");
	return -1;
}

static int parse_expression(struct importer *importer, int object_index,
			    uint32_t *result)
{
	int status;

	if (++importer->expression_depth > TCTI_TARGET_MAX_AST_DEPTH) {
		set_error(importer->error, TCTI_TARGET_IMPORT_DEPTH_LIMIT,
			  object_index < 0 ? 0 : importer->tokens[object_index].start,
			  "condition AST depth exceeded");
		importer->expression_depth--;
		return -1;
	}
	status = parse_expression_inner(importer, object_index, result);
	if (!status && *result < importer->inventory->expression_count) {
		const struct json_token *token = &importer->tokens[object_index];

		importer->inventory->expressions[*result].source_offset = token->start;
		importer->inventory->expressions[*result].source_length =
			token->end - token->start;
	}
	importer->expression_depth--;
	return status;
}

static int unsigned_primitive(const struct importer *importer, int token_index,
			      unsigned int *value)
{
	const struct json_token *token;
	unsigned long parsed = 0;
	size_t i;

	if (token_index < 0 ||
	    (size_t)token_index >= importer->token_count)
		return -1;
	token = &importer->tokens[token_index];
	if (token->kind != JSON_PRIMITIVE || token->start == token->end)
		return -1;
	for (i = token->start; i < token->end; i++) {
		char digit = importer->json[i];

		if (digit < '0' || digit > '9' ||
		    parsed > (UINT32_MAX - (unsigned int)(digit - '0')) / 10)
			return -1;
		parsed = parsed * 10 + (unsigned int)(digit - '0');
	}
	*value = (unsigned int)parsed;
	return 0;
}

static void discard_operands_from(struct tcti_target_inventory *inventory,
				  size_t first)
{
	while (inventory->operand_count > first) {
		struct tcti_target_operand *operand =
			&inventory->operands[--inventory->operand_count];

		inventory->operand_name_bytes -= strlen(operand->name) + 1;
		free(operand->name);
	}
}

static void discard_fixed_operands_from(struct tcti_target_inventory *inventory,
					size_t first)
{
	while (inventory->fixed_operand_count > first) {
		struct tcti_target_fixed_operand *operand =
			&inventory->fixed_operands[--inventory->fixed_operand_count];

		inventory->fixed_operand_name_bytes -= strlen(operand->name) + 1;
		free(operand->name);
	}
}

static int add_operand(struct importer *importer, int name_token,
		       uint32_t leaf_index, uint32_t condition,
		       unsigned int start, unsigned int width,
		       uint32_t variable_mask)
{
	struct tcti_target_inventory *inventory = importer->inventory;
	char *name;
	size_t name_bytes;
	size_t index;

	if (!variable_mask)
		return 0;
	name = copy_token(importer, name_token);
	if (!name || !name[0]) {
		free(name);
		set_error(importer->error, TCTI_TARGET_IMPORT_INVALID_SOURCE,
			  importer->tokens[name_token].start,
			  "variable A64 encoding field lacks a name");
		return -1;
	}
	name_bytes = strlen(name) + 1;
	if (name_bytes > TCTI_TARGET_MAX_OPERAND_NAME_BYTES ||
	    inventory->operand_name_bytes >
	    TCTI_TARGET_MAX_OPERAND_NAME_BYTES - name_bytes ||
	    inventory->operand_count >= TCTI_TARGET_MAX_OPERANDS) {
		free(name);
		set_error(importer->error, TCTI_TARGET_IMPORT_INPUT_LIMIT,
			  importer->tokens[name_token].start,
			  "A64 variable encoding field resource limit exceeded");
		return -1;
	}
	for (index = 0; index < inventory->operand_count; index++) {
		const struct tcti_target_operand *prior =
			&inventory->operands[index];

		if (prior->leaf_index == leaf_index && !strcmp(prior->name, name)) {
			free(name);
			set_error(importer->error, TCTI_TARGET_IMPORT_INVALID_SOURCE,
				  importer->tokens[name_token].start,
				  "duplicate variable A64 encoding field");
			return -1;
		}
	}
	if (reserve((void **)&inventory->operands, &inventory->operand_capacity,
		    inventory->operand_count + 1, sizeof(*inventory->operands))) {
		free(name);
		set_error(importer->error, TCTI_TARGET_IMPORT_NO_MEMORY, 0,
			  "cannot allocate variable A64 encoding fields");
		return -1;
	}
	inventory->operands[inventory->operand_count++] =
		(struct tcti_target_operand) {
			.name = name,
			.leaf_index = leaf_index,
			.condition = condition,
			.variable_mask = variable_mask,
			.start = (uint8_t)start,
			.width = (uint8_t)width,
		};
	inventory->operand_name_bytes += name_bytes;
	return 0;
}

static int add_fixed_operand(struct importer *importer, int name_token,
			     uint32_t leaf_index, uint32_t condition,
			     unsigned int start, unsigned int width,
			     uint32_t fixed_mask, uint32_t fixed_value,
			     size_t source_offset, size_t source_length)
{
	struct tcti_target_inventory *inventory = importer->inventory;
	char *name;
	size_t name_bytes;
	size_t index;

	name = copy_token(importer, name_token);
	if (!name || !name[0]) {
		free(name);
		set_error(importer->error, TCTI_TARGET_IMPORT_INVALID_SOURCE,
			  importer->tokens[name_token].start,
			  "fixed A64 encoding field lacks a name");
		return -1;
	}
	name_bytes = strlen(name) + 1;
	if (name_bytes > TCTI_TARGET_MAX_OPERAND_NAME_BYTES ||
	    inventory->fixed_operand_name_bytes >
	    TCTI_TARGET_MAX_OPERAND_NAME_BYTES - name_bytes ||
	    inventory->fixed_operand_count >= TCTI_TARGET_MAX_OPERANDS) {
		free(name);
		set_error(importer->error, TCTI_TARGET_IMPORT_INPUT_LIMIT,
			  importer->tokens[name_token].start,
			  "fixed A64 encoding field resource limit exceeded");
		return -1;
	}
	for (index = 0; index < inventory->fixed_operand_count; index++) {
		const struct tcti_target_fixed_operand *prior =
			&inventory->fixed_operands[index];

		if (prior->leaf_index == leaf_index && !strcmp(prior->name, name)) {
			free(name);
			set_error(importer->error, TCTI_TARGET_IMPORT_INVALID_SOURCE,
				  importer->tokens[name_token].start,
				  "duplicate fixed A64 encoding field");
			return -1;
		}
	}
	if (reserve((void **)&inventory->fixed_operands,
		    &inventory->fixed_operand_capacity,
		    inventory->fixed_operand_count + 1,
		    sizeof(*inventory->fixed_operands))) {
		free(name);
		set_error(importer->error, TCTI_TARGET_IMPORT_NO_MEMORY, 0,
			  "cannot allocate fixed A64 encoding fields");
		return -1;
	}
	inventory->fixed_operands[inventory->fixed_operand_count++] =
		(struct tcti_target_fixed_operand) {
			.name = name,
			.leaf_index = leaf_index,
			.condition = condition,
			.fixed_mask = fixed_mask,
			.fixed_value = fixed_value,
			.start = (uint8_t)start,
			.width = (uint8_t)width,
			.source_offset = source_offset,
			.source_length = source_length,
		};
	inventory->fixed_operand_name_bytes += name_bytes;
	return 0;
}

static int decode_encoding(struct importer *importer, int node_index,
			   uint32_t leaf_index, uint32_t condition,
			   uint32_t *mask, uint32_t *pattern)
{
	int encoding = object_find(importer, node_index, "encoding");
	int width = object_find(importer, encoding, "width");
	int values = object_find(importer, encoding, "values");
	unsigned int bits;
	size_t i;
	size_t first_operand = importer->inventory->operand_count;
	size_t first_fixed_operand = importer->inventory->fixed_operand_count;
	uint32_t occupied = 0;

	*mask = 0;
	*pattern = 0;
	if (unsigned_primitive(importer, width, &bits) || bits != 32 ||
	    values < 0 || importer->tokens[values].kind != JSON_ARRAY) {
		set_error(importer->error, TCTI_TARGET_IMPORT_INVALID_SOURCE,
			  importer->tokens[node_index].start,
			  "A64 leaf lacks a 32-bit encoding");
		return -1;
	}
	for (i = 0; i < importer->tokens[values].size; i++) {
		int field = array_element(importer, values, i);
		int type = object_find(importer, field, "_type");
		int name_token = object_find(importer, field, "name");
		int range = object_find(importer, field, "range");
		int start_token = object_find(importer, range, "start");
		int width_token = object_find(importer, range, "width");
		int value = object_find(importer, field, "value");
		int text_token = object_find(importer, value, "value");
		unsigned int start;
		unsigned int field_width;
		const struct json_token *text;
		uint32_t field_mask;
		uint32_t variable_mask = 0;
		size_t bit;

		if (unsigned_primitive(importer, start_token, &start) ||
		    unsigned_primitive(importer, width_token, &field_width) ||
		    !field_width || start >= 32 || field_width > 32 - start ||
		    text_token < 0 ||
		    importer->tokens[text_token].kind != JSON_STRING) {
			set_error(importer->error,
				  TCTI_TARGET_IMPORT_INVALID_SOURCE,
				  importer->tokens[field].start,
				  "invalid A64 encoding field");
			goto fail;
		}
		if (field_width == 32)
			field_mask = UINT32_MAX;
		else
			field_mask = ((1U << field_width) - 1U) << start;
		if (occupied & field_mask) {
			set_error(importer->error, TCTI_TARGET_IMPORT_INVALID_SOURCE,
				  importer->tokens[range].start,
				  "overlapping A64 encoding fields");
			goto fail;
		}
		occupied |= field_mask;
		text = &importer->tokens[text_token];
		if (text->end - text->start != field_width + 2 ||
		    importer->json[text->start] != '\'' ||
		    importer->json[text->end - 1] != '\'') {
			set_error(importer->error,
				  TCTI_TARGET_IMPORT_INVALID_SOURCE,
				  text->start, "invalid A64 encoding bits");
			goto fail;
		}
		for (bit = 0; bit < field_width; bit++) {
			char source_bit = importer->json[text->start + bit + 1];
			uint32_t target_bit =
				1U << (start + field_width - bit - 1);

			if (source_bit == '0' || source_bit == '1') {
				*mask |= target_bit;
				if (source_bit == '1')
					*pattern |= target_bit;
			} else if (source_bit != 'x') {
				set_error(importer->error,
					  TCTI_TARGET_IMPORT_INVALID_SOURCE,
					  text->start + bit + 1,
					  "unsupported A64 encoding bit");
				goto fail;
			} else {
				variable_mask |= target_bit;
			}
		}
		if (variable_mask) {
			if (type < 0 || name_token < 0 ||
			    !token_equals(importer->json, &importer->tokens[type],
					  "Instruction.Encodeset.Field") ||
			    importer->tokens[name_token].kind != JSON_STRING) {
				set_error(importer->error,
					  TCTI_TARGET_IMPORT_INVALID_SOURCE,
					  importer->tokens[field].start,
					  "variable A64 encoding bits require a named field");
				goto fail;
			}
			if (add_operand(importer, name_token, leaf_index, condition,
					start, field_width, variable_mask))
				goto fail;
		} else if (type >= 0 && name_token >= 0 &&
			   token_equals(importer->json, &importer->tokens[type],
					"Instruction.Encodeset.Field") &&
			   importer->tokens[name_token].kind == JSON_STRING &&
			   add_fixed_operand(importer, name_token, leaf_index, condition,
					     start, field_width, field_mask,
					     *pattern & field_mask,
					     importer->tokens[field].start,
					     importer->tokens[field].end -
					     importer->tokens[field].start)) {
			goto fail;
		}
	}
	return 0;
fail:
	discard_operands_from(importer->inventory, first_operand);
	discard_fixed_operands_from(importer->inventory, first_fixed_operand);
	return -1;
}

static int source_mnemonic(const struct importer *importer, int node_index)
{
	int assembly = object_find(importer, node_index, "assembly");
	int symbols = object_find(importer, assembly, "symbols");
	size_t i;

	if (symbols < 0 || importer->tokens[symbols].kind != JSON_ARRAY)
		return -1;
	for (i = 0; i < importer->tokens[symbols].size; i++) {
		int symbol = array_element(importer, symbols, i);
		int type = object_find(importer, symbol, "_type");

		if (type >= 0 &&
		    token_equals(importer->json, &importer->tokens[type],
				  "Instruction.Symbols.Literal"))
			return object_find(importer, symbol, "value");
	}
	return -1;
}

static int add_leaf(struct importer *importer, int node_index,
			    uint32_t condition)
{
	struct tcti_target_inventory *inventory = importer->inventory;
	struct tcti_target_leaf leaf = {0};
	int name = object_find(importer, node_index, "name");
	int mnemonic = source_mnemonic(importer, node_index);
	int operation = object_find(importer, node_index, "operation_id");
	int condition_object = object_find(importer, node_index, "condition");
	int preferred = object_find(importer, node_index, "preferred");
	size_t first_operand = inventory->operand_count;
	size_t first_fixed_operand = inventory->fixed_operand_count;
	size_t i;

	leaf.name = copy_token(importer, name);
	leaf.mnemonic = copy_token(importer, mnemonic);
	leaf.operation_id = copy_token(importer, operation);
	leaf.condition = condition;
	leaf.source_offset = importer->tokens[node_index].start;
	leaf.source_length = importer->tokens[node_index].end -
		leaf.source_offset;
	if (condition_object >= 0) {
		leaf.condition_source_offset = importer->tokens[condition_object].start;
		leaf.condition_source_length = importer->tokens[condition_object].end -
			leaf.condition_source_offset;
	}
	if (preferred >= 0) {
		leaf.preferred_source_offset = importer->tokens[preferred].start;
		leaf.preferred_source_length = importer->tokens[preferred].end -
			leaf.preferred_source_offset;
		if (importer->tokens[preferred].kind != JSON_PRIMITIVE) {
			leaf.preferred_present = true;
		} else if (leaf.preferred_source_length != 4 ||
			   memcmp(importer->json + leaf.preferred_source_offset,
				  "null", 4)) {
			set_error(importer->error, TCTI_TARGET_IMPORT_INVALID_SOURCE,
				  importer->tokens[preferred].start,
				  "A64 leaf preferred declaration is invalid");
			goto fail;
		}
	}
	if (!leaf.name || !leaf.mnemonic || !leaf.operation_id ||
	    !leaf.source_length || condition_object < 0 ||
	    !leaf.condition_source_length || preferred < 0 ||
	    !leaf.preferred_source_length) {
		set_error(importer->error, TCTI_TARGET_IMPORT_INVALID_SOURCE,
			  importer->tokens[node_index].start,
			  "A64 leaf lacks a complete identity");
		goto fail;
	}
	for (i = 0; i < inventory->leaf_count; i++) {
		if (!strcmp(inventory->leaves[i].name, leaf.name)) {
			set_error(importer->error,
				  TCTI_TARGET_IMPORT_INVALID_SOURCE,
				  importer->tokens[name].start,
				  "duplicate A64 leaf name: %s", leaf.name);
			goto fail;
		}
	}
	if (decode_encoding(importer, node_index,
			    (uint32_t)inventory->leaf_count, condition,
			    &leaf.encoding_mask, &leaf.encoding_pattern))
		goto fail;
	if (reserve((void **)&inventory->leaves, &inventory->leaf_capacity,
		    inventory->leaf_count + 1, sizeof(*inventory->leaves))) {
		set_error(importer->error, TCTI_TARGET_IMPORT_NO_MEMORY, 0,
			  "cannot allocate A64 target leaves");
		goto fail;
	}
	inventory->leaves[inventory->leaf_count++] = leaf;
	return 0;
fail:
	discard_operands_from(inventory, first_operand);
	discard_fixed_operands_from(inventory, first_fixed_operand);
	free(leaf.name);
	free(leaf.mnemonic);
	free(leaf.operation_id);
	return -1;
}

static int add_instruction_alias(struct importer *importer, int node_index,
				 uint32_t condition)
{
	struct tcti_target_inventory *inventory = importer->inventory;
	struct tcti_target_instruction_alias alias = {0};
	int name = object_find(importer, node_index, "name");
	int operation = object_find(importer, node_index, "operation_id");
	int condition_object = object_find(importer, node_index, "condition");
	int preferred = object_find(importer, node_index, "preferred");

	if (name < 0 || operation < 0 || condition_object < 0 || preferred < 0 ||
	    inventory->instruction_alias_count >= UINT32_MAX) {
		set_error(importer->error, TCTI_TARGET_IMPORT_INVALID_SOURCE,
			  importer->tokens[node_index].start,
			  "InstructionAlias lacks a complete identity or source condition");
		return -1;
	}
	alias.name = copy_token(importer, name);
	alias.operation_id = copy_token(importer, operation);
	alias.condition = condition;
	alias.ordinal = (uint32_t)inventory->instruction_alias_count;
	alias.source_offset = importer->tokens[node_index].start;
	alias.source_length = importer->tokens[node_index].end - alias.source_offset;
	alias.condition_source_offset = importer->tokens[condition_object].start;
	alias.condition_source_length = importer->tokens[condition_object].end -
		alias.condition_source_offset;
	alias.preferred_source_offset = importer->tokens[preferred].start;
	alias.preferred_source_length = importer->tokens[preferred].end -
		alias.preferred_source_offset;
	if (!alias.name || !alias.operation_id || !alias.source_length ||
	    !alias.condition_source_length || !alias.preferred_source_length)
		goto invalid;
	if (importer->tokens[preferred].kind == JSON_PRIMITIVE) {
		const struct json_token *token = &importer->tokens[preferred];

		if (token->end - token->start != 4 ||
		    memcmp(importer->json + token->start, "null", 4))
			goto invalid;
	} else
		alias.preferred_present = true;
	if (reserve((void **)&inventory->instruction_aliases,
		    &inventory->instruction_alias_capacity,
		    inventory->instruction_alias_count + 1,
		    sizeof(*inventory->instruction_aliases))) {
		set_error(importer->error, TCTI_TARGET_IMPORT_NO_MEMORY, 0,
			  "cannot allocate InstructionAlias provenance");
		goto fail;
	}
	inventory->instruction_aliases[inventory->instruction_alias_count++] = alias;
	return 0;
invalid:
	set_error(importer->error, TCTI_TARGET_IMPORT_INVALID_SOURCE,
		  importer->tokens[node_index].start,
		  "InstructionAlias has an invalid identity or preferred declaration");
fail:
	free(alias.name);
	free(alias.operation_id);
	return -1;
}

static int import_node(struct importer *importer, int node_index,
			       uint32_t parent_condition);

static int import_node_inner(struct importer *importer, int node_index,
				     uint32_t parent_condition)
{
	int condition_object = object_find(importer, node_index, "condition");
	int type = object_find(importer, node_index, "_type");
	uint32_t local;
	uint32_t combined;
	int children;
	size_t i;

	if (condition_object < 0 ||
	    parse_expression(importer, condition_object, &local))
		return -1;
	if (parent_condition == TCTI_TARGET_EXPR_NONE)
		combined = local;
	else if (add_expression(importer, (struct tcti_target_expr) {
			.kind = TCTI_TARGET_EXPR_AND,
			.left = parent_condition,
			.right = local,
			.first_item = TCTI_TARGET_EXPR_NONE,
		}, &combined))
		return -1;
	if (type < 0) {
		set_error(importer->error, TCTI_TARGET_IMPORT_INVALID_SOURCE,
			  importer->tokens[node_index].start,
			  "A64 tree node lacks _type");
		return -1;
	}
	if (token_equals(importer->json, &importer->tokens[type],
			 "Instruction.Instruction")) {
		children = object_find(importer, node_index, "children");
		if (add_leaf(importer, node_index, combined))
			return -1;
		if (children < 0)
			return 0;
		if (importer->tokens[children].kind != JSON_ARRAY) {
			set_error(importer->error, TCTI_TARGET_IMPORT_INVALID_SOURCE,
				  importer->tokens[children].start,
				  "A64 Instruction children is not an array");
			return -1;
		}
		for (i = 0; i < importer->tokens[children].size; i++)
			if (import_node(importer,
					array_element(importer, children, i), combined))
				return -1;
		return 0;
	}
	if (token_equals(importer->json, &importer->tokens[type],
			 "Instruction.InstructionAlias"))
		return add_instruction_alias(importer, node_index, combined);
	if (!token_equals(importer->json, &importer->tokens[type],
			  "Instruction.InstructionSet") &&
	    !token_equals(importer->json, &importer->tokens[type],
			  "Instruction.InstructionGroup")) {
		set_error(importer->error,
			  TCTI_TARGET_IMPORT_UNSUPPORTED_GRAMMAR,
			  importer->tokens[type].start,
			  "unsupported A64 tree node type");
		return -1;
	}
	children = object_find(importer, node_index, "children");
	if (children < 0 || importer->tokens[children].kind != JSON_ARRAY) {
		set_error(importer->error, TCTI_TARGET_IMPORT_INVALID_SOURCE,
			  importer->tokens[node_index].start,
			  "A64 container lacks children");
		return -1;
	}
	for (i = 0; i < importer->tokens[children].size; i++)
		if (import_node(importer, array_element(importer, children, i),
				combined))
			return -1;
	return 0;
}

static int import_node(struct importer *importer, int node_index,
			       uint32_t parent_condition)
{
	int status;

	if (++importer->tree_depth > TCTI_TARGET_MAX_TREE_DEPTH) {
		set_error(importer->error, TCTI_TARGET_IMPORT_DEPTH_LIMIT,
			  node_index < 0 ? 0 : importer->tokens[node_index].start,
			  "A64 instruction tree depth exceeded");
		importer->tree_depth--;
		return -1;
	}
	status = import_node_inner(importer, node_index, parent_condition);
	importer->tree_depth--;
	return status;
}

static int find_a64(const struct importer *importer, int root)
{
	int sets = object_find(importer, root, "instructions");
	size_t i;

	if (sets < 0 || importer->tokens[sets].kind != JSON_ARRAY)
		return -1;
	for (i = 0; i < importer->tokens[sets].size; i++) {
		int candidate = array_element(importer, sets, i);
		int name = object_find(importer, candidate, "name");

		if (name >= 0 &&
		    token_equals(importer->json, &importer->tokens[name], "A64"))
			return candidate;
	}
	return -1;
}

static int object_member(const struct importer *importer, int object_index,
			 size_t member, int *key, int *value)
{
	const struct json_token *object;
	size_t cursor;
	size_t index;

	if (object_index < 0 || (size_t)object_index >= importer->token_count ||
	    !key || !value)
		return -1;
	object = &importer->tokens[object_index];
	if (object->kind != JSON_OBJECT || member >= object->size)
		return -1;
	cursor = (size_t)object_index + 1;
	for (index = 0; index < member; index++) {
		if (cursor >= importer->token_count)
			return -1;
		cursor = importer->tokens[cursor].next;
		if (cursor >= importer->token_count)
			return -1;
		cursor = importer->tokens[cursor].next;
	}
	if (cursor >= importer->token_count ||
	    importer->tokens[cursor].kind != JSON_STRING ||
	    importer->tokens[cursor].next >= importer->token_count)
		return -1;
	*key = (int)cursor;
	*value = (int)importer->tokens[cursor].next;
	return 0;
}

static int object_find_member(const struct importer *importer, int object_index,
			      const char *name, int *key, int *value)
{
	const struct json_token *object;
	size_t index;

	if (object_index < 0 || (size_t)object_index >= importer->token_count ||
	    !name || !key || !value)
		return -1;
	object = &importer->tokens[object_index];
	if (object->kind != JSON_OBJECT)
		return -1;
	for (index = 0; index < object->size; index++) {
		int member_key;
		int member_value;

		if (object_member(importer, object_index, index, &member_key,
				  &member_value))
			return -1;
		if (token_equals(importer->json, &importer->tokens[member_key], name)) {
			*key = member_key;
			*value = member_value;
			return 0;
		}
	}
	return -1;
}

static int import_operation_semantic_member(struct importer *importer,
					    int object, const char *name,
					    bool semantic_body,
					    struct tcti_target_operation *operation)
{
	int key;
	int value;
	size_t member_offset;
	size_t member_length;
	size_t value_offset;
	size_t value_length;

	if (object_find_member(importer, object, name, &key, &value))
		return 0;
	member_offset = importer->tokens[key].start;
	member_length = importer->tokens[value].end - member_offset;
	value_offset = importer->tokens[value].start;
	value_length = importer->tokens[value].end - value_offset;
	if (!member_length || !value_length) {
		set_error(importer->error, TCTI_TARGET_IMPORT_INVALID_SOURCE,
			  importer->tokens[key].start,
			  "Instructions.json has an empty operation semantic member");
		return -1;
	}
	if (semantic_body) {
		if (importer->tokens[value].kind != JSON_STRING) {
			set_error(importer->error, TCTI_TARGET_IMPORT_INVALID_SOURCE,
				  importer->tokens[value].start,
				  "Instructions.json operation member is not a string");
			return -1;
		}
		operation->semantic_member_source_offset = member_offset;
		operation->semantic_member_source_length = member_length;
		operation->semantic_body_source_offset = value_offset;
		operation->semantic_body_source_length = value_length;
		operation->semantic_body_state =
			token_equals(importer->json, &importer->tokens[value],
				     "// Not specified") ?
			TCTI_TARGET_OPERATION_BODY_PLACEHOLDER :
			TCTI_TARGET_OPERATION_BODY_PRESENT;
		tcti_target_inventory_sha256(importer->json + value_offset,
					      value_length,
					      operation->semantic_body_sha256);
	} else {
		operation->decode_member_source_offset = member_offset;
		operation->decode_member_source_length = member_length;
		operation->decode_source_offset = value_offset;
		operation->decode_source_length = value_length;
		operation->decode_state =
			importer->tokens[value].kind == JSON_PRIMITIVE &&
			value_length == 4U &&
			!memcmp(importer->json + value_offset, "null", 4U) ?
			TCTI_TARGET_OPERATION_DECODE_NULL :
			TCTI_TARGET_OPERATION_DECODE_PRESENT;
		tcti_target_inventory_sha256(importer->json + value_offset,
					      value_length, operation->decode_sha256);
	}
	return 0;
}

static int import_operations(struct importer *importer, int root)
{
	int operations = object_find(importer, root, "operations");
	size_t index;

	if (operations < 0 || importer->tokens[operations].kind != JSON_OBJECT) {
		set_error(importer->error, TCTI_TARGET_IMPORT_INVALID_SOURCE, 0,
			"Instructions.json lacks its authoritative operations object");
		return -1;
	}
	for (index = 0; index < importer->tokens[operations].size; index++) {
		int key;
		int value;
		struct tcti_target_operation operation = { 0 };
		int type;

		if (object_member(importer, operations, index, &key, &value) ||
		    importer->tokens[value].kind != JSON_OBJECT ||
		    (type = object_find(importer, value, "_type")) < 0 ||
		    (!token_equals(importer->json, &importer->tokens[type],
				   "Instruction.Operation") &&
		     !token_equals(importer->json, &importer->tokens[type],
				   "Instruction.OperationAlias"))) {
			set_error(importer->error, TCTI_TARGET_IMPORT_INVALID_SOURCE,
				  key < 0 ? 0 : importer->tokens[key].start,
				  "Instructions.json has an invalid operation object");
			return -1;
		}
		operation.id = copy_token(importer, key);
		operation.is_alias = token_equals(importer->json,
			&importer->tokens[type], "Instruction.OperationAlias");
		if (operation.is_alias) {
			int target = object_find(importer, value, "operation_id");

			operation.alias_operation_id = copy_token(importer, target);
			if (!operation.alias_operation_id) {
				free(operation.id);
				set_error(importer->error,
					  TCTI_TARGET_IMPORT_INVALID_SOURCE,
					  importer->tokens[value].start,
					  "Instruction.OperationAlias lacks operation_id");
				return -1;
			}
		}
		operation.source_offset = importer->tokens[value].start;
		operation.source_length = importer->tokens[value].end -
			operation.source_offset;
		{
			int note = object_find(importer, value, "operational_note");

			if (note >= 0) {
				operation.operational_note_present = true;
				operation.operational_note_source_offset =
					importer->tokens[note].start;
				operation.operational_note_source_length =
					importer->tokens[note].end -
					operation.operational_note_source_offset;
				if (!operation.operational_note_source_length) {
					free(operation.id);
					set_error(importer->error,
						  TCTI_TARGET_IMPORT_INVALID_SOURCE,
						  importer->tokens[note].start,
						  "Instructions.json has an empty operational_note token");
					return -1;
				}
			}
		}
		if (import_operation_semantic_member(importer, value, "operation", true,
						     &operation) ||
		    import_operation_semantic_member(importer, value, "decode", false,
						     &operation)) {
			free(operation.id);
			free(operation.alias_operation_id);
			return -1;
		}
		if (!operation.id || !operation.source_length ||
		    tcti_target_inventory_operation(importer->inventory, operation.id)) {
			free(operation.id);
			free(operation.alias_operation_id);
			set_error(importer->error, TCTI_TARGET_IMPORT_INVALID_SOURCE,
				  importer->tokens[key].start,
				  "Instructions.json has a duplicate or empty operation");
			return -1;
		}
		if (reserve((void **)&importer->inventory->operations,
			    &importer->inventory->operation_capacity,
			    importer->inventory->operation_count + 1,
			    sizeof(*importer->inventory->operations))) {
			free(operation.id);
			free(operation.alias_operation_id);
			set_error(importer->error, TCTI_TARGET_IMPORT_NO_MEMORY, 0,
				  "cannot allocate operation provenance");
			return -1;
		}
		importer->inventory->operations[importer->inventory->operation_count++] =
			operation;
	}
	return 0;
}

static struct tcti_target_operation *mutable_operation(
	struct tcti_target_inventory *inventory, const char *id)
{
	size_t index;

	if (!id)
		return NULL;
	for (index = 0; index < inventory->operation_count; index++)
		if (!strcmp(inventory->operations[index].id, id))
			return &inventory->operations[index];
	return NULL;
}

const struct tcti_target_operation *
tcti_target_inventory_operation(const struct tcti_target_inventory *inventory,
					 const char *id)
{
	if (!inventory)
		return NULL;
	return mutable_operation((struct tcti_target_inventory *)inventory, id);
}

/*
 * Resolve a source operation through its alias chain. The imported operation
 * table is complete before this pass, so no forward declaration is treated as
 * absent. Bounded traversal rejects cycles and unresolved aliases explicitly.
 */
static int resolve_operation_alias(struct importer *importer, const char *id)
{
	struct tcti_target_inventory *inventory = importer->inventory;
	struct tcti_target_operation *operation;
	struct tcti_target_operation **path;
	size_t steps = 0;
	size_t index;
	const char *canonical;

	path = calloc(inventory->operation_count, sizeof(*path));
	if (!path) {
		set_error(importer->error, TCTI_TARGET_IMPORT_NO_MEMORY, 0,
			  "cannot allocate OperationAlias resolution path");
		return -1;
	}
	operation = mutable_operation(inventory, id);
	while (operation && operation->is_alias) {
		if (operation->canonical_operation_id) {
			canonical = operation->canonical_operation_id;
			goto resolve;
		}
		if (steps == inventory->operation_count) {
			set_error(importer->error, TCTI_TARGET_IMPORT_INVALID_SOURCE,
				  operation->source_offset,
				  "Instruction.OperationAlias cycle for %s", id);
			goto fail;
		}
		path[steps++] = operation;
		operation = mutable_operation(inventory, operation->alias_operation_id);
	}
	if (!operation) {
		set_error(importer->error, TCTI_TARGET_IMPORT_INVALID_SOURCE, 0,
			  "Instruction.OperationAlias target is absent for %s", id);
		goto fail;
	}
	canonical = operation->id;
resolve:
	for (index = 0; index < steps; index++) {
		path[index]->canonical_operation_id = strdup(canonical);
		if (!path[index]->canonical_operation_id) {
			set_error(importer->error, TCTI_TARGET_IMPORT_NO_MEMORY, 0,
				  "cannot retain canonical operation identity");
			while (index)
				free(path[--index]->canonical_operation_id),
				path[index]->canonical_operation_id = NULL;
			goto fail;
		}
		inventory->reachable_operation_alias_count++;
	}
	free(path);
	return 0;
fail:
	free(path);
	return -1;
}

static int resolve_reachable_operation_aliases(struct importer *importer)
{
	struct tcti_target_inventory *inventory = importer->inventory;
	size_t index;

	for (index = 0; index < inventory->leaf_count; index++) {
		struct tcti_target_operation *operation = mutable_operation(
			inventory, inventory->leaves[index].operation_id);

		if (!operation) {
			set_error(importer->error, TCTI_TARGET_IMPORT_INVALID_SOURCE, 0,
				  "A64 leaf references an operation absent from Instructions.json");
			return -1;
		}
		if (operation->is_alias && !operation->canonical_operation_id) {
			if (resolve_operation_alias(importer, operation->id))
				return -1;
		}
	}
	for (index = 0; index < inventory->instruction_alias_count; index++) {
		struct tcti_target_instruction_alias *alias =
			&inventory->instruction_aliases[index];
		struct tcti_target_operation *operation = mutable_operation(
			inventory, alias->operation_id);

		if (!operation) {
			set_error(importer->error, TCTI_TARGET_IMPORT_INVALID_SOURCE,
				  alias->source_offset,
				  "InstructionAlias references an operation absent from Instructions.json");
			return -1;
		}
		if (operation->is_alias) {
			if (!operation->canonical_operation_id) {
				if (resolve_operation_alias(importer, operation->id))
					return -1;
			}
			alias->canonical_operation_id = strdup(operation->canonical_operation_id);
		} else {
			alias->canonical_operation_id = strdup(operation->id);
		}
		if (!alias->canonical_operation_id) {
			set_error(importer->error, TCTI_TARGET_IMPORT_NO_MEMORY, 0,
				  "cannot retain InstructionAlias canonical operation identity");
			return -1;
		}
	}
	return 0;
}

static int validate_leaf_operations(struct importer *importer)
{
	return resolve_reachable_operation_aliases(importer);
}

static int validate_source_metadata(struct importer *importer, int root)
{
	int metadata = object_find(importer, root, "_meta");
	int version = object_find(importer, metadata, "version");
	int architecture = object_find(importer, version, "architecture");
	int build = object_find(importer, version, "build");
	int reference = object_find(importer, version, "ref");
	int schema = object_find(importer, version, "schema");
	int timestamp = object_find(importer, version, "timestamp");

	if (architecture < 0 || build < 0 || reference < 0 || schema < 0 ||
	    timestamp < 0 ||
	    !token_equals(importer->json, &importer->tokens[architecture],
			  source_architecture) ||
	    !token_equals(importer->json, &importer->tokens[build],
			  source_build) ||
	    !token_equals(importer->json, &importer->tokens[reference],
			  source_ref) ||
	    !token_equals(importer->json, &importer->tokens[schema],
			  source_schema) ||
	    !token_equals(importer->json, &importer->tokens[timestamp],
			  "2026-06-24 17:12:14")) {
		set_error(importer->error, TCTI_TARGET_IMPORT_INVALID_SOURCE, 0,
			  "Instructions.json metadata does not match the pinned "
			  "Arm AARCHMRS 2026-06 source");
		return -1;
	}
	return 0;
}

struct sha256_state {
	uint32_t state[8];
	uint64_t bytes;
	unsigned char block[64];
	size_t used;
};

static uint32_t rotate_right(uint32_t value, unsigned int amount)
{
	return (value >> amount) | (value << (32 - amount));
}

static void sha256_block(struct sha256_state *sha, const unsigned char *block)
{
	static const uint32_t round_constants[64] = {
		0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
		0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
		0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
		0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
		0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
		0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
		0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
		0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U,
	};
	uint32_t words[64];
	uint32_t a, b, c, d, e, f, g, h;
	size_t index;

	for (index = 0; index < 16; index++)
		words[index] = ((uint32_t)block[index * 4] << 24) |
			((uint32_t)block[index * 4 + 1] << 16) |
			((uint32_t)block[index * 4 + 2] << 8) | block[index * 4 + 3];
	for (; index < 64; index++)
		words[index] = words[index - 16] +
			(rotate_right(words[index - 15], 7) ^ rotate_right(words[index - 15], 18) ^ (words[index - 15] >> 3)) +
			words[index - 7] +
			(rotate_right(words[index - 2], 17) ^ rotate_right(words[index - 2], 19) ^ (words[index - 2] >> 10));
	a = sha->state[0]; b = sha->state[1]; c = sha->state[2]; d = sha->state[3];
	e = sha->state[4]; f = sha->state[5]; g = sha->state[6]; h = sha->state[7];
	for (index = 0; index < 64; index++) {
		uint32_t first = h + (rotate_right(e, 6) ^ rotate_right(e, 11) ^ rotate_right(e, 25)) +
			((e & f) ^ (~e & g)) + round_constants[index] + words[index];
		uint32_t second = (rotate_right(a, 2) ^ rotate_right(a, 13) ^ rotate_right(a, 22)) +
			((a & b) ^ (a & c) ^ (b & c));
		h = g; g = f; f = e; e = d + first; d = c; c = b; b = a; a = first + second;
	}
	sha->state[0] += a; sha->state[1] += b; sha->state[2] += c; sha->state[3] += d;
	sha->state[4] += e; sha->state[5] += f; sha->state[6] += g; sha->state[7] += h;
}

static void sha256_update(struct sha256_state *sha, const char *data, size_t length)
{
	sha->bytes += length;
	while (length) {
		size_t copied = 64 - sha->used;
		if (copied > length)
			copied = length;
		memcpy(sha->block + sha->used, data, copied);
		sha->used += copied; data += copied; length -= copied;
		if (sha->used == 64) {
			sha256_block(sha, sha->block);
			sha->used = 0;
		}
	}
}

void tcti_target_inventory_sha256(const void *data, size_t length,
					 char output[65])
{
	static const char hex[] = "0123456789abcdef";
	struct sha256_state sha = { .state = { 0x6a09e667U, 0xbb67ae85U,
		0x3c6ef372U, 0xa54ff53aU, 0x510e527fU, 0x9b05688cU,
		0x1f83d9abU, 0x5be0cd19U } };
	uint64_t bits;
	size_t index;

	sha256_update(&sha, data, length);
	bits = sha.bytes * 8;
	sha.block[sha.used++] = 0x80;
	if (sha.used > 56) {
		memset(sha.block + sha.used, 0, 64 - sha.used);
		sha256_block(&sha, sha.block);
		sha.used = 0;
	}
	memset(sha.block + sha.used, 0, 56 - sha.used);
	for (index = 0; index < 8; index++)
		sha.block[56 + index] = (unsigned char)(bits >> (56 - index * 8));
	sha256_block(&sha, sha.block);
	for (index = 0; index < 8; index++) {
		uint32_t word = sha.state[index];
		size_t byte;
		for (byte = 0; byte < 4; byte++) {
			unsigned char value = (unsigned char)(word >> (24 - byte * 8));
			output[(index * 4 + byte) * 2] = hex[value >> 4];
			output[(index * 4 + byte) * 2 + 1] = hex[value & 15];
		}
	}
	output[64] = '\0';
}

void tcti_target_inventory_destroy(struct tcti_target_inventory *inventory)
{
	size_t i;

	if (!inventory)
		return;
	for (i = 0; i < inventory->leaf_count; i++) {
		free(inventory->leaves[i].name);
		free(inventory->leaves[i].mnemonic);
		free(inventory->leaves[i].operation_id);
	}
	for (i = 0; i < inventory->expression_count; i++)
		free(inventory->expressions[i].text);
	for (i = 0; i < inventory->operand_count; i++)
		free(inventory->operands[i].name);
	for (i = 0; i < inventory->fixed_operand_count; i++)
		free(inventory->fixed_operands[i].name);
	free(inventory->leaves);
	free(inventory->expressions);
	free(inventory->set_items);
	free(inventory->operands);
	free(inventory->fixed_operands);
	for (i = 0; i < inventory->operation_count; i++) {
		free(inventory->operations[i].id);
		free(inventory->operations[i].alias_operation_id);
		free(inventory->operations[i].canonical_operation_id);
	}
	free(inventory->operations);
	for (i = 0; i < inventory->instruction_alias_count; i++) {
		free(inventory->instruction_aliases[i].name);
		free(inventory->instruction_aliases[i].operation_id);
		free(inventory->instruction_aliases[i].canonical_operation_id);
	}
	free(inventory->instruction_aliases);
	memset(inventory, 0, sizeof(*inventory));
}

int tcti_target_inventory_import(const char *json, size_t length,
				 struct tcti_target_inventory *inventory,
				 struct tcti_target_import_error *error)
{
	struct tcti_target_import_error local_error = {0};
	struct parser parser = {0};
	struct importer importer;
	int root;
	int a64;
	int status = -1;
	char digest[65];

	if (!error)
		error = &local_error;
	memset(error, 0, sizeof(*error));
	if (!json || !length || !inventory) {
		set_error(error, TCTI_TARGET_IMPORT_INVALID_ARGUMENT, 0,
			  "json bytes and inventory are required");
		return -1;
	}
	if (length > TCTI_TARGET_MAX_INPUT_BYTES) {
		set_error(error, TCTI_TARGET_IMPORT_INPUT_LIMIT, 0,
			  "Instructions.json exceeds the input limit");
		return -1;
	}
	memset(inventory, 0, sizeof(*inventory));
	parser.data = json;
	parser.length = length;
	parser.error = error;
	root = parse_value(&parser);
	skip_space(&parser);
	if (root < 0 || parser.position != parser.length) {
		if (!error->code)
			set_error(error, TCTI_TARGET_IMPORT_INVALID_JSON,
				  parser.position,
				  "trailing bytes after JSON root");
		goto out;
	}
	importer = (struct importer) {
		.json = json,
		.tokens = parser.tokens,
		.token_count = parser.count,
		.inventory = inventory,
		.error = error,
	};
	if (validate_source_metadata(&importer, root))
		goto out_inventory;
	a64 = find_a64(&importer, root);
	if (a64 < 0) {
		set_error(error, TCTI_TARGET_IMPORT_INVALID_SOURCE, 0,
			  "Instructions.json lacks the A64 instruction set");
		goto out_inventory;
	}
	if (import_node(&importer, a64, TCTI_TARGET_EXPR_NONE))
		goto out_inventory;
	if (inventory->leaf_count != TCTI_A64_TARGET_LEAF_COUNT) {
		set_error(error, TCTI_TARGET_IMPORT_COUNT_MISMATCH, 0,
			  "A64 source contains %zu leaves, expected %u",
			  inventory->leaf_count, TCTI_A64_TARGET_LEAF_COUNT);
		goto out_inventory;
	}
	if (import_operations(&importer, root) ||
	    validate_leaf_operations(&importer))
		goto out_inventory;
	tcti_target_inventory_sha256(json, length, digest);
	if (strcmp(digest, source_sha256)) {
		set_error(error, TCTI_TARGET_IMPORT_HASH_MISMATCH, 0,
			  "Instructions.json SHA-256 does not match the pinned source");
		goto out_inventory;
	}
	status = 0;
	goto out;

out_inventory:
	tcti_target_inventory_destroy(inventory);
out:
	free(parser.tokens);
	return status;
}
