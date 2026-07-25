/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TEST_SUITES_H
#define ORLIX_TCTI_TEST_SUITES_H

#include <kunit/test.h>

extern struct kunit_suite tcti_lse_decode_test_suite;
extern struct kunit_suite tcti_atomic_memory_test_suite;
extern struct kunit_suite tcti_cssc_min_max_immediate_test_suite;
extern struct kunit_suite tcti_crypto_decode_boundary_test_suite;
extern struct kunit_suite tcti_crc32_decode_boundary_test_suite;
extern struct kunit_suite tcti_crypto_extension_decode_boundary_test_suite;
extern struct kunit_suite tcti_lse_source_bound_test_suite;
extern struct kunit_suite tcti_lse128_noncas_test_suite;
extern struct kunit_suite tcti_add_sub_immediate_test_suite;
extern struct kunit_suite tcti_logical_shifted_register_test_suite;
extern struct kunit_suite tcti_bitfield_extract_source_bound_test_suite;

#endif
