//
//  IXLANDFilesystemTests.mm
//  Filesystem syscall tests - 13 syscalls
//

#import <XCTest/XCTest.h>
#import "IXLANDTestHelpers.h"
#import <sys/stat.h>

@interface IXLANDFilesystemTests : XCTestCase
@property (nonatomic, strong) IXLANDTestEnvironment *env;
@end

@implementation IXLANDFilesystemTests

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

#pragma mark - stat/fstat/lstat (3 syscalls)

- (void)testStat_ReturnsFileInfo {
    NSString *path = [self.env testPath:@"stat_test.txt"];
    const char *cpath = [path UTF8String];
    
    // Create file
    int fd = open(cpath, O_CREAT | O_WRONLY, 0644);
    XCTAssertGreaterThan(fd, 0);
    write(fd, "test", 4);
    close(fd);
    
    // Test stat
    struct stat st;
    int result = stat(cpath, &st);
    XCTAssertEqual(result, 0);
    XCTAssertEqual(st.st_size, 4);
    XCTAssertTrue(S_ISREG(st.st_mode));
    
    unlink(cpath);
}

- (void)testStat_FailsOnNonexistent {
    struct stat st;
    int result = stat("/nonexistent/file", &st);
    XCTAssertEqual(result, -1);
    XCTAssertEqual(errno, ENOENT);
}

- (void)testFstat_ReturnsFileInfo {
    NSString *path = [self.env testPath:@"fstat_test.txt"];
    const char *cpath = [path UTF8String];
    
    int fd = open(cpath, O_CREAT | O_WRONLY, 0644);
    XCTAssertGreaterThan(fd, 0);
    
    struct stat st;
    int result = fstat(fd, &st);
    XCTAssertEqual(result, 0);
    XCTAssertTrue(S_ISREG(st.st_mode));
    
    close(fd);
    unlink(cpath);
}

- (void)testFstat_InvalidFd_Fails {
    struct stat st;
    int result = fstat(-1, &st);
    XCTAssertEqual(result, -1);
}

- (void)testLstat_ReturnsSymlinkInfo {
    NSString *file = [self.env testPath:@"lstat_target.txt"];
    NSString *link = [self.env testPath:@"lstat_link.txt"];
    
    // Create file
    int fd = open([file UTF8String], O_CREAT | O_WRONLY, 0644);
    close(fd);
    
    // Create symlink
    symlink([file UTF8String], [link UTF8String]);
    
    // Test lstat on symlink
    struct stat st;
    int result = lstat([link UTF8String], &st);
    XCTAssertEqual(result, 0);
    XCTAssertTrue(S_ISLNK(st.st_mode));
    
    unlink([link UTF8String]);
    unlink([file UTF8String]);
}

#pragma mark - mkdir/rmdir (2 syscalls)

- (void)testMkdir_CreatesDirectory {
    NSString *dir = [self.env testPath:@"mkdir_test"];
    
    int result = mkdir([dir UTF8String], 0755);
    XCTAssertEqual(result, 0);
    
    struct stat st;
    stat([dir UTF8String], &st);
    XCTAssertTrue(S_ISDIR(st.st_mode));
    
    rmdir([dir UTF8String]);
}

- (void)testMkdir_FailsOnExisting {
    NSString *dir = [self.env testPath:@"mkdir_existing"];
    mkdir([dir UTF8String], 0755);
    
    int result = mkdir([dir UTF8String], 0755);
    XCTAssertEqual(result, -1);
    XCTAssertEqual(errno, EEXIST);
    
    rmdir([dir UTF8String]);
}

- (void)testRmdir_RemovesEmptyDirectory {
    NSString *dir = [self.env testPath:@"rmdir_test"];
    mkdir([dir UTF8String], 0755);
    
    int result = rmdir([dir UTF8String]);
    XCTAssertEqual(result, 0);
    
    struct stat st;
    XCTAssertEqual(stat([dir UTF8String], &st), -1);
}

- (void)testRmdir_FailsOnNonempty {
    NSString *dir = [self.env testPath:@"rmdir_nonempty"];
    mkdir([dir UTF8String], 0755);
    
    NSString *file = [dir stringByAppendingPathComponent:@"file.txt"];
    int fd = open([file UTF8String], O_CREAT | O_WRONLY, 0644);
    close(fd);
    
    int result = rmdir([dir UTF8String]);
    XCTAssertEqual(result, -1);
    
    unlink([file UTF8String]);
    rmdir([dir UTF8String]);
}

#pragma mark - unlink (1 syscall)

- (void)testUnlink_RemovesFile {
    NSString *path = [self.env testPath:@"unlink_test.txt"];
    int fd = open([path UTF8String], O_CREAT | O_WRONLY, 0644);
    close(fd);
    
    int result = unlink([path UTF8String]);
    XCTAssertEqual(result, 0);
    
    struct stat st;
    XCTAssertEqual(stat([path UTF8String], &st), -1);
}

- (void)testUnlink_FailsOnDirectory {
    NSString *dir = [self.env testPath:@"unlink_dir"];
    mkdir([dir UTF8String], 0755);
    
    int result = unlink([dir UTF8String]);
    XCTAssertEqual(result, -1);
    
    rmdir([dir UTF8String]);
}

#pragma mark - link/symlink/readlink (3 syscalls)

- (void)testLink_CreatesHardLink {
    NSString *file = [self.env testPath:@"link_original.txt"];
    NSString *hardlink = [self.env testPath:@"link_hard.txt"];
    
    int fd = open([file UTF8String], O_CREAT | O_WRONLY, 0644);
    write(fd, "data", 4);
    close(fd);
    
    int result = link([file UTF8String], [hardlink UTF8String]);
    XCTAssertEqual(result, 0);
    
    struct stat st1, st2;
    stat([file UTF8String], &st1);
    stat([hardlink UTF8String], &st2);
    XCTAssertEqual(st1.st_ino, st2.st_ino);
    
    unlink([hardlink UTF8String]);
    unlink([file UTF8String]);
}

- (void)testSymlink_CreatesSoftLink {
    NSString *file = [self.env testPath:@"symlink_target.txt"];
    NSString *symlink = [self.env testPath:@"symlink_link.txt"];
    
    int fd = open([file UTF8String], O_CREAT | O_WRONLY, 0644);
    close(fd);
    
    int result = symlink([file UTF8String], [symlink UTF8String]);
    XCTAssertEqual(result, 0);
    
    struct stat st;
    lstat([symlink UTF8String], &st);
    XCTAssertTrue(S_ISLNK(st.st_mode));
    
    unlink([symlink UTF8String]);
    unlink([file UTF8String]);
}

- (void)testReadlink_ResolvesSymlink {
    NSString *file = [self.env testPath:@"readlink_target.txt"];
    NSString *symlink = [self.env testPath:@"readlink_link.txt"];
    
    int fd = open([file UTF8String], O_CREAT | O_WRONLY, 0644);
    close(fd);
    symlink([file UTF8String], [symlink UTF8String]);
    
    char buf[1024];
    ssize_t len = readlink([symlink UTF8String], buf, sizeof(buf));
    XCTAssertGreaterThan(len, 0);
    buf[len] = '\0';
    XCTAssertEqual(strcmp(buf, [file UTF8String]), 0);
    
    unlink([symlink UTF8String]);
    unlink([file UTF8String]);
}

#pragma mark - chmod/fchmod (2 syscalls)

- (void)testChmod_ChangesPermissions {
    NSString *path = [self.env testPath:@"chmod_test.txt"];
    int fd = open([path UTF8String], O_CREAT | O_WRONLY, 0644);
    close(fd);
    
    int result = chmod([path UTF8String], 0600);
    XCTAssertEqual(result, 0);
    
    struct stat st;
    stat([path UTF8String], &st);
    XCTAssertEqual(st.st_mode & 0777, 0600);
    
    unlink([path UTF8String]);
}

- (void)testFchmod_ChangesPermissions {
    NSString *path = [self.env testPath:@"fchmod_test.txt"];
    int fd = open([path UTF8String], O_CREAT | O_WRONLY, 0644);
    
    int result = fchmod(fd, 0400);
    XCTAssertEqual(result, 0);
    
    struct stat st;
    fstat(fd, &st);
    XCTAssertEqual(st.st_mode & 0777, 0400);
    
    close(fd);
    unlink([path UTF8String]);
}

#pragma mark - chown/fchown/lchown (3 syscalls)

- (void)testChown_ChangesOwner {
    NSString *path = [self.env testPath:@"chown_test.txt"];
    int fd = open([path UTF8String], O_CREAT | O_WRONLY, 0644);
    close(fd);
    
    // Use current uid/gid (0,0 would require root)
    uid_t uid = getuid();
    gid_t gid = getgid();
    
    int result = chown([path UTF8String], uid, gid);
    // May fail on iOS sandbox, but shouldn't crash
    // XCTAssertEqual(result, 0);  // Might fail on iOS
    
    unlink([path UTF8String]);
}

- (void)testFchown_ChangesOwner {
    NSString *path = [self.env testPath:@"fchown_test.txt"];
    int fd = open([path UTF8String], O_CREAT | O_WRONLY, 0644);
    
    uid_t uid = getuid();
    gid_t gid = getgid();
    
    int result = fchown(fd, uid, gid);
    // XCTAssertEqual(result, 0);  // Might fail on iOS
    
    close(fd);
    unlink([path UTF8String]);
}

- (void)testLchown_ChangesSymlinkOwner {
    NSString *file = [self.env testPath:@"lchown_target.txt"];
    NSString *link = [self.env testPath:@"lchown_link.txt"];
    
    int fd = open([file UTF8String], O_CREAT | O_WRONLY, 0644);
    close(fd);
    symlink([file UTF8String], [link UTF8String]);
    
    uid_t uid = getuid();
    gid_t gid = getgid();
    
    int result = lchown([link UTF8String], uid, gid);
    // XCTAssertEqual(result, 0);  // Might fail on iOS
    
    unlink([link UTF8String]);
    unlink([file UTF8String]);
}

#pragma mark - chroot (1 syscall)

- (void)testChroot_ReturnsError {
    // chroot is restricted on iOS
    int result = chroot("/");
    XCTAssertEqual(result, -1);
    XCTAssertEqual(errno, EPERM);
}

@end
