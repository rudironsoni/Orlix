//
//  IXLANDTtyTests.mm
//  IXLANDTestSuiteTests
//
//  TTY syscall tests (isatty, tcgetattr, tcsetattr)
//

#import <XCTest/XCTest.h>
#import <unistd.h>
#import <termios.h>
#import <fcntl.h>
#import "IXLANDTestHelpers.h"

@interface IXLANDTtyTests : XCTestCase
@end

@implementation IXLANDTtyTests

- (void)setUp {
    [super setUp];
    IXLANDResetLibraryState();
}

- (void)tearDown {
    IXLANDResetLibraryState();
    [super tearDown];
}

// MARK: - isatty Tests

- (void)testIsatty_Stdin {
    int result = isatty(STDIN_FILENO);
    
    // In test environment, stdin is usually not a TTY
    // Result depends on how tests are run
    NSLog(@"isatty(STDIN_FILENO) = %d", result);
    
    // Just verify it doesn't crash and returns 0 or 1
    XCTAssertTrue(result == 0 || result == 1, @"isatty should return 0 or 1");
}

- (void)testIsatty_Stdout {
    int result = isatty(STDOUT_FILENO);
    
    NSLog(@"isatty(STDOUT_FILENO) = %d", result);
    
    // In Xcode test runner, stdout is not a TTY
    XCTAssertTrue(result == 0 || result == 1, @"isatty should return 0 or 1");
}

- (void)testIsatty_Stderr {
    int result = isatty(STDERR_FILENO);
    
    NSLog(@"isatty(STDERR_FILENO) = %d", result);
    
    XCTAssertTrue(result == 0 || result == 1, @"isatty should return 0 or 1");
}

- (void)testIsatty_InvalidFd {
    int result = isatty(-1);
    
    XCTAssertEqual(result, 0, @"isatty with invalid fd should return 0 (not a tty)");
    XCTAssertEqual(errno, EBADF, @"Should set EBADF error");
}

- (void)testIsatty_NonExistentFd {
    int result = isatty(9999);
    
    XCTAssertEqual(result, 0, @"isatty with non-existent fd should return 0");
    XCTAssertEqual(errno, EBADF, @"Should set EBADF error");
}

- (void)testIsatty_RegularFile {
    NSString *testFile = [IXLANDTestHelpers tempFilePathWithPrefix:@"tty_test"];
    const char *path = [testFile UTF8String];
    
    int fd = open(path, O_CREAT | O_RDWR, 0644);
    XCTAssertGreaterThanOrEqual(fd, 0, @"Should be able to create test file");
    
    if (fd >= 0) {
        int result = isatty(fd);
        
        XCTAssertEqual(result, 0, @"isatty on regular file should return 0");
        
        close(fd);
    }
    
    // Cleanup
    unlink(path);
}

- (void)testIsatty_Pipe {
    int pipefd[2];
    int result = pipe(pipefd);
    XCTAssertEqual(result, 0, @"pipe should succeed");
    
    if (result == 0) {
        int ttyResult = isatty(pipefd[0]);
        XCTAssertEqual(ttyResult, 0, @"isatty on pipe read end should return 0");
        
        ttyResult = isatty(pipefd[1]);
        XCTAssertEqual(ttyResult, 0, @"isatty on pipe write end should return 0");
        
        close(pipefd[0]);
        close(pipefd[1]);
    }
}

// MARK: - tcgetattr Tests

- (void)testTcgetattr_Stdin {
    struct termios term;
    
    int result = tcgetattr(STDIN_FILENO, &term);
    
    if (isatty(STDIN_FILENO)) {
        XCTAssertEqual(result, 0, @"tcgetattr on TTY should succeed");
        if (result == 0) {
            // Verify some fields are set
            XCTAssertNotEqual(term.c_iflag, (tcflag_t)-1, @"c_iflag should be set");
            XCTAssertNotEqual(term.c_oflag, (tcflag_t)-1, @"c_oflag should be set");
            XCTAssertNotEqual(term.c_cflag, (tcflag_t)-1, @"c_cflag should be set");
            XCTAssertNotEqual(term.c_lflag, (tcflag_t)-1, @"c_lflag should be set");
        }
    } else {
        XCTAssertEqual(result, -1, @"tcgetattr on non-TTY should fail");
        XCTAssertEqual(errno, ENOTTY, @"Should set ENOTTY");
    }
}

- (void)testTcgetattr_InvalidFd {
    struct termios term;
    
    int result = tcgetattr(-1, &term);
    
    XCTAssertEqual(result, -1, @"tcgetattr with invalid fd should fail");
    XCTAssertEqual(errno, EBADF, @"Should set EBADF");
}

- (void)testTcgetattr_RegularFile {
    NSString *testFile = [IXLANDTestHelpers tempFilePathWithPrefix:@"tcgetattr_test"];
    const char *path = [testFile UTF8String];
    
    int fd = open(path, O_CREAT | O_RDWR, 0644);
    XCTAssertGreaterThanOrEqual(fd, 0, @"Should be able to create test file");
    
    if (fd >= 0) {
        struct termios term;
        int result = tcgetattr(fd, &term);
        
        XCTAssertEqual(result, -1, @"tcgetattr on regular file should fail");
        XCTAssertEqual(errno, ENOTTY, @"Should set ENOTTY");
        
        close(fd);
    }
    
    unlink(path);
}

- (void)testTcgetattr_NullTermios {
    // Null pointer should cause EFAULT or EINVAL
    int result = tcgetattr(STDIN_FILENO, NULL);
    
    // Behavior varies by implementation
    if (result == -1) {
        NSLog(@"tcgetattr with NULL termios failed with errno: %d", errno);
        XCTAssertTrue(errno == EFAULT || errno == EINVAL,
                      @"Should set EFAULT or EINVAL for NULL pointer");
    }
}

// MARK: - tcsetattr Tests

- (void)testTcsetattr_Stdin {
    struct termios term;
    
    int getResult = tcgetattr(STDIN_FILENO, &term);
    
    if (getResult == 0) {
        // Save original settings
        struct termios original = term;
        
        // Try to set with no changes
        int result = tcsetattr(STDIN_FILENO, TCSANOW, &term);
        XCTAssertEqual(result, 0, @"tcsetattr with no changes should succeed");
        
        // Restore original (even though we didn't change anything)
        result = tcsetattr(STDIN_FILENO, TCSANOW, &original);
        XCTAssertEqual(result, 0, @"tcsetattr restore should succeed");
    } else {
        // Not a TTY, should fail
        XCTAssertEqual(getResult, -1, @"tcgetattr should fail on non-TTY");
    }
}

- (void)testTcsetattr_InvalidFd {
    struct termios term = {0};
    
    int result = tcsetattr(-1, TCSANOW, &term);
    
    XCTAssertEqual(result, -1, @"tcsetattr with invalid fd should fail");
    XCTAssertEqual(errno, EBADF, @"Should set EBADF");
}

- (void)testTcsetattr_NonExistentFd {
    struct termios term = {0};
    
    int result = tcsetattr(9999, TCSANOW, &term);
    
    XCTAssertEqual(result, -1, @"tcsetattr with non-existent fd should fail");
    XCTAssertEqual(errno, EBADF, @"Should set EBADF");
}

- (void)testTcsetattr_RegularFile {
    NSString *testFile = [IXLANDTestHelpers tempFilePathWithPrefix:@"tcsetattr_test"];
    const char *path = [testFile UTF8String];
    
    int fd = open(path, O_CREAT | O_RDWR, 0644);
    XCTAssertGreaterThanOrEqual(fd, 0, @"Should be able to create test file");
    
    if (fd >= 0) {
        struct termios term = {0};
        int result = tcsetattr(fd, TCSANOW, &term);
        
        XCTAssertEqual(result, -1, @"tcsetattr on regular file should fail");
        XCTAssertEqual(errno, ENOTTY, @"Should set ENOTTY");
        
        close(fd);
    }
    
    unlink(path);
}

- (void)testTcsetattr_InvalidAction {
    // Only TCSANOW, TCSADRAIN, TCSAFLUSH are valid
    struct termios term;
    
    int getResult = tcgetattr(STDIN_FILENO, &term);
    if (getResult == 0) {
        int result = tcsetattr(STDIN_FILENO, 999, &term);
        
        XCTAssertEqual(result, -1, @"tcsetattr with invalid action should fail");
        XCTAssertEqual(errno, EINVAL, @"Should set EINVAL");
    }
}

- (void)testTcsetattr_NullTermios {
    int result = tcsetattr(STDIN_FILENO, TCSANOW, NULL);
    
    if (result == -1) {
        NSLog(@"tcsetattr with NULL termios failed with errno: %d", errno);
        XCTAssertTrue(errno == EFAULT || errno == EINVAL,
                      @"Should set EFAULT or EINVAL for NULL pointer");
    }
}

- (void)testTcsetattr_ActionValues {
    struct termios term;
    
    int getResult = tcgetattr(STDIN_FILENO, &term);
    
    if (getResult == 0) {
        // Test TCSADRAIN
        int result = tcsetattr(STDIN_FILENO, TCSADRAIN, &term);
        if (result == 0) {
            NSLog(@"TCSADRAIN supported");
        }
        
        // Test TCSAFLUSH
        result = tcsetattr(STDIN_FILENO, TCSAFLUSH, &term);
        if (result == 0) {
            NSLog(@"TCSAFLUSH supported");
        }
        
        // Restore with TCSANOW
        result = tcsetattr(STDIN_FILENO, TCSANOW, &term);
        XCTAssertEqual(result, 0, @"TCSANOW should always work");
    }
}

// MARK: - Integration Tests

- (void)testTtyWorkflow_SaveAndRestore {
    struct termios original;
    
    int result = tcgetattr(STDIN_FILENO, &original);
    
    if (result == 0) {
        // Save settings
        struct termios saved = original;
        
        // Make a small change (disable echo temporarily)
        struct termios modified = original;
        modified.c_lflag &= ~ECHO;
        
        // Apply change
        result = tcsetattr(STDIN_FILENO, TCSANOW, &modified);
        XCTAssertEqual(result, 0, @"Should be able to modify TTY settings");
        
        if (result == 0) {
            // Read back settings
            struct termios current;
            result = tcgetattr(STDIN_FILENO, &current);
            XCTAssertEqual(result, 0, @"Should be able to read TTY settings");
            
            // Verify change was applied
            XCTAssertEqual(current.c_lflag & ECHO, 0, @"ECHO should be disabled");
            
            // Restore original
            result = tcsetattr(STDIN_FILENO, TCSANOW, &saved);
            XCTAssertEqual(result, 0, @"Should be able to restore TTY settings");
            
            // Verify restoration
            result = tcgetattr(STDIN_FILENO, &current);
            XCTAssertEqual(result, 0, @"Should be able to read after restore");
            XCTAssertEqual(current.c_lflag & ECHO, saved.c_lflag & ECHO,
                          @"ECHO flag should be restored to original");
        }
    } else {
        NSLog(@"Skipping TTY workflow test - not a TTY");
    }
}

- (void)testTtyStress_MultipleGetSetCycles {
    struct termios term;
    
    int getResult = tcgetattr(STDIN_FILENO, &term);
    
    if (getResult == 0) {
        struct termios original = term;
        
        for (int i = 0; i < 100; i++) {
            // Get
            int result = tcgetattr(STDIN_FILENO, &term);
            XCTAssertEqual(result, 0, @"Get cycle %d should succeed", i);
            
            // Set (no change)
            result = tcsetattr(STDIN_FILENO, TCSANOW, &term);
            XCTAssertEqual(result, 0, @"Set cycle %d should succeed", i);
        }
        
        // Restore original
        tcsetattr(STDIN_FILENO, TCSANOW, &original);
        
        IXLANDAssertNoMemoryLeak(@"TTY stress test should not leak");
    }
}

// MARK: - Edge Cases

- (void)testIsatty_ClosedFd {
    NSString *testFile = [IXLANDTestHelpers tempFilePathWithPrefix:@"closed_fd_test"];
    const char *path = [testFile UTF8String];
    
    int fd = open(path, O_CREAT | O_RDWR, 0644);
    XCTAssertGreaterThanOrEqual(fd, 0, @"Should be able to create test file");
    
    if (fd >= 0) {
        close(fd);
        
        int result = isatty(fd);
        
        XCTAssertEqual(result, 0, @"isatty on closed fd should return 0");
        XCTAssertEqual(errno, EBADF, @"Should set EBADF");
    }
    
    unlink(path);
}

- (void)testTermiosSpeedSettings {
    struct termios term;
    
    int result = tcgetattr(STDIN_FILENO, &term);
    
    if (result == 0) {
        // Verify speed macros work
        speed_t inputSpeed = cfgetispeed(&term);
        speed_t outputSpeed = cfgetospeed(&term);
        
        NSLog(@"TTY speeds - input: %u, output: %u", inputSpeed, outputSpeed);
        
        // Speeds should be non-negative
        XCTAssertGreaterThanOrEqual((int)inputSpeed, 0, @"Input speed should be >= 0");
        XCTAssertGreaterThanOrEqual((int)outputSpeed, 0, @"Output speed should be >= 0");
    }
}

@end
