/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_feature_field_domain_binding_artifact.h"

#ifdef __KERNEL__
#include <linux/string.h>
#ifndef UINT64_C
#define UINT64_C(value) value ## ULL
#endif
#else
#include <string.h>
#endif

#ifdef ORLIX_TCTI_FEATURE_FIELD_DOMAIN_TEST_DEF_ONLY
#define DOMAIN_DEF "target_feature_field_domain_binding_artifact_test.c"
#elif !defined(DOMAIN_DEF)
#define DOMAIN_DEF "../isa/target_feature_field_domain_binding.def"
#endif
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_SOURCE(...)
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_IDENTITY(...)
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_RANGE_V3(...)
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_VALUESET_V3(...)
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_DOMAIN_V3(...)
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_LINK_V3(...)
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_EXPRESSION_V3(...)
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_EXPRESSION_CHILD_V3(...)
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_VALUE_CANDIDATE_V3(...)
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_CONSTRAINT_V3(...)
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_CONSTRAINT_ITEM_V3(...)
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_CONSTRAINT_CANDIDATE_V3(...)
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_ALTERNATIVE_V3(...)
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_OCCURRENCE_V3(...)
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_COUNTS_V3(o, g, m, a, al, r, v, d, \
	l, e, ec, vc, c, ci, cc) \
	CANONICAL_OCCURRENCES = o, CANONICAL_GROUPS = g, CANONICAL_MAPPED = m, \
	CANONICAL_AMBIGUOUS = a, CANONICAL_ALTERNATIVES = al, \
	CANONICAL_RANGES = r, CANONICAL_VALUESETS = v, CANONICAL_DOMAINS = d, \
	CANONICAL_LINKS = l, CANONICAL_EXPRESSIONS = e, \
	CANONICAL_EXPRESSION_CHILDREN = ec, CANONICAL_VALUE_CANDIDATES = vc, \
	CANONICAL_CONSTRAINTS = c, CANONICAL_CONSTRAINT_ITEMS = ci, \
	CANONICAL_CONSTRAINT_CANDIDATES = cc,
enum canonical_counts {
#include DOMAIN_DEF
};
#undef ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_COUNTS_V3

#ifndef ORLIX_TCTI_FEATURE_FIELD_DOMAIN_TEST_DEF_ONLY
_Static_assert(CANONICAL_OCCURRENCES ==
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_OCCURRENCE_COUNT,
	"feature field-domain occurrence census changed");
_Static_assert(CANONICAL_GROUPS ==
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_IDENTITY_GROUP_COUNT,
	"feature field-domain identity-group census changed");
_Static_assert(CANONICAL_MAPPED ==
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_MAPPED_COUNT,
	"feature field-domain mapped census changed");
_Static_assert(CANONICAL_AMBIGUOUS ==
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_AMBIGUOUS_FIELD_COUNT,
	"feature field-domain ambiguous census changed");
_Static_assert(CANONICAL_ALTERNATIVES ==
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ALTERNATIVE_COUNT,
	"feature field-domain alternative census changed");
#endif

struct canonical_storage {
	struct orlix_tcti_feature_field_domain_binding
		bindings[CANONICAL_OCCURRENCES ? CANONICAL_OCCURRENCES : 1];
	struct orlix_tcti_feature_field_domain_alternative
		alternatives[CANONICAL_ALTERNATIVES ? CANONICAL_ALTERNATIVES : 1];
	struct orlix_tcti_feature_field_domain_range
		ranges[CANONICAL_RANGES ? CANONICAL_RANGES : 1];
	struct orlix_tcti_feature_field_domain_valueset
		valuesets[CANONICAL_VALUESETS ? CANONICAL_VALUESETS : 1];
	struct orlix_tcti_feature_field_domain_node
		domains[CANONICAL_DOMAINS ? CANONICAL_DOMAINS : 1];
	struct orlix_tcti_feature_field_domain_link
		links[CANONICAL_LINKS ? CANONICAL_LINKS : 1];
	struct orlix_tcti_feature_field_domain_expression
		expressions[CANONICAL_EXPRESSIONS ? CANONICAL_EXPRESSIONS : 1];
	orlix_tcti_feature_artifact_u32 expression_children[
		CANONICAL_EXPRESSION_CHILDREN ? CANONICAL_EXPRESSION_CHILDREN : 1];
	struct orlix_tcti_feature_field_domain_value_candidate
		value_candidates[CANONICAL_VALUE_CANDIDATES ?
			CANONICAL_VALUE_CANDIDATES : 1];
	struct orlix_tcti_feature_field_domain_constraint constraints[
		CANONICAL_CONSTRAINTS ? CANONICAL_CONSTRAINTS : 1];
	struct orlix_tcti_feature_field_domain_constraint_item
		constraint_items[CANONICAL_CONSTRAINT_ITEMS ?
			CANONICAL_CONSTRAINT_ITEMS : 1];
	struct orlix_tcti_feature_field_domain_constraint_candidate
		constraint_candidates[CANONICAL_CONSTRAINT_CANDIDATES ?
			CANONICAL_CONSTRAINT_CANDIDATES : 1];
};

#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_COUNTS_V3(...)
#undef ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_RANGE_V3
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_RANGE_V3(i, s, w) \
	.ranges[i] = { s, w },
#undef ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_VALUESET_V3
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_VALUESET_V3(i, t) \
	.valuesets[i] = { t },
#undef ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_DOMAIN_V3
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_DOMAIN_V3(i, t, v, s, e, l, m, p, \
	ce, fc, cc, vi, nvi, fl, lc) \
	.domains[i] = { t, v, s, e, l, m, p, ce, fc, cc, vi, nvi, fl, lc },
#undef ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_LINK_V3
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_LINK_V3(i, k, v, d) \
	.links[i] = { k, v, d },
#undef ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_EXPRESSION_V3
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_EXPRESSION_V3(i, t, n, o, v, r, \
	rs, rn, f, fc, cc, sk, in, b, p, io, il, so, sl, xo, xl) \
	.expressions[i] = { t, n, o, v, r, rs, rn, f, fc, cc, sk, in, b, p, \
		{ io, il }, { so, sl }, { xo, xl } },
#undef ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_EXPRESSION_CHILD_V3
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_EXPRESSION_CHILD_V3(i, e) \
	.expression_children[i] = e,
#undef ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_VALUE_CANDIDATE_V3
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_VALUE_CANDIDATE_V3(i, k, v, f, c) \
	.value_candidates[i] = { k, v, f, c },
#undef ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_CONSTRAINT_V3
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_CONSTRAINT_V3(i, k, v, f, c) \
	.constraints[i] = { k, v, f, c },
#undef ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_CONSTRAINT_ITEM_V3
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_CONSTRAINT_ITEM_V3(i, c, v, d) \
	.constraint_items[i] = { c, v, d },
#undef ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_CONSTRAINT_CANDIDATE_V3
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_CONSTRAINT_CANDIDATE_V3(i, c, k, f, n) \
	.constraint_candidates[i] = { c, k, f, n },
#undef ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_ALTERNATIVE_V3
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_ALTERNATIVE_V3(i, fi, vi, fsi, fce, \
	wce, fsce, rfce, rwce, fr, rc, fv, vc, fd, dc, fl, lc, fe, ec, fec, ecc, \
	fvc, vcc, fco, coc, fci, cic, fcc, ccc, fo, fn, vo, vn, fso, fsn, fceo, \
	fcen, wco, wcn, fsco, fscn) \
	.alternatives[i] = { fi, vi, fsi, fce, wce, fsce, rfce, rwce, fr, rc, \
		fv, vc, fd, dc, fl, lc, fe, ec, fec, ecc, fvc, vcc, fco, coc, fci, \
		cic, fcc, ccc, { fo, fn }, { vo, vn }, { fso, fsn }, { fceo, fcen }, \
		{ wco, wcn }, { fsco, fscn } },
#undef ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_OCCURRENCE_V3
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_OCCURRENCE_V3(i, fn, g, oc, ri, fi, \
	vi, di, st, rn, se, ik, io, il, sk, so, sl, xo, xl, ro, rl, fo, fl, vo, \
	vl, fa, ac, fsi, efc, rw, fw, rc, vm, rm, rv, rco, fce, wce, fsce, ty, \
	si, mk, sem, res, fso, fsl, fco, fcl, wco, wcl, fsco, fscl) \
	.bindings[i] = { i, fn, g, oc, ri, fi, vi, di, st, rn, se, \
		{ ik, { io, il } }, { sk, { so, sl } }, { xo, xl }, { ro, rl }, \
		{ fo, fl }, { vo, vl }, fa, ac, { fsi, efc, rw, fw, rc, vm, rm, rv, \
		rco, fce, wce, fsce, ty, si, mk, sem, res, { fso, fsl }, \
		{ fco, fcl }, { wco, wcl }, { fsco, fscl } } },
static const struct canonical_storage canonical = {
#include DOMAIN_DEF
};

#undef ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_SOURCE
#undef ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_COUNTS_V3
#undef ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_IDENTITY
#undef ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_RANGE_V3
#undef ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_VALUESET_V3
#undef ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_DOMAIN_V3
#undef ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_LINK_V3
#undef ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_EXPRESSION_V3
#undef ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_EXPRESSION_CHILD_V3
#undef ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_VALUE_CANDIDATE_V3
#undef ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_CONSTRAINT_V3
#undef ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_CONSTRAINT_ITEM_V3
#undef ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_CONSTRAINT_CANDIDATE_V3
#undef ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_ALTERNATIVE_V3
#undef ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_OCCURRENCE_V3

static const struct orlix_tcti_feature_field_domain_binding_artifact
	canonical_artifact = {
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_SOURCE(a, b, r, s, h, l, rh, rl) \
	.source = { a, b, r, s, h, l }, .register_source_sha256 = rh, \
	.register_source_length = rl,
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_COUNTS_V3(o, g, m, a, al, r, v, d, \
	l, e, ec, vc, c, ci, cc) \
	.occurrence_count = o, .identity_group_count = g, .mapped_count = m, \
	.ambiguous_field_count = a, .alternative_count = al, .range_count = r, \
	.valueset_count = v, .domain_count = d, .link_count = l, \
	.expression_count = e, .expression_child_count = ec, \
	.value_candidate_count = vc, .constraint_count = c, \
	.constraint_item_count = ci, .constraint_candidate_count = cc,
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_IDENTITY(i) .identity = i,
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_RANGE_V3(...)
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_VALUESET_V3(...)
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_DOMAIN_V3(...)
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_LINK_V3(...)
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_EXPRESSION_V3(...)
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_EXPRESSION_CHILD_V3(...)
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_VALUE_CANDIDATE_V3(...)
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_CONSTRAINT_V3(...)
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_CONSTRAINT_ITEM_V3(...)
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_CONSTRAINT_CANDIDATE_V3(...)
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_ALTERNATIVE_V3(...)
#define ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_OCCURRENCE_V3(...)
#include DOMAIN_DEF
	.bindings = canonical.bindings, .alternatives = canonical.alternatives,
	.ranges = canonical.ranges, .valuesets = canonical.valuesets,
	.domains = canonical.domains, .links = canonical.links,
	.expressions = canonical.expressions,
	.expression_children = canonical.expression_children,
	.value_candidates = canonical.value_candidates,
	.constraints = canonical.constraints,
	.constraint_items = canonical.constraint_items,
	.constraint_candidates = canonical.constraint_candidates,
};

const struct orlix_tcti_feature_field_domain_binding_artifact *
orlix_tcti_feature_field_domain_binding_artifact_canonical(void)
{
	return &canonical_artifact;
}

static enum orlix_tcti_feature_field_domain_binding_error fail(
	struct orlix_tcti_feature_field_domain_binding_diagnostic *diagnostic,
	enum orlix_tcti_feature_field_domain_binding_error error,
	orlix_tcti_feature_artifact_u32 index)
{
	if (diagnostic) {
		diagnostic->error = error;
		diagnostic->index = index;
	}
	return error;
}

static int text_equal(const char *a, const char *b)
{
	return (!a && !b) || (a && b && !strcmp(a, b));
}

static int span_equal(const struct orlix_tcti_feature_artifact_span *a,
	const struct orlix_tcti_feature_artifact_span *b)
{
	return a->offset == b->offset && a->length == b->length;
}

static int span_valid(const struct orlix_tcti_feature_artifact_span *span,
	orlix_tcti_feature_artifact_u32 limit, int required)
{
	if (!span->length)
		return !required && !span->offset;
	return span->offset < limit && span->length <= limit - span->offset;
}

static int slice_valid(orlix_tcti_feature_artifact_u32 first,
	orlix_tcti_feature_artifact_u32 count, orlix_tcti_feature_artifact_u32 total)
{
	return first <= total && count <= total - first;
}

static int local_index(orlix_tcti_feature_artifact_u32 index,
	orlix_tcti_feature_artifact_u32 count, int optional)
{
	return (optional && index == ORLIX_TCTI_FEATURE_FIELD_DOMAIN_INDEX_NONE) ||
		index < count;
}

static int child_slice_valid(orlix_tcti_feature_artifact_u32 first,
	orlix_tcti_feature_artifact_u32 count, orlix_tcti_feature_artifact_u32 total)
{
	return count ? slice_valid(first, count, total) :
		first == ORLIX_TCTI_FEATURE_FIELD_DOMAIN_INDEX_NONE;
}

static int pointer_counts_valid(
	const struct orlix_tcti_feature_field_domain_binding_artifact *a)
{
#define PRESENT(count, pointer) (!(count) || (pointer))
	return PRESENT(a->occurrence_count, a->bindings) &&
		PRESENT(a->alternative_count, a->alternatives) &&
		PRESENT(a->range_count, a->ranges) &&
		PRESENT(a->valueset_count, a->valuesets) &&
		PRESENT(a->domain_count, a->domains) &&
		PRESENT(a->link_count, a->links) &&
		PRESENT(a->expression_count, a->expressions) &&
		PRESENT(a->expression_child_count, a->expression_children) &&
		PRESENT(a->value_candidate_count, a->value_candidates) &&
		PRESENT(a->constraint_count, a->constraints) &&
		PRESENT(a->constraint_item_count, a->constraint_items) &&
		PRESENT(a->constraint_candidate_count, a->constraint_candidates);
#undef PRESENT
}

static int domain_topology_valid(
	const struct orlix_tcti_feature_field_domain_binding_artifact *a,
	const struct orlix_tcti_feature_field_domain_alternative *alt)
{
	orlix_tcti_feature_artifact_u32 i, j;
	for (i = 0; i < alt->domain_count; i++) {
		const struct orlix_tcti_feature_field_domain_node *d =
			&a->domains[alt->first_domain + i];
		orlix_tcti_feature_artifact_u32 cursor = i;
		if (!local_index(d->parent_domain, alt->domain_count, 1) ||
		    !local_index(d->condition_expression, alt->expression_count, 1) ||
		    !child_slice_valid(d->first_child_domain, d->child_domain_count,
			alt->domain_count) ||
		    !local_index(d->valueset_index, alt->valueset_count, 1) ||
		    !local_index(d->nested_valueset_index, alt->valueset_count, 1) ||
		    !slice_valid(d->first_link, d->link_count, alt->link_count))
			return 0;
		for (j = 0; j < d->child_domain_count; j++)
			if (a->domains[alt->first_domain + d->first_child_domain + j].parent_domain != i)
				return 0;
		for (j = 0; j <= alt->domain_count; j++) {
			cursor = a->domains[alt->first_domain + cursor].parent_domain;
			if (cursor == ORLIX_TCTI_FEATURE_FIELD_DOMAIN_INDEX_NONE)
				break;
			if (cursor >= alt->domain_count)
				return 0;
		}
		if (j > alt->domain_count)
			return 0;
		{
			orlix_tcti_feature_artifact_u32 references = 0, parent;
			for (parent = 0; parent < alt->domain_count; parent++) {
				const struct orlix_tcti_feature_field_domain_node *p =
					&a->domains[alt->first_domain + parent];
				for (j = 0; j < p->child_domain_count; j++)
					if (p->first_child_domain + j == i)
						references++;
			}
			if (references != (d->parent_domain ==
				ORLIX_TCTI_FEATURE_FIELD_DOMAIN_INDEX_NONE ? 0U : 1U))
				return 0;
		}
	}
	for (i = 0; i < alt->link_count; i++) {
		orlix_tcti_feature_artifact_u32 references = 0;
		const struct orlix_tcti_feature_field_domain_link *link =
			&a->links[alt->first_link + i];
		if (!local_index(link->domain_index, alt->domain_count, 0))
			return 0;
		for (j = 0; j < alt->domain_count; j++) {
			const struct orlix_tcti_feature_field_domain_node *d =
				&a->domains[alt->first_domain + j];
			if (i >= d->first_link && i - d->first_link < d->link_count) {
				if (link->domain_index != j)
					return 0;
				references++;
			}
		}
		if (references != 1U)
			return 0;
	}
	return 1;
}

static int expression_topology_valid(
	const struct orlix_tcti_feature_field_domain_binding_artifact *a,
	const struct orlix_tcti_feature_field_domain_alternative *alt)
{
	orlix_tcti_feature_artifact_u32 i, j;
	for (i = 0; i < alt->expression_count; i++) {
		const struct orlix_tcti_feature_field_domain_expression *e =
			&a->expressions[alt->first_expression + i];
		orlix_tcti_feature_artifact_u32 cursor = i;
		if (!child_slice_valid(e->first_child, e->child_count,
			alt->expression_child_count) ||
		    !local_index(e->parent_expression, alt->expression_count, 1) ||
		    e->boolean > 1U ||
		    !span_valid(&e->instance_source, a->register_source_length, 0) ||
		    !span_valid(&e->slices_source, a->register_source_length, 0) ||
		    !span_valid(&e->source, a->register_source_length, 1))
			return 0;
		for (j = 0; j < e->child_count; j++) {
			orlix_tcti_feature_artifact_u32 child =
				a->expression_children[alt->first_expression_child +
					e->first_child + j];
			if (child >= alt->expression_count ||
			    a->expressions[alt->first_expression + child].parent_expression != i)
				return 0;
		}
		for (j = 0; j <= alt->expression_count; j++) {
			cursor = a->expressions[alt->first_expression + cursor].parent_expression;
			if (cursor == ORLIX_TCTI_FEATURE_FIELD_DOMAIN_INDEX_NONE)
				break;
			if (cursor >= alt->expression_count)
				return 0;
		}
		if (j > alt->expression_count)
			return 0;
		{
			orlix_tcti_feature_artifact_u32 references = 0, parent;
			for (parent = 0; parent < alt->expression_count; parent++) {
				const struct orlix_tcti_feature_field_domain_expression *p =
					&a->expressions[alt->first_expression + parent];
				for (j = 0; j < p->child_count; j++)
					if (a->expression_children[
						alt->first_expression_child + p->first_child + j] == i)
						references++;
			}
			if (references != (e->parent_expression ==
				ORLIX_TCTI_FEATURE_FIELD_DOMAIN_INDEX_NONE ? 0U : 1U))
				return 0;
		}
	}
	return 1;
}

static int alternatives_valid(
	const struct orlix_tcti_feature_field_domain_binding_artifact *a)
{
	orlix_tcti_feature_artifact_u32 i;
	orlix_tcti_feature_artifact_u32 next[10] = { 0 };
	for (i = 0; i < a->alternative_count; i++) {
		const struct orlix_tcti_feature_field_domain_alternative *x =
			&a->alternatives[i];
		orlix_tcti_feature_artifact_u32 j;
#define EXACT_SLICE(first, count, total, slot) \
	if ((first) != next[slot] || !slice_valid(first, count, total)) return 0; \
	next[slot] += (count)
		EXACT_SLICE(x->first_range, x->range_count, a->range_count, 0);
		EXACT_SLICE(x->first_valueset, x->valueset_count, a->valueset_count, 1);
		EXACT_SLICE(x->first_domain, x->domain_count, a->domain_count, 2);
		EXACT_SLICE(x->first_link, x->link_count, a->link_count, 3);
		EXACT_SLICE(x->first_expression, x->expression_count,
			a->expression_count, 4);
		EXACT_SLICE(x->first_expression_child, x->expression_child_count,
			a->expression_child_count, 5);
		EXACT_SLICE(x->first_value_candidate, x->value_candidate_count,
			a->value_candidate_count, 6);
		EXACT_SLICE(x->first_constraint, x->constraint_count,
			a->constraint_count, 7);
		EXACT_SLICE(x->first_constraint_item, x->constraint_item_count,
			a->constraint_item_count, 8);
		EXACT_SLICE(x->first_constraint_candidate, x->constraint_candidate_count,
			a->constraint_candidate_count, 9);
#undef EXACT_SLICE
		if (!span_valid(&x->field_source, a->register_source_length, 1) ||
		    !span_valid(&x->value_relation_source, a->register_source_length, 1) ||
		    !span_valid(&x->fieldset_source, a->register_source_length, 1) ||
		    !span_valid(&x->field_condition_source, a->register_source_length, 0) ||
		    !span_valid(&x->wrapper_condition_source, a->register_source_length, 0) ||
		    !span_valid(&x->fieldset_condition_source, a->register_source_length, 0) ||
		    !local_index(x->field_condition_expression, x->expression_count, 1) ||
		    !local_index(x->wrapper_condition_expression, x->expression_count, 1) ||
		    !local_index(x->fieldset_condition_expression, x->expression_count, 1) ||
		    !local_index(x->relation_field_condition_expression,
			x->expression_count, 1) ||
		    !local_index(x->relation_wrapper_condition_expression,
			x->expression_count, 1) ||
		    !domain_topology_valid(a, x) || !expression_topology_valid(a, x))
			return 0;
		for (j = 0; j < x->range_count; j++)
			if (!a->ranges[x->first_range + j].width)
				return 0;
		for (j = 0; j < x->value_candidate_count; j++) {
			const struct orlix_tcti_feature_field_domain_value_candidate *v =
				&a->value_candidates[x->first_value_candidate + j];
			if (!local_index(v->valueset_index, x->valueset_count, 1) ||
			    !slice_valid(v->first_domain, v->domain_count, x->domain_count))
				return 0;
		}
		for (j = 0; j < x->constraint_count; j++) {
			const struct orlix_tcti_feature_field_domain_constraint *c =
				&a->constraints[x->first_constraint + j];
			orlix_tcti_feature_artifact_u32 k;
			if (!local_index(c->valueset_index, x->valueset_count, 1) ||
			    !slice_valid(c->first_item, c->item_count,
				x->constraint_item_count))
				return 0;
			for (k = 0; k < c->item_count; k++)
				if (a->constraint_items[x->first_constraint_item +
					c->first_item + k].constraint_index != j)
					return 0;
		}
		for (j = 0; j < x->constraint_item_count; j++) {
			const struct orlix_tcti_feature_field_domain_constraint_item *item =
				&a->constraint_items[x->first_constraint_item + j];
			orlix_tcti_feature_artifact_u32 k, references = 0;
			if (!local_index(item->constraint_index, x->constraint_count, 0) ||
			    !local_index(item->valueset_index, x->valueset_count, 1) ||
			    !local_index(item->domain_index, x->domain_count, 1))
				return 0;
			for (k = 0; k < x->constraint_count; k++) {
				const struct orlix_tcti_feature_field_domain_constraint *c =
					&a->constraints[x->first_constraint + k];
				if (j >= c->first_item && j - c->first_item < c->item_count) {
					if (item->constraint_index != k)
						return 0;
					references++;
				}
			}
			if (references != 1U)
				return 0;
		}
		for (j = 0; j < x->constraint_candidate_count; j++) {
			const struct orlix_tcti_feature_field_domain_constraint_candidate *c =
				&a->constraint_candidates[x->first_constraint_candidate + j];
			if (!local_index(c->constraint_index, x->constraint_count, 0) ||
			    !slice_valid(c->first_domain, c->domain_count, x->domain_count))
				return 0;
		}
	}
	return next[0] == a->range_count && next[1] == a->valueset_count &&
		next[2] == a->domain_count && next[3] == a->link_count &&
		next[4] == a->expression_count && next[5] == a->expression_child_count &&
		next[6] == a->value_candidate_count && next[7] == a->constraint_count &&
		next[8] == a->constraint_item_count &&
		next[9] == a->constraint_candidate_count;
}

static int qualifier_equal(
	const struct orlix_tcti_feature_artifact_field_qualifier *a,
	const struct orlix_tcti_feature_artifact_field_qualifier *b)
{
	return a->kind == b->kind && span_equal(&a->source, &b->source);
}

static int normalized_domain_equal(
	const struct orlix_tcti_feature_field_normalized_domain *a,
	const struct orlix_tcti_feature_field_normalized_domain *b)
{
#define EQ(f) (a->f == b->f)
	return EQ(fieldset_index) && EQ(equivalent_field_count) &&
		EQ(register_width) && EQ(field_width) && EQ(range_count) &&
		EQ(value_member_count) && EQ(range_member_count) &&
		EQ(relation_value_count) && EQ(relation_constraint_count) &&
		EQ(field_condition_expression) && EQ(wrapper_condition_expression) &&
		EQ(fieldset_condition_expression) && EQ(type) && EQ(signedness) &&
		EQ(member_kind) && EQ(semantic_identity) && EQ(resolution_identity) &&
		span_equal(&a->fieldset_source, &b->fieldset_source) &&
		span_equal(&a->field_condition_source, &b->field_condition_source) &&
		span_equal(&a->wrapper_condition_source, &b->wrapper_condition_source) &&
		span_equal(&a->fieldset_condition_source, &b->fieldset_condition_source);
#undef EQ
}

static int binding_equal(
	const struct orlix_tcti_feature_field_domain_binding *a,
	const struct orlix_tcti_feature_field_domain_binding *b)
{
#define EQ(f) (a->f == b->f)
	return EQ(order) && EQ(feature_node_index) && EQ(identity_group_index) &&
		EQ(occurrence_count) && EQ(register_index) && EQ(field_index) &&
		EQ(value_relation_index) && EQ(disposition) &&
		text_equal(a->field_state, b->field_state) &&
		text_equal(a->field_register_name, b->field_register_name) &&
		text_equal(a->field_selector, b->field_selector) &&
		qualifier_equal(&a->field_instance, &b->field_instance) &&
		qualifier_equal(&a->field_slices, &b->field_slices) &&
		span_equal(&a->feature_source, &b->feature_source) &&
		span_equal(&a->register_source, &b->register_source) &&
		span_equal(&a->field_source, &b->field_source) &&
		span_equal(&a->value_relation_source, &b->value_relation_source) &&
		EQ(first_alternative) && EQ(alternative_count) &&
		normalized_domain_equal(&a->domain, &b->domain);
#undef EQ
}

static int alternative_equal(
	const struct orlix_tcti_feature_field_domain_alternative *a,
	const struct orlix_tcti_feature_field_domain_alternative *b)
{
#define EQ(f) (a->f == b->f)
	return EQ(field_index) && EQ(value_relation_index) && EQ(fieldset_index) &&
		EQ(field_condition_expression) && EQ(wrapper_condition_expression) &&
		EQ(fieldset_condition_expression) &&
		EQ(relation_field_condition_expression) &&
		EQ(relation_wrapper_condition_expression) && EQ(first_range) &&
		EQ(range_count) && EQ(first_valueset) && EQ(valueset_count) &&
		EQ(first_domain) && EQ(domain_count) && EQ(first_link) && EQ(link_count) &&
		EQ(first_expression) && EQ(expression_count) &&
		EQ(first_expression_child) && EQ(expression_child_count) &&
		EQ(first_value_candidate) && EQ(value_candidate_count) &&
		EQ(first_constraint) && EQ(constraint_count) &&
		EQ(first_constraint_item) && EQ(constraint_item_count) &&
		EQ(first_constraint_candidate) && EQ(constraint_candidate_count) &&
		span_equal(&a->field_source, &b->field_source) &&
		span_equal(&a->value_relation_source, &b->value_relation_source) &&
		span_equal(&a->fieldset_source, &b->fieldset_source) &&
		span_equal(&a->field_condition_source, &b->field_condition_source) &&
		span_equal(&a->wrapper_condition_source, &b->wrapper_condition_source) &&
		span_equal(&a->fieldset_condition_source, &b->fieldset_condition_source);
#undef EQ
}

static int node_equal(const struct orlix_tcti_feature_field_domain_node *a,
	const struct orlix_tcti_feature_field_domain_node *b)
{
#define EQ(f) (a->f == b->f)
	return text_equal(a->type, b->type) && text_equal(a->value, b->value) &&
		text_equal(a->start, b->start) && text_equal(a->end, b->end) &&
		text_equal(a->link, b->link) && text_equal(a->meaning, b->meaning) &&
		EQ(parent_domain) && EQ(condition_expression) &&
		EQ(first_child_domain) && EQ(child_domain_count) && EQ(valueset_index) &&
		EQ(nested_valueset_index) && EQ(first_link) && EQ(link_count);
#undef EQ
}

static int expression_equal(
	const struct orlix_tcti_feature_field_domain_expression *a,
	const struct orlix_tcti_feature_field_domain_expression *b)
{
#define EQ(f) (a->f == b->f)
	return text_equal(a->type, b->type) && text_equal(a->name, b->name) &&
		text_equal(a->op, b->op) && text_equal(a->value, b->value) &&
		text_equal(a->role, b->role) &&
		text_equal(a->register_state, b->register_state) &&
		text_equal(a->register_name, b->register_name) &&
		text_equal(a->field_name, b->field_name) && EQ(first_child) &&
		EQ(child_count) && EQ(scalar_kind) && EQ(integer) && EQ(boolean) &&
		EQ(parent_expression) &&
		span_equal(&a->instance_source, &b->instance_source) &&
		span_equal(&a->slices_source, &b->slices_source) &&
		span_equal(&a->source, &b->source);
#undef EQ
}

static int artifact_deep_equal(
	const struct orlix_tcti_feature_field_domain_binding_artifact *a,
	const struct orlix_tcti_feature_field_domain_binding_artifact *b)
{
	/* Canonical C initializers contain no padding-sensitive comparison here. */
	if (!text_equal(a->source.architecture, b->source.architecture) ||
	    !text_equal(a->source.build, b->source.build) ||
	    !text_equal(a->source.reference, b->source.reference) ||
	    !text_equal(a->source.schema, b->source.schema) ||
	    !text_equal(a->source.sha256, b->source.sha256) ||
	    a->source.length != b->source.length ||
	    !text_equal(a->register_source_sha256, b->register_source_sha256) ||
	    a->register_source_length != b->register_source_length ||
	    a->occurrence_count != b->occurrence_count ||
	    a->identity_group_count != b->identity_group_count ||
	    a->mapped_count != b->mapped_count ||
	    a->ambiguous_field_count != b->ambiguous_field_count ||
	    a->alternative_count != b->alternative_count ||
	    a->range_count != b->range_count || a->valueset_count != b->valueset_count ||
	    a->domain_count != b->domain_count || a->link_count != b->link_count ||
	    a->expression_count != b->expression_count ||
	    a->expression_child_count != b->expression_child_count ||
	    a->value_candidate_count != b->value_candidate_count ||
	    a->constraint_count != b->constraint_count ||
	    a->constraint_item_count != b->constraint_item_count ||
	    a->constraint_candidate_count != b->constraint_candidate_count ||
	    a->identity != b->identity)
		return 0;
	{
		orlix_tcti_feature_artifact_u32 i;
		for (i = 0; i < a->occurrence_count; i++)
			if (!binding_equal(&a->bindings[i], &b->bindings[i]))
				return 0;
		for (i = 0; i < a->alternative_count; i++)
			if (!alternative_equal(&a->alternatives[i], &b->alternatives[i]))
				return 0;
		for (i = 0; i < a->range_count; i++)
			if (a->ranges[i].start != b->ranges[i].start ||
			    a->ranges[i].width != b->ranges[i].width)
				return 0;
		for (i = 0; i < a->valueset_count; i++)
			if (!text_equal(a->valuesets[i].type, b->valuesets[i].type))
				return 0;
		for (i = 0; i < a->domain_count; i++)
			if (!node_equal(&a->domains[i], &b->domains[i]))
				return 0;
		for (i = 0; i < a->link_count; i++)
			if (!text_equal(a->links[i].key, b->links[i].key) ||
			    !text_equal(a->links[i].value, b->links[i].value) ||
			    a->links[i].domain_index != b->links[i].domain_index)
				return 0;
		for (i = 0; i < a->expression_count; i++)
			if (!expression_equal(&a->expressions[i], &b->expressions[i]))
				return 0;
		for (i = 0; i < a->expression_child_count; i++)
			if (a->expression_children[i] != b->expression_children[i])
				return 0;
#define EQ_SCALAR_TABLE(member, count, body) \
		for (i = 0; i < a->count; i++) { body }
		EQ_SCALAR_TABLE(value_candidates, value_candidate_count,
			if (a->value_candidates[i].kind != b->value_candidates[i].kind ||
			    a->value_candidates[i].valueset_index != b->value_candidates[i].valueset_index ||
			    a->value_candidates[i].first_domain != b->value_candidates[i].first_domain ||
			    a->value_candidates[i].domain_count != b->value_candidates[i].domain_count)
				return 0;);
		EQ_SCALAR_TABLE(constraints, constraint_count,
			if (a->constraints[i].kind != b->constraints[i].kind ||
			    a->constraints[i].valueset_index != b->constraints[i].valueset_index ||
			    a->constraints[i].first_item != b->constraints[i].first_item ||
			    a->constraints[i].item_count != b->constraints[i].item_count)
				return 0;);
		EQ_SCALAR_TABLE(constraint_items, constraint_item_count,
			if (a->constraint_items[i].constraint_index != b->constraint_items[i].constraint_index ||
			    a->constraint_items[i].valueset_index != b->constraint_items[i].valueset_index ||
			    a->constraint_items[i].domain_index != b->constraint_items[i].domain_index)
				return 0;);
		EQ_SCALAR_TABLE(constraint_candidates, constraint_candidate_count,
			if (a->constraint_candidates[i].constraint_index != b->constraint_candidates[i].constraint_index ||
			    a->constraint_candidates[i].kind != b->constraint_candidates[i].kind ||
			    a->constraint_candidates[i].first_domain != b->constraint_candidates[i].first_domain ||
			    a->constraint_candidates[i].domain_count != b->constraint_candidates[i].domain_count)
				return 0;);
#undef EQ_SCALAR_TABLE
	}
	return 1;
}

enum orlix_tcti_feature_field_domain_binding_error
orlix_tcti_feature_field_domain_binding_validate_with_contract(
	const struct orlix_tcti_feature_artifact *feature_artifact,
	const struct orlix_tcti_feature_field_domain_binding_artifact *artifact,
	const struct orlix_tcti_feature_field_domain_binding_contract *contract,
	struct orlix_tcti_feature_field_domain_binding_scratch *scratch,
	struct orlix_tcti_feature_field_domain_binding_diagnostic *diagnostic)
{
	orlix_tcti_feature_artifact_u32 i, next_alternative = 0;
	if (!feature_artifact || !artifact || !contract || !scratch)
		return fail(diagnostic,
			ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_INVALID_ARGUMENT, 0);
	if (artifact->occurrence_count != contract->occurrence_count ||
	    artifact->identity_group_count != contract->identity_group_count ||
	    artifact->mapped_count != contract->mapped_count ||
	    artifact->ambiguous_field_count != contract->ambiguous_field_count ||
	    artifact->alternative_count != contract->alternative_count)
		return fail(diagnostic,
			ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_COUNT_MISMATCH, 0);
	if (!text_equal(artifact->source.architecture,
		feature_artifact->source.architecture) ||
	    !text_equal(artifact->source.build, feature_artifact->source.build) ||
	    !text_equal(artifact->source.reference, feature_artifact->source.reference) ||
	    !text_equal(artifact->source.schema, feature_artifact->source.schema) ||
	    !text_equal(artifact->source.sha256, feature_artifact->source.sha256) ||
	    artifact->source.length != feature_artifact->source.length ||
	    !text_equal(artifact->register_source_sha256,
		contract->register_source_sha256) ||
	    artifact->register_source_length != contract->register_source_length)
		return fail(diagnostic,
			ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_PIN_MISMATCH, 0);
	if (!pointer_counts_valid(artifact))
		return fail(diagnostic,
			ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_POINTER_MISSING, 0);
	if ((feature_artifact->counts.node_count && !feature_artifact->nodes) ||
	    (feature_artifact->counts.node_count && !scratch->feature_node_coverage) ||
	    (artifact->identity_group_count && !scratch->identity_group_coverage))
		return fail(diagnostic,
			ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_POINTER_MISSING, 0);
	if (scratch->feature_node_coverage_count < feature_artifact->counts.node_count ||
	    scratch->identity_group_coverage_count < artifact->identity_group_count)
		return fail(diagnostic,
			ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_SCRATCH_TOO_SMALL, 0);
	memset(scratch->feature_node_coverage, 0, feature_artifact->counts.node_count);
	memset(scratch->identity_group_coverage, 0,
		artifact->identity_group_count * sizeof(*scratch->identity_group_coverage));
	if (!alternatives_valid(artifact))
		return fail(diagnostic,
			ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_TOPOLOGY_INVALID, 0);
	for (i = 0; i < artifact->occurrence_count; i++) {
		const struct orlix_tcti_feature_field_domain_binding *b =
			&artifact->bindings[i];
		const struct orlix_tcti_feature_artifact_node *node;
		if (b->order != i || b->feature_node_index >= feature_artifact->counts.node_count ||
		    b->identity_group_index >= artifact->identity_group_count ||
		    b->disposition != ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MAPPED)
			return fail(diagnostic,
				ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_INDEX_INVALID, i);
		node = &feature_artifact->nodes[b->feature_node_index];
		if (node->kind != ORLIX_TCTI_FEATURE_ARTIFACT_FIELD ||
		    scratch->feature_node_coverage[b->feature_node_index] ||
		    !text_equal(node->field_state, b->field_state) ||
		    !text_equal(node->field_register_name, b->field_register_name) ||
		    !text_equal(node->field_selector, b->field_selector) ||
		    node->field_instance.kind != b->field_instance.kind ||
		    node->field_slices.kind != b->field_slices.kind ||
		    !span_equal(&node->field_source, &b->feature_source) ||
		    !span_equal(&node->field_instance.source, &b->field_instance.source) ||
		    !span_equal(&node->field_slices.source, &b->field_slices.source))
			return fail(diagnostic,
				ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_PROVENANCE_INVALID, i);
		if (b->first_alternative != next_alternative || !b->alternative_count ||
		    !slice_valid(b->first_alternative, b->alternative_count,
			artifact->alternative_count) ||
		    b->alternative_count != b->domain.equivalent_field_count ||
		    !b->domain.register_width || !b->domain.field_width ||
		    b->domain.field_width > b->domain.register_width ||
		    b->domain.type >= ORLIX_TCTI_FEATURE_FIELD_DOMAIN_TYPE_COUNT ||
		    b->domain.signedness >= ORLIX_TCTI_FEATURE_FIELD_DOMAIN_SIGNEDNESS_COUNT ||
		    b->domain.member_kind >= ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MEMBER_KIND_COUNT ||
		    !span_valid(&b->register_source, artifact->register_source_length, 1) ||
		    !span_valid(&b->field_source, artifact->register_source_length, 1) ||
		    !span_valid(&b->value_relation_source,
			artifact->register_source_length, 1) ||
		    !span_valid(&b->domain.fieldset_source,
			artifact->register_source_length, 1) ||
		    !span_valid(&b->domain.field_condition_source,
			artifact->register_source_length, 0) ||
		    !span_valid(&b->domain.wrapper_condition_source,
			artifact->register_source_length, 0) ||
		    !span_valid(&b->domain.fieldset_condition_source,
			artifact->register_source_length, 0))
			return fail(diagnostic,
				ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_SLICE_INVALID, i);
		{
			orlix_tcti_feature_artifact_u32 alternative;
			for (alternative = 0; alternative < b->alternative_count;
			     alternative++) {
				const struct orlix_tcti_feature_field_domain_alternative *x =
					&artifact->alternatives[b->first_alternative + alternative];
				orlix_tcti_feature_artifact_u32 left, right;
				if (x->range_count != b->domain.range_count ||
				    x->value_candidate_count != b->domain.relation_value_count ||
				    x->constraint_count !=
					b->domain.relation_constraint_count)
					return fail(diagnostic,
						ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_SLICE_INVALID,
						i);
				for (left = 0; left < x->range_count; left++) {
					const struct orlix_tcti_feature_field_domain_range *range =
						&artifact->ranges[x->first_range + left];
					if (range->start >= b->domain.register_width ||
					    range->width > b->domain.register_width - range->start)
						return fail(diagnostic,
							ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_SLICE_INVALID,
							i);
				}
				for (left = 0; left < x->range_count; left++) {
					const struct orlix_tcti_feature_field_domain_range *range =
						&artifact->ranges[x->first_range + left];
					for (right = left + 1; right < x->range_count; right++) {
						const struct orlix_tcti_feature_field_domain_range *other =
							&artifact->ranges[x->first_range + right];
						if ((range->start <= other->start &&
						     other->start - range->start < range->width) ||
						    (other->start < range->start &&
						     range->start - other->start < other->width))
							return fail(diagnostic,
								ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_SLICE_INVALID,
								i);
					}
				}
			}
		}
		next_alternative += b->alternative_count;
		scratch->feature_node_coverage[b->feature_node_index] = 1;
		scratch->identity_group_coverage[b->identity_group_index]++;
	}
	if (next_alternative != artifact->alternative_count)
		return fail(diagnostic,
			ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_COVERAGE_INVALID, 0);
	for (i = 0; i < artifact->occurrence_count; i++)
		if (artifact->bindings[i].occurrence_count !=
		    scratch->identity_group_coverage[
			artifact->bindings[i].identity_group_index])
			return fail(diagnostic,
				ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_COVERAGE_INVALID, i);
	for (i = 0; i < feature_artifact->counts.node_count; i++)
		if (feature_artifact->nodes[i].kind == ORLIX_TCTI_FEATURE_ARTIFACT_FIELD &&
		    !scratch->feature_node_coverage[i])
			return fail(diagnostic,
				ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_COVERAGE_INVALID, i);
	return ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_VALID;
}

enum orlix_tcti_feature_field_domain_binding_error
orlix_tcti_feature_field_domain_binding_validate(
	const struct orlix_tcti_feature_artifact *feature_artifact,
	const struct orlix_tcti_feature_field_domain_binding_artifact *artifact,
	struct orlix_tcti_feature_field_domain_binding_scratch *scratch,
	struct orlix_tcti_feature_field_domain_binding_diagnostic *diagnostic)
{
	static const struct orlix_tcti_feature_field_domain_binding_contract contract = {
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_OCCURRENCE_COUNT,
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_IDENTITY_GROUP_COUNT,
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_MAPPED_COUNT,
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_AMBIGUOUS_FIELD_COUNT,
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ALTERNATIVE_COUNT,
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_REGISTER_SOURCE_SHA256,
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_REGISTER_SOURCE_LENGTH,
	};
	enum orlix_tcti_feature_field_domain_binding_error error =
		orlix_tcti_feature_field_domain_binding_validate_with_contract(
			feature_artifact, artifact, &contract, scratch, diagnostic);
	if (error != ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_VALID)
		return error;
	if (!artifact_deep_equal(artifact, &canonical_artifact))
		return fail(diagnostic,
			ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_CANONICAL_MISMATCH, 0);
	return ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_VALID;
}

const char *orlix_tcti_feature_field_domain_binding_error_name(
	enum orlix_tcti_feature_field_domain_binding_error error)
{
	switch (error) {
	case ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_VALID: return "valid";
	case ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_INVALID_ARGUMENT: return "invalid argument";
	case ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_PIN_MISMATCH: return "pin mismatch";
	case ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_COUNT_MISMATCH: return "count mismatch";
	case ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_POINTER_MISSING: return "pointer missing";
	case ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_DISPOSITION_INVALID: return "invalid disposition";
	case ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_INDEX_INVALID: return "invalid index";
	case ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_PROVENANCE_INVALID: return "invalid provenance";
	case ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_COVERAGE_INVALID: return "invalid coverage";
	case ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_SLICE_INVALID: return "invalid slice";
	case ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_TOPOLOGY_INVALID: return "invalid topology";
	case ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_CANONICAL_MISMATCH: return "canonical mismatch";
	case ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_SCRATCH_TOO_SMALL: return "scratch too small";
	}
	return "unknown";
}
