/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_SYSTEM_ACCESS_SELECTOR_H
#define ORLIX_TCTI_TARGET_SYSTEM_ACCESS_SELECTOR_H

#include <stddef.h>
#include <stdint.h>

/*
 * These are selector values after removing Rt.  MRS/MSR encode only the
 * architectural op0 values 2 and 3, so their selector field is 15 bits.
 */
#define ORLIX_TCTI_SYSTEM_ACCESS_MRS_MSR_SELECTOR_WIDTH 15U
#define ORLIX_TCTI_SYSTEM_ACCESS_SYS_SYSL_SELECTOR_WIDTH 14U
#define ORLIX_TCTI_SYSTEM_ACCESS_MRS_MSR_SELECTOR_COUNT (1U << ORLIX_TCTI_SYSTEM_ACCESS_MRS_MSR_SELECTOR_WIDTH)
#define ORLIX_TCTI_SYSTEM_ACCESS_SYS_SYSL_SELECTOR_COUNT (1U << ORLIX_TCTI_SYSTEM_ACCESS_SYS_SYSL_SELECTOR_WIDTH)

enum orlix_tcti_system_access_selector_form {
	ORLIX_TCTI_SYSTEM_ACCESS_SELECTOR_MRS,
	ORLIX_TCTI_SYSTEM_ACCESS_SELECTOR_MSR,
	ORLIX_TCTI_SYSTEM_ACCESS_SELECTOR_SYS,
	ORLIX_TCTI_SYSTEM_ACCESS_SELECTOR_SYSL,
};

enum orlix_tcti_system_access_selector_field {
	ORLIX_TCTI_SYSTEM_ACCESS_SELECTOR_FIELD_OP0,
	ORLIX_TCTI_SYSTEM_ACCESS_SELECTOR_FIELD_OP1,
	ORLIX_TCTI_SYSTEM_ACCESS_SELECTOR_FIELD_CRN,
	ORLIX_TCTI_SYSTEM_ACCESS_SELECTOR_FIELD_CRM,
	ORLIX_TCTI_SYSTEM_ACCESS_SELECTOR_FIELD_OP2,
};

enum orlix_tcti_system_access_source_value_kind {
	ORLIX_TCTI_SYSTEM_ACCESS_SOURCE_VALUE,
	ORLIX_TCTI_SYSTEM_ACCESS_SOURCE_EQUATION,
	ORLIX_TCTI_SYSTEM_ACCESS_SOURCE_GROUP,
};

enum orlix_tcti_system_access_normalized_value_kind {
	ORLIX_TCTI_SYSTEM_ACCESS_NORMALIZED_VALUE,
	ORLIX_TCTI_SYSTEM_ACCESS_NORMALIZED_EQUATION,
	ORLIX_TCTI_SYSTEM_ACCESS_NORMALIZED_GROUP,
};

#define ORLIX_TCTI_SYSTEM_ACCESS_VALUE_MAX_SLICES 16U
#define ORLIX_TCTI_SYSTEM_ACCESS_VALUE_MAX_GROUP_MEMBERS 16U

/* A parsed Arm Values.Value bit literal, for example "'011'". */
struct orlix_tcti_system_access_value {
	uint8_t value;
	uint8_t width;
};

struct orlix_tcti_system_access_slice {
	uint8_t start;
	uint8_t width;
};

struct orlix_tcti_system_access_equation_value {
	const char *identifier;
	size_t identifier_length;
	uint8_t source_width;
	const struct orlix_tcti_system_access_slice *slices;
	size_t slice_count;
};

struct orlix_tcti_system_access_source_value;

struct orlix_tcti_system_access_group_value {
	const struct orlix_tcti_system_access_source_value *members;
	size_t member_count;
};

struct orlix_tcti_system_access_source_value {
	enum orlix_tcti_system_access_source_value_kind kind;
	union {
		struct {
			const char *literal;
			size_t length;
		} value;
		struct orlix_tcti_system_access_equation_value equation;
		struct orlix_tcti_system_access_group_value group;
	};
};

struct orlix_tcti_system_access_normalized_value {
	enum orlix_tcti_system_access_normalized_value_kind kind;
	enum orlix_tcti_system_access_selector_field field;
	uint8_t width;
	uint8_t lsb;
	union {
		uint8_t value;
		struct orlix_tcti_system_access_equation_value equation;
		struct {
			size_t first_member;
			size_t member_count;
		} group;
	};
};

struct orlix_tcti_system_access_value_scratch {
	struct orlix_tcti_system_access_normalized_value *members;
	size_t member_capacity;
	size_t member_count;
};

struct orlix_tcti_system_access_mrs_msr_fields {
	uint8_t op0;
	uint8_t op1;
	uint8_t crn;
	uint8_t crm;
	uint8_t op2;
};

struct orlix_tcti_system_access_sys_sysl_fields {
	uint8_t op1;
	uint8_t crn;
	uint8_t crm;
	uint8_t op2;
};

typedef int (*orlix_tcti_system_access_selector_callback)(uint16_t selector,
						      void *context);

/*
 * Parse only the source representation used by Values.Value: a quoted binary
 * bit string.  The expected width is part of the source schema contract.
 */
int orlix_tcti_system_access_value_parse(const char *literal, size_t length,
				   uint8_t expected_width,
				   struct orlix_tcti_system_access_value *value);
int orlix_tcti_system_access_selector_field_width(
	enum orlix_tcti_system_access_selector_form form,
	enum orlix_tcti_system_access_selector_field field, uint8_t *width);
int orlix_tcti_system_access_value_normalize(
	enum orlix_tcti_system_access_selector_form form,
	enum orlix_tcti_system_access_selector_field field,
	const struct orlix_tcti_system_access_source_value *source,
	struct orlix_tcti_system_access_value_scratch *scratch,
	struct orlix_tcti_system_access_normalized_value *normalized);

int orlix_tcti_system_access_mrs_msr_pack(
	const struct orlix_tcti_system_access_mrs_msr_fields *fields,
	uint16_t *selector);
int orlix_tcti_system_access_sys_sysl_pack(
	const struct orlix_tcti_system_access_sys_sysl_fields *fields,
	uint16_t *selector);

int orlix_tcti_system_access_selector_width(
	enum orlix_tcti_system_access_selector_form form, uint8_t *width);
int orlix_tcti_system_access_selector_enumerate(
	enum orlix_tcti_system_access_selector_form form,
	orlix_tcti_system_access_selector_callback callback, void *context);

#endif /* ORLIX_TCTI_TARGET_SYSTEM_ACCESS_SELECTOR_H */
