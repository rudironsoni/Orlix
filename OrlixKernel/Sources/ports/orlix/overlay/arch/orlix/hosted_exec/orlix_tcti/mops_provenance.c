/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/kernel.h>

#include "mops_provenance.h"

#define ORLIX_TCTI_DDI0602_2026_06_ARCHIVE_SHA256 \
	"63a01a1696483bbe2edfef9e0f0cd053d6c1c619ec0587876cb7a60bb344f354"

struct orlix_tcti_mops_ddi0602_row {
	u32 ordinal;
	const char *execute_locator;
};

#define ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE(...)
#define ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW(...)
#define ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW(ordinal, leaf, file, decode, \
		decode_digest, decode_lines, decode_size, decode_source_digest, execute, \
		execute_digest, execute_lines, execute_size, execute_source_digest) \
	{ ordinal, execute },
static const struct orlix_tcti_mops_ddi0602_row mops_ddi0602_rows[] = {
#include "isa/target_asl_availability.def"
};
#undef ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW
#undef ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW
#undef ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE

static const char *orlix_tcti_mops_ddi0602_locator(u32 ordinal)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(mops_ddi0602_rows); index++)
		if (mops_ddi0602_rows[index].ordinal == ordinal)
			return mops_ddi0602_rows[index].execute_locator;
	return NULL;
}

/*
 * The groups below are the exact contiguous source-ordinal partitions in the
 * pinned AARCHMRS 2026-06 artifact.  `operation` is selected from the source
 * operation sequence, so a caller cannot accidentally treat every phase as
 * the same instruction. `asl_operation` identifies the external architecture
 * locator. The 12 FEAT_MOPS_GO leaves are explicitly "Not specified" by their
 * direct official operation source. They remain unimplemented and receive no
 * execution or proof credit. The remaining 120 leaves use their external
 * DDI0602 provenance and remain unimplemented here.
 */
struct orlix_tcti_mops_leaf_group {
	u32 first_ordinal;
	u32 leaf_count;
	const char *feature;
	const char *const *operations;
	u32 operation_count;
	const char *const *asl_operations;
	u32 asl_operation_count;
	enum orlix_tcti_mops_phase phase;
};

static const char *const set_go_operations[] = {
	"SETGOP", "SETGOPT", "SETGOPN", "SETGOPTN",
	"SETGOM", "SETGOMT", "SETGOMN", "SETGOMTN",
	"SETGOE", "SETGOET", "SETGOEN", "SETGOETN",
};

static const char *const copy_operations[] = {
	"CPYFP", "CPYFPWT", "CPYFPRT", "CPYFPT",
	"CPYFPWN", "CPYFPWTWN", "CPYFPRTWN", "CPYFPTWN",
	"CPYFPRN", "CPYFPWTRN", "CPYFPRTRN", "CPYFPTRN",
	"CPYFPN", "CPYFPWTN", "CPYFPRTN", "CPYFPTN",
};

static const char *const copy_main_operations[] = {
	"CPYFM", "CPYFMWT", "CPYFMRT", "CPYFMT",
	"CPYFMWN", "CPYFMWTWN", "CPYFMRTWN", "CPYFMTWN",
	"CPYFMRN", "CPYFMWTRN", "CPYFMRTRN", "CPYFMTRN",
	"CPYFMN", "CPYFMWTN", "CPYFMRTN", "CPYFMTN",
};

static const char *const copy_epilogue_operations[] = {
	"CPYFE", "CPYFEWT", "CPYFERT", "CPYFET",
	"CPYFEWN", "CPYFEWTWN", "CPYFERTWN", "CPYFETWN",
	"CPYFERN", "CPYFEWTRN", "CPYFERTRN", "CPYFETRN",
	"CPYFEN", "CPYFEWTN", "CPYFERTN", "CPYFETN",
};

static const char *const set_operations[] = {
	"SETP", "SETPT", "SETPN", "SETPTN",
};

static const char *const set_main_operations[] = {
	"SETM", "SETMT", "SETMN", "SETMTN",
};

static const char *const set_epilogue_operations[] = {
	"SETE", "SETET", "SETEN", "SETETN",
};

static const char *const copy_backward_operations[] = {
	"CPYP", "CPYPWT", "CPYPRT", "CPYPT",
	"CPYPWN", "CPYPWTWN", "CPYPRTWN", "CPYPTWN",
	"CPYPRN", "CPYPWTRN", "CPYPRTRN", "CPYPTRN",
	"CPYPN", "CPYPWTN", "CPYPRTN", "CPYPTN",
};

static const char *const copy_backward_main_operations[] = {
	"CPYM", "CPYMWT", "CPYMRT", "CPYMT",
	"CPYMWN", "CPYMWTWN", "CPYMRTWN", "CPYMTWN",
	"CPYMRN", "CPYMWTRN", "CPYMRTRN", "CPYMTRN",
	"CPYMN", "CPYMWTN", "CPYMRTN", "CPYMTN",
};

static const char *const copy_backward_epilogue_operations[] = {
	"CPYE", "CPYEWT", "CPYERT", "CPYET",
	"CPYEWN", "CPYEWTWN", "CPYERTWN", "CPYETWN",
	"CPYERN", "CPYEWTRN", "CPYERTRN", "CPYETRN",
	"CPYEN", "CPYEWTN", "CPYERTN", "CPYETN",
};

static const char *const set_tagged_operations[] = {
	"SETGP", "SETGPT", "SETGPN", "SETGPTN",
};

static const char *const set_tagged_main_operations[] = {
	"SETGM", "SETGMT", "SETGMN", "SETGMTN",
};

static const char *const set_tagged_epilogue_operations[] = {
	"SETGE", "SETGET", "SETGEN", "SETGETN",
};

static const struct orlix_tcti_mops_leaf_group mops_leaf_groups[] = {
	{ 2675U, 12U, "FEAT_MOPS_GO", set_go_operations,
	  ARRAY_SIZE(set_go_operations), set_go_operations,
	  ARRAY_SIZE(set_go_operations),
	  ORLIX_TCTI_MOPS_PHASE_SET_GO },
	{ 2704U, 16U, "FEAT_MOPS", copy_operations,
	  ARRAY_SIZE(copy_operations), copy_operations,
	  ARRAY_SIZE(copy_operations),
	  ORLIX_TCTI_MOPS_PHASE_COPY_FORWARD_PROLOGUE },
	{ 2720U, 16U, "FEAT_MOPS", copy_main_operations,
	  ARRAY_SIZE(copy_main_operations), copy_operations,
	  ARRAY_SIZE(copy_operations),
	  ORLIX_TCTI_MOPS_PHASE_COPY_FORWARD_MAIN },
	{ 2736U, 16U, "FEAT_MOPS", copy_epilogue_operations,
	  ARRAY_SIZE(copy_epilogue_operations), copy_operations,
	  ARRAY_SIZE(copy_operations),
	  ORLIX_TCTI_MOPS_PHASE_COPY_FORWARD_EPILOGUE },
	{ 2752U, 4U, "FEAT_MOPS", set_operations,
	  ARRAY_SIZE(set_operations), set_operations,
	  ARRAY_SIZE(set_operations),
	  ORLIX_TCTI_MOPS_PHASE_SET_PROLOGUE },
	{ 2756U, 4U, "FEAT_MOPS", set_main_operations,
	  ARRAY_SIZE(set_main_operations), set_operations,
	  ARRAY_SIZE(set_operations),
	  ORLIX_TCTI_MOPS_PHASE_SET_MAIN },
	{ 2760U, 4U, "FEAT_MOPS", set_epilogue_operations,
	  ARRAY_SIZE(set_epilogue_operations), set_operations,
	  ARRAY_SIZE(set_operations),
	  ORLIX_TCTI_MOPS_PHASE_SET_EPILOGUE },
	{ 2764U, 16U, "FEAT_MOPS", copy_backward_operations,
	  ARRAY_SIZE(copy_backward_operations), copy_backward_operations,
	  ARRAY_SIZE(copy_backward_operations),
	  ORLIX_TCTI_MOPS_PHASE_COPY_BACKWARD_PROLOGUE },
	{ 2780U, 16U, "FEAT_MOPS", copy_backward_main_operations,
	  ARRAY_SIZE(copy_backward_main_operations), copy_backward_operations,
	  ARRAY_SIZE(copy_backward_operations),
	  ORLIX_TCTI_MOPS_PHASE_COPY_BACKWARD_MAIN },
	{ 2796U, 16U, "FEAT_MOPS", copy_backward_epilogue_operations,
	  ARRAY_SIZE(copy_backward_epilogue_operations), copy_backward_operations,
	  ARRAY_SIZE(copy_backward_operations),
	  ORLIX_TCTI_MOPS_PHASE_COPY_BACKWARD_EPILOGUE },
	{ 2812U, 4U, "FEAT_MOPS", set_tagged_operations,
	  ARRAY_SIZE(set_tagged_operations), set_tagged_operations,
	  ARRAY_SIZE(set_tagged_operations),
	  ORLIX_TCTI_MOPS_PHASE_SET_TAGGED_PROLOGUE },
	{ 2816U, 4U, "FEAT_MOPS", set_tagged_main_operations,
	  ARRAY_SIZE(set_tagged_main_operations), set_tagged_operations,
	  ARRAY_SIZE(set_tagged_operations),
	  ORLIX_TCTI_MOPS_PHASE_SET_TAGGED_MAIN },
	{ 2820U, 4U, "FEAT_MOPS", set_tagged_epilogue_operations,
	  ARRAY_SIZE(set_tagged_epilogue_operations), set_tagged_operations,
	  ARRAY_SIZE(set_tagged_operations),
	  ORLIX_TCTI_MOPS_PHASE_SET_TAGGED_EPILOGUE },
};

bool orlix_tcti_mops_leaf_provenance(u32 source_ordinal,
			       struct orlix_tcti_mops_leaf_provenance *provenance)
{
	const struct orlix_tcti_mops_leaf_group *group;
	u32 index;
	unsigned int i;

	if (!provenance)
		return false;

	for (i = 0; i < ARRAY_SIZE(mops_leaf_groups); ++i) {
		group = &mops_leaf_groups[i];
		if (source_ordinal < group->first_ordinal ||
		    source_ordinal >= group->first_ordinal + group->leaf_count)
			continue;

		index = source_ordinal - group->first_ordinal;
		provenance->source_ordinal = source_ordinal;
		provenance->feature = group->feature;
		provenance->operation = group->operations[index % group->operation_count];
		provenance->asl_operation =
			group->asl_operations[index % group->asl_operation_count];
		provenance->phase = group->phase;
		provenance->semantic_provenance =
			group->phase == ORLIX_TCTI_MOPS_PHASE_SET_GO ?
			ORLIX_TCTI_MOPS_PROVENANCE_OFFICIAL_NOT_SPECIFIED :
			ORLIX_TCTI_MOPS_PROVENANCE_EXTERNAL_DDI0602;
		if ((source_ordinal >= 2704U && source_ordinal <= 2751U) ||
		    (source_ordinal >= 2764U && source_ordinal <= 2811U)) {
			provenance->ddi0602_locator =
				orlix_tcti_mops_ddi0602_locator(source_ordinal);
			provenance->ddi0602_archive_sha256 =
				ORLIX_TCTI_DDI0602_2026_06_ARCHIVE_SHA256;
			provenance->production_owner =
				"orlix_tcti_execute_mops_copy";
			provenance->kunit_suite = "orlix-tcti-mops-copy";
			provenance->linux_proof_disposition =
				"not_applicable_no_linux_visible_abi";
			provenance->implementation_status =
				ORLIX_TCTI_MOPS_IMPLEMENTATION_PRODUCTION;
			provenance->proof_status =
				ORLIX_TCTI_MOPS_PROOF_KUNIT_OWNER;
		} else {
			provenance->ddi0602_locator = NULL;
			provenance->ddi0602_archive_sha256 = NULL;
			provenance->production_owner = NULL;
			provenance->kunit_suite = NULL;
			provenance->linux_proof_disposition = NULL;
			provenance->implementation_status =
				ORLIX_TCTI_MOPS_IMPLEMENTATION_REQUIRED_UNIMPLEMENTED;
			provenance->proof_status =
				ORLIX_TCTI_MOPS_PROOF_REQUIRED_UNPROVEN;
		}
		return true;
	}

	return false;
}
