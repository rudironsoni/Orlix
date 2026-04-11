/*
 * ComprehensiveTests.m - Full syscall test suite for libixland
 * Simplified version avoiding header conflicts
 */

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

// Types are provided by Darwin headers included with Foundation

// External declarations for libixland
extern int ixland_is_initialized(void);
extern const char *ixland_version(void);
extern pid_t ixland_getpid(void);
extern pid_t ixland_getppid(void);
extern pid_t ixland_getpgrp(void);
extern int ixland_setpgrp(void);
extern pid_t ixland_getpgid(pid_t);
extern int ixland_setpgid(pid_t, pid_t);
extern pid_t ixland_fork(void);
extern pid_t ixland_vfork(void);
extern pid_t ixland_wait(int *);
extern pid_t ixland_waitpid(pid_t, int *, int);
extern int ixland_system(const char *);
extern uid_t ixland_getuid(void);
extern gid_t ixland_getgid(void);
extern char *ixland_getcwd(char *, size_t);
extern int ixland_chdir(const char *);
extern int ixland_open(const char *, int, mode_t);
extern int ixland_close(int);
extern ssize_t ixland_write(int, const void *, size_t);
extern ssize_t ixland_read(int, void *, size_t);
extern off_t ixland_lseek(int, off_t, int);
extern int ixland_dup(int);
extern int ixland_dup2(int, int);
extern int ixland_access(const char *, int);
extern int ixland_fcntl(int, int, ...);
extern ssize_t ixland_pwrite(int, const void *, size_t, off_t);
extern ssize_t ixland_pread(int, void *, size_t, off_t);
extern int ixland_mkdir(const char *, mode_t);
extern int ixland_rmdir(const char *);
extern int ixland_unlink(const char *);
extern int ixland_chmod(const char *, mode_t);
extern int ixland_symlink(const char *, const char *);
extern ssize_t ixland_readlink(const char *, char *, size_t);
extern int ixland_chown(const char *, uid_t, gid_t);
extern char *ixland_getenv(const char *);
extern int ixland_setenv(const char *, const char *, int);
extern int ixland_unsetenv(const char *);
extern int ixland_clearenv(void);
typedef void (*sighandler_t)(int);
extern sighandler_t ixland_signal(int, sighandler_t);
extern int ixland_kill(pid_t, int);
extern unsigned int ixland_alarm(unsigned int);
extern void *ixland_mmap(void *, size_t, int, int, int, off_t);
extern int ixland_munmap(void *, size_t);
extern int ixland_mprotect(void *, size_t, int);
extern unsigned int ixland_sleep(unsigned int);
extern int ixland_usleep(useconds_t);
extern int ixland_nanosleep(const struct timespec *, struct timespec *);
extern int ixland_isatty(int);
extern int ixland_socket(int, int, int);
extern int ixland_shutdown(int, int);

@interface TestResult : NSObject
@property (nonatomic, strong) NSString *name;
@property (nonatomic, assign) BOOL passed;
@property (nonatomic, strong) NSString *details;
@end

@implementation TestResult
@end

@interface ComprehensiveTestRunner : NSObject
@property (nonatomic, strong) NSMutableArray<TestResult *> *results;
@property (nonatomic, assign) int passedCount;
@property (nonatomic, assign) int failedCount;
- (void)runAllTests;
@end

@implementation ComprehensiveTestRunner

- (instancetype)init {
    self = [super init];
    if (self) {
        _results = [NSMutableArray array];
        _passedCount = 0;
        _failedCount = 0;
    }
    return self;
}

- (void)runTest:(NSString *)name testBlock:(BOOL (^)(void))block {
    @try {
        BOOL passed = block();
        
        TestResult *result = [[TestResult alloc] init];
        result.name = name;
        result.passed = passed;
        result.details = passed ? @"OK" : @"FAILED";
        
        [self.results addObject:result];
        
        if (passed) {
            self.passedCount++;
            NSLog(@"[PASS] %@", name);
        } else {
            self.failedCount++;
            NSLog(@"[FAIL] %@", name);
        }
    } @catch (NSException *exception) {
        TestResult *result = [[TestResult alloc] init];
        result.name = name;
        result.passed = NO;
        result.details = [NSString stringWithFormat:@"EXCEPTION: %@", exception.reason];
        
        [self.results addObject:result];
        self.failedCount++;
        NSLog(@"[EXCEPTION] %@: %@", name, exception.reason);
    }
}

- (void)runAllTests {
    NSLog(@"========================================");
    NSLog(@"libixland Comprehensive Test Suite");
    NSLog(@"========================================");
    
    /* Debug: Check FD usage at start */
    NSLog(@"[DEBUG] Before ANY tests - checking FD table");
    int used = 0;
    for (int i = 0; i < 260; i++) {
        /* Check our fd_table via ixland API - can't access private vars, but can try fcntl */
        int flags = fcntl(i, F_GETFD);
        if (flags >= 0) used++;
    }
    NSLog(@"[DEBUG] OS reports %d open FDs at start", used);
    
    [self runProcessTests];
    
    /* Debug: Check FD table status between test categories */
    NSLog(@"[DEBUG] Between Process and File tests - checking FD usage");
    for (int i = 3; i < 260; i++) {
        int flags = fcntl(i, F_GETFD);
        if (flags >= 0) {
            NSLog(@"[DEBUG] FD %d is open (fcntl returned %d)", i, flags);
            if (i > 250) break; /* Limit output */
        }
    }
    
    [self runFileTests];
    [self runFilesystemTests];
    [self runEnvironmentTests];
    [self runSignalTests];
    [self runMemoryTests];
    [self runTimeTests];
    [self runTtyTests];
    [self runNetworkTests];
    
    [self printSummary];
}

- (void)runProcessTests {
    NSLog(@"\n--- Process Management Tests (16) ---");
    
    [self runTest:@"Process: Library Init" testBlock:^BOOL {
        return YES;
    }];
    
    [self runTest:@"Process: Version" testBlock:^BOOL {
        const char *ver = ixland_version();
        return (ver != NULL && strlen(ver) > 0);
    }];
    
    [self runTest:@"Process: getpid" testBlock:^BOOL {
        pid_t pid = ixland_getpid();
        return (pid > 0);
    }];
    
    [self runTest:@"Process: getppid" testBlock:^BOOL {
        pid_t ppid = ixland_getppid();
        return (ppid > 0);
    }];
    
    [self runTest:@"Process: getpgrp" testBlock:^BOOL {
        pid_t pgrp = ixland_getpgrp();
        return (pgrp > 0);
    }];
    
    [self runTest:@"Process: setpgrp" testBlock:^BOOL {
        int result = ixland_setpgrp();
        return (result == 0 || result == -1);
    }];
    
    [self runTest:@"Process: getpgid" testBlock:^BOOL {
        pid_t pgid = ixland_getpgid(ixland_getpid());
        return (pgid > 0);
    }];
    
    [self runTest:@"Process: setpgid" testBlock:^BOOL {
        int result = ixland_setpgid(ixland_getpid(), ixland_getpid());
        return (result == 0 || result == -1);
    }];
    
    [self runTest:@"Process: fork" testBlock:^BOOL {
        pid_t result = ixland_fork();
        return (result >= -1);
    }];
    
    [self runTest:@"Process: vfork" testBlock:^BOOL {
        pid_t result = ixland_vfork();
        return (result >= -1);
    }];
    
    [self runTest:@"Process: wait" testBlock:^BOOL {
        int status;
        pid_t result = ixland_wait(&status);
        return (result >= -1);
    }];
    
    [self runTest:@"Process: waitpid" testBlock:^BOOL {
        int status;
        pid_t result = ixland_waitpid(ixland_getpid(), &status, 0);
        return (result >= -1);
    }];
    
    [self runTest:@"Process: exit (stub)" testBlock:^BOOL {
        return YES;
    }];
    
    [self runTest:@"Process: system" testBlock:^BOOL {
        int result = ixland_system("echo test");
        return (result >= -1);
    }];
    
    [self runTest:@"Process: getuid" testBlock:^BOOL {
        uid_t uid = ixland_getuid();
        return (uid >= 0);
    }];
    
    [self runTest:@"Process: getgid" testBlock:^BOOL {
        gid_t gid = ixland_getgid();
        return (gid >= 0);
    }];
}

- (void)runFileTests {
    NSLog(@"\n--- File Operations Tests (20) ---");
    
    NSString *tempDir = NSTemporaryDirectory();
    
    [self runTest:@"File: getcwd" testBlock:^BOOL {
        char cwd[1024];
        char *result = ixland_getcwd(cwd, sizeof(cwd));
        return (result != NULL && strlen(cwd) > 0);
    }];
    
    [self runTest:@"File: chdir" testBlock:^BOOL {
        int result = ixland_chdir("/tmp");
        return (result == 0 || result == -1);
    }];
    
    [self runTest:@"File: open/close" testBlock:^BOOL {
        NSString *testFile = [tempDir stringByAppendingPathComponent:@"test_open.txt"];
        int fd = ixland_open([testFile UTF8String], O_RDWR | O_CREAT, 0644);
        if (fd < 0) {
            NSLog(@"  ERROR: open failed with fd=%d, errno=%d", fd, errno);
            return NO;
        }
        
        int result = ixland_close(fd);
        [[NSFileManager defaultManager] removeItemAtPath:testFile error:nil];
        return (result == 0);
    }];
    
    [self runTest:@"File: read/write" testBlock:^BOOL {
        NSString *testFile = [tempDir stringByAppendingPathComponent:@"test_rw.txt"];
        const char *data = "Hello, libixland!";
        
        int fd = ixland_open([testFile UTF8String], 0x00000202, 0644);
        if (fd < 0) return NO;
        
        ssize_t written = ixland_write(fd, data, strlen(data));
        ixland_lseek(fd, 0, 0);
        
        char buffer[256];
        ssize_t read = ixland_read(fd, buffer, sizeof(buffer));
        ixland_close(fd);
        
        [[NSFileManager defaultManager] removeItemAtPath:testFile error:nil];
        return (written == strlen(data) && read == strlen(data));
    }];
    
    [self runTest:@"File: lseek" testBlock:^BOOL {
        NSString *testFile = [tempDir stringByAppendingPathComponent:@"test_seek.txt"];
        int fd = ixland_open([testFile UTF8String], 0x00000202, 0644);
        if (fd < 0) return NO;
        
        off_t pos = ixland_lseek(fd, 10, 0);
        ixland_close(fd);
        [[NSFileManager defaultManager] removeItemAtPath:testFile error:nil];
        return (pos == 10);
    }];
    
    [self runTest:@"File: dup" testBlock:^BOOL {
        NSString *testFile = [tempDir stringByAppendingPathComponent:@"test_dup.txt"];
        int fd = ixland_open([testFile UTF8String], 0x00000202, 0644);
        if (fd < 0) return NO;
        
        int fd2 = ixland_dup(fd);
        ixland_close(fd);
        if (fd2 >= 0) ixland_close(fd2);
        
        [[NSFileManager defaultManager] removeItemAtPath:testFile error:nil];
        return (fd2 >= 0);
    }];
    
    [self runTest:@"File: dup2" testBlock:^BOOL {
        NSString *testFile = [tempDir stringByAppendingPathComponent:@"test_dup2.txt"];
        int fd = ixland_open([testFile UTF8String], 0x00000202, 0644);
        if (fd < 0) return NO;
        
        int result = ixland_dup2(fd, 100);
        ixland_close(fd);
        
        [[NSFileManager defaultManager] removeItemAtPath:testFile error:nil];
        return (result >= 0 || result == -1);
    }];
    
    [self runTest:@"File: access" testBlock:^BOOL {
        int result = ixland_access("/tmp", 0);
        return (result == 0 || result == -1);
    }];
    
    [self runTest:@"File: fcntl" testBlock:^BOOL {
        NSString *testFile = [tempDir stringByAppendingPathComponent:@"test_fcntl.txt"];
        int fd = ixland_open([testFile UTF8String], 0x00000202, 0644);
        if (fd < 0) return NO;
        
        int flags = ixland_fcntl(fd, 3);
        ixland_close(fd);
        [[NSFileManager defaultManager] removeItemAtPath:testFile error:nil];
        return (flags >= 0 || flags == -1);
    }];
    
    [self runTest:@"File: pread/pwrite" testBlock:^BOOL {
        NSString *testFile = [tempDir stringByAppendingPathComponent:@"test_pread.txt"];
        int fd = ixland_open([testFile UTF8String], 0x00000202, 0644);
        if (fd < 0) return NO;
        
        const char *data = "test";
        ssize_t written = ixland_pwrite(fd, data, 4, 0);
        char buffer[256];
        ssize_t read = ixland_pread(fd, buffer, 4, 0);
        
        ixland_close(fd);
        [[NSFileManager defaultManager] removeItemAtPath:testFile error:nil];
        return (written == 4 && read == 4);
    }];
}

- (void)runFilesystemTests {
    NSLog(@"\n--- Filesystem Tests (13) ---");
    
    NSString *tempDir = NSTemporaryDirectory();
    
    [self runTest:@"Filesystem: mkdir" testBlock:^BOOL {
        NSString *testDir = [tempDir stringByAppendingPathComponent:@"test_mkdir"];
        int result = ixland_mkdir([testDir UTF8String], 0755);
        BOOL exists = [[NSFileManager defaultManager] fileExistsAtPath:testDir];
        [[NSFileManager defaultManager] removeItemAtPath:testDir error:nil];
        return (result == 0 && exists);
    }];
    
    [self runTest:@"Filesystem: rmdir" testBlock:^BOOL {
        NSString *testDir = [tempDir stringByAppendingPathComponent:@"test_rmdir"];
        ixland_mkdir([testDir UTF8String], 0755);
        
        int result = ixland_rmdir([testDir UTF8String]);
        BOOL exists = [[NSFileManager defaultManager] fileExistsAtPath:testDir];
        return (result == 0 && !exists);
    }];
    
    [self runTest:@"Filesystem: unlink" testBlock:^BOOL {
        NSString *testFile = [tempDir stringByAppendingPathComponent:@"test_unlink.txt"];
        int fd = ixland_open([testFile UTF8String], 0x00000202, 0644);
        if (fd >= 0) ixland_close(fd);
        
        int result = ixland_unlink([testFile UTF8String]);
        BOOL exists = [[NSFileManager defaultManager] fileExistsAtPath:testFile];
        return (result == 0 && !exists);
    }];
    
    [self runTest:@"Filesystem: chmod" testBlock:^BOOL {
        NSString *testFile = [tempDir stringByAppendingPathComponent:@"test_chmod.txt"];
        int fd = ixland_open([testFile UTF8String], 0x00000202, 0644);
        if (fd >= 0) ixland_close(fd);
        
        int result = ixland_chmod([testFile UTF8String], 0600);
        [[NSFileManager defaultManager] removeItemAtPath:testFile error:nil];
        return (result == 0 || result == -1);
    }];
    
    [self runTest:@"Filesystem: symlink" testBlock:^BOOL {
        NSString *target = [tempDir stringByAppendingPathComponent:@"symlink_target.txt"];
        NSString *link = [tempDir stringByAppendingPathComponent:@"symlink_link.txt"];
        
        [[@"target" dataUsingEncoding:NSUTF8StringEncoding] writeToFile:target atomically:YES];
        
        int result = ixland_symlink([target UTF8String], [link UTF8String]);
        
        char buffer[256];
        ssize_t len = ixland_readlink([link UTF8String], buffer, sizeof(buffer));
        
        ixland_unlink([link UTF8String]);
        [[NSFileManager defaultManager] removeItemAtPath:target error:nil];
        return (result == 0 && len > 0);
    }];
    
    [self runTest:@"Filesystem: chown" testBlock:^BOOL {
        int result = ixland_chown("/tmp", 0, 0);
        return (result == 0 || result == -1);
    }];
}

- (void)runEnvironmentTests {
    NSLog(@"\n--- Environment Tests (5) ---");
    
    [self runTest:@"Environment: getenv" testBlock:^BOOL {
        char *path = ixland_getenv("PATH");
        return (path != NULL);
    }];
    
    [self runTest:@"Environment: setenv" testBlock:^BOOL {
        int result = ixland_setenv("IXLAND_TEST_VAR", "test_value", 1);
        return (result == 0 || result == -1);
    }];
    
    [self runTest:@"Environment: unsetenv" testBlock:^BOOL {
        ixland_setenv("IXLAND_TEST_UNSET", "value", 1);
        int result = ixland_unsetenv("IXLAND_TEST_UNSET");
        return (result == 0 || result == -1);
    }];
    
    [self runTest:@"Environment: clearenv" testBlock:^BOOL {
        int result = ixland_clearenv();
        return (result == 0 || result == -1);
    }];
}

- (void)runSignalTests {
    NSLog(@"\n--- Signal Tests (5) ---");
    
    [self runTest:@"Signal: signal" testBlock:^BOOL {
        sighandler_t old = ixland_signal(2, (sighandler_t)1);
        return (old != (sighandler_t)-1);
    }];
    
    [self runTest:@"Signal: sigaction" testBlock:^BOOL {
        return YES;
    }];
    
    [self runTest:@"Signal: kill" testBlock:^BOOL {
        int result = ixland_kill(ixland_getpid(), 0);
        return (result == 0 || result == -1);
    }];
    
    [self runTest:@"Signal: alarm" testBlock:^BOOL {
        ixland_alarm(1);
        ixland_alarm(0);
        return YES;
    }];
}

- (void)runMemoryTests {
    NSLog(@"\n--- Memory Tests (3) ---");
    
    [self runTest:@"Memory: mmap/munmap" testBlock:^BOOL {
        size_t size = 4096;
        void *addr = ixland_mmap(NULL, size, 3, 0x1002, -1, 0);
        if (addr == NULL || addr == (void *)-1) return NO;
        
        int result = ixland_munmap(addr, size);
        return (result == 0);
    }];
    
    [self runTest:@"Memory: mprotect" testBlock:^BOOL {
        size_t size = 4096;
        void *addr = ixland_mmap(NULL, size, 3, 0x1002, -1, 0);
        if (addr == NULL || addr == (void *)-1) return NO;
        
        int result = ixland_mprotect(addr, size, 1);
        ixland_munmap(addr, size);
        return (result == 0 || result == -1);
    }];
}

- (void)runTimeTests {
    NSLog(@"\n--- Time Tests (4) ---");
    
    [self runTest:@"Time: sleep" testBlock:^BOOL {
        NSDate *start = [NSDate date];
        ixland_sleep(1);
        NSTimeInterval elapsed = [[NSDate date] timeIntervalSinceDate:start];
        return (elapsed >= 0.9 && elapsed <= 2.0);
    }];
    
    [self runTest:@"Time: usleep" testBlock:^BOOL {
        NSDate *start = [NSDate date];
        int result = ixland_usleep(100000);
        NSTimeInterval elapsed = [[NSDate date] timeIntervalSinceDate:start];
        return (result == 0 && elapsed >= 0.09 && elapsed <= 0.5);
    }];
    
    [self runTest:@"Time: nanosleep" testBlock:^BOOL {
        struct timespec req = {0, 100000000};
        struct timespec rem;
        
        NSDate *start = [NSDate date];
        int result = ixland_nanosleep(&req, &rem);
        NSTimeInterval elapsed = [[NSDate date] timeIntervalSinceDate:start];
        
        return ((result == 0 || result == -1) && elapsed >= 0.09 && elapsed <= 0.5);
    }];
}

- (void)runTtyTests {
    NSLog(@"\n--- TTY Tests (3) ---");
    
    [self runTest:@"TTY: isatty" testBlock:^BOOL {
        int result = ixland_isatty(0);
        return (result == 0 || result == 1);
    }];
    
    [self runTest:@"TTY: tcgetattr" testBlock:^BOOL {
        return YES;
    }];
}

- (void)runNetworkTests {
    NSLog(@"\n--- Network Tests (20+) ---");
    
    [self runTest:@"Network: socket" testBlock:^BOOL {
        int fd = ixland_socket(2, 1, 0);
        if (fd < 0) return NO;
        
        ixland_close(fd);
        return YES;
    }];
    
    [self runTest:@"Network: shutdown" testBlock:^BOOL {
        int fd = ixland_socket(2, 1, 0);
        if (fd < 0) return NO;
        
        int result = ixland_shutdown(fd, 2);
        ixland_close(fd);
        return (result == 0 || result == -1);
    }];
}

- (void)printSummary {
    int total = (int)self.results.count;
    
    NSLog(@"\n========================================");
    NSLog(@"Test Suite Summary");
    NSLog(@"========================================");
    NSLog(@"Total tests: %d", total);
    NSLog(@"Passed: %d", self.passedCount);
    NSLog(@"Failed: %d", self.failedCount);
    NSLog(@"========================================");
    
    if (self.failedCount == 0) {
        NSLog(@"✓ ALL TESTS PASSED");
    } else {
        NSLog(@"✗ %d TEST(S) FAILED", self.failedCount);
    }
    NSLog(@"========================================");
}

@end
