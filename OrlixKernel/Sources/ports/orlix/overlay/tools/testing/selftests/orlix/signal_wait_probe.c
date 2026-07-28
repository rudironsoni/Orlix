// SPDX-License-Identifier: GPL-2.0

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/wait.h>
#include <ucontext.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

#ifndef TRAP_BRKPT
#define TRAP_BRKPT 1 /* Linux UAPI asm-generic/siginfo.h */
#endif

static volatile sig_atomic_t usr1_count;
static volatile sig_atomic_t usr2_count;

static void usr1_handler(int signum)
{
	if (signum == SIGUSR1)
		usr1_count++;
}

static void usr2_handler(int signum)
{
	if (signum == SIGUSR2)
		usr2_count++;
}

static bool install_handler(int signum, void (*handler)(int))
{
	struct sigaction action;

	action.sa_handler = handler;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	return sigaction(signum, &action, NULL) == 0;
}

static bool signal_handler_runs(void)
{
	usr1_count = 0;
	if (!install_handler(SIGUSR1, usr1_handler))
		return false;
	if (kill(getpid(), SIGUSR1) != 0)
		return false;
	return usr1_count == 1;
}

static bool blocked_signal_is_pending(void)
{
	sigset_t blocked;
	sigset_t pending;

	usr2_count = 0;
	if (!install_handler(SIGUSR2, usr2_handler))
		return false;
	sigemptyset(&blocked);
	sigaddset(&blocked, SIGUSR2);
	if (sigprocmask(SIG_BLOCK, &blocked, NULL) != 0)
		return false;
	if (kill(getpid(), SIGUSR2) != 0)
		return false;
	if (usr2_count != 0)
		return false;
	if (sigpending(&pending) != 0)
		return false;
	return sigismember(&pending, SIGUSR2) == 1;
}

static bool unblocked_pending_signal_runs(void)
{
	sigset_t unblocked;

	sigemptyset(&unblocked);
	sigaddset(&unblocked, SIGUSR2);
	if (sigprocmask(SIG_UNBLOCK, &unblocked, NULL) != 0)
		return false;
	return usr2_count == 1;
}

static bool waitpid_observes_signal_termination(void)
{
	pid_t child;
	int status = 0;

	child = fork();
	if (child == 0) {
		for (;;)
			pause();
	}
	if (child < 0)
		return false;
	if (kill(child, SIGTERM) != 0)
		return false;
	if (waitpid(child, &status, 0) != child)
		return false;
	return WIFSIGNALED(status) && WTERMSIG(status) == SIGTERM;
}

enum undefined_instruction {
	UNDEFINED_BRK,
	UNDEFINED_HLT,
	UNDEFINED_UDF,
	UNDEFINED_HVC,
	UNDEFINED_SMC,
	UNDEFINED_DCPS1,
	UNDEFINED_DCPS2,
	UNDEFINED_DCPS3,
};

static volatile sig_atomic_t exact_signal_seen;
static volatile sig_atomic_t exact_signal_error;
static int exact_expected_signal;
static int exact_expected_code;
static uintptr_t exact_expected_pc;

static void exact_instruction_handler(int signal_number, siginfo_t *info,
				      void *context)
{
	ucontext_t *ucontext = context;
	uintptr_t observed_pc;

	if (!ucontext) {
		exact_signal_error = 1;
		return;
	}
	observed_pc = (uintptr_t)ucontext->uc_mcontext.pc;
	if (signal_number != exact_expected_signal)
		exact_signal_error = 2;
	else if (!info)
		exact_signal_error = 3;
	else if (info->si_code != exact_expected_code)
		exact_signal_error = 4;
	else if ((uintptr_t)info->si_addr != exact_expected_pc)
		exact_signal_error = 5;
	else if (observed_pc != exact_expected_pc)
		exact_signal_error = 6;
	exact_signal_seen++;
	ucontext->uc_mcontext.pc = observed_pc + sizeof(uint32_t);
}

static bool install_exact_instruction_handler(int signal_number)
{
	struct sigaction action = {};

	action.sa_sigaction = exact_instruction_handler;
	action.sa_flags = SA_SIGINFO;
	if (sigemptyset(&action.sa_mask))
		return false;
	return sigaction(signal_number, &action, NULL) == 0;
}

static void execute_exact_instruction(enum undefined_instruction instruction)
{
	switch (instruction) {
	case UNDEFINED_BRK:
		exact_expected_pc = (uintptr_t)&&brk_instruction;
brk_instruction:
		__asm__ volatile("brk #1");
		return;
	case UNDEFINED_HLT:
		exact_expected_pc = (uintptr_t)&&hlt_instruction;
hlt_instruction:
		__asm__ volatile("hlt #0");
		return;
	case UNDEFINED_UDF:
		exact_expected_pc = (uintptr_t)&&udf_instruction;
udf_instruction:
		__asm__ volatile(".inst 0x00001234");
		return;
	case UNDEFINED_HVC:
		exact_expected_pc = (uintptr_t)&&hvc_instruction;
hvc_instruction:
		__asm__ volatile(".inst 0xd4024682");
		return;
	case UNDEFINED_SMC:
		exact_expected_pc = (uintptr_t)&&smc_instruction;
smc_instruction:
		__asm__ volatile(".inst 0xd4024683");
		return;
	case UNDEFINED_DCPS1:
		exact_expected_pc = (uintptr_t)&&dcps1_instruction;
dcps1_instruction:
		__asm__ volatile(".inst 0xd4a24681");
		return;
	case UNDEFINED_DCPS2:
		exact_expected_pc = (uintptr_t)&&dcps2_instruction;
dcps2_instruction:
		__asm__ volatile(".inst 0xd4a24682");
		return;
	case UNDEFINED_DCPS3:
		exact_expected_pc = (uintptr_t)&&dcps3_instruction;
dcps3_instruction:
		__asm__ volatile(".inst 0xd4a24683");
		return;
	}
}

static bool observes_exact_catchable_instruction_signal(
	enum undefined_instruction instruction, int signal_number, int signal_code)
{
	pid_t child;
	int status = 0;

	child = fork();
	if (child == 0) {
		exact_signal_seen = 0;
		exact_signal_error = 0;
		exact_expected_signal = signal_number;
		exact_expected_code = signal_code;
		if (!install_exact_instruction_handler(signal_number))
			_exit(10);
		execute_exact_instruction(instruction);
		if (exact_signal_seen != 1 || exact_signal_error)
			_exit(11 + exact_signal_error);
		_exit(0);
	}
	if (child < 0)
		return false;
	if (waitpid(child, &status, 0) != child)
		return false;
	return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

int main(void)
{
	static const char message[] = "ORLIX-SIGNAL-WAIT-PROBE\n";

	(void)write(STDOUT_FILENO, message, sizeof(message) - 1);
	orlix_test_plan(12);

	orlix_test_result(signal_handler_runs(),
			  "signal handler runs for delivered signal");
	orlix_test_result(blocked_signal_is_pending(),
			  "blocked signal remains pending");
	orlix_test_result(unblocked_pending_signal_runs(),
			  "unblocked pending signal runs handler");
	orlix_test_result(waitpid_observes_signal_termination(),
			  "waitpid observes signal termination status");
	orlix_test_result(observes_exact_catchable_instruction_signal(
			  UNDEFINED_BRK, SIGTRAP, TRAP_BRKPT),
			  "AArch64 BRK delivers resumable TRAP_BRKPT with exact PC");
	orlix_test_result(observes_exact_catchable_instruction_signal(
			  UNDEFINED_HLT, SIGILL, ILL_ILLOPC),
			  "AArch64 HLT delivers resumable ILL_ILLOPC with exact PC");
	orlix_test_result(observes_exact_catchable_instruction_signal(
			  UNDEFINED_UDF, SIGILL, ILL_ILLOPC),
			  "AArch64 UDF delivers resumable ILL_ILLOPC with exact PC");
	orlix_test_result(observes_exact_catchable_instruction_signal(
			  UNDEFINED_HVC, SIGILL, ILL_ILLOPC),
			  "AArch64 HVC at EL0 delivers resumable ILL_ILLOPC");
	orlix_test_result(observes_exact_catchable_instruction_signal(
			  UNDEFINED_SMC, SIGILL, ILL_ILLOPC),
			  "AArch64 SMC at EL0 delivers resumable ILL_ILLOPC");
	orlix_test_result(observes_exact_catchable_instruction_signal(
			  UNDEFINED_DCPS1, SIGILL, ILL_ILLOPC),
			  "AArch64 DCPS1 outside Debug delivers resumable ILL_ILLOPC");
	orlix_test_result(observes_exact_catchable_instruction_signal(
			  UNDEFINED_DCPS2, SIGILL, ILL_ILLOPC),
			  "AArch64 DCPS2 outside Debug delivers resumable ILL_ILLOPC");
	orlix_test_result(observes_exact_catchable_instruction_signal(
			  UNDEFINED_DCPS3, SIGILL, ILL_ILLOPC),
			  "AArch64 DCPS3 outside Debug delivers resumable ILL_ILLOPC");

	orlix_test_exit();
}
