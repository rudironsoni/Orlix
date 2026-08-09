// SPDX-License-Identifier: GPL-2.0-only
#include <asm/hosted_exec.h>
#include <asm/ptrace.h>
#include <internal/asm/host_time.h>
#include <linux/array_size.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/string.h>

#include "decode_aarch64.h"
#include "fpmr_state.h"
#include "system_accessor.h"

#ifndef UINT64_C
#define UINT64_C(value) value ## ULL
#endif

#define ORLIX_TCTI_ACCESSOR_READ 1U
#define ORLIX_TCTI_ACCESSOR_WRITE 2U
#define ORLIX_TCTI_ACCESSOR_IMPLEMENTED 1U
#define ORLIX_TCTI_FPCR_WRITABLE_MASK (GENMASK(26, 22) | BIT(15) | GENMASK(12, 8))
#define ORLIX_TCTI_FPSR_WRITABLE_MASK (BIT(27) | BIT(7) | GENMASK(4, 0))
#define ORLIX_TCTI_CTR_EL0_VALUE (BIT_ULL(29) | BIT_ULL(28) | (4ULL << 16) | \
	(3ULL << 14) | 4ULL)
#define ORLIX_TCTI_DCZID_EL0_VALUE BIT_ULL(4)
#define ORLIX_TCTI_CNTFRQ_EL0_VALUE 1000000000ULL

struct orlix_tcti_system_accessor_row {
	u32 accessor;
	u32 selector;
	const char *generic;
	const char *variant;
	u8 direction;
	u8 disposition;
	u8 implementation;
	u32 condition;
	u32 access;
	u64 selector_identity;
	u64 condition_identity;
	u64 access_identity;
	u64 decode_key;
	enum orlix_tcti_system_accessor_operation operation;
	const char *decoder_owner;
	const char *execution_owner;
};

#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR_SOURCE(...)
#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR_COUNTS(accessor_count, ...) \
	enum { ORLIX_TCTI_SYSTEM_ACCESSOR_ROW_COUNT = accessor_count };
#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR_SEMANTIC_COUNTS(...)
#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR_IDENTITY(...)
#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR(...)
#include "isa/target_system_accessor_reconciliation.def"
#undef ORLIX_TCTI_A64_SYSTEM_ACCESSOR
#undef ORLIX_TCTI_A64_SYSTEM_ACCESSOR_IDENTITY
#undef ORLIX_TCTI_A64_SYSTEM_ACCESSOR_SEMANTIC_COUNTS
#undef ORLIX_TCTI_A64_SYSTEM_ACCESSOR_COUNTS
#undef ORLIX_TCTI_A64_SYSTEM_ACCESSOR_SOURCE

#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR_SOURCE(...)
#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR_COUNTS(...)
#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR_SEMANTIC_COUNTS(...)
#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR_IDENTITY(...)
#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR(accessor, encoding, name, variant, generic, \
	direction, disposition, selectors, condition, access, concrete, applicability, \
	semantics, implementation, proof, selector_identity, condition_identity, access_identity, \
	decode_key, operation, decoder_owner, execution_owner, ...) \
	{ accessor, concrete, generic, variant, direction, disposition, implementation, condition, access, \
	  selector_identity, condition_identity, access_identity, decode_key, operation, decoder_owner, \
	  execution_owner },
static const struct orlix_tcti_system_accessor_row orlix_tcti_system_accessors[] = {
#include "isa/target_system_accessor_reconciliation.def"
};
_Static_assert(ARRAY_SIZE(orlix_tcti_system_accessors) ==
	ORLIX_TCTI_SYSTEM_ACCESSOR_ROW_COUNT,
	"generated SystemAccessor row count drifted");
#undef ORLIX_TCTI_A64_SYSTEM_ACCESSOR
#undef ORLIX_TCTI_A64_SYSTEM_ACCESSOR_IDENTITY
#undef ORLIX_TCTI_A64_SYSTEM_ACCESSOR_SEMANTIC_COUNTS
#undef ORLIX_TCTI_A64_SYSTEM_ACCESSOR_COUNTS
#undef ORLIX_TCTI_A64_SYSTEM_ACCESSOR_SOURCE

static const char *orlix_tcti_system_accessor_route_generic(
	enum orlix_tcti_system_accessor_route route, u8 *direction)
{
	switch (route) {
	case ORLIX_TCTI_SYSTEM_ACCESSOR_ROUTE_MRS:
		*direction = ORLIX_TCTI_ACCESSOR_READ;
		return "MRS_RS_systemmove";
	case ORLIX_TCTI_SYSTEM_ACCESSOR_ROUTE_MSR:
		*direction = ORLIX_TCTI_ACCESSOR_WRITE;
		return "MSR_SR_systemmove";
	case ORLIX_TCTI_SYSTEM_ACCESSOR_ROUTE_SYS:
		*direction = 3U;
		return "SYS_CR_systeminstrs";
	case ORLIX_TCTI_SYSTEM_ACCESSOR_ROUTE_SYSL:
		*direction = ORLIX_TCTI_ACCESSOR_READ;
		return "SYSL_RC_systeminstrs";
	case ORLIX_TCTI_SYSTEM_ACCESSOR_ROUTE_MSRR:
		*direction = ORLIX_TCTI_ACCESSOR_WRITE;
		return "MSRR_SR_systemmovepr";
	case ORLIX_TCTI_SYSTEM_ACCESSOR_ROUTE_MRRS:
		*direction = ORLIX_TCTI_ACCESSOR_READ;
		return "MRRS_RS_systemmovepr";
	default:
		return NULL;
	}
}

/*
 * A system-register instruction carries selector and direction only. Multiple
 * generated rows may therefore represent one encoding. Select the unsigned
 * canonical semantic identity, never source order, and reject ambiguity.
 */
static int orlix_tcti_system_accessor_semantic_key_compare(
	const struct orlix_tcti_system_accessor_row *left,
	const struct orlix_tcti_system_accessor_row *right)
{
	if (left->condition_identity != right->condition_identity)
		return left->condition_identity < right->condition_identity ? -1 : 1;
	if (left->access_identity != right->access_identity)
		return left->access_identity < right->access_identity ? -1 : 1;
	return 0;
}

static const struct orlix_tcti_system_accessor_row *
orlix_tcti_find_system_accessor(u16 selector,
	enum orlix_tcti_system_accessor_route route)
{
	const struct orlix_tcti_system_accessor_row *found = NULL;
	const char *generic;
	u8 direction;
	size_t index;

	generic = orlix_tcti_system_accessor_route_generic(route, &direction);
	if (!generic)
		return NULL;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_system_accessors); index++) {
		const struct orlix_tcti_system_accessor_row *row =
			&orlix_tcti_system_accessors[index];

		if (row->selector != selector || row->direction != direction ||
		    strcmp(row->generic, generic))
			continue;
		if (!found ||
		    orlix_tcti_system_accessor_semantic_key_compare(row, found) < 0)
			found = row;
		else if (!orlix_tcti_system_accessor_semantic_key_compare(row, found))
			return NULL;
	}
	return found;
}

static const struct orlix_tcti_system_accessor_row *
orlix_tcti_find_system_accessor_id(u32 accessor)
{
	const struct orlix_tcti_system_accessor_row *found = NULL;
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_system_accessors); index++) {
		const struct orlix_tcti_system_accessor_row *row =
			&orlix_tcti_system_accessors[index];

		if (row->accessor != accessor)
			continue;
		if (found)
			return NULL;
		found = row;
	}
	return found;
}

static void orlix_tcti_system_accessor_bind_decoded(
	const struct orlix_tcti_system_accessor_row *row,
	struct orlix_tcti_decoded_instruction *decoded)
{
	decoded->decode_class = ORLIX_TCTI_DECODE_SYSTEM_REGISTER;
	decoded->system_accessor_selector = row->selector;
	decoded->system_accessor_id = row->accessor;
	decoded->system_accessor_condition = row->condition;
	decoded->system_accessor_access = row->access;
	decoded->system_accessor_disposition = row->disposition;
	decoded->system_accessor_implementation = row->implementation;
	decoded->system_accessor_selector_identity = row->selector_identity;
	decoded->system_accessor_condition_identity = row->condition_identity;
	decoded->system_accessor_access_identity = row->access_identity;
	decoded->system_accessor_decode_key = row->decode_key;
	decoded->system_accessor_operation = row->operation;
	decoded->system_accessor_decoder_owner = row->decoder_owner;
	decoded->system_accessor_execution_owner = row->execution_owner;
	decoded->system_register_write = row->direction == ORLIX_TCTI_ACCESSOR_WRITE;
}

bool orlix_tcti_system_accessor_decode(
	u16 selector, bool write, struct orlix_tcti_decoded_instruction *decoded)
{
	const struct orlix_tcti_system_accessor_row *row;

	if (!decoded)
		return false;
	row = orlix_tcti_find_system_accessor(selector,
		write ? ORLIX_TCTI_SYSTEM_ACCESSOR_ROUTE_MSR :
			ORLIX_TCTI_SYSTEM_ACCESSOR_ROUTE_MRS);
	if (!row)
		return false;
	orlix_tcti_system_accessor_bind_decoded(row, decoded);
	return true;
}

bool orlix_tcti_system_accessor_decode_route(
	u16 selector, enum orlix_tcti_system_accessor_route route,
	struct orlix_tcti_decoded_instruction *decoded)
{
	const struct orlix_tcti_system_accessor_row *row;

	if (!decoded)
		return false;
	row = orlix_tcti_find_system_accessor(selector, route);
	if (!row)
		return false;
	orlix_tcti_system_accessor_bind_decoded(row, decoded);
	return true;
}

int orlix_tcti_execute_system_register(
	struct pt_regs *regs, const struct orlix_tcti_decoded_instruction *decoded)
{
	const struct orlix_tcti_system_accessor_row *row;
	enum orlix_tcti_system_accessor_operation operation;
	u64 value;

	if (!regs || !decoded)
		return -EINVAL;
	row = orlix_tcti_find_system_accessor_id(decoded->system_accessor_id);
	if (!row || decoded->system_accessor_id != row->accessor ||
	    decoded->system_accessor_condition != row->condition ||
	    decoded->system_accessor_access != row->access ||
	    decoded->system_accessor_disposition != row->disposition ||
	    decoded->system_accessor_implementation != row->implementation ||
	    decoded->system_accessor_selector_identity != row->selector_identity ||
	    decoded->system_accessor_condition_identity != row->condition_identity ||
	    decoded->system_accessor_access_identity != row->access_identity ||
	    decoded->system_accessor_decode_key != row->decode_key ||
	    decoded->system_accessor_operation != row->operation ||
	    decoded->system_accessor_decoder_owner != row->decoder_owner ||
	    decoded->system_accessor_execution_owner != row->execution_owner ||
	    row->implementation != ORLIX_TCTI_ACCESSOR_IMPLEMENTED)
		return -EOPNOTSUPP;
	operation = row->operation;
	if (operation == ORLIX_TCTI_SYSTEM_ACCESSOR_OPERATION_NONE)
		return -EOPNOTSUPP;
	value = decoded->rt == 31 ? 0 : regs->regs[decoded->rt];

	switch (operation) {
	case ORLIX_TCTI_SYSTEM_ACCESSOR_OPERATION_TPIDR_EL0_WRITE:
#if defined(ORLIX_APP_HOSTED_BOOT)
		orlix_hosted_set_current_user_tls(value);
#else
		current->thread.user_tls = value;
#endif
		break;
	case ORLIX_TCTI_SYSTEM_ACCESSOR_OPERATION_TPIDR_EL0_READ:
		if (decoded->rt != 31)
			regs->regs[decoded->rt] = current->thread.user_tls;
		break;
	case ORLIX_TCTI_SYSTEM_ACCESSOR_OPERATION_NZCV_WRITE:
		regs->pstate &= ~(PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT);
		regs->pstate |= value & (PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT);
		break;
	case ORLIX_TCTI_SYSTEM_ACCESSOR_OPERATION_NZCV_READ:
		if (decoded->rt != 31)
			regs->regs[decoded->rt] = regs->pstate &
				(PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT);
		break;
	case ORLIX_TCTI_SYSTEM_ACCESSOR_OPERATION_FPCR_WRITE:
		current->thread.user_fpcr = value & ORLIX_TCTI_FPCR_WRITABLE_MASK;
		break;
	case ORLIX_TCTI_SYSTEM_ACCESSOR_OPERATION_FPCR_READ:
		if (decoded->rt != 31)
			regs->regs[decoded->rt] = current->thread.user_fpcr;
		break;
	case ORLIX_TCTI_SYSTEM_ACCESSOR_OPERATION_FPSR_WRITE:
		current->thread.user_fpsr = value & ORLIX_TCTI_FPSR_WRITABLE_MASK;
		break;
	case ORLIX_TCTI_SYSTEM_ACCESSOR_OPERATION_FPSR_READ:
		if (decoded->rt != 31)
			regs->regs[decoded->rt] = current->thread.user_fpsr;
		break;
	case ORLIX_TCTI_SYSTEM_ACCESSOR_OPERATION_FPMR_WRITE:
		orlix_tcti_fpmr_write_current(value);
		break;
	case ORLIX_TCTI_SYSTEM_ACCESSOR_OPERATION_FPMR_READ:
		if (decoded->rt != 31)
			regs->regs[decoded->rt] = orlix_tcti_fpmr_current();
		break;
	case ORLIX_TCTI_SYSTEM_ACCESSOR_OPERATION_TPIDRRO_EL0_READ:
		if (decoded->rt != 31)
			regs->regs[decoded->rt] = 0;
		break;
	case ORLIX_TCTI_SYSTEM_ACCESSOR_OPERATION_CTR_EL0_READ:
		if (decoded->rt != 31)
			regs->regs[decoded->rt] = ORLIX_TCTI_CTR_EL0_VALUE;
		break;
	case ORLIX_TCTI_SYSTEM_ACCESSOR_OPERATION_DCZID_EL0_READ:
		if (decoded->rt != 31)
			regs->regs[decoded->rt] = ORLIX_TCTI_DCZID_EL0_VALUE;
		break;
	case ORLIX_TCTI_SYSTEM_ACCESSOR_OPERATION_CNTFRQ_EL0_READ:
		if (decoded->rt != 31)
			regs->regs[decoded->rt] = ORLIX_TCTI_CNTFRQ_EL0_VALUE;
		break;
	case ORLIX_TCTI_SYSTEM_ACCESSOR_OPERATION_CNTVCT_EL0_READ:
		if (decoded->rt != 31)
			regs->regs[decoded->rt] = orlix_host_time_monotonic_ns();
		break;
	default:
		return -EOPNOTSUPP;
	}
	regs->pc += sizeof(u32);
	return 0;
}
