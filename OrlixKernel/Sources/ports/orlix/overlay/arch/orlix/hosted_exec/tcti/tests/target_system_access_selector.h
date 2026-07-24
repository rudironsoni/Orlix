/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_SYSTEM_ACCESS_SELECTOR_H
#define ORLIX_TCTI_TARGET_SYSTEM_ACCESS_SELECTOR_H

#include <stddef.h>
#include <stdint.h>

/*
 * These are selector values after removing Rt.  MRS/MSR encode only the
 * architectural op0 values 2 and 3, so their selector field is 15 bits.
 */
#define TCTI_SYSTEM_ACCESS_MRS_MSR_SELECTOR_WIDTH 15U
#define TCTI_SYSTEM_ACCESS_SYS_SYSL_SELECTOR_WIDTH 14U
#define TCTI_SYSTEM_ACCESS_MRS_MSR_SELECTOR_COUNT (1U << TCTI_SYSTEM_ACCESS_MRS_MSR_SELECTOR_WIDTH)
#define TCTI_SYSTEM_ACCESS_SYS_SYSL_SELECTOR_COUNT (1U << TCTI_SYSTEM_ACCESS_SYS_SYSL_SELECTOR_WIDTH)

enum tcti_system_access_selector_form {
	TCTI_SYSTEM_ACCESS_SELECTOR_MRS,
	TCTI_SYSTEM_ACCESS_SELECTOR_MSR,
	TCTI_SYSTEM_ACCESS_SELECTOR_SYS,
	TCTI_SYSTEM_ACCESS_SELECTOR_SYSL,
};

enum tcti_system_access_selector_field {
	TCTI_SYSTEM_ACCESS_SELECTOR_FIELD_OP0,
	TCTI_SYSTEM_ACCESS_SELECTOR_FIELD_OP1,
	TCTI_SYSTEM_ACCESS_SELECTOR_FIELD_CRN,
	TCTI_SYSTEM_ACCESS_SELECTOR_FIELD_CRM,
	TCTI_SYSTEM_ACCESS_SELECTOR_FIELD_OP2,
};

enum tcti_system_access_source_value_kind {
	TCTI_SYSTEM_ACCESS_SOURCE_VALUE,
	TCTI_SYSTEM_ACCESS_SOURCE_EQUATION,
	TCTI_SYSTEM_ACCESS_SOURCE_GROUP,
};

enum tcti_system_access_normalized_value_kind {
	TCTI_SYSTEM_ACCESS_NORMALIZED_VALUE,
	TCTI_SYSTEM_ACCESS_NORMALIZED_EQUATION,
	TCTI_SYSTEM_ACCESS_NORMALIZED_GROUP,
};

#define TCTI_SYSTEM_ACCESS_VALUE_MAX_SLICES 16U
#define TCTI_SYSTEM_ACCESS_VALUE_MAX_GROUP_MEMBERS 16U

/* A parsed Arm Values.Value bit literal, for example "'011'". */
struct tcti_system_access_value {
	uint8_t value;
	uint8_t width;
};

struct tcti_system_access_slice {
	uint8_t start;
	uint8_t width;
};

struct tcti_system_access_equation_value {
	const char *identifier;
	size_t identifier_length;
	uint8_t source_width;
	const struct tcti_system_access_slice *slices;
	size_t slice_count;
};

struct tcti_system_access_source_value;

struct tcti_system_access_group_value {
	const struct tcti_system_access_source_value *members;
	size_t member_count;
};

struct tcti_system_access_source_value {
	enum tcti_system_access_source_value_kind kind;
	union {
		struct {
			const char *literal;
			size_t length;
		} value;
		struct tcti_system_access_equation_value equation;
		struct tcti_system_access_group_value group;
	};
};

struct tcti_system_access_normalized_value {
	enum tcti_system_access_normalized_value_kind kind;
	enum tcti_system_access_selector_field field;
	uint8_t width;
	uint8_t lsb;
	union {
		uint8_t value;
		struct tcti_system_access_equation_value equation;
		struct {
			size_t first_member;
			size_t member_count;
		} group;
	};
};

struct tcti_system_access_value_scratch {
	struct tcti_system_access_normalized_value *members;
	size_t member_capacity;
	size_t member_count;
};

struct tcti_system_access_mrs_msr_fields {
	uint8_t op0;
	uint8_t op1;
	uint8_t crn;
	uint8_t crm;
	uint8_t op2;
};

struct tcti_system_access_sys_sysl_fields {
	uint8_t op1;
	uint8_t crn;
	uint8_t crm;
	uint8_t op2;
};

typedef int (*tcti_system_access_selector_callback)(uint16_t selector,
						      void *context);

/*
 * Parse only the source representation used by Values.Value: a quoted binary
 * bit string.  The expected width is part of the source schema contract.
 */
int tcti_system_access_value_parse(const char *literal, size_t length,
				   uint8_t expected_width,
				   struct tcti_system_access_value *value);
int tcti_system_access_selector_field_width(
	enum tcti_system_access_selector_form form,
	enum tcti_system_access_selector_field field, uint8_t *width);
int tcti_system_access_value_normalize(
	enum tcti_system_access_selector_form form,
	enum tcti_system_access_selector_field field,
	const struct tcti_system_access_source_value *source,
	struct tcti_system_access_value_scratch *scratch,
	struct tcti_system_access_normalized_value *normalized);

int tcti_system_access_mrs_msr_pack(
	const struct tcti_system_access_mrs_msr_fields *fields,
	uint16_t *selector);
int tcti_system_access_sys_sysl_pack(
	const struct tcti_system_access_sys_sysl_fields *fields,
	uint16_t *selector);

int tcti_system_access_selector_width(
	enum tcti_system_access_selector_form form, uint8_t *width);
int tcti_system_access_selector_enumerate(
	enum tcti_system_access_selector_form form,
	tcti_system_access_selector_callback callback, void *context);

#endif /* ORLIX_TCTI_TARGET_SYSTEM_ACCESS_SELECTOR_H */
