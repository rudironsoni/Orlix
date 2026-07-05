// SPDX-License-Identifier: GPL-2.0-only
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <linux/types.h>

#define NO_SYSCALL (-1)
#define PSR_MODE_MASK 0xfUL
#define PSR_MODE_EL0t 0x00000000UL

struct pt_regs {
	u64 regs[31];
	u64 sp;
	u64 pc;
	u64 pstate;
	u64 orig_x0;
	s32 syscallno;
	u32 unused;
};

struct tcti_kernel_execve_binfmt_elf_smoke_payload {
	unsigned char elf_class;
	unsigned char elf_data;
	unsigned short elf_type;
	unsigned short elf_machine;
	unsigned int load_segment_count;
	unsigned long entry_pc;
	unsigned long stack_top;
};

struct tcti_kernel_execve_binfmt_elf_smoke_result {
	bool payload_is_elf64;
	bool payload_is_little_endian;
	bool payload_is_aarch64;
	bool payload_has_load_segment;
	bool payload_type_supported;
	bool arch_accepts_payload;
	bool start_thread_called;
	bool entry_pc_recorded;
	bool stack_pointer_recorded;
	bool user_mode_prepared;
	bool syscall_state_cleared;
	unsigned long entry_pc;
	unsigned long stack_pointer;
	s32 syscallno_after_start_thread;
};

static void start_thread_for_smoke(struct pt_regs *regs, unsigned long pc,
				   unsigned long sp)
{
	memset(regs, 0, sizeof(*regs));
	regs->pc = pc;
	regs->sp = sp;
	regs->pstate = PSR_MODE_EL0t;
	regs->syscallno = NO_SYSCALL;
}

#include "../execve_binfmt_smoke.h"

static unsigned long parse_ulong(const char *value)
{
	return strtoul(value, NULL, 0);
}

static bool smoke_result_passed(
	const struct tcti_kernel_execve_binfmt_elf_smoke_result *result,
	const struct tcti_kernel_execve_binfmt_elf_smoke_payload *payload)
{
	return result->payload_is_elf64 &&
	       result->payload_is_little_endian &&
	       result->payload_is_aarch64 &&
	       result->payload_has_load_segment &&
	       result->payload_type_supported &&
	       result->arch_accepts_payload &&
	       result->start_thread_called &&
	       result->entry_pc_recorded &&
	       result->stack_pointer_recorded &&
	       result->user_mode_prepared &&
	       result->syscall_state_cleared &&
	       result->entry_pc == payload->entry_pc &&
	       result->stack_pointer == payload->stack_top - sizeof(unsigned long) &&
	       result->syscallno_after_start_thread == NO_SYSCALL;
}

int main(int argc, char **argv)
{
	const char *test_name =
		"tcti_kernel_execve_binfmt_elf_smoke_prepares_tcti_entry";
	struct tcti_kernel_execve_binfmt_elf_smoke_payload payload;
	struct tcti_kernel_execve_binfmt_elf_smoke_result result;
	struct pt_regs regs;
	bool ok;

	if (argc != 7) {
		fprintf(stderr,
			"usage: %s <elf_class> <elf_data> <elf_type> <elf_machine> <load_segments> <entry_pc>\n",
			argv[0]);
		return 2;
	}

	memset(&payload, 0, sizeof(payload));
	memset(&result, 0, sizeof(result));
	memset(&regs, 0, sizeof(regs));
	payload.elf_class = (unsigned char)parse_ulong(argv[1]);
	payload.elf_data = (unsigned char)parse_ulong(argv[2]);
	payload.elf_type = (unsigned short)parse_ulong(argv[3]);
	payload.elf_machine = (unsigned short)parse_ulong(argv[4]);
	payload.load_segment_count = (unsigned int)parse_ulong(argv[5]);
	payload.entry_pc = parse_ulong(argv[6]);
	payload.stack_top = 0x0000800000000000UL;

	ok = tcti_kernel_execve_binfmt_elf_smoke_execute(
		&payload, &regs, &result, start_thread_for_smoke);
	ok = ok && smoke_result_passed(&result, &payload);

	printf("ORLIX-EXECVE-BINFMT-RUNNER-BEGIN\n");
	printf("KTAP version 1\n");
	printf("1..1\n");
	if (ok)
		printf("ok 1 - orlix-tcti-decode.%s\n", test_name);
	else
		printf("not ok 1 - orlix-tcti-decode.%s\n", test_name);
	printf("%s=%s\n", test_name, ok ? "pass" : "fail");
	printf("ORLIX-EXECVE-BINFMT-RUNNER payload_is_elf64=%s\n",
	       result.payload_is_elf64 ? "true" : "false");
	printf("ORLIX-EXECVE-BINFMT-RUNNER payload_is_little_endian=%s\n",
	       result.payload_is_little_endian ? "true" : "false");
	printf("ORLIX-EXECVE-BINFMT-RUNNER payload_is_aarch64=%s\n",
	       result.payload_is_aarch64 ? "true" : "false");
	printf("ORLIX-EXECVE-BINFMT-RUNNER payload_has_load_segment=%s\n",
	       result.payload_has_load_segment ? "true" : "false");
	printf("ORLIX-EXECVE-BINFMT-RUNNER payload_type_supported=%s\n",
	       result.payload_type_supported ? "true" : "false");
	printf("ORLIX-EXECVE-BINFMT-RUNNER arch_accepts_payload=%s\n",
	       result.arch_accepts_payload ? "true" : "false");
	printf("ORLIX-EXECVE-BINFMT-RUNNER start_thread_called=%s\n",
	       result.start_thread_called ? "true" : "false");
	printf("ORLIX-EXECVE-BINFMT-RUNNER entry_pc=0x%lx\n",
	       result.entry_pc);
	printf("ORLIX-EXECVE-BINFMT-RUNNER stack_pointer=0x%lx\n",
	       result.stack_pointer);
	printf("ORLIX-EXECVE-BINFMT-RUNNER user_mode_prepared=%s\n",
	       result.user_mode_prepared ? "true" : "false");
	printf("ORLIX-EXECVE-BINFMT-RUNNER syscall_state_cleared=%s\n",
	       result.syscall_state_cleared ? "true" : "false");
	printf("ORLIX-EXECVE-BINFMT-RUNNER-END status=%s\n",
	       ok ? "pass" : "fail");

	return ok ? 0 : 1;
}
