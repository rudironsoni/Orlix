//
// TaskGroupTests.m
// IXLandSystemTests
//
// INTERNAL RUNTIME SEMANTIC TEST
// NOT public wrapper compatibility proof
//
// This file intentionally tests internal IXLandSystem owner semantics through
// internal entry points (getpgrp_impl, setpgid_impl, etc.).
//
// Public drop-in compatibility is deferred to IXLandMLibC/sysroot integration.
// These tests verify the runtime semantics, not the public ABI.
//

#import <XCTest/XCTest.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Internal headers - these are the OWNER entry points we test */
#include "kernel/task.h"
#include "kernel/signal.h"

/* Declare library init function */
extern int library_init(const void *config);
extern int library_is_initialized(void);

@interface TaskGroupTests : XCTestCase
@end

@implementation TaskGroupTests

- (void)setUp {
    [super setUp];
    if (!library_is_initialized()) {
        library_init(NULL);
    }
}

#pragma mark - A. Initial Task PGID/SID Coherence

- (void)testInitialTaskPgidAndSidCoherence {
    struct task_struct *task = get_current();
    XCTAssertNotNull(task, @"Should have a current task");
    
    XCTAssertEqual(task->pgid, task->pid, @"Initial task should be its own process group leader");
    XCTAssertEqual(task->sid, task->pid, @"Initial task should be its own session leader");
}

#pragma mark - B. getpgrp_impl Current Group Behavior

- (void)testGetpgrpImplReturnsCurrentTaskPgid {
    struct task_struct *task = get_current();
    XCTAssertNotNull(task, @"Should have a current task");
    
    int32_t pgid = getpgrp_impl();
    
    XCTAssertGreaterThan(pgid, 0, @"Process group ID should be positive");
    XCTAssertEqual(pgid, task->pgid, @"getpgrp_impl should return task pgid");
}

#pragma mark - C. getpgid_impl Lookup Behavior

- (void)testGetpgidImplReturnsTargetPgid {
    struct task_struct *task = get_current();
    XCTAssertNotNull(task, @"Should have a current task");
    
    int32_t pgid = getpgid_impl(task->pid);
    
    XCTAssertGreaterThan(pgid, 0, @"Process group ID should be positive");
    XCTAssertEqual(pgid, task->pgid, @"getpgid_impl should return task pgid");
}

- (void)testGetpgidImplZeroReturnsCurrentPgid {
    struct task_struct *task = get_current();
    XCTAssertNotNull(task, @"Should have a current task");
    
    int32_t pgid_zero = getpgid_impl(0);
    int32_t pgid_explicit = getpgid_impl(task->pid);
    
    XCTAssertEqual(pgid_zero, pgid_explicit, @"getpgid_impl(0) should equal getpgid_impl(pid)");
}

- (void)testGetpgidImplRejectsInvalidPid {
    errno = 0;
    int32_t pgid = getpgid_impl(-9999);
    
    XCTAssertEqual(pgid, -1, @"Should return -1 for invalid pid");
    XCTAssertEqual(errno, ESRCH, @"errno should be ESRCH");
}

#pragma mark - D. setpgid_impl Validation

- (void)testSetpgidImplRejectsNegativePgid {
    struct task_struct *task = get_current();
    XCTAssertNotNull(task, @"Should have a current task");
    
    errno = 0;
    int result = setpgid_impl(task->pid, -5);
    
    XCTAssertEqual(result, -1, @"Should fail with negative pgid");
    XCTAssertEqual(errno, EINVAL, @"errno should be EINVAL");
}

- (void)testSetpgidImplRejectsInvalidPid {
    errno = 0;
    int result = setpgid_impl(-9999, 0);
    
    XCTAssertEqual(result, -1, @"Should fail");
    XCTAssertEqual(errno, ESRCH, @"errno should be ESRCH");
}

- (void)testSetpgidImplRejectsSessionLeader {
    struct task_struct *task = get_current();
    XCTAssertNotNull(task, @"Should have a current task");
    
    // If task is session leader, setpgid to different group should fail
    if (task->sid == task->pid) {
        errno = 0;
        int result = setpgid_impl(task->pid, task->pid + 1);
        
        // Session leader cannot join another process group
        XCTAssertEqual(result, -1, @"Session leader should not be able to change pgid");
        XCTAssertEqual(errno, EPERM, @"errno should be EPERM");
    } else {
        XCTSkip(@"Task is not session leader");
    }
}

- (void)testSetpgidImplCreatesNewGroupWithZero {
    // Create a child task to test with (we can't change init task's group if it's session leader)
    // For now, just verify that setpgid_impl with pgid=0 is accepted if valid
    struct task_struct *task = get_current();
    XCTAssertNotNull(task, @"Should have a current task");
    
    // If we set pgid to current pid (which means later setpgid(0) would use new group)
    // This test validates the internal logic accepts valid transitions
    errno = 0;
    int result = setpgid_impl(task->pid, task->pid);
    
    // May succeed (already in that group) or fail (if session leader constraints)
    if (result == -1) {
        XCTAssertTrue(errno == EPERM, @"Expected EPERM if session leader");
    } else {
        int32_t new_pgid = getpgid_impl(task->pid);
        XCTAssertEqual(new_pgid, task->pid, @"pgid should be task pid");
    }
}

#pragma mark - E. setsid_impl Session Management

- (void)testSetsidImplRejectsProcessGroupLeader {
    struct task_struct *task = get_current();
    XCTAssertNotNull(task, @"Should have a current task");
    
    // If task is process group leader, setsid should fail with EPERM
    if (task->pgid == task->pid) {
        errno = 0;
        int32_t result = setsid_impl();
        
        XCTAssertEqual(result, -1, @"setsid should fail for process group leader");
        XCTAssertEqual(errno, EPERM, @"errno should be EPERM");
    } else {
        XCTSkip(@"Task is not process group leader");
    }
}

- (void)testSetsidImplSucceedsForNonLeader {
    // This would require creating a non-leader task
    // Currently we test that the implementation is callable
    // Real test requires fork/clone infrastructure
    XCTSkip(@"Requires fork/clone to create non-leader task");
}

#pragma mark - F. getsid_impl Session Lookup

- (void)testGetsidImplReturnsCurrentSession {
    struct task_struct *task = get_current();
    XCTAssertNotNull(task, @"Should have a current task");
    
    int32_t sid = getsid_impl(task->pid);
    
    XCTAssertGreaterThan(sid, 0, @"Session ID should be positive");
    XCTAssertEqual(sid, task->sid, @"getsid_impl should return task sid");
}

- (void)testGetsidImplZeroReturnsCurrentSession {
    struct task_struct *task = get_current();
    XCTAssertNotNull(task, @"Should have a current task");
    
    int32_t sid_zero = getsid_impl(0);
    int32_t sid_explicit = getsid_impl(task->pid);
    
    XCTAssertEqual(sid_zero, sid_explicit, @"getsid_impl(0) should equal getsid_impl(pid)");
}

- (void)testGetsidImplRejectsInvalidPid {
    errno = 0;
    int32_t sid = getsid_impl(-9999);
    
    XCTAssertEqual(sid, -1, @"Should return -1");
    XCTAssertEqual(errno, ESRCH, @"errno should be ESRCH");
}

#pragma mark - G. do_killpg Signal Delivery

- (void)testDoKillpgSignalDelivery {
    struct task_struct *task = get_current();
    XCTAssertNotNull(task, @"Should have a current task");
    XCTAssertNotNull(task->signal, @"Task should have signal state");
    
    // Block SIGUSR1 first using do_sigprocmask
    struct signal_mask_bits mask = {0};
    struct signal_mask_bits oldmask = {0};
    mask.sig[SIGUSR1 >> 6] |= (1ULL << (SIGUSR1 & 63));
    
    errno = 0;
    int result = do_sigprocmask(SIG_BLOCK, &mask, &oldmask);
    XCTAssertEqual(result, 0, @"sigprocmask should succeed");
    
    // Clear pending first
    memset(&task->signal->pending, 0, sizeof(task->signal->pending));
    
    // Send signal to our process group using do_killpg
    int32_t pgid = task->pgid ? task->pgid : task->pid;
    errno = 0;
    result = do_killpg(pgid, SIGUSR1);
    
    XCTAssertEqual(result, 0, @"do_killpg should succeed");
    
    // Check if signal is pending
    struct signal_mask_bits pending = {0};
    result = do_sigpending(&pending);
    XCTAssertEqual(result, 0, @"do_sigpending should succeed");
    
    bool is_pending = (pending.sig[SIGUSR1 >> 6] & (1ULL << (SIGUSR1 & 63))) != 0;
    XCTAssertTrue(is_pending, @"SIGUSR1 should be pending");
    
    // Restore mask
    do_sigprocmask(SIG_SETMASK, &oldmask, NULL);
}

- (void)testDoKillpgRejectsInvalidPgid {
    errno = 0;
    int result = do_killpg(-9999, SIGUSR1);
    
    XCTAssertEqual(result, -1, @"Should fail");
    XCTAssertEqual(errno, ESRCH, @"errno should be ESRCH");
}

#pragma mark - H. waitpid_impl Child State

- (void)testWaitpidImplNoChildReturnsEchild {
    struct task_struct *task = get_current();
    XCTAssertNotNull(task, @"Should have a current task");
    
    int status = 0;
    errno = 0;
    int32_t result = waitpid_impl(-1, &status, WNOHANG);
    
    // Should return 0 if no children (non-blocking)
    // But if no task has a child relationship in our virtual table, it might return ECHILD
    NSLog(@"waitpid_impl returned %d, errno=%d", result, errno);
    
    // The call should complete without crashing; exact behavior depends on task state
    XCTAssertTrue(result == 0 || result == -1, @"Should return 0 or -1");
    if (result == -1) {
        XCTAssertEqual(errno, ECHILD, @"Should return ECHILD if no children");
    }
}

#pragma mark - I. Signal 64 Internal Handling

- (void)testSignal64InternalHandling {
    struct task_struct *task = get_current();
    XCTAssertNotNull(task, @"Should have a current task");
    XCTAssertNotNull(task->signal, @"Task should have signal state");
    
    // Signal 64 is the last valid signal number in Linux
    // This test catches the bridge indexing bug (signal >> 5 vs signal - 1)
    
    // Block signal 64
    struct signal_mask_bits mask = {0};
    struct signal_mask_bits oldmask = {0};
    mask.sig[(64 - 1) >> 6] |= (1ULL << ((64 - 1) & 63));
    
    errno = 0;
    int result = do_sigprocmask(SIG_BLOCK, &mask, &oldmask);
    XCTAssertEqual(result, 0, @"sigprocmask should succeed");
    
    // Clear pending
    memset(&task->signal->pending, 0, sizeof(task->signal->pending));
    
    // Raise signal 64 using do_raise
    errno = 0;
    result = do_raise(64);
    
    XCTAssertEqual(result, 0, @"do_raise(64) should succeed");
    
    // Check pending
    struct signal_mask_bits pending = {0};
    result = do_sigpending(&pending);
    XCTAssertEqual(result, 0, @"do_sigpending should succeed");
    
    bool is_pending = (pending.sig[(64 - 1) >> 6] & (1ULL << ((64 - 1) & 63))) != 0;
    XCTAssertTrue(is_pending, @"Signal 64 should be pending");
    
    // Restore mask
    do_sigprocmask(SIG_SETMASK, &oldmask, NULL);
}

@end
