// SPDX-License-Identifier: MIT

#include <signal.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

static volatile sig_atomic_t usr1_count;
static volatile sig_atomic_t usr2_count;

static void print_observation(const char *name, const char *value)
{
	printf("{\"observation\":\"%s\",\"value\":\"%s\"}\n", name, value);
	fflush(stdout);
}

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

static int install_handler(int signum, void (*handler)(int))
{
	struct sigaction action;

	action.sa_handler = handler;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	return sigaction(signum, &action, NULL) == 0;
}

static int signal_handler_runs(void)
{
	usr1_count = 0;
	if (!install_handler(SIGUSR1, usr1_handler))
		return 0;
	if (kill(getpid(), SIGUSR1) != 0)
		return 0;
	return usr1_count == 1;
}

static int blocked_signal_is_pending(void)
{
	sigset_t blocked;
	sigset_t pending;

	usr2_count = 0;
	if (!install_handler(SIGUSR2, usr2_handler))
		return 0;
	sigemptyset(&blocked);
	sigaddset(&blocked, SIGUSR2);
	if (sigprocmask(SIG_BLOCK, &blocked, NULL) != 0)
		return 0;
	if (kill(getpid(), SIGUSR2) != 0)
		return 0;
	if (usr2_count != 0)
		return 0;
	if (sigpending(&pending) != 0)
		return 0;
	return sigismember(&pending, SIGUSR2) == 1;
}

static int unblocked_pending_signal_runs(void)
{
	sigset_t unblocked;

	sigemptyset(&unblocked);
	sigaddset(&unblocked, SIGUSR2);
	if (sigprocmask(SIG_UNBLOCK, &unblocked, NULL) != 0)
		return 0;
	return usr2_count == 1;
}

static int waitpid_observes_signal_termination(void)
{
	pid_t child;
	int status = 0;

	child = fork();
	if (child == 0) {
		for (;;)
			pause();
	}
	if (child < 0)
		return 0;
	if (kill(child, SIGTERM) != 0)
		return 0;
	if (waitpid(child, &status, 0) != child)
		return 0;
	return WIFSIGNALED(status) && WTERMSIG(status) == SIGTERM;
}

static int observe(const char *name, int ok)
{
	print_observation(name, ok ? "ok" : "fail");
	return ok;
}

int main(void)
{
	int passed = 1;

	passed &= observe("signal-handler-runs", signal_handler_runs());
	passed &= observe("blocked-signal-pending", blocked_signal_is_pending());
	passed &= observe("unblocked-pending-handler",
			  unblocked_pending_signal_runs());
	passed &= observe("waitpid-signal-termination",
			  waitpid_observes_signal_termination());

	return passed ? 0 : 1;
}
