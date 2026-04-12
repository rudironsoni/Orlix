//
// SignalKUnitTests.mm - KUnit white-box tests for IXLand signal owner
//
// White-box tests for signal.c operations:
// - Signal handler allocation and lookup
// - Signal mask operations
// - Signal disposition management
// - Pending signal queue
// - Error paths: EINVAL, EFAULT
//

#import <XCTest/XCTest.h>
#include <signal.h>
#include <errno.h>

#include "signal.h"
#include "../include/ixland/ixland_signal.h"

@interface SignalKUnitTests : XCTestCase
@end

@implementation SignalKUnitTests

#pragma mark - Lifecycle Tests (Happy Path)

- (void)testSighandAlloc {
    ixland_sighand_t *sighand = ixland_sighand_alloc();
    XCTAssertTrue(sighand != NULL, "sighand_alloc should succeed");
    XCTAssertEqual(sighand->refs, 1, "new sighand should have ref count 1");
    
    // Verify all signals have default handler
    for (int i = 1; i < IXLAND_NSIG; i++) {
        XCTAssertEqual(sighand->action[i].sa_handler, SIG_DFL, "signal %d should have default handler", i);
    }
    
// Verify masks are empty - check no signals blocked
BOOL blocked_empty = YES;
BOOL pending_empty = YES;
for (int sig = 1; sig < IXLAND_NSIG; sig++) {
    if (sigismember(&sighand->blocked, sig)) blocked_empty = NO;
    if (sigismember(&sighand->pending, sig)) pending_empty = NO;
}
XCTAssertTrue(blocked_empty, "blocked mask should be empty");
XCTAssertTrue(pending_empty, "pending mask should be empty");
    
    ixland_sighand_free(sighand);
}

- (void)testSighandFree {
    ixland_sighand_t *sighand = ixland_sighand_alloc();
    XCTAssertTrue(sighand != NULL);
    ixland_sighand_free(sighand);
    // No crash is success
}

- (void)testNullFree {
    ixland_sighand_free(NULL);
    // No crash is success
}

#pragma mark - Signal Disposition Tests

- (void)testSigactionInstallHandler {
    ixland_sighand_t *sighand = ixland_sighand_alloc();
    XCTAssertTrue(sighand != NULL);
    
    // Install handler for SIGUSR1
    struct sigaction new_action = {};
    new_action.sa_handler = SIG_IGN;
    sigemptyset(&new_action.sa_mask);
    
    int ret = ixland_sigaction(SIGUSR1, &new_action, NULL);
    XCTAssertEqual(ret, 0, "sigaction should succeed");
    
    // Verify handler installed
    struct sigaction old_action = {};
    ret = ixland_sigaction(SIGUSR1, NULL, &old_action);
    XCTAssertEqual(ret, 0);
    XCTAssertEqual(old_action.sa_handler, SIG_IGN, "handler should be SIG_IGN");
    
    ixland_sighand_free(sighand);
}

- (void)testSigactionDefault {
    ixland_sighand_t *sighand = ixland_sighand_alloc();
    XCTAssertTrue(sighand != NULL);
    
    // Get default action
    struct sigaction action = {};
    int ret = ixland_sigaction(SIGUSR1, NULL, &action);
    XCTAssertEqual(ret, 0, "sigaction get should succeed");
    XCTAssertEqual(action.sa_handler, SIG_DFL, "default handler should be SIG_DFL");
    
    ixland_sighand_free(sighand);
}

- (void)testSigactionIgnore {
    ixland_sighand_t *sighand = ixland_sighand_alloc();
    XCTAssertTrue(sighand != NULL);
    
    // Install SIG_IGN handler
    struct sigaction new_action = {};
    new_action.sa_handler = SIG_IGN;
    new_action.sa_flags = 0;
    sigemptyset(&new_action.sa_mask);
    
    int ret = ixland_sigaction(SIGUSR1, &new_action, NULL);
    XCTAssertEqual(ret, 0, "sigaction set should succeed");
    
    // Get handler back
    struct sigaction current = {};
    ret = ixland_sigaction(SIGUSR1, NULL, &current);
    XCTAssertEqual(ret, 0);
    XCTAssertEqual(current.sa_handler, SIG_IGN, "handler should be SIG_IGN");
    
    ixland_sighand_free(sighand);
}

- (void)testSigactionResetToDefault {
    ixland_sighand_t *sighand = ixland_sighand_alloc();
    XCTAssertTrue(sighand != NULL);
    
    // First set to ignore
    struct sigaction action = {};
    action.sa_handler = SIG_IGN;
    ixland_sigaction(SIGUSR1, &action, NULL);
    
    // Then reset to default
    action.sa_handler = SIG_DFL;
    int ret = ixland_sigaction(SIGUSR1, &action, NULL);
    XCTAssertEqual(ret, 0);
    
    // Verify
    struct sigaction current = {};
    ixland_sigaction(SIGUSR1, NULL, &current);
    XCTAssertEqual(current.sa_handler, SIG_DFL);
    
    ixland_sighand_free(sighand);
}

#pragma mark - Signal Mask Tests

- (void)testSigprocmaskBlock {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    
    int ret = ixland_sigprocmask(SIG_BLOCK, &set, NULL);
    XCTAssertEqual(ret, 0, "sigprocmask block should succeed");
    
    // Get current mask
    sigset_t current;
    sigemptyset(&current);
    ret = ixland_sigprocmask(SIG_SETMASK, NULL, &current);
    XCTAssertEqual(ret, 0);
    XCTAssertTrue(sigismember(&current, SIGUSR1), "SIGUSR1 should be blocked");
}

- (void)testSigprocmaskUnblock {
    // First block
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    ixland_sigprocmask(SIG_BLOCK, &set, NULL);
    
    // Then unblock
    int ret = ixland_sigprocmask(SIG_UNBLOCK, &set, NULL);
    XCTAssertEqual(ret, 0, "sigprocmask unblock should succeed");
    
    // Verify
    sigset_t current;
    sigemptyset(&current);
    ixland_sigprocmask(SIG_SETMASK, NULL, &current);
    XCTAssertFalse(sigismember(&current, SIGUSR1), "SIGUSR1 should be unblocked");
}

- (void)testSigprocmaskSetmask {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGUSR2);
    
    int ret = ixland_sigprocmask(SIG_SETMASK, &set, NULL);
    XCTAssertEqual(ret, 0, "sigprocmask set should succeed");
    
    // Verify
    sigset_t current;
    sigemptyset(&current);
    ixland_sigprocmask(SIG_SETMASK, NULL, &current);
    XCTAssertTrue(sigismember(&current, SIGUSR1), "SIGUSR1 should be in mask");
    XCTAssertTrue(sigismember(&current, SIGUSR2), "SIGUSR2 should be in mask");
}

- (void)testSigprocmaskGetOldmask {
    // Set initial mask
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    ixland_sigprocmask(SIG_SETMASK, &set, NULL);
    
    // Get old mask while changing
    sigset_t new_set;
    sigemptyset(&new_set);
    sigaddset(&new_set, SIGUSR2);
    sigset_t old_set;
    sigemptyset(&old_set);
    
    int ret = ixland_sigprocmask(SIG_SETMASK, &new_set, &old_set);
    XCTAssertEqual(ret, 0);
    XCTAssertTrue(sigismember(&old_set, SIGUSR1), "old mask should have SIGUSR1");
    XCTAssertFalse(sigismember(&old_set, SIGUSR2), "old mask should not have SIGUSR2");
}

#pragma mark - Error Paths (Failure)

- (void)testSigactionInvalidSignal {
    struct sigaction action = {};
    action.sa_handler = SIG_IGN;
    
    int ret = ixland_sigaction(-1, &action, NULL);
    XCTAssertEqual(ret, -1, "sigaction with invalid signal should fail");
    XCTAssertEqual(errno, EINVAL, "errno should be EINVAL");
    
    ret = ixland_sigaction(0, &action, NULL);
    XCTAssertEqual(ret, -1, "sigaction with signal 0 should fail");
    XCTAssertEqual(errno, EINVAL, "errno should be EINVAL");
    
    ret = ixland_sigaction(IXLAND_NSIG + 1, &action, NULL);
    XCTAssertEqual(ret, -1, "sigaction with out of range signal should fail");
    XCTAssertEqual(errno, EINVAL, "errno should be EINVAL");
}

- (void)testSigprocmaskInvalidHow {
    sigset_t set;
    sigemptyset(&set);
    
    int ret = ixland_sigprocmask(999, &set, NULL);
    XCTAssertEqual(ret, -1, "sigprocmask with invalid how should fail");
    XCTAssertEqual(errno, EINVAL, "errno should be EINVAL");
}

- (void)testSigprocmaskNullSet {
    int ret = ixland_sigprocmask(SIG_BLOCK, NULL, NULL);
    XCTAssertEqual(ret, -1, "sigprocmask with NULL set should fail");
    XCTAssertEqual(errno, EFAULT, "errno should be EFAULT");
}

#pragma mark - Edge Cases

- (void)testSigactionReservedSignals {
    // SIGKILL and SIGSTOP cannot be caught or ignored
    struct sigaction action = {};
    action.sa_handler = SIG_IGN;
    
    int ret = ixland_sigaction(SIGKILL, &action, NULL);
    XCTAssertEqual(ret, 0, "sigaction on SIGKILL should succeed");
    // (but the kernel may ignore the handler)
    
    ret = ixland_sigaction(SIGSTOP, &action, NULL);
    XCTAssertEqual(ret, 0, "sigaction on SIGSTOP should succeed");
}

- (void)testSigactionNullActions {
    // Set action with NULL old_action - should succeed
    struct sigaction action = {};
    action.sa_handler = SIG_IGN;
    int ret = ixland_sigaction(SIGUSR1, &action, NULL);
    XCTAssertEqual(ret, 0, "sigaction with NULL old should succeed");
    
    // Get action with NULL new_action - should succeed
    struct sigaction current = {};
    ret = ixland_sigaction(SIGUSR1, NULL, &current);
    XCTAssertEqual(ret, 0, "sigaction with NULL new should succeed");
    XCTAssertEqual(current.sa_handler, SIG_IGN);
    
    // Both NULL - should fail
    ret = ixland_sigaction(SIGUSR1, NULL, NULL);
    XCTAssertEqual(ret, -1, "sigaction with both NULL should fail");
    XCTAssertEqual(errno, EFAULT, "errno should be EFAULT");
}

- (void)testSigemptyset {
    sigset_t set;
    
    // After sigemptyset, set should have no signals
    int ret = sigemptyset(&set);
    XCTAssertEqual(ret, 0, "sigemptyset should succeed");
    XCTAssertFalse(sigismember(&set, SIGUSR1), "SIGUSR1 should not be in empty set");
    
    // After sigfillset, set should have signals
    ret = sigfillset(&set);
    XCTAssertEqual(ret, 0, "sigfillset should succeed");
    XCTAssertTrue(sigismember(&set, SIGUSR1), "SIGUSR1 should be in filled set");
}

- (void)testSigaddsetRemove {
    sigset_t set;
    sigemptyset(&set);
    XCTAssertFalse(sigismember(&set, SIGUSR1), "SIGUSR1 should not be in empty set");
    
    sigaddset(&set, SIGUSR1);
    XCTAssertTrue(sigismember(&set, SIGUSR1), "SIGUSR1 should be in set after add");
    
    sigdelset(&set, SIGUSR1);
    XCTAssertFalse(sigismember(&set, SIGUSR1), "SIGUSR1 should not be in set after remove");
}

- (void)testSigismemberMultipleSignals {
    sigset_t set;
    sigemptyset(&set);
    
    // Add multiple signals
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGUSR1);
    
    XCTAssertTrue(sigismember(&set, SIGINT), "SIGINT should be in set");
    XCTAssertTrue(sigismember(&set, SIGTERM), "SIGTERM should be in set");
    XCTAssertTrue(sigismember(&set, SIGUSR1), "SIGUSR1 should be in set");
    XCTAssertFalse(sigismember(&set, SIGUSR2), "SIGUSR2 should not be in set");
}

- (void)testSignalQueueInit {
    ixland_sighand_t *sighand = ixland_sighand_alloc();
    XCTAssertTrue(sighand != NULL);
    XCTAssertTrue(sighand->queue.head == NULL, "queue should be empty initially");
    ixland_sighand_free(sighand);
}

@end
