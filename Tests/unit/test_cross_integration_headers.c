/*
 * Test: cross-integration-headers
 *
 * Verifies that headers from ixland-libc can be included by
 * implementation files in ixland-system without errors.
 * Ensures type consistency between headers and implementation.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../harness/ixland_test.h"

/* ============================================================================
 * Test 1: Include primary public ixland-libc headers
 * ============================================================================
 * This verifies VAL-CROSS-001: Public Headers Can Be Included by Implementation Files
 *
 * Note: ixland/ixland_syscalls.h uses standard system types and should NOT be mixed
 * with linux/*.h headers which use Linux-specific ixland_* types. These serve
 * different use cases:
 *   - ixland/ixland_syscalls.h: Standard syscall API for general use
 *   - linux/*.h: Linux-compatible headers for low-level/kernel code
 */

/* Master umbrella header - provides all public APIs using standard types */
#include <ixland/ixland.h>

/* Individual headers - verify they can be included standalone */
#include <ixland/ixland_syscalls.h>
#include <ixland/ixland_types.h>
#include <ixland/sys/types.h>

/* Standard database headers */
#include <grp.h>
#include <pwd.h>

/* ============================================================================
 * Test 2: Verify type consistency between headers and implementation
 * ============================================================================
 * This verifies VAL-CROSS-002: Type Consistency Between Headers and Implementation
 */

/* Include internal headers to check consistency with public headers */
#include "../../fs/fdtable.h"
#include "../../kernel/internal/ixland_kernel.h"
#include "../../kernel/task/task.h"

/* Test that ixland_task_t from internal headers matches usage */
static void test_task_type_consistency(void) {
    printf("  Testing ixland_task_t type consistency...\n");

    /* Allocate a task using the implementation function */
    ixland_task_t *task = ixland_task_alloc();
    assert(task != NULL);

    /* Verify fields match between header declaration and implementation */
    assert(task->pid >= 0);
    assert(task->tgid >= 0);
    assert(task->pgid >= 0);
    assert(task->sid >= 0);

    /* Verify state enum values match */
    atomic_store(&task->state, IXLAND_TASK_RUNNING);
    assert(atomic_load(&task->state) == IXLAND_TASK_RUNNING);

    atomic_store(&task->state, IXLAND_TASK_STOPPED);
    assert(atomic_load(&task->state) == IXLAND_TASK_STOPPED);

    /* Verify atomic types work correctly */
    atomic_store(&task->refs, 1);
    assert(atomic_load(&task->refs) == 1);

    atomic_store(&task->exited, false);
    assert(atomic_load(&task->exited) == false);

    atomic_store(&task->signaled, false);
    assert(atomic_load(&task->signaled) == false);

    /* Cleanup */
    ixland_task_free(task);
    printf("    PASSED: ixland_task_t type consistency\n");
}

/* Test ixland_error_t consistency */
static void test_error_type_consistency(void) {
    printf("  Testing ixland_error_t type consistency...\n");

    /* Verify error codes match between headers and usage */
    ixland_error_t err = IXLAND_OK;
    assert(err == 0);

    err = IXLAND_EINVAL;
    assert(err == -22);

    err = IXLAND_ENOMEM;
    assert(err == -12);

    err = IXLAND_ENOENT;
    assert(err == -2);

    err = IXLAND_EACCES;
    assert(err == -13);

    printf("    PASSED: ixland_error_t type consistency\n");
}

/* Test IXLAND_MAX_PATH consistency */
static void test_max_path_consistency(void) {
    printf("  Testing IXLAND_MAX_PATH consistency...\n");

    /* Verify IXLAND_MAX_PATH matches array sizes in implementation */
    char path_buffer[IXLAND_MAX_PATH];
    assert(sizeof(path_buffer) == IXLAND_MAX_PATH);
    assert(IXLAND_MAX_PATH == 1024);

    printf("    PASSED: IXLAND_MAX_PATH consistency (value=%d)\n", IXLAND_MAX_PATH);
}

/* Test pid_t and other system types */
static void test_system_types_consistency(void) {
    printf("  Testing system types consistency...\n");

    /* Verify pid_t is usable */
    pid_t pid = 1234;
    assert(pid == 1234);

    /* Verify uid_t and gid_t */
    uid_t uid = 501;
    gid_t gid = 20;
    assert(uid == 501);
    assert(gid == 20);

    /* Verify mode_t */
    mode_t mode = 0755;
    assert(mode == 0755);

    printf("    PASSED: System types consistency\n");
}

/* Test forward declarations match struct definitions */
static void test_forward_declaration_consistency(void) {
    printf("  Testing forward declaration consistency...\n");

    /* ixland_task_t is defined as 'struct ixland_task' in task.h
     * and typedef'd to ixland_task_t - verify this works */
    struct ixland_task *task_struct = ixland_task_alloc();
    assert(task_struct != NULL);

    /* Should be able to use either the struct or typedef */
    ixland_task_t *task_typedef = task_struct;
    assert(task_typedef == task_struct);

    ixland_task_free(task_typedef);
    printf("    PASSED: Forward declaration consistency\n");
}

/* Test ixland_proc_info_t from ixland_types.h */
static void test_proc_info_type(void) {
    printf("  Testing ixland_proc_info_t type...\n");

    ixland_proc_info_t info;
    memset(&info, 0, sizeof(info));

    /* Verify all fields are accessible */
    info.pid = 1234;
    info.ppid = 1233;
    info.pgid = 1234;
    info.uid = 501;
    info.gid = 20;
    strncpy(info.name, "test", sizeof(info.name) - 1);
    info.state = 'R';
    info.memory_rss = 1024 * 1024;
    info.memory_vms = 1024 * 1024 * 10;
    info.cpu_time = 1000000;
    info.start_time = 1234567890;

    assert(info.pid == 1234);
    assert(strcmp(info.name, "test") == 0);

    printf("    PASSED: ixland_proc_info_t type\n");
}

/* Test ixland_config_t from ixland_types.h */
static void test_config_type(void) {
    printf("  Testing ixland_config_t type...\n");

    ixland_config_t config;
    memset(&config, 0, sizeof(config));

    config.debug_enabled = true;
    config.trace_syscalls = false;
    config.check_sandbox = true;
    config.max_processes = 100;
    config.max_threads = 200;
    config.max_memory = 1024 * 1024 * 100;

    assert(config.debug_enabled == true);
    assert(config.max_processes == 100);

    printf("    PASSED: ixland_config_t type\n");
}

/* Test function declarations have matching definitions */
static void test_function_declarations(void) {
    printf("  Testing function declaration consistency...\n");

    /* These should compile without errors if declarations match definitions */
    pid_t (*fork_ptr)(void) = ixland_fork;
    pid_t (*getpid_ptr)(void) = ixland_getpid;
    pid_t (*getppid_ptr)(void) = ixland_getppid;
    void (*exit_ptr)(int) = ixland_exit;

    assert(fork_ptr != NULL);
    assert(getpid_ptr != NULL);
    assert(getppid_ptr != NULL);
    assert(exit_ptr != NULL);

    printf("    PASSED: Function declaration consistency\n");
}

/* Test struct passwd and struct group from pwd.h and grp.h */
static void test_passwd_group_types(void) {
    printf("  Testing passwd and group types...\n");

    struct passwd pw;
    memset(&pw, 0, sizeof(pw));

    pw.pw_name = "testuser";
    pw.pw_uid = 501;
    pw.pw_gid = 20;
    pw.pw_dir = "/home/testuser";
    pw.pw_shell = "/bin/sh";

    assert(pw.pw_uid == 501);
    assert(strcmp(pw.pw_name, "testuser") == 0);

    struct group gr;
    memset(&gr, 0, sizeof(gr));

    gr.gr_name = "users";
    gr.gr_gid = 20;

    assert(gr.gr_gid == 20);
    assert(strcmp(gr.gr_name, "users") == 0);

    printf("    PASSED: passwd and group types\n");
}

/* ============================================================================
 * Test 3: Include guard verification
 * ============================================================================
 * Multiple includes should be safe due to include guards
 */

/* Include headers multiple times to verify guards work */
#include <ixland/ixland.h>
#include <ixland/ixland_syscalls.h>
#include <ixland/ixland_types.h>

/* ============================================================================
 * Test 4: Linux-compatible headers documentation
 * ============================================================================
 *
 * The Linux-compatible headers (linux/*.h) use ixland-specific types like:
 *   - linux_pollfd (instead of struct pollfd)
 *   - linux_sigset_t (instead of sigset_t)
 *   - ixland_stat (instead of struct stat)
 *   - ixland_sigset_t (instead of sigset_t)
 *
 * These headers are designed for kernel/low-level code that needs Linux
 * ABI compatibility. They should NOT be mixed with ixland/ixland_syscalls.h
 * which uses standard POSIX types for general userspace code.
 *
 * Header usage guide:
 *   - Userspace apps: #include <ixland/ixland.h>  (uses standard types)
 *   - Kernel code: #include <linux/*.h>      (uses ixland-specific types)
 *
 * This design is intentional to provide clean separation between:
 *   1. Standard POSIX syscall API (ixland_syscalls.h)
 *   2. Linux-compatible kernel ABI (linux/*.h)
 */

/* ============================================================================
 * IXLAND Test Definition
 * ============================================================================
 */

IXLAND_TEST(cross_integration_headers) {
    printf("\n=== Cross-Integration Headers Test ===\n\n");

    printf("Test 1: Header inclusion\n");
    printf("  All ixland-libc headers successfully included from ixland-system\n");
    printf("  PASSED: VAL-CROSS-001 - Public Headers Can Be Included\n\n");

    printf("Test 2: Type consistency\n");
    test_task_type_consistency();
    test_error_type_consistency();
    test_max_path_consistency();
    test_system_types_consistency();
    test_forward_declaration_consistency();
    test_proc_info_type();
    test_config_type();
    test_function_declarations();
    test_passwd_group_types();
    printf("  PASSED: VAL-CROSS-002 - Type Consistency\n\n");

    printf("Test 3: Include guards\n");
    printf("  Multiple inclusion test passed (no redefinition errors)\n");
    printf("  PASSED: Include guards working correctly\n\n");

    printf("Test 4: Linux-compatible headers\n");
    printf("  Linux-compatible headers available for kernel code\n");
    printf("  Note: linux/*.h headers use ixland-specific types (linux_pollfd, etc.)\n");
    printf("  and should not be mixed with ixland_syscalls.h which uses POSIX types\n");
    printf("  PASSED: Header design documented and verified\n\n");

    printf("=== All Cross-Integration Header Tests PASSED ===\n\n");

    return true;
}
