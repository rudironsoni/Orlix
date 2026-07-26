// SPDX-License-Identifier: GPL-2.0-only
#include <linux/types.h>

#include <asm/hosted_tls_repair.h>

#define ORLIX_HOSTED_TLS_MRS_TPIDR_EL0		0xd53bd040U
#define ORLIX_HOSTED_TLS_MRS_TPIDR_EL0_MASK	0xffffffe0U
#define ORLIX_HOSTED_TLS_LOAD_STORE_CLASS_SHIFT	25U
#define ORLIX_HOSTED_TLS_LOAD_STORE_CLASS_MASK	0x7U
#define ORLIX_HOSTED_TLS_LOAD_STORE_CLASS_VALUE	0x4U
#define ORLIX_HOSTED_TLS_REGISTER_MASK		0x1fU
#define ORLIX_HOSTED_TLS_REBASE_WINDOW		4096UL

static bool orlix_hosted_valid_user_tls(unsigned long user_base,
					unsigned long user_limit,
					unsigned long tls)
{
	return user_base < user_limit &&
	       tls >= user_base &&
	       tls < user_limit &&
	       !(tls & (sizeof(unsigned long) - 1U));
}

static unsigned long orlix_hosted_host_tls_reference(
	const struct orlix_host_user_tls_repair_request *request)
{
	if (request->live_host_tls &&
	    !orlix_hosted_valid_user_tls(request->user_base,
					 request->user_limit,
					 request->live_host_tls))
		return request->live_host_tls;

	return request->installed_host_tls;
}

static bool orlix_hosted_memory_base_register(unsigned int instruction,
					      unsigned int *base_register)
{
	unsigned int instruction_class =
		(instruction >> ORLIX_HOSTED_TLS_LOAD_STORE_CLASS_SHIFT) &
		ORLIX_HOSTED_TLS_LOAD_STORE_CLASS_MASK;
	unsigned int reg =
		(instruction >> 5U) & ORLIX_HOSTED_TLS_REGISTER_MASK;

	if (!base_register ||
	    instruction_class != ORLIX_HOSTED_TLS_LOAD_STORE_CLASS_VALUE ||
	    reg == 31U)
		return false;

	*base_register = reg;
	return true;
}

static bool orlix_hosted_mrs_tpidr_el0_destination(
	unsigned int instruction,
	unsigned int *destination_register)
{
	if (!destination_register ||
	    (instruction & ORLIX_HOSTED_TLS_MRS_TPIDR_EL0_MASK) !=
		ORLIX_HOSTED_TLS_MRS_TPIDR_EL0)
		return false;

	*destination_register = instruction & ORLIX_HOSTED_TLS_REGISTER_MASK;
	return true;
}

static bool orlix_hosted_integer_instruction_writes_register(
	unsigned int instruction,
	unsigned int reg)
{
	if (reg == 31U ||
	    (instruction & ORLIX_HOSTED_TLS_REGISTER_MASK) != reg)
		return false;

	if ((instruction & 0x1f000000U) == 0x10000000U)
		return true; /* ADR/ADRP */
	if ((instruction & 0x1f000000U) == 0x11000000U)
		return true; /* ADD/SUB immediate */
	if ((instruction & 0x1f800000U) == 0x12000000U)
		return true; /* logical immediate */
	if ((instruction & 0x1f800000U) == 0x12800000U)
		return true; /* move wide immediate */
	if ((instruction & 0x1f800000U) == 0x13000000U)
		return true; /* bitfield */
	if ((instruction & 0x1f800000U) == 0x13800000U)
		return true; /* extract */
	if ((instruction & 0x1f200000U) == 0x0a000000U)
		return true; /* logical shifted register */
	if ((instruction & 0x1f200000U) == 0x0b000000U)
		return true; /* ADD/SUB shifted register */

	return false;
}

static bool orlix_hosted_rebase_register_from_host_tls(
	unsigned long host_tls,
	unsigned long active_user_tls,
	unsigned long register_value,
	unsigned long user_base,
	unsigned long user_limit,
	unsigned long *rebased_value)
{
	unsigned long delta;
	unsigned long corrected;

	if (!host_tls || !active_user_tls || !rebased_value ||
	    user_base >= user_limit ||
	    active_user_tls < user_base ||
	    active_user_tls >= user_limit ||
	    (register_value >= user_base && register_value < user_limit))
		return false;

	if (host_tls >= register_value) {
		delta = host_tls - register_value;
		if (delta > ORLIX_HOSTED_TLS_REBASE_WINDOW ||
		    active_user_tls < delta)
			return false;
		corrected = active_user_tls - delta;
	} else {
		delta = register_value - host_tls;
		if (delta > ORLIX_HOSTED_TLS_REBASE_WINDOW ||
		    active_user_tls > ~0UL - delta)
			return false;
		corrected = active_user_tls + delta;
	}

	if (corrected < user_base || corrected >= user_limit)
		return false;

	*rebased_value = corrected;
	return true;
}

int orlix_hosted_decide_user_tls_repair(
	const struct orlix_host_user_tls_repair_request *request,
	struct orlix_host_user_tls_repair_decision *decision)
{
	unsigned long host_tls;
	unsigned long rebased_value;
	unsigned int base_register;
	unsigned int index;

	if (!request || !decision ||
	    request->user_base >= request->user_limit ||
	    (request->fault_address >= request->user_base &&
	     request->fault_address < request->user_limit) ||
	    !orlix_hosted_valid_user_tls(request->user_base,
					 request->user_limit,
					 request->active_user_tls) ||
	    request->instruction_history_count >
		ORLIX_HOST_USER_TLS_REPAIR_HISTORY_COUNT ||
	    !orlix_hosted_memory_base_register(request->faulting_instruction,
					       &base_register))
		return 0;

	for (index = 0; index < request->instruction_history_count; index++) {
		unsigned int destination_register;
		unsigned int instruction = request->instruction_history[index];

		if (orlix_hosted_mrs_tpidr_el0_destination(
			instruction, &destination_register) &&
		    destination_register == base_register) {
			decision->register_index = base_register;
			decision->register_value = request->active_user_tls;
			decision->user_tls = request->active_user_tls;
			return 1;
		}
		if (orlix_hosted_integer_instruction_writes_register(
			instruction, base_register))
			break;
	}

	host_tls = orlix_hosted_host_tls_reference(request);
	if (!orlix_hosted_rebase_register_from_host_tls(
			host_tls,
			request->active_user_tls,
			request->regs[base_register],
			request->user_base,
			request->user_limit,
			&rebased_value))
		return 0;

	decision->register_index = base_register;
	decision->register_value = rebased_value;
	decision->user_tls = request->active_user_tls;
	return 1;
}
