/* SPDX-License-Identifier: GPL-2.0-only */
/* A bounded, symbolic importer for the pinned Arm Features.json. */
#include "target_feature_model.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INPUT (8U * 1024U * 1024U)
#define MAX_TOKENS 300000U
#define MAX_DEPTH 256U
#define PIN "633259000ffd3da32900bd0c0c1beae4a9eea7095c278f74d62a00c846b41187"

enum json_kind { JSON_OBJECT, JSON_ARRAY, JSON_STRING, JSON_PRIMITIVE };
struct token { enum json_kind kind; size_t start, end, next, size, text_len; char *text; };
struct parser { const char *s; size_t n, at, depth, count, capacity; struct token *tokens; struct tcti_feature_error *error; };
struct importer { const char *s; struct token *tokens; size_t count, depth; struct tcti_feature_model *model; struct tcti_feature_error *error; };

static void error(struct tcti_feature_error *e, enum tcti_feature_error_code code, size_t offset, const char *fmt, ...)
{
	va_list ap;
	if (!e || e->code)
		return;
	e->code = code;
	e->offset = offset;
	va_start(ap, fmt);
	vsnprintf(e->message, sizeof(e->message), fmt, ap);
	va_end(ap);
}

static int grow(void **p, size_t *capacity, size_t wanted, size_t element_size)
{
	size_t n = *capacity ? *capacity : 64;
	void *q;
	while (n < wanted) {
		if (n > SIZE_MAX / 2)
			return -1;
		n *= 2;
	}
	if (n > SIZE_MAX / element_size)
		return -1;
	q = realloc(*p, n * element_size);
	if (!q)
		return -1;
	*p = q;
	*capacity = n;
	return 0;
}

static int token_add(struct parser *p, enum json_kind kind, size_t start)
{
	if (p->count == MAX_TOKENS || grow((void **)&p->tokens, &p->capacity,
					     p->count + 1, sizeof(*p->tokens))) {
		error(p->error, TCTI_FEATURE_LIMIT, p->at, "JSON token limit");
		return -1;
	}
	p->tokens[p->count] = (struct token){ .kind = kind, .start = start };
	return (int)p->count++;
}

static void whitespace(struct parser *p)
{
	while (p->at < p->n && (p->s[p->at] == ' ' || p->s[p->at] == '\t' ||
				  p->s[p->at] == '\n' || p->s[p->at] == '\r'))
		p->at++;
}

static int hex(unsigned char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

static int append_utf8(char **text, size_t *length, size_t *capacity, uint32_t cp)
{
	unsigned char bytes[4];
	size_t n;
	if (cp <= 0x7f) bytes[0] = (unsigned char)cp, n = 1;
	else if (cp <= 0x7ff) bytes[0] = 0xc0 | (cp >> 6), bytes[1] = 0x80 | (cp & 63), n = 2;
	else if (cp <= 0xffff) bytes[0] = 0xe0 | (cp >> 12), bytes[1] = 0x80 | ((cp >> 6) & 63), bytes[2] = 0x80 | (cp & 63), n = 3;
	else if (cp <= 0x10ffff) bytes[0] = 0xf0 | (cp >> 18), bytes[1] = 0x80 | ((cp >> 12) & 63), bytes[2] = 0x80 | ((cp >> 6) & 63), bytes[3] = 0x80 | (cp & 63), n = 4;
	else return -1;
	if (grow((void **)text, capacity, *length + n + 1, 1)) return -1;
	memcpy(*text + *length, bytes, n);
	*length += n;
	(*text)[*length] = '\0';
	return 0;
}

static int raw_utf8(struct parser *p, uint32_t *cp)
{
	unsigned char a = (unsigned char)p->s[p->at++], b, c, d;
	if (a < 0x80) { *cp = a; return 0; }
	if (a < 0xc2 || a > 0xf4 || p->at >= p->n) return -1;
	b = (unsigned char)p->s[p->at++];
	if ((b & 0xc0) != 0x80) return -1;
	if (a < 0xe0) { *cp = ((a & 31) << 6) | (b & 63); return 0; }
	if (p->at >= p->n || (a == 0xe0 && b < 0xa0) || (a == 0xed && b > 0x9f)) return -1;
	c = (unsigned char)p->s[p->at++];
	if ((c & 0xc0) != 0x80) return -1;
	if (a < 0xf0) { *cp = ((a & 15) << 12) | ((b & 63) << 6) | (c & 63); return 0; }
	if (p->at >= p->n || (a == 0xf0 && b < 0x90) || (a == 0xf4 && b > 0x8f)) return -1;
	d = (unsigned char)p->s[p->at++];
	if ((d & 0xc0) != 0x80) return -1;
	*cp = ((a & 7) << 18) | ((b & 63) << 12) | ((c & 63) << 6) | (d & 63);
	return 0;
}

static int json_string(struct parser *p)
{
	size_t start = p->at++, length = 0, capacity = 0;
	char *text = NULL;
	int index = token_add(p, JSON_STRING, start);
	if (index < 0) return -1;
	while (p->at < p->n) {
		unsigned char c = (unsigned char)p->s[p->at];
		uint32_t cp;
		if (c == '"') {
			p->at++;
			if (grow((void **)&text, &capacity, length + 1, 1)) goto oom;
			text[length] = '\0';
			p->tokens[index].end = p->at;
			p->tokens[index].next = p->count;
			p->tokens[index].text = text;
			p->tokens[index].text_len = length;
			return index;
		}
		if (c < 0x20) goto invalid;
		if (c == '\\') {
			int h0, h1, h2, h3;
			p->at++;
			if (p->at == p->n) goto invalid;
			switch (p->s[p->at++]) {
			case '"': cp = '"'; break; case '\\': cp = '\\'; break; case '/': cp = '/'; break;
			case 'b': cp = '\b'; break; case 'f': cp = '\f'; break; case 'n': cp = '\n'; break;
			case 'r': cp = '\r'; break; case 't': cp = '\t'; break;
			case 'u':
				if (p->n - p->at < 4) goto invalid;
				h0 = hex((unsigned char)p->s[p->at]); h1 = hex((unsigned char)p->s[p->at + 1]);
				h2 = hex((unsigned char)p->s[p->at + 2]); h3 = hex((unsigned char)p->s[p->at + 3]);
				if (h0 < 0 || h1 < 0 || h2 < 0 || h3 < 0) goto invalid;
				p->at += 4; cp = (uint32_t)((h0 << 12) | (h1 << 8) | (h2 << 4) | h3);
				if (cp >= 0xd800 && cp <= 0xdbff) {
					uint32_t high = cp;
					if (p->n - p->at < 6 || p->s[p->at] != '\\' || p->s[p->at + 1] != 'u') goto invalid;
					h0 = hex((unsigned char)p->s[p->at + 2]); h1 = hex((unsigned char)p->s[p->at + 3]);
					h2 = hex((unsigned char)p->s[p->at + 4]); h3 = hex((unsigned char)p->s[p->at + 5]);
					if (h0 < 0 || h1 < 0 || h2 < 0 || h3 < 0) goto invalid;
					cp = (uint32_t)((h0 << 12) | (h1 << 8) | (h2 << 4) | h3);
					if (cp < 0xdc00 || cp > 0xdfff) goto invalid;
					p->at += 6;
					cp = 0x10000 + ((high - 0xd800) << 10) +
						((uint32_t)((h0 << 12) | (h1 << 8) | (h2 << 4) | h3) - 0xdc00);
				} else if (cp >= 0xdc00 && cp <= 0xdfff) goto invalid;
				break;
			default: goto invalid;
			}
		} else if (raw_utf8(p, &cp)) goto invalid;
		else if (c == 0) goto invalid;
		if (append_utf8(&text, &length, &capacity, cp)) goto oom;
	}
invalid:
	free(text); error(p->error, TCTI_FEATURE_INVALID_JSON, p->at, "invalid JSON string"); return -1;
oom:
	free(text); error(p->error, TCTI_FEATURE_NO_MEMORY, p->at, "JSON string"); return -1;
}

static int json_value(struct parser *p);
static int duplicate_key(struct parser *p, int object, int key)
{
	size_t i, pairs = p->tokens[object].size;
	int old = object + 1;
	for (i = 0; i < pairs; i++) {
		if (p->tokens[old].text_len == p->tokens[key].text_len &&
			!memcmp(p->tokens[old].text, p->tokens[key].text, p->tokens[key].text_len))
			return 1;
		old = (int)p->tokens[p->tokens[old].next].next;
	}
	return 0;
}

static int json_container(struct parser *p, enum json_kind kind, char close)
{
	int object = token_add(p, kind, p->at++);
	if (object < 0) return -1;
	whitespace(p);
	if (p->at < p->n && p->s[p->at] == close) goto done;
	for (;;) {
		int key, value;
		if (kind == JSON_OBJECT) {
			if (p->at == p->n || p->s[p->at] != '"') goto invalid;
			key = json_string(p);
			if (key < 0) return -1;
			if (duplicate_key(p, object, key)) { error(p->error, TCTI_FEATURE_DUPLICATE_KEY, p->tokens[key].start, "duplicate JSON key"); return -1; }
			whitespace(p);
			if (p->at == p->n || p->s[p->at++] != ':') goto invalid;
			if (json_value(p) < 0) return -1;
		} else {
			value = json_value(p);
			if (value < 0) return -1;
		}
		p->tokens[object].size++;
		whitespace(p);
		if (p->at < p->n && p->s[p->at] == close) break;
		if (p->at == p->n || p->s[p->at++] != ',') goto invalid;
		whitespace(p);
	}
done:
	p->at++;
	p->tokens[object].end = p->at;
	p->tokens[object].next = p->count;
	return object;
invalid:
	error(p->error, TCTI_FEATURE_INVALID_JSON, p->at, "malformed JSON container");
	return -1;
}

static int json_primitive(struct parser *p)
{
	size_t start = p->at;
	int index;
	if (p->n - p->at >= 4 && !memcmp(p->s + p->at, "true", 4)) p->at += 4;
	else if (p->n - p->at >= 5 && !memcmp(p->s + p->at, "false", 5)) p->at += 5;
	else if (p->n - p->at >= 4 && !memcmp(p->s + p->at, "null", 4)) p->at += 4;
	else {
		if (p->s[p->at] == '-') p->at++;
		if (p->at == p->n) goto invalid;
		if (p->s[p->at] == '0') p->at++;
		else if (p->s[p->at] >= '1' && p->s[p->at] <= '9') while (p->at < p->n && isdigit((unsigned char)p->s[p->at])) p->at++;
		else goto invalid;
		if (p->at < p->n && p->s[p->at] == '.') { p->at++; if (p->at == p->n || !isdigit((unsigned char)p->s[p->at])) goto invalid; while (p->at < p->n && isdigit((unsigned char)p->s[p->at])) p->at++; }
		if (p->at < p->n && (p->s[p->at] == 'e' || p->s[p->at] == 'E')) { p->at++; if (p->at < p->n && (p->s[p->at] == '+' || p->s[p->at] == '-')) p->at++; if (p->at == p->n || !isdigit((unsigned char)p->s[p->at])) goto invalid; while (p->at < p->n && isdigit((unsigned char)p->s[p->at])) p->at++; }
	}
	if (p->at < p->n && !strchr(" \t\r\n,]}", p->s[p->at])) goto invalid;
	index = token_add(p, JSON_PRIMITIVE, start);
	if (index >= 0) p->tokens[index].end = p->at, p->tokens[index].next = p->count;
	return index;
invalid:
	error(p->error, TCTI_FEATURE_INVALID_JSON, p->at, "invalid JSON primitive");
	return -1;
}

static int json_value(struct parser *p)
{
	int result;
	whitespace(p);
	if (++p->depth > MAX_DEPTH) { error(p->error, TCTI_FEATURE_LIMIT, p->at, "JSON depth"); return -1; }
	if (p->at == p->n) result = -1;
	else if (p->s[p->at] == '{') result = json_container(p, JSON_OBJECT, '}');
	else if (p->s[p->at] == '[') result = json_container(p, JSON_ARRAY, ']');
	else if (p->s[p->at] == '"') result = json_string(p);
	else result = json_primitive(p);
	p->depth--;
	if (result < 0 && !p->error->code) error(p->error, TCTI_FEATURE_INVALID_JSON, p->at, "missing JSON value");
	return result;
}

static bool string_eq(const struct importer *in, int index, const char *text)
{
	size_t length = strlen(text);
	return index >= 0 && (size_t)index < in->count && in->tokens[index].kind == JSON_STRING &&
		in->tokens[index].text_len == length && !memcmp(in->tokens[index].text, text, length);
}

static int array_child(const struct importer *in, int array, size_t nth)
{
	size_t index;
	if (array < 0 || (size_t)array >= in->count || in->tokens[array].kind != JSON_ARRAY || nth >= in->tokens[array].size) return -1;
	index = (size_t)array + 1;
	while (nth--) index = in->tokens[index].next;
	return (int)index;
}

static int field(const struct importer *in, int object, const char *name)
{
	size_t index, pairs;
	if (object < 0 || (size_t)object >= in->count || in->tokens[object].kind != JSON_OBJECT) return -1;
	index = (size_t)object + 1;
	for (pairs = in->tokens[object].size; pairs; pairs--) {
		if (string_eq(in, (int)index, name)) return (int)in->tokens[index].next;
		index = in->tokens[in->tokens[index].next].next;
	}
	return -1;
}

static char *string_copy(const struct importer *in, int index)
{
	char *copy;
	if (index < 0 || (size_t)index >= in->count || in->tokens[index].kind != JSON_STRING || memchr(in->tokens[index].text, 0, in->tokens[index].text_len)) return NULL;
	copy = malloc(in->tokens[index].text_len + 1);
	if (copy) memcpy(copy, in->tokens[index].text, in->tokens[index].text_len + 1);
	return copy;
}

static int node_add(struct importer *in, struct tcti_feature_node node, uint32_t *out)
{
	if (in->model->node_count == UINT32_MAX || grow((void **)&in->model->nodes, &in->model->node_capacity, in->model->node_count + 1, sizeof(*in->model->nodes))) {
		free(node.text); error(in->error, TCTI_FEATURE_NO_MEMORY, 0, "feature nodes"); return -1;
	}
	*out = (uint32_t)in->model->node_count;
	in->model->nodes[in->model->node_count++] = node;
	return 0;
}

static int node(struct importer *in, int object, uint32_t *out);
static int node_children(struct importer *in, int array, uint32_t *first, uint32_t *count)
{
	size_t i, n;
	if (array < 0 || (size_t)array >= in->count || in->tokens[array].kind != JSON_ARRAY || in->tokens[array].size > UINT32_MAX || in->model->child_count > UINT32_MAX - in->tokens[array].size) goto bad;
	n = in->tokens[array].size;
	if (grow((void **)&in->model->children, &in->model->child_capacity, in->model->child_count + n, sizeof(*in->model->children))) goto bad;
	*first = (uint32_t)in->model->child_count; *count = (uint32_t)n;
	for (i = 0; i < n; i++) if (node(in, array_child(in, array, i), &in->model->children[in->model->child_count++])) return -1;
	return 0;
bad:
	error(in->error, TCTI_FEATURE_INVALID_SOURCE, array >= 0 && (size_t)array < in->count ? in->tokens[array].start : 0, "AST children"); return -1;
}

static int operator_kind(struct importer *in, int object, enum tcti_feature_node_kind *kind)
{
	int op = field(in, object, "op");
	if (string_eq(in, op, "&&")) *kind = TCTI_FEATURE_AND;
	else if (string_eq(in, op, "||")) *kind = TCTI_FEATURE_OR;
	else if (string_eq(in, op, "==")) *kind = TCTI_FEATURE_EQ;
	else if (string_eq(in, op, "!=")) *kind = TCTI_FEATURE_NE;
	else if (string_eq(in, op, "<")) *kind = TCTI_FEATURE_LT;
	else if (string_eq(in, op, ">")) *kind = TCTI_FEATURE_GT;
	else if (string_eq(in, op, ">=")) *kind = TCTI_FEATURE_GE;
	else if (string_eq(in, op, "IN")) *kind = TCTI_FEATURE_IN;
	else if (string_eq(in, op, "==>")) *kind = TCTI_FEATURE_IMPLIES;
	else if (string_eq(in, op, "<=>")) *kind = TCTI_FEATURE_IFF;
	else { error(in->error, TCTI_FEATURE_UNSUPPORTED_GRAMMAR, in->tokens[object].start, "unsupported operator"); return -1; }
	return 0;
}

static int primitive_integer(const struct importer *in, int index, int64_t *value)
{
	char *end;
	if (index < 0 || (size_t)index >= in->count || in->tokens[index].kind != JSON_PRIMITIVE) return -1;
	*value = strtoll(in->s + in->tokens[index].start, &end, 10);
	return (size_t)(end - in->s) == in->tokens[index].end ? 0 : -1;
}

static bool primitive_eq(const struct importer *in, int index, const char *text)
{
	size_t length = strlen(text);
	return index >= 0 && (size_t)index < in->count && in->tokens[index].kind == JSON_PRIMITIVE &&
		in->tokens[index].end - in->tokens[index].start == length &&
		!memcmp(in->s + in->tokens[index].start, text, length);
}

static int field_null_qualifier(struct importer *in, int object,
	const char *name, struct tcti_feature_field_qualifier *qualifier)
{
	int value = field(in, object, name);

	/*
	 * Do not collapse a missing qualifier with JSON null. The pinned source
	 * contains an explicit null for both fields, and another representation has
	 * no defined target-inventory meaning yet.
	 */
	if (!primitive_eq(in, value, "null")) {
		error(in->error, TCTI_FEATURE_UNSUPPORTED_GRAMMAR,
			value >= 0 && (size_t)value < in->count ?
			in->tokens[value].start : in->tokens[object].start,
			"unsupported Types.Field qualifier");
		return -1;
	}
	qualifier->kind = TCTI_FEATURE_FIELD_QUALIFIER_NULL;
	qualifier->provenance = (struct tcti_feature_provenance) {
		in->tokens[value].start,
		in->tokens[value].end - in->tokens[value].start,
	};
	return 0;
}

static int node(struct importer *in, int object, uint32_t *out)
{
	int type, value;
	struct tcti_feature_node n;
	if (object < 0 || (size_t)object >= in->count || in->tokens[object].kind != JSON_OBJECT) goto malformed;
	if (++in->depth > MAX_DEPTH) { error(in->error, TCTI_FEATURE_LIMIT, in->tokens[object].start, "AST depth"); return -1; }
	type = field(in, object, "_type");
	n = (struct tcti_feature_node){ .left = TCTI_FEATURE_NODE_NONE, .right = TCTI_FEATURE_NODE_NONE, .first_child = TCTI_FEATURE_NODE_NONE, .provenance = { in->tokens[object].start, in->tokens[object].end - in->tokens[object].start } };
	if (string_eq(in, type, "AST.Bool")) { value = field(in, object, "value"); if (!primitive_eq(in, value, "true") && !primitive_eq(in, value, "false")) goto malformed_depth; n.kind = TCTI_FEATURE_BOOL; n.integer = in->s[in->tokens[value].start] == 't'; }
	else if (string_eq(in, type, "AST.Identifier") || string_eq(in, type, "Values.Value")) { n.kind = string_eq(in, type, "AST.Identifier") ? TCTI_FEATURE_IDENTIFIER : TCTI_FEATURE_VALUE; n.text = string_copy(in, field(in, object, "value")); if (!n.text) goto malformed_depth; }
	else if (string_eq(in, type, "AST.Integer")) { n.kind = TCTI_FEATURE_INTEGER; if (primitive_integer(in, field(in, object, "value"), &n.integer)) goto malformed_depth; }
	else if (string_eq(in, type, "AST.DotAtom") || string_eq(in, type, "AST.Set")) { n.kind = string_eq(in, type, "AST.DotAtom") ? TCTI_FEATURE_DOT_ATOM : TCTI_FEATURE_SET; if (node_children(in, field(in, object, "values"), &n.first_child, &n.child_count)) goto fail_depth; }
	else if (string_eq(in, type, "AST.UnaryOp")) { if (!string_eq(in, field(in, object, "op"), "!")) { error(in->error, TCTI_FEATURE_UNSUPPORTED_GRAMMAR, in->tokens[object].start, "unsupported unary operator"); goto fail_depth; } n.kind = TCTI_FEATURE_NOT; if (node(in, field(in, object, "expr"), &n.left)) goto fail_depth; }
	else if (string_eq(in, type, "AST.BinaryOp")) { if (operator_kind(in, object, &n.kind) || node(in, field(in, object, "left"), &n.left) || node(in, field(in, object, "right"), &n.right)) goto fail_depth; }
	else if (string_eq(in, type, "AST.Function")) { if (string_eq(in, field(in, object, "name"), "UInt")) n.kind = TCTI_FEATURE_UINT; else if (string_eq(in, field(in, object, "name"), "SInt")) n.kind = TCTI_FEATURE_SINT; else { error(in->error, TCTI_FEATURE_UNSUPPORTED_GRAMMAR, in->tokens[object].start, "unsupported function"); goto fail_depth; } if (node_children(in, field(in, object, "arguments"), &n.first_child, &n.child_count) || n.child_count != 1) { error(in->error, TCTI_FEATURE_INVALID_SOURCE, in->tokens[object].start, "function arguments"); goto fail_depth; } }
	else if (string_eq(in, type, "Types.Field")) {
		n.kind = TCTI_FEATURE_FIELD;
		value = field(in, object, "value");
		if (value < 0 || in->tokens[value].kind != JSON_OBJECT)
			goto malformed_depth;
		n.field.state = string_copy(in, field(in, value, "state"));
		n.field.register_name = string_copy(in, field(in, value, "name"));
		n.field.selector = string_copy(in, field(in, value, "field"));
		n.field.provenance = (struct tcti_feature_provenance) {
			in->tokens[value].start, in->tokens[value].end - in->tokens[value].start,
		};
		if (!n.field.state || !n.field.register_name || !n.field.selector)
			goto malformed_depth;
		if (field_null_qualifier(in, value, "instance", &n.field.instance) ||
		    field_null_qualifier(in, value, "slices", &n.field.slices))
			goto fail_depth;
	}
	else { error(in->error, TCTI_FEATURE_UNSUPPORTED_GRAMMAR, in->tokens[object].start, "unsupported AST type"); goto fail_depth; }
	in->depth--; return node_add(in, n, out);
malformed_depth:
	error(in->error, TCTI_FEATURE_INVALID_SOURCE, in->tokens[object].start, "malformed AST node");
fail_depth:
	free(n.text); free(n.field.state); free(n.field.register_name);
	free(n.field.selector); in->depth--; return -1;
malformed:
	error(in->error, TCTI_FEATURE_INVALID_SOURCE, object >= 0 && (size_t)object < in->count ? in->tokens[object].start : 0, "malformed AST node"); return -1;
}

static int metadata(const struct importer *in, int root)
{
	int meta = field(in, root, "_meta"), version = field(in, meta, "version");
	return string_eq(in, field(in, root, "_type"), "Features") && string_eq(in, field(in, version, "architecture"), "vFATAp1-A") && string_eq(in, field(in, version, "build"), "818") && string_eq(in, field(in, version, "ref"), "2026-06_rel") && string_eq(in, field(in, version, "schema"), "2.9.5") ? 0 : -1;
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

static void sha256_block(struct sha256_state *sha,
	const unsigned char *block)
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
			((uint32_t)block[index * 4 + 2] << 8) |
			block[index * 4 + 3];
	for (; index < 64; index++)
		words[index] = words[index - 16] +
			(rotate_right(words[index - 15], 7) ^
			 rotate_right(words[index - 15], 18) ^
			 (words[index - 15] >> 3)) +
			words[index - 7] +
			(rotate_right(words[index - 2], 17) ^
			 rotate_right(words[index - 2], 19) ^
			 (words[index - 2] >> 10));
	a = sha->state[0]; b = sha->state[1]; c = sha->state[2];
	d = sha->state[3]; e = sha->state[4]; f = sha->state[5];
	g = sha->state[6]; h = sha->state[7];
	for (index = 0; index < 64; index++) {
		uint32_t first = h +
			(rotate_right(e, 6) ^ rotate_right(e, 11) ^
			 rotate_right(e, 25)) +
			((e & f) ^ (~e & g)) + round_constants[index] +
			words[index];
		uint32_t second =
			(rotate_right(a, 2) ^ rotate_right(a, 13) ^
			 rotate_right(a, 22)) +
			((a & b) ^ (a & c) ^ (b & c));

		h = g; g = f; f = e; e = d + first;
		d = c; c = b; b = a; a = first + second;
	}
	sha->state[0] += a; sha->state[1] += b;
	sha->state[2] += c; sha->state[3] += d;
	sha->state[4] += e; sha->state[5] += f;
	sha->state[6] += g; sha->state[7] += h;
}

static void sha256_update(struct sha256_state *sha, const char *data,
	size_t length)
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

static void sha256_hex(const char *data, size_t length, char output[65])
{
	static const char hex[] = "0123456789abcdef";
	struct sha256_state sha = { .state = {
		0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
		0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U
	} };
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
		sha.block[56 + index] =
			(unsigned char)(bits >> (56 - index * 8));
	sha256_block(&sha, sha.block);
	for (index = 0; index < 8; index++) {
		uint32_t word = sha.state[index];
		size_t byte;

		for (byte = 0; byte < 4; byte++) {
			unsigned char value =
				(unsigned char)(word >> (24 - byte * 8));

			output[(index * 4 + byte) * 2] = hex[value >> 4];
			output[(index * 4 + byte) * 2 + 1] =
				hex[value & 15];
		}
	}
	output[64] = '\0';
}

int tcti_target_feature_model_import(const char *s, size_t n,
	struct tcti_feature_model *model, struct tcti_feature_error *e)
{
	struct tcti_feature_error local = { 0 };
	struct parser p = { .s = s, .n = n, .error = e ? e : &local };
	struct importer in;
	int root, parameters, constraints;
	size_t i, j;
	char digest[65];
	if (!s || !n || !model) { error(p.error, TCTI_FEATURE_INVALID_ARGUMENT, 0, "input and model required"); return -1; }
	if (n > MAX_INPUT) { error(p.error, TCTI_FEATURE_LIMIT, 0, "input limit"); return -1; }
	memset(model, 0, sizeof(*model));
	root = json_value(&p); whitespace(&p);
	if (root < 0 || p.at != n) { if (!p.error->code) error(p.error, TCTI_FEATURE_INVALID_JSON, p.at, "trailing JSON"); goto fail; }
	in = (struct importer){ .s = s, .tokens = p.tokens, .count = p.count, .model = model, .error = p.error };
	if (metadata(&in, root)) { error(p.error, TCTI_FEATURE_PIN_MISMATCH, 0, "Features.json metadata pin"); goto fail; }
	parameters = field(&in, root, "parameters"); constraints = field(&in, root, "constraints");
	if (parameters < 0 || constraints < 0 || p.tokens[parameters].kind != JSON_ARRAY || p.tokens[constraints].kind != JSON_ARRAY) goto bad;
	for (i = 0; i < p.tokens[parameters].size; i++) {
		int parameter = array_child(&in, parameters, i), pc = field(&in, parameter, "constraints");
		struct tcti_feature_parameter x = { .name = string_copy(&in, field(&in, parameter, "name")), .first_constraint = (uint32_t)model->constraint_count, .provenance = { p.tokens[parameter].start, p.tokens[parameter].end - p.tokens[parameter].start } };
		if (parameter < 0 || p.tokens[parameter].kind != JSON_OBJECT || !string_eq(&in, field(&in, parameter, "_type"), "Parameters.Boolean") || !x.name || pc < 0 || p.tokens[pc].kind != JSON_ARRAY || grow((void **)&model->parameters, &model->parameter_capacity, model->parameter_count + 1, sizeof(*model->parameters))) { free(x.name); goto bad; }
		for (j = 0; j < p.tokens[pc].size; j++) { uint32_t index; if (node(&in, array_child(&in, pc, j), &index) || grow((void **)&model->constraints, &model->constraint_capacity, model->constraint_count + 1, sizeof(*model->constraints))) goto fail; x.constraint_count++; model->constraints[model->constraint_count++] = index; }
		model->parameters[model->parameter_count++] = x;
	}
	for (i = 0; i < p.tokens[constraints].size; i++) { uint32_t index; if (node(&in, array_child(&in, constraints, i), &index) || grow((void **)&model->constraints, &model->constraint_capacity, model->constraint_count + 1, sizeof(*model->constraints))) goto fail; model->constraints[model->constraint_count++] = index; }
	if (model->parameter_count != TCTI_FEATURE_PARAMETER_COUNT || model->constraint_count != TCTI_FEATURE_CONSTRAINT_COUNT) { error(p.error, TCTI_FEATURE_COUNT_MISMATCH, 0, "Features.json counts"); goto fail; }
	sha256_hex(s, n, digest);
	if (memcmp(digest, PIN, sizeof(digest))) { error(p.error, TCTI_FEATURE_PIN_MISMATCH, 0, "Features.json SHA-256 pin"); goto fail; }
	for (i = 0; i < p.count; i++) free(p.tokens[i].text);
	free(p.tokens); return 0;
bad:
	error(p.error, TCTI_FEATURE_INVALID_SOURCE, 0, "Features.json shape");
fail:
	for (i = 0; i < p.count; i++) free(p.tokens[i].text);
	free(p.tokens); tcti_target_feature_model_destroy(model); return -1;
}

void tcti_target_feature_model_destroy(struct tcti_feature_model *model)
{
	size_t i;
	if (!model) return;
	for (i = 0; i < model->parameter_count; i++) free(model->parameters[i].name);
	for (i = 0; i < model->node_count; i++) {
		free(model->nodes[i].text);
		free(model->nodes[i].field.state);
		free(model->nodes[i].field.register_name);
		free(model->nodes[i].field.selector);
	}
	free(model->parameters); free(model->constraints); free(model->nodes); free(model->children); memset(model, 0, sizeof(*model));
}
