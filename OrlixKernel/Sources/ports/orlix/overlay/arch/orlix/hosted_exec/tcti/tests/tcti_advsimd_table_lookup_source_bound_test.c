// SPDX-License-Identifier: GPL-2.0-only
/*
 * The eight direct AARCHMRS 2026-06 leaves 3672 through 3679 are the
 * complete classic AdvSIMD TBL/TBX family.  The architectural encoding has
 * no reserved table-count or Q combinations: the pinned leaves exhaust the
 * four table lengths for each of TBL and TBX.
 */
#include <kunit/test.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/syscalls.h>

#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/tcti.h>

#include "../decode_aarch64.h"

#define TCTI_ADVSIMD_TABLE_SVC 0xd4000001U

struct tcti_advsimd_table_leaf {
  u32 source_ordinal;
  const char *source_name;
  u32 source_mask;
  u32 source_pattern;
  u8 table_count;
  enum tcti_simd_table_lookup_op operation;
};

struct tcti_advsimd_table_manifest_leaf {
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
static const struct tcti_advsimd_table_manifest_leaf
    tcti_advsimd_table_manifest[] = {
#include "../isa/source_manifest.def"
};
#undef TCTI_A64_SOURCE_MANIFEST_ROW
#undef TCTI_A64_SOURCE_MANIFEST_SOURCE

static const struct tcti_advsimd_table_leaf tcti_advsimd_table_leaves[] = {
    {3672U, "TBL_asimdtbl_L1_1", 0xbfe0fc00U, 0x0e000000U, 1U,
     TCTI_SIMD_TABLE_LOOKUP_TBL},
    {3673U, "TBX_asimdtbl_L1_1", 0xbfe0fc00U, 0x0e001000U, 1U,
     TCTI_SIMD_TABLE_LOOKUP_TBX},
    {3674U, "TBL_asimdtbl_L2_2", 0xbfe0fc00U, 0x0e002000U, 2U,
     TCTI_SIMD_TABLE_LOOKUP_TBL},
    {3675U, "TBX_asimdtbl_L2_2", 0xbfe0fc00U, 0x0e003000U, 2U,
     TCTI_SIMD_TABLE_LOOKUP_TBX},
    {3676U, "TBL_asimdtbl_L3_3", 0xbfe0fc00U, 0x0e004000U, 3U,
     TCTI_SIMD_TABLE_LOOKUP_TBL},
    {3677U, "TBX_asimdtbl_L3_3", 0xbfe0fc00U, 0x0e005000U, 3U,
     TCTI_SIMD_TABLE_LOOKUP_TBX},
    {3678U, "TBL_asimdtbl_L4_4", 0xbfe0fc00U, 0x0e006000U, 4U,
     TCTI_SIMD_TABLE_LOOKUP_TBL},
    {3679U, "TBX_asimdtbl_L4_4", 0xbfe0fc00U, 0x0e007000U, 4U,
     TCTI_SIMD_TABLE_LOOKUP_TBX},
};

static const struct tcti_advsimd_table_manifest_leaf *
tcti_advsimd_table_source_leaf(u32 ordinal) {
  size_t index;

  for (index = 0; index < ARRAY_SIZE(tcti_advsimd_table_manifest); index++)
    if (tcti_advsimd_table_manifest[index].ordinal == ordinal)
      return &tcti_advsimd_table_manifest[index];
  return NULL;
}

static u32
tcti_advsimd_table_instruction(const struct tcti_advsimd_table_leaf *leaf, u8 q,
                               u8 rd, u8 rn, u8 rm) {
  return leaf->source_pattern | ((u32)q << 30) | ((u32)rm << 16) |
         ((u32)rn << 5) | rd;
}

static unsigned long tcti_advsimd_table_map_instruction(struct kunit *test,
                                                        u32 instruction) {
  const u32 program[] = {instruction, TCTI_ADVSIMD_TABLE_SVC};
  unsigned long mapped;
  int ret;

  KUNIT_ASSERT_NOT_NULL(test, current->mm);
  mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
  ret = tcti_write_user_data(current->mm, mapped, program, sizeof(program));
  if (ret) {
    vm_munmap(mapped, PAGE_SIZE);
    KUNIT_FAIL(test, "could not write AdvSIMD table program: %d", ret);
    return 0;
  }
  ret = sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_EXEC);
  if (ret) {
    vm_munmap(mapped, PAGE_SIZE);
    KUNIT_FAIL(test, "could not protect AdvSIMD table program: %d", ret);
    return 0;
  }
  return mapped;
}

static u8 tcti_advsimd_table_byte(const u64 simd[], u8 reg, u8 lane) {
  return simd[reg * 2 + lane / sizeof(u64)] >> ((lane % sizeof(u64)) * 8);
}

static void tcti_advsimd_table_set_byte(u64 simd[], u8 reg, u8 lane, u8 value) {
  u64 *word = &simd[reg * 2 + lane / sizeof(u64)];
  u8 shift = (lane % sizeof(u64)) * 8;

  *word = (*word & ~(0xffULL << shift)) | ((u64)value << shift);
}

static void tcti_advsimd_table_source_leaves_decode(struct kunit *test) {
  size_t index;
  u8 q;

  KUNIT_ASSERT_EQ(test, 8U, ARRAY_SIZE(tcti_advsimd_table_leaves));
  for (index = 0; index < ARRAY_SIZE(tcti_advsimd_table_leaves); index++) {
    const struct tcti_advsimd_table_leaf *leaf =
        &tcti_advsimd_table_leaves[index];
    const struct tcti_advsimd_table_manifest_leaf *source =
        tcti_advsimd_table_source_leaf(leaf->source_ordinal);

    KUNIT_ASSERT_NOT_NULL(test, source);
    KUNIT_EXPECT_STREQ(test, leaf->source_name, source->id);
    KUNIT_EXPECT_EQ(test, leaf->source_mask, source->mask);
    KUNIT_EXPECT_EQ(test, leaf->source_pattern, source->pattern);
    for (q = 0; q < 2; q++) {
      struct tcti_decoded_instruction decoded = tcti_decode_aarch64(
          tcti_advsimd_table_instruction(leaf, q, 31, 30, 29));

      KUNIT_EXPECT_EQ_MSG(test, TCTI_DECODE_SIMD_TABLE_LOOKUP,
                          decoded.decode_class, "%s q=%u", leaf->source_name,
                          q);
      KUNIT_EXPECT_EQ(test, leaf->operation, decoded.simd_table_lookup_op);
      KUNIT_EXPECT_EQ(test, leaf->table_count, decoded.simd_table_count);
      KUNIT_EXPECT_EQ(test, q != 0, decoded.simd_q);
      KUNIT_EXPECT_EQ(test, q ? 16U : 8U, decoded.result_size);
      KUNIT_EXPECT_EQ(test, 31, decoded.rd);
      KUNIT_EXPECT_EQ(test, 30, decoded.rn);
      KUNIT_EXPECT_EQ(test, 29, decoded.rm);
    }
  }
}

static void tcti_advsimd_table_source_leaves_execute(struct kunit *test) {
  size_t leaf_index;
  u8 q;

  for (leaf_index = 0; leaf_index < ARRAY_SIZE(tcti_advsimd_table_leaves);
       leaf_index++) {
    const struct tcti_advsimd_table_leaf *leaf =
        &tcti_advsimd_table_leaves[leaf_index];

    for (q = 0; q < 2; q++) {
      u8 overlap = (leaf_index * 2 + q) & 3U;
      u8 rd = overlap == 1 ? 20 : overlap == 3 ? 6 : 4;
      u8 rn = 20;
      u8 rm = overlap == 2 ? 20 : overlap == 3 ? 6 : 6;
      u8 result_size = q ? 16 : 8;
      u32 instruction = tcti_advsimd_table_instruction(leaf, q, rd, rn, rm);
      unsigned long mapped =
          tcti_advsimd_table_map_instruction(test, instruction);
      struct pt_regs regs = {};
      struct pt_regs before;
      struct tcti_result result;
      u32 observed_program[2];
      u64 saved_simd[ARRAY_SIZE(current->thread.user_simd)];
      u64 before_simd[ARRAY_SIZE(current->thread.user_simd)];
      u64 expected_simd[ARRAY_SIZE(current->thread.user_simd)];
      unsigned long saved_valid = current->thread.user_simd_valid;
      unsigned long saved_fpcr = current->thread.user_fpcr;
      unsigned long saved_fpsr = current->thread.user_fpsr;
      unsigned int reg;
      u8 lane;

      KUNIT_ASSERT_NE(test, 0UL, mapped);
      memcpy(saved_simd, current->thread.user_simd, sizeof(saved_simd));
      for (reg = 0; reg < ARRAY_SIZE(current->thread.user_simd); reg++)
        current->thread.user_simd[reg] = 0x9e3779b97f4a7c15ULL ^ reg;
      for (lane = 0; lane < leaf->table_count * 16; lane++)
        tcti_advsimd_table_set_byte(current->thread.user_simd,
                                    (rn + lane / 16) & 31U, lane % 16,
                                    0x40U + lane);
      for (lane = 0; lane < 16; lane++)
        tcti_advsimd_table_set_byte(current->thread.user_simd, rm, lane,
                                    lane & 1 ? leaf->table_count * 16 + lane
                                             : (lane * 7) %
                                                   (leaf->table_count * 16));

      memcpy(before_simd, current->thread.user_simd, sizeof(before_simd));
      memcpy(expected_simd, before_simd, sizeof(expected_simd));
      for (lane = 0; lane < 16; lane++) {
        u8 index = tcti_advsimd_table_byte(before_simd, rm, lane);
        u8 value = 0;

        if (lane < result_size && index < leaf->table_count * 16)
          value = tcti_advsimd_table_byte(before_simd, (rn + index / 16) & 31U,
                                          index % 16);
        else if (lane < result_size &&
                 leaf->operation == TCTI_SIMD_TABLE_LOOKUP_TBX)
          value = tcti_advsimd_table_byte(before_simd, rd, lane);
        tcti_advsimd_table_set_byte(expected_simd, rd, lane, value);
      }

      for (reg = 0; reg < ARRAY_SIZE(regs.regs); reg++)
        regs.regs[reg] = 0x9876000000000000ULL | reg;
      regs.pc = mapped;
      regs.sp = 0x00000001fffffff0ULL;
      regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT;
      regs.syscallno = NO_SYSCALL;
      before = regs;
      current->thread.user_simd_valid = 0;
      current->thread.user_fpcr = BIT(22) | BIT(24);
      current->thread.user_fpsr = BIT(27) | BIT(4);
      result = tcti_resume_user(current, &regs, current->mm);

      KUNIT_EXPECT_EQ_MSG(test, TCTI_EXIT_SYSCALL, result.reason,
                          "%s q=%u overlap=%u", leaf->source_name, q, overlap);
      KUNIT_EXPECT_EQ(test, 0L, result.status);
      KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), result.pc);
      KUNIT_EXPECT_EQ(test, TCTI_ADVSIMD_TABLE_SVC, result.instruction);
      KUNIT_ASSERT_EQ(test, 0,
                      tcti_read_user_data(current->mm, mapped, observed_program,
                                          sizeof(observed_program)));
      KUNIT_EXPECT_EQ(test, instruction, observed_program[0]);
      KUNIT_EXPECT_EQ(test, TCTI_ADVSIMD_TABLE_SVC, observed_program[1]);
      KUNIT_EXPECT_MEMEQ(test, expected_simd, current->thread.user_simd,
                         sizeof(expected_simd));
      KUNIT_EXPECT_EQ(test, 1UL, current->thread.user_simd_valid);
      KUNIT_EXPECT_EQ(test, BIT(22) | BIT(24), current->thread.user_fpcr);
      KUNIT_EXPECT_EQ(test, BIT(27) | BIT(4), current->thread.user_fpsr);
      KUNIT_EXPECT_MEMEQ(test, before.regs, regs.regs, sizeof(regs.regs));
      KUNIT_EXPECT_EQ(test, before.sp, regs.sp);
      KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);
      KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), regs.pc);

      memcpy(current->thread.user_simd, saved_simd, sizeof(saved_simd));
      current->thread.user_simd_valid = saved_valid;
      current->thread.user_fpcr = saved_fpcr;
      current->thread.user_fpsr = saved_fpsr;
      KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
    }
  }
}

static struct kunit_case tcti_advsimd_table_source_bound_cases[] = {
    KUNIT_CASE(tcti_advsimd_table_source_leaves_decode),
    KUNIT_CASE(tcti_advsimd_table_source_leaves_execute),
    {}};

static struct kunit_suite tcti_advsimd_table_source_bound_suite = {
    .name = "orlix-tcti-advsimd-table-source-bound",
    .test_cases = tcti_advsimd_table_source_bound_cases,
};

kunit_test_suite(tcti_advsimd_table_source_bound_suite);

MODULE_LICENSE("GPL");
