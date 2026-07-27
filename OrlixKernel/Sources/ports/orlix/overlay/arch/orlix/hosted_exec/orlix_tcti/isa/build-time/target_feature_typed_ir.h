/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_FEATURE_TYPED_IR_H
#define ORLIX_TCTI_FEATURE_TYPED_IR_H
#include "target_feature_model.h"
#include "target_typed_expression.h"
enum orlix_tcti_feature_typed_error {
  ORLIX_TCTI_FEATURE_TYPED_OK,
  ORLIX_TCTI_FEATURE_TYPED_ARGUMENT,
  ORLIX_TCTI_FEATURE_TYPED_UNBOUND_IDENTIFIER,
  ORLIX_TCTI_FEATURE_TYPED_SCALAR_BLOCKER,
  ORLIX_TCTI_FEATURE_TYPED_FIELD_BLOCKER,
  ORLIX_TCTI_FEATURE_TYPED_TYPE,
  ORLIX_TCTI_FEATURE_TYPED_MEMORY,
  ORLIX_TCTI_FEATURE_TYPED_LIMIT
};
struct orlix_tcti_feature_typed_diagnostic {
  enum orlix_tcti_feature_typed_error code;
  enum orlix_tcti_feature_node_kind source_kind;
  struct orlix_tcti_feature_provenance provenance;
};
struct orlix_tcti_feature_typed_result {
  uint32_t *constraints;
  size_t constraint_count;
  uint32_t *parameters;
  size_t parameter_count;
  size_t grammar_counts[ORLIX_TCTI_FEATURE_VALUE + 1];
  struct orlix_tcti_feature_typed_diagnostic *diagnostics;
  size_t diagnostic_count, diagnostic_capacity;
};
int orlix_tcti_feature_typed_lower(const struct orlix_tcti_feature_model *,
                             struct orlix_tcti_typed_expression *,
                             struct orlix_tcti_feature_typed_result *);
void orlix_tcti_feature_typed_result_destroy(struct orlix_tcti_feature_typed_result *);
#endif
