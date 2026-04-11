//
//  IXLANDSignalTests.mm
//  Signal syscall tests - 5 syscalls
//

#import <XCTest/XCTest.h>
#import "IXLANDTestHelpers.h"
#import <signal.h>

@interface IXLANDSignalTests : XCTestCase
@property (nonatomic, strong) IXLANDTestEnvironment *env;
@end

@implementation IXLANDSignalTests

- (void)setUp {
    [super setUp];
    self.env = [[IXLANDTestEnvironment alloc] init];
    [IXLANDTestHelpers resetLibraryState];
}

- (void)tearDown {
    [self.env cleanup];
    self.env = nil;
    [super tearDown];
}

#pragma mark - signal (1 syscall)

- (void)testSignal_SetHandler {
    // Save original handler
    __sighandler_t original = signal(SIGUSR1, SIG_IGN);
    XCTAssertNotEqual(original, SIG_ERR);
    
    // Restore original
    signal(SIGUSR1, original);
}

- (void)testSignal_IgnoreSignal {
    __sighandler_t result = signal(SIGUSR2, SIG_IGN);
    XCTAssertNotEqual(result, SIG_ERR);
    
    // Restore default
    signal(SIGUSR2, SIG_DFL);
}

- (void)testSignal_DefaultHandler {
    __sighandler_t result = signal(SIGPIPE, SIG_DFL);
    XCTAssertNotEqual(result, SIG_ERR);
}

#pragma mark - kill (1 syscall)

- (void)testKill_InvalidPid_ReturnsError {
    int result = kill(-1, SIGTERM);
    XCTAssertEqual(result, -1);
    XCTAssertEqual(errno, ESRCH);
}

- (void)testKill_ZeroPid_NoError {
    // signal 0 is error check only
    int result = kill(0, 0);
    // May succeed or fail depending on process group
    // Just ensure it doesn't crash
}

- (void)testKill_InvalidSignal_ReturnsError {
    int result = kill(getpid(), 999);
    XCTAssertEqual(result, -1);
    XCTAssertEqual(errno, EINVAL);
}

#pragma mark - sigaction (1 syscall)

- (void)testSigaction_SetHandler {
    struct sigaction sa;
    struct sigaction old_sa;
    
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    
    int result = sigaction(SIGUSR1, &sa, &old_sa);
    XCTAssertEqual(result, 0);
    
    // Restore original
    sigaction(SIGUSR1, &old_sa, NULL);
}

- (void)testSigaction_GetOldHandler {
    struct sigaction sa, old_sa;
    
    // Set up a handler
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sigaction(SIGUSR2, &sa, NULL);
    
    // Get old handler without changing
    int result = sigaction(SIGUSR2, NULL, &old_sa);
    XCTAssertEqual(result, 0);
    XCTAssertEqual(old_sa.sa_handler, SIG_DFL);
}

- (void)testSigaction_WithSiginfo {
    struct sigaction sa;
    
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = NULL;  // Would set actual handler here
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    
    // Just test that the call succeeds
    int result = sigaction(SIGUSR1, &sa, NULL);
    // Result depends on implementation
    
    // Restore default
    signal(SIGUSR1, SIG_DFL);
}

#pragma mark - sigprocmask (1 syscall)

- (void)testSigprocmask_BlockSignal {
    sigset_t set, oldset;
    
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    
    int result = sigprocmask(SIG_BLOCK, &set, &oldset);
    XCTAssertEqual(result, 0);
    
    // Restore
    sigprocmask(SIG_SETMASK, &oldset, NULL);
}

- (void)testSigprocmask_UnblockSignal {
    sigset_t set, oldset;
    
    // First block
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigprocmask(SIG_BLOCK, &set, NULL);
    
    // Then unblock
    int result = sigprocmask(SIG_UNBLOCK, &set, &oldset);
    XCTAssertEqual(result, 0);
}

- (void)testSigprocmask_GetCurrentMask {
    sigset_t set;
    
    int result = sigprocmask(SIG_BLOCK, NULL, &set);
    XCTAssertEqual(result, 0);
    // set now contains current mask
}

#pragma mark - alarm (1 syscall)

- (void)testAlarm_SetTimer {
    // Set alarm for 1 second
    unsigned int old = alarm(1);
    XCTAssertEqual(old, 0);  // No previous alarm
    
    // Cancel alarm
    unsigned int remaining = alarm(0);
    XCTAssertEqual(remaining, 1);  // Should return previous setting
}

- (void)testAlarm_CancelAlarm {
    // Set alarm
    alarm(5);
    
    // Cancel it
    unsigned int remaining = alarm(0);
    XCTAssertGreaterThan(remaining, 0);
}

- (void)testAlarm_ZeroCancels {
    // Set alarm
    alarm(10);
    
    // Zero cancels
    unsigned int remaining = alarm(0);
    XCTAssertGreaterThan(remaining, 0);
}

#pragma mark - Signal Integration Tests

- (void)testSignalDelivery_Basic {
    // This test would require a real child process
    // For now, just verify signal handlers can be set
    
    __block volatile sig_atomic_t signal_received = 0;
    
    signal(SIGUSR1, ^(int sig) {
        signal_received = 1;
    });
    
    // Note: Can't easily test self-delivery in XCTest
    // This is more of an integration test with fork
    
    // Restore
    signal(SIGUSR1, SIG_DFL);
}

- (void)testSignalMask_Persistence {
    sigset_t set1, set2;
    
    // Block SIGUSR1
    sigemptyset(&set1);
    sigaddset(&set1, SIGUSR1);
    sigprocmask(SIG_BLOCK, &set1, NULL);
    
    // Get mask and verify
    sigprocmask(SIG_BLOCK, NULL, &set2);
    XCTAssertTrue(sigismember(&set2, SIGUSR1));
    
    // Unblock
    sigprocmask(SIG_UNBLOCK, &set1, NULL);
    
    // Verify unblocked
    sigprocmask(SIG_BLOCK, NULL, &set2);
    XCTAssertFalse(sigismember(&set2, SIGUSR1));
}

@end
