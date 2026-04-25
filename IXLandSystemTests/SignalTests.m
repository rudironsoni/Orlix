//
// SignalTests.m
// IXLandSystemTests
//
// INTERNAL RUNTIME SEMANTIC TEST
// NOT public wrapper compatibility proof
//
// This file intentionally tests internal IXLandSystem signal semantics through
// internal entry points (do_sigprocmask, do_sigaction, do_raise, etc.).
//
// Public drop-in compatibility is deferred to IXLandMLibC/sysroot integration.
// This file intentionally tests internal IXLandSystem signal semantics through
// internal entry points.
//
// Allowed includes:
//   - "kernel/signal.h" (private owner header)
//   - "kernel/task.h" (private owner header)
//   - <errno.h>, <stdint.h>, <stdbool.h>, <string.h> (neutral C headers)
//
// Forbidden includes:
//   - <signal.h>, <unistd.h>, <sys/wait.h> (public POSIX)
//   - <linux/...>, <asm/...>, <asm-generic/...> (Linux UAPI)
//   - path traversal into third_party/linux-uapi
//   - manual extern declarations for public POSIX names
//   - calling public names like sigprocmask(), sigaction(), raise(), kill()
//

#import <XCTest/XCTest.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Internal headers - these are the OWNER entry points we test */
#include "kernel/signal.h"
#include "kernel/task.h"

/* Local test constants for internal semantic invocation.
 * These are NOT public ABI claims - they are local values matching
 * Linux UAPI constants used to exercise internal _impl/do_ entry points.
 * Public drop-in compatibility proof is a separate concern. */
#define TEST_SIG_BLOCK   0
#define TEST_SIG_UNBLOCK 1
#define TEST_SIG_SETMASK 2
#define TEST_SIGUSR1     10
#define TEST_SIGUSR2     12
#define TEST_SIGCHLD     17

/* Declare library init function */
extern int library_init(const void *config);
extern int library_is_initialized(void);

@interface SignalTests : XCTestCase
@end

@implementation SignalTests

- (void)setUp {
    [super setUp];
    if (!library_is_initialized()) {
        library_init(NULL);
    }
    /* Clean up any lingering file descriptors */
    extern int close(int fd);
    for (int fd = 3; fd < 256; fd++) {
        close(fd);
    }
}

- (void)tearDown {
    /* Clean up any open file descriptors */
    extern int close(int fd);
    for (int fd = 3; fd < 256; fd++) {
        close(fd);
    }
    [super tearDown];
}

#pragma mark - A. Library Initialization

- (void)testLibraryInitialization {
    BOOL isInit = library_is_initialized();
    NSLog(@"SignalTests: Library initialized: %@", isInit ? @"YES" : @"NO");
    XCTAssertTrue(isInit, @"Library should be initialized");
}

#pragma mark - B. Signal Mask Internal Operations

- (void)testDoSigprocmaskBasicOperations {
    struct task_struct *task = get_current();
    XCTAssertTrue(task != NULL, @"Should have a current task");
    XCTAssertTrue(task->signal != NULL, @"Task should have signal state");
    
    struct signal_mask_bits mask = {0};
    struct signal_mask_bits oldmask = {0};
    struct signal_mask_bits queried = {0};
    
    // Block SIGUSR1
    mask.sig[(TEST_SIGUSR1 - 1) >> 6] |= (1ULL << ((TEST_SIGUSR1 - 1) & 63));
    
    errno = 0;
    int result = do_sigprocmask(TEST_SIG_BLOCK, &mask, &oldmask);
    XCTAssertEqual(result, 0, @"do_sigprocmask SIG_BLOCK should succeed");
    
    // Query mask
    errno = 0;
    result = do_sigprocmask(TEST_SIG_BLOCK, NULL, &queried);
    XCTAssertEqual(result, 0, @"do_sigprocmask query should succeed");
    
    // SIGUSR1 should be blocked
    bool is_blocked = signal_is_blocked(task, TEST_SIGUSR1);
    XCTAssertTrue(is_blocked, @"SIGUSR1 should be blocked");
    
    // Unblock SIGUSR1
    mask.sig[(TEST_SIGUSR1 - 1) >> 6] |= (1ULL << ((TEST_SIGUSR1 - 1) & 63));
    errno = 0;
    result = do_sigprocmask(TEST_SIG_UNBLOCK, &mask, NULL);
    XCTAssertEqual(result, 0, @"do_sigprocmask SIG_UNBLOCK should succeed");
    
    is_blocked = signal_is_blocked(task, TEST_SIGUSR1);
    XCTAssertFalse(is_blocked, @"SIGUSR1 should be unblocked after SIG_UNBLOCK");
}

- (void)testDoSigprocmaskInvalidHow {
    struct signal_mask_bits mask = {0};
    struct signal_mask_bits oldmask = {0};
    
    errno = 0;
    int result = do_sigprocmask(999, &mask, &oldmask);
    XCTAssertEqual(result, -1, @"do_sigprocmask with invalid 'how' should fail");
    XCTAssertEqual(errno, EINVAL, @"errno should be EINVAL");
}

#pragma mark - C. Signal Generation Internal Operations

- (void)testDoRaiseSignalToSelf {
    struct task_struct *task = get_current();
    XCTAssertTrue(task != NULL, @"Should have a current task");
    XCTAssertTrue(task->signal != NULL, @"Task should have signal state");
    
    // Block SIGUSR1 first so it becomes pending
    struct signal_mask_bits mask = {0};
    struct signal_mask_bits oldmask = {0};
    mask.sig[(TEST_SIGUSR1 - 1) >> 6] |= (1ULL << ((TEST_SIGUSR1 - 1) & 63));
    
    int result = do_sigprocmask(TEST_SIG_BLOCK, &mask, &oldmask);
    XCTAssertEqual(result, 0, @"Block SIGUSR1 should succeed");
    
    // Clear pending first
    memset(&task->signal->pending, 0, sizeof(task->signal->pending));
    
    // Raise SIGUSR1 to self
    errno = 0;
    result = do_raise(TEST_SIGUSR1);
    XCTAssertEqual(result, 0, @"do_raise should succeed");
    
    // Check pending
    struct signal_mask_bits pending = {0};
    result = do_sigpending(&pending);
    XCTAssertEqual(result, 0, @"do_sigpending should succeed");
    
    bool is_pending = (pending.sig[(TEST_SIGUSR1 - 1) >> 6] & (1ULL << ((TEST_SIGUSR1 - 1) & 63))) != 0;
    XCTAssertTrue(is_pending, @"SIGUSR1 should be pending after do_raise while blocked");
    
    // Restore mask
    do_sigprocmask(TEST_SIG_SETMASK, &oldmask, NULL);
}

- (void)testDoKillSignalToCurrentTask {
    struct task_struct *task = get_current();
    XCTAssertTrue(task != NULL, @"Should have a current task");
    XCTAssertTrue(task->signal != NULL, @"Task should have signal state");
    
    // Block SIGUSR1 first
    struct signal_mask_bits mask = {0};
    struct signal_mask_bits oldmask = {0};
    mask.sig[(TEST_SIGUSR1 - 1) >> 6] |= (1ULL << ((TEST_SIGUSR1 - 1) & 63));
    
    int result = do_sigprocmask(TEST_SIG_BLOCK, &mask, &oldmask);
    XCTAssertEqual(result, 0, @"Block SIGUSR1 should succeed");
    
    // Clear pending first
    memset(&task->signal->pending, 0, sizeof(task->signal->pending));
    
    // Send SIGUSR1 to current task via do_kill
    errno = 0;
    result = do_kill(task->pid, TEST_SIGUSR1);
    XCTAssertEqual(result, 0, @"do_kill to current task should succeed");
    
    // Check pending
    struct signal_mask_bits pending = {0};
    result = do_sigpending(&pending);
    XCTAssertEqual(result, 0, @"do_sigpending should succeed");
    
    bool is_pending = (pending.sig[(TEST_SIGUSR1 - 1) >> 6] & (1ULL << ((TEST_SIGUSR1 - 1) & 63))) != 0;
    XCTAssertTrue(is_pending, @"SIGUSR1 should be pending after do_kill while blocked");
    
    // Restore mask
    do_sigprocmask(TEST_SIG_SETMASK, &oldmask, NULL);
}

- (void)testDoKillpgSignalToProcessGroup {
    struct task_struct *task = get_current();
    XCTAssertTrue(task != NULL, @"Should have a current task");
    
    // Send SIGUSR1 to our own process group
    int32_t pgid = task->pgid ? task->pgid : task->pid;
    
    // Block SIGUSR1 first so it becomes pending if sent
    struct signal_mask_bits mask = {0};
    struct signal_mask_bits oldmask = {0};
    mask.sig[(TEST_SIGUSR1 - 1) >> 6] |= (1ULL << ((TEST_SIGUSR1 - 1) & 63));
    
    int result = do_sigprocmask(TEST_SIG_BLOCK, &mask, &oldmask);
    XCTAssertEqual(result, 0, @"Block SIGUSR1 should succeed");
    
    // Clear pending first
    memset(&task->signal->pending, 0, sizeof(task->signal->pending));
    
    // Send SIGUSR1 to our process group
    errno = 0;
    result = do_killpg(pgid, TEST_SIGUSR1);
    XCTAssertEqual(result, 0, @"do_killpg should succeed");
    
    // Check pending
    struct signal_mask_bits pending = {0};
    result = do_sigpending(&pending);
    XCTAssertEqual(result, 0, @"do_sigpending should succeed");
    
    bool is_pending = (pending.sig[(TEST_SIGUSR1 - 1) >> 6] & (1ULL << ((TEST_SIGUSR1 - 1) & 63))) != 0;
    XCTAssertTrue(is_pending, @"SIGUSR1 should be pending after do_killpg while blocked");
    
    // Restore mask
    do_sigprocmask(TEST_SIG_SETMASK, &oldmask, NULL);
}

- (void)testDoKillInvalidPid {
    errno = 0;
    int result = do_kill(-9999, TEST_SIGUSR1);
    XCTAssertEqual(result, -1, @"do_kill with invalid pid should fail");
    XCTAssertEqual(errno, ESRCH, @"errno should be ESRCH");
}

- (void)testDoKillpgInvalidPgid {
    errno = 0;
    int result = do_killpg(-9999, TEST_SIGUSR1);
    XCTAssertEqual(result, -1, @"do_killpg with invalid pgid should fail");
    XCTAssertEqual(errno, ESRCH, @"errno should be ESRCH");
}

#pragma mark - D. Signal Pending Internal Operations

- (void)testDoSigpendingEmptyAfterClear {
    struct task_struct *task = get_current();
    XCTAssertTrue(task != NULL, @"Should have a current task");
    XCTAssertTrue(task->signal != NULL, @"Task should have signal state");
    
    // Clear pending
    memset(&task->signal->pending, 0, sizeof(task->signal->pending));
    
    // Check pending should be empty
    struct signal_mask_bits pending = {0};
    errno = 0;
    int result = do_sigpending(&pending);
    XCTAssertEqual(result, 0, @"do_sigpending should succeed");
    
    // Verify all bits are zero
    for (int i = 0; i < KERNEL_SIG_NUM_WORDS; i++) {
        XCTAssertEqual(pending.sig[i], 0ULL, @"Pending mask should be empty after clear");
    }
}

- (void)testDoSigpendingWorksAfterSignalGeneration {
    struct task_struct *task = get_current();
    XCTAssertTrue(task != NULL, @"Should have a current task");
    XCTAssertTrue(task->signal != NULL, @"Task should have signal state");
    
    // Block SIGUSR1
    struct signal_mask_bits mask = {0};
    struct signal_mask_bits oldmask = {0};
    mask.sig[(TEST_SIGUSR1 - 1) >> 6] |= (1ULL << ((TEST_SIGUSR1 - 1) & 63));
    
    int result = do_sigprocmask(TEST_SIG_BLOCK, &mask, &oldmask);
    XCTAssertEqual(result, 0, @"Block SIGUSR1 should succeed");
    
    // Clear pending first
    memset(&task->signal->pending, 0, sizeof(task->signal->pending));
    
    // Generate signal
    result = do_raise(TEST_SIGUSR1);
    XCTAssertEqual(result, 0, @"do_raise should succeed");
    
    // Check pending
    struct signal_mask_bits pending = {0};
    result = do_sigpending(&pending);
    XCTAssertEqual(result, 0, @"do_sigpending should succeed");
    
    bool is_pending = (pending.sig[(TEST_SIGUSR1 - 1) >> 6] & (1ULL << ((TEST_SIGUSR1 - 1) & 63))) != 0;
    XCTAssertTrue(is_pending, @"SIGUSR1 should be pending");
    
    // Restore mask
    do_sigprocmask(TEST_SIG_SETMASK, &oldmask, NULL);
}

#pragma mark - E. Signal Queue Internal Operations

- (void)testSignalEnqueueDequeueInternal {
    struct task_struct *task = get_current();
    XCTAssertTrue(task != NULL, @"Should have a current task");
    XCTAssertTrue(task->signal != NULL, @"Task should have signal state");
    
    // Enqueue a signal directly
    errno = 0;
    int result = signal_enqueue_task(task, TEST_SIGUSR2);
    XCTAssertEqual(result, 0, @"signal_enqueue_task should succeed");
    
    // Dequeue the signal
    struct signal_mask_bits mask = {0};
    int32_t dequeued_sig = 0;
    result = signal_dequeue(task, &mask, &dequeued_sig);
    XCTAssertEqual(result, 1, @"signal_dequeue should find a signal");
    XCTAssertEqual(dequeued_sig, TEST_SIGUSR2, @"Dequeued signal should be SIGUSR2");
}

#pragma mark - F. Signal 64 Boundary Internal Handling

- (void)testSignal64InternalHandling {
    struct task_struct *task = get_current();
    XCTAssertTrue(task != NULL, @"Should have a current task");
    XCTAssertTrue(task->signal != NULL, @"Task should have signal state");
    
    // Signal 64 is the last valid signal number in Linux
    // Block signal 64 using do_sigprocmask
    struct signal_mask_bits mask = {0};
    struct signal_mask_bits oldmask = {0};
    mask.sig[(64 - 1) >> 6] |= (1ULL << ((64 - 1) & 63));
    
    errno = 0;
    int result = do_sigprocmask(TEST_SIG_BLOCK, &mask, &oldmask);
    XCTAssertEqual(result, 0, @"sigprocmask for signal 64 should succeed");
    
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
    do_sigprocmask(TEST_SIG_SETMASK, &oldmask, NULL);
}

@end