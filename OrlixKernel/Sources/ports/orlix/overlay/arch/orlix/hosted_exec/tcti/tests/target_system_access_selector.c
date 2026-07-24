// SPDX-License-Identifier: GPL-2.0-only
#include "target_system_access_selector.h"

#include <errno.h>

static int value_width(enum tcti_system_access_selector_form form,
		       enum tcti_system_access_selector_field field,
		       uint8_t *width)
{
	if (!width)
		return -EINVAL;
	if (field > TCTI_SYSTEM_ACCESS_SELECTOR_FIELD_OP2)
		return -EINVAL;
	if ((form == TCTI_SYSTEM_ACCESS_SELECTOR_SYS ||
	     form == TCTI_SYSTEM_ACCESS_SELECTOR_SYSL) &&
	    field == TCTI_SYSTEM_ACCESS_SELECTOR_FIELD_OP0)
		return -EINVAL;

	switch (field) {
	case TCTI_SYSTEM_ACCESS_SELECTOR_FIELD_OP0:
		*width = 2U;
		return 0;
	case TCTI_SYSTEM_ACCESS_SELECTOR_FIELD_OP1:
	case TCTI_SYSTEM_ACCESS_SELECTOR_FIELD_OP2:
		*width = 3U;
		return 0;
	case TCTI_SYSTEM_ACCESS_SELECTOR_FIELD_CRN:
	case TCTI_SYSTEM_ACCESS_SELECTOR_FIELD_CRM:
		*width = 4U;
		return 0;
	default:
		return -EINVAL;
	}
}

static int equation_width(const struct tcti_system_access_equation_value *equation,
			  uint8_t expected_width)
{
	uint64_t used = 0;
	uint8_t total = 0;
	size_t index;

	if (!equation || !equation->identifier || !equation->identifier_length ||
	    !equation->source_width || equation->source_width > 64U ||
	    !equation->slices || !equation->slice_count ||
	    equation->slice_count > TCTI_SYSTEM_ACCESS_VALUE_MAX_SLICES)
		return -EINVAL;
	for (index = 0; index < equation->slice_count; index++) {
		const struct tcti_system_access_slice *slice =
			&equation->slices[index];
		uint8_t bit;

		if (!slice->width || slice->start >= equation->source_width ||
		    slice->width > equation->source_width - slice->start ||
		    total > expected_width - slice->width)
			return -EINVAL;
		for (bit = 0; bit < slice->width; bit++) {
			uint8_t position = (uint8_t)(slice->start + bit);

			if (used & (UINT64_C(1) << position))
				return -EINVAL;
			used |= UINT64_C(1) << position;
		}
		total = (uint8_t)(total + slice->width);
	}
	return total == expected_width ? 0 : -EINVAL;
}

static int normalize_atom(
	enum tcti_system_access_selector_field field, uint8_t expected_width,
	uint8_t lsb, const struct tcti_system_access_source_value *source,
	struct tcti_system_access_normalized_value *normalized)
{
	struct tcti_system_access_value value;

	if (!source || !normalized)
		return -EINVAL;
	normalized->field = field;
	normalized->width = expected_width;
	normalized->lsb = lsb;
	switch (source->kind) {
	case TCTI_SYSTEM_ACCESS_SOURCE_VALUE:
		if (tcti_system_access_value_parse(source->value.literal,
				source->value.length, expected_width, &value))
			return -EINVAL;
		normalized->kind = TCTI_SYSTEM_ACCESS_NORMALIZED_VALUE;
		normalized->value = value.value;
		return 0;
	case TCTI_SYSTEM_ACCESS_SOURCE_EQUATION:
		if (equation_width(&source->equation, expected_width))
			return -EINVAL;
		normalized->kind = TCTI_SYSTEM_ACCESS_NORMALIZED_EQUATION;
		normalized->equation = source->equation;
		return 0;
	case TCTI_SYSTEM_ACCESS_SOURCE_GROUP:
	default:
		return -EINVAL;
	}
}

int tcti_system_access_value_parse(const char *literal, size_t length,
				   uint8_t expected_width,
				   struct tcti_system_access_value *value)
{
	uint8_t parsed = 0;
	size_t index;

	if (!literal || !value || !expected_width || expected_width > 8U)
		return -EINVAL;
	if (length != (size_t)expected_width + 2U || literal[0] != '\'' ||
	    literal[length - 1U] != '\'')
		return -EINVAL;
	for (index = 0; index < expected_width; index++) {
		char bit = literal[index + 1U];

		if (bit != '0' && bit != '1')
			return -EINVAL;
		parsed = (uint8_t)((parsed << 1U) | (uint8_t)(bit - '0'));
	}
	value->value = parsed;
	value->width = expected_width;
	return 0;
}

int tcti_system_access_selector_field_width(
	enum tcti_system_access_selector_form form,
	enum tcti_system_access_selector_field field, uint8_t *width)
{
	uint8_t selector_width;

	if (tcti_system_access_selector_width(form, &selector_width))
		return -EINVAL;
	return value_width(form, field, width);
}

int tcti_system_access_value_normalize(
	enum tcti_system_access_selector_form form,
	enum tcti_system_access_selector_field field,
	const struct tcti_system_access_source_value *source,
	struct tcti_system_access_value_scratch *scratch,
	struct tcti_system_access_normalized_value *normalized)
{
	uint8_t expected_width;
	size_t initial_count;
	size_t index;
	uint8_t total = 0;

	if (!source || !scratch || !normalized ||
	    tcti_system_access_selector_field_width(form, field, &expected_width))
		return -EINVAL;
	if (source->kind != TCTI_SYSTEM_ACCESS_SOURCE_GROUP)
		return normalize_atom(field, expected_width, 0U, source, normalized);
	if (!source->group.members || !source->group.member_count ||
	    source->group.member_count > TCTI_SYSTEM_ACCESS_VALUE_MAX_GROUP_MEMBERS ||
	    !scratch->members || scratch->member_count > scratch->member_capacity ||
	    source->group.member_count > scratch->member_capacity - scratch->member_count)
		return -EINVAL;

	initial_count = scratch->member_count;
	for (index = 0; index < source->group.member_count; index++) {
		const struct tcti_system_access_source_value *member =
			&source->group.members[index];
		struct tcti_system_access_normalized_value *output =
			&scratch->members[scratch->member_count];
		uint8_t member_width;

		if (member->kind == TCTI_SYSTEM_ACCESS_SOURCE_GROUP)
			goto invalid;
		if (member->kind == TCTI_SYSTEM_ACCESS_SOURCE_VALUE) {
			if (!member->value.length || member->value.length < 3U ||
			    member->value.length - 2U > expected_width)
				goto invalid;
			member_width = (uint8_t)(member->value.length - 2U);
		} else if (member->kind == TCTI_SYSTEM_ACCESS_SOURCE_EQUATION) {
			if (!member->equation.slice_count ||
			    member->equation.slice_count >
				TCTI_SYSTEM_ACCESS_VALUE_MAX_SLICES)
				goto invalid;
			member_width = 0;
			for (size_t slice = 0; slice < member->equation.slice_count;
			     slice++) {
				if (!member->equation.slices[slice].width ||
				    member_width > expected_width -
					member->equation.slices[slice].width)
					goto invalid;
				member_width = (uint8_t)(member_width +
					member->equation.slices[slice].width);
			}
		} else {
			goto invalid;
		}
		if (!member_width || total > expected_width - member_width ||
		    normalize_atom(field, member_width, 0U, member, output))
			goto invalid;
		total = (uint8_t)(total + member_width);
		scratch->member_count++;
	}
	if (total != expected_width)
		goto invalid;

	/* Arm Values.Group members are ordered most-significant to least-significant. */
	for (index = initial_count; index < scratch->member_count; index++) {
		total = (uint8_t)(total - scratch->members[index].width);
		scratch->members[index].lsb = total;
	}
	normalized->kind = TCTI_SYSTEM_ACCESS_NORMALIZED_GROUP;
	normalized->field = field;
	normalized->width = expected_width;
	normalized->lsb = 0U;
	normalized->group.first_member = initial_count;
	normalized->group.member_count = source->group.member_count;
	return 0;

invalid:
	scratch->member_count = initial_count;
	return -EINVAL;
}

int tcti_system_access_mrs_msr_pack(
	const struct tcti_system_access_mrs_msr_fields *fields,
	uint16_t *selector)
{
	if (!fields || !selector || fields->op0 < 2U || fields->op0 > 3U ||
	    fields->op1 > 7U || fields->crn > 15U || fields->crm > 15U ||
	    fields->op2 > 7U)
		return -EINVAL;

	/* Instruction bit 20 is fixed by this class.  op0 is encoded at bit 19. */
	*selector = (uint16_t)(((uint16_t)(fields->op0 - 2U) << 14U) |
				       ((uint16_t)fields->op1 << 11U) |
				       ((uint16_t)fields->crn << 7U) |
				       ((uint16_t)fields->crm << 3U) |
				       (uint16_t)fields->op2);
	return 0;
}

int tcti_system_access_sys_sysl_pack(
	const struct tcti_system_access_sys_sysl_fields *fields,
	uint16_t *selector)
{
	if (!fields || !selector || fields->op1 > 7U || fields->crn > 15U ||
	    fields->crm > 15U || fields->op2 > 7U)
		return -EINVAL;

	*selector = (uint16_t)(((uint16_t)fields->op1 << 11U) |
				       ((uint16_t)fields->crn << 7U) |
				       ((uint16_t)fields->crm << 3U) |
				       (uint16_t)fields->op2);
	return 0;
}

int tcti_system_access_selector_width(
	enum tcti_system_access_selector_form form, uint8_t *width)
{
	if (!width)
		return -EINVAL;

	switch (form) {
	case TCTI_SYSTEM_ACCESS_SELECTOR_MRS:
	case TCTI_SYSTEM_ACCESS_SELECTOR_MSR:
		*width = TCTI_SYSTEM_ACCESS_MRS_MSR_SELECTOR_WIDTH;
		return 0;
	case TCTI_SYSTEM_ACCESS_SELECTOR_SYS:
	case TCTI_SYSTEM_ACCESS_SELECTOR_SYSL:
		*width = TCTI_SYSTEM_ACCESS_SYS_SYSL_SELECTOR_WIDTH;
		return 0;
	default:
		return -EINVAL;
	}
}

int tcti_system_access_selector_enumerate(
	enum tcti_system_access_selector_form form,
	tcti_system_access_selector_callback callback, void *context)
{
	uint8_t width;
	unsigned int selector;
	unsigned int count;
	int result;

	if (!callback || tcti_system_access_selector_width(form, &width))
		return -EINVAL;
	count = 1U << width;
	for (selector = 0; selector < count; selector++) {
		result = callback((uint16_t)selector, context);
		if (result)
			return result;
	}
	return 0;
}
