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
#include "system_accessor.h"

#define ORLIX_TCTI_ACCESSOR_READ 1U
#define ORLIX_TCTI_ACCESSOR_WRITE 2U
#define ORLIX_TCTI_ACCESSOR_IMPLEMENTED 1U
#define ORLIX_TCTI_FPCR_WRITABLE_MASK (GENMASK(26, 22) | BIT(15) | GENMASK(12, 8))
#define ORLIX_TCTI_FPSR_WRITABLE_MASK (BIT(27) | BIT(7) | GENMASK(4, 0))
#define ORLIX_TCTI_CTR_EL0_VALUE (BIT_ULL(29) | BIT_ULL(28) | (4ULL << 16) | \
	(3ULL << 14) | 4ULL)
#define ORLIX_TCTI_DCZID_EL0_VALUE BIT_ULL(4)
#define ORLIX_TCTI_CNTFRQ_EL0_VALUE 1000000000ULL

enum orlix_tcti_system_accessor_operation {
	ORLIX_TCTI_SYSTEM_ACCESSOR_UNKNOWN,
	ORLIX_TCTI_SYSTEM_ACCESSOR_CNTFRQ_EL0_READ,
	ORLIX_TCTI_SYSTEM_ACCESSOR_CNTVCT_EL0_READ,
	ORLIX_TCTI_SYSTEM_ACCESSOR_CTR_EL0_READ,
	ORLIX_TCTI_SYSTEM_ACCESSOR_DCZID_EL0_READ,
	ORLIX_TCTI_SYSTEM_ACCESSOR_FPCR_READ,
	ORLIX_TCTI_SYSTEM_ACCESSOR_FPCR_WRITE,
	ORLIX_TCTI_SYSTEM_ACCESSOR_FPSR_READ,
	ORLIX_TCTI_SYSTEM_ACCESSOR_FPSR_WRITE,
	ORLIX_TCTI_SYSTEM_ACCESSOR_NZCV_READ,
	ORLIX_TCTI_SYSTEM_ACCESSOR_NZCV_WRITE,
	ORLIX_TCTI_SYSTEM_ACCESSOR_TPIDR_EL0_READ,
	ORLIX_TCTI_SYSTEM_ACCESSOR_TPIDR_EL0_WRITE,
	ORLIX_TCTI_SYSTEM_ACCESSOR_TPIDRRO_EL0_READ,
};

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
	const char *decoder_owner;
	const char *execution_owner;
};

#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR_SOURCE(...)
#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR_COUNTS(accessor_count, ...) \
	enum { ORLIX_TCTI_SYSTEM_ACCESSOR_ROW_COUNT = accessor_count };
#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR_SEMANTIC_COUNTS(...)
#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR_IDENTITY(...)
#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR(accessor, encoding, name, variant, generic, \
	direction, disposition, selectors, condition, access, concrete, applicability, \
	semantics, implementation, selector_identity, condition_identity, access_identity, \
	decoder_owner, execution_owner, ...) \
	{ accessor, concrete, generic, variant, direction, disposition, implementation, condition, access, \
	  selector_identity, condition_identity, access_identity, decoder_owner, \
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

static bool orlix_tcti_system_accessor_is_mrs_msr(
	const struct orlix_tcti_system_accessor_row *row)
{
	return !strcmp(row->generic, "MRS_RS_systemmove") ||
		!strcmp(row->generic, "MSR_SR_systemmove");
}

/*
 * An instruction supplies only selector and direction.  Source rows sharing
 * that encoding are aliases, so decode deterministically selects the lowest
 * generated semantic key, never a source ordinal or a selector allowlist.
 */
static struct orlix_tcti_system_accessor_semantic_key
orlix_tcti_system_accessor_semantic_key(
	const struct orlix_tcti_system_accessor_row *row)
{
	return (struct orlix_tcti_system_accessor_semantic_key) {
		.condition_identity = row->condition_identity,
		.access_identity = row->access_identity,
	};
}

static int orlix_tcti_system_accessor_semantic_key_compare(
	const struct orlix_tcti_system_accessor_row *left,
	const struct orlix_tcti_system_accessor_row *right)
{
	struct orlix_tcti_system_accessor_semantic_key left_key =
		orlix_tcti_system_accessor_semantic_key(left);
	struct orlix_tcti_system_accessor_semantic_key right_key =
		orlix_tcti_system_accessor_semantic_key(right);

	if (left_key.condition_identity != right_key.condition_identity)
		return left_key.condition_identity < right_key.condition_identity ? -1 : 1;
	if (left_key.access_identity != right_key.access_identity)
		return left_key.access_identity < right_key.access_identity ? -1 : 1;
	return 0;
}

static const struct orlix_tcti_system_accessor_row *
orlix_tcti_find_system_accessor(u16 selector, bool write)
{
	const struct orlix_tcti_system_accessor_row *found = NULL;
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_system_accessors); index++) {
		const struct orlix_tcti_system_accessor_row *row =
			&orlix_tcti_system_accessors[index];

		if (!orlix_tcti_system_accessor_is_mrs_msr(row) ||
		    row->selector != selector ||
		    row->direction != (write ? ORLIX_TCTI_ACCESSOR_WRITE :
						ORLIX_TCTI_ACCESSOR_READ))
			continue;
		if (!found || orlix_tcti_system_accessor_semantic_key_compare(row, found) < 0)
			found = row;
		else if (!orlix_tcti_system_accessor_semantic_key_compare(row, found))
			return NULL;
	}
	return found;
}

/*
 * Variant and direction are generated ledger identity.  Do not bind execution
 * semantics to generated row ordinals: source refreshes may renumber them.
 */
static enum orlix_tcti_system_accessor_operation
orlix_tcti_system_accessor_operation(
	const struct orlix_tcti_system_accessor_row *row)
{
	if (!strcmp(row->variant, "CNTFRQ_EL0"))
		return row->direction == ORLIX_TCTI_ACCESSOR_READ ?
			ORLIX_TCTI_SYSTEM_ACCESSOR_CNTFRQ_EL0_READ :
			ORLIX_TCTI_SYSTEM_ACCESSOR_UNKNOWN;
	if (!strcmp(row->variant, "CNTVCT_EL0"))
		return row->direction == ORLIX_TCTI_ACCESSOR_READ ?
			ORLIX_TCTI_SYSTEM_ACCESSOR_CNTVCT_EL0_READ :
			ORLIX_TCTI_SYSTEM_ACCESSOR_UNKNOWN;
	if (!strcmp(row->variant, "CTR_EL0"))
		return row->direction == ORLIX_TCTI_ACCESSOR_READ ?
			ORLIX_TCTI_SYSTEM_ACCESSOR_CTR_EL0_READ :
			ORLIX_TCTI_SYSTEM_ACCESSOR_UNKNOWN;
	if (!strcmp(row->variant, "DCZID_EL0"))
		return row->direction == ORLIX_TCTI_ACCESSOR_READ ?
			ORLIX_TCTI_SYSTEM_ACCESSOR_DCZID_EL0_READ :
			ORLIX_TCTI_SYSTEM_ACCESSOR_UNKNOWN;
	if (!strcmp(row->variant, "FPCR"))
		return row->direction == ORLIX_TCTI_ACCESSOR_READ ?
			ORLIX_TCTI_SYSTEM_ACCESSOR_FPCR_READ :
			ORLIX_TCTI_SYSTEM_ACCESSOR_FPCR_WRITE;
	if (!strcmp(row->variant, "FPSR"))
		return row->direction == ORLIX_TCTI_ACCESSOR_READ ?
			ORLIX_TCTI_SYSTEM_ACCESSOR_FPSR_READ :
			ORLIX_TCTI_SYSTEM_ACCESSOR_FPSR_WRITE;
	if (!strcmp(row->variant, "NZCV"))
		return row->direction == ORLIX_TCTI_ACCESSOR_READ ?
			ORLIX_TCTI_SYSTEM_ACCESSOR_NZCV_READ :
			ORLIX_TCTI_SYSTEM_ACCESSOR_NZCV_WRITE;
	if (!strcmp(row->variant, "TPIDR_EL0"))
		return row->direction == ORLIX_TCTI_ACCESSOR_READ ?
			ORLIX_TCTI_SYSTEM_ACCESSOR_TPIDR_EL0_READ :
			ORLIX_TCTI_SYSTEM_ACCESSOR_TPIDR_EL0_WRITE;
	if (!strcmp(row->variant, "TPIDRRO_EL0"))
		return row->direction == ORLIX_TCTI_ACCESSOR_READ ?
			ORLIX_TCTI_SYSTEM_ACCESSOR_TPIDRRO_EL0_READ :
			ORLIX_TCTI_SYSTEM_ACCESSOR_UNKNOWN;
	return ORLIX_TCTI_SYSTEM_ACCESSOR_UNKNOWN;
}

bool orlix_tcti_system_accessor_decode(
	u16 selector, bool write, struct orlix_tcti_decoded_instruction *decoded)
{
	const struct orlix_tcti_system_accessor_row *row;

	if (!decoded)
		return false;
	row = orlix_tcti_find_system_accessor(selector, write);
	if (!row)
		return false;
	decoded->decode_class = ORLIX_TCTI_DECODE_SYSTEM_REGISTER;
	decoded->system_accessor_selector = selector;
	decoded->system_accessor_id = row->accessor;
	decoded->system_accessor_condition = row->condition;
	decoded->system_accessor_access = row->access;
	decoded->system_accessor_disposition = row->disposition;
	decoded->system_accessor_implementation = row->implementation;
	decoded->system_accessor_selector_identity = row->selector_identity;
	decoded->system_accessor_condition_identity = row->condition_identity;
	decoded->system_accessor_access_identity = row->access_identity;
	decoded->system_accessor_semantic_key =
		orlix_tcti_system_accessor_semantic_key(row);
	decoded->system_accessor_decoder_owner = row->decoder_owner;
	decoded->system_accessor_execution_owner = row->execution_owner;
	decoded->system_register_write = write;
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
	row = orlix_tcti_find_system_accessor(decoded->system_accessor_selector,
			decoded->system_register_write);
	if (!row || decoded->system_accessor_id != row->accessor ||
	    decoded->system_accessor_condition != row->condition ||
	    decoded->system_accessor_access != row->access ||
	    decoded->system_accessor_disposition != row->disposition ||
	    decoded->system_accessor_implementation != row->implementation ||
	    decoded->system_accessor_selector_identity != row->selector_identity ||
	    decoded->system_accessor_condition_identity != row->condition_identity ||
	    decoded->system_accessor_access_identity != row->access_identity ||
	    decoded->system_accessor_semantic_key.condition_identity !=
		row->condition_identity ||
	    decoded->system_accessor_semantic_key.access_identity != row->access_identity ||
	    decoded->system_accessor_decoder_owner != row->decoder_owner ||
	    decoded->system_accessor_execution_owner != row->execution_owner ||
	    row->implementation != ORLIX_TCTI_ACCESSOR_IMPLEMENTED)
		return -EOPNOTSUPP;
	operation = orlix_tcti_system_accessor_operation(row);
	if (operation == ORLIX_TCTI_SYSTEM_ACCESSOR_UNKNOWN)
		return -EOPNOTSUPP;
	value = decoded->rt == 31 ? 0 : regs->regs[decoded->rt];

	switch (operation) {
	case ORLIX_TCTI_SYSTEM_ACCESSOR_TPIDR_EL0_WRITE:
#if defined(ORLIX_APP_HOSTED_BOOT)
		orlix_hosted_set_current_user_tls(value);
#else
		current->thread.user_tls = value;
#endif
		break;
	case ORLIX_TCTI_SYSTEM_ACCESSOR_TPIDR_EL0_READ:
		if (decoded->rt != 31)
			regs->regs[decoded->rt] = current->thread.user_tls;
		break;
	case ORLIX_TCTI_SYSTEM_ACCESSOR_NZCV_WRITE:
		regs->pstate &= ~(PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT);
		regs->pstate |= value & (PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT);
		break;
	case ORLIX_TCTI_SYSTEM_ACCESSOR_NZCV_READ:
		if (decoded->rt != 31)
			regs->regs[decoded->rt] = regs->pstate &
				(PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT);
		break;
	case ORLIX_TCTI_SYSTEM_ACCESSOR_FPCR_WRITE:
		current->thread.user_fpcr = value & ORLIX_TCTI_FPCR_WRITABLE_MASK;
		break;
	case ORLIX_TCTI_SYSTEM_ACCESSOR_FPCR_READ:
		if (decoded->rt != 31)
			regs->regs[decoded->rt] = current->thread.user_fpcr;
		break;
	case ORLIX_TCTI_SYSTEM_ACCESSOR_FPSR_WRITE:
		current->thread.user_fpsr = value & ORLIX_TCTI_FPSR_WRITABLE_MASK;
		break;
	case ORLIX_TCTI_SYSTEM_ACCESSOR_FPSR_READ:
		if (decoded->rt != 31)
			regs->regs[decoded->rt] = current->thread.user_fpsr;
		break;
	case ORLIX_TCTI_SYSTEM_ACCESSOR_TPIDRRO_EL0_READ:
		if (decoded->rt != 31)
			regs->regs[decoded->rt] = 0;
		break;
	case ORLIX_TCTI_SYSTEM_ACCESSOR_CTR_EL0_READ:
		if (decoded->rt != 31)
			regs->regs[decoded->rt] = ORLIX_TCTI_CTR_EL0_VALUE;
		break;
	case ORLIX_TCTI_SYSTEM_ACCESSOR_DCZID_EL0_READ:
		if (decoded->rt != 31)
			regs->regs[decoded->rt] = ORLIX_TCTI_DCZID_EL0_VALUE;
		break;
	case ORLIX_TCTI_SYSTEM_ACCESSOR_CNTFRQ_EL0_READ:
		if (decoded->rt != 31)
			regs->regs[decoded->rt] = ORLIX_TCTI_CNTFRQ_EL0_VALUE;
		break;
	case ORLIX_TCTI_SYSTEM_ACCESSOR_CNTVCT_EL0_READ:
		if (decoded->rt != 31)
			regs->regs[decoded->rt] = orlix_host_time_monotonic_ns();
		break;
	default:
		return -EOPNOTSUPP;
	}
	regs->pc += sizeof(u32);
	return 0;
}
