/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_BASE_SYSTEM_139_H
#define ORLIX_TCTI_BASE_SYSTEM_139_H

#include <linux/types.h>

#include "decode_aarch64.h"

/*
 * Pinned AARCHMRS 2026-06 source-manifest precedence for HINT-space leaves.
 * A generic HINT is selected only after every more-specific candidate loses.
 */
enum orlix_tcti_hint_precedence_disposition {
	ORLIX_TCTI_HINT_PRECEDENCE_UNDEFINED,
	ORLIX_TCTI_HINT_PRECEDENCE_FEATURE_UNAVAILABLE,
};

struct orlix_tcti_hint_precedence_leaf {
	u16 source_ordinal;
	u32 mask;
	u32 pattern;
	enum orlix_tcti_hint_precedence_disposition disposition;
};

struct orlix_tcti_base_system_139_feature_leaf {
	u16 source_ordinal;
	u32 mask;
	u32 pattern;
	enum orlix_tcti_feature_hint_op operation;
};

/*
 * The issue denominator is deliberately a source-owned set, rather than an
 * ordinal range.  Keep it beside the family lowering contract so tests can
 * prove both cardinality and ownership against target_execution_slice_map.
 */
#define ORLIX_TCTI_BASE_SYSTEM_139_LEAF_ORDINALS \
	2236U, 2237U, 2238U, 2239U, 2240U, 2241U, 2242U, 2243U, 2244U, \
	2250U, 2251U, 2252U, 2253U, 2254U, 2255U, 2264U, 2266U, 2267U, \
	2268U, 2269U, 2270U, 2271U, 2272U, 2273U, 2274U, 2275U, 2276U, \
	2277U, 2278U, 2279U, 2280U, 2281U, 2282U, 2283U, 2284U, 2285U, \
	2286U, 2287U, 2344U

#define ORLIX_TCTI_BASE_SYSTEM_139_LEAF_COUNT 39U

#define ORLIX_TCTI_HINT_PRECEDENCE_LEAVES \
	/* BASE_POINTER_AUTH semantic precedence. */ \
	{ 2245U, 0xffffffffU, 0xd50320ffU, ORLIX_TCTI_HINT_PRECEDENCE_UNDEFINED }, \
	{ 2246U, 0xffffffffU, 0xd503211fU, ORLIX_TCTI_HINT_PRECEDENCE_UNDEFINED }, \
	{ 2247U, 0xffffffffU, 0xd503215fU, ORLIX_TCTI_HINT_PRECEDENCE_UNDEFINED }, \
	{ 2248U, 0xffffffffU, 0xd503219fU, ORLIX_TCTI_HINT_PRECEDENCE_UNDEFINED }, \
	{ 2249U, 0xffffffffU, 0xd50321dfU, ORLIX_TCTI_HINT_PRECEDENCE_UNDEFINED }, \
	{ 2256U, 0xffffffffU, 0xd503231fU, ORLIX_TCTI_HINT_PRECEDENCE_UNDEFINED }, \
	{ 2257U, 0xffffffffU, 0xd503233fU, ORLIX_TCTI_HINT_PRECEDENCE_UNDEFINED }, \
	{ 2258U, 0xffffffffU, 0xd503235fU, ORLIX_TCTI_HINT_PRECEDENCE_UNDEFINED }, \
	{ 2259U, 0xffffffffU, 0xd503237fU, ORLIX_TCTI_HINT_PRECEDENCE_UNDEFINED }, \
	{ 2260U, 0xffffffffU, 0xd503239fU, ORLIX_TCTI_HINT_PRECEDENCE_UNDEFINED }, \
	{ 2261U, 0xffffffffU, 0xd50323bfU, ORLIX_TCTI_HINT_PRECEDENCE_UNDEFINED }, \
	{ 2262U, 0xffffffffU, 0xd50323dfU, ORLIX_TCTI_HINT_PRECEDENCE_UNDEFINED }, \
	{ 2263U, 0xffffffffU, 0xd50323ffU, ORLIX_TCTI_HINT_PRECEDENCE_UNDEFINED }, \
	{ 2265U, 0xffffffffU, 0xd50324ffU, ORLIX_TCTI_HINT_PRECEDENCE_UNDEFINED }, \
	/* BASE_SYSTEM feature-conditioned leaves, unavailable in the target. */ \
	{ 2244U, 0xffffffffU, 0xd50320dfU, ORLIX_TCTI_HINT_PRECEDENCE_FEATURE_UNAVAILABLE }, \
	{ 2250U, 0xffffffffU, 0xd503221fU, ORLIX_TCTI_HINT_PRECEDENCE_FEATURE_UNAVAILABLE }, \
	{ 2251U, 0xffffffffU, 0xd503223fU, ORLIX_TCTI_HINT_PRECEDENCE_FEATURE_UNAVAILABLE }, \
	{ 2252U, 0xffffffffU, 0xd503225fU, ORLIX_TCTI_HINT_PRECEDENCE_FEATURE_UNAVAILABLE }, \
	{ 2253U, 0xffffffffU, 0xd503227fU, ORLIX_TCTI_HINT_PRECEDENCE_FEATURE_UNAVAILABLE }, \
	{ 2255U, 0xffffffffU, 0xd50322dfU, ORLIX_TCTI_HINT_PRECEDENCE_FEATURE_UNAVAILABLE }, \
	{ 2264U, 0xffffff3fU, 0xd503241fU, ORLIX_TCTI_HINT_PRECEDENCE_UNDEFINED }, \
	{ 2266U, 0xffffffffU, 0xd503251fU, ORLIX_TCTI_HINT_PRECEDENCE_FEATURE_UNAVAILABLE }, \
	{ 2267U, 0xffffffdfU, 0xd503261fU, ORLIX_TCTI_HINT_PRECEDENCE_FEATURE_UNAVAILABLE }, \
	{ 2268U, 0xffffffdfU, 0xd503265fU, ORLIX_TCTI_HINT_PRECEDENCE_FEATURE_UNAVAILABLE }, \
	{ 2269U, 0xffffffffU, 0xd503269fU, ORLIX_TCTI_HINT_PRECEDENCE_FEATURE_UNAVAILABLE }

/* Pinned DDI0602 feature-on operations.  Each has a distinct decoder op. */
#define ORLIX_TCTI_BASE_SYSTEM_139_FEATURE_HINT_LEAVES \
	{ 2244U, 0xffffffffU, 0xd50320dfU, ORLIX_TCTI_FEATURE_HINT_DGH }, \
	{ 2250U, 0xffffffffU, 0xd503221fU, ORLIX_TCTI_FEATURE_HINT_ESB }, \
	{ 2251U, 0xffffffffU, 0xd503223fU, ORLIX_TCTI_FEATURE_HINT_PSB }, \
	{ 2252U, 0xffffffffU, 0xd503225fU, ORLIX_TCTI_FEATURE_HINT_TSB }, \
	{ 2253U, 0xffffffffU, 0xd503227fU, ORLIX_TCTI_FEATURE_HINT_GCSB }, \
	{ 2255U, 0xffffffffU, 0xd50322dfU, ORLIX_TCTI_FEATURE_HINT_CLRBHB }, \
	{ 2264U, 0xffffff3fU, 0xd503241fU, ORLIX_TCTI_FEATURE_HINT_BTI }, \
	{ 2266U, 0xffffffffU, 0xd503251fU, ORLIX_TCTI_FEATURE_HINT_CHKFEAT }, \
	{ 2267U, 0xffffffdfU, 0xd503261fU, ORLIX_TCTI_FEATURE_HINT_STSHH }, \
	{ 2268U, 0xffffffdfU, 0xd503265fU, ORLIX_TCTI_FEATURE_HINT_SHUH }, \
	{ 2269U, 0xffffffffU, 0xd503269fU, ORLIX_TCTI_FEATURE_HINT_STCPH }, \
	{ 2344U, 0xffd8f000U, 0xd5002000U, ORLIX_TCTI_FEATURE_HINT_HINTE }

/* Feature-conditioned forms outside this semantic slice stay fail-closed. */
#define ORLIX_TCTI_BASE_SYSTEM_139_FEATURE_UNAVAILABLE_LEAVES \
	{ 2285U, 0xfff80000U, 0xd5480000U }, /* SYSP */ \
	{ 2286U, 0xfff00000U, 0xd5500000U }, /* MSRR */ \
	{ 2287U, 0xfff00000U, 0xd5700000U }  /* MRRS */

#endif /* ORLIX_TCTI_BASE_SYSTEM_139_H */
