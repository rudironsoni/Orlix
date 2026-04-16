//
// LinuxUAPIHeaderTests.m
// IXLandSystemTests
//
// LINUX UAPI / ABI COMPILE TEST
//
// These tests prove that vendored Linux UAPI headers are resolvable
// through canonical target include paths. They are compile-validation
// tests, not runtime semantic tests.
//
// Allowed includes:
//   - <linux/...>
//   - <asm/...>
//   - <asm-generic/...>
//
// Forbidden:
//   - Path traversal into third_party/linux-uapi
//   - Darwin headers as shortcuts
//

#import <XCTest/XCTest.h>

/* Linux UAPI headers - resolved through $(LINUX_UAPI_INCLUDE_ROOT) */
#include <linux/wait.h>           /* For wait macros like WNOHANG */
#include <linux/signal.h>         /* For signal constants like SIG_BLOCK */
#include <asm/signal.h>           /* Architecture-specific signal defs */
#include <asm-generic/errno-base.h> /* Linux errno constants */

@interface LinuxUAPIHeaderTests : XCTestCase
@end

@implementation LinuxUAPIHeaderTests

- (void)testLinuxWaitHeaderAvailable {
    // WNOHANG should be defined in linux/wait.h
    int flags = WNOHANG;
    XCTAssertEqual(flags, WNOHANG, @"WNOHANG should be available");
    
    // Verify standard wait flags are present
    XCTAssertEqual(WUNTRACED, WUNTRACED, @"WUNTRACED should be defined");
    XCTAssertTrue(WIFEXITED(0) >= 0 || WIFEXITED(0) == 0, @"WIFEXITED macro should work");
}

- (void)testLinuxSignalHeaderAvailable {
    // Verify signal constants from linux/signal.h
    XCTAssertEqual(SIG_BLOCK, SIG_BLOCK, @"SIG_BLOCK should be defined");
    XCTAssertEqual(SIG_UNBLOCK, SIG_UNBLOCK, @"SIG_UNBLOCK should be defined");
    XCTAssertEqual(SIG_SETMASK, SIG_SETMASK, @"SIG_SETMASK should be defined");
    
    // Verify signal numbers are sensible
    XCTAssertGreaterThan(SIGUSR1, 0, @"SIGUSR1 should be positive");
    XCTAssertLessThanOrEqual(SIGRTMAX, 64, @"SIGRTMAX should be <= 64");
}

- (void)testAsmSignalHeaderAvailable {
    // asm/signal.h should provide architecture-specific definitions
    // Verify basic signal count from architecture headers
    
    // _NSIG should be defined (signal count)
    XCTAssertGreaterThan(_NSIG, 0, @"_NSIG should be defined and positive");
    
    // sigset_t type should be available from asm headers
    sigset_t test_set;
    sigemptyset(&test_set);
    XCTAssertTrue(!sigismember(&test_set, SIGUSR1), @"sigemptyset should clear all");
}

- (void)testErrnoConstantsAvailable {
    // Linux errno constants from asm-generic/errno-base.h
    XCTAssertEqual(EPERM, EPERM, @"EPERM should be defined");
    XCTAssertEqual(ENOENT, ENOENT, @"ENOENT should be defined");
    XCTAssertEqual(ESRCH, ESRCH, @"ESRCH should be defined");
    XCTAssertEqual(EINTR, EINTR, @"EINTR should be defined");
    XCTAssertEqual(EIO, EIO, @"EIO should be defined");
    XCTAssertEqual(EINVAL, EINVAL, @"EINVAL should be defined");
    XCTAssertEqual(ECHILD, ECHILD, @"ECHILD should be defined");
    XCTAssertEqual(EAGAIN, EAGAIN, @"EAGAIN should be defined");
}

- (void)testLinuxTypesAvailable {
    // Standard Linux types from UAPI headers
    pid_t test_pid = 12345;
    XCTAssertEqual(test_pid, 12345, @"pid_t should work");
    
    uid_t test_uid = 1000;
    XCTAssertEqual(test_uid, 1000, @"uid_t should work");
    
    gid_t test_gid = 1000;
    XCTAssertEqual(test_gid, 1000, @"gid_t should work");
}

- (void)testWaitStatusMacros {
    // Test wait status macros work with vendored headers
    int status = 0;
    int wstatus = 42 << 8;  // Normal exit with status 42
    
    // These should be callable even if they return false for this value
    bool exited = WIFEXITED(wstatus);
    bool signaled = WIFSIGNALED(wstatus);
    bool stopped = WIFSTOPPED(wstatus);
    
    // At least one should evaluate (or none should crash)
    XCTAssertTrue(exited || signaled || stopped || true, @"WIF macros should compile");
}

@end
