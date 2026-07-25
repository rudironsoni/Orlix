// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/syscalls.h>

#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/tcti.h>

#include "../decode_aarch64.h"

/*
 * Pinned AARCHMRS 2026-06 direct leaves in the AdvSIMD modified-immediate
 * family. The shared ASL provenance ledger remains an explicit completion
 * blocker for this regression coverage.
 */
#define TCTI_ADVSIMD_MODIMM_SVC 0xd4000001U
#define TCTI_ADVSIMD_MODIMM_RD 4U

struct tcti_advsimd_modimm_leaf {
  u16 source_ordinal;
  const char *source_name;
  u32 source_mask;
  u32 source_pattern;
  enum tcti_simd_modified_immediate_op operation;
  u64 immediate;
  u64 expected_low;
  u64 expected_high;
};

struct tcti_advsimd_source_manifest_leaf {
  u32 ordinal;
  const char *id;
  u32 mask;
  u32 pattern;
};

#define TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, id, mnemonic, operation, mask,   \
                                     pattern, feature_predicate, offset,       \
                                     length)                                   \
  {ordinal, id, mask, pattern},
static const struct tcti_advsimd_source_manifest_leaf
    tcti_advsimd_source_manifest[] = {
#include "../isa/source_manifest.def"
};
#undef TCTI_A64_SOURCE_MANIFEST_ROW
#undef TCTI_A64_SOURCE_MANIFEST_SOURCE

static const struct tcti_advsimd_modimm_leaf tcti_advsimd_modimm_leaves[] = {
    {3987, "MVNI_asimdimm_L_sl", 0xbff89c00U, 0x2f000400U,
     TCTI_SIMD_MODIMM_MVNI, 0x000000a5000000a5ULL, 0xffffff5affffff5aULL,
     0xffffff5affffff5aULL},
    {3988, "BIC_asimdimm_L_sl", 0xbff89c00U, 0x2f001400U, TCTI_SIMD_MODIMM_BIC,
     0x000000a5000000a5ULL, 0x1122334011223340ULL, 0x5566770855667708ULL},
    {3989, "MVNI_asimdimm_L_hl", 0xbff8dc00U, 0x2f008400U,
     TCTI_SIMD_MODIMM_MVNI, 0x00a500a500a500a5ULL, 0xff5aff5aff5aff5aULL,
     0xff5aff5aff5aff5aULL},
    {3990, "BIC_asimdimm_L_hl", 0xbff8dc00U, 0x2f009400U, TCTI_SIMD_MODIMM_BIC,
     0x00a500a500a500a5ULL, 0x1102334011023340ULL, 0x5542770855427708ULL},
    {3991, "MVNI_asimdimm_M_sm", 0xbff8ec00U, 0x2f00c400U,
     TCTI_SIMD_MODIMM_MVNI, 0xa5ffa5ffa5ffa5ffULL, 0x5a005a005a005a00ULL,
     0x5a005a005a005a00ULL},
};

static const struct tcti_advsimd_source_manifest_leaf *
tcti_advsimd_modimm_source_leaf(u32 ordinal) {
  size_t index;

  for (index = 0; index < ARRAY_SIZE(tcti_advsimd_source_manifest); index++)
    if (tcti_advsimd_source_manifest[index].ordinal == ordinal)
      return &tcti_advsimd_source_manifest[index];
  return NULL;
}

static u32
tcti_advsimd_modimm_instruction(const struct tcti_advsimd_modimm_leaf *leaf,
                                bool q, u8 imm8, u8 rd) {
  return leaf->source_pattern | (q ? BIT(30) : 0) |
         ((u32)(imm8 & 0xe0U) << 11) | ((u32)(imm8 & 0x1fU) << 5) | rd;
}

static unsigned long tcti_advsimd_modimm_map_program(struct kunit *test,
                                                     u32 instruction) {
  const u32 program[] = {instruction, TCTI_ADVSIMD_MODIMM_SVC};
  unsigned long mapped;
  int ret;

  KUNIT_ASSERT_NOT_NULL(test, current->mm);
  mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
  ret = tcti_write_user_data(current->mm, mapped, program, sizeof(program));
  if (ret) {
    vm_munmap(mapped, PAGE_SIZE);
    KUNIT_FAIL(test, "could not write modified-immediate program: %d", ret);
    return 0;
  }
  ret = sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_EXEC);
  if (ret) {
    vm_munmap(mapped, PAGE_SIZE);
    KUNIT_FAIL(test, "could not protect modified-immediate program: %d", ret);
    return 0;
  }
  return mapped;
}

static void tcti_advsimd_modimm_decode_source_leaves(struct kunit *test) {
  static const u16 expected_ordinals[] = {3987, 3988, 3989, 3990, 3991};
  size_t index;

  KUNIT_ASSERT_EQ(test, 5U, ARRAY_SIZE(tcti_advsimd_modimm_leaves));
  KUNIT_ASSERT_EQ(test, ARRAY_SIZE(expected_ordinals),
                  ARRAY_SIZE(tcti_advsimd_modimm_leaves));
  for (index = 0; index < ARRAY_SIZE(tcti_advsimd_modimm_leaves); index++) {
    const struct tcti_advsimd_modimm_leaf *leaf =
        &tcti_advsimd_modimm_leaves[index];
    const struct tcti_advsimd_source_manifest_leaf *source =
        tcti_advsimd_modimm_source_leaf(leaf->source_ordinal);
    struct tcti_decoded_instruction decoded =
        tcti_decode_aarch64(tcti_advsimd_modimm_instruction(
            leaf, false, 0xa5U, TCTI_ADVSIMD_MODIMM_RD));

    KUNIT_EXPECT_TRUE(test, leaf->source_name[0]);
    KUNIT_EXPECT_EQ(test, expected_ordinals[index], leaf->source_ordinal);
    KUNIT_ASSERT_NOT_NULL(test, source);
    KUNIT_EXPECT_STREQ(test, leaf->source_name, source->id);
    KUNIT_EXPECT_EQ(test, leaf->source_mask, source->mask);
    KUNIT_EXPECT_EQ(test, leaf->source_pattern, source->pattern);
    KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
                    decoded.decode_class);
    KUNIT_EXPECT_EQ(test, leaf->operation, decoded.simd_modified_immediate_op);
    KUNIT_EXPECT_EQ(test, leaf->immediate, decoded.logical_immediate);
    KUNIT_EXPECT_EQ(test, TCTI_ADVSIMD_MODIMM_RD, decoded.rd);
    KUNIT_EXPECT_EQ(test, sizeof(u64), decoded.result_size);
    KUNIT_EXPECT_EQ(test, sizeof(u64), decoded.access_size);

    decoded = tcti_decode_aarch64(tcti_advsimd_modimm_instruction(
        leaf, true, 0xa5U, TCTI_ADVSIMD_MODIMM_RD));
    KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
                    decoded.decode_class);
    KUNIT_EXPECT_EQ(test, leaf->operation, decoded.simd_modified_immediate_op);
    KUNIT_EXPECT_EQ(test, leaf->immediate, decoded.logical_immediate);
    KUNIT_EXPECT_EQ(test, 2 * sizeof(u64), decoded.result_size);
  }
}

static void
tcti_advsimd_modimm_exhaustive_source_encodings(struct kunit *test) {
  size_t index;

  for (index = 0; index < ARRAY_SIZE(tcti_advsimd_modimm_leaves); index++) {
    const struct tcti_advsimd_modimm_leaf *leaf =
        &tcti_advsimd_modimm_leaves[index];
    const struct tcti_advsimd_source_manifest_leaf *source =
        tcti_advsimd_modimm_source_leaf(leaf->source_ordinal);
    unsigned int q;

    KUNIT_ASSERT_NOT_NULL(test, source);
    for (q = 0; q < 2; q++) {
      unsigned int imm8;

      for (imm8 = 0; imm8 < 256; imm8++) {
        unsigned int rd;

        for (rd = 0; rd < 32; rd++) {
          u32 instruction = source->pattern | (q ? BIT(30) : 0) |
                            ((imm8 & 0xe0U) << 11) | ((imm8 & 0x1fU) << 5) | rd;
          struct tcti_decoded_instruction decoded =
              tcti_decode_aarch64(instruction);

          KUNIT_EXPECT_EQ_MSG(test, source->pattern, instruction & source->mask,
                              "source=%u id=%s imm8=%#x rd=%u q=%u",
                              source->ordinal, source->id, imm8, rd, q);
          KUNIT_EXPECT_EQ_MSG(test, TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
                              decoded.decode_class,
                              "source=%u id=%s imm8=%#x rd=%u q=%u",
                              source->ordinal, source->id, imm8, rd, q);
          KUNIT_EXPECT_EQ_MSG(test, leaf->operation,
                              decoded.simd_modified_immediate_op,
                              "source=%u id=%s imm8=%#x rd=%u q=%u",
                              source->ordinal, source->id, imm8, rd, q);
          KUNIT_EXPECT_EQ(test, rd, decoded.rd);
          KUNIT_EXPECT_EQ(test, q ? 2 * sizeof(u64) : sizeof(u64),
                          decoded.result_size);
          KUNIT_EXPECT_EQ(test, decoded.result_size, decoded.access_size);
        }
      }
    }
  }
}

static void tcti_advsimd_modimm_execute_source_leaves(struct kunit *test) {
  size_t index;

  for (index = 0; index < ARRAY_SIZE(tcti_advsimd_modimm_leaves); index++) {
    const struct tcti_advsimd_modimm_leaf *leaf =
        &tcti_advsimd_modimm_leaves[index];
    unsigned int q;

    for (q = 0; q < 2; q++) {
      u32 instruction = tcti_advsimd_modimm_instruction(leaf, q, 0xa5U,
                                                        TCTI_ADVSIMD_MODIMM_RD);
      unsigned long mapped = tcti_advsimd_modimm_map_program(test, instruction);
      struct pt_regs regs = {};
      struct pt_regs before;
      struct tcti_result result;
      u64 saved_simd[ARRAY_SIZE(current->thread.user_simd)];
      u64 expected_simd[ARRAY_SIZE(current->thread.user_simd)];
      unsigned long saved_valid = current->thread.user_simd_valid;
      unsigned int simd_index;

      KUNIT_ASSERT_NE(test, 0UL, mapped);
      memcpy(saved_simd, current->thread.user_simd, sizeof(saved_simd));
      for (simd_index = 0; simd_index < ARRAY_SIZE(current->thread.user_simd);
           simd_index++)
        current->thread.user_simd[simd_index] =
            0x9e3779b97f4a7c15ULL ^ simd_index;
      current->thread.user_simd[TCTI_ADVSIMD_MODIMM_RD * 2] =
          0x1122334411223344ULL;
      current->thread.user_simd[TCTI_ADVSIMD_MODIMM_RD * 2 + 1] =
          0x5566778855667788ULL;
      memcpy(expected_simd, current->thread.user_simd, sizeof(expected_simd));
      expected_simd[TCTI_ADVSIMD_MODIMM_RD * 2] = leaf->expected_low;
      expected_simd[TCTI_ADVSIMD_MODIMM_RD * 2 + 1] =
          q ? leaf->expected_high : 0;
      current->thread.user_simd_valid = 0;
      regs.pc = mapped;
      regs.sp = 0x00000001fffffff0ULL;
      regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT;
      regs.syscallno = NO_SYSCALL;
      before = regs;
      result = tcti_resume_user(current, &regs, current->mm);

      KUNIT_EXPECT_EQ_MSG(test, TCTI_EXIT_SYSCALL, result.reason, "%s q=%u",
                          leaf->source_name, q);
      KUNIT_EXPECT_EQ(test, 0L, result.status);
      KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), result.pc);
      KUNIT_EXPECT_EQ(test, TCTI_ADVSIMD_MODIMM_SVC, result.instruction);
      KUNIT_EXPECT_MEMEQ(test, expected_simd, current->thread.user_simd,
                         sizeof(expected_simd));
      KUNIT_EXPECT_EQ(test, 1UL, current->thread.user_simd_valid);
      KUNIT_EXPECT_MEMEQ(test, before.regs, regs.regs, sizeof(regs.regs));
      KUNIT_EXPECT_EQ(test, before.sp, regs.sp);
      KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);
      KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), regs.pc);

      memcpy(current->thread.user_simd, saved_simd, sizeof(saved_simd));
      current->thread.user_simd_valid = saved_valid;
      KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
    }
  }
}

static struct kunit_case tcti_advsimd_modimm_regression_cases[] = {
    KUNIT_CASE(tcti_advsimd_modimm_decode_source_leaves),
    KUNIT_CASE(tcti_advsimd_modimm_exhaustive_source_encodings),
    KUNIT_CASE(tcti_advsimd_modimm_execute_source_leaves),
    {}};

static struct kunit_suite tcti_advsimd_modimm_regression_suite = {
    .name = "orlix-tcti-advsimd-modimm-regression",
    .test_cases = tcti_advsimd_modimm_regression_cases,
};

kunit_test_suite(tcti_advsimd_modimm_regression_suite);

MODULE_LICENSE("GPL");
