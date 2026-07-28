/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_NATIVE_OBSERVATION_H
#define ORLIX_TCTI_NATIVE_OBSERVATION_H

#include <linux/types.h>

#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>

#include "../decode_aarch64.h"
#include "target_proof_ingestion.h"

#define ORLIX_TCTI_NATIVE_OBSERVATION_MAX_MEMORY 128U
#define ORLIX_TCTI_NATIVE_OBSERVATION_SIMD_REGS 32U
#define ORLIX_TCTI_NATIVE_OBSERVATION_SIMD_BYTES 16U
#define ORLIX_TCTI_NATIVE_OBSERVATION_ZT0_BYTES 64U
#define ORLIX_TCTI_NATIVE_SOURCE_ORDINAL_NONE (~0U)

enum orlix_tcti_native_obligation {
	ORLIX_TCTI_NATIVE_OBLIGATION_INVALID,
	ORLIX_TCTI_NATIVE_OBLIGATION_DECODE,
	ORLIX_TCTI_NATIVE_OBLIGATION_LEGAL_ENCODINGS,
	ORLIX_TCTI_NATIVE_OBLIGATION_REJECTED_ENCODINGS,
	ORLIX_TCTI_NATIVE_OBLIGATION_RESULT,
	ORLIX_TCTI_NATIVE_OBLIGATION_GPR,
	ORLIX_TCTI_NATIVE_OBLIGATION_FLAGS,
	ORLIX_TCTI_NATIVE_OBLIGATION_MEMORY,
	ORLIX_TCTI_NATIVE_OBLIGATION_FP_SIMD,
	ORLIX_TCTI_NATIVE_OBLIGATION_SVE,
	ORLIX_TCTI_NATIVE_OBLIGATION_SME,
	ORLIX_TCTI_NATIVE_OBLIGATION_FAULT,
	ORLIX_TCTI_NATIVE_OBLIGATION_ATOMICITY,
	ORLIX_TCTI_NATIVE_OBLIGATION_ORDERING,
};

enum orlix_tcti_native_observation_state {
	ORLIX_TCTI_NATIVE_OBSERVATION_UNVERIFIED,
	ORLIX_TCTI_NATIVE_OBSERVATION_INCOMPLETE,
	ORLIX_TCTI_NATIVE_OBSERVATION_MATCH,
	ORLIX_TCTI_NATIVE_OBSERVATION_IDENTITY_MISMATCH,
	ORLIX_TCTI_NATIVE_OBSERVATION_RESULT_MISMATCH,
	ORLIX_TCTI_NATIVE_OBSERVATION_GPR_MISMATCH,
	ORLIX_TCTI_NATIVE_OBSERVATION_FLAGS_MISMATCH,
	ORLIX_TCTI_NATIVE_OBSERVATION_MEMORY_MISMATCH,
	ORLIX_TCTI_NATIVE_OBSERVATION_FP_SIMD_MISMATCH,
	ORLIX_TCTI_NATIVE_OBSERVATION_SVE_MISMATCH,
	ORLIX_TCTI_NATIVE_OBSERVATION_SME_MISMATCH,
	ORLIX_TCTI_NATIVE_OBSERVATION_FAULT_MISMATCH,
	ORLIX_TCTI_NATIVE_OBSERVATION_ATOMICITY_MISMATCH,
	ORLIX_TCTI_NATIVE_OBSERVATION_ORDERING_MISMATCH,
	ORLIX_TCTI_NATIVE_OBSERVATION_POISONED,
	ORLIX_TCTI_NATIVE_OBSERVATION_STATE_UNAVAILABLE,
};

/* Deliberately excludes orig_x0, syscall bookkeeping, and struct padding. */
struct orlix_tcti_native_gpr_state {
	u64 x[31];
	u64 sp;
	u64 pc;
	u64 pstate;
};

struct orlix_tcti_native_memory_state {
	unsigned long address;
	size_t size;
	const u8 *bytes;
};

struct orlix_tcti_native_flags_state {
	u64 nzcv;
	bool valid;
};

struct orlix_tcti_native_fp_simd_state {
	u8 v[ORLIX_TCTI_NATIVE_OBSERVATION_SIMD_REGS]
	    [ORLIX_TCTI_NATIVE_OBSERVATION_SIMD_BYTES];
	u64 fpcr;
	u64 fpsr;
	bool valid;
};

struct orlix_tcti_native_sve_state {
	u16 vl_bytes;
	const u8 *z;
	const u8 *p;
	const u8 *ffr;
	bool valid;
};

struct orlix_tcti_native_sme_state {
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
	const u8 *za;
	size_t za_size;
	bool zt0_applicable;
	bool zt0_present;
	const u8 *zt0;
	size_t zt0_size;
	bool valid;
	bool production_available;
};

struct orlix_tcti_native_fault_witness {
	unsigned long address;
	enum orlix_tcti_access access;
	bool valid;
	bool occurred;
	bool precise;
	bool side_effects_committed;
};

struct orlix_tcti_native_atomicity_witness {
	unsigned long address;
	u8 width;
	u8 before[16];
	u8 after[16];
	u8 returned[16];
	u32 linearization_count;
	bool valid;
	bool completed;
	bool torn;
	bool forbidden_outcome;
};

struct orlix_tcti_native_ordering_witness {
	unsigned long address;
	enum orlix_tcti_atomic_memory_order order;
	u64 predecessor_epoch;
	u64 successor_epoch;
	u64 observed_value;
	bool valid;
	bool edge_observed;
	bool forbidden_outcome;
};

struct orlix_tcti_native_observation_spec {
	u32 source_ordinal;
	enum orlix_tcti_native_obligation obligation;
	enum orlix_tcti_decode_class expected_decode_class;
	bool expected_decode_class_valid;
	struct orlix_tcti_result result;
	struct orlix_tcti_native_gpr_state gpr;
	union {
		struct orlix_tcti_native_memory_state memory;
		struct orlix_tcti_native_fp_simd_state fp_simd;
		struct orlix_tcti_native_sve_state sve;
		struct orlix_tcti_native_sme_state sme;
		struct orlix_tcti_native_fault_witness fault;
		struct orlix_tcti_native_atomicity_witness atomicity;
		struct orlix_tcti_native_ordering_witness ordering;
	} expected;
};

/*
 * One object discharges one source-leaf obligation through one production
 * resume. execute() validates the entry instruction against the canonical
 * instruction artifact, calls orlix_tcti_resume_user() exactly once, and
 * captures the structured exit and typed GPR state internally. The selected
 * witness is mandatory where applicable.
 *
 * The canonical instruction artifact currently owns ordinal and encoding, but
 * does not yet publish the per-leaf required-obligation set. This layer binds
 * the requested obligation exactly inside the object; proof ingestion must
 * additionally reject it against that authoritative set once it is published.
 * There is deliberately no generic pass operation: only compare() can produce
 * MATCH after every required typed observation has been supplied exactly once.
 * Scalable state and addressed bytes are copied into heap-owned storage.
 */
struct orlix_tcti_native_observation;

void orlix_tcti_native_gpr_capture(struct orlix_tcti_native_gpr_state *state,
				   const struct pt_regs *regs);
struct orlix_tcti_native_observation *
orlix_tcti_native_observation_create(
		const struct orlix_tcti_native_observation_spec *spec);
void orlix_tcti_native_observation_destroy(
		struct orlix_tcti_native_observation *observation);
enum orlix_tcti_native_observation_state
orlix_tcti_native_observation_state(
		const struct orlix_tcti_native_observation *observation);

int orlix_tcti_native_observation_execute(
		struct orlix_tcti_native_observation *observation,
		struct task_struct *task, struct pt_regs *regs, struct mm_struct *mm);
int orlix_tcti_native_observation_add_encoding_domain(
		struct orlix_tcti_native_observation *observation);
int orlix_tcti_native_observation_add_memory(
		struct orlix_tcti_native_observation *observation,
		const struct orlix_tcti_native_memory_state *memory);
int orlix_tcti_native_observation_add_fp_simd(
		struct orlix_tcti_native_observation *observation,
		const struct orlix_tcti_native_fp_simd_state *fp_simd);
int orlix_tcti_native_observation_add_sve(
		struct orlix_tcti_native_observation *observation,
		const struct orlix_tcti_native_sve_state *sve);
int orlix_tcti_native_observation_add_sme(
		struct orlix_tcti_native_observation *observation,
		const struct orlix_tcti_native_sme_state *sme);
int orlix_tcti_native_observation_add_fault(
		struct orlix_tcti_native_observation *observation,
		const struct orlix_tcti_native_fault_witness *fault);
int orlix_tcti_native_observation_add_atomicity(
		struct orlix_tcti_native_observation *observation,
		const struct orlix_tcti_native_atomicity_witness *atomicity);
int orlix_tcti_native_observation_add_ordering(
		struct orlix_tcti_native_observation *observation,
		const struct orlix_tcti_native_ordering_witness *ordering);
int orlix_tcti_native_observation_compare(
		struct orlix_tcti_native_observation *observation);
int orlix_tcti_native_observation_export(
		struct orlix_tcti_native_observation *observation,
		struct orlix_tcti_target_native_result_record **record);

#ifdef CONFIG_KUNIT
void orlix_tcti_native_observation_test_mutate_decode_class(
		enum orlix_tcti_decode_class decode_class);
void orlix_tcti_native_observation_test_clear_decode_mutation(void);
#endif

#endif /* ORLIX_TCTI_NATIVE_OBSERVATION_H */
