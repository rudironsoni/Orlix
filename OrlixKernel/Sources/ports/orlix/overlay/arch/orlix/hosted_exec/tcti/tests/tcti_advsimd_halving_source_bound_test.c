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
 * The six AdvSIMD halving integer leaves are direct AARCHMRS 2026-06
 * source leaves.  These vectors use arithmetic definitions independent of
 * the executor's overflow-avoiding bit identities, including unsigned
 * underflow for UHSUB.
 */
#define TCTI_ADVSIMD_HALVING_SVC 0xd4000001U
#define TCTI_ADVSIMD_HALVING_RD 4U
#define TCTI_ADVSIMD_HALVING_RN 5U
#define TCTI_ADVSIMD_HALVING_RM 6U

struct tcti_advsimd_halving_leaf {
  u16 source_ordinal;
  const char *source_name;
  u32 source_mask;
  u32 source_pattern;
	enum tcti_simd_vector_arithmetic_op operation;
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

static const struct tcti_advsimd_halving_leaf tcti_advsimd_halving_leaves[] = {
    {3895U, "SHADD_asimdsame_only", 0xbf20fc00U, 0x0e200400U,
     TCTI_SIMD_ARITH_SHADD},
    {3897U, "SRHADD_asimdsame_only", 0xbf20fc00U, 0x0e201400U,
     TCTI_SIMD_ARITH_SRHADD},
    {3898U, "SHSUB_asimdsame_only", 0xbf20fc00U, 0x0e202400U,
     TCTI_SIMD_ARITH_SHSUB},
    {3937U, "UHADD_asimdsame_only", 0xbf20fc00U, 0x2e200400U,
     TCTI_SIMD_ARITH_UHADD},
    {3939U, "URHADD_asimdsame_only", 0xbf20fc00U, 0x2e201400U,
     TCTI_SIMD_ARITH_URHADD},
    {3940U, "UHSUB_asimdsame_only", 0xbf20fc00U, 0x2e202400U,
     TCTI_SIMD_ARITH_UHSUB},
};

static const struct tcti_advsimd_source_manifest_leaf *
tcti_advsimd_halving_source_leaf(u32 ordinal) {
  size_t index;

  for (index = 0; index < ARRAY_SIZE(tcti_advsimd_source_manifest); index++)
    if (tcti_advsimd_source_manifest[index].ordinal == ordinal)
      return &tcti_advsimd_source_manifest[index];
  return NULL;
}

static u32
tcti_advsimd_halving_instruction(const struct tcti_advsimd_halving_leaf *leaf,
                                 u8 q, u8 size, u8 rd, u8 rn, u8 rm) {
  return leaf->source_pattern | ((u32)q << 30) | ((u32)size << 22) |
         ((u32)rm << 16) | ((u32)rn << 5) | rd;
}

static unsigned long tcti_advsimd_halving_map_instruction(struct kunit *test,
                                                          u32 instruction) {
  const u32 program[] = {instruction, TCTI_ADVSIMD_HALVING_SVC};
  unsigned long mapped;
  int ret;

  KUNIT_ASSERT_NOT_NULL(test, current->mm);
  mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
  ret = tcti_write_user_data(current->mm, mapped, program, sizeof(program));
  if (ret) {
    vm_munmap(mapped, PAGE_SIZE);
    KUNIT_FAIL(test, "could not write AdvSIMD halving program: %d", ret);
    return 0;
  }
  ret = sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_EXEC);
  if (ret) {
    vm_munmap(mapped, PAGE_SIZE);
    KUNIT_FAIL(test, "could not protect AdvSIMD halving program: %d", ret);
    return 0;
  }
  return mapped;
}

static s64 tcti_advsimd_floor_div2(s64 value) {
  return value >= 0 ? value / 2 : -(((-value) + 1) / 2);
}

static u64 tcti_advsimd_halving_expected(
	enum tcti_simd_vector_arithmetic_op operation,
                                         u64 left, u64 right, u8 bits) {
  u64 mask = GENMASK_ULL(bits - 1, 0);

  switch (operation) {
  case TCTI_SIMD_ARITH_SHADD:
    return tcti_advsimd_floor_div2(sign_extend64(left, bits - 1) +
                                   sign_extend64(right, bits - 1)) &
           mask;
  case TCTI_SIMD_ARITH_SRHADD:
    return tcti_advsimd_floor_div2(sign_extend64(left, bits - 1) +
                                   sign_extend64(right, bits - 1) + 1) &
           mask;
  case TCTI_SIMD_ARITH_SHSUB:
    return tcti_advsimd_floor_div2(sign_extend64(left, bits - 1) -
                                   sign_extend64(right, bits - 1)) &
           mask;
  case TCTI_SIMD_ARITH_UHADD:
    return (left + right) / 2;
  case TCTI_SIMD_ARITH_URHADD:
    return (left + right + 1) / 2;
  case TCTI_SIMD_ARITH_UHSUB:
    return tcti_advsimd_floor_div2((s64)left - (s64)right) & mask;
  default:
    return 0;
  }
}

static u64 tcti_advsimd_halving_lane_input(u8 bits, u8 lane, bool right) {
  static const u64 left_values[] = {
      0x01ULL, 0x80ULL, 0x7fULL, 0xffULL, 0x02ULL, 0x7eULL, 0x00ULL, 0x81ULL,
  };
  static const u64 right_values[] = {
      0x02ULL, 0x01ULL, 0xffULL, 0x80ULL, 0x01ULL, 0x7fULL, 0xffULL, 0x02ULL,
  };
  u64 value = (right ? right_values : left_values)[lane & 7U];
  u64 mask = GENMASK_ULL(bits - 1, 0);

  if (bits > 8)
    value |=
        (lane & 1U ? mask & ~(mask >> 1) : 0) | ((u64)(lane + 1) << (bits - 8));
  return value & mask;
}

static void tcti_advsimd_halving_fill_register(u64 register_words[2], u8 bytes,
                                               bool right, u8 result_bytes) {
  u8 lane;
  u8 lane_count = result_bytes / bytes;

  register_words[0] = 0;
  register_words[1] = 0;
  for (lane = 0; lane < lane_count; lane++) {
    u8 byte = lane * bytes;
    u8 word = byte / sizeof(u64);
    u8 shift = (byte % sizeof(u64)) * 8;

    register_words[word] |=
        tcti_advsimd_halving_lane_input(bytes * 8, lane, right) << shift;
  }
}

static void tcti_advsimd_halving_expected_register(
	u64 expected_words[2], enum tcti_simd_vector_arithmetic_op operation,
    const u64 left[2], const u64 right[2], u8 bytes, u8 result_bytes) {
  u8 lane;
  u8 lane_count = result_bytes / bytes;
  u64 mask = GENMASK_ULL(bytes * 8 - 1, 0);

  expected_words[0] = 0;
  expected_words[1] = 0;
  for (lane = 0; lane < lane_count; lane++) {
    u8 byte = lane * bytes;
    u8 word = byte / sizeof(u64);
    u8 shift = (byte % sizeof(u64)) * 8;
    u64 lhs = (left[word] >> shift) & mask;
    u64 rhs = (right[word] >> shift) & mask;

    expected_words[word] |=
        tcti_advsimd_halving_expected(operation, lhs, rhs, bytes * 8) << shift;
  }
}

static void tcti_advsimd_halving_source_leaves_decode(struct kunit *test) {
  size_t index;
  u8 size;
  u8 q;

  KUNIT_ASSERT_EQ(test, 6U, ARRAY_SIZE(tcti_advsimd_halving_leaves));
  for (index = 0; index < ARRAY_SIZE(tcti_advsimd_halving_leaves); index++) {
    const struct tcti_advsimd_halving_leaf *leaf =
        &tcti_advsimd_halving_leaves[index];
    const struct tcti_advsimd_source_manifest_leaf *source =
        tcti_advsimd_halving_source_leaf(leaf->source_ordinal);

    KUNIT_ASSERT_NOT_NULL(test, source);
    KUNIT_EXPECT_STREQ(test, leaf->source_name, source->id);
    KUNIT_EXPECT_EQ(test, leaf->source_mask, source->mask);
    KUNIT_EXPECT_EQ(test, leaf->source_pattern, source->pattern);
    for (q = 0; q < 2; q++) {
      for (size = 0; size < 3; size++) {
        struct tcti_decoded_instruction decoded =
            tcti_decode_aarch64(tcti_advsimd_halving_instruction(
                leaf, q, size, TCTI_ADVSIMD_HALVING_RD, TCTI_ADVSIMD_HALVING_RN,
                TCTI_ADVSIMD_HALVING_RM));

        KUNIT_EXPECT_EQ_MSG(test, TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
                            decoded.decode_class, "%s q=%u size=%u",
                            leaf->source_name, q, size);
        KUNIT_EXPECT_EQ(test, leaf->operation, decoded.simd_arithmetic_op);
        KUNIT_EXPECT_EQ(test, BIT(size), decoded.access_size);
        KUNIT_EXPECT_EQ(test, q ? 2 * sizeof(u64) : sizeof(u64),
                        decoded.result_size);
      }
    }
  }
}

static void
tcti_advsimd_halving_reserved_64bit_lanes_reject(struct kunit *test) {
  size_t index;
  u8 q;

  for (index = 0; index < ARRAY_SIZE(tcti_advsimd_halving_leaves); index++) {
    const struct tcti_advsimd_halving_leaf *leaf =
        &tcti_advsimd_halving_leaves[index];

    for (q = 0; q < 2; q++) {
      struct tcti_decoded_instruction decoded =
          tcti_decode_aarch64(tcti_advsimd_halving_instruction(
              leaf, q, 3, TCTI_ADVSIMD_HALVING_RD, TCTI_ADVSIMD_HALVING_RN,
              TCTI_ADVSIMD_HALVING_RM));

      KUNIT_EXPECT_EQ_MSG(test, TCTI_DECODE_UNSUPPORTED, decoded.decode_class,
                          "%s q=%u", leaf->source_name, q);
    }
  }
}

static void tcti_advsimd_halving_source_leaves_execute(struct kunit *test) {
  size_t index;
  u8 size;
  u8 q;

  for (index = 0; index < ARRAY_SIZE(tcti_advsimd_halving_leaves); index++) {
    const struct tcti_advsimd_halving_leaf *leaf =
        &tcti_advsimd_halving_leaves[index];

    for (q = 0; q < 2; q++) {
      for (size = 0; size < 3; size++) {
        u32 instruction = tcti_advsimd_halving_instruction(
            leaf, q, size, TCTI_ADVSIMD_HALVING_RD, TCTI_ADVSIMD_HALVING_RN,
            TCTI_ADVSIMD_HALVING_RM);
        unsigned long mapped =
            tcti_advsimd_halving_map_instruction(test, instruction);
        struct pt_regs regs = {};
        struct pt_regs before;
        struct tcti_result result;
        u64 saved_simd[ARRAY_SIZE(current->thread.user_simd)];
        u64 expected_simd[ARRAY_SIZE(current->thread.user_simd)];
        u64 left[2];
        u64 right[2];
        unsigned long saved_valid = current->thread.user_simd_valid;
        unsigned long saved_fpcr = current->thread.user_fpcr;
        unsigned long saved_fpsr = current->thread.user_fpsr;
        u8 result_bytes = q ? 2 * sizeof(u64) : sizeof(u64);
        unsigned int simd_index;

        KUNIT_ASSERT_NE(test, 0UL, mapped);
        memcpy(saved_simd, current->thread.user_simd, sizeof(saved_simd));
        for (simd_index = 0; simd_index < ARRAY_SIZE(current->thread.user_simd);
             simd_index++)
          current->thread.user_simd[simd_index] =
              0x9e3779b97f4a7c15ULL ^ simd_index;
        tcti_advsimd_halving_fill_register(left, BIT(size), false,
                                           result_bytes);
        tcti_advsimd_halving_fill_register(right, BIT(size), true,
                                           result_bytes);
        current->thread.user_simd[TCTI_ADVSIMD_HALVING_RN * 2] = left[0];
        current->thread.user_simd[TCTI_ADVSIMD_HALVING_RN * 2 + 1] = left[1];
        current->thread.user_simd[TCTI_ADVSIMD_HALVING_RM * 2] = right[0];
        current->thread.user_simd[TCTI_ADVSIMD_HALVING_RM * 2 + 1] = right[1];
        memcpy(expected_simd, current->thread.user_simd, sizeof(expected_simd));
        tcti_advsimd_halving_expected_register(
            &expected_simd[TCTI_ADVSIMD_HALVING_RD * 2], leaf->operation, left,
            right, BIT(size), result_bytes);
        if (leaf->operation == TCTI_SIMD_ARITH_UHSUB) {
          u64 lane_mask = GENMASK_ULL(BIT(size) * 8 - 1, 0);

          KUNIT_EXPECT_EQ(test, lane_mask,
                          expected_simd[TCTI_ADVSIMD_HALVING_RD * 2] &
                              lane_mask);
        }
        current->thread.user_simd_valid = 0;
        current->thread.user_fpcr = BIT(22) | BIT(24);
        current->thread.user_fpsr = BIT(27) | BIT(4);
        regs.pc = mapped;
        regs.sp = 0x00000001fffffff0ULL;
        regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT;
        regs.syscallno = NO_SYSCALL;
        before = regs;
        result = tcti_resume_user(current, &regs, current->mm);

        KUNIT_EXPECT_EQ_MSG(test, TCTI_EXIT_SYSCALL, result.reason,
                            "%s q=%u size=%u", leaf->source_name, q, size);
        KUNIT_EXPECT_EQ(test, 0L, result.status);
        KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), result.pc);
        KUNIT_EXPECT_EQ(test, TCTI_ADVSIMD_HALVING_SVC, result.instruction);
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
}

static struct kunit_case tcti_advsimd_halving_source_bound_cases[] = {
    KUNIT_CASE(tcti_advsimd_halving_source_leaves_decode),
    KUNIT_CASE(tcti_advsimd_halving_reserved_64bit_lanes_reject),
    KUNIT_CASE(tcti_advsimd_halving_source_leaves_execute),
    {}};

static struct kunit_suite tcti_advsimd_halving_source_bound_suite = {
    .name = "orlix-tcti-advsimd-halving-source-bound",
    .test_cases = tcti_advsimd_halving_source_bound_cases,
};

kunit_test_suite(tcti_advsimd_halving_source_bound_suite);

MODULE_LICENSE("GPL");
