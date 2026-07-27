// SPDX-License-Identifier: GPL-2.0-only
#include <linux/types.h>

#include "decode_aarch64.h"
#include "tls_repair.h"

#define ORLIX_TCTI_TLS_REBASE_WINDOW 4096UL

static bool orlix_tcti_valid_user_tls(unsigned long user_base,
				      unsigned long user_limit,
				      unsigned long tls)
{
	return user_base < user_limit &&
	       tls >= user_base &&
	       tls < user_limit &&
	       !(tls & (sizeof(unsigned long) - 1U));
}

static unsigned long orlix_tcti_host_tls_reference(
	const struct orlix_host_user_tls_repair_request *request)
{
	if (request->live_host_tls &&
	    !orlix_tcti_valid_user_tls(request->user_base,
				       request->user_limit,
				       request->live_host_tls))
		return request->live_host_tls;

	return request->installed_host_tls;
}

static bool orlix_tcti_add_memory_offset(unsigned long base, s64 offset,
					 unsigned long *address)
{
	unsigned long magnitude;

	if (!address)
		return false;
	if (offset >= 0) {
		if (base > ~0UL - (unsigned long)offset)
			return false;
		*address = base + (unsigned long)offset;
		return true;
	}

	magnitude = (unsigned long)(-(offset + 1)) + 1UL;
	if (base < magnitude)
		return false;
	*address = base - magnitude;
	return true;
}

static bool orlix_tcti_memory_effective_address(
	const struct orlix_tcti_decoded_instruction *decoded,
	unsigned long base,
	unsigned long *effective_address)
{
	if (!decoded || !effective_address || decoded->rn == 31U)
		return false;

	switch (decoded->decode_class) {
	case ORLIX_TCTI_DECODE_LOAD_STORE_UNSIGNED_IMMEDIATE:
		return orlix_tcti_add_memory_offset(base,
						 decoded->memory_offset,
						 effective_address);
	case ORLIX_TCTI_DECODE_LOAD_STORE_SIGNED_IMMEDIATE:
		if (decoded->memory_index_mode == ORLIX_TCTI_MEMORY_INDEX_POST) {
			*effective_address = base;
			return true;
		}
		return orlix_tcti_add_memory_offset(base,
						 decoded->memory_offset,
						 effective_address);
	default:
		return false;
	}
}

static bool orlix_tcti_fault_memory_base_register(
	const struct orlix_tcti_decoded_instruction *decoded,
	const struct orlix_host_user_tls_repair_request *request,
	unsigned int *base_register)
{
	unsigned long effective_address;

	if (!decoded || !request || !base_register || decoded->rn == 31U ||
	    !orlix_tcti_memory_effective_address(
		    decoded, request->regs[decoded->rn], &effective_address))
		return false;

	if (effective_address != request->fault_address)
		return false;

	*base_register = decoded->rn;
	return true;
}

static bool orlix_tcti_rebase_register_from_host_tls(
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
		if (delta > ORLIX_TCTI_TLS_REBASE_WINDOW ||
		    active_user_tls < delta)
			return false;
		corrected = active_user_tls - delta;
	} else {
		delta = register_value - host_tls;
		if (delta > ORLIX_TCTI_TLS_REBASE_WINDOW ||
		    active_user_tls > ~0UL - delta)
			return false;
		corrected = active_user_tls + delta;
	}

	if (corrected < user_base || corrected >= user_limit)
		return false;

	*rebased_value = corrected;
	return true;
}

int orlix_tcti_decide_user_tls_repair(
	const struct orlix_host_user_tls_repair_request *request,
	struct orlix_host_user_tls_repair_decision *decision)
{
	struct orlix_tcti_decoded_instruction decoded;
	unsigned long repaired_address;
	unsigned long host_tls;
	unsigned long rebased_value;
	unsigned int base_register;

	if (!request || !decision ||
	    request->user_base >= request->user_limit ||
	    (request->fault_address >= request->user_base &&
	     request->fault_address < request->user_limit) ||
	    !orlix_tcti_valid_user_tls(request->user_base,
				       request->user_limit,
				       request->active_user_tls))
		return 0;

	decoded = orlix_tcti_decode_aarch64(request->faulting_instruction);
	if (!orlix_tcti_fault_memory_base_register(
			&decoded, request, &base_register))
		return 0;

	host_tls = orlix_tcti_host_tls_reference(request);
	if (!orlix_tcti_rebase_register_from_host_tls(
			host_tls,
			request->active_user_tls,
			request->regs[base_register],
			request->user_base,
			request->user_limit,
			&rebased_value))
		return 0;
	if (!orlix_tcti_memory_effective_address(
			&decoded, rebased_value, &repaired_address) ||
	    repaired_address < request->user_base ||
	    repaired_address >= request->user_limit)
		return 0;

	decision->register_index = base_register;
	decision->register_value = rebased_value;
	decision->user_tls = request->active_user_tls;
	return 1;
}
