//
//  SignalTests.m
//  IXLandSystemTests
//
//  Signal semantics tests for IXLandSystem
//

#import <XCTest/XCTest.h>
#import <signal.h>
#import <unistd.h>
#import <sys/wait.h>
#import <errno.h>
#import <string.h>
#import <stdlib.h>

@interface SignalTests : XCTestCase

@end

@implementation SignalTests

/* Test 1: Signal wrapper availability */
- (void)testSignalWrappersAreAccessible {
    /* Verify wrappers exist and can be called */
    int result;
    
    /* sigprocmask */
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    result = sigprocmask(SIG_BLOCK, &mask, &oldmask);
    XCTAssertEqual(result, 0, @"sigprocmask should succeed");
    
    /* sigaction */
    struct sigaction act = {0};
    act.sa_handler = SIG_DFL;
    result = sigaction(SIGUSR1, &act, NULL);
    XCTAssertEqual(result, 0, @"sigaction should succeed for valid signal");
    
    /* raise */
    result = raise(SIGUSR1);
    sleep(1); /* Give signal time to process */
}

/* Test 2: Blocked vs Pending behavior */
- (void)testBlockedVersusPending {
    /* Block SIGUSR1 */
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    int result = sigprocmask(SIG_BLOCK, &mask, NULL);
    XCTAssertEqual(result, 0, @"Block SIGUSR1 should succeed");
    
    /* Raise SIGUSR1 */
    result = raise(SIGUSR1);
    XCTAssertEqual(result, 0, @"raise should succeed");
    
    /* Query pending */
    sigset_t pending;
    sigemptyset(&pending);
    result = sigpending(&pending);
    XCTAssertEqual(result, 0, @"sigpending should succeed");
    
    /* SIGUSR1 should be pending */
    XCTAssertTrue(sigismember(&pending, SIGUSR1) == 1, @"SIGUSR1 should be pending while blocked");
    
    /* Unblock */
    result = sigprocmask(SIG_UNBLOCK, &mask, NULL);
    XCTAssertEqual(result, 0, @"Unblock should succeed");
}

/* Test 3: Signal routing */
- (void)testSignalRouting {
    /* Test with different signal numbers */
    int result;
    
    /* Block SIGUSR2 */
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR2);
    result = sigprocmask(SIG_BLOCK, &mask, NULL);
    XCTAssertEqual(result, 0, @"Block SIGUSR2 should succeed");
    
    /* Raise SIGUSR2 */
    result = raise(SIGUSR2);
    XCTAssertEqual(result, 0, @"raise SIGUSR2 should succeed");
    
    /* Verify pending */
    sigset_t pending;
    sigemptyset(&pending);
    result = sigpending(&pending);
    XCTAssertEqual(result, 0, @"sigpending should succeed");
    XCTAssertTrue(sigismember(&pending, SIGUSR2) == 1, @"SIGUSR2 should be pending");
    
    /* Unblock */
    result = sigprocmask(SIG_UNBLOCK, &mask, NULL);
    XCTAssertEqual(result, 0, @"Unblock SIGUSR2 should succeed");
}

/* Test 4: Fork inheritance (using fork to test if it works in IXLand context) */
- (void)testForkInheritance {
    /* Set signal mask in parent */
    sigset_t parent_mask;
    sigemptyset(&parent_mask);
    sigaddset(&parent_mask, SIGUSR1);
    int result = sigprocmask(SIG_BLOCK, &parent_mask, NULL);
    XCTAssertEqual(result, 0, @"Parent block should succeed");
    
    /* Set handler - ignore SIGUSR2 */
    struct sigaction act = {0};
    act.sa_handler = SIG_IGN;
    result = sigaction(SIGUSR2, &act, NULL);
    XCTAssertEqual(result, 0, @"Set handler should succeed");
    
    /* Fork */
    pid_t pid = fork();
    XCTAssertGreaterThanOrEqual(pid, 0, @"fork should succeed");
    
    if (pid == 0) {
        /* Child: test inherited state */
        sigset_t child_mask;
        sigemptyset(&child_mask);
        int childResult = sigprocmask(SIG_BLOCK, NULL, &child_mask);
        if (childResult != 0) {
            exit(1);
        }
        if (sigismember(&child_mask, SIGUSR1) != 1) {
            exit(1);
        }
        
        /* Check handler */
        struct sigaction child_act;
        childResult = sigaction(SIGUSR2, NULL, &child_act);
        if (childResult != 0 || child_act.sa_handler != SIG_IGN) {
            exit(1);
        }
        
        /* Check pending */
        sigset_t child_pending;
        childResult = sigpending(&child_pending);
        if (childResult != 0) {
            exit(1);
        }
        if (sigismember(&child_pending, SIGUSR1) || sigismember(&child_pending, SIGUSR2)) {
            exit(1);
        }
        
        exit(0);
    } else {
        /* Parent: wait for child */
        int status;
        pid_t waited = waitpid(pid, &status, 0);
        XCTAssertEqual(waited, pid, @"waitpid should return child pid");
        XCTAssertTrue(WIFEXITED(status), @"Child should exit normally");
        XCTAssertEqual(WEXITSTATUS(status), 0, @"Child should exit with status 0, meaning inherited state is correct");
    }
}

/* Test 5: Wait integration with signals */
- (void)testWaitIntegration {
    pid_t pid = fork();
    XCTAssertGreaterThanOrEqual(pid, 0, @"fork should succeed");
    
    if (pid == 0) {
        /* Child: exit after brief delay */
        sleep(1);
        exit(42);
    } else {
        /* Parent: send signal to child, then wait */
        int result = kill(pid, SIGUSR1);
        XCTAssertEqual(result, 0, @"Kill should succeed");
        
        /* Wait */
        int status;
        pid_t waited = waitpid(pid, &status, 0);
        XCTAssertEqual(waited, pid, @"waitpid should return child pid");
        XCTAssertTrue(WIFEXITED(status), @"Child should exit normally");
        XCTAssertEqual(WEXITSTATUS(status), 42, @"Exit status should be preserved");
    }
}

@end
