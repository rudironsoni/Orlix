/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_ENGINE_H
#define ORLIX_TCTI_ENGINE_H

#include <linux/elf.h>
#include <asm/tcti.h>

struct tcti_kernel_syscall_dispatch_smoke_result {
	bool decoded_svc;
	bool svc_boundary_reached;
	bool handoff_prepared;
	unsigned long observed_syscall_nr;
	bool orlix_syscall_dispatch_entered;
	bool linux_return_state_written;
	long return_value;
	unsigned long return_x0;
	s32 syscallno_after_dispatch;
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

struct tcti_kernel_fault_signal_smoke_result {
	bool tcti_user_fault_exit;
	bool fault_address_recorded;
	bool fault_access_recorded;
	bool fault_pc_recorded;
	bool fault_instruction_recorded;
	bool linux_fault_handler_entered;
	bool linux_signal_result_recorded;
	unsigned long fault_address;
	enum tcti_access fault_access;
	unsigned long fault_pc;
	u32 fault_instruction;
	int handler_return;
	int signal_number;
	int signal_code;
	unsigned long signaled_address;
};

struct tcti_kernel_wait_reaping_smoke_result {
	bool tcti_task_exit_observed;
	bool child_exit_state_recorded;
	bool linux_wait_entered;
	bool linux_wait_status_recorded;
	bool linux_reaping_completed;
	int child_pid;
	int child_exit_code;
	int wait_result_pid;
	int wait_status;
	int wait_return;
};

struct tcti_kernel_pty_console_smoke_result {
	bool tcti_write_syscall_observed;
	bool linux_stdout_source_recorded;
	bool linux_stderr_source_recorded;
	bool linux_pty_write_entered;
	bool host_console_mirror_called;
	int stdout_fd;
	int stderr_fd;
	unsigned long stdout_bytes;
	unsigned long stderr_bytes;
	unsigned long mirrored_bytes;
};

void tcti_prepare_syscall_handoff(struct pt_regs *regs);
bool tcti_prepare_successful_execve_return(struct pt_regs *regs);
bool tcti_static_pie_initial_tls(unsigned long base, const Elf64_Phdr *phdr,
				 unsigned long *initial_tls);
bool tcti_kernel_syscall_dispatch_smoke_for_tests(
	struct pt_regs *regs,
	struct tcti_kernel_syscall_dispatch_smoke_result *out);
bool tcti_kernel_execve_binfmt_elf_smoke_for_tests(
	const struct tcti_kernel_execve_binfmt_elf_smoke_payload *payload,
	struct pt_regs *regs,
	struct tcti_kernel_execve_binfmt_elf_smoke_result *out);

#endif /* ORLIX_TCTI_ENGINE_H */
