// SPDX-License-Identifier: GPL-2.0-only
#include <asm/ptrace.h>
#include <asm/tcti.h>
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/syscalls.h>

#include "../decode_aarch64.h"

#define TCTI_FP_CONDITIONAL_COMPARE_MASK 0xffe00c10U
#define TCTI_FP_CONDITIONAL_COMPARE_RESERVED_TYPE 2U
#define TCTI_FP_CONDITIONAL_COMPARE_SVC 0xd4000001U
#define TCTI_FP_CONDITIONAL_COMPARE_NZCV                                       \
  (PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT)

struct tcti_fp_conditional_compare_leaf {
  u16 ordinal;
  const char *name;
  u32 pattern;
  u8 access_size;
  bool signal_all_nans;
};

/* Exact required direct leaves from pinned Arm AARCHMRS 2026-06. */
static const struct tcti_fp_conditional_compare_leaf
    tcti_fp_conditional_compare_leaves[] = {
        {4302, "FCCMP_S_floatccmp", 0x1e200400U, sizeof(u32), false},
        {4303, "FCCMPE_S_floatccmp", 0x1e200410U, sizeof(u32), true},
        {4304, "FCCMP_D_floatccmp", 0x1e600400U, sizeof(u64), false},
        {4305, "FCCMPE_D_floatccmp", 0x1e600410U, sizeof(u64), true},
};

static u32 tcti_fp_conditional_compare_instruction(
    const struct tcti_fp_conditional_compare_leaf *leaf, u8 rn, u8 rm,
    u8 condition, u8 nzcv) {
  return leaf->pattern | ((u32)rm << 16) | ((u32)condition << 12) |
         ((u32)rn << 5) | nzcv;
}

static bool tcti_fp_conditional_compare_condition_holds(u8 condition, u8 nzcv) {
  bool n = nzcv & BIT(3);
  bool z = nzcv & BIT(2);
  bool c = nzcv & BIT(1);
  bool v = nzcv & BIT(0);
  bool result;

  switch (condition >> 1) {
  case 0:
    result = z;
    break;
  case 1:
    result = c;
    break;
  case 2:
    result = n;
    break;
  case 3:
    result = v;
    break;
  case 4:
    result = c && !z;
    break;
  case 5:
    result = n == v;
    break;
  case 6:
    result = !z && n == v;
    break;
  default:
    result = true;
    break;
  }

  return (condition & 1U) && condition != 0xfU ? !result : result;
}

static unsigned long tcti_fp_conditional_compare_map(struct kunit *test,
                                                     u32 instruction) {
  const u32 program[] = {instruction, TCTI_FP_CONDITIONAL_COMPARE_SVC};
  unsigned long mapped;
  int ret;

  mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
  ret = tcti_write_user_data(current->mm, mapped, program, sizeof(program));
  KUNIT_ASSERT_EQ(test, 0, ret);
  ret = sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_EXEC);
  KUNIT_ASSERT_EQ(test, 0, ret);
  return mapped;
}

static void tcti_fp_conditional_compare_expect_decode(
    struct kunit *test, const struct tcti_fp_conditional_compare_leaf *leaf,
    u32 instruction, u8 rn, u8 rm, u8 condition, u8 nzcv) {
  struct tcti_decoded_instruction decoded = tcti_decode_aarch64(instruction);

  KUNIT_ASSERT_EQ_MSG(test, TCTI_DECODE_FP_SCALAR_COMPARE, decoded.decode_class,
                      "%s %#x", leaf->name, instruction);
  KUNIT_EXPECT_TRUE(test, decoded.fp_conditional);
  KUNIT_EXPECT_EQ(test, leaf->signal_all_nans, decoded.fp_signal_all_nans);
  KUNIT_EXPECT_EQ(test, rn, decoded.rn);
  KUNIT_EXPECT_EQ(test, rm, decoded.rm);
  KUNIT_EXPECT_EQ(test, condition, decoded.condition);
  KUNIT_EXPECT_EQ(test, nzcv, decoded.nzcv);
  KUNIT_EXPECT_EQ(test, leaf->access_size, decoded.access_size);
  KUNIT_EXPECT_EQ(test, leaf->access_size, decoded.result_size);
  KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
}

static void tcti_fp_conditional_compare_source_bindings(struct kunit *test) {
  static const u16 expected_ordinals[] = {4302, 4303, 4304, 4305};
  size_t index;

  KUNIT_ASSERT_EQ(test, ARRAY_SIZE(expected_ordinals),
                  ARRAY_SIZE(tcti_fp_conditional_compare_leaves));
  for (index = 0; index < ARRAY_SIZE(tcti_fp_conditional_compare_leaves);
       index++) {
    const struct tcti_fp_conditional_compare_leaf *leaf =
        &tcti_fp_conditional_compare_leaves[index];
    u32 instruction =
        tcti_fp_conditional_compare_instruction(leaf, 31, 30, 15, 10);

    KUNIT_EXPECT_EQ(test, expected_ordinals[index], leaf->ordinal);
    KUNIT_EXPECT_TRUE(test, leaf->name[0]);
    KUNIT_EXPECT_EQ(test, leaf->pattern,
                    leaf->pattern & TCTI_FP_CONDITIONAL_COMPARE_MASK);
    tcti_fp_conditional_compare_expect_decode(test, leaf, instruction, 31, 30,
                                              15, 10);
  }
}

static void
tcti_fp_conditional_compare_all_legal_fields_decode(struct kunit *test) {
  size_t index;

  for (index = 0; index < ARRAY_SIZE(tcti_fp_conditional_compare_leaves);
       index++) {
    const struct tcti_fp_conditional_compare_leaf *leaf =
        &tcti_fp_conditional_compare_leaves[index];
    u8 condition;

    for (condition = 0; condition < 16; condition++) {
      u8 nzcv;

      for (nzcv = 0; nzcv < 16; nzcv++) {
        u8 rn;

        for (rn = 0; rn < 32; rn++) {
          u8 rm;

          for (rm = 0; rm < 32; rm++)
            tcti_fp_conditional_compare_expect_decode(
                test, leaf,
                tcti_fp_conditional_compare_instruction(leaf, rn, rm, condition,
                                                        nzcv),
                rn, rm, condition, nzcv);
        }
      }
    }
  }
}

static void tcti_fp_conditional_compare_reserved_type_rejects_all_fields(
    struct kunit *test) {
  u8 signal;

  for (signal = 0; signal < 2; signal++) {
    u8 condition;

    for (condition = 0; condition < 16; condition++) {
      u8 nzcv;

      for (nzcv = 0; nzcv < 16; nzcv++) {
        u8 rn;

        for (rn = 0; rn < 32; rn++) {
          u8 rm;

          for (rm = 0; rm < 32; rm++) {
            u32 instruction =
                0x1e200400U |
                ((u32)TCTI_FP_CONDITIONAL_COMPARE_RESERVED_TYPE << 22) |
                ((u32)rm << 16) | ((u32)condition << 12) | ((u32)rn << 5) |
                (signal << 4) | nzcv;

            KUNIT_EXPECT_EQ_MSG(
                test, TCTI_DECODE_UNSUPPORTED,
                tcti_decode_aarch64(instruction).decode_class,
                "reserved type signal=%u cond=%u nzcv=%u V%u,V%u", signal,
                condition, nzcv, rn, rm);
          }
        }
      }
    }
  }
}

static void tcti_fp_conditional_compare_production_path(
    struct kunit *test, const struct tcti_fp_conditional_compare_leaf *leaf,
    u8 rn, u8 rm, u8 condition, u8 nzcv) {
  u32 instruction =
      tcti_fp_conditional_compare_instruction(leaf, rn, rm, condition, 10);
  const u32 expected_program[] = {
      instruction,
      TCTI_FP_CONDITIONAL_COMPARE_SVC,
  };
  struct pt_regs regs = {};
  struct pt_regs before;
  u64 simd_before[ARRAY_SIZE(current->thread.user_simd)];
  u32 observed_program[ARRAY_SIZE(expected_program)];
  struct tcti_result result;
  unsigned long mapped;
  unsigned long expected_pstate;
  u8 register_index;
  int ret;

  for (register_index = 0; register_index < 31; register_index++)
    regs.regs[register_index] =
        0x9e3779b97f4a7c15ULL ^ ((u64)register_index << 32) ^ instruction;
  regs.sp = 0x00000001fffffff0ULL;
  regs.pstate = PSR_MODE_EL0t |
                (0x155UL & ~(TCTI_FP_CONDITIONAL_COMPARE_NZCV | PSR_MODE_MASK));
  regs.pstate |=
      (nzcv & BIT(3) ? PSR_N_BIT : 0) | (nzcv & BIT(2) ? PSR_Z_BIT : 0) |
      (nzcv & BIT(1) ? PSR_C_BIT : 0) | (nzcv & BIT(0) ? PSR_V_BIT : 0);
  regs.syscallno = NO_SYSCALL;
  for (register_index = 0;
       register_index < ARRAY_SIZE(current->thread.user_simd); register_index++)
    current->thread.user_simd[register_index] =
        0x6a09e667f3bcc909ULL ^ ((u64)register_index << 40) ^ instruction;
  current->thread.user_simd[rn * 2] = leaf->access_size == sizeof(u32)
                                          ? 0xaaaaaaaa3f800000ULL
                                          : 0x3ff0000000000000ULL;
  current->thread.user_simd[rm * 2] = leaf->access_size == sizeof(u32)
                                          ? 0xbbbbbbbb40000000ULL
                                          : 0x4000000000000000ULL;
  current->thread.user_fpsr = BIT(7);
  memcpy(simd_before, current->thread.user_simd, sizeof(simd_before));
  before = regs;
  mapped = tcti_fp_conditional_compare_map(test, instruction);
  regs.pc = mapped;
  before.pc = mapped;

  result = tcti_resume_user(current, &regs, current->mm);
  expected_pstate = before.pstate & ~TCTI_FP_CONDITIONAL_COMPARE_NZCV;
  expected_pstate |=
      tcti_fp_conditional_compare_condition_holds(condition, nzcv)
          ? PSR_N_BIT
          : ((10 & BIT(3) ? PSR_N_BIT : 0) | (10 & BIT(2) ? PSR_Z_BIT : 0) |
             (10 & BIT(1) ? PSR_C_BIT : 0) | (10 & BIT(0) ? PSR_V_BIT : 0));

  KUNIT_EXPECT_EQ_MSG(test, TCTI_EXIT_SYSCALL, result.reason, "%s %#x",
                      leaf->name, instruction);
  KUNIT_EXPECT_EQ(test, 0L, result.status);
  KUNIT_EXPECT_EQ(test, TCTI_FP_CONDITIONAL_COMPARE_SVC, result.instruction);
  KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), result.pc);
  for (register_index = 0; register_index < 31; register_index++)
    KUNIT_EXPECT_EQ(test, before.regs[register_index],
                    regs.regs[register_index]);
  KUNIT_EXPECT_EQ(test, before.sp, regs.sp);
  KUNIT_EXPECT_EQ(test, expected_pstate, regs.pstate);
  KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), regs.pc);
  KUNIT_EXPECT_MEMEQ(test, simd_before, current->thread.user_simd,
                     sizeof(simd_before));
  KUNIT_EXPECT_EQ(test, BIT(7), current->thread.user_fpsr);
  ret = tcti_read_user_data(current->mm, mapped, observed_program,
                            sizeof(observed_program));
  KUNIT_EXPECT_EQ(test, 0, ret);
  KUNIT_EXPECT_MEMEQ(test, expected_program, observed_program,
                     sizeof(expected_program));
  KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void
tcti_fp_conditional_compare_all_leaves_mapped_rx(struct kunit *test) {
  size_t index;

  for (index = 0; index < ARRAY_SIZE(tcti_fp_conditional_compare_leaves);
       index++) {
    const struct tcti_fp_conditional_compare_leaf *leaf =
        &tcti_fp_conditional_compare_leaves[index];
    u8 condition;

    for (condition = 0; condition < 16; condition++) {
      u8 nzcv;

      for (nzcv = 0; nzcv < 16; nzcv++) {
        tcti_fp_conditional_compare_production_path(test, leaf, 1, 2, condition,
                                                    nzcv);
        tcti_fp_conditional_compare_production_path(test, leaf, 31, 30,
                                                    condition, nzcv);
      }
    }
  }
}

static struct kunit_case tcti_fp_conditional_compare_source_bound_test_cases[] =
    {KUNIT_CASE(tcti_fp_conditional_compare_source_bindings),
     KUNIT_CASE(tcti_fp_conditional_compare_all_legal_fields_decode),
     KUNIT_CASE(tcti_fp_conditional_compare_reserved_type_rejects_all_fields),
     KUNIT_CASE(tcti_fp_conditional_compare_all_leaves_mapped_rx),
     {}};

static struct kunit_suite tcti_fp_conditional_compare_source_bound_test_suite =
    {
        .name = "orlix-tcti-fp-conditional-compare-source-bound",
        .test_cases = tcti_fp_conditional_compare_source_bound_test_cases,
};

kunit_test_suite(tcti_fp_conditional_compare_source_bound_test_suite);
