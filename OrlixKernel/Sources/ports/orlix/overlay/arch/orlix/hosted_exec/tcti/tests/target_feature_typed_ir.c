/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_feature_typed_ir.h"
#include <stdlib.h>
#include <string.h>
#define LOWER_DEPTH 256U
static int reserve(void **p, size_t *c, size_t n, size_t z) {
  size_t q = *c ? *c : 32;
  void *v;
  while (q < n) {
    if (q > SIZE_MAX / 2)
      return -1;
    q *= 2;
  }
  v = realloc(*p, q * z);
  if (!v)
    return -1;
  *p = v;
  *c = q;
  return 0;
}
static int parameter(const struct tcti_feature_model *m, const char *name) {
  size_t i;
  if (!name)
    return -1;
  for (i = 0; i < m->parameter_count; i++)
    if (!strcmp(m->parameters[i].name, name))
      return (int)i;
  return -1;
}
static int diagnostic(struct tcti_feature_typed_result *r,
                      const struct tcti_feature_node *n,
                      enum tcti_feature_typed_error code) {
  if (reserve((void **)&r->diagnostics, &r->diagnostic_capacity,
              r->diagnostic_count + 1, sizeof(*r->diagnostics)))
    return -1;
  r->diagnostics[r->diagnostic_count++] =
      (struct tcti_feature_typed_diagnostic){code, n->kind, n->provenance};
  return 0;
}
static enum tcti_typed_op operation(enum tcti_feature_node_kind k) {
  switch (k) {
  case TCTI_FEATURE_NOT:
    return TCTI_TYPED_NOT;
  case TCTI_FEATURE_AND:
    return TCTI_TYPED_AND;
  case TCTI_FEATURE_OR:
    return TCTI_TYPED_OR;
  case TCTI_FEATURE_EQ:
    return TCTI_TYPED_EQ;
  case TCTI_FEATURE_NE:
    return TCTI_TYPED_NE;
  case TCTI_FEATURE_LT:
    return TCTI_TYPED_LT;
  case TCTI_FEATURE_GT:
    return TCTI_TYPED_GT;
  case TCTI_FEATURE_GE:
    return TCTI_TYPED_GE;
  case TCTI_FEATURE_IN:
    return TCTI_TYPED_IN;
  case TCTI_FEATURE_IMPLIES:
    return TCTI_TYPED_IMPLIES;
  case TCTI_FEATURE_IFF:
    return TCTI_TYPED_IFF;
  case TCTI_FEATURE_UINT:
    return TCTI_TYPED_UINT;
  default:
    return TCTI_TYPED_SINT;
  }
}
static int lower(const struct tcti_feature_model *m, uint32_t index,
                 unsigned depth, struct tcti_typed_expression *x,
                 struct tcti_feature_typed_result *r, uint32_t *out) {
  const struct tcti_feature_node *n;
  uint32_t child[2];
  size_t count = 0;
  enum tcti_typed_error error;
  if (index >= m->node_count || depth > LOWER_DEPTH)
    return -1;
  n = &m->nodes[index];
  if (n->kind == TCTI_FEATURE_BOOL)
    return tcti_typed_literal(x, TCTI_TYPED_BOOL, 0, (uint64_t)n->integer, 0, 0,
                              (struct tcti_typed_provenance){
                                  n->provenance.offset, n->provenance.length},
                              out, &error);
  if (n->kind == TCTI_FEATURE_IDENTIFIER) {
    if (parameter(m, n->text) < 0) {
      diagnostic(r, n, TCTI_FEATURE_TYPED_UNBOUND_IDENTIFIER);
      return 1;
    }
    return tcti_typed_literal(x, TCTI_TYPED_BOOL, 0, 0, 0, 0,
                              (struct tcti_typed_provenance){
                                  n->provenance.offset, n->provenance.length},
                              out, &error);
  }
  if (n->kind == TCTI_FEATURE_INTEGER || n->kind == TCTI_FEATURE_DOT_ATOM ||
      n->kind == TCTI_FEATURE_SET || n->kind == TCTI_FEATURE_VALUE) {
    diagnostic(r, n, TCTI_FEATURE_TYPED_SCALAR_BLOCKER);
    return 1;
  }
  if (n->kind == TCTI_FEATURE_FIELD) {
    diagnostic(r, n, TCTI_FEATURE_TYPED_FIELD_BLOCKER);
    return 1;
  }
	if (n->kind == TCTI_FEATURE_NOT || n->kind == TCTI_FEATURE_UINT ||
	    n->kind == TCTI_FEATURE_SINT) {
		uint32_t source_child = n->left;
		int s;

		if (n->kind != TCTI_FEATURE_NOT) {
			if (n->child_count != 1 ||
			    n->first_child >= m->child_count)
				return -1;
			source_child = m->children[n->first_child];
		}
		s = lower(m, source_child, depth + 1, x, r, &child[0]);
    count = 1;
    if (s)
      return s;
  } else {
    int a = lower(m, n->left, depth + 1, x, r, &child[0]);
    int b = lower(m, n->right, depth + 1, x, r, &child[1]);
    count = 2;
    if (a || b)
      return a < 0 || b < 0 ? -1 : 1;
  }
  if (tcti_typed_apply(x, operation(n->kind), child, count,
                       (struct tcti_typed_provenance){n->provenance.offset,
                                                      n->provenance.length},
                       out, &error)) {
    diagnostic(r, n, TCTI_FEATURE_TYPED_TYPE);
    return 1;
  }
  return 0;
}
int tcti_feature_typed_lower(const struct tcti_feature_model *m,
                             struct tcti_typed_expression *x,
                             struct tcti_feature_typed_result *r) {
  size_t i;
  if (!m || !x || !r)
    return -1;
  memset(r, 0, sizeof(*r));
  r->parameters = calloc(m->parameter_count, sizeof(*r->parameters));
  r->constraints = malloc(m->constraint_count * sizeof(*r->constraints));
  if ((m->parameter_count && !r->parameters) ||
      (m->constraint_count && !r->constraints))
    goto fail;
	for (i = 0; i < m->parameter_count; i++) {
    enum tcti_typed_error e;
    if (tcti_typed_literal(
            x, TCTI_TYPED_BOOL, 0, 0, 0, 0,
            (struct tcti_typed_provenance){m->parameters[i].provenance.offset,
                                           m->parameters[i].provenance.length},
            &r->parameters[i], &e))
      goto fail;
		r->parameter_count++;
	}
	for (i = 0; i < m->node_count; i++) {
		if (m->nodes[i].kind > TCTI_FEATURE_VALUE)
			goto fail;
		r->grammar_counts[m->nodes[i].kind]++;
	}
  for (i = 0; i < m->constraint_count; i++) {
    int status = lower(m, m->constraints[i], 0, x, r,
                       &r->constraints[r->constraint_count]);
    if (status < 0)
      goto fail;
    if (!status)
      r->constraint_count++;
  }
  return 0;
fail:
  tcti_feature_typed_result_destroy(r);
  return -1;
}
void tcti_feature_typed_result_destroy(struct tcti_feature_typed_result *r) {
  if (!r)
    return;
  free(r->constraints);
  free(r->parameters);
  free(r->diagnostics);
  memset(r, 0, sizeof(*r));
}
