// SPDX-License-Identifier: GPL-2.0
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <ucontext.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

#ifndef TRAP_BRKPT
#define TRAP_BRKPT 1 /* Linux UAPI asm-generic/siginfo.h */
#endif

static volatile sig_atomic_t signal_seen;
static volatile sig_atomic_t signal_error;
static int expected_signal;
static int expected_code;
static uintptr_t expected_pc;

static long svc_getpid(void)
{
	register long x0 __asm__("x0") = 0;
	register long x8 __asm__("x8") = __NR_getpid;

	__asm__ volatile("svc #0" : "+r"(x0) : "r"(x8) : "memory");
	return x0;
}

static void exception_handler(int signal_number, siginfo_t *info, void *context)
{
	ucontext_t *ucontext = context;
	uintptr_t observed_pc;

	if (!ucontext) {
		signal_error = 1;
		return;
	}
	observed_pc = (uintptr_t)ucontext->uc_mcontext.pc;
	if (signal_number != expected_signal)
		signal_error = 2;
	else if (!info)
		signal_error = 3;
	else if (info->si_code != expected_code)
		signal_error = 4;
	else if ((uintptr_t)info->si_addr != expected_pc ||
		 observed_pc != expected_pc)
		signal_error = 5;
	signal_seen++;
	ucontext->uc_mcontext.pc = observed_pc + sizeof(uint32_t);
}

static bool install_exception_handler(int signal_number)
{
	struct sigaction action = {};

	action.sa_sigaction = exception_handler;
	action.sa_flags = SA_SIGINFO;
	if (sigemptyset(&action.sa_mask))
		return false;
	return sigaction(signal_number, &action, NULL) == 0;
}

static bool exact_exception_signal(bool breakpoint)
{
	int signal_number = breakpoint ? SIGTRAP : SIGILL;
	int signal_code = breakpoint ? TRAP_BRKPT : ILL_ILLOPC;

	signal_seen = 0;
	signal_error = 0;
	expected_signal = signal_number;
	expected_code = signal_code;
	if (!install_exception_handler(signal_number))
		return false;
	if (breakpoint) {
		expected_pc = (uintptr_t)&&brk_instruction;
brk_instruction:
		__asm__ volatile("brk #1");
	} else {
		expected_pc = (uintptr_t)&&hlt_instruction;
hlt_instruction:
		__asm__ volatile("hlt #0");
	}
	return signal_seen == 1 && !signal_error;
}

int main(void)
{
	orlix_test_plan(3);
	orlix_test_result(svc_getpid() == getpid(),
			  "AArch64 SVC hands getpid to the Linux syscall dispatcher");
	orlix_test_result(exact_exception_signal(true),
			  "AArch64 BRK delivers Linux SIGTRAP TRAP_BRKPT at its PC");
	orlix_test_result(exact_exception_signal(false),
			  "AArch64 HLT delivers Linux SIGILL ILL_ILLOPC at its PC");
	orlix_test_exit();
}
