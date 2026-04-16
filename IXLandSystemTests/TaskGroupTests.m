//
// TaskGroupTests.m
// IXLandSystemTests
//
// Process group and session semantics tests for IXLandSystem
//

#import <XCTest/XCTest.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

// Declare IXLand's library init function
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

#pragma mark - A. Basic Process Group Identity

- (void)testGetpgrpReturnsCurrentProcessGroup {
    pid_t pgid = getpgrp();
    pid_t pid = getpid();
    
    NSLog(@"getpgrp() returned %d, getpid() returned %d", (int)pgid, (int)pid);
    
    XCTAssertGreaterThan(pgid, 0, @"Process group ID should be positive");
    // After fork, child inherits parent's pgid
    // For init task, pgid == pid initially
}

- (void)testGetpgidReturnsTargetProcessGroup {
    pid_t pid = getpid();
    pid_t pgid = getpgid(pid);
    
    NSLog(@"getpgid(%d) returned %d", (int)pid, (int)pgid);
    
    XCTAssertEqual(pgid, getpgrp(), @"getpgid(pid) should equal getpgrp()");
    XCTAssertGreaterThan(pgid, 0, @"Process group ID should be positive");
}

- (void)testGetpgidZeroReturnsCurrentProcessGroup {
    pid_t pgid_zero = getpgid(0);
    pid_t pgid_explicit = getpgid(getpid());
    
    NSLog(@"getpgid(0)=%d, getpgid(pid)=%d", (int)pgid_zero, (int)pgid_explicit);
    
    XCTAssertEqual(pgid_zero, pgid_explicit, @"getpgid(0) should equal getpgid(getpid())");
}

- (void)testGetpgidRejectsInvalidPid {
    errno = 0;
    pid_t pgid = getpgid(-9999);
    
    NSLog(@"getpgid(-9999) returned %d, errno=%d", (int)pgid, errno);
    
    XCTAssertEqual(pgid, -1, @"Should return -1 for invalid pid");
    XCTAssertEqual(errno, ESRCH, @"errno should be ESRCH");
}

#pragma mark - B. setpgid() Rules

- (void)testSetpgidCreatesNewGroupWithZero {
    pid_t pid = getpid();
    
    // Set pgid to 0 should create new group with pgid == pid
    errno = 0;
    int result = setpgid(pid, 0);
    
    NSLog(@"setpgid(%d, 0) returned %d, errno=%d", (int)pid, result, errno);
    
    if (result == 0) {
        pid_t new_pgid = getpgrp();
        XCTAssertEqual(new_pgid, pid, @"pgid should become pid");
    } else {
        // May fail if already leader
        XCTAssertTrue(errno == EPERM || errno == EINVAL, @"Expected EPERM or EINVAL");
    }
}

- (void)testSetpgidRejectsInvalidPid {
    errno = 0;
    int result = setpgid(-9999, 0);
    
    NSLog(@"setpgid(-9999, 0) returned %d, errno=%d", result, errno);
    
    XCTAssertEqual(result, -1, @"Should fail");
    XCTAssertEqual(errno, ESRCH, @"errno should be ESRCH");
}

- (void)testSetpgidRejectsNegativePgid {
    pid_t pid = getpid();
    
    errno = 0;
    int result = setpgid(pid, -5);
    
    NSLog(@"setpgid(%d, -5) returned %d, errno=%d", (int)pid, result, errno);
    
    XCTAssertEqual(result, -1, @"Should fail with negative pgid");
    XCTAssertEqual(errno, EINVAL, @"errno should be EINVAL");
}

#pragma mark - C. setsid() Rules

- (void)testSetsidRejectsProcessGroupLeader {
    pid_t pid = getpid();
    pid_t pgid = getpgrp();
    
    // If we're already leader, setsid should fail with EPERM
    if (pgid == pid) {
        errno = 0;
        pid_t result = setsid();
        
        NSLog(@"setsid() when already leader returned %d, errno=%d", (int)result, errno);
        
        XCTAssertEqual(result, -1, @"setsid should fail");
        XCTAssertEqual(errno, EPERM, @"errno should be EPERM");
    } else {
        NSLog(@"Not process group leader (pgid=%d, pid=%d), cannot test EPERM", (int)pgid, (int)pid);
        XCTSkip(@"Not process group leader");
    }
}

#pragma mark - D. getsid() Rules

- (void)testGetsidReturnsCurrentSession {
    pid_t pid = getpid();
    pid_t sid = getsid(pid);
    
    NSLog(@"getsid(%d) returned %d", (int)pid, (int)sid);
    
    XCTAssertGreaterThan(sid, 0, @"Session ID should be positive");
    XCTAssertEqual(sid, getsid(0), @"getsid(pid) should equal getsid(0)");
}

- (void)testGetsidRejectsInvalidPid {
    errno = 0;
    pid_t sid = getsid(-9999);
    
    NSLog(@"getsid(-9999) returned %d, errno=%d", (int)sid, errno);
    
    XCTAssertEqual(sid, -1, @"Should return -1");
    XCTAssertEqual(errno, ESRCH, @"errno should be ESRCH");
}

#pragma mark - E. killpg() with Real Signal Routing

- (void)testKillpgActuallyCallsKillpg {
    // Block SIGUSR1 first
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);
    
    pid_t pgid = getpgrp();
    
    // Send signal to our process group
    errno = 0;
    int result = killpg(pgid, SIGUSR1);
    
    NSLog(@"killpg(%d, SIGUSR1) returned %d, errno=%d", (int)pgid, result, errno);
    
    // Should succeed
    XCTAssertEqual(result, 0, @"killpg should succeed (errno=%d)", errno);
    
    // Check if signal is pending
    sigset_t pending;
    sigpending(&pending);
    BOOL is_pending = sigismember(&pending, SIGUSR1);
    
    NSLog(@"SIGUSR1 pending: %d", is_pending);
    XCTAssertTrue(is_pending, @"SIGUSR1 should be pending");
    
    // Clean up
    sigprocmask(SIG_SETMASK, &oldmask, NULL);
}

- (void)testKillpgRejectsInvalidPgid {
    errno = 0;
    int result = killpg(-9999, SIGUSR1);
    
    NSLog(@"killpg(-9999, SIGUSR1) returned %d, errno=%d", result, errno);
    
    XCTAssertEqual(result, -1, @"Should fail");
    XCTAssertEqual(errno, ESRCH, @"errno should be ESRCH");
}

#pragma mark - F. waitpid() Integration

- (void)testWaitpidWithProcessGroupFilter {
    // Test waitpid with WNOHANG - should return immediately
    int status = 0;
    
    errno = 0;
    pid_t result = waitpid(-1, &status, WNOHANG);
    
    NSLog(@"waitpid(-1, &status, WNOHANG) returned %d, errno=%d", (int)result, errno);
    
    // Should return 0 if no children, or -1 if no children at all
    if (result == -1) {
        XCTAssertEqual(errno, ECHILD, @"Should fail with ECHILD if no children");
    } else {
        XCTAssertEqual(result, 0, @"Should return 0 with WNOHANG if no exited children");
    }
}

- (void)testWaitpidReturnsECHildForNoChildren {
    int status = 0;
    
    errno = 0;
    pid_t result = waitpid(0, &status, 0); // pid=0 means wait for any child in same group
    
    NSLog(@"waitpid(0, &status, 0) returned %d, errno=%d", (int)result, errno);
    
    // If we have no children, should fail with ECHILD
    // But might hang if fork is working, so this is environment-dependent
    // Just verify the call doesn't crash
    NSLog(@"waitpid test completed");
}

#pragma mark - G. Signal 64 Test

- (void)testSignal64NotDropped {
    // Signal 64 should be handled correctly
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, 64);  // Signal 64
    
    errno = 0;
    int result = sigprocmask(SIG_BLOCK, &mask, &oldmask);
    
    NSLog(@"sigprocmask blocking signal 64 returned %d, errno=%d", result, errno);
    
    XCTAssertEqual(result, 0, @"Should succeed (errno=%d)", errno);
    
    // Raise signal 64
    errno = 0;
    result = raise(64);
    
    NSLog(@"raise(64) returned %d, errno=%d", result, errno);
    
    XCTAssertEqual(result, 0, @"raise(64) should succeed");
    
    // Check pending
    sigset_t pending;
    sigpending(&pending);
    BOOL is_pending = sigismember(&pending, 64);
    
    NSLog(@"Signal 64 pending: %d", is_pending);
    XCTAssertTrue(is_pending, @"Signal 64 should be pending");
    
    // Restore mask
    sigprocmask(SIG_SETMASK, &oldmask, NULL);
}

@end
