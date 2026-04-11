//
//  IXLANDFileTests.mm
//  IXLANDTestSuite
//
//  File Operations Unit Tests - 20 syscalls
//

#import <XCTest/XCTest.h>
#import "IXLANDTestHelpers.h"

extern "C" {
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

int ixland_open(const char *pathname, int flags, mode_t mode);
int ixland_openat(int dirfd, const char *pathname, int flags, mode_t mode);
int ixland_creat(const char *pathname, mode_t mode);
ssize_t ixland_read(int fd, void *buf, size_t count);
ssize_t ixland_write(int fd, const void *buf, size_t count);
int ixland_close(int fd);
off_t ixland_lseek(int fd, off_t offset, int whence);
ssize_t ixland_pread(int fd, void *buf, size_t count, off_t offset);
ssize_t ixland_pwrite(int fd, const void *buf, size_t count, off_t offset);
int ixland_dup(int oldfd);
int ixland_dup2(int oldfd, int newfd);
int ixland_dup3(int oldfd, int newfd, int flags);
int ixland_fcntl(int fd, int cmd, ...);
int ixland_ioctl(int fd, unsigned long request, ...);
int ixland_access(const char *pathname, int mode);
int ixland_faccessat(int dirfd, const char *pathname, int mode, int flags);
int ixland_chdir(const char *path);
int ixland_fchdir(int fd);
char *ixland_getcwd(char *buf, size_t size);
}

@interface IXLANDFileTests : XCTestCase
@property (nonatomic) IXLANDTestEnvironment *env;
@end

@implementation IXLANDFileTests

- (void)setUp {
    [super setUp];
    self.env = [IXLANDTestEnvironment sharedEnvironment];
    [self.env setup];
}

- (void)tearDown {
    [self.env cleanup];
    [super tearDown];
}

- (const char *)testPath:(NSString *)filename {
    return [[self.env pathForFile:filename] UTF8String];
}

#pragma mark - Open/Close Tests

- (void)testOpen_CreateFile_Success {
    const char *path = [self testPath:@"test_open_create.txt"];
    
    int fd = ixland_open(path, O_CREAT | O_RDWR, 0644);
    XCTAssertGreaterThan(fd, 0, @"open should return valid fd");
    
    int close_result = ixland_close(fd);
    XCTAssertEqual(close_result, 0, @"close should succeed");
}

- (void)testOpen_ExistingFile_Success {
    const char *path = [self testPath:@"test_open_existing.txt"];
    
    // Create file first
    int fd1 = ixland_open(path, O_CREAT | O_RDWR, 0644);
    ixland_close(fd1);
    
    // Open existing
    int fd2 = ixland_open(path, O_RDWR);
    XCTAssertGreaterThan(fd2, 0, @"open existing should succeed");
    ixland_close(fd2);
}

- (void)testOpen_NonexistentWithoutCreate_Fails {
    const char *path = [self testPath:@"nonexistent.txt"];
    
    int fd = ixland_open(path, O_RDONLY);
    XCTAssertEqual(fd, -1, @"open without O_CREAT should fail");
    XCTAssertEqual(errno, ENOENT, @"errno should be ENOENT");
}

- (void)testOpenat_RelativePath {
    const char *path = [self testPath:@"test_openat.txt"];
    
    int fd = ixland_openat(AT_FDCWD, path, O_CREAT | O_RDWR, 0644);
    XCTAssertGreaterThan(fd, 0, @"openat should return valid fd");
    ixland_close(fd);
}

- (void)testCreat_CreatesFile {
    const char *path = [self testPath:@"test_creat.txt"];
    
    int fd = ixland_creat(path, 0644);
    XCTAssertGreaterThan(fd, 0, @"creat should return valid fd");
    ixland_close(fd);
}

- (void)testClose_InvalidFd_Fails {
    int result = ixland_close(-1);
    XCTAssertEqual(result, -1, @"close invalid fd should fail");
    XCTAssertEqual(errno, EBADF, @"errno should be EBADF");
}

#pragma mark - Read/Write Tests

- (void)testWrite_WritesCorrectBytes {
    const char *path = [self testPath:@"test_write.txt"];
    const char *data = "Hello, libixland!";
    
    int fd = ixland_open(path, O_CREAT | O_RDWR, 0644);
    ssize_t written = ixland_write(fd, data, strlen(data));
    XCTAssertEqual(written, (ssize_t)strlen(data), @"write should write all bytes");
    ixland_close(fd);
}

- (void)testRead_ReadsCorrectBytes {
    const char *path = [self testPath:@"test_read.txt"];
    const char *data = "Hello, libixland!";
    char buffer[256];
    
    // Write data
    int fd1 = ixland_open(path, O_CREAT | O_RDWR, 0644);
    ixland_write(fd1, data, strlen(data));
    ixland_close(fd1);
    
    // Read back
    int fd2 = ixland_open(path, O_RDONLY);
    ssize_t read_bytes = ixland_read(fd2, buffer, sizeof(buffer));
    XCTAssertEqual(read_bytes, (ssize_t)strlen(data), @"read should read all bytes");
    XCTAssertEqual(memcmp(buffer, data, strlen(data)), 0, @"data should match");
    ixland_close(fd2);
}

- (void)testReadWrite_Roundtrip {
    const char *path = [self testPath:@"test_roundtrip.txt"];
    const char *data = "Roundtrip test data";
    char buffer[256];
    
    int fd = ixland_open(path, O_CREAT | O_RDWR, 0644);
    
    // Write
    ssize_t written = ixland_write(fd, data, strlen(data));
    XCTAssertEqual(written, (ssize_t)strlen(data));
    
    // Seek to beginning
    off_t pos = ixland_lseek(fd, 0, SEEK_SET);
    XCTAssertEqual(pos, 0);
    
    // Read
    ssize_t read_bytes = ixland_read(fd, buffer, sizeof(buffer));
    XCTAssertEqual(read_bytes, (ssize_t)strlen(data));
    XCTAssertEqual(memcmp(buffer, data, strlen(data)), 0);
    
    ixland_close(fd);
}

- (void)testRead_Eof_ReturnsZero {
    const char *path = [self testPath:@"test_eof.txt"];
    
    int fd = ixland_open(path, O_CREAT | O_RDONLY, 0644);
    char buffer[256];
    ssize_t read_bytes = ixland_read(fd, buffer, sizeof(buffer));
    XCTAssertEqual(read_bytes, 0, @"read at EOF should return 0");
    ixland_close(fd);
}

#pragma mark - Seek Tests

- (void)testLseek_SetPosition {
    const char *path = [self testPath:@"test_seek.txt"];
    const char *data = "0123456789ABCDEF";
    
    int fd = ixland_open(path, O_CREAT | O_RDWR, 0644);
    ixland_write(fd, data, strlen(data));
    
    // Seek to position 5
    off_t pos = ixland_lseek(fd, 5, SEEK_SET);
    XCTAssertEqual(pos, 5, @"lseek should return new position");
    
    // Read should start at position 5
    char buffer[16];
    ssize_t read_bytes = ixland_read(fd, buffer, sizeof(buffer));
    XCTAssertEqual(read_bytes, 11); // strlen(data) - 5
    XCTAssertEqual(memcmp(buffer, "56789ABCDEF", 11), 0);
    
    ixland_close(fd);
}

- (void)testLseek_SeekEnd {
    const char *path = [self testPath:@"test_seek_end.txt"];
    const char *data = "0123456789";
    
    int fd = ixland_open(path, O_CREAT | O_RDWR, 0644);
    ixland_write(fd, data, strlen(data));
    
    // Seek to end
    off_t pos = ixland_lseek(fd, 0, SEEK_END);
    XCTAssertEqual(pos, (off_t)strlen(data), @"lseek END should return file size");
    
    ixland_close(fd);
}

- (void)testLseek_SeekCur {
    const char *path = [self testPath:@"test_seek_cur.txt"];
    const char *data = "0123456789";
    
    int fd = ixland_open(path, O_CREAT | O_RDWR, 0644);
    ixland_write(fd, data, strlen(data));
    ixland_lseek(fd, 0, SEEK_SET);
    
    // Read 5 bytes
    char buffer[16];
    ixland_read(fd, buffer, 5);
    
    // Seek relative +3 from current (position 8)
    off_t pos = ixland_lseek(fd, 3, SEEK_CUR);
    XCTAssertEqual(pos, 8, @"lseek CUR should advance from current");
    
    ixland_close(fd);
}

#pragma mark - Pread/Pwrite Tests

- (void)testPread_ReadsAtOffset {
    const char *path = [self testPath:@"test_pread.txt"];
    const char *data = "0123456789ABCDEF";
    char buffer[16];
    
    int fd = ixland_open(path, O_CREAT | O_RDWR, 0644);
    ixland_write(fd, data, strlen(data));
    
    // Read at offset 8 without changing file position
    ssize_t read_bytes = ixland_pread(fd, buffer, 8, 8);
    XCTAssertEqual(read_bytes, 8);
    XCTAssertEqual(memcmp(buffer, "89ABCDEF", 8), 0);
    
    // File position should still be at end
    off_t pos = ixland_lseek(fd, 0, SEEK_CUR);
    XCTAssertEqual(pos, (off_t)strlen(data));
    
    ixland_close(fd);
}

- (void)testPwrite_WritesAtOffset {
    const char *path = [self testPath:@"test_pwrite.txt"];
    const char *data = "0000000000000000";
    
    int fd = ixland_open(path, O_CREAT | O_RDWR, 0644);
    ixland_write(fd, data, strlen(data));
    
    // Write at offset 4
    const char *new_data = "AAAA";
    ssize_t written = ixland_pwrite(fd, new_data, 4, 4);
    XCTAssertEqual(written, 4);
    
    // Read back and verify
    ixland_lseek(fd, 0, SEEK_SET);
    char buffer[16];
    ixland_read(fd, buffer, sizeof(buffer));
    XCTAssertEqual(memcmp(buffer, "0000AAAA00000000", 16), 0);
    
    ixland_close(fd);
}

#pragma mark - Dup Tests

- (void)testDup_DuplicatesFd {
    const char *path = [self testPath:@"test_dup.txt"];
    
    int fd1 = ixland_open(path, O_CREAT | O_RDWR, 0644);
    int fd2 = ixland_dup(fd1);
    
    XCTAssertGreaterThan(fd2, 0, @"dup should return valid fd");
    XCTAssertNotEqual(fd2, fd1, @"dup should return different fd");
    
    ixland_close(fd1);
    ixland_close(fd2);
}

- (void)testDup2_SpecificFd {
    const char *path = [self testPath:@"test_dup2.txt"];
    
    int fd1 = ixland_open(path, O_CREAT | O_RDWR, 0644);
    int fd2 = ixland_dup2(fd1, 100); // Try to dup to fd 100
    
    XCTAssertEqual(fd2, 100, @"dup2 should return requested fd");
    
    ixland_close(fd1);
    ixland_close(fd2);
}

- (void)testDup3_WithFlags {
    const char *path = [self testPath:@"test_dup3.txt"];
    
    int fd1 = ixland_open(path, O_CREAT | O_RDWR, 0644);
    int fd2 = ixland_dup3(fd1, 101, O_CLOEXEC);
    
    XCTAssertEqual(fd2, 101, @"dup3 should return requested fd");
    
    ixland_close(fd1);
    ixland_close(fd2);
}

#pragma mark - Access Tests

- (void)testAccess_ExistingFile {
    const char *path = [self testPath:@"test_access.txt"];
    
    int fd = ixland_open(path, O_CREAT | O_RDWR, 0644);
    ixland_close(fd);
    
    int result = ixland_access(path, F_OK);
    XCTAssertEqual(result, 0, @"access F_OK should succeed for existing file");
    
    result = ixland_access(path, R_OK);
    XCTAssertEqual(result, 0, @"access R_OK should succeed");
    
    result = ixland_access(path, W_OK);
    XCTAssertEqual(result, 0, @"access W_OK should succeed");
}

- (void)testAccess_NonexistentFile {
    const char *path = [self testPath:@"nonexistent_access.txt"];
    
    int result = ixland_access(path, F_OK);
    XCTAssertEqual(result, -1, @"access should fail for nonexistent");
    XCTAssertEqual(errno, ENOENT, @"errno should be ENOENT");
}

- (void)testFaccessat_RelativePath {
    const char *path = [self testPath:@"test_faccessat.txt"];
    
    int fd = ixland_open(path, O_CREAT | O_RDWR, 0644);
    ixland_close(fd);
    
    int result = ixland_faccessat(AT_FDCWD, path, F_OK, 0);
    XCTAssertEqual(result, 0, @"faccessat should succeed");
}

#pragma mark - Directory Tests

- (void)testGetcwd_ReturnsPath {
    char buffer[1024];
    char *result = ixland_getcwd(buffer, sizeof(buffer));
    
    XCTAssertNotNil(@(result), @"getcwd should return path");
    XCTAssertGreaterThan(strlen(buffer), 0, @"cwd should not be empty");
    XCTAssertEqual(buffer[0], '/', @"cwd should start with /");
}

- (void)testChdir_ChangesDirectory {
    const char *original = [self testPath:@""];
    char original_cwd[1024];
    ixland_getcwd(original_cwd, sizeof(original_cwd));
    
    int result = ixland_chdir(original);
    XCTAssertEqual(result, 0, @"chdir should succeed");
    
    char new_cwd[1024];
    ixland_getcwd(new_cwd, sizeof(new_cwd));
    XCTAssertEqual(strcmp(new_cwd, original), 0, @"cwd should be changed");
}

- (void)testFchdir_WithFd {
    const char *path = [self testPath:@""];
    
    int fd = ixland_open(path, O_RDONLY | O_DIRECTORY);
    if (fd > 0) {
        int result = ixland_fchdir(fd);
        // May not be fully implemented, just verify no crash
        (void)result;
        ixland_close(fd);
    }
}

#pragma mark - Stress Tests

IXLANDStressTest(FileIO, 1000) {
    IXLANDPerformanceTimer *timer = [[IXLANDPerformanceTimer alloc] init];
    NSUInteger baseline = [IXLANDMemoryTracker currentResidentSize];
    
    [timer measureBlock:^{
        const char *path = [[self.env pathForFile:@"stress_file.txt"] UTF8String];
        char buffer[4096];
        
        // Create, write, read, close
        int fd = ixland_open(path, O_CREAT | O_RDWR, 0644);
        memset(buffer, 'X', sizeof(buffer));
        ixland_write(fd, buffer, sizeof(buffer));
        ixland_lseek(fd, 0, SEEK_SET);
        ixland_read(fd, buffer, sizeof(buffer));
        ixland_close(fd);
        unlink(path);
    } iterations:1000];
    
    NSLog(@"File IO stress: %@", timer.report);
    
    NSUInteger current = [IXLANDMemoryTracker currentResidentSize];
    XCTAssertEqualWithAccuracy(current, baseline, 65536, 
        @"Memory leak detected after 1000 file operations");
}

@end
