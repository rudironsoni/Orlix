// SPDX-License-Identifier: GPL-2.0

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

#define FUTEX_WAIT 0
#define FUTEX_WAKE 1

static long orlix_raw_syscall1(long nr, long arg0)
{
	register long x0 __asm__("x0") = arg0;
	register long x8 __asm__("x8") = nr;

	__asm__ volatile("svc #0"
			 : "+r"(x0)
			 : "r"(x8)
			 : "memory", "cc");
	return x0;
}

static long orlix_raw_syscall4(long nr, long arg0, long arg1, long arg2,
			       long arg3)
{
	register long x0 __asm__("x0") = arg0;
	register long x1 __asm__("x1") = arg1;
	register long x2 __asm__("x2") = arg2;
	register long x3 __asm__("x3") = arg3;
	register long x8 __asm__("x8") = nr;

	__asm__ volatile("svc #0"
			 : "+r"(x0)
			 : "r"(x1), "r"(x2), "r"(x3), "r"(x8)
			 : "memory", "cc");
	return x0;
}

static int futex_wait(volatile int *uaddr, int expected,
		      const struct timespec *timeout)
{
	return (int)orlix_raw_syscall4(SYS_futex, (long)uaddr, FUTEX_WAIT,
				       expected, (long)timeout);
}

static int futex_wake(volatile int *uaddr, int count)
{
	return (int)orlix_raw_syscall4(SYS_futex, (long)uaddr, FUTEX_WAKE,
				       count, 0);
}

static void stage(const char *name)
{
	orlix_write_all("# futex ");
	orlix_write_all(name);
	orlix_write_all("\n");
}

static bool wait_returns_eagain_when_value_changed(void)
{
	volatile int word = 1;
	int ret;

	stage("eagain enter");
	ret = futex_wait(&word, 0, NULL);
	stage("eagain returned");
	return ret == -EAGAIN;
}

static bool wait_honors_timeout(void)
{
	volatile int word = 0;
	struct timespec timeout = { .tv_sec = 0, .tv_nsec = 50 * 1000 * 1000 };
	int ret;

	stage("timeout enter");
	ret = futex_wait(&word, 0, &timeout);
	stage("timeout returned");
	return ret == -ETIMEDOUT;
}

struct wake_state {
	volatile int word;
	volatile int child_entered;
	volatile int child_woke;
};

static bool parent_wake_unblocks_child(void)
{
	struct wake_state *state;
	struct timespec pause = { .tv_sec = 0, .tv_nsec = 20 * 1000 * 1000 };
	pid_t child;
	int i;
	int status;

	state = mmap(NULL, sizeof(*state), PROT_READ | PROT_WRITE,
		     MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (state == MAP_FAILED)
		return false;
	state->word = 0;
	state->child_entered = 0;
	state->child_woke = 0;

	stage("fork child");
	child = fork();
	if (child < 0)
		return false;
	if (child == 0) {
		__atomic_store_n(&state->child_entered, 1, __ATOMIC_RELEASE);
		if (futex_wait(&state->word, 0, NULL) == 0)
			__atomic_store_n(&state->child_woke, 1,
					 __ATOMIC_RELEASE);
		orlix_raw_syscall1(SYS_exit, 0);
	}

	for (i = 0; i < 1000 &&
	     !__atomic_load_n(&state->child_entered, __ATOMIC_ACQUIRE); i++)
		(void)orlix_raw_syscall1(SYS_sched_yield, 0);

	(void)orlix_raw_syscall4(SYS_nanosleep, (long)&pause, 0, 0, 0);
	stage("wake child");
	__atomic_store_n(&state->word, 1, __ATOMIC_RELEASE);
	if (futex_wake(&state->word, 1) < 0)
		return false;

	for (i = 0; i < 50 &&
	     !__atomic_load_n(&state->child_woke, __ATOMIC_ACQUIRE); i++)
		(void)orlix_raw_syscall4(SYS_nanosleep, (long)&pause, 0, 0, 0);

	stage("wake waitpid");
	(void)waitpid(child, &status, 0);
	return __atomic_load_n(&state->child_woke, __ATOMIC_ACQUIRE) == 1;
}

int main(void)
{
	orlix_test_plan(3);
	orlix_test_result(wait_returns_eagain_when_value_changed(),
			  "futex wait returns EAGAIN when the word already changed");
	orlix_test_result(wait_honors_timeout(),
			  "futex wait returns ETIMEDOUT");
	orlix_test_result(parent_wake_unblocks_child(),
			  "futex wake unblocks a waiting child");
	orlix_test_exit();
}
