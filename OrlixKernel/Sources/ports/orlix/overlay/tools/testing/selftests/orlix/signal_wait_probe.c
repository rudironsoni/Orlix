// SPDX-License-Identifier: GPL-2.0

#include <signal.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

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

static bool waitpid_observes_brk_as_sigtrap(void)
{
	pid_t child;
	int status = 0;

	child = fork();
	if (child == 0) {
		__asm__ volatile("brk #1");
		_exit(127);
	}
	if (child < 0)
		return false;
	if (waitpid(child, &status, 0) != child)
		return false;
	return WIFSIGNALED(status) && WTERMSIG(status) == SIGTRAP;
}

static bool waitpid_observes_hlt_as_sigill(void)
{
	pid_t child;
	int status = 0;

	child = fork();
	if (child == 0) {
		__asm__ volatile("hlt #0");
		_exit(127);
	}
	if (child < 0)
		return false;
	if (waitpid(child, &status, 0) != child)
		return false;

	return WIFSIGNALED(status) && WTERMSIG(status) == SIGILL;
}

enum undefined_instruction {
	UNDEFINED_UDF,
	UNDEFINED_HVC,
	UNDEFINED_SMC,
	UNDEFINED_DCPS1,
	UNDEFINED_DCPS2,
	UNDEFINED_DCPS3,
};

static void execute_undefined_instruction(enum undefined_instruction instruction)
{
	switch (instruction) {
	case UNDEFINED_UDF:
		__asm__ volatile(".inst 0x00001234");
		break;
	case UNDEFINED_HVC:
		__asm__ volatile(".inst 0xd4024682");
		break;
	case UNDEFINED_SMC:
		__asm__ volatile(".inst 0xd4024683");
		break;
	case UNDEFINED_DCPS1:
		__asm__ volatile(".inst 0xd4a24681");
		break;
	case UNDEFINED_DCPS2:
		__asm__ volatile(".inst 0xd4a24682");
		break;
	case UNDEFINED_DCPS3:
		__asm__ volatile(".inst 0xd4a24683");
		break;
	}
}

static bool waitpid_observes_undefined_as_sigill(
	enum undefined_instruction instruction)
{
	pid_t child;
	int status = 0;

	child = fork();
	if (child == 0) {
		execute_undefined_instruction(instruction);
		_exit(127);
	}
	if (child < 0)
		return false;
	if (waitpid(child, &status, 0) != child)
		return false;
	return WIFSIGNALED(status) && WTERMSIG(status) == SIGILL;
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
	orlix_test_result(waitpid_observes_brk_as_sigtrap(),
			  "AArch64 BRK is delivered as SIGTRAP");
	orlix_test_result(waitpid_observes_hlt_as_sigill(),
			  "AArch64 HLT is delivered as SIGILL");
	orlix_test_result(waitpid_observes_undefined_as_sigill(UNDEFINED_UDF),
			  "AArch64 UDF is delivered as SIGILL");
	orlix_test_result(waitpid_observes_undefined_as_sigill(UNDEFINED_HVC),
			  "AArch64 HVC at EL0 is delivered as SIGILL");
	orlix_test_result(waitpid_observes_undefined_as_sigill(UNDEFINED_SMC),
			  "AArch64 SMC at EL0 is delivered as SIGILL");
	orlix_test_result(waitpid_observes_undefined_as_sigill(UNDEFINED_DCPS1),
			  "AArch64 DCPS1 outside Debug state is delivered as SIGILL");
	orlix_test_result(waitpid_observes_undefined_as_sigill(UNDEFINED_DCPS2),
			  "AArch64 DCPS2 outside Debug state is delivered as SIGILL");
	orlix_test_result(waitpid_observes_undefined_as_sigill(UNDEFINED_DCPS3),
			  "AArch64 DCPS3 outside Debug state is delivered as SIGILL");

	orlix_test_exit();
}
