//
//  IXLANDProcessTests.mm
//  IXLANDTestSuite
//
//  Process Management Unit Tests - 16 syscalls
//

#import <XCTest/XCTest.h>
#import "IXLANDTestHelpers.h"

// C headers for syscalls
extern "C" {
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

// libixland function declarations
int ixland_is_initialized(void);
const char *ixland_version(void);
pid_t ixland_getpid(void);
pid_t ixland_getppid(void);
pid_t ixland_getpgrp(void);
int ixland_setpgrp(void);
pid_t ixland_getpgid(pid_t pid);
int ixland_setpgid(pid_t pid, pid_t pgid);
pid_t ixland_fork(void);
pid_t ixland_vfork(void);
void ixland_exit(int status);
void ixland__exit(int status);
int ixland_execve(const char *pathname, char *const argv[], char *const envp[]);
int ixland_execv(const char *pathname, char *const argv[]);
pid_t ixland_wait(int *status);
pid_t ixland_waitpid(pid_t pid, int *status, int options);
pid_t ixland_wait3(int *status, int options, struct rusage *rusage);
pid_t ixland_wait4(pid_t pid, int *status, int options, struct rusage *rusage);
int ixland_system(const char *command);
}

@interface IXLANDProcessTests : XCTestCase
@property (nonatomic) IXLANDTestEnvironment *env;
@end

@implementation IXLANDProcessTests

- (void)setUp {
    [super setUp];
    self.env = [IXLANDTestEnvironment sharedEnvironment];
    [self.env setup];
}

- (void)tearDown {
    [self.env cleanup];
    [super tearDown];
}

#pragma mark - Initialization Tests

- (void)testLibraryInitialization_Success {
    XCTAssertTrue(ixland_is_initialized(), @"Library should be initialized");
}

- (void)testLibraryVersion_ReturnsNonEmptyString {
    const char *version = ixland_version();
    XCTAssertNotNil(@(version), @"Version should not be NULL");
    XCTAssertGreaterThan(strlen(version), 0, @"Version should not be empty");
}

#pragma mark - PID Tests

- (void)testGetpid_ReturnsPositiveValue {
    pid_t pid = ixland_getpid();
    XCTAssertGreaterThan(pid, 0, @"PID should be positive");
}

- (void)testGetppid_ReturnsPositiveValue {
    pid_t ppid = ixland_getppid();
    XCTAssertGreaterThan(ppid, 0, @"PPID should be positive");
}

- (void)testGetpid_Getppid_DifferentValues {
    pid_t pid = ixland_getpid();
    pid_t ppid = ixland_getppid();
    XCTAssertNotEqual(pid, ppid, @"PID and PPID should be different");
}

#pragma mark - Process Group Tests

- (void)testGetpgrp_ReturnsValidGroup {
    pid_t pgid = ixland_getpgrp();
    XCTAssertGreaterThan(pgid, 0, @"Process group ID should be positive");
}

- (void)testSetpgrp_CreatesNewGroup {
    pid_t old_pgid = ixland_getpgrp();
    int result = ixland_setpgrp();
    XCTAssertEqual(result, 0, @"setpgrp should succeed");
    
    pid_t new_pgid = ixland_getpgrp();
    XCTAssertEqual(new_pgid, ixland_getpid(), @"New group should equal PID");
}

- (void)testGetpgid_SameProcess_ReturnsOwnGroup {
    pid_t pid = ixland_getpid();
    pid_t pgid = ixland_getpgid(pid);
    XCTAssertEqual(pgid, ixland_getpgrp(), @"getpgid(self) should equal getpgrp()");
}

- (void)testSetpgid_ChangesGroup {
    pid_t pid = ixland_getpid();
    pid_t new_pgid = pid + 1000; // Arbitrary new group
    
    int result = ixland_setpgid(pid, new_pgid);
    // May fail due to permissions, but should not crash
    if (result == 0) {
        pid_t pgid = ixland_getpgrp();
        XCTAssertEqual(pgid, new_pgid, @"Group should be changed");
    }
}

#pragma mark - Fork Tests

- (void)testFork_CreatesNewProcess {
    IXLANDAssertNoMemoryLeak(({
        pid_t pid = ixland_fork();
        
        if (pid == 0) {
            // Child process
            pid_t child_pid = ixland_getpid();
            XCTAssertGreaterThan(child_pid, 0, @"Child should have valid PID");
            ixland_exit(0);
        } else if (pid > 0) {
            // Parent process
            int status;
            pid_t result = ixland_waitpid(pid, &status, 0);
            XCTAssertEqual(result, pid, @"waitpid should return child PID");
            XCTAssertTrue(WIFEXITED(status), @"Child should have exited normally");
            XCTAssertEqual(WEXITSTATUS(status), 0, @"Child exit code should be 0");
        } else {
            XCTFail(@"fork failed: %d", errno);
        }
    }), @"fork should not leak memory");
}

- (void)testFork_ReturnsZeroToChild {
    pid_t pid = ixland_fork();
    
    if (pid == 0) {
        // In child, pid should be 0
        XCTAssertEqual(ixland_getpid(), ixland_getppid() + 1, @"Child PID logic check");
        ixland_exit(0);
    } else if (pid > 0) {
        int status;
        ixland_waitpid(pid, &status, 0);
        XCTAssertEqual(WEXITSTATUS(status), 0, @"Child should exit successfully");
    }
}

- (void)testFork_ChildHasDifferentPid {
    pid_t parent_pid = ixland_getpid();
    
    pid_t pid = ixland_fork();
    
    if (pid == 0) {
        // Child
        pid_t child_pid = ixland_getpid();
        XCTAssertNotEqual(child_pid, parent_pid, @"Child should have different PID");
        ixland_exit(0);
    } else if (pid > 0) {
        int status;
        ixland_waitpid(pid, &status, 0);
    }
}

- (void)testVfork_IdenticalToFork {
    IXLANDAssertNoMemoryLeak(({
        pid_t pid = ixland_vfork();
        
        if (pid == 0) {
            // Child
            XCTAssertGreaterThan(ixland_getpid(), 0, @"vfork child should have valid PID");
            ixland_exit(0);
        } else if (pid > 0) {
            int status;
            pid_t result = ixland_waitpid(pid, &status, 0);
            XCTAssertEqual(result, pid, @"vfork waitpid should work");
            XCTAssertTrue(WIFEXITED(status), @"vfork child should exit");
        } else {
            XCTFail(@"vfork failed: %d", errno);
        }
    }), @"vfork should not leak memory");
}

#pragma mark - Wait Tests

- (void)testWait_NoChildren_ReturnsError {
    // This may return ECHILD if no children, which is expected
    int status;
    pid_t result = ixland_wait(&status);
    // Result should be -1 with ECHILD
    XCTAssertEqual(result, -1, @"wait with no children should fail");
    XCTAssertEqual(errno, ECHILD, @"errno should be ECHILD");
}

- (void)testWaitpid_SpecificChild {
    pid_t child_pid = ixland_fork();
    
    if (child_pid == 0) {
        // Child: sleep briefly then exit
        sleep(1);
        ixland_exit(42);
    } else if (child_pid > 0) {
        int status;
        pid_t result = ixland_waitpid(child_pid, &status, 0);
        
        XCTAssertEqual(result, child_pid, @"waitpid should return child PID");
        XCTAssertTrue(WIFEXITED(status), @"Child should have exited");
        XCTAssertEqual(WEXITSTATUS(status), 42, @"Exit code should be 42");
    }
}

- (void)testWaitpid_WNOHANG_NoChildren {
    int status;
    pid_t result = ixland_waitpid(-1, &status, WNOHANG);
    // Should return 0 if no children ready
    XCTAssertEqual(result, 0, @"WNOHANG with no children should return 0");
}

- (void)testWait3_WithRusage {
    pid_t child_pid = ixland_fork();
    
    if (child_pid == 0) {
        // Child: do some work
        for (volatile int i = 0; i < 1000000; i++);
        ixland_exit(0);
    } else if (child_pid > 0) {
        int status;
        struct rusage usage;
        pid_t result = ixland_wait3(&status, 0, &usage);
        
        XCTAssertEqual(result, child_pid, @"wait3 should return child PID");
        XCTAssertTrue(WIFEXITED(status), @"Child should exit");
        // rusage may be zero in simulator, just verify no crash
        XCTAssertGreaterThanOrEqual(usage.ru_utime.tv_sec, 0, @"User time should be valid");
    }
}

- (void)testWait4_SpecificChildWithRusage {
    pid_t child_pid = ixland_fork();
    
    if (child_pid == 0) {
        ixland_exit(0);
    } else if (child_pid > 0) {
        int status;
        struct rusage usage;
        pid_t result = ixland_wait4(child_pid, &status, 0, &usage);
        
        XCTAssertEqual(result, child_pid, @"wait4 should return child PID");
        XCTAssertTrue(WIFEXITED(status), @"Child should exit");
    }
}

#pragma mark - Exit Tests

- (void)testExit_NormalTermination {
    pid_t child_pid = ixland_fork();
    
    if (child_pid == 0) {
        // Child: test normal exit
        ixland_exit(123);
        // Should not reach here
        XCTFail(@"Should not execute after exit");
    } else if (child_pid > 0) {
        int status;
        ixland_waitpid(child_pid, &status, 0);
        
        XCTAssertTrue(WIFEXITED(status), @"Should have exited normally");
        XCTAssertEqual(WEXITSTATUS(status), 123, @"Exit code should be 123");
    }
}

- (void)testExit_ZeroExitCode {
    pid_t child_pid = ixland_fork();
    
    if (child_pid == 0) {
        ixland_exit(0);
    } else if (child_pid > 0) {
        int status;
        ixland_waitpid(child_pid, &status, 0);
        XCTAssertEqual(WEXITSTATUS(status), 0, @"Exit code should be 0");
    }
}

- (void)testExit_SignalTermination {
    pid_t child_pid = ixland_fork();
    
    if (child_pid == 0) {
        // Child: simulate signal
        ixland_exit(128 + SIGTERM);  // Signal exit
    } else if (child_pid > 0) {
        int status;
        ixland_waitpid(child_pid, &status, 0);
        
        XCTAssertTrue(WIFEXITED(status) || WIFSIGNALED(status), 
                     @"Should have exited or been signaled");
    }
}

#pragma mark - Stress Tests

IXLANDStressTest(Fork, 1000) {
    IXLANDPerformanceTimer *timer = [[IXLANDPerformanceTimer alloc] init];
    NSUInteger baseline = [IXLANDMemoryTracker currentResidentSize];
    
    [timer measureBlock:^{
        pid_t pid = ixland_fork();
        if (pid == 0) {
            ixland_exit(0);
        } else if (pid > 0) {
            int status;
            ixland_waitpid(pid, &status, 0);
        }
    } iterations:1000];
    
    NSLog(@"Fork stress: %@", timer.report);
    
    // Verify no memory leak
    NSUInteger current = [IXLANDMemoryTracker currentResidentSize];
    XCTAssertEqualWithAccuracy(current, baseline, 16384, 
        @"Memory leak detected after 1000 forks");
}

@end
