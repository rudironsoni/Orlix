/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_feature_typed_ir.h"
#include "target_inventory_import.h"
#include <stdio.h>
#include <stdlib.h>
static char *read_source(const char *path, size_t *length) {
  FILE *f = fopen(path, "rb");
  long n;
  char *s;
  if (!f)
    return NULL;
  if (fseek(f, 0, SEEK_END) || (n = ftell(f)) < 0 || fseek(f, 0, SEEK_SET)) {
    fclose(f);
    return NULL;
  }
  s = malloc((size_t)n);
  if (!s || fread(s, 1, (size_t)n, f) != (size_t)n) {
    free(s);
    fclose(f);
    return NULL;
  }
  fclose(f);
  *length = (size_t)n;
  return s;
}
static int synthetic(void) {
  struct orlix_tcti_feature_parameter p = {.name = "A"};
  struct orlix_tcti_feature_node nodes[] = {
      {.kind = ORLIX_TCTI_FEATURE_IDENTIFIER, .text = "A", .provenance = {2, 3}},
      {.kind = ORLIX_TCTI_FEATURE_BOOL, .integer = 1, .provenance = {7, 4}},
      {.kind = ORLIX_TCTI_FEATURE_IMPLIES,
       .left = 0,
       .right = 1,
       .provenance = {1, 10}}};
  uint32_t roots[] = {2};
  struct orlix_tcti_feature_model m = {.parameters = &p,
                                 .parameter_count = 1,
                                 .nodes = nodes,
                                 .node_count = 3,
                                 .constraints = roots,
                                 .constraint_count = 1};
  struct orlix_tcti_typed_expression x = {0};
  struct orlix_tcti_feature_typed_result r;
  int failed = orlix_tcti_feature_typed_lower(&m, &x, &r) || r.parameter_count != 1 ||
               r.constraint_count != 1 || r.diagnostic_count ||
               x.nodes[r.constraints[0]].op != ORLIX_TCTI_TYPED_IMPLIES ||
               x.nodes[r.constraints[0]].provenance.offset != 1;
  orlix_tcti_feature_typed_result_destroy(&r);
  orlix_tcti_typed_destroy(&x);
  return failed;
}
int main(int argc, char **argv) {
  const char *path = argc > 1 ? argv[1] : getenv("ORLIX_TCTI_FEATURES_JSON");
  struct orlix_tcti_target_import_error unused;
  struct orlix_tcti_feature_error error;
  struct orlix_tcti_feature_model model;
  struct orlix_tcti_typed_expression typed = {0};
  struct orlix_tcti_feature_typed_result result;
  char *source;
  size_t length, total = 0, i;
  int failed = synthetic();
  (void)unused;
  if (!path)
    return 2;
  source = read_source(path, &length);
  if (!source)
    return 1;
  if (orlix_tcti_target_feature_model_import(source, length, &model, &error)) {
    free(source);
    return 1;
  }
  if (orlix_tcti_feature_typed_lower(&model, &typed, &result)) {
    if (result.diagnostic_count)
      fprintf(stderr, "feature typed IR lower failed: code=%d kind=%d offset=%zu\n",
              result.diagnostics[0].code, result.diagnostics[0].source_kind,
              result.diagnostics[0].provenance.offset);
    failed = 1;
  }
  failed |= result.parameter_count != ORLIX_TCTI_FEATURE_PARAMETER_COUNT;
  failed |= result.diagnostic_count != 0;
	for (i = 0; i <= ORLIX_TCTI_FEATURE_VALUE; i++)
		total += result.grammar_counts[i];
	failed |= total != model.node_count;
	for (i = 0; i <= ORLIX_TCTI_FEATURE_VALUE; i++)
		failed |= result.grammar_counts[i] == 0;
  orlix_tcti_feature_typed_result_destroy(&result);
  orlix_tcti_typed_destroy(&typed);
  orlix_tcti_target_feature_model_destroy(&model);
  free(source);
  if (failed)
    fprintf(stderr, "feature typed IR test failed\n");
  return !!failed;
}
