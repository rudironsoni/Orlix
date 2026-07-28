/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __ORLIX_TCTI_BASE_A64_CONTROL_FLOW_SEMANTIC_BLOCKERS_H
#define __ORLIX_TCTI_BASE_A64_CONTROL_FLOW_SEMANTIC_BLOCKERS_H

#include <linux/types.h>

enum orlix_tcti_base_control_flow_blocker_obligation {
	ORLIX_TCTI_BASE_CONTROL_FLOW_BRANCH_TYPE_STATE = 1U << 0,
	ORLIX_TCTI_BASE_CONTROL_FLOW_GUARDED_PAGE = 1U << 1,
	ORLIX_TCTI_BASE_CONTROL_FLOW_TARGET_COMPATIBILITY = 1U << 2,
};

enum orlix_tcti_base_control_flow_blocker_status {
	ORLIX_TCTI_BASE_CONTROL_FLOW_EXTERNAL_SEMANTICS_UNAVAILABLE,
};

struct orlix_tcti_base_control_flow_semantic_blocker {
	u16 source_ordinal;
	const char *source_id;
	const char *feature;
	const char *external_locator;
	const char *external_execute_sha256;
	u32 obligations;
	enum orlix_tcti_base_control_flow_blocker_status status;
};

/*
 * The pinned artifact retains external DDI0602 identities and digests, not the
 * semantic bodies.  Baseline execution therefore grants no credit for these
 * FEAT_BTI guarded-page variants.  Disabled HWCAP does not remove them.
 */
static const struct orlix_tcti_base_control_flow_semantic_blocker
orlix_tcti_base_control_flow_semantic_blockers[] = {
	{ 2288U, "BR_64_branch_reg", "FEAT_BTI",
	  "br.xml#A64.control.branch_reg.BR_64_branch_reg/execute",
	  "b78268df907c253387f095bbcb9a36fd21187a690da2815e9358b9bde682b170",
	  ORLIX_TCTI_BASE_CONTROL_FLOW_BRANCH_TYPE_STATE |
	  ORLIX_TCTI_BASE_CONTROL_FLOW_GUARDED_PAGE |
	  ORLIX_TCTI_BASE_CONTROL_FLOW_TARGET_COMPATIBILITY,
	  ORLIX_TCTI_BASE_CONTROL_FLOW_EXTERNAL_SEMANTICS_UNAVAILABLE },
	{ 2291U, "BLR_64_branch_reg", "FEAT_BTI",
	  "blr.xml#A64.control.branch_reg.BLR_64_branch_reg/execute",
	  "cf6e35e7751c17dc41cb95db3c93537a5aa87f3f64a156816882faf322fc18aa",
	  ORLIX_TCTI_BASE_CONTROL_FLOW_BRANCH_TYPE_STATE |
	  ORLIX_TCTI_BASE_CONTROL_FLOW_GUARDED_PAGE |
	  ORLIX_TCTI_BASE_CONTROL_FLOW_TARGET_COMPATIBILITY,
	  ORLIX_TCTI_BASE_CONTROL_FLOW_EXTERNAL_SEMANTICS_UNAVAILABLE },
	{ 2294U, "RET_64R_branch_reg", "FEAT_BTI",
	  "ret.xml#A64.control.branch_reg.RET_64R_branch_reg/execute",
	  "42c2e907ae511330cef94054b2ee62509bdbdca0d5c952d44e20d53a6b0ab3f9",
	  ORLIX_TCTI_BASE_CONTROL_FLOW_BRANCH_TYPE_STATE |
	  ORLIX_TCTI_BASE_CONTROL_FLOW_GUARDED_PAGE |
	  ORLIX_TCTI_BASE_CONTROL_FLOW_TARGET_COMPATIBILITY,
	  ORLIX_TCTI_BASE_CONTROL_FLOW_EXTERNAL_SEMANTICS_UNAVAILABLE },
};

#endif /* __ORLIX_TCTI_BASE_A64_CONTROL_FLOW_SEMANTIC_BLOCKERS_H */
