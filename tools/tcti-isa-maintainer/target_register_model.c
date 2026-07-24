// SPDX-License-Identifier: GPL-2.0-only
#include "target_register_model.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define tcti_register_model_destroy tcti_register_model_destroy_base

#define MAX_INPUT (128U * 1024U * 1024U)
#define MAX_TOKENS 5000000U
#define MAX_DEPTH 256U
#define MAX_ALLOC (512U * 1024U * 1024U)

enum kind { OBJ, ARR, STR, PRIM };
struct tok { enum kind kind; size_t start, end, next, size; };
struct parser { const char *s; size_t n, p, depth, count, cap, *allocated; struct tok *t; struct tcti_register_model_error *e; };
struct import {
	const char *s;
	struct tok *t;
	size_t n;
	size_t depth;
	size_t *allocated;
	uint32_t *expression_by_token;
	struct tcti_register_model *m;
	struct tcti_register_model_error *e;
};

static void fail(struct tcti_register_model_error *e, enum tcti_register_model_error_code c, size_t p, const char *f, ...)
{
	va_list ap;
	if (!e || e->code)
		return;
	e->code = c; e->offset = p;
	va_start(ap, f); (void)vsnprintf(e->message, sizeof(e->message), f, ap); va_end(ap);
}
static int grow(void **p, size_t *cap, size_t n, size_t size, size_t *allocated)
{
	size_t next = *cap ? *cap : 64, bytes, old_bytes; void *q;
	if (n <= *cap) return 0;
	while (next < n) { if (next > SIZE_MAX / 2) return -1; next *= 2; }
	if (next > SIZE_MAX / size || (bytes = next * size) > MAX_ALLOC || (old_bytes = *cap * size) > bytes || *allocated - old_bytes > MAX_ALLOC - bytes) return -1;
	q = realloc(*p, next * size); if (!q) return -1;
	*p = q; *cap = next; *allocated = *allocated - old_bytes + bytes; return 0;
}
static int addtok(struct parser *p, enum kind k, size_t start)
{
	if (p->count >= MAX_TOKENS || grow((void **)&p->t, &p->cap, p->count + 1, sizeof(*p->t), p->allocated)) {
		fail(p->e, p->count >= MAX_TOKENS ? TCTI_REGISTER_MODEL_INPUT_LIMIT : TCTI_REGISTER_MODEL_NO_MEMORY, p->p, "JSON token storage limit exceeded"); return -1;
	}
	p->t[p->count] = (struct tok){ .kind = k, .start = start }; return (int)p->count++;
}
static void ws(struct parser *p) { while (p->p < p->n && strchr(" \t\r\n", p->s[p->p])) p->p++; }
static int value(struct parser *p);
static int decoded(const char *s,size_t n,char **out,size_t *len,size_t *allocated);
/*
 * Object-key duplicate detection is on the parser's hot path. Arm's source
 * uses ordinary ASCII keys, so avoid allocating and decoding every pair of
 * already-valid unescaped strings. Escaped keys still compare by decoded
 * Unicode value, preserving JSON's duplicate-key semantics.
 */
static int token_same(const struct parser *p, int a, int b)
{
	const char *left_source;
	const char *right_source;
	char *left = NULL;
	char *right = NULL;
	size_t left_length;
	size_t right_length;
	size_t left_allocation;
	size_t right_allocation;
	int same = -1;

	left_length = p->t[a].end - p->t[a].start;
	right_length = p->t[b].end - p->t[b].start;
	left_source = p->s + p->t[a].start;
	right_source = p->s + p->t[b].start;
	if (!memchr(left_source, '\\', left_length) &&
	    !memchr(right_source, '\\', right_length))
		return left_length == right_length &&
		       !memcmp(left_source, right_source, left_length);

	left_allocation = left_length + 1;
	right_allocation = right_length + 1;
	if (decoded(left_source, left_length, &left, &left_length, p->allocated))
		goto out;
	if (decoded(right_source, right_length, &right, &right_length,
		    p->allocated))
		goto out;
	same = left_length == right_length && !memcmp(left, right, left_length);
out:
	if (left) {
		free(left);
		*p->allocated -= left_allocation;
	}
	if (right) {
		free(right);
		*p->allocated -= right_allocation;
	}
	return same;
}
static int string(struct parser *p)
{
	size_t quote = p->p++; int i = addtok(p, STR, p->p);
	if (i < 0) return -1;
	while (p->p < p->n) { unsigned char c = (unsigned char)p->s[p->p++];
		if (c == '"') { p->t[i].end = p->p - 1; p->t[i].next = p->count; return i; }
		if (c < 0x20) break;
		if (c == '\\') { if (p->p == p->n) break; c = (unsigned char)p->s[p->p++];
			if (strchr("\"\\/bfnrt", c)) continue;
			if (c != 'u' || p->n - p->p < 4) break;
			for (size_t x = 0; x < 4; x++) { c = (unsigned char)p->s[p->p++]; if (!strchr("0123456789abcdefABCDEF", c)) goto bad; }
		}
	}
bad: fail(p->e, TCTI_REGISTER_MODEL_INVALID_JSON, quote, "invalid JSON string"); return -1;
}
static int primitive(struct parser *p)
{
	size_t start = p->p; int i;
	while (p->p < p->n && !strchr(" \t\r\n,]}", p->s[p->p])) p->p++;
	if (start == p->p) goto bad;
	if (!((p->p - start == 4 && !memcmp(p->s + start, "true", 4)) || (p->p - start == 5 && !memcmp(p->s + start, "false", 5)) || (p->p - start == 4 && !memcmp(p->s + start, "null", 4)))) {
		size_t x = 0, n = p->p - start; const char *s = p->s + start;
		if (s[x] == '-') x++; if (x == n || (s[x] < '0' || s[x] > '9')) goto bad;
		if (s[x] == '0') x++; else while (x < n && s[x] >= '0' && s[x] <= '9') x++;
		if (x < n && s[x] == '.') { x++; if (x == n || s[x] < '0' || s[x] > '9') goto bad; while (x < n && s[x] >= '0' && s[x] <= '9') x++; }
		if (x < n && (s[x] == 'e' || s[x] == 'E')) { x++; if (x < n && (s[x] == '+' || s[x] == '-')) x++; if (x == n || s[x] < '0' || s[x] > '9') goto bad; while (x < n && s[x] >= '0' && s[x] <= '9') x++; }
		if (x != n) goto bad;
	}
	i = addtok(p, PRIM, start); if (i < 0) return -1; p->t[i].end = p->p; p->t[i].next = p->count; return i;
bad: fail(p->e, TCTI_REGISTER_MODEL_INVALID_JSON, start, "invalid JSON primitive"); return -1;
}
static int array(struct parser *p)
{
	int i = addtok(p, ARR, p->p++); if (i < 0) return -1;
	if (++p->depth > MAX_DEPTH) goto deep; ws(p);
	if (p->p < p->n && p->s[p->p] == ']') goto done;
	for (;;) { if (value(p) < 0) goto out; p->t[i].size++; ws(p); if (p->p < p->n && p->s[p->p] == ']') goto done; if (p->p == p->n || p->s[p->p++] != ',') goto out; ws(p); }
done: p->p++; p->depth--; p->t[i].end = p->p; p->t[i].next = p->count; return i;
deep: fail(p->e, TCTI_REGISTER_MODEL_DEPTH_LIMIT, p->p - 1, "JSON depth limit exceeded"); return -1;
out: p->depth--; if (!p->e->code) fail(p->e, TCTI_REGISTER_MODEL_INVALID_JSON, p->p, "invalid JSON array"); return -1;
}
static int object(struct parser *p)
{
	int i = addtok(p, OBJ, p->p++); if (i < 0) return -1;
	if (++p->depth > MAX_DEPTH) goto deep; ws(p);
	if (p->p < p->n && p->s[p->p] == '}') goto done;
	for (;;) { int key; size_t prior,c; if (p->p == p->n || p->s[p->p] != '"' || (key=string(p)) < 0) goto out; c=(size_t)i+1; for(prior=0;prior<p->t[i].size;prior++){int same=token_same(p,key,(int)c);if(same){fail(p->e,TCTI_REGISTER_MODEL_INVALID_JSON,p->t[key].start,"duplicate JSON object key");goto out;}if(same<0){fail(p->e,TCTI_REGISTER_MODEL_INVALID_JSON,p->t[key].start,"invalid JSON string");goto out;}c=p->t[p->t[c].next].next;} ws(p); if (p->p == p->n || p->s[p->p++] != ':' || value(p) < 0) goto out; p->t[i].size++; ws(p); if (p->p < p->n && p->s[p->p] == '}') goto done; if (p->p == p->n || p->s[p->p++] != ',') goto out; ws(p); }
done: p->p++; p->depth--; p->t[i].end = p->p; p->t[i].next = p->count; return i;
deep: fail(p->e, TCTI_REGISTER_MODEL_DEPTH_LIMIT, p->p - 1, "JSON depth limit exceeded"); return -1;
out: p->depth--; if (!p->e->code) fail(p->e, TCTI_REGISTER_MODEL_INVALID_JSON, p->p, "invalid JSON object"); return -1;
}
static int value(struct parser *p) { ws(p); if (p->p == p->n) return -1; if (p->s[p->p] == '{') return object(p); if (p->s[p->p] == '[') return array(p); if (p->s[p->p] == '"') return string(p); return primitive(p); }
static int hex(unsigned char c) { if(c>='0'&&c<='9')return c-'0'; if(c>='a'&&c<='f')return c-'a'+10; if(c>='A'&&c<='F')return c-'A'+10; return -1; }
static int utf8(const char *s,size_t n,size_t *p,uint32_t *out) { unsigned char c=(unsigned char)s[(*p)++]; uint32_t v; size_t more;
	if(c<0x80){*out=c;return 0;} if(c>=0xc2&&c<=0xdf){v=c&31;more=1;} else if(c>=0xe0&&c<=0xef){v=c&15;more=2;} else if(c>=0xf0&&c<=0xf4){v=c&7;more=3;} else return -1;
	while(more--){if(*p>=n||((unsigned char)s[*p]&0xc0)!=0x80)return -1;v=(v<<6)|((unsigned char)s[(*p)++]&63);} if(v>=0xd800&&v<=0xdfff)return -1; *out=v;return 0; }
static int decoded(const char *s,size_t n,char **out,size_t *len,size_t *allocated) { size_t p=0,u=0; char *d;
if(n==SIZE_MAX||*allocated>MAX_ALLOC-(n+1))return -1; d=malloc(n+1);if(!d)return -1;*allocated+=n+1;
	while(p<n){uint32_t v; if((unsigned char)s[p]<0x20)goto bad; if(s[p]!='\\'){if(utf8(s,n,&p,&v))goto bad;}else{int h; p++;if(p>=n)goto bad;switch(s[p++]){case '"':v='"';break;case '\\':v='\\';break;case '/':v='/';break;case 'b':v='\b';break;case 'f':v='\f';break;case 'n':v='\n';break;case 'r':v='\r';break;case 't':v='\t';break;case 'u':if(p+4>n||(h=hex(s[p]))<0)goto bad;v=(uint32_t)h<<12;if((h=hex(s[p+1]))<0)goto bad;v|=(uint32_t)h<<8;if((h=hex(s[p+2]))<0)goto bad;v|=(uint32_t)h<<4;if((h=hex(s[p+3]))<0)goto bad;v|=h;p+=4;if(v>=0xd800&&v<=0xdbff){uint32_t lo;if(p+6>n||s[p]!='\\'||s[p+1]!='u')goto bad;p+=2;for(h=0;h<4;h++){int q=hex(s[p+h]);if(q<0)goto bad;lo=(h?lo<<4:0)|(uint32_t)q;}p+=4;if(lo<0xdc00||lo>0xdfff)goto bad;v=0x10000+((v-0xd800)<<10)+(lo-0xdc00);}else if(v>=0xdc00&&v<=0xdfff)goto bad;break;default:goto bad;}}
	if(v<0x80)d[u++]=(char)v;else if(v<0x800){d[u++]=(char)(0xc0|(v>>6));d[u++]=(char)(0x80|(v&63));}else if(v<0x10000){d[u++]=(char)(0xe0|(v>>12));d[u++]=(char)(0x80|(v>>6));d[u++]=(char)(0x80|(v&63));}else{d[u++]=(char)(0xf0|(v>>18));d[u++]=(char)(0x80|(v>>12));d[u++]=(char)(0x80|(v>>6));d[u++]=(char)(0x80|(v&63));}}
	d[u]=0;*out=d;if(len)*len=u;return 0; bad:free(d);*allocated-=n+1;return -1; }
static int eq(const struct import *x, int i, const char *s) { char *d; int ok=0; size_t n, encoded; if(i<0||(size_t)i>=x->n||x->t[i].kind!=STR)return 0; encoded=x->t[i].end-x->t[i].start; if(!decoded(x->s+x->t[i].start,encoded,&d,&n,x->allocated)) { ok=n==strlen(s)&&!memcmp(d,s,n);free(d);*x->allocated-=encoded+1; } return ok; }
static int member(const struct import *x, int o, const char *key)
{
	size_t c, z; if (o < 0 || x->t[o].kind != OBJ) return -1; c = (size_t)o + 1;
	for (z = 0; z < x->t[o].size; z++) { size_t v = x->t[c].next; if (eq(x, (int)c, key)) return (int)v; c = x->t[v].next; } return -1;
}
static int elem(const struct import *x, int a, size_t z) { size_t c; if (a < 0 || x->t[a].kind != ARR || z >= x->t[a].size) return -1; c = (size_t)a + 1; while (z--) c = x->t[c].next; return (int)c; }
static char *dup(const struct import *x, int i) { char *p; if(i<0||x->t[i].kind!=STR)return NULL; if(decoded(x->s+x->t[i].start,x->t[i].end-x->t[i].start,&p,NULL,x->allocated))return NULL; return p; }
static char *domain_text(const struct import *x, int i) { if(i>=0&&x->t[i].kind==OBJ)i=member(x,i,"value"); return dup(x,i); }
static int number(const struct import *x, int i, uint32_t *out) { char b[32]; char *end; unsigned long v; size_t n; if (i < 0 || x->t[i].kind != PRIM) return -1; n = x->t[i].end - x->t[i].start; if (!n || n >= sizeof(b)) return -1; memcpy(b, x->s + x->t[i].start, n); b[n] = 0; v = strtoul(b, &end, 10); if (*end || v > UINT32_MAX) return -1; *out = (uint32_t)v; return 0; }
static int scalar_value(const struct import *x, int token, uint32_t *kind,
			int64_t *integer, uint32_t *boolean, char **text)
{
	char buffer[64];
	char *end;
	size_t length;

	*kind = 0;
	*integer = 0;
	*boolean = 0;
	*text = NULL;
	if (token < 0)
		return 0;
	if (x->t[token].kind == STR) {
		*kind = 1;
		*text = dup(x, token);
		return *text ? 0 : -1;
	}
	if (x->t[token].kind != PRIM)
		return -1;
	length = x->t[token].end - x->t[token].start;
	if (length == 4 && !memcmp(x->s + x->t[token].start, "true", 4)) {
		*kind = 2;
		*boolean = 1;
		return 0;
	}
	if (length == 5 && !memcmp(x->s + x->t[token].start, "false", 5)) {
		*kind = 2;
		return 0;
	}
	if (!length || length >= sizeof(buffer))
		return -1;
	memcpy(buffer, x->s + x->t[token].start, length);
	buffer[length] = 0;
	*integer = strtoll(buffer, &end, 10);
	if (*end)
		return -1;
	*kind = 3;
	return 0;
}
static int expression_child(struct import *x, uint32_t parent, uint32_t child)
{
	struct tcti_register_expression *p;
	if (parent == UINT32_MAX)
		return 0;
	if (parent >= x->m->expression_count ||
	    grow((void **)&x->m->expression_children,
		 &x->m->expression_child_capacity,
		 x->m->expression_child_count + 1,
		 sizeof(*x->m->expression_children), x->allocated)) {
		fail(x->e, TCTI_REGISTER_MODEL_NO_MEMORY, 0,
		     "cannot store ordered expression edge");
		return -1;
	}
	p = &x->m->expressions[parent];
	if (!p->child_count)
		p->first_child = (uint32_t)x->m->expression_child_count;
	x->m->expression_children[x->m->expression_child_count++] = child;
	p->child_count++;
	return 0;
}
static int addreg(struct import *x, int o, int type, uint32_t parent, uint32_t *index) { int meta=member(x,o,"_meta"); struct tcti_register_identity r = { .name=dup(x, member(x,o,"name")), .type=dup(x,type), .state=dup(x,member(x,o,"state")), .index_variable=dup(x,member(x,o,"index_variable")), .parent_register=parent, .metadata_owner=meta>=0?(uint32_t)x->m->register_count:(parent==UINT32_MAX?UINT32_MAX:x->m->registers[parent].metadata_owner), .metadata_index=parent==UINT32_MAX?UINT32_MAX:x->m->registers[parent].metadata_index, .default_access_expression=UINT32_MAX, .first_fieldset=UINT32_MAX, .fieldset_count=0, .metadata_offset=meta>=0?x->t[meta].start:0, .metadata_length=meta>=0?x->t[meta].end-x->t[meta].start:0, .source_offset=x->t[o].start, .source_length=x->t[o].end-x->t[o].start }; size_t i; if (meta >= 0) { for (i = 0; i < x->m->metadata_count; i++) if (x->m->metadata[i].source_offset == x->t[meta].start && x->m->metadata[i].source_length == x->t[meta].end - x->t[meta].start) { r.metadata_index = (uint32_t)i; break; } } if(!r.name||!r.type||r.metadata_owner==UINT32_MAX||r.metadata_index==UINT32_MAX){free(r.name);free(r.type);free(r.state);free(r.index_variable);fail(x->e,TCTI_REGISTER_MODEL_INVALID_SOURCE,x->t[o].start,"register lacks identity or inherited typed metadata");return -1;} for(i=0;i<x->m->register_count;i++)if(x->m->registers[i].parent_register==parent&&!strcmp(x->m->registers[i].name,r.name)&&!strcmp(x->m->registers[i].type,r.type)&&((!x->m->registers[i].state&&!r.state)||(x->m->registers[i].state&&r.state&&!strcmp(x->m->registers[i].state,r.state)))){free(r.name);free(r.type);free(r.state);free(r.index_variable);fail(x->e,TCTI_REGISTER_MODEL_INVALID_SOURCE,x->t[o].start,"duplicate register identity");return -1;} if (grow((void **)&x->m->registers, &x->m->register_capacity, x->m->register_count + 1, sizeof(r),x->allocated)) { free(r.name); free(r.type); free(r.state); free(r.index_variable); fail(x->e,TCTI_REGISTER_MODEL_NO_MEMORY,x->t[o].start,"cannot store register identity"); return -1; } *index=(uint32_t)x->m->register_count; x->m->registers[x->m->register_count++] = r; return 0; }
static int add_expression(struct import *x, int object, uint32_t parent,
			  int role, uint32_t *index);
static int field_condition_roots(struct import *x, int object, uint32_t field,
				 int wrapper_condition);
static int addfieldset(struct import *x, int o, uint32_t reg, uint32_t width)
{
	int condition = member(x, o, "condition");
	struct tcti_register_fieldset f = {
		.register_index = reg, .width = width, .name = dup(x, member(x, o, "name")),
		.display = dup(x, member(x, o, "display")),
		.condition_offset = condition >= 0 ? x->t[condition].start : 0,
		.condition_length = condition >= 0 ? x->t[condition].end - x->t[condition].start : 0,
		.condition_expression = UINT32_MAX,
		.source_offset = x->t[o].start, .source_length = x->t[o].end - x->t[o].start,
	};
	if (grow((void **)&x->m->fieldsets, &x->m->fieldset_capacity,
		 x->m->fieldset_count + 1, sizeof(f), x->allocated)) {
		free(f.name); free(f.display);
		fail(x->e, TCTI_REGISTER_MODEL_NO_MEMORY, x->t[o].start, "cannot store Fieldset");
		return -1;
	}
	x->m->fieldsets[x->m->fieldset_count++] = f;
	if (condition >= 0) {
		uint32_t root;
		if (x->t[condition].kind != OBJ ||
		    add_expression(x, condition, UINT32_MAX, -1, &root)) {
			fail(x->e, TCTI_REGISTER_MODEL_UNSUPPORTED_TYPE,
			     x->t[condition].start, "unsupported Fieldset condition");
			return -1;
		}
		x->m->fieldsets[x->m->fieldset_count - 1].condition_expression = root;
	}
	return 0;
}
static int allowed_accessor(const struct import *x, int type)
{
	return eq(x, type, "Accessors.SystemAccessor") ||
		eq(x, type, "Accessors.SystemAccessorArray") ||
		eq(x, type, "Accessors.MemoryMapped") ||
		eq(x, type, "Accessors.BlockAccess") ||
		eq(x, type, "Accessors.ExternalDebug") ||
		eq(x, type, "Accessors.BlockAccessArray") ||
		eq(x, type, "Accessors.ImplementationDefinedOffsetAccessorArray");
}
static int type_prefix(const struct import *x, int token, const char *prefix)
{
	char *text;
	size_t encoded, length;
	int result = 0;
	if (token < 0 || x->t[token].kind != STR) return 0;
	encoded = x->t[token].end - x->t[token].start;
	if (decoded(x->s + x->t[token].start, encoded, &text, &length, x->allocated)) return 0;
	result = length >= strlen(prefix) && !memcmp(text, prefix, strlen(prefix));
	free(text);
	*x->allocated -= encoded + 1;
	return result;
}
static int tree_object(const struct import *x, int object)
{
	int type = member(x, object, "_type");
	return eq(x, type, "Accessors.Permission.SystemAccess") ||
		eq(x, type, "Accessors.Permission.MemoryAccess") ||
		eq(x, type, "Accessors.Permission.AccessTypes.Memory.ReadWriteAccess") ||
		eq(x, type, "Accessors.Permission.AccessTypes.Memory.ImplementationDefined") ||
		eq(x, type, "AST.BinaryOp") || eq(x, type, "AST.Bool") ||
		eq(x, type, "AST.Concat") || eq(x, type, "AST.DotAtom") ||
		eq(x, type, "AST.Function") || eq(x, type, "AST.Identifier") ||
		eq(x, type, "AST.Integer") || eq(x, type, "AST.Set") ||
		eq(x, type, "AST.Slice") || eq(x, type, "AST.SquareOp") ||
		eq(x, type, "AST.UnaryOp") || eq(x, type, "AST.Assignment") ||
		eq(x, type, "AST.Return") || eq(x, type, "AST.Type") ||
		eq(x, type, "AST.TypeAnnotation") || eq(x, type, "Types.Field") ||
		eq(x, type, "Types.RegisterType") || eq(x, type, "Types.String") ||
		eq(x, type, "Values.Value");
}
static int tree_candidate(const struct import *x, int object)
{
	int type = member(x, object, "_type");
	return type_prefix(x, type, "AST.") || type_prefix(x, type, "Types.");
}
static int null_value(const struct import *x, int token)
{
	return token >= 0 && x->t[token].kind == PRIM &&
		x->t[token].end - x->t[token].start == 4 &&
		!memcmp(x->s + x->t[token].start, "null", 4);
}
static int tree_operator_allowed(const struct import *x, int object)
{
	int type = member(x, object, "_type"), op = member(x, object, "op");
	if (op < 0) return 1;
	if (eq(x, type, "AST.UnaryOp")) return eq(x, op, "!");
	if (!eq(x, type, "AST.BinaryOp")) return 0;
	return eq(x, op, "!=") || eq(x, op, "&&") || eq(x, op, "*") || eq(x, op, "MOD") ||
		eq(x, op, "+") || eq(x, op, "-") || eq(x, op, "<") ||
		eq(x, op, "<=") || eq(x, op, "==") || eq(x, op, ">") || eq(x, op, ">=") ||
		eq(x, op, "IN") || eq(x, op, "||") || eq(x, op, "::");
}
static int expression_key_allowed(const struct import *x, int type, int key)
{
	if (eq(x,key,"_type")) return 1;
	if (eq(x,type,"Accessors.Permission.SystemAccess") ||
	    eq(x,type,"Accessors.Permission.MemoryAccess"))
		return eq(x,key,"access") || eq(x,key,"condition");
	if (eq(x,type,"Accessors.Permission.AccessTypes.Memory.ReadWriteAccess"))
		return eq(x,key,"read") || eq(x,key,"write");
	if (eq(x,type,"Accessors.Permission.AccessTypes.Memory.ImplementationDefined"))
		return eq(x,key,"constraints");
	if (eq(x,type,"AST.BinaryOp")) return eq(x,key,"left") || eq(x,key,"op") || eq(x,key,"right");
	if (eq(x,type,"AST.UnaryOp")) return eq(x,key,"expr") || eq(x,key,"op");
	if (eq(x,type,"AST.Function")) return eq(x,key,"arguments") || eq(x,key,"name") || eq(x,key,"parameters");
	if (eq(x,type,"AST.Assignment")) return eq(x,key,"var") || eq(x,key,"val");
	if (eq(x,type,"AST.Return")) return eq(x,key,"val");
	if (eq(x,type,"AST.Type")) return eq(x,key,"name");
	if (eq(x,type,"AST.TypeAnnotation")) return eq(x,key,"var") || eq(x,key,"type");
	if (eq(x,type,"AST.SquareOp")) return eq(x,key,"var") || eq(x,key,"arguments");
	if (eq(x,type,"AST.Slice")) return eq(x,key,"left") || eq(x,key,"right");
	if (eq(x,type,"AST.Bool") || eq(x,type,"AST.Identifier") || eq(x,type,"AST.Integer")) return eq(x,key,"value");
	if (eq(x,type,"AST.Concat") || eq(x,type,"AST.DotAtom") || eq(x,type,"AST.Set")) return eq(x,key,"values");
	if (eq(x,type,"Types.Field")) return eq(x,key,"value");
	if (eq(x,type,"Types.RegisterType")) return eq(x,key,"value");
	if (eq(x,type,"Types.String") || eq(x,type,"Values.Value")) return eq(x,key,"value") || eq(x,key,"meaning");
	return 0;
}

static int memory_read_access(const struct import *x, int token,
			      enum tcti_register_memory_read_access *out)
{
	if (eq(x, token, "R")) {
		*out = TCTI_REGISTER_MEMORY_READ_R;
		return 0;
	}
	if (eq(x, token, "RAZ")) {
		*out = TCTI_REGISTER_MEMORY_READ_RAZ;
		return 0;
	}
	if (eq(x, token, "RES0")) {
		*out = TCTI_REGISTER_MEMORY_READ_RES0;
		return 0;
	}
	if (eq(x, token, "RESERVED")) {
		*out = TCTI_REGISTER_MEMORY_READ_RESERVED;
		return 0;
	}
	if (eq(x, token, "UNKNOWN")) {
		*out = TCTI_REGISTER_MEMORY_READ_UNKNOWN;
		return 0;
	}
	if (eq(x, token, "ERROR")) {
		*out = TCTI_REGISTER_MEMORY_READ_ERROR;
		return 0;
	}
	return -1;
}

static int memory_write_access(const struct import *x, int token,
			       enum tcti_register_memory_write_access *out)
{
	if (eq(x, token, "W")) {
		*out = TCTI_REGISTER_MEMORY_WRITE_W;
		return 0;
	}
	if (eq(x, token, "RESERVED")) {
		*out = TCTI_REGISTER_MEMORY_WRITE_RESERVED;
		return 0;
	}
	if (eq(x, token, "WI")) {
		*out = TCTI_REGISTER_MEMORY_WRITE_WI;
		return 0;
	}
	if (eq(x, token, "RES0")) {
		*out = TCTI_REGISTER_MEMORY_WRITE_RES0;
		return 0;
	}
	if (eq(x, token, "ERROR")) {
		*out = TCTI_REGISTER_MEMORY_WRITE_ERROR;
		return 0;
	}
	return -1;
}

static int add_memory_access(struct import *x, int object,
			     uint32_t expression_index)
{
	int read = member(x, object, "read");
	int write = member(x, object, "write");
	struct tcti_register_memory_access access = {
		.form = TCTI_REGISTER_MEMORY_ACCESS_EXPLICIT_OBJECT,
		.read_origin = TCTI_REGISTER_MEMORY_ACCESS_EXPLICIT,
		.write_origin = TCTI_REGISTER_MEMORY_ACCESS_EXPLICIT,
		.legacy_sentinel = TCTI_REGISTER_MEMORY_ACCESS_LEGACY_NONE,
		.owner_kind = TCTI_REGISTER_MEMORY_ACCESS_OWNER_EXPRESSION,
		.owner_index = expression_index,
		.read_offset = read >= 0 ? x->t[read].start : 0,
		.read_length = read >= 0 ? x->t[read].end - x->t[read].start : 0,
		.write_offset = write >= 0 ? x->t[write].start : 0,
		.write_length = write >= 0 ? x->t[write].end - x->t[write].start : 0,
		.source_offset = x->t[object].start,
		.source_length = x->t[object].end - x->t[object].start,
	};

	if (x->t[object].kind != OBJ || x->t[object].size != 3 || read < 0 ||
	    write < 0 || x->t[read].kind != STR || x->t[write].kind != STR ||
	    memory_read_access(x, read, &access.read) ||
	    memory_write_access(x, write, &access.write)) {
		fail(x->e, TCTI_REGISTER_MODEL_INVALID_SOURCE, x->t[object].start,
		     "ReadWriteAccess has an unsupported typed permission");
		return -1;
	}
	if (grow((void **)&x->m->memory_accesses,
		 &x->m->memory_access_capacity, x->m->memory_access_count + 1,
		 sizeof(access), x->allocated)) {
		fail(x->e, TCTI_REGISTER_MODEL_NO_MEMORY, x->t[object].start,
		     "cannot retain typed ReadWriteAccess");
		return -1;
	}
	x->m->memory_accesses[x->m->memory_access_count++] = access;
	return 0;
}

static int add_implementation_defined_permission(struct import *x, int object,
					 uint32_t expression_index)
{
	int constraints = member(x, object, "constraints");
	struct tcti_register_implementation_defined_permission permission = {
		.expression_index = expression_index,
		.first_memory_access = UINT32_MAX,
		.source_offset = x->t[object].start,
		.source_length = x->t[object].end - x->t[object].start,
	};
	size_t index;

	if (x->t[object].kind != OBJ || x->t[object].size != 2 ||
	    constraints < 0 ||
	    (!null_value(x, constraints) && x->t[constraints].kind != ARR)) {
		fail(x->e, TCTI_REGISTER_MODEL_INVALID_SOURCE, x->t[object].start,
		     "ImplementationDefined has invalid permission constraints");
		return -1;
	}
	if (null_value(x, constraints)) {
		permission.kind = TCTI_REGISTER_IMPLEMENTATION_DEFINED_CONSTRAINT_NULL;
	} else {
		permission.kind = TCTI_REGISTER_IMPLEMENTATION_DEFINED_CONSTRAINT_ARRAY;
		for (index = 0; index < x->m->memory_access_count; index++) {
			const struct tcti_register_memory_access *access =
				&x->m->memory_accesses[index];
			if (access->owner_kind !=
			    TCTI_REGISTER_MEMORY_ACCESS_OWNER_EXPRESSION ||
			    access->owner_index >= x->m->expression_count ||
			    x->m->expressions[access->owner_index].parent_expression !=
			    expression_index)
				continue;
			if (permission.first_memory_access == UINT32_MAX)
				permission.first_memory_access = (uint32_t)index;
			permission.memory_access_count++;
		}
		if (permission.memory_access_count != x->t[constraints].size) {
			fail(x->e, TCTI_REGISTER_MODEL_INVALID_SOURCE,
			     x->t[constraints].start,
			     "ImplementationDefined constraints are not ReadWriteAccess entries");
			return -1;
		}
	}
	if (grow((void **)&x->m->implementation_defined_permissions,
		 &x->m->implementation_defined_permission_capacity,
		 x->m->implementation_defined_permission_count + 1,
		 sizeof(permission), x->allocated)) {
		fail(x->e, TCTI_REGISTER_MODEL_NO_MEMORY, x->t[object].start,
		     "cannot retain ImplementationDefined permission constraints");
		return -1;
	}
	x->m->implementation_defined_permissions[
		x->m->implementation_defined_permission_count++] = permission;
	return 0;
}
static int add_expression(struct import *x, int object, uint32_t parent,
			  int role, uint32_t *index)
{
	int type = member(x, object, "_type"), payload = member(x, object, "value");

	if (object < 0 || (size_t)object >= x->n) {
		fail(x->e, TCTI_REGISTER_MODEL_INVALID_SOURCE, 0,
		     "condition/action tree token is outside the parsed source");
		return -1;
	}
	if (x->expression_by_token[object] != UINT32_MAX) {
		*index = x->expression_by_token[object];
		return 0;
	}
	struct tcti_register_expression expression = {
		.type = dup(x, type), .name = dup(x, member(x, object, "name")),
		.op = dup(x, member(x, object, "op")),
		.role = role >= 0 ? dup(x, role) : NULL, .parent_expression = parent,
		.first_child = UINT32_MAX,
		.register_state = payload >= 0 && x->t[payload].kind == OBJ ? dup(x, member(x,payload,"state")) : NULL,
		.register_name = payload >= 0 && x->t[payload].kind == OBJ ? dup(x, member(x,payload,"name")) : NULL,
		.field_name = payload >= 0 && x->t[payload].kind == OBJ ? dup(x, member(x,payload,"field")) : NULL,
		.instance_offset = payload >= 0 && x->t[payload].kind == OBJ && member(x,payload,"instance") >= 0 ? x->t[member(x,payload,"instance")].start : 0,
		.instance_length = payload >= 0 && x->t[payload].kind == OBJ && member(x,payload,"instance") >= 0 ? x->t[member(x,payload,"instance")].end-x->t[member(x,payload,"instance")].start : 0,
		.slices_offset = payload >= 0 && x->t[payload].kind == OBJ && member(x,payload,"slices") >= 0 ? x->t[member(x,payload,"slices")].start : 0,
		.slices_length = payload >= 0 && x->t[payload].kind == OBJ && member(x,payload,"slices") >= 0 ? x->t[member(x,payload,"slices")].end-x->t[member(x,payload,"slices")].start : 0,
		.source_offset = x->t[object].start, .source_length = x->t[object].end - x->t[object].start,
	};
	size_t child, z;
	if (payload >= 0 && x->t[payload].kind != OBJ &&
	    scalar_value(x, payload, &expression.scalar_kind, &expression.integer,
			 &expression.boolean, &expression.value)) {
		fail(x->e, TCTI_REGISTER_MODEL_INVALID_SOURCE, x->t[object].start,
		     "condition/action scalar has unsupported JSON type");
		goto cleanup;
	}
	if (!tree_object(x, object) || !tree_operator_allowed(x, object)) {
		free(expression.type); free(expression.name); free(expression.op); free(expression.value); free(expression.role);
		fail(x->e, TCTI_REGISTER_MODEL_UNSUPPORTED_TYPE, x->t[object].start, "unsupported condition/action tree node");
		return -1;
	}
	if (!expression.type || grow((void **)&x->m->expressions, &x->m->expression_capacity,
		x->m->expression_count + 1, sizeof(expression), x->allocated)) {
		free(expression.type); free(expression.name); free(expression.op); free(expression.value); free(expression.role);
		fail(x->e, TCTI_REGISTER_MODEL_NO_MEMORY, x->t[object].start, "cannot store condition/action tree");
		return -1;
	}
	child = (size_t)object + 1;
	for (z = 0; z < x->t[object].size; z++) {
		if (!expression_key_allowed(x, type, (int)child)) {
			free(expression.type); free(expression.name); free(expression.op); free(expression.value); free(expression.role);
			fail(x->e, TCTI_REGISTER_MODEL_UNSUPPORTED_TYPE, x->t[child].start, "unknown condition/action tree key");
			return -1;
		}
		child = x->t[x->t[child].next].next;
	}
	*index = (uint32_t)x->m->expression_count;
	x->m->expressions[x->m->expression_count++] = expression;
	x->expression_by_token[object] = *index;
	if (expression_child(x, parent, *index))
		return -1;
	child = (size_t)object + 1;
	for (z = 0; z < x->t[object].size; z++) {
		size_t value = x->t[child].next;
		if (x->t[value].kind == OBJ && tree_candidate(x, (int)value) && !tree_object(x, (int)value)) {
			fail(x->e, TCTI_REGISTER_MODEL_UNSUPPORTED_TYPE, x->t[value].start, "unsupported condition/action tree child");
			return -1;
		} else if (x->t[value].kind == OBJ && tree_object(x, (int)value)) {
			uint32_t ignored;
			if (add_expression(x, (int)value, *index, (int)child, &ignored)) return -1;
		} else if (x->t[value].kind == ARR) {
			size_t item;
			for (item = 0; item < x->t[value].size; item++) {
				int nested = elem(x, (int)value, item);
				if (nested >= 0 && x->t[nested].kind == OBJ && tree_candidate(x, nested) && !tree_object(x, nested)) {
					fail(x->e, TCTI_REGISTER_MODEL_UNSUPPORTED_TYPE, x->t[nested].start, "unsupported condition/action tree array child");
					return -1;
				} else if (nested >= 0 && x->t[nested].kind == OBJ && tree_object(x, nested)) {
					uint32_t ignored;
					if (add_expression(x, nested, *index, (int)child, &ignored)) return -1;
				}
			}
		}
		child = x->t[value].next;
	}
	if (eq(x, type, "Accessors.Permission.AccessTypes.Memory.ReadWriteAccess") &&
	    add_memory_access(x, object, *index))
		return -1;
	if (eq(x, type,
	       "Accessors.Permission.AccessTypes.Memory.ImplementationDefined") &&
	    add_implementation_defined_permission(x, object, *index))
		return -1;
	return 0;
cleanup:
	free(expression.type); free(expression.name); free(expression.op);
	free(expression.value); free(expression.role); free(expression.register_state);
	free(expression.register_name); free(expression.field_name);
	return -1;
}
static int allowed_selector_name(const struct import *x, int name)
{
	return eq(x,name,"CRd") || eq(x,name,"CRm") || eq(x,name,"CRn") ||
		eq(x,name,"M") || eq(x,name,"M1") || eq(x,name,"R") ||
		eq(x,name,"coproc") || eq(x,name,"op0") || eq(x,name,"op1") ||
		eq(x,name,"op2") || eq(x,name,"opc1") || eq(x,name,"opc2") ||
		eq(x,name,"reg");
}
static int add_group_fragment(struct import *x, uint32_t group_index,
			      uint32_t kind, uint32_t payload_index,
			      size_t offset, size_t length)
{
	struct tcti_register_selector_group_fragment fragment = {
		.kind = kind, .payload_index = payload_index,
		.source_offset = offset, .source_length = length,
	};
	if (grow((void **)&x->m->selector_group_fragments,
		 &x->m->selector_group_fragment_capacity,
		 x->m->selector_group_fragment_count + 1,
		 sizeof(fragment), x->allocated)) {
		fail(x->e, TCTI_REGISTER_MODEL_NO_MEMORY, offset,
		     "cannot retain group selector fragment");
		return -1;
	}
	if (!x->m->selector_groups[group_index].fragment_count)
		x->m->selector_groups[group_index].first_fragment =
			(uint32_t)x->m->selector_group_fragment_count;
	x->m->selector_group_fragments[x->m->selector_group_fragment_count++] = fragment;
	x->m->selector_groups[group_index].fragment_count++;
	return 0;
}
static int add_selector_payload(struct import *x, int object, uint32_t selector_index)
{
	struct tcti_register_system_selector *selector =
		&x->m->system_selectors[selector_index];
	int value = member(x, object, "value");
	if (eq(x, member(x, object, "_type"), "Values.Value")) {
		struct tcti_register_selector_literal literal = {
			.selector_index = selector_index,
			.source_offset = value >= 0 ? x->t[value].start : 0,
			.source_length = value >= 0 ? x->t[value].end - x->t[value].start : 0,
		};
		if (value < 0 || x->t[value].kind != STR) {
			fail(x->e, TCTI_REGISTER_MODEL_INVALID_SOURCE, x->t[object].start,
			     "Values.Value selector lacks string value");
			return -1;
		}
		if (grow((void **)&x->m->selector_literals,
			 &x->m->selector_literal_capacity,
			 x->m->selector_literal_count + 1, sizeof(literal), x->allocated))
			goto nomem;
		selector->value_kind = TCTI_REGISTER_SELECTOR_VALUE_LITERAL;
		selector->payload_index = (uint32_t)x->m->selector_literal_count;
		x->m->selector_literals[x->m->selector_literal_count++] = literal;
		return 0;
	}
	if (eq(x, member(x, object, "_type"), "Values.EquationValue")) {
		int slices = member(x, object, "slice");
		struct tcti_register_selector_equation equation = {
			.selector_index = selector_index, .identifier = dup(x, value),
			.first_slice = (uint32_t)x->m->selector_slice_count,
			.source_offset = x->t[object].start,
			.source_length = x->t[object].end - x->t[object].start,
		};
		size_t index;
		if (!equation.identifier || slices < 0 || x->t[slices].kind != ARR ||
		    grow((void **)&x->m->selector_equations,
			 &x->m->selector_equation_capacity,
			 x->m->selector_equation_count + 1, sizeof(equation), x->allocated)) {
			free(equation.identifier); goto nomem;
		}
		selector->value_kind = TCTI_REGISTER_SELECTOR_VALUE_EQUATION;
		selector->payload_index = (uint32_t)x->m->selector_equation_count;
		x->m->selector_equations[x->m->selector_equation_count++] = equation;
		for (index = 0; index < x->t[slices].size; index++) {
			int range = elem(x, slices, index);
			uint32_t start, width;
			struct tcti_register_selector_slice slice;
			if (range < 0 || !eq(x, member(x, range, "_type"), "Range") ||
			    number(x, member(x, range, "start"), &start) ||
			    number(x, member(x, range, "width"), &width) || !width ||
			    grow((void **)&x->m->selector_slices,
				 &x->m->selector_slice_capacity,
				 x->m->selector_slice_count + 1, sizeof(slice), x->allocated)) goto nomem;
			slice = (struct tcti_register_selector_slice){
				.equation_index = selector->payload_index, .start = start, .width = width,
				.source_offset = x->t[range].start,
				.source_length = x->t[range].end - x->t[range].start,
			};
			x->m->selector_slices[x->m->selector_slice_count++] = slice;
			x->m->selector_equations[selector->payload_index].slice_count++;
		}
		return 0;
	}
	if (eq(x, member(x, object, "_type"), "Values.Group")) {
		struct tcti_register_selector_group group = {
			.selector_index = selector_index,
			.first_fragment = UINT32_MAX,
			.source_offset = x->t[object].start,
			.source_length = x->t[object].end - x->t[object].start,
		};
		if (grow((void **)&x->m->selector_groups,
			 &x->m->selector_group_capacity,
			 x->m->selector_group_count + 1, sizeof(group), x->allocated)) goto nomem;
		selector->value_kind = TCTI_REGISTER_SELECTOR_VALUE_GROUP;
		selector->payload_index = (uint32_t)x->m->selector_group_count;
		x->m->selector_groups[x->m->selector_group_count++] = group;
		{
			const char *text = selector->value;
			size_t position = 0, length = text ? strlen(text) : 0;
			int value_token = member(x, object, "value");
			if (!text || value_token < 0 || x->t[value_token].kind != STR)
				goto unsupported;
			while (position < length) {
				size_t start = position;
				if (text[position] == ':') { position++; continue; }
				if (text[position] == '\'') {
					struct tcti_register_selector_literal literal;
					while (++position < length && text[position] != '\'')
						if (text[position] != '0' && text[position] != '1') goto unsupported;
					if (position == length) goto unsupported;
					literal = (struct tcti_register_selector_literal){
						.selector_index = selector_index,
						.source_offset = x->t[value_token].start + 1 + start,
						.source_length = position - start + 1,
					};
					if (grow((void **)&x->m->selector_literals,
						 &x->m->selector_literal_capacity,
						 x->m->selector_literal_count + 1,
						 sizeof(literal), x->allocated)) goto nomem;
					if (add_group_fragment(x, selector->payload_index,
						 TCTI_REGISTER_SELECTOR_VALUE_LITERAL,
						 (uint32_t)x->m->selector_literal_count,
						 literal.source_offset, literal.source_length)) return -1;
					x->m->selector_literals[x->m->selector_literal_count++] = literal;
					position++;
				} else {
					struct tcti_register_selector_equation equation;
					struct tcti_register_selector_slice slice;
					size_t name_start = position, name_end, number_start;
					uint32_t high, low;
					while (position < length && ((text[position] >= 'A' && text[position] <= 'Z') || (text[position] >= 'a' && text[position] <= 'z') || (text[position] >= '0' && text[position] <= '9') || text[position] == '_')) position++;
					if (position == name_start || position >= length) goto unsupported; name_end = position; if (text[position++] != '[') goto unsupported;
					number_start = position; while (position < length && text[position] >= '0' && text[position] <= '9') position++;
					if (number_start == position || position >= length) goto unsupported;
					high = (uint32_t)strtoul(text + number_start, NULL, 10);
					if (text[position] == ']') { low = high; position++; }
					else { if (text[position++] != ':') goto unsupported; number_start = position; while (position < length && text[position] >= '0' && text[position] <= '9') position++; if (number_start == position || position >= length || text[position++] != ']') goto unsupported; low = (uint32_t)strtoul(text + number_start, NULL, 10); }
					if (high < low) goto unsupported;
					equation = (struct tcti_register_selector_equation){ .selector_index=selector_index, .identifier=malloc(name_end - name_start + 1), .first_slice=(uint32_t)x->m->selector_slice_count, .slice_count=1, .source_offset=x->t[value_token].start+1+start, .source_length=position-start };
					if (!equation.identifier || grow((void **)&x->m->selector_equations,&x->m->selector_equation_capacity,x->m->selector_equation_count+1,sizeof(equation),x->allocated) || grow((void **)&x->m->selector_slices,&x->m->selector_slice_capacity,x->m->selector_slice_count+1,sizeof(slice),x->allocated)) { free(equation.identifier); goto nomem; }
					memcpy(equation.identifier,text+name_start,name_end-name_start); equation.identifier[name_end-name_start]=0;
					slice=(struct tcti_register_selector_slice){ .equation_index=(uint32_t)x->m->selector_equation_count,.start=low,.width=high-low+1,.source_offset=equation.source_offset,.source_length=equation.source_length };
					if (add_group_fragment(x,selector->payload_index,TCTI_REGISTER_SELECTOR_VALUE_EQUATION,(uint32_t)x->m->selector_equation_count,equation.source_offset,equation.source_length)) { free(equation.identifier); return -1; }
					x->m->selector_equations[x->m->selector_equation_count++]=equation; x->m->selector_slices[x->m->selector_slice_count++]=slice;
				}
			}
		}
		return 0;
	}
	unsupported:
	fail(x->e, TCTI_REGISTER_MODEL_UNSUPPORTED_TYPE, x->t[object].start,
	     "unsupported Values.Group selector grammar");
	return -1;
	fail(x->e, TCTI_REGISTER_MODEL_UNSUPPORTED_TYPE, x->t[object].start,
	     "unsupported system selector payload");
	return -1;
nomem:
	fail(x->e, TCTI_REGISTER_MODEL_NO_MEMORY, x->t[object].start,
	     "cannot retain typed selector payload");
	return -1;
}
static int addselector(struct import *x, int o, uint32_t encoding_index, int name)
{
	int type = member(x, o, "_type");
	struct tcti_register_system_selector selector = {
		.name = dup(x, name), .type = dup(x, type), .value = domain_text(x, member(x, o, "value")),
		.value_kind = TCTI_REGISTER_SELECTOR_VALUE_NONE,
		.payload_index = UINT32_MAX, .value_expression = UINT32_MAX,
		.encoding_index = encoding_index, .source_offset = x->t[o].start,
		.source_length = x->t[o].end - x->t[o].start,
	};
	if (!selector.name || !selector.type || !allowed_selector_name(x,name) ||
		(!eq(x,type,"Values.Value") && !eq(x,type,"Values.EquationValue") &&
		 !eq(x,type,"Values.Group")) || grow((void **)&x->m->system_selectors,
		&x->m->system_selector_capacity, x->m->system_selector_count + 1,
		sizeof(selector), x->allocated)) {
		free(selector.name); free(selector.type); free(selector.value);
		fail(x->e, TCTI_REGISTER_MODEL_NO_MEMORY, x->t[o].start, "cannot store system selector");
		return -1;
	}
	{
		uint32_t selector_index = (uint32_t)x->m->system_selector_count;
		x->m->system_selectors[x->m->system_selector_count++] = selector;
		return add_selector_payload(x, o, selector_index);
	}
}
static int addsystem_encoding(struct import *x, int o, uint32_t accessor_index)
{
	int fields = member(x, o, "encodings");
	int asmvalue = member(x, o, "asmvalue");
	struct tcti_register_system_encoding encoding = {
		.type = dup(x, member(x, o, "_type")),
		.asmvalue = dup(x, asmvalue), .accessor_index = accessor_index,
		.asmvalue_is_null = null_value(x, asmvalue),
		.source_offset = x->t[o].start, .source_length = x->t[o].end - x->t[o].start,
	};
	size_t child, z;
	uint32_t index;
	if (asmvalue < 0 || !eq(x, member(x, o, "_type"), "Encoding") ||
	    (!encoding.asmvalue_is_null && x->t[asmvalue].kind != STR)) {
		free(encoding.type); free(encoding.asmvalue);
		fail(x->e, TCTI_REGISTER_MODEL_INVALID_SOURCE, x->t[o].start,
		     "invalid Encoding assembly value");
		return -1;
	}
	if (encoding.asmvalue_is_null) {
		if (*x->allocated >= MAX_ALLOC || !(encoding.asmvalue = malloc(1))) {
			free(encoding.type);
			fail(x->e, TCTI_REGISTER_MODEL_NO_MEMORY, x->t[o].start,
			     "cannot retain null system assembly value");
			return -1;
		}
		encoding.asmvalue[0] = 0;
		(*x->allocated)++;
	}
	if (fields < 0 || x->t[fields].kind != OBJ ||
		grow((void **)&x->m->system_encodings, &x->m->system_encoding_capacity,
		x->m->system_encoding_count + 1, sizeof(encoding), x->allocated)) {
		free(encoding.type); free(encoding.asmvalue);
		fail(x->e, TCTI_REGISTER_MODEL_INVALID_SOURCE, x->t[o].start, "invalid system encoding");
		return -1;
	}
	index = (uint32_t)x->m->system_encoding_count;
	x->m->system_encodings[x->m->system_encoding_count++] = encoding;
	child = (size_t)fields + 1;
	for (z = 0; z < x->t[fields].size; z++) {
		size_t value = x->t[child].next;
		if (addselector(x, (int)value, index, (int)child)) return -1;
		child = x->t[value].next;
	}
	return 0;
}
static int accessor_key_allowed(const struct import *x, int type, int key)
{
	if (eq(x, key, "_type") || eq(x, key, "access") ||
	    eq(x, key, "condition"))
		return 1;
	if (eq(x, type, "Accessors.SystemAccessorArray"))
		return eq(x, key, "encoding") || eq(x, key, "index_variable") ||
			eq(x, key, "indexes") || eq(x, key, "name");
	if (eq(x, type, "Accessors.BlockAccess"))
		return eq(x, key, "offset") || eq(x, key, "references");
	if (eq(x, type, "Accessors.BlockAccessArray"))
		return eq(x, key, "index_variable") || eq(x, key, "indexes") ||
			eq(x, key, "offset") || eq(x, key, "references");
	if (eq(x, type, "Accessors.ImplementationDefinedOffsetAccessorArray"))
		return eq(x, key, "index_variable") || eq(x, key, "indexes") ||
			eq(x, key, "offset");
	if (eq(x, type, "Accessors.ExternalDebug"))
		return eq(x, key, "component") || eq(x, key, "instance") ||
			eq(x, key, "offset") || eq(x, key, "power_domain") ||
			eq(x, key, "range");
	if (eq(x, type, "Accessors.MemoryMapped"))
		return eq(x, key, "component") || eq(x, key, "frame") ||
			eq(x, key, "instance") || eq(x, key, "offset") ||
			eq(x, key, "power_domain") || eq(x, key, "range");
	return 0;
}

static int accessor_has_exact_keys(const struct import *x, int object, int type)
{
	size_t index;
	int required = 3;

	if (eq(x, type, "Accessors.SystemAccessorArray"))
		required += 4;
	else if (eq(x, type, "Accessors.BlockAccess"))
		required += 2;
	else if (eq(x, type, "Accessors.BlockAccessArray"))
		required += 4;
	else if (eq(x, type, "Accessors.ImplementationDefinedOffsetAccessorArray"))
		required += 3;
	else if (eq(x, type, "Accessors.ExternalDebug"))
		required += 5;
	else if (eq(x, type, "Accessors.MemoryMapped"))
		required += 6;
	else
		return -1;
	if (x->t[object].kind != OBJ || x->t[object].size != (size_t)required)
		return -1;
	for (index = 0; index < x->t[object].size; index++) {
		size_t key = (size_t)object + 1;
		size_t prior;
		for (prior = 0; prior < index; prior++)
			key = x->t[x->t[key].next].next;
		if (!accessor_key_allowed(x, type, (int)key))
			return -1;
	}
	return 0;
}

static int accessor_range(struct import *x, int token,
			  struct tcti_register_accessor *accessor)
{
	uint32_t start, width;

	if (token < 0)
		return -1;
	if (null_value(x, token)) {
		accessor->range_is_null = 1;
		return 0;
	}
	if (x->t[token].kind != OBJ ||
	    !eq(x, member(x, token, "_type"), "Range") ||
	    x->t[token].size != 3 ||
	    number(x, member(x, token, "start"), &start) ||
	    number(x, member(x, token, "width"), &width) || !width)
		return -1;
	accessor->range_start = start;
	accessor->range_width = width;
	return 0;
}

static int accessor_index_range(struct import *x, int token,
				struct tcti_register_accessor *accessor)
{
	int range;
	uint32_t start, width;

	if (token < 0 || x->t[token].kind != ARR || x->t[token].size != 1 ||
	    (range = elem(x, token, 0)) < 0 || x->t[range].kind != OBJ ||
	    !eq(x, member(x, range, "_type"), "Range") ||
	    x->t[range].size != 3 ||
	    number(x, member(x, range, "start"), &start) ||
	    number(x, member(x, range, "width"), &width) || !width)
		return -1;
	accessor->index_start = start;
	accessor->index_width = width;
	return 0;
}

static int accessor_text(struct import *x, int token, int permit_null,
			 char **out, uint32_t *is_null)
{
	*is_null = 0;
	if (token < 0)
		return -1;
	if (null_value(x, token)) {
		if (!permit_null)
			return -1;
		*is_null = 1;
		return 0;
	}
	if (x->t[token].kind != STR || !(*out = dup(x, token)))
		return -1;
	return 0;
}

static int add_accessor_offset_expression(struct import *x,
					 uint32_t accessor_index, int token)
{
	struct tcti_register_accessor_offset_expression item;
	uint32_t expression;

	if (token < 0 || x->t[token].kind != OBJ || !tree_object(x, token) ||
	    add_expression(x, token, UINT32_MAX, -1, &expression))
		return -1;
	item = (struct tcti_register_accessor_offset_expression){
		.accessor_index = accessor_index,
		.expression_index = expression,
		.source_offset = x->t[token].start,
		.source_length = x->t[token].end - x->t[token].start,
	};
	if (grow((void **)&x->m->accessor_offset_expressions,
		 &x->m->accessor_offset_expression_capacity,
		 x->m->accessor_offset_expression_count + 1, sizeof(item),
		 x->allocated))
		return -1;
	if (!x->m->accessors[accessor_index].offset_expression_count)
		x->m->accessors[accessor_index].first_offset_expression =
			(uint32_t)x->m->accessor_offset_expression_count;
	x->m->accessor_offset_expressions[x->m->accessor_offset_expression_count++] = item;
	x->m->accessors[accessor_index].offset_expression_count++;
	return 0;
}

static int accessor_offsets(struct import *x, uint32_t accessor_index, int token,
			    int expect_array)
{
	size_t index;

	if (token < 0 || (expect_array && x->t[token].kind != ARR) ||
	    (!expect_array && x->t[token].kind != OBJ))
		return -1;
	if (expect_array) {
		if (!x->t[token].size)
			return -1;
		for (index = 0; index < x->t[token].size; index++)
			if (add_accessor_offset_expression(x, accessor_index,
					elem(x, token, index)))
				return -1;
		return 0;
	}
	return add_accessor_offset_expression(x, accessor_index, token);
}

static int addaccessor(struct import *x, int o, int type, uint32_t reg)
{
	int condition = member(x, o, "condition"), access = member(x, o, "access");
	int encoding = member(x, o, "encoding");
	struct tcti_register_accessor a = {
		.type = dup(x, type), .name = dup(x, member(x, o, "name")),
		.index_variable = dup(x, member(x, o, "index_variable")),
		.register_index = reg,
		.index_start = UINT32_MAX, .index_width = 0,
		.range_start = UINT32_MAX, .range_width = 0,
		.first_offset_expression = UINT32_MAX,
		.references_expression = UINT32_MAX,
		.first_system_encoding = UINT32_MAX, .system_encoding_count = 0,
		.condition_expression = UINT32_MAX, .access_expression = UINT32_MAX,
		.condition_offset = condition >= 0 ? x->t[condition].start : 0,
		.condition_length = condition >= 0 ? x->t[condition].end - x->t[condition].start : 0,
		.access_offset = access >= 0 ? x->t[access].start : 0,
		.access_length = access >= 0 ? x->t[access].end - x->t[access].start : 0,
		.encoding_offset = encoding >= 0 ? x->t[encoding].start : 0,
		.encoding_length = encoding >= 0 ? x->t[encoding].end - x->t[encoding].start : 0,
		.source_offset = x->t[o].start, .source_length = x->t[o].end - x->t[o].start,
	};
	if (!allowed_accessor(x, type) || !a.type) {
		free(a.type); free(a.name); free(a.index_variable);
		fail(x->e, TCTI_REGISTER_MODEL_UNSUPPORTED_TYPE, x->t[o].start, "unsupported accessor");
		return -1;
	}
	if (!eq(x, type, "Accessors.SystemAccessor") &&
	    accessor_has_exact_keys(x, o, type)) {
		free(a.type); free(a.name); free(a.index_variable);
		fail(x->e, TCTI_REGISTER_MODEL_INVALID_SOURCE, x->t[o].start,
		     "accessor has missing, extra, or unsupported metadata");
		return -1;
	}
	if (!eq(x, type, "Accessors.SystemAccessor") &&
	    (!tree_object(x, condition) || !tree_object(x, access))) {
		free(a.type); free(a.name); free(a.index_variable);
		fail(x->e, TCTI_REGISTER_MODEL_INVALID_SOURCE, x->t[o].start,
		     "accessor lacks typed condition or access");
		return -1;
	}
	if (grow((void **)&x->m->accessors, &x->m->accessor_capacity,
		 x->m->accessor_count + 1, sizeof(a), x->allocated)) {
		free(a.type); free(a.name); free(a.index_variable);
		fail(x->e, TCTI_REGISTER_MODEL_NO_MEMORY, x->t[o].start, "cannot store accessor");
		return -1;
	}
	{
		uint32_t index = (uint32_t)x->m->accessor_count;
		int encodings = member(x, o, "encoding");
		x->m->accessors[x->m->accessor_count++] = a;
		if (eq(x, type, "Accessors.SystemAccessorArray") ||
		    eq(x, type, "Accessors.BlockAccessArray") ||
		    eq(x, type, "Accessors.ImplementationDefinedOffsetAccessorArray")) {
			if (!x->m->accessors[index].index_variable ||
			    accessor_index_range(x, member(x, o, "indexes"),
					 &x->m->accessors[index])) {
				fail(x->e, TCTI_REGISTER_MODEL_INVALID_SOURCE,
				     x->t[o].start, "invalid accessor index domain");
				return -1;
			}
		}
		if (eq(x, type, "Accessors.SystemAccessorArray") &&
		    (!x->m->accessors[index].name || encodings < 0 ||
		     x->t[encodings].kind != ARR || !x->t[encodings].size)) {
			fail(x->e, TCTI_REGISTER_MODEL_INVALID_SOURCE,
			     x->t[o].start, "invalid SystemAccessorArray encoding metadata");
			return -1;
		}
		if (eq(x, type, "Accessors.BlockAccess") ||
		    eq(x, type, "Accessors.BlockAccessArray") ||
		    eq(x, type, "Accessors.ImplementationDefinedOffsetAccessorArray")) {
			if (accessor_offsets(x, index, member(x, o, "offset"), 1)) {
				fail(x->e, TCTI_REGISTER_MODEL_INVALID_SOURCE,
				     x->t[o].start, "invalid accessor offset expression array");
				return -1;
			}
		}
		if (eq(x, type, "Accessors.MemoryMapped") ||
		    eq(x, type, "Accessors.ExternalDebug")) {
			struct tcti_register_accessor *stored = &x->m->accessors[index];
			if (accessor_text(x, member(x, o, "component"), 0,
					  &stored->component, &stored->component_is_null) ||
			    accessor_text(x, member(x, o, "instance"), 1,
					  &stored->instance, &stored->instance_is_null) ||
			    accessor_text(x, member(x, o, "power_domain"), 1,
					  &stored->power_domain, &stored->power_domain_is_null) ||
			    (eq(x, type, "Accessors.MemoryMapped") &&
			     accessor_text(x, member(x, o, "frame"), 1,
					   &stored->frame, &stored->frame_is_null)) ||
			    accessor_range(x, member(x, o, "range"), stored) ||
			    accessor_offsets(x, index, member(x, o, "offset"), 0)) {
				fail(x->e, TCTI_REGISTER_MODEL_INVALID_SOURCE,
				     x->t[o].start, "invalid mapped accessor metadata");
				return -1;
			}
		}
		if (eq(x, type, "Accessors.BlockAccess") ||
		    eq(x, type, "Accessors.BlockAccessArray")) {
			int references = member(x, o, "references");
			if (references < 0 || x->t[references].kind != OBJ ||
			    !tree_object(x, references) ||
			    add_expression(x, references, UINT32_MAX, -1,
				&x->m->accessors[index].references_expression)) {
				fail(x->e, TCTI_REGISTER_MODEL_INVALID_SOURCE,
				     x->t[o].start, "invalid block accessor reference");
				return -1;
			}
		}
		if (condition >= 0 && ( !tree_object(x, condition) ||
		    add_expression(x, condition, UINT32_MAX, -1,
		    &x->m->accessors[index].condition_expression))) { fail(x->e,TCTI_REGISTER_MODEL_UNSUPPORTED_TYPE,x->t[condition].start,"unsupported accessor condition"); return -1; }
		if (!null_value(x, access) && access >= 0 && ( !tree_object(x, access) ||
		    add_expression(x, access, UINT32_MAX, -1,
		    &x->m->accessors[index].access_expression))) { fail(x->e,TCTI_REGISTER_MODEL_UNSUPPORTED_TYPE,x->t[access].start,"unsupported accessor action"); return -1; }
		if (encodings >= 0 && x->t[encodings].kind == ARR) {
			size_t z;
			x->m->accessors[index].first_system_encoding = (uint32_t)x->m->system_encoding_count;
			for (z = 0; z < x->t[encodings].size; z++)
				if (addsystem_encoding(x, elem(x, encodings, z), index)) return -1;
			x->m->accessors[index].system_encoding_count =
				(uint32_t)x->m->system_encoding_count - x->m->accessors[index].first_system_encoding;
		}
	}
	return 0;
}
static int addfield(struct import *x, int o, int type, uint32_t width, uint32_t reg, uint32_t parent, int wrapper_condition, int wrapper_object, uint32_t *index) { int condition=member(x,o,"condition"), branches=member(x,o,"fields"); struct tcti_field_identity f = { .name=dup(x,member(x,o,"name")), .type=dup(x,type), .width=width, .register_index=reg, .parent_field=parent, .fieldset_index=(uint32_t)(x->m->fieldset_count-1), .first_range=(uint32_t)x->m->range_count, .condition_offset=condition>=0?x->t[condition].start:0, .condition_length=condition>=0?x->t[condition].end-x->t[condition].start:0, .wrapper_condition_offset=wrapper_condition>=0?x->t[wrapper_condition].start:0, .wrapper_condition_length=wrapper_condition>=0?x->t[wrapper_condition].end-x->t[wrapper_condition].start:0, .branch_offset=branches>=0?x->t[branches].start:0, .branch_length=branches>=0?x->t[branches].end-x->t[branches].start:0, .source_offset=x->t[o].start, .source_length=x->t[o].end-x->t[o].start }; if (!f.type || grow((void **)&x->m->fields, &x->m->field_capacity, x->m->field_count + 1, sizeof(f),x->allocated)) { free(f.name); free(f.type); fail(x->e,TCTI_REGISTER_MODEL_NO_MEMORY,x->t[o].start,"cannot store field identity"); return -1; } *index=(uint32_t)x->m->field_count; x->m->fields[x->m->field_count++] = f; if (wrapper_object >= 0) { struct tcti_conditional_field_branch b = { .conditional_field_index=parent, .field_index=*index, .condition_offset=x->t[wrapper_condition].start, .condition_length=x->t[wrapper_condition].end-x->t[wrapper_condition].start, .source_offset=x->t[wrapper_object].start, .source_length=x->t[wrapper_object].end-x->t[wrapper_object].start }; if (parent==UINT32_MAX || grow((void **)&x->m->field_branches,&x->m->field_branch_capacity,x->m->field_branch_count+1,sizeof(b),x->allocated)) { fail(x->e,TCTI_REGISTER_MODEL_NO_MEMORY,x->t[o].start,"cannot store conditional-field branch"); return -1; } x->m->field_branches[x->m->field_branch_count++]=b; } return 0; }
static int field_condition_roots(struct import *x, int object, uint32_t field,
				 int wrapper_condition)
{
	int condition = member(x, object, "condition");
	uint32_t root;
	x->m->fields[field].condition_expression = UINT32_MAX;
	x->m->fields[field].wrapper_condition_expression = UINT32_MAX;

	if (condition >= 0) {
		if (x->t[condition].kind != OBJ ||
		    add_expression(x, condition, UINT32_MAX, -1, &root)) {
			fail(x->e, TCTI_REGISTER_MODEL_UNSUPPORTED_TYPE,
			     x->t[condition].start, "unsupported field condition");
			return -1;
		}
		x->m->fields[field].condition_expression = root;
	}
	if (wrapper_condition >= 0) {
		if (x->t[wrapper_condition].kind != OBJ ||
		    add_expression(x, wrapper_condition, UINT32_MAX, -1, &root)) {
			fail(x->e, TCTI_REGISTER_MODEL_UNSUPPORTED_TYPE,
			     x->t[wrapper_condition].start, "unsupported conditional field wrapper");
			return -1;
		}
		x->m->fields[field].wrapper_condition_expression = root;
		{
			size_t branch;
			for (branch = 0; branch < x->m->field_branch_count; branch++)
				if (x->m->field_branches[branch].field_index == field &&
				    x->m->field_branches[branch].condition_offset ==
				    x->t[wrapper_condition].start) {
					x->m->field_branches[branch].condition_expression = root;
					break;
				}
		}
	}
	return 0;
}
static int allowed_field(const struct import *x, int t) { return eq(x,t,"Fields.Array") || eq(x,t,"Fields.ConditionalField") || eq(x,t,"Fields.ConstantField") || eq(x,t,"Fields.Dynamic") || eq(x,t,"Fields.Field") || eq(x,t,"Fields.ImplementationDefined") || eq(x,t,"Fields.Reserved") || eq(x,t,"Fields.Vector"); }
static int allowed_domain(const struct import *x, int t) { return eq(x,t,"Values.Value") || eq(x,t,"Values.ValueRange") || eq(x,t,"Values.Link") || eq(x,t,"Values.ConditionalValue"); }
static int addlink(struct import *x, int o, uint32_t domain_index)
{
	size_t z, child;
	if (o < 0 || x->t[o].kind != OBJ)
		return 0;
	child = (size_t)o + 1;
	for (z = 0; z < x->t[o].size; z++) {
		size_t value = x->t[child].next;
		struct tcti_register_link link = {
			.key = dup(x, (int)child), .value = dup(x, (int)value),
			.domain_index = domain_index, .source_offset = x->t[child].start,
			.source_length = x->t[value].end - x->t[child].start,
		};
		if (!link.key || !link.value || grow((void **)&x->m->links, &x->m->link_capacity,
			 x->m->link_count + 1, sizeof(link), x->allocated)) {
			free(link.key); free(link.value);
			fail(x->e, TCTI_REGISTER_MODEL_NO_MEMORY, x->t[child].start, "cannot store Values.Link key");
			return -1;
		}
		x->m->links[x->m->link_count++] = link;
		child = x->t[value].next;
	}
	return 0;
}
static int addvalueset(struct import *x, int o, uint32_t field, uint32_t *index, int constraint)
{
	int type = member(x, o, "_type");
	struct tcti_register_valueset v = {
		.type = dup(x, type), .field_index = field,
		.source_offset = x->t[o].start, .source_length = x->t[o].end - x->t[o].start,
	};
	if (!v.type || (!eq(x, type, "Valuesets.Values") &&
		!eq(x, type, "Valuesets.ImplementationDefined"))) {
		free(v.type);
		fail(x->e, TCTI_REGISTER_MODEL_UNSUPPORTED_TYPE, x->t[o].start, "unsupported Valueset");
		return -1;
	}
	if (grow((void **)&x->m->valuesets, &x->m->valueset_capacity,
		 x->m->valueset_count + 1, sizeof(v), x->allocated)) {
		free(v.type);
		fail(x->e, TCTI_REGISTER_MODEL_NO_MEMORY, x->t[o].start, "cannot store Valueset");
		return -1;
	}
	*index = (uint32_t)x->m->valueset_count;
	x->m->valuesets[x->m->valueset_count++] = v;
	if (!constraint) x->m->top_level_valueset_count++;
	return 0;
}
static int domain(struct import *x, int o, uint32_t field, uint32_t valueset,
		  uint32_t parent, int constraint);
static int add_constraint_item(struct import *x, uint32_t constraint_index,
			       uint32_t valueset_index, uint32_t domain_index,
			       int token)
{
	struct tcti_register_constraint_item item = {
		.constraint_index = constraint_index,
		.valueset_index = valueset_index,
		.domain_index = domain_index,
		.source_offset = x->t[token].start,
		.source_length = x->t[token].end - x->t[token].start,
	};
	if (grow((void **)&x->m->constraint_items,
		 &x->m->constraint_item_capacity,
		 x->m->constraint_item_count + 1, sizeof(item), x->allocated)) {
		fail(x->e, TCTI_REGISTER_MODEL_NO_MEMORY, x->t[token].start,
		     "cannot retain constraint item");
		return -1;
	}
	x->m->constraint_items[x->m->constraint_item_count++] = item;
	return 0;
}
static int addconstraint(struct import *x, int token, uint32_t field)
{
	struct tcti_register_constraint c = {
		.kind = TCTI_REGISTER_CONSTRAINT_NULL, .field_index = field,
		.valueset_index = UINT32_MAX, .first_item = UINT32_MAX,
		.source_offset = x->t[token].start,
		.source_length = x->t[token].end - x->t[token].start,
	};
	if (null_value(x, token)) c.kind = TCTI_REGISTER_CONSTRAINT_NULL;
	else if (x->t[token].kind == ARR) c.kind = TCTI_REGISTER_CONSTRAINT_ARRAY;
	else if (x->t[token].kind == OBJ && eq(x, member(x, token, "_type"), "Valuesets.Values")) {
		int values;
		size_t z;
		c.kind = TCTI_REGISTER_CONSTRAINT_VALUESET;
		if (addvalueset(x, token, field, &c.valueset_index, 1)) return -1;
		values = member(x, token, "values");
		if (values < 0 || x->t[values].kind != ARR) {
			fail(x->e, TCTI_REGISTER_MODEL_INVALID_SOURCE, x->t[token].start, "constraint Valueset lacks array");
			return -1;
		}
		for (z = 0; z < x->t[values].size; z++)
			if (domain(x, elem(x, values, z), field, c.valueset_index, UINT32_MAX, 1)) return -1;
	} else {
		fail(x->e, TCTI_REGISTER_MODEL_UNSUPPORTED_TYPE, x->t[token].start, "unsupported constraint container");
		return -1;
	}
	if (c.kind == TCTI_REGISTER_CONSTRAINT_VALUESET)
		c.item_count = 1;
	else if (c.kind == TCTI_REGISTER_CONSTRAINT_ARRAY)
		c.item_count = (uint32_t)x->t[token].size;
	if (grow((void **)&x->m->constraints, &x->m->constraint_capacity,
		x->m->constraint_count + 1, sizeof(c), x->allocated)) {
		fail(x->e, TCTI_REGISTER_MODEL_NO_MEMORY, x->t[token].start, "cannot store constraint container");
		return -1;
	}
	{
		uint32_t constraint_index = (uint32_t)x->m->constraint_count;
		size_t item;
		x->m->constraints[x->m->constraint_count++] = c;
		if (c.kind == TCTI_REGISTER_CONSTRAINT_VALUESET) {
			if (add_constraint_item(x, constraint_index, c.valueset_index,
					UINT32_MAX, token))
				return -1;
		} else if (c.kind == TCTI_REGISTER_CONSTRAINT_ARRAY) {
			for (item = 0; item < x->t[token].size; item++)
				if (add_constraint_item(x, constraint_index, UINT32_MAX,
						UINT32_MAX, elem(x, token, item)))
					return -1;
		}
		x->m->constraints[constraint_index].first_item =
			c.kind == TCTI_REGISTER_CONSTRAINT_NULL ? UINT32_MAX :
			(uint32_t)(x->m->constraint_item_count -
			 x->m->constraints[constraint_index].item_count);
		x->m->constraints[constraint_index].item_count =
			(uint32_t)(x->m->constraint_item_count -
			 (c.kind == TCTI_REGISTER_CONSTRAINT_NULL ?
			  x->m->constraint_item_count :
			  x->m->constraints[constraint_index].first_item));
	}
	return 0;
}
static int domain(struct import *x, int o, uint32_t field, uint32_t valueset,
		  uint32_t parent, int constraint)
{
	int t = member(x,o,"_type"), nested, condition=member(x,o,"condition"), links=member(x,o,"links"); struct tcti_value_domain d = { .type=dup(x,t), .value=domain_text(x,member(x,o,"value")), .end=domain_text(x,member(x,o,"end")), .link=links>=0?dup(x,member(x,links,"ISS")):NULL, .meaning=domain_text(x,member(x,o,"meaning")), .field_index=field, .parent_domain=parent, .condition_expression=UINT32_MAX, .first_child_domain=UINT32_MAX, .nested_valueset_index=UINT32_MAX, .condition_offset=condition>=0?x->t[condition].start:0, .condition_length=condition>=0?x->t[condition].end-x->t[condition].start:0, .source_offset=x->t[o].start, .source_length=x->t[o].end-x->t[o].start, .start=domain_text(x,member(x,o,"start")), .valueset_index=valueset };
	if (!allowed_domain(x,t)) { free(d.type); free(d.value); free(d.end); free(d.link); free(d.start); fail(x->e,TCTI_REGISTER_MODEL_UNSUPPORTED_TYPE,x->t[o].start,"unsupported Valuesets.Values domain"); return -1; }
	if (eq(x,t,"Values.ConditionalValue")) {
		nested = member(x,o,"values");
		if (nested < 0 || x->t[nested].kind != OBJ ||
		    !eq(x, member(x,nested,"_type"), "Valuesets.Values") ||
		    member(x,nested,"values") < 0 ||
		    x->t[member(x,nested,"values")].kind != ARR) {
			free(d.type); free(d.value); free(d.end); free(d.link); free(d.start);
			fail(x->e,TCTI_REGISTER_MODEL_INVALID_SOURCE,x->t[o].start,"ConditionalValue lacks Valuesets.Values");
			return -1;
		}
		d.branch_offset = x->t[nested].start;
		d.branch_length = x->t[nested].end - x->t[nested].start;
	}
	if (!d.type || grow((void **)&x->m->domains,&x->m->domain_capacity,x->m->domain_count+1,sizeof(d),x->allocated)) { free(d.type); free(d.value); free(d.end); free(d.link); free(d.start); fail(x->e,TCTI_REGISTER_MODEL_NO_MEMORY,x->t[o].start,"cannot store value domain"); return -1; }
	{ uint32_t index=(uint32_t)x->m->domain_count; x->m->domains[x->m->domain_count++] = d;
		if (parent != UINT32_MAX) {
			struct tcti_value_domain *owner = &x->m->domains[parent];
			if (!owner->child_domain_count)
				owner->first_child_domain = index;
			owner->child_domain_count++;
		} else if (constraint) x->m->constraint_domain_count++; else x->m->top_level_domain_count++;
		if (condition >= 0) { uint32_t root; if (x->t[condition].kind != OBJ || add_expression(x,condition,UINT32_MAX,-1,&root)) { fail(x->e,TCTI_REGISTER_MODEL_UNSUPPORTED_TYPE,x->t[condition].start,"unsupported value condition"); return -1; } x->m->domains[index].condition_expression=root; }
		if (addlink(x, links, index)) return -1; if (eq(x,t,"Values.ConditionalValue")) { int values=member(x,nested,"values"); uint32_t child_valueset; size_t z; if (addvalueset(x,nested,field,&child_valueset,1)) return -1; x->m->domains[index].nested_valueset_index=child_valueset; for(z=0;z<x->t[values].size;z++) if(domain(x,elem(x,values,z),field,child_valueset,index,constraint)) return -1; } }
	return 0;
}
static int ranges(struct import *x, int a, uint32_t width, uint32_t field)
{
	size_t z; if (a < 0 || x->t[a].kind != ARR) { fail(x->e,TCTI_REGISTER_MODEL_INVALID_SOURCE,0,"field lacks rangeset"); return -1; }
	for(z=0;z<x->t[a].size;z++) { int r=elem(x,a,z); uint32_t start, bits; struct tcti_register_range item; if(r<0 || !eq(x,member(x,r,"_type"),"Range") || number(x,member(x,r,"start"),&start) || number(x,member(x,r,"width"),&bits) || !bits || start > width || bits > width-start) { fail(x->e,TCTI_REGISTER_MODEL_INVALID_SOURCE,r<0?0:x->t[r].start,"invalid Range for fieldset width"); return -1; } item=(struct tcti_register_range){start,bits,field,x->t[r].start,x->t[r].end-x->t[r].start}; if(grow((void **)&x->m->ranges,&x->m->range_capacity,x->m->range_count+1,sizeof(item),x->allocated)){fail(x->e,TCTI_REGISTER_MODEL_NO_MEMORY,x->t[r].start,"cannot store Range");return -1;} x->m->ranges[x->m->range_count++]=item; x->m->fields[field].range_count++; }
	return 0;
}
static int metadata_key_count(const struct import *x, int object,
			      const char *first, const char *second)
{
	return x->t[object].kind == OBJ && x->t[object].size == 2 &&
		member(x, object, first) >= 0 && member(x, object, second) >= 0;
}

static int add_metadata(struct import *x, int object)
{
	int meta = member(x, object, "_meta");
	int license;
	int version;
	struct tcti_register_metadata metadata;

	if (meta < 0 || x->t[meta].kind != OBJ || x->t[meta].size != 2 ||
	    member(x, meta, "license") < 0 || member(x, meta, "version") < 0) {
		fail(x->e, TCTI_REGISTER_MODEL_INVALID_SOURCE, x->t[object].start,
		     "register metadata lacks exact license and version objects");
		return -1;
	}
	license = member(x, meta, "license");
	version = member(x, meta, "version");
	if (!metadata_key_count(x, license, "copyright", "info") ||
	    x->t[member(x, license, "copyright")].kind != STR ||
	    x->t[member(x, license, "info")].kind != STR ||
	    x->t[version].kind != OBJ || x->t[version].size != 5 ||
	    member(x, version, "architecture") < 0 ||
	    member(x, version, "build") < 0 || member(x, version, "ref") < 0 ||
	    member(x, version, "schema") < 0 ||
	    member(x, version, "timestamp") < 0 ||
	    x->t[member(x, version, "architecture")].kind != STR ||
	    x->t[member(x, version, "build")].kind != STR ||
	    x->t[member(x, version, "ref")].kind != STR ||
	    x->t[member(x, version, "schema")].kind != STR ||
	    x->t[member(x, version, "timestamp")].kind != STR) {
		fail(x->e, TCTI_REGISTER_MODEL_INVALID_SOURCE, x->t[meta].start,
		     "register metadata has missing, extra, or incorrectly typed fields");
		return -1;
	}
	metadata = (struct tcti_register_metadata){
		.copyright = dup(x, member(x, license, "copyright")),
		.license_info = dup(x, member(x, license, "info")),
		.architecture = dup(x, member(x, version, "architecture")),
		.build = dup(x, member(x, version, "build")),
		.ref = dup(x, member(x, version, "ref")),
		.schema = dup(x, member(x, version, "schema")),
		.timestamp = dup(x, member(x, version, "timestamp")),
		.source_offset = x->t[meta].start,
		.source_length = x->t[meta].end - x->t[meta].start,
	};
	if (!metadata.copyright || !metadata.license_info || !metadata.architecture ||
	    !metadata.build || !metadata.ref || !metadata.schema ||
	    !metadata.timestamp ||
	    grow((void **)&x->m->metadata, &x->m->metadata_capacity,
		 x->m->metadata_count + 1, sizeof(metadata), x->allocated)) {
		free(metadata.copyright);
		free(metadata.license_info);
		free(metadata.architecture);
		free(metadata.build);
		free(metadata.ref);
		free(metadata.schema);
		free(metadata.timestamp);
		fail(x->e, TCTI_REGISTER_MODEL_NO_MEMORY, x->t[meta].start,
		     "cannot retain typed register metadata");
		return -1;
	}
	x->m->metadata[x->m->metadata_count++] = metadata;
	return 0;
}

static int metadata(struct import *x, int o)
{
	int meta=member(x,o,"_meta"), version=member(x,meta,"version");
	if(meta<0){fail(x->e,TCTI_REGISTER_MODEL_INVALID_SOURCE,x->t[o].start,"register lacks pinned metadata");return -1;}
	if(version<0||!eq(x,member(x,version,"architecture"),"vFATAp1-A")||!eq(x,member(x,version,"build"),"818")||!eq(x,member(x,version,"ref"),"2026-06_rel")||!eq(x,member(x,version,"schema"),"2.9.5")||!eq(x,member(x,version,"timestamp"),"2026-06-24 17:12:14")){fail(x->e,TCTI_REGISTER_MODEL_INVALID_SOURCE,x->t[o].start,"register metadata does not match pin");return -1;}return 0;
}

static int register_block_default_access(struct import *x, int object,
					 uint32_t register_index)
{
	int default_access = member(x, object, "default_access");
	uint32_t expression;

	if (register_index >= x->m->register_count || default_access < 0 ||
	    x->t[default_access].kind != OBJ ||
	    !eq(x, member(x, default_access, "_type"),
		"Accessors.Permission.AccessTypes.Memory.ReadWriteAccess") ||
	    add_expression(x, default_access, UINT32_MAX, -1, &expression)) {
		fail(x->e, TCTI_REGISTER_MODEL_INVALID_SOURCE, x->t[object].start,
		     "RegisterBlock lacks typed default access");
		return -1;
	}
	x->m->registers[register_index].default_access_expression = expression;
	return 0;
}

static int walk(struct import *x, int i, uint32_t width, uint32_t reg,
		uint32_t parent_field, int require_metadata, int wrapper_condition, int wrapper_object)
{
	int t; size_t z;
	if (wrapper_object >= 0 && wrapper_condition < 0) {
		fail(x->e, TCTI_REGISTER_MODEL_INVALID_SOURCE, x->t[i].start,
		     "conditional field branch lacks condition");
		return -1;
	}
	if (++x->depth > MAX_DEPTH) { fail(x->e,TCTI_REGISTER_MODEL_DEPTH_LIMIT,x->t[i].start,"source tree depth limit exceeded"); x->depth--; return -1; }
	if (x->t[i].kind == OBJ) { t=member(x,i,"_type");
		if (tree_candidate(x, i)) {
			uint32_t root;
			if (!tree_object(x, i) || add_expression(x, i, UINT32_MAX,
							     -1, &root)) {
				if (!x->e->code)
					fail(x->e, TCTI_REGISTER_MODEL_INVALID_SOURCE,
					     x->t[i].start, "invalid condition/action tree");
				goto bad;
			}
			x->depth--;
			return 0;
		}
		if(eq(x,t,"Register")||eq(x,t,"RegisterArray")||eq(x,t,"RegisterBlock")) { uint32_t next; if (((require_metadata || member(x,i,"_meta") >= 0) && (metadata(x,i) || add_metadata(x,i))) || addreg(x,i,t,reg,&next)) goto bad; reg=next; if (eq(x,t,"RegisterBlock") && register_block_default_access(x,i,reg)) goto bad; }
		if (t >= 0 && allowed_accessor(x, t) && addaccessor(x, i, t, reg)) goto bad;
		if(eq(x,t,"Fieldset")) { if(number(x,member(x,i,"width"),&width) || !width || width > 128 || reg == UINT32_MAX || addfieldset(x, i, reg, width)) { fail(x->e,TCTI_REGISTER_MODEL_INVALID_SOURCE,x->t[i].start,"invalid Fieldset"); goto bad; } if(reg!=UINT32_MAX){struct tcti_register_identity *r=&x->m->registers[reg];if(r->first_fieldset==UINT32_MAX)r->first_fieldset=(uint32_t)(x->m->fieldset_count-1);r->fieldset_count++;} }
		if (t >= 0 && x->t[t].kind == STR && x->t[t].end-x->t[t].start > 7 && !memcmp(x->s+x->t[t].start,"Fields.",7)) { int vals, constraints; uint32_t field; if(!allowed_field(x,t)) { fail(x->e,TCTI_REGISTER_MODEL_UNSUPPORTED_TYPE,x->t[i].start,"unsupported field type"); goto bad; } if(!width || reg==UINT32_MAX || addfield(x,i,t,width,reg,parent_field,wrapper_condition,wrapper_object,&field) || ranges(x,member(x,i,"rangeset"),width,field)) goto bad; vals=member(x,i,"values"); if(vals>=0) { size_t q; int a; uint32_t valueset; if (addvalueset(x, vals, field, &valueset, 0)) goto bad; a=member(x,vals,"values"); if(a<0||x->t[a].kind!=ARR) { fail(x->e,TCTI_REGISTER_MODEL_INVALID_SOURCE,x->t[vals].start,"Valueset lacks array"); goto bad; } for(q=0;q<x->t[a].size;q++) if(domain(x,elem(x,a,q),field,valueset,UINT32_MAX,0)) goto bad; } constraints=member(x,i,"constraints"); if(constraints>=0 && addconstraint(x,constraints,field)) goto bad; parent_field=field; }
		{ int wrapped=member(x,i,"field"), condition=member(x,i,"condition"), constraints=member(x,i,"constraints"); size_t c=(size_t)i+1; if (constraints >= 0 && !(t >= 0 && x->t[t].kind == STR && x->t[t].end-x->t[t].start > 7 && !memcmp(x->s+x->t[t].start,"Fields.",7)) && addconstraint(x,constraints,UINT32_MAX)) goto bad; for(z=0;z<x->t[i].size;z++) { size_t v=x->t[c].next; if(walk(x,(int)v,width,reg,parent_field,0,(int)v==wrapped?condition:-1,(int)v==wrapped?i:-1)) goto bad; c=x->t[v].next; } }
	} else if(x->t[i].kind==ARR) for(z=0;z<x->t[i].size;z++) if(walk(x,elem(x,i,z),width,reg,parent_field,require_metadata,wrapper_condition,wrapper_object)) goto bad;
	x->depth--; return 0;
bad: x->depth--; return -1;
}
static int source_object_token(const struct import *x, size_t offset)
{
	size_t left = 0, right = x->n;
	while (left < right) {
		size_t middle = left + (right - left) / 2;
		if (x->t[middle].start < offset)
			left = middle + 1;
		else
			right = middle;
	}
	while (left < x->n && x->t[left].start == offset) {
		if (x->t[left].kind == OBJ)
			return (int)left;
		left++;
	}
	return -1;
}
static int populate_field_condition_roots(struct import *x)
{
	size_t index;
	for (index = 0; index < x->m->field_count; index++) {
		struct tcti_field_identity *field = &x->m->fields[index];
		int object = source_object_token(x, field->source_offset);
		int wrapper = field->wrapper_condition_length ?
			source_object_token(x, field->wrapper_condition_offset) : -1;
		if (object < 0 || field_condition_roots(x, object,
				(uint32_t)index, wrapper))
			return -1;
	}
	return 0;
}
static int rebuild_expression_children(struct import *x)
{
	size_t index, total = 0, edge_count;
	size_t *next;
	for (index = 0; index < x->m->expression_count; index++) {
		x->m->expressions[index].first_child = UINT32_MAX;
		x->m->expressions[index].child_count = 0;
	}
	for (index = 0; index < x->m->expression_count; index++) {
		uint32_t parent = x->m->expressions[index].parent_expression;
		if (parent == UINT32_MAX)
			continue;
		if (parent >= index) {
			fail(x->e, TCTI_REGISTER_MODEL_INVALID_SOURCE, 0,
			     "expression parent does not precede child");
			return -1;
		}
		x->m->expressions[parent].child_count++;
		total++;
	}
	edge_count = total;
	if (grow((void **)&x->m->expression_children,
		 &x->m->expression_child_capacity, edge_count,
		 sizeof(*x->m->expression_children), x->allocated)) {
		fail(x->e, TCTI_REGISTER_MODEL_NO_MEMORY, 0,
		     "cannot rebuild ordered expression edges");
		return -1;
	}
	if (x->m->expression_count > SIZE_MAX / sizeof(*next) ||
	    !(next = calloc(x->m->expression_count, sizeof(*next)))) {
		fail(x->e, TCTI_REGISTER_MODEL_NO_MEMORY, 0,
		     "cannot index ordered expression edges");
		return -1;
	}
	total = 0;
	for (index = 0; index < x->m->expression_count; index++) {
		struct tcti_register_expression *expression = &x->m->expressions[index];
		if (expression->child_count) {
			expression->first_child = (uint32_t)total;
			next[index] = total;
			total += expression->child_count;
		}
	}
	x->m->expression_child_count = edge_count;
	for (index = 0; index < x->m->expression_count; index++) {
		uint32_t parent = x->m->expressions[index].parent_expression;
		if (parent != UINT32_MAX)
			x->m->expression_children[next[parent]++] = (uint32_t)index;
	}
	free(next);
	return 0;
}
struct sha256_state { uint32_t state[8]; uint64_t bytes; unsigned char block[64]; size_t used; };
static uint32_t rotate_right(uint32_t value, unsigned int amount) { return (value >> amount) | (value << (32 - amount)); }
static void sha256_block(struct sha256_state *sha, const unsigned char *block)
{
	static const uint32_t k[64] = { 0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U };
	uint32_t words[64], a,b,c,d,e,f,g,h; size_t i;
	for(i=0;i<16;i++) words[i]=((uint32_t)block[4*i]<<24)|((uint32_t)block[4*i+1]<<16)|((uint32_t)block[4*i+2]<<8)|block[4*i+3];
	for(;i<64;i++) words[i]=words[i-16]+(rotate_right(words[i-15],7)^rotate_right(words[i-15],18)^(words[i-15]>>3))+words[i-7]+(rotate_right(words[i-2],17)^rotate_right(words[i-2],19)^(words[i-2]>>10));
	a=sha->state[0];b=sha->state[1];c=sha->state[2];d=sha->state[3];e=sha->state[4];f=sha->state[5];g=sha->state[6];h=sha->state[7];
	for(i=0;i<64;i++){uint32_t one=h+(rotate_right(e,6)^rotate_right(e,11)^rotate_right(e,25))+((e&f)^((~e)&g))+k[i]+words[i];uint32_t two=(rotate_right(a,2)^rotate_right(a,13)^rotate_right(a,22))+((a&b)^(a&c)^(b&c));h=g;g=f;f=e;e=d+one;d=c;c=b;b=a;a=one+two;}
	sha->state[0]+=a;sha->state[1]+=b;sha->state[2]+=c;sha->state[3]+=d;sha->state[4]+=e;sha->state[5]+=f;sha->state[6]+=g;sha->state[7]+=h;
}
static void sha256_hex(const char *data, size_t length, char output[65])
{
	static const char hex[]="0123456789abcdef"; struct sha256_state s={{0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U},0,{0},0}; uint64_t bits=(uint64_t)length*8; size_t i;
	while(length){size_t n=64-s.used;if(n>length)n=length;memcpy(s.block+s.used,data,n);s.used+=n;data+=n;length-=n;if(s.used==64){sha256_block(&s,s.block);s.used=0;}}
	s.block[s.used++]=0x80;if(s.used>56){memset(s.block+s.used,0,64-s.used);sha256_block(&s,s.block);s.used=0;}memset(s.block+s.used,0,56-s.used);for(i=0;i<8;i++)s.block[56+i]=(unsigned char)(bits>>(56-8*i));sha256_block(&s,s.block);
	for(i=0;i<8;i++){size_t j;for(j=0;j<4;j++){unsigned char v=(unsigned char)(s.state[i]>>(24-8*j));output[8*i+2*j]=hex[v>>4];output[8*i+2*j+1]=hex[v&15];}}output[64]=0;
}
void tcti_register_model_destroy(struct tcti_register_model *m) { size_t i; if(!m)return; for(i=0;i<m->register_count;i++){free(m->registers[i].name);free(m->registers[i].type);free(m->registers[i].state);free(m->registers[i].index_variable);} for(i=0;i<m->metadata_count;i++){free(m->metadata[i].copyright);free(m->metadata[i].license_info);free(m->metadata[i].architecture);free(m->metadata[i].build);free(m->metadata[i].ref);free(m->metadata[i].schema);free(m->metadata[i].timestamp);} for(i=0;i<m->field_count;i++){free(m->fields[i].name);free(m->fields[i].type);} for(i=0;i<m->fieldset_count;i++){free(m->fieldsets[i].name);free(m->fieldsets[i].display);} for(i=0;i<m->domain_count;i++){free(m->domains[i].type);free(m->domains[i].value);free(m->domains[i].start);free(m->domains[i].end);free(m->domains[i].link);free(m->domains[i].meaning);} for(i=0;i<m->valueset_count;i++)free(m->valuesets[i].type); for(i=0;i<m->link_count;i++){free(m->links[i].key);free(m->links[i].value);} for(i=0;i<m->accessor_count;i++){free(m->accessors[i].type);free(m->accessors[i].name);free(m->accessors[i].index_variable);free(m->accessors[i].component);free(m->accessors[i].frame);free(m->accessors[i].instance);free(m->accessors[i].power_domain);} for(i=0;i<m->system_encoding_count;i++){free(m->system_encodings[i].type);free(m->system_encodings[i].asmvalue);} for(i=0;i<m->system_selector_count;i++){free(m->system_selectors[i].name);free(m->system_selectors[i].type);free(m->system_selectors[i].value);} for(i=0;i<m->expression_count;i++){free(m->expressions[i].type);free(m->expressions[i].name);free(m->expressions[i].op);free(m->expressions[i].value);free(m->expressions[i].role);free(m->expressions[i].register_state);free(m->expressions[i].register_name);free(m->expressions[i].field_name);} free(m->registers);free(m->metadata);free(m->memory_accesses);free(m->implementation_defined_permissions);free(m->fields);free(m->ranges);free(m->field_branches);free(m->fieldsets);free(m->domains);free(m->valuesets);free(m->constraints);free(m->constraint_items);free(m->links);free(m->accessors);free(m->accessor_offset_expressions);free(m->system_encodings);free(m->system_selectors);free(m->expressions);free(m->expression_children);free(m->source);memset(m,0,sizeof(*m)); }
#undef tcti_register_model_destroy
void tcti_register_model_destroy(struct tcti_register_model *m)
{
	size_t index;
	if (!m)
		return;
	for (index = 0; index < m->system_selector_count; index++)
		free(m->system_selectors[index].meaning);
	for (index = 0; index < m->selector_equation_count; index++)
		free(m->selector_equations[index].identifier);
	free(m->selector_literals);
	free(m->selector_equations);
	free(m->selector_slices);
	free(m->selector_groups);
	free(m->selector_group_fragments);
	tcti_register_model_destroy_base(m);
}
int tcti_register_model_import(const char *json,size_t length,struct tcti_register_model *m,struct tcti_register_model_error *e)
{
	struct tcti_register_model_error local={0};
	struct parser p={0};
	struct import x;
	size_t allocated=0;
	size_t expression_index_bytes;
	uint32_t *expression_by_token = NULL;
	size_t token_index;
	int root;
	char digest[65];

	if(!e)e=&local;
	memset(e,0,sizeof(*e));
	if(!json||!length||!m){fail(e,TCTI_REGISTER_MODEL_INVALID_ARGUMENT,0,"JSON and model are required");return -1;}
	if (m->source || m->registers || m->metadata || m->memory_accesses ||
	    m->implementation_defined_permissions || m->fields || m->expressions) {
		fail(e, TCTI_REGISTER_MODEL_INVALID_ARGUMENT, 0,
		     "model must be destroyed before reuse");
		return -1;
	}
	if(length>MAX_INPUT){fail(e,TCTI_REGISTER_MODEL_INPUT_LIMIT,0,"Registers.json exceeds input limit");return -1;}
	memset(m,0,sizeof(*m));
	m->allocation_limit_bytes = MAX_ALLOC;
	p.s=json;
	p.n=length;
	p.allocated=&allocated;
	p.e=e;
	root=value(&p);
	ws(&p);
	if(root<0||p.p!=length||p.t[root].kind!=ARR){if(!e->code)fail(e,TCTI_REGISTER_MODEL_INVALID_JSON,p.p,"Registers.json must be one JSON array");goto bad;}
	if (p.count > SIZE_MAX / sizeof(*expression_by_token) ||
	    (expression_index_bytes = p.count * sizeof(*expression_by_token)) >
	    MAX_ALLOC || allocated > MAX_ALLOC - expression_index_bytes ||
	    !(expression_by_token = malloc(expression_index_bytes))) {
		fail(e, TCTI_REGISTER_MODEL_NO_MEMORY, 0,
		     "cannot index condition/action tree source objects");
		goto bad;
	}
	allocated += expression_index_bytes;
	for (token_index = 0; token_index < p.count; token_index++)
		expression_by_token[token_index] = UINT32_MAX;
	x=(struct import){
		.s=json,
		.t=p.t,
		.n=p.count,
		.depth=0,
		.allocated=&allocated,
		.expression_by_token=expression_by_token,
		.m=m,
		.e=e,
	};
	if(walk(&x,root,0,UINT32_MAX,UINT32_MAX,1,-1,-1) ||
	   populate_field_condition_roots(&x) || rebuild_expression_children(&x)) goto bad;
	for (token_index = 0; token_index < p.count; token_index++) {
		int type;

		if (p.t[token_index].kind != OBJ)
			continue;
		type = member(&x, (int)token_index, "_type");
		if (eq(&x, type,
		       "Accessors.Permission.AccessTypes.Memory.ReadWriteAccess") &&
		    expression_by_token[token_index] == UINT32_MAX) {
			fail(e, TCTI_REGISTER_MODEL_INVALID_SOURCE,
			     p.t[token_index].start,
			     "typed ReadWriteAccess was not reached by the source walk");
			goto bad;
		}
	}
	if(p.t[root].size!=TCTI_REGISTER_METADATA_COUNT||m->register_count!=TCTI_REGISTER_RECORD_COUNT||m->metadata_count!=TCTI_REGISTER_METADATA_COUNT||m->memory_access_count!=TCTI_REGISTER_MEMORY_ACCESS_COUNT||m->implementation_defined_permission_count!=17U||m->fieldset_count!=TCTI_REGISTER_FIELDSET_COUNT||m->field_count!=TCTI_REGISTER_FIELD_COUNT||m->range_count!=TCTI_REGISTER_RANGE_COUNT||m->top_level_valueset_count!=TCTI_REGISTER_VALUESET_COUNT||m->top_level_domain_count!=TCTI_REGISTER_DOMAIN_COUNT||m->accessor_count!=TCTI_REGISTER_ACCESSOR_COUNT||m->constraint_count!=1656U||m->constraint_domain_count!=2958U){
		size_t rwa_expressions = 0;
		for (token_index = 0; token_index < m->expression_count; token_index++)
			rwa_expressions += !strcmp(m->expressions[token_index].type,
				"Accessors.Permission.AccessTypes.Memory.ReadWriteAccess");
		fail(e,TCTI_REGISTER_MODEL_COUNT_MISMATCH,0,"register counts records=%zu metadata=%zu memory=%zu rwa=%zu implementation-defined=%zu fieldsets=%zu fields=%zu ranges=%zu valuesets=%zu domains=%zu accessors=%zu constraints=%zu/%zu",m->register_count,m->metadata_count,m->memory_access_count,rwa_expressions,m->implementation_defined_permission_count,m->fieldset_count,m->field_count,m->range_count,m->top_level_valueset_count,m->top_level_domain_count,m->accessor_count,m->constraint_count,m->constraint_domain_count);goto bad;
	}
	sha256_hex(json,length,digest);
	if(strcmp(digest,TCTI_REGISTER_SOURCE_SHA256)){fail(e,TCTI_REGISTER_MODEL_HASH_MISMATCH,0,"Registers.json SHA-256 does not match pin");goto bad;}
	allocated -= expression_index_bytes;
	free(expression_by_token);
	expression_by_token = NULL;
	allocated -= p.cap * sizeof(*p.t);
	free(p.t);
	p.t = NULL;
	if (allocated > MAX_ALLOC - length || !(m->source = malloc(length))) { fail(e,TCTI_REGISTER_MODEL_NO_MEMORY,0,"cannot retain Registers.json source"); goto bad; }
	memcpy(m->source, json, length);
	m->source_length = length;
	allocated += length;
	m->allocation_bytes=allocated;
	m->allocation_high_water_bytes=allocated;
	return 0;
bad:
	free(expression_by_token);
	tcti_register_model_destroy(m);
	free(p.t);
	return -1;
}
