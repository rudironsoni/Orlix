// SPDX-License-Identifier: GPL-2.0-only
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/overflow.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/utsname.h>

#include "orlix_tcti_native_observation.h"
#include "target_instruction_artifact.h"
#include "target_proof_ingestion_private.h"

#define ORLIX_TCTI_NATIVE_OBSERVATION_MAGIC 0x54434f42U

#define ORLIX_TCTI_NATIVE_HAVE_EXECUTION BIT(0)
#define ORLIX_TCTI_NATIVE_HAVE_WITNESS BIT(1)

enum orlix_tcti_native_internal_path {
	ORLIX_TCTI_NATIVE_INTERNAL_PATH_NONE,
	ORLIX_TCTI_NATIVE_INTERNAL_PATH_RESUME_USER,
};

struct orlix_tcti_native_owned_bytes {
	size_t size;
	u8 *data;
};

struct orlix_tcti_native_owned_sve {
	u16 vl_bytes;
	struct orlix_tcti_native_owned_bytes z;
	struct orlix_tcti_native_owned_bytes p;
	struct orlix_tcti_native_owned_bytes ffr;
	bool valid;
};

struct orlix_tcti_native_owned_sme {
	bool sm_present;
	bool streaming_mode;
	bool za_control_present;
	bool za_enabled;
	bool vl_present;
	u16 vl_bytes;
	bool svl_present;
	u16 svl_bytes;
	bool za_applicable;
	bool za_present;
	struct orlix_tcti_native_owned_bytes za;
	bool zt0_applicable;
	bool zt0_present;
	struct orlix_tcti_native_owned_bytes zt0;
	bool valid;
	bool production_available;
};

union orlix_tcti_native_owned_witness {
	struct {
		unsigned long address;
		struct orlix_tcti_native_owned_bytes bytes;
	} memory;
	struct orlix_tcti_native_fp_simd_state fp_simd;
	struct orlix_tcti_native_owned_sve sve;
	struct orlix_tcti_native_owned_sme sme;
	struct orlix_tcti_native_fault_witness fault;
	struct orlix_tcti_native_atomicity_witness atomicity;
	struct orlix_tcti_native_ordering_witness ordering;
};

struct orlix_tcti_native_observation {
	u32 magic;
	u8 expected_mask;
	u8 observed_mask;
	enum orlix_tcti_native_observation_state state;
	u32 expected_source_ordinal;
	enum orlix_tcti_native_obligation expected_obligation;
	enum orlix_tcti_native_internal_path execution_path;
	u32 execution_count;
	bool execution_attempted;
	bool source_bound;
	bool poisoned;
	bool exported;
	struct orlix_tcti_result expected_result;
	struct orlix_tcti_result observed_result;
	struct orlix_tcti_native_gpr_state expected_gpr;
	struct orlix_tcti_native_gpr_state observed_gpr;
	union orlix_tcti_native_owned_witness expected;
	union orlix_tcti_native_owned_witness observed;
};

static bool orlix_tcti_native_obligation_valid(
		enum orlix_tcti_native_obligation obligation)
{
	return obligation >= ORLIX_TCTI_NATIVE_OBLIGATION_RESULT &&
	       obligation <= ORLIX_TCTI_NATIVE_OBLIGATION_ORDERING;
}

static bool orlix_tcti_native_reason_valid(enum orlix_tcti_exit_reason reason)
{
	return reason >= ORLIX_TCTI_EXIT_SYSCALL &&
	       reason <= ORLIX_TCTI_EXIT_ALIGNMENT_FAULT;
}

static bool orlix_tcti_native_access_valid(enum orlix_tcti_access access)
{
	return access >= ORLIX_TCTI_ACCESS_FETCH &&
	       access <= ORLIX_TCTI_ACCESS_WRITE;
}

static bool orlix_tcti_native_result_valid(
		const struct orlix_tcti_result *result)
{
	return result && orlix_tcti_native_reason_valid(result->reason) &&
	       orlix_tcti_native_access_valid(result->fault_access);
}

static bool orlix_tcti_native_vl_valid(u16 vl_bytes)
{
	return vl_bytes >= ORLIX_TCTI_SVE_MIN_VL_BYTES &&
	       vl_bytes <= ORLIX_TCTI_SVE_MAX_VL_BYTES &&
	       !(vl_bytes % ORLIX_TCTI_SVE_MIN_VL_BYTES);
}

static bool orlix_tcti_native_atomicity_structurally_valid(
		const struct orlix_tcti_native_atomicity_witness *witness)
{
	bool width_valid;

	if (!witness)
		return false;
	width_valid = witness->width == 1 || witness->width == 2 ||
		      witness->width == 4 || witness->width == 8 ||
		      witness->width == 16;
	return witness->valid && width_valid;
}

static bool orlix_tcti_native_atomicity_success(
		const struct orlix_tcti_native_atomicity_witness *witness)
{
	return orlix_tcti_native_atomicity_structurally_valid(witness) &&
	       witness->completed && witness->linearization_count == 1 &&
	       !witness->torn && !witness->forbidden_outcome;
}

static bool orlix_tcti_native_ordering_structurally_valid(
		const struct orlix_tcti_native_ordering_witness *witness)
{
	return witness && witness->valid &&
	       witness->order >= ORLIX_TCTI_ATOMIC_MEMORY_ACQUIRE &&
	       witness->order <= ORLIX_TCTI_ATOMIC_MEMORY_ACQ_REL;
}

static bool orlix_tcti_native_ordering_success(
		const struct orlix_tcti_native_ordering_witness *witness)
{
	return orlix_tcti_native_ordering_structurally_valid(witness) &&
	       witness->successor_epoch > witness->predecessor_epoch &&
	       witness->edge_observed && !witness->forbidden_outcome;
}

static bool orlix_tcti_native_fault_structurally_valid(
		const struct orlix_tcti_native_fault_witness *witness)
{
	return witness && witness->valid &&
	       orlix_tcti_native_access_valid(witness->access);
}

static bool orlix_tcti_native_fault_success(
		const struct orlix_tcti_native_fault_witness *witness)
{
	return orlix_tcti_native_fault_structurally_valid(witness) &&
	       witness->occurred && witness->precise &&
	       !witness->side_effects_committed;
}

static int orlix_tcti_native_copy_bytes(
		struct orlix_tcti_native_owned_bytes *destination,
		const u8 *source, size_t size)
{
	if (!destination || !source || !size)
		return -EINVAL;
	destination->data = kmemdup(source, size, GFP_KERNEL);
	if (!destination->data)
		return -ENOMEM;
	destination->size = size;
	return 0;
}

static void orlix_tcti_native_free_bytes(
		struct orlix_tcti_native_owned_bytes *bytes)
{
	if (!bytes)
		return;
	kfree(bytes->data);
	bytes->data = NULL;
	bytes->size = 0;
}

static int orlix_tcti_native_copy_sve(
		struct orlix_tcti_native_owned_sve *destination,
		const struct orlix_tcti_native_sve_state *source)
{
	size_t z_size;
	size_t p_size;
	size_t predicate_size;
	int ret;

	if (!destination || !source || !source->valid ||
	    !orlix_tcti_native_vl_valid(source->vl_bytes) ||
	    !source->z || !source->p || !source->ffr)
		return -EINVAL;
	z_size = ORLIX_TCTI_SVE_ZREG_COUNT * source->vl_bytes;
	predicate_size = source->vl_bytes / 8;
	p_size = ORLIX_TCTI_SVE_PREG_COUNT * predicate_size;
	ret = orlix_tcti_native_copy_bytes(&destination->z, source->z, z_size);
	if (ret)
		return ret;
	ret = orlix_tcti_native_copy_bytes(&destination->p, source->p, p_size);
	if (ret)
		goto free_z;
	ret = orlix_tcti_native_copy_bytes(&destination->ffr, source->ffr,
					    predicate_size);
	if (ret)
		goto free_p;
	destination->vl_bytes = source->vl_bytes;
	destination->valid = true;
	return 0;

free_p:
	orlix_tcti_native_free_bytes(&destination->p);
free_z:
	orlix_tcti_native_free_bytes(&destination->z);
	return ret;
}

static void orlix_tcti_native_free_sve(
		struct orlix_tcti_native_owned_sve *sve)
{
	orlix_tcti_native_free_bytes(&sve->ffr);
	orlix_tcti_native_free_bytes(&sve->p);
	orlix_tcti_native_free_bytes(&sve->z);
	sve->valid = false;
}

static int orlix_tcti_native_copy_sme(
		struct orlix_tcti_native_owned_sme *destination,
		const struct orlix_tcti_native_sme_state *source)
{
	size_t required_za_size;
	int ret;

	if (!destination || !source || !source->valid || !source->sm_present ||
	    !source->vl_present || !orlix_tcti_native_vl_valid(source->vl_bytes) ||
	    !source->svl_present ||
	    !orlix_tcti_native_vl_valid(source->svl_bytes) ||
	    (!source->za_control_present && source->za_enabled) ||
	    source->za_applicable != source->za_enabled ||
	    source->za_present != source->za_applicable ||
	    source->zt0_present != source->zt0_applicable)
		return -EINVAL;
	if (source->za_applicable) {
		if (check_mul_overflow((size_t)source->svl_bytes,
				       (size_t)source->svl_bytes,
				       &required_za_size) ||
		    source->za_size != required_za_size || !source->za)
			return -EINVAL;
		ret = orlix_tcti_native_copy_bytes(&destination->za, source->za,
						    source->za_size);
		if (ret)
			return ret;
	} else if (source->za || source->za_size) {
		return -EINVAL;
	}
	if (source->zt0_applicable) {
		if (source->zt0_size != ORLIX_TCTI_NATIVE_OBSERVATION_ZT0_BYTES ||
		    !source->zt0) {
			ret = -EINVAL;
			goto free_za;
		}
		ret = orlix_tcti_native_copy_bytes(&destination->zt0, source->zt0,
						    source->zt0_size);
		if (ret)
			goto free_za;
	} else if (source->zt0 || source->zt0_size) {
		ret = -EINVAL;
		goto free_za;
	}
	destination->sm_present = source->sm_present;
	destination->streaming_mode = source->streaming_mode;
	destination->za_control_present = source->za_control_present;
	destination->za_enabled = source->za_enabled;
	destination->vl_present = source->vl_present;
	destination->vl_bytes = source->vl_bytes;
	destination->svl_present = source->svl_present;
	destination->svl_bytes = source->svl_bytes;
	destination->za_applicable = source->za_applicable;
	destination->za_present = source->za_present;
	destination->zt0_applicable = source->zt0_applicable;
	destination->zt0_present = source->zt0_present;
	destination->valid = true;
	destination->production_available = source->production_available;
	return 0;

free_za:
	orlix_tcti_native_free_bytes(&destination->za);
	return ret;
}

static void orlix_tcti_native_free_sme(
		struct orlix_tcti_native_owned_sme *sme)
{
	orlix_tcti_native_free_bytes(&sme->zt0);
	orlix_tcti_native_free_bytes(&sme->za);
	sme->valid = false;
}

static bool orlix_tcti_native_observation_valid(
		const struct orlix_tcti_native_observation *observation)
{
	return observation &&
	       observation->magic == ORLIX_TCTI_NATIVE_OBSERVATION_MAGIC;
}

void orlix_tcti_native_gpr_capture(struct orlix_tcti_native_gpr_state *state,
				   const struct pt_regs *regs)
{
	if (!state || !regs)
		return;
	memcpy(state->x, regs->regs, sizeof(state->x));
	state->sp = regs->sp;
	state->pc = regs->pc;
	state->pstate = regs->pstate;
}

static int orlix_tcti_native_copy_expected_witness(
		struct orlix_tcti_native_observation *observation,
		const struct orlix_tcti_native_observation_spec *spec)
{
	switch (spec->obligation) {
	case ORLIX_TCTI_NATIVE_OBLIGATION_RESULT:
	case ORLIX_TCTI_NATIVE_OBLIGATION_GPR:
		return 0;
	case ORLIX_TCTI_NATIVE_OBLIGATION_MEMORY:
		if (!spec->expected.memory.size || !spec->expected.memory.bytes ||
		    spec->expected.memory.size >
			    ORLIX_TCTI_NATIVE_OBSERVATION_MAX_MEMORY)
			return -EINVAL;
		observation->expected.memory.address =
			spec->expected.memory.address;
		return orlix_tcti_native_copy_bytes(
			&observation->expected.memory.bytes,
			spec->expected.memory.bytes, spec->expected.memory.size);
	case ORLIX_TCTI_NATIVE_OBLIGATION_FP_SIMD:
		if (!spec->expected.fp_simd.valid)
			return -EINVAL;
		observation->expected.fp_simd = spec->expected.fp_simd;
		return 0;
	case ORLIX_TCTI_NATIVE_OBLIGATION_SVE:
		return orlix_tcti_native_copy_sve(&observation->expected.sve,
						   &spec->expected.sve);
	case ORLIX_TCTI_NATIVE_OBLIGATION_SME:
		return orlix_tcti_native_copy_sme(&observation->expected.sme,
						   &spec->expected.sme);
	case ORLIX_TCTI_NATIVE_OBLIGATION_FAULT:
		if (!orlix_tcti_native_fault_success(&spec->expected.fault) ||
		    (spec->result.reason != ORLIX_TCTI_EXIT_USER_FAULT &&
		     spec->result.reason != ORLIX_TCTI_EXIT_ALIGNMENT_FAULT) ||
		    spec->expected.fault.address != spec->result.fault_address ||
		    spec->expected.fault.access != spec->result.fault_access)
			return -EINVAL;
		observation->expected.fault = spec->expected.fault;
		return 0;
	case ORLIX_TCTI_NATIVE_OBLIGATION_ATOMICITY:
		if (!orlix_tcti_native_atomicity_success(&spec->expected.atomicity))
			return -EINVAL;
		observation->expected.atomicity = spec->expected.atomicity;
		return 0;
	case ORLIX_TCTI_NATIVE_OBLIGATION_ORDERING:
		if (!orlix_tcti_native_ordering_success(&spec->expected.ordering))
			return -EINVAL;
		observation->expected.ordering = spec->expected.ordering;
		return 0;
	case ORLIX_TCTI_NATIVE_OBLIGATION_INVALID:
		return -EINVAL;
	}
	return -EINVAL;
}

struct orlix_tcti_native_observation *
orlix_tcti_native_observation_create(
		const struct orlix_tcti_native_observation_spec *spec)
{
	struct orlix_tcti_native_observation *observation;
	int ret;

	if (!spec || spec->source_ordinal == ORLIX_TCTI_NATIVE_SOURCE_ORDINAL_NONE ||
	    !orlix_tcti_native_obligation_valid(spec->obligation) ||
	    !orlix_tcti_native_result_valid(&spec->result))
		return NULL;
	observation = kzalloc(sizeof(*observation), GFP_KERNEL);
	if (!observation)
		return NULL;
	observation->state = ORLIX_TCTI_NATIVE_OBSERVATION_UNVERIFIED;
	observation->expected_source_ordinal = spec->source_ordinal;
	observation->expected_obligation = spec->obligation;
	observation->expected_result = spec->result;
	observation->expected_gpr = spec->gpr;
	observation->expected_mask = ORLIX_TCTI_NATIVE_HAVE_EXECUTION;
	if (spec->obligation != ORLIX_TCTI_NATIVE_OBLIGATION_RESULT &&
	    spec->obligation != ORLIX_TCTI_NATIVE_OBLIGATION_GPR)
		observation->expected_mask |= ORLIX_TCTI_NATIVE_HAVE_WITNESS;
	ret = orlix_tcti_native_copy_expected_witness(observation, spec);
	if (ret) {
		orlix_tcti_native_observation_destroy(observation);
		return NULL;
	}
	observation->magic = ORLIX_TCTI_NATIVE_OBSERVATION_MAGIC;
	return observation;
}

static void orlix_tcti_native_free_witness(
		union orlix_tcti_native_owned_witness *witness,
		enum orlix_tcti_native_obligation obligation)
{
	switch (obligation) {
	case ORLIX_TCTI_NATIVE_OBLIGATION_MEMORY:
		orlix_tcti_native_free_bytes(&witness->memory.bytes);
		break;
	case ORLIX_TCTI_NATIVE_OBLIGATION_SVE:
		orlix_tcti_native_free_sve(&witness->sve);
		break;
	case ORLIX_TCTI_NATIVE_OBLIGATION_SME:
		orlix_tcti_native_free_sme(&witness->sme);
		break;
	default:
		break;
	}
}

void orlix_tcti_native_observation_destroy(
		struct orlix_tcti_native_observation *observation)
{
	if (!observation)
		return;
	orlix_tcti_native_free_witness(&observation->observed,
					observation->expected_obligation);
	orlix_tcti_native_free_witness(&observation->expected,
					observation->expected_obligation);
	observation->magic = 0;
	kfree(observation);
}

enum orlix_tcti_native_observation_state
orlix_tcti_native_observation_state(
		const struct orlix_tcti_native_observation *observation)
{
	if (!orlix_tcti_native_observation_valid(observation))
		return ORLIX_TCTI_NATIVE_OBSERVATION_UNVERIFIED;
	return observation->state;
}

static int orlix_tcti_native_prepare_add(
		struct orlix_tcti_native_observation *observation, u8 bit,
		enum orlix_tcti_native_obligation obligation)
{
	if (!orlix_tcti_native_observation_valid(observation))
		return -EINVAL;
	if (observation->poisoned)
		return -EPERM;
	if (!(observation->expected_mask & bit)) {
		observation->poisoned = true;
		observation->state = ORLIX_TCTI_NATIVE_OBSERVATION_POISONED;
		return -EPROTOTYPE;
	}
	if (bit == ORLIX_TCTI_NATIVE_HAVE_WITNESS &&
	    observation->expected_obligation != obligation) {
		observation->poisoned = true;
		observation->state = ORLIX_TCTI_NATIVE_OBSERVATION_POISONED;
		return -EPROTOTYPE;
	}
	if (observation->observed_mask & bit) {
		observation->poisoned = true;
		observation->state = ORLIX_TCTI_NATIVE_OBSERVATION_POISONED;
		return -EALREADY;
	}
	return 0;
}

static int orlix_tcti_native_poison(
		struct orlix_tcti_native_observation *observation, int error)
{
	if (orlix_tcti_native_observation_valid(observation)) {
		observation->poisoned = true;
		observation->state = ORLIX_TCTI_NATIVE_OBSERVATION_POISONED;
	}
	return error;
}

static int orlix_tcti_native_effective_encoding(
		const struct orlix_tcti_target_instruction_artifact *artifact,
		u32 ordinal, u32 *mask, u32 *pattern)
{
	const struct orlix_tcti_target_instruction_artifact_leaf *leaf;
	u32 index;

	if (!artifact || ordinal >= artifact->leaf_count || !mask || !pattern)
		return -EINVAL;
	leaf = &artifact->leaves[ordinal];
	if (leaf->fixed_operand_first > artifact->fixed_operand_count ||
	    leaf->fixed_operand_count >
		    artifact->fixed_operand_count - leaf->fixed_operand_first)
		return -EBADMSG;
	*mask = leaf->encoding_mask;
	*pattern = leaf->encoding_pattern;
	for (index = leaf->fixed_operand_first;
	     index < leaf->fixed_operand_first + leaf->fixed_operand_count;
	     index++) {
		const struct orlix_tcti_target_instruction_artifact_fixed_operand *fixed =
			&artifact->fixed_operands[index];

		if (fixed->leaf_index != ordinal ||
		    (fixed->fixed_value & ~fixed->fixed_mask) ||
		    ((*pattern ^ fixed->fixed_value) &
		     (*mask & fixed->fixed_mask)))
			return -EBADMSG;
		*pattern = (*pattern & ~fixed->fixed_mask) | fixed->fixed_value;
		*mask |= fixed->fixed_mask;
	}
	if (*pattern & ~*mask)
		return -EBADMSG;
	return 0;
}

static int orlix_tcti_native_bind_unique_leaf(
		const struct orlix_tcti_target_instruction_artifact *artifact,
		u32 requested_ordinal, u32 instruction)
{
	u32 requested_mask;
	u32 requested_pattern;
	u32 ordinal;
	int ret;

	ret = orlix_tcti_native_effective_encoding(artifact, requested_ordinal,
						  &requested_mask,
						  &requested_pattern);
	if (ret)
		return ret;
	if ((instruction & requested_mask) != requested_pattern)
		return -EBADMSG;
	for (ordinal = 0; ordinal < artifact->leaf_count; ordinal++) {
		u32 other_mask;
		u32 other_pattern;

		if (ordinal == requested_ordinal)
			continue;
		ret = orlix_tcti_native_effective_encoding(artifact, ordinal,
							  &other_mask,
							  &other_pattern);
		if (ret)
			return ret;
		if ((instruction & other_mask) != other_pattern)
			continue;
		/*
		 * The requested leaf must strictly contain every other matching
		 * leaf's fixed-bit knowledge. Equal or crossing masks require the
		 * feature/alias resolver and therefore remain ambiguous here.
		 */
		if ((requested_mask & other_mask) != other_mask ||
		    requested_mask == other_mask)
			return -ENOTUNIQ;
	}
	return 0;
}

int orlix_tcti_native_observation_execute(
		struct orlix_tcti_native_observation *observation,
		struct task_struct *task, struct pt_regs *regs, struct mm_struct *mm)
{
	struct orlix_tcti_target_instruction_artifact_validation_result validation;
	const struct orlix_tcti_target_instruction_artifact *artifact;
	unsigned long entry_pc;
	int ret;

	if (!orlix_tcti_native_observation_valid(observation))
		return -EINVAL;
	if (observation->poisoned)
		return -EPERM;
	if (observation->execution_attempted)
		return orlix_tcti_native_poison(observation, -EALREADY);
	observation->execution_attempted = true;
	if (!task || !regs || !mm || task != current || mm != current->mm ||
	    task->mm != mm)
		return orlix_tcti_native_poison(observation, -EINVAL);
	artifact = orlix_tcti_target_instruction_artifact_canonical();
	if (!artifact || orlix_tcti_target_instruction_artifact_validate(
				artifact, &validation))
		return orlix_tcti_native_poison(observation, -EBADMSG);
	if (observation->expected_source_ordinal >= artifact->leaf_count)
		return orlix_tcti_native_poison(observation, -ERANGE);

	entry_pc = regs->pc;
	observation->execution_path =
		ORLIX_TCTI_NATIVE_INTERNAL_PATH_RESUME_USER;
	observation->execution_count++;
	observation->observed_result = orlix_tcti_resume_user(task, regs, mm);
	orlix_tcti_native_gpr_capture(&observation->observed_gpr, regs);
	if (!observation->observed_result.entry_valid ||
	    observation->observed_result.entry_pc != entry_pc)
		return orlix_tcti_native_poison(observation, -EBADMSG);
	ret = orlix_tcti_native_bind_unique_leaf(
		artifact, observation->expected_source_ordinal,
		observation->observed_result.entry_instruction);
	if (ret)
		return orlix_tcti_native_poison(observation, ret);

	observation->source_bound = true;
	observation->observed_mask |= ORLIX_TCTI_NATIVE_HAVE_EXECUTION;
	return 0;
}

int orlix_tcti_native_observation_add_memory(
		struct orlix_tcti_native_observation *observation,
		const struct orlix_tcti_native_memory_state *memory)
{
	int ret = orlix_tcti_native_prepare_add(observation,
					ORLIX_TCTI_NATIVE_HAVE_WITNESS,
					ORLIX_TCTI_NATIVE_OBLIGATION_MEMORY);

	if (ret)
		return ret;
	if (!memory || !memory->size || !memory->bytes ||
	    memory->size > ORLIX_TCTI_NATIVE_OBSERVATION_MAX_MEMORY)
		return orlix_tcti_native_poison(observation, -EINVAL);
	observation->observed.memory.address = memory->address;
	ret = orlix_tcti_native_copy_bytes(&observation->observed.memory.bytes,
					   memory->bytes, memory->size);
	if (ret)
		return orlix_tcti_native_poison(observation, ret);
	observation->observed_mask |= ORLIX_TCTI_NATIVE_HAVE_WITNESS;
	return 0;
}

int orlix_tcti_native_observation_add_fp_simd(
		struct orlix_tcti_native_observation *observation,
		const struct orlix_tcti_native_fp_simd_state *fp_simd)
{
	int ret = orlix_tcti_native_prepare_add(observation,
					ORLIX_TCTI_NATIVE_HAVE_WITNESS,
					ORLIX_TCTI_NATIVE_OBLIGATION_FP_SIMD);

	if (ret)
		return ret;
	if (!fp_simd || !fp_simd->valid)
		return orlix_tcti_native_poison(observation, -EINVAL);
	observation->observed.fp_simd = *fp_simd;
	observation->observed_mask |= ORLIX_TCTI_NATIVE_HAVE_WITNESS;
	return 0;
}

int orlix_tcti_native_observation_add_sve(
		struct orlix_tcti_native_observation *observation,
		const struct orlix_tcti_native_sve_state *sve)
{
	int ret = orlix_tcti_native_prepare_add(observation,
					ORLIX_TCTI_NATIVE_HAVE_WITNESS,
					ORLIX_TCTI_NATIVE_OBLIGATION_SVE);

	if (ret)
		return orlix_tcti_native_poison(observation, ret);
	ret = orlix_tcti_native_copy_sve(&observation->observed.sve, sve);
	if (ret)
		return orlix_tcti_native_poison(observation, ret);
	observation->observed_mask |= ORLIX_TCTI_NATIVE_HAVE_WITNESS;
	return 0;
}

int orlix_tcti_native_observation_add_sme(
		struct orlix_tcti_native_observation *observation,
		const struct orlix_tcti_native_sme_state *sme)
{
	int ret = orlix_tcti_native_prepare_add(observation,
					ORLIX_TCTI_NATIVE_HAVE_WITNESS,
					ORLIX_TCTI_NATIVE_OBLIGATION_SME);

	if (ret)
		return ret;
	ret = orlix_tcti_native_copy_sme(&observation->observed.sme, sme);
	if (ret)
		return orlix_tcti_native_poison(observation, ret);
	observation->observed_mask |= ORLIX_TCTI_NATIVE_HAVE_WITNESS;
	return 0;
}

#define ORLIX_TCTI_NATIVE_ADD_SMALL(name, type, field, obligation, validator) \
int orlix_tcti_native_observation_add_##name( \
		struct orlix_tcti_native_observation *observation, \
		const struct type *value) \
{ \
	int ret = orlix_tcti_native_prepare_add(observation, \
					ORLIX_TCTI_NATIVE_HAVE_WITNESS, obligation); \
	if (ret) \
		return ret; \
	if (!validator(value)) \
		return orlix_tcti_native_poison(observation, -EINVAL); \
	observation->observed.field = *value; \
	observation->observed_mask |= ORLIX_TCTI_NATIVE_HAVE_WITNESS; \
	return 0; \
}

ORLIX_TCTI_NATIVE_ADD_SMALL(fault, orlix_tcti_native_fault_witness, fault,
	ORLIX_TCTI_NATIVE_OBLIGATION_FAULT,
	orlix_tcti_native_fault_structurally_valid)
ORLIX_TCTI_NATIVE_ADD_SMALL(atomicity, orlix_tcti_native_atomicity_witness,
	atomicity, ORLIX_TCTI_NATIVE_OBLIGATION_ATOMICITY,
	orlix_tcti_native_atomicity_structurally_valid)
ORLIX_TCTI_NATIVE_ADD_SMALL(ordering, orlix_tcti_native_ordering_witness,
	ordering, ORLIX_TCTI_NATIVE_OBLIGATION_ORDERING,
	orlix_tcti_native_ordering_structurally_valid)

static bool orlix_tcti_native_result_equal(
		const struct orlix_tcti_result *left,
		const struct orlix_tcti_result *right)
{
	return left->reason == right->reason && left->status == right->status &&
	       left->fault_address == right->fault_address &&
	       left->fault_access == right->fault_access && left->pc == right->pc &&
	       left->instruction == right->instruction;
}

static bool orlix_tcti_native_gpr_equal(
		const struct orlix_tcti_native_gpr_state *left,
		const struct orlix_tcti_native_gpr_state *right)
{
	return !memcmp(left->x, right->x, sizeof(left->x)) &&
	       left->sp == right->sp && left->pc == right->pc &&
	       left->pstate == right->pstate;
}

static bool orlix_tcti_native_owned_bytes_equal(
		const struct orlix_tcti_native_owned_bytes *left,
		const struct orlix_tcti_native_owned_bytes *right)
{
	return left->size == right->size && left->size &&
	       !memcmp(left->data, right->data, left->size);
}

static bool orlix_tcti_native_fp_simd_equal(
		const struct orlix_tcti_native_fp_simd_state *left,
		const struct orlix_tcti_native_fp_simd_state *right)
{
	return left->valid && right->valid &&
	       !memcmp(left->v, right->v, sizeof(left->v)) &&
	       left->fpcr == right->fpcr && left->fpsr == right->fpsr;
}

static bool orlix_tcti_native_fault_equal(
		const struct orlix_tcti_native_fault_witness *left,
		const struct orlix_tcti_native_fault_witness *right)
{
	return left->address == right->address && left->access == right->access &&
	       left->valid == right->valid &&
	       left->occurred == right->occurred && left->precise == right->precise &&
	       left->side_effects_committed == right->side_effects_committed;
}

static bool orlix_tcti_native_atomicity_equal(
		const struct orlix_tcti_native_atomicity_witness *left,
		const struct orlix_tcti_native_atomicity_witness *right)
{
	return left->address == right->address && left->width == right->width &&
	       left->linearization_count == right->linearization_count &&
	       left->valid == right->valid &&
	       left->completed == right->completed && left->torn == right->torn &&
	       left->forbidden_outcome == right->forbidden_outcome &&
	       !memcmp(left->before, right->before, left->width) &&
	       !memcmp(left->after, right->after, left->width) &&
	       !memcmp(left->returned, right->returned, left->width);
}

static bool orlix_tcti_native_ordering_equal(
		const struct orlix_tcti_native_ordering_witness *left,
		const struct orlix_tcti_native_ordering_witness *right)
{
	return left->address == right->address && left->order == right->order &&
	       left->predecessor_epoch == right->predecessor_epoch &&
	       left->successor_epoch == right->successor_epoch &&
	       left->observed_value == right->observed_value &&
	       left->valid == right->valid &&
	       left->edge_observed == right->edge_observed &&
	       left->forbidden_outcome == right->forbidden_outcome;
}

static int orlix_tcti_native_compare_witness(
		struct orlix_tcti_native_observation *observation)
{
	switch (observation->expected_obligation) {
	case ORLIX_TCTI_NATIVE_OBLIGATION_RESULT:
	case ORLIX_TCTI_NATIVE_OBLIGATION_GPR:
		return 0;
	case ORLIX_TCTI_NATIVE_OBLIGATION_MEMORY:
		if (observation->expected.memory.address !=
			    observation->observed.memory.address ||
		    !orlix_tcti_native_owned_bytes_equal(
			    &observation->expected.memory.bytes,
			    &observation->observed.memory.bytes))
			observation->state =
				ORLIX_TCTI_NATIVE_OBSERVATION_MEMORY_MISMATCH;
		break;
	case ORLIX_TCTI_NATIVE_OBLIGATION_FP_SIMD:
		if (!orlix_tcti_native_fp_simd_equal(
			    &observation->expected.fp_simd,
			    &observation->observed.fp_simd))
			observation->state =
				ORLIX_TCTI_NATIVE_OBSERVATION_FP_SIMD_MISMATCH;
		break;
	case ORLIX_TCTI_NATIVE_OBLIGATION_SVE:
		if (!observation->expected.sve.valid ||
		    !observation->observed.sve.valid ||
		    observation->expected.sve.vl_bytes !=
			    observation->observed.sve.vl_bytes ||
		    !orlix_tcti_native_owned_bytes_equal(&observation->expected.sve.z,
						  &observation->observed.sve.z) ||
		    !orlix_tcti_native_owned_bytes_equal(&observation->expected.sve.p,
						  &observation->observed.sve.p) ||
		    !orlix_tcti_native_owned_bytes_equal(
			    &observation->expected.sve.ffr,
			    &observation->observed.sve.ffr))
			observation->state = ORLIX_TCTI_NATIVE_OBSERVATION_SVE_MISMATCH;
		break;
	case ORLIX_TCTI_NATIVE_OBLIGATION_SME:
		if (!observation->expected.sme.production_available ||
		    !observation->observed.sme.production_available) {
			observation->state =
				ORLIX_TCTI_NATIVE_OBSERVATION_STATE_UNAVAILABLE;
			return -EOPNOTSUPP;
		}
		if (!observation->expected.sme.valid ||
		    !observation->observed.sme.valid ||
		    observation->expected.sme.sm_present !=
			    observation->observed.sme.sm_present ||
		    observation->expected.sme.streaming_mode !=
			    observation->observed.sme.streaming_mode ||
		    observation->expected.sme.za_control_present !=
			    observation->observed.sme.za_control_present ||
		    observation->expected.sme.za_enabled !=
			    observation->observed.sme.za_enabled ||
		    observation->expected.sme.vl_present !=
			    observation->observed.sme.vl_present ||
		    observation->expected.sme.vl_bytes !=
			    observation->observed.sme.vl_bytes ||
		    observation->expected.sme.svl_present !=
			    observation->observed.sme.svl_present ||
		    observation->expected.sme.svl_bytes !=
			    observation->observed.sme.svl_bytes ||
		    observation->expected.sme.za_applicable !=
			    observation->observed.sme.za_applicable ||
		    observation->expected.sme.za_present !=
			    observation->observed.sme.za_present ||
		    observation->expected.sme.zt0_applicable !=
			    observation->observed.sme.zt0_applicable ||
		    observation->expected.sme.zt0_present !=
			    observation->observed.sme.zt0_present ||
		    (observation->expected.sme.za_applicable &&
		     !orlix_tcti_native_owned_bytes_equal(
			     &observation->expected.sme.za,
			     &observation->observed.sme.za)) ||
		    (observation->expected.sme.zt0_applicable &&
		     !orlix_tcti_native_owned_bytes_equal(
			     &observation->expected.sme.zt0,
			     &observation->observed.sme.zt0)))
			observation->state = ORLIX_TCTI_NATIVE_OBSERVATION_SME_MISMATCH;
		break;
	case ORLIX_TCTI_NATIVE_OBLIGATION_FAULT:
		if (!orlix_tcti_native_fault_equal(&observation->expected.fault,
						   &observation->observed.fault))
			observation->state =
				ORLIX_TCTI_NATIVE_OBSERVATION_FAULT_MISMATCH;
		break;
	case ORLIX_TCTI_NATIVE_OBLIGATION_ATOMICITY:
		if (!orlix_tcti_native_atomicity_equal(
			    &observation->expected.atomicity,
			    &observation->observed.atomicity))
			observation->state =
				ORLIX_TCTI_NATIVE_OBSERVATION_ATOMICITY_MISMATCH;
		break;
	case ORLIX_TCTI_NATIVE_OBLIGATION_ORDERING:
		if (!orlix_tcti_native_ordering_equal(
			    &observation->expected.ordering,
			    &observation->observed.ordering))
			observation->state =
				ORLIX_TCTI_NATIVE_OBSERVATION_ORDERING_MISMATCH;
		break;
	case ORLIX_TCTI_NATIVE_OBLIGATION_INVALID:
		observation->state = ORLIX_TCTI_NATIVE_OBSERVATION_POISONED;
		return -EINVAL;
	}
	return observation->state == ORLIX_TCTI_NATIVE_OBSERVATION_UNVERIFIED ?
		0 : -EBADE;
}

int orlix_tcti_native_observation_compare(
		struct orlix_tcti_native_observation *observation)
{
	int ret;

	if (!orlix_tcti_native_observation_valid(observation))
		return -EINVAL;
	if (observation->poisoned) {
		observation->state = ORLIX_TCTI_NATIVE_OBSERVATION_POISONED;
		return -EPERM;
	}
	if (observation->observed_mask != observation->expected_mask) {
		observation->state = ORLIX_TCTI_NATIVE_OBSERVATION_INCOMPLETE;
		return -EINPROGRESS;
	}
	observation->state = ORLIX_TCTI_NATIVE_OBSERVATION_UNVERIFIED;
	if (!observation->source_bound || observation->execution_count != 1 ||
	    observation->execution_path !=
		    ORLIX_TCTI_NATIVE_INTERNAL_PATH_RESUME_USER) {
		observation->state = ORLIX_TCTI_NATIVE_OBSERVATION_IDENTITY_MISMATCH;
		return -EBADE;
	}
	if (!orlix_tcti_native_result_equal(&observation->expected_result,
					    &observation->observed_result)) {
		observation->state = ORLIX_TCTI_NATIVE_OBSERVATION_RESULT_MISMATCH;
		return -EBADE;
	}
	if (!orlix_tcti_native_gpr_equal(&observation->expected_gpr,
					 &observation->observed_gpr)) {
		observation->state = ORLIX_TCTI_NATIVE_OBSERVATION_GPR_MISMATCH;
		return -EBADE;
	}
	ret = orlix_tcti_native_compare_witness(observation);
	if (ret)
		return ret;
	observation->state = ORLIX_TCTI_NATIVE_OBSERVATION_MATCH;
	return 0;
}

int orlix_tcti_native_observation_export(
		struct orlix_tcti_native_observation *observation,
		struct orlix_tcti_target_native_result_record **record)
{
	const struct orlix_tcti_target_instruction_artifact_leaf *leaf;
	const struct orlix_tcti_target_instruction_artifact *artifact;
	struct orlix_tcti_target_native_result_record *exported;
	char build_identity[ORLIX_TCTI_TARGET_PROOF_BUILD_ID_MAX];
	u32 mask;
	u32 pattern;
	int ret;

	if (!record)
		return -EINVAL;
	*record = NULL;
	if (!orlix_tcti_native_observation_valid(observation))
		return -EINVAL;
	if (observation->exported)
		return orlix_tcti_native_poison(observation, -EALREADY);
	if (observation->state != ORLIX_TCTI_NATIVE_OBSERVATION_MATCH ||
	    !observation->source_bound || observation->execution_count != 1 ||
	    observation->execution_path !=
		    ORLIX_TCTI_NATIVE_INTERNAL_PATH_RESUME_USER)
		return -EPERM;
	if (observation->expected_obligation !=
		    ORLIX_TCTI_NATIVE_OBLIGATION_RESULT &&
	    observation->expected_obligation != ORLIX_TCTI_NATIVE_OBLIGATION_GPR)
		return -EOPNOTSUPP;
	artifact = orlix_tcti_target_instruction_artifact_canonical();
	if (!artifact || observation->expected_source_ordinal >= artifact->leaf_count)
		return -EBADMSG;
	ret = orlix_tcti_native_effective_encoding(
		artifact, observation->expected_source_ordinal, &mask, &pattern);
	if (ret)
		return ret;
	leaf = &artifact->leaves[observation->expected_source_ordinal];
	scnprintf(build_identity, sizeof(build_identity), "%s|%s|%s",
		 init_utsname()->release, init_utsname()->version,
		 init_utsname()->machine);
	exported = kzalloc(sizeof(*exported), GFP_KERNEL);
	if (!exported)
		return -ENOMEM;
	exported->source_ordinal = observation->expected_source_ordinal;
	exported->encoding_mask = mask;
	exported->encoding_pattern = pattern;
	exported->entry_instruction = observation->observed_result.entry_instruction;
	exported->kind = observation->expected_obligation ==
			     ORLIX_TCTI_NATIVE_OBLIGATION_RESULT ?
		ORLIX_TCTI_TARGET_NATIVE_RESULT_RESULT :
		ORLIX_TCTI_TARGET_NATIVE_RESULT_GPR;
	exported->production_resume = true;
	exported->source_bound = true;
	exported->match = true;
	exported->resume_count = 1;
#define COPY_EXPORT_FIELD(field, source) \
	do { \
		if (strscpy(exported->field, (source), sizeof(exported->field)) < 0) \
			goto invalid_export; \
	} while (0)
	COPY_EXPORT_FIELD(leaf_name,
		(const char *)&artifact->string_pool[leaf->name_offset]);
	COPY_EXPORT_FIELD(mnemonic,
		(const char *)&artifact->string_pool[leaf->mnemonic_offset]);
	COPY_EXPORT_FIELD(operation_id,
		(const char *)&artifact->string_pool[leaf->operation_offset]);
	COPY_EXPORT_FIELD(artifact_architecture, artifact->architecture);
	COPY_EXPORT_FIELD(artifact_build, artifact->build);
	COPY_EXPORT_FIELD(artifact_reference, artifact->reference);
	COPY_EXPORT_FIELD(artifact_schema, artifact->schema);
	COPY_EXPORT_FIELD(artifact_source_sha256, artifact->source_sha256);
	COPY_EXPORT_FIELD(executing_kernel_identity, build_identity);
	COPY_EXPORT_FIELD(implementation_owner, "orlix_tcti_resume_user");
	COPY_EXPORT_FIELD(decoder_owner, "orlix_tcti_decode_aarch64");
	COPY_EXPORT_FIELD(lowering_owner, "orlix_tcti_execute_decoded_semantics");
#undef COPY_EXPORT_FIELD
	/* Stable replay key over internally captured execution and build identity. */
	exported->identity = ORLIX_TCTI_PROOF_U64_C(1469598103934665603);
#define HASH_EXPORT_BYTES(pointer, length) \
	do { \
		const u8 *cursor = (const u8 *)(pointer); \
		size_t byte_index; \
		for (byte_index = 0; byte_index < (length); byte_index++) { \
			exported->identity ^= cursor[byte_index]; \
			exported->identity *= ORLIX_TCTI_PROOF_U64_C(1099511628211); \
		} \
	} while (0)
	HASH_EXPORT_BYTES(&exported->source_ordinal,
			  sizeof(exported->source_ordinal));
	HASH_EXPORT_BYTES(&exported->entry_instruction,
			  sizeof(exported->entry_instruction));
	HASH_EXPORT_BYTES(&exported->kind, sizeof(exported->kind));
	HASH_EXPORT_BYTES(exported->artifact_source_sha256,
			  strlen(exported->artifact_source_sha256));
	HASH_EXPORT_BYTES(exported->executing_kernel_identity,
			  strlen(exported->executing_kernel_identity));
#undef HASH_EXPORT_BYTES
	if (!exported->identity)
		exported->identity = 1;
	exported->magic = ORLIX_TCTI_TARGET_NATIVE_RECORD_MAGIC;
	*record = exported;
	observation->exported = true;
	return 0;

invalid_export:
	kfree(exported);
	return -EOVERFLOW;
}

void orlix_tcti_target_native_result_record_destroy(
	struct orlix_tcti_target_native_result_record *record)
{
	if (!record)
		return;
	record->magic = 0;
	kfree(record);
}
