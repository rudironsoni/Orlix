// SPDX-License-Identifier: GPL-2.0

#include <stdint.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

#define TLS_COOKIE UINT64_C(0x4f524c4958544c53)

static __thread volatile uintptr_t tls_cookie = TLS_COOKIE;

static uintptr_t read_tls_register(void)
{
	uintptr_t value;

	__asm__ volatile("mrs %0, tpidr_el0" : "=r"(value));
	return value;
}

int main(void)
{
	uintptr_t initial_tls;
	uintptr_t after_gettid_tls;
	uintptr_t after_getpid_tls;
	long tid;
	pid_t pid;

	orlix_test_plan(4);

	initial_tls = read_tls_register();
	tid = syscall(SYS_gettid);
	after_gettid_tls = read_tls_register();
	pid = getpid();
	after_getpid_tls = read_tls_register();

	orlix_test_result(initial_tls != 0 && tls_cookie == TLS_COOKIE,
			  "mlibc starts with usable userspace TLS");
	orlix_test_result(tid > 0, "gettid syscall returns a task id");
	orlix_test_result(after_gettid_tls == initial_tls,
			  "raw syscall preserves userspace TLS");
	orlix_test_result(pid > 0 && after_getpid_tls == initial_tls,
			  "libc syscall wrapper preserves userspace TLS");

	orlix_test_exit();
}
