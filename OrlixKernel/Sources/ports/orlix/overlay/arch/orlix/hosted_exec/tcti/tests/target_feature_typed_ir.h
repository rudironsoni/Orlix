/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_FEATURE_TYPED_IR_H
#define ORLIX_TCTI_FEATURE_TYPED_IR_H
#include "target_feature_model.h"
#include "target_typed_expression.h"
enum tcti_feature_typed_error {
  TCTI_FEATURE_TYPED_OK,
  TCTI_FEATURE_TYPED_ARGUMENT,
  TCTI_FEATURE_TYPED_UNBOUND_IDENTIFIER,
  TCTI_FEATURE_TYPED_SCALAR_BLOCKER,
  TCTI_FEATURE_TYPED_FIELD_BLOCKER,
  TCTI_FEATURE_TYPED_TYPE,
  TCTI_FEATURE_TYPED_MEMORY,
  TCTI_FEATURE_TYPED_LIMIT
};
struct tcti_feature_typed_diagnostic {
  enum tcti_feature_typed_error code;
  enum tcti_feature_node_kind source_kind;
  struct tcti_feature_provenance provenance;
};
struct tcti_feature_typed_result {
  uint32_t *constraints;
  size_t constraint_count;
  uint32_t *parameters;
  size_t parameter_count;
  size_t grammar_counts[TCTI_FEATURE_VALUE + 1];
  struct tcti_feature_typed_diagnostic *diagnostics;
  size_t diagnostic_count, diagnostic_capacity;
};
int tcti_feature_typed_lower(const struct tcti_feature_model *,
                             struct tcti_typed_expression *,
                             struct tcti_feature_typed_result *);
void tcti_feature_typed_result_destroy(struct tcti_feature_typed_result *);
#endif
