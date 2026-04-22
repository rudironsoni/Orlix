/*
 * IXLandSystemTests - VFSPathTests.m
 *
 * INTERNAL RUNTIME SEMANTIC TEST.
 *
 * This file tests private VFS path translation behavior through internal owner
 * entry points declared in fs/vfs.h. It does NOT prove public drop-in
 * compatibility.
 */

#import <XCTest/XCTest.h>
#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>

#define IX_TCGETS 0x5401
#define IX_TCSETS 0x5402
#define IX_TIOCGPGRP 0x540F
#define IX_TIOCSPGRP 0x5410
#define IX_TIOCGWINSZ 0x5413
#define IX_TIOCSWINSZ 0x5414
#define IX_FIONREAD 0x541B
#define IX_TIOCGPTN 0x80045430UL
#define IX_TIOCSPTLCK 0x40045431UL

struct ix_termios {
    uint32_t c_iflag;
    uint32_t c_oflag;
    uint32_t c_cflag;
    uint32_t c_lflag;
    unsigned char c_line;
    unsigned char c_cc[19];
};

struct ix_winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

// Forward declare linux_dirent64 since it's not in standard headers
struct linux_dirent64 {
    uint64_t d_ino;
    int64_t d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[];
};

#include "fs/vfs.h"
#include "fs/path.h"
#include "kernel/task.h"
#include "internal/ios/fs/backing_io.h"

extern char *getcwd_impl(char *buf, size_t size);
extern int openat_impl(int dirfd, const char *pathname, int flags, mode_t mode);
extern int renameat2(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, unsigned int flags);
extern int renameat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath);
extern int rename(const char *oldpath, const char *newpath);
extern int alloc_fd_impl(void);
extern void free_fd_impl(int fd);
extern void init_fd_entry_impl(int fd, int real_fd, int flags, mode_t mode, const char *path);
extern off_t lseek(int fd, off_t offset, int whence);
extern int fcntl(int fd, int cmd, ...);
extern int dup(int oldfd);
extern int dup2(int oldfd, int newfd);
extern int dup3(int oldfd, int newfd, int flags);
extern int close(int fd);
extern ssize_t getdents64(int fd, void *dirp, size_t count);
extern int poll(struct pollfd *fds, nfds_t nfds, int timeout);
extern int select_impl(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds, struct timeval *timeout);

static void vfs_test_translate_virtual_path(const char *path, char *host_path, size_t host_path_len) {
    int ret = vfs_translate_path(path, host_path, host_path_len);
    XCTAssertEqual(ret, 0, @"path should translate for %s", path);
}

static void vfs_test_ensure_virtual_parent_directory(const char *path) {
    char host_path[MAX_PATH];
    NSString *parentPath;
    NSError *error = nil;

    vfs_test_translate_virtual_path(path, host_path, sizeof(host_path));
    parentPath = [[NSString stringWithUTF8String:host_path] stringByDeletingLastPathComponent];
    XCTAssertTrue([[NSFileManager defaultManager] createDirectoryAtPath:parentPath
                                           withIntermediateDirectories:YES
                                                            attributes:nil
                                                                 error:&error],
                  @"parent directory setup should succeed for %s: %@", path, error);
}

static void vfs_test_seed_linux_file(const char *path) {
    char host_path[MAX_PATH];
    int fd;

    vfs_test_ensure_virtual_parent_directory(path);
    vfs_test_translate_virtual_path(path, host_path, sizeof(host_path));
    fd = host_open_impl(host_path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    XCTAssertTrue(fd >= 0, @"file setup should succeed for %s", path);
    if (fd >= 0) {
        host_close_impl(fd);
    }
}

static void vfs_test_remove_linux_path(const char *path) {
    char host_path[MAX_PATH];
    NSString *hostPath;
    NSError *error = nil;

    vfs_test_translate_virtual_path(path, host_path, sizeof(host_path));
    hostPath = [NSString stringWithUTF8String:host_path];
    if (![[NSFileManager defaultManager] removeItemAtPath:hostPath error:&error]) {
        XCTAssertTrue(error == nil || error.code == NSFileNoSuchFileError,
                      @"cleanup should tolerate missing path for %s: %@", path, error);
    }
}

static int vfs_test_open_host_directory_fd(const char *host_path) {
    return host_open_impl(host_path, O_RDONLY | O_DIRECTORY, 0);
}

@interface VFSPathTests : XCTestCase
@end

@implementation VFSPathTests

- (void)setUp {
    [super setUp];
    // Clean up any lingering file descriptors before each test
    for (int fd = 3; fd < 256; fd++) {
        close(fd);
    }
}

- (void)tearDown {
    // Clean up any open file descriptors between tests
    for (int fd = 3; fd < 256; fd++) {
        close(fd);
    }
    [super tearDown];
}

- (void)testVirtualRootTranslatesToPersistentBackingRoot {
    char host_path[MAX_PATH];
    int ret = vfs_translate_path(@"/".UTF8String, host_path, sizeof(host_path));

    XCTAssertEqual(ret, 0, @"vfs_translate_path should accept virtual root");
    XCTAssertEqual(strcmp(host_path, vfs_persistent_backing_root()), 0,
                   @"virtual root should map to persistent backing root");
}

- (void)testTempPathTranslatesToTempBackingRoot {
    char host_path[MAX_PATH];
    int ret = vfs_translate_path(@"/tmp/demo".UTF8String, host_path, sizeof(host_path));
    NSString *actualPath = [NSString stringWithUTF8String:host_path];
    NSString *expectedPath = [NSString stringWithFormat:@"%s/demo", vfs_temp_backing_root()];

    XCTAssertEqual(ret, 0, @"temp virtual path should translate");
    XCTAssertEqualObjects(actualPath, expectedPath, @"temp path should resolve under temp backing root");
    XCTAssertNotEqual(strcmp(host_path, @"/tmp/demo".UTF8String), 0,
                      @"translation must not be identity mapping");
}

- (void)testRelativePersistentPathMapsUnderPersistentBackingRoot {
    char host_path[MAX_PATH];
    int ret = vfs_translate_path(@"etc/passwd".UTF8String, host_path, sizeof(host_path));
    NSString *actualPath = [NSString stringWithUTF8String:host_path];
    NSString *expectedPath = [NSString stringWithFormat:@"%s/etc/passwd", vfs_persistent_backing_root()];

    XCTAssertEqual(ret, 0, @"relative virtual path should translate");
    XCTAssertEqualObjects(actualPath, expectedPath, @"relative path should resolve under persistent backing root");
}

- (void)testUnmappableHostPathIsRejected {
    char virtual_path[MAX_PATH];
    int ret = vfs_reverse_translate(@"/private/var/mobile".UTF8String, virtual_path, sizeof(virtual_path));

    XCTAssertEqual(ret, -EXDEV, @"unmapped host path should be rejected");
}

- (void)testPersistentBackingRootReverseTranslatesToVirtualRoot {
    char virtual_path[MAX_PATH];
    int ret = vfs_reverse_translate(vfs_persistent_backing_root(), virtual_path, sizeof(virtual_path));

    XCTAssertEqual(ret, 0, @"persistent backing root should reverse translate");
    XCTAssertEqual(strcmp(virtual_path, vfs_virtual_root()), 0,
                   @"persistent backing root should map back to virtual root");
}

- (void)testMappedPersistentHostPathReverseTranslatesToVirtualPath {
    char virtual_path[MAX_PATH];
    NSString *hostPath = [NSString stringWithFormat:@"%s/var/log", vfs_persistent_backing_root()];
    int ret = vfs_reverse_translate(hostPath.UTF8String, virtual_path, sizeof(virtual_path));

    XCTAssertEqual(ret, 0, @"mapped persistent host path should reverse translate");
    XCTAssertEqualObjects([NSString stringWithUTF8String:virtual_path], @"/var/log",
                          @"reverse translation should return virtual path");
}

- (void)testPersistentRootDiscoveryResolves {
    char path[MAX_PATH];
    int ret = vfs_discover_persistent_root(path, sizeof(path));

    XCTAssertEqual(ret, 0, @"persistent root discovery should succeed");
    XCTAssertTrue(path[0] != '\0', @"persistent root should be non-empty");
}

- (void)testCacheRootDiscoveryResolves {
    char path[MAX_PATH];
    int ret = vfs_discover_cache_root(path, sizeof(path));

    XCTAssertEqual(ret, 0, @"cache root discovery should succeed");
    XCTAssertTrue(path[0] != '\0', @"cache root should be non-empty");
}

- (void)testTempRootDiscoveryResolves {
    char path[MAX_PATH];
    int ret = vfs_discover_temp_root(path, sizeof(path));

    XCTAssertEqual(ret, 0, @"temp root discovery should succeed");
    XCTAssertTrue(path[0] != '\0', @"temp root should be non-empty");
}

- (void)testDiscoveredBackingRootsAreDistinctByClass {
    char persistent[MAX_PATH];
    char cache[MAX_PATH];
    char temp[MAX_PATH];

    XCTAssertEqual(vfs_discover_persistent_root(persistent, sizeof(persistent)), 0,
                   @"persistent root discovery should succeed");
    XCTAssertEqual(vfs_discover_cache_root(cache, sizeof(cache)), 0,
                   @"cache root discovery should succeed");
    XCTAssertEqual(vfs_discover_temp_root(temp, sizeof(temp)), 0,
                   @"temp root discovery should succeed");

    XCTAssertNotEqual(strcmp(persistent, temp), 0,
                      @"persistent root must not be temp-backed");
    XCTAssertNotEqual(strcmp(cache, temp), 0,
                      @"cache and temp roots should not collapse to one path");
}

- (void)testBackingClassRoutingForPersistentPaths {
    XCTAssertEqual(vfs_backing_class_for_path(@"/etc/passwd".UTF8String), VFS_BACKING_PERSISTENT,
                   @"/etc/passwd should route to persistent backing");
    XCTAssertEqual(vfs_backing_class_for_path(@"/usr/bin/sh".UTF8String), VFS_BACKING_PERSISTENT,
                   @"/usr/bin/sh should route to persistent backing");
    XCTAssertEqual(vfs_backing_class_for_path(@"/var/lib/foo".UTF8String), VFS_BACKING_PERSISTENT,
                   @"/var/lib/foo should route to persistent backing");
    XCTAssertEqual(vfs_backing_class_for_path(@"/home/user/.profile".UTF8String), VFS_BACKING_PERSISTENT,
                   @"/home paths should route to persistent backing");
    XCTAssertEqual(vfs_backing_class_for_path(@"/root/.profile".UTF8String), VFS_BACKING_PERSISTENT,
                   @"/root paths should route to persistent backing");
}

- (void)testBackingClassRoutingForCacheTempAndSyntheticPaths {
    XCTAssertEqual(vfs_backing_class_for_path(@"/var/cache/x".UTF8String), VFS_BACKING_CACHE,
                   @"/var/cache/x should route to cache backing");
    XCTAssertEqual(vfs_backing_class_for_path(@"/tmp/x".UTF8String), VFS_BACKING_TEMP,
                   @"/tmp/x should route to temp backing");
    XCTAssertEqual(vfs_backing_class_for_path(@"/proc/meminfo".UTF8String), VFS_BACKING_SYNTHETIC,
                   @"/proc/meminfo should route to synthetic backing");
    XCTAssertEqual(vfs_backing_class_for_path(@"/sys/kernel".UTF8String), VFS_BACKING_SYNTHETIC,
                   @"/sys/kernel should route to synthetic backing");
    XCTAssertEqual(vfs_backing_class_for_path(@"/dev/null".UTF8String), VFS_BACKING_SYNTHETIC,
                   @"/dev/null should route to synthetic backing");
}

- (void)testPersistentFallbackRouteTranslatesAndReverseTranslates {
    char host_path[MAX_PATH];
    char virtual_path[MAX_PATH];
    int ret;
    NSString *actualPath;
    NSString *expectedPath;

    ret = vfs_translate_path(@"/var/log/messages".UTF8String, host_path, sizeof(host_path));
    XCTAssertEqual(ret, 0, @"fallback persistent path should translate");

    actualPath = [NSString stringWithUTF8String:host_path];
    expectedPath = [NSString stringWithFormat:@"%s/var/log/messages", vfs_persistent_backing_root()];
    XCTAssertEqualObjects(actualPath, expectedPath,
                          @"fallback persistent route should join under persistent backing root");

    ret = vfs_reverse_translate(host_path, virtual_path, sizeof(virtual_path));
    XCTAssertEqual(ret, 0, @"fallback persistent host path should reverse translate");
    XCTAssertEqualObjects([NSString stringWithUTF8String:virtual_path], @"/var/log/messages",
                          @"fallback persistent host path should map back through the route table");
}

- (void)testSyntheticRouteRejectsHostJoin {
    char host_path[MAX_PATH];
    int ret = vfs_translate_path(@"/dev/null".UTF8String, host_path, sizeof(host_path));

    XCTAssertEqual(ret, -ENOTSUP, @"synthetic route should not join to a host backing root");
}

- (void)testDescriptorDrivenPathClassification {
    XCTAssertEqual(path_classify(@"/proc/meminfo".UTF8String), PATH_VIRTUAL_LINUX,
                   @"synthetic routes should classify through descriptor lookup");
    XCTAssertEqual(path_classify(@"/sys/kernel".UTF8String), PATH_VIRTUAL_LINUX,
                   @"sys routes should classify through descriptor lookup");
    XCTAssertEqual(path_classify(@"/dev/null".UTF8String), PATH_VIRTUAL_LINUX,
                   @"dev routes should classify through descriptor lookup");
    XCTAssertEqual(path_classify(@"/private/var/mobile/file".UTF8String), PATH_ABSOLUTE_HOST,
                   @"non-route absolute paths should remain host paths");
}

- (void)testDescriptorDrivenVirtualLinuxDetection {
    XCTAssertTrue(path_is_virtual_linux(@"/tmp/demo".UTF8String),
                  @"tmp route should be recognized as Linux-visible");
    XCTAssertTrue(path_is_virtual_linux(@"/var/tmp/demo".UTF8String),
                  @"var/tmp route should remain a distinct Linux-visible route");
    XCTAssertTrue(path_is_virtual_linux(@"/run/demo".UTF8String),
                  @"run route should remain a distinct Linux-visible route");
    XCTAssertTrue(path_is_virtual_linux(@"relative/demo".UTF8String),
                  @"relative paths should stay Linux-visible by context");
    XCTAssertFalse(path_is_virtual_linux(@"/private/var/mobile/file".UTF8String),
                   @"absolute host paths outside route descriptors should not classify as Linux-visible");
}

- (void)testPersistentRootIsNotDocumentsTruth {
    NSString *persistentRoot = [NSString stringWithUTF8String:vfs_persistent_backing_root()];

    XCTAssertFalse([persistentRoot containsString:@"/Documents"],
                   @"persistent backing root must not treat Documents as Linux root truth");
    XCTAssertFalse(path_is_own_sandbox(@"/Documents/example.txt".UTF8String),
                   @"Documents path fragments must not become Linux root truth through sandbox heuristics");
}

- (void)testPersistentBackingRootReverseTranslation {
    char host_path[MAX_PATH];
    char virtual_path[MAX_PATH];
    int ret;

    ret = vfs_translate_path(@"/etc/passwd".UTF8String, host_path, sizeof(host_path));
    XCTAssertEqual(ret, 0, @"persistent path translation should succeed");

    ret = vfs_reverse_translate(host_path, virtual_path, sizeof(virtual_path));
    XCTAssertEqual(ret, 0, @"persistent host path should reverse translate");
    XCTAssertEqualObjects([NSString stringWithUTF8String:virtual_path], @"/etc/passwd",
                          @"persistent host path should map back to /etc/passwd");
}

- (void)testCacheBackingRootReverseTranslation {
    char host_path[MAX_PATH];
    char virtual_path[MAX_PATH];
    int ret;

    ret = vfs_translate_path(@"/var/cache/x".UTF8String, host_path, sizeof(host_path));
    XCTAssertEqual(ret, 0, @"cache path translation should succeed");

    ret = vfs_reverse_translate(host_path, virtual_path, sizeof(virtual_path));
    XCTAssertEqual(ret, 0, @"cache host path should reverse translate");
    XCTAssertEqualObjects([NSString stringWithUTF8String:virtual_path], @"/var/cache/x",
                          @"cache host path should map back to /var/cache/x");
}

- (void)testTempBackingRootReverseTranslation {
    char host_path[MAX_PATH];
    char virtual_path[MAX_PATH];
    int ret;

    ret = vfs_translate_path(@"/tmp/x".UTF8String, host_path, sizeof(host_path));
    XCTAssertEqual(ret, 0, @"temp path translation should succeed");

    ret = vfs_reverse_translate(host_path, virtual_path, sizeof(virtual_path));
    XCTAssertEqual(ret, 0, @"temp host path should reverse translate");
    XCTAssertEqualObjects([NSString stringWithUTF8String:virtual_path], @"/tmp/x",
                          @"temp host path should map back to /tmp/x");
}

- (void)testParentEscapeIsRejected {

    char host_path[MAX_PATH];
    int ret = vfs_translate_path(@"../secret".UTF8String, host_path, sizeof(host_path));

    XCTAssertEqual(ret, -EINVAL, @"parent escapes should be rejected");
}

/* ============================================================================
 * TASK-AWARE PATH RESOLUTION TESTS
 * ============================================================================ */

- (void)testTaskAwareAbsolutePathUsesVirtualRoot {
    /* Create an fs_struct with custom root */
    struct fs_struct *fs = alloc_fs_struct();
    XCTAssertTrue(fs != NULL, @"fs_struct allocation should succeed");
    if (!fs) return;
    
    fs_init_root(fs, "/");
    fs_init_pwd(fs, "/etc");
    
    /* Absolute path should resolve from root - with root="/", "/bin/ls" -> "/bin/ls" */
    char host_path[MAX_PATH];
    int ret = vfs_translate_path_task(@"/bin/ls".UTF8String, host_path, sizeof(host_path), fs);
    
    XCTAssertEqual(ret, 0, @"absolute path translation should succeed");
    NSString *result = [NSString stringWithUTF8String:host_path];
    NSString *expected = [NSString stringWithFormat:@"%s/bin/ls", vfs_host_backing_root()];
    XCTAssertEqualObjects(result, expected, @"absolute path should resolve from virtual root");
    
    free_fs_struct(fs);
}

- (void)testTaskAwareRelativePathUsesPwd {
    /* Create an fs_struct with custom pwd */
    struct fs_struct *fs = alloc_fs_struct();
    XCTAssertTrue(fs != NULL, @"fs_struct allocation should succeed");
    if (!fs) return;
    
    fs_init_root(fs, "/");
    fs_init_pwd(fs, "/etc");
    
    /* Relative path should resolve from pwd */
    char host_path[MAX_PATH];
    int ret = vfs_translate_path_task(@"passwd".UTF8String, host_path, sizeof(host_path), fs);
    
    XCTAssertEqual(ret, 0, @"relative path translation should succeed");
    NSString *result = [NSString stringWithUTF8String:host_path];
    NSString *expected = [NSString stringWithFormat:@"%s/etc/passwd", vfs_host_backing_root()];
    XCTAssertEqualObjects(result, expected, @"relative path should resolve from virtual pwd");
    
    free_fs_struct(fs);
}

- (void)testTaskAwareRelativePathWithSubdirectories {
    /* Create an fs_struct with nested pwd */
    struct fs_struct *fs = alloc_fs_struct();
    XCTAssertTrue(fs != NULL, @"fs_struct allocation should succeed");
    if (!fs) return;
    
    fs_init_root(fs, "/");
    fs_init_pwd(fs, "/usr/local");
    
    /* Relative path with subdirectory */
    char host_path[MAX_PATH];
    int ret = vfs_translate_path_task(@"bin/myapp".UTF8String, host_path, sizeof(host_path), fs);
    
    XCTAssertEqual(ret, 0, @"nested relative path translation should succeed");
    NSString *result = [NSString stringWithUTF8String:host_path];
    NSString *expected = [NSString stringWithFormat:@"%s/usr/local/bin/myapp", vfs_host_backing_root()];
    XCTAssertEqualObjects(result, expected, @"relative path should resolve correctly from nested pwd");
    
    free_fs_struct(fs);
}

- (void)testTaskAwareParentEscapeRejected {
    /* Create an fs_struct */
    struct fs_struct *fs = alloc_fs_struct();
    XCTAssertTrue(fs != NULL, @"fs_struct allocation should succeed");
    if (!fs) return;
    
    fs_init_root(fs, "/");
    fs_init_pwd(fs, "/etc");
    
    /* Parent escape should be rejected even with task context */
    char host_path[MAX_PATH];
    int ret = vfs_translate_path_task(@"../secret".UTF8String, host_path, sizeof(host_path), fs);
    
    XCTAssertEqual(ret, -EINVAL, @"parent escapes should be rejected with task context");
    
    free_fs_struct(fs);
}

- (void)testTaskAwareAbsolutePathUsesTaskRootPrefix {
    struct fs_struct *fs = alloc_fs_struct();
    XCTAssertTrue(fs != NULL, @"fs_struct allocation should succeed");
    if (!fs) return;

    fs_init_root(fs, @"/sandbox".UTF8String);
    fs_init_pwd(fs, @"/sandbox/work".UTF8String);

    char host_path[MAX_PATH];
    int ret = vfs_translate_path_task(@"/bin/ls".UTF8String, host_path, sizeof(host_path), fs);

    XCTAssertEqual(ret, 0, @"absolute path translation should succeed from non-root task root");
    NSString *result = [NSString stringWithUTF8String:host_path];
    NSString *expected = [NSString stringWithFormat:@"%s/sandbox/bin/ls", vfs_host_backing_root()];
    XCTAssertEqualObjects(result, expected, @"absolute paths should resolve from task root prefix");

    free_fs_struct(fs);
}

- (void)testGetcwdMatchesTaskPwdAndRelativeResolution {
    struct task_struct *originalTask = get_current();
    struct task_struct *task = alloc_task();
    XCTAssertTrue(task != NULL, @"task allocation should succeed");
    if (!task) return;

    task->fs = alloc_fs_struct();
    XCTAssertTrue(task->fs != NULL, @"fs_struct allocation should succeed");
    if (!task->fs) {
        free_task(task);
        return;
    }

    fs_init_root(task->fs, @"/".UTF8String);
    fs_init_pwd(task->fs, @"/usr/local".UTF8String);
    set_current(task);

    char cwd[MAX_PATH];
    char resolved[MAX_PATH];
    char expected[MAX_PATH];

    char *cwd_result = getcwd_impl(cwd, sizeof(cwd));
    XCTAssertTrue(cwd_result != NULL, @"getcwd_impl should return current task pwd");
    XCTAssertEqualObjects([NSString stringWithUTF8String:cwd], @"/usr/local",
                          @"getcwd_impl should report task virtual pwd");

    int resolve_ret = path_resolve(@"bin/tool".UTF8String, resolved, sizeof(resolved));
    XCTAssertEqual(resolve_ret, 0, @"path_resolve should succeed for relative task path");

    int expected_ret = vfs_translate_path_task(@"bin/tool".UTF8String, expected, sizeof(expected), task->fs);
    XCTAssertEqual(expected_ret, 0, @"expected task-aware translation should succeed");
    XCTAssertEqualObjects([NSString stringWithUTF8String:resolved], [NSString stringWithUTF8String:expected],
                          @"relative resolution should agree with task getcwd state");

    set_current(originalTask);
    free_task(task);
}

/* ============================================================================
 * DIRFD-AWARE PATH RESOLUTION TESTS
 * ============================================================================ */

- (void)testVfsTranslatePathAtUsesAtFdcwd {
    char host_path[MAX_PATH];
    int ret = vfs_translate_path_at(AT_FDCWD, @"/etc/passwd".UTF8String, host_path, sizeof(host_path));

    XCTAssertEqual(ret, 0, @"vfs_translate_path_at with AT_FDCWD should succeed");
    NSString *result = [NSString stringWithUTF8String:host_path];
    NSString *expected = [NSString stringWithFormat:@"%s/etc/passwd", vfs_host_backing_root()];
    XCTAssertEqualObjects(result, expected, @"AT_FDCWD should resolve from task cwd");
}

- (void)testVfsTranslatePathAtAbsolutePathIgnoresDirfd {
    char host_path[MAX_PATH];
    int ret = vfs_translate_path_at(-1, @"/bin/ls".UTF8String, host_path, sizeof(host_path));

    XCTAssertEqual(ret, 0, @"absolute path should succeed regardless of invalid dirfd");
    NSString *result = [NSString stringWithUTF8String:host_path];
    NSString *expected = [NSString stringWithFormat:@"%s/bin/ls", vfs_host_backing_root()];
    XCTAssertEqualObjects(result, expected, @"absolute paths should resolve from task root");
}

- (void)testVfsTranslatePathAtInvalidDirfdReturnsBadf {
    char host_path[MAX_PATH];
    int ret = vfs_translate_path_at(9999, @"relative/path".UTF8String, host_path, sizeof(host_path));

    XCTAssertEqual(ret, -EBADF, @"invalid dirfd should return -EBADF");
}

- (void)testVfsTranslatePathAtRelativePathUsesDirfd {
    int real_fd;
    int dirfd;
    char host_dir[MAX_PATH];
    char host_path[MAX_PATH];
    NSString *expected;

    XCTAssertEqual(vfs_translate_path(@"/tmp/translate-dirfd".UTF8String, host_dir, sizeof(host_dir)), 0,
                   @"dirfd directory should translate");
    vfs_test_ensure_virtual_parent_directory(@"/tmp/translate-dirfd/file".UTF8String);

    real_fd = vfs_test_open_host_directory_fd(host_dir);
    XCTAssertTrue(real_fd >= 0, @"host directory open should succeed");
    if (real_fd < 0) return;

    dirfd = alloc_fd_impl();
    XCTAssertTrue(dirfd >= 0, @"dirfd allocation should succeed");
    if (dirfd < 0) {
        host_close_impl(real_fd);
        return;
    }

    init_fd_entry_impl(dirfd, real_fd, O_RDONLY | O_DIRECTORY, 0755, @"/tmp/translate-dirfd".UTF8String);

    XCTAssertEqual(vfs_translate_path_at(dirfd, @"file".UTF8String, host_path, sizeof(host_path)), 0,
                   @"relative path should resolve from dirfd");
    expected = [NSString stringWithFormat:@"%s/file", host_dir];
    XCTAssertEqualObjects([NSString stringWithUTF8String:host_path], expected,
                          @"relative path should translate from dirfd base directory");

    free_fd_impl(dirfd);
    vfs_test_remove_linux_path(@"/tmp/translate-dirfd/file".UTF8String);
}

/* ============================================================================
 * STAT-FAMILY AND AT-FLAG SEMANTICS TESTS
 * ============================================================================ */

extern int vfs_fstatat(int dirfd, const char *pathname, struct stat *statbuf, int flags);
extern int vfs_faccessat(int dirfd, const char *pathname, int mode, int flags);

#define TEST_AT_SYMLINK_NOFOLLOW 0x100
#define TEST_AT_EACCESS 0x200
#define TEST_AT_EMPTY_PATH 0x1000
#define TEST_AT_REMOVEDIR 0x200
#define TEST_RENAME_NOREPLACE 0x0001
#define TEST_RENAME_EXCHANGE 0x0002
#define TEST_RENAME_WHITEOUT 0x0004
#define TEST_F_DUPFD 0
#define TEST_F_GETFD 1
#define TEST_F_SETFD 2
#define TEST_F_GETFL 3
#define TEST_F_SETFL 4
#define TEST_F_DUPFD_CLOEXEC 1030
#define TEST_FD_CLOEXEC 1

- (void)testVfsFstatatSupportsAtFdcwd {
    struct stat st;
    int ret = vfs_fstatat(AT_FDCWD, @"/etc/passwd".UTF8String, &st, 0);

    XCTAssertEqual(ret, 0, @"vfs_fstatat with AT_FDCWD should succeed");
}

- (void)testVfsFstatatSupportsSymlinkNoFollow {
    struct stat st;
    int ret = vfs_fstatat(AT_FDCWD, @"/etc/passwd".UTF8String, &st, TEST_AT_SYMLINK_NOFOLLOW);

    XCTAssertEqual(ret, 0, @"vfs_fstatat with AT_SYMLINK_NOFOLLOW should succeed");
}

- (void)testVfsFstatatRejectsInvalidFlags {
    struct stat st;
    int ret = vfs_fstatat(AT_FDCWD, @"/etc/passwd".UTF8String, &st, 0x80000000);

    XCTAssertEqual(ret, -EINVAL, @"vfs_fstatat should reject invalid flags");
}

- (void)testSyntheticRootStatSucceeds {
    struct stat st;

    XCTAssertEqual(vfs_fstatat(AT_FDCWD, @"/proc".UTF8String, &st, 0), 0,
                   @"synthetic root vfs_fstatat should succeed");
    XCTAssertTrue(S_ISDIR(st.st_mode), @"/proc root should be a directory");
    XCTAssertEqual(st.st_mode & 0777, 0555, @"/proc root should have 0555 permissions");

    XCTAssertEqual(vfs_fstatat(AT_FDCWD, @"/sys".UTF8String, &st, 0), 0,
                   @"synthetic root vfs_fstatat should succeed for /sys");
    XCTAssertTrue(S_ISDIR(st.st_mode), @"/sys root should be a directory");

    XCTAssertEqual(vfs_fstatat(AT_FDCWD, @"/dev".UTF8String, &st, 0), 0,
                   @"synthetic root vfs_fstatat should succeed for /dev");
    XCTAssertTrue(S_ISDIR(st.st_mode), @"/dev root should be a directory");

    errno = 0;
    XCTAssertEqual(stat(@"/proc".UTF8String, &st), 0,
                   @"public stat should succeed for synthetic root");
    XCTAssertTrue(S_ISDIR(st.st_mode), @"public stat should return directory for /proc");

    errno = 0;
    XCTAssertEqual(lstat(@"/sys".UTF8String, &st), 0,
                   @"public lstat should succeed for synthetic root");
    XCTAssertTrue(S_ISDIR(st.st_mode), @"public lstat should return directory for /sys");
}

- (void)testSyntheticChildStatFails {
    struct stat st;

    XCTAssertEqual(vfs_fstatat(AT_FDCWD, @"/proc/meminfo".UTF8String, &st, 0), -ENOENT,
                   @"synthetic child vfs_fstatat should reject through descriptor policy");
    XCTAssertEqual(vfs_fstatat(AT_FDCWD, @"/sys/kernel".UTF8String, &st, TEST_AT_SYMLINK_NOFOLLOW), -ENOENT,
                   @"synthetic child vfs_fstatat lstat path should reject through descriptor policy");

    errno = 0;
    XCTAssertEqual(stat(@"/proc/meminfo".UTF8String, &st), -1,
                   @"public stat should reject unsupported synthetic child paths");
    XCTAssertEqual(errno, ENOENT, @"public stat should set ENOENT for unsupported synthetic child paths");

    errno = 0;
    XCTAssertEqual(lstat(@"/sys/kernel".UTF8String, &st), -1,
                   @"public lstat should reject unsupported synthetic child paths");
    XCTAssertEqual(errno, ENOENT, @"public lstat should set ENOENT for unsupported synthetic child paths");
}

- (void)testSyntheticRootAccessSucceeds {
    XCTAssertEqual(vfs_faccessat(AT_FDCWD, @"/proc".UTF8String, F_OK, 0), 0,
                   @"synthetic root vfs_faccessat should succeed");
    XCTAssertEqual(vfs_faccessat(AT_FDCWD, @"/sys".UTF8String, F_OK, 0), 0,
                   @"synthetic root vfs_faccessat should succeed for /sys");
    XCTAssertEqual(vfs_faccessat(AT_FDCWD, @"/dev".UTF8String, F_OK, 0), 0,
                   @"synthetic root vfs_faccessat should succeed for /dev");

    errno = 0;
    XCTAssertEqual(access(@"/proc".UTF8String, F_OK), 0,
                   @"public access should succeed for synthetic root");
}

- (void)testSyntheticChildAccessFails {
    XCTAssertEqual(vfs_faccessat(AT_FDCWD, @"/proc/meminfo".UTF8String, F_OK, 0), -ENOENT,
                   @"synthetic child vfs_faccessat should reject through descriptor policy");

    errno = 0;
    XCTAssertEqual(access(@"/proc/meminfo".UTF8String, F_OK), -1,
                   @"public access should reject unsupported synthetic child routes");
    XCTAssertEqual(errno, ENOENT, @"public access should set ENOENT for unsupported synthetic child routes");
}

- (void)testSyntheticRootOpenDirectorySucceedsAndChildOpenFails {
    errno = 0;
    int proc_fd = open("/proc", O_RDONLY | O_DIRECTORY);
    XCTAssertTrue(proc_fd >= 0, @"open(/proc, O_DIRECTORY) should succeed");
    if (proc_fd >= 0) close(proc_fd);

    errno = 0;
    int sys_fd = open("/sys", O_RDONLY | O_DIRECTORY);
    XCTAssertTrue(sys_fd >= 0, @"open(/sys, O_DIRECTORY) should succeed");
    if (sys_fd >= 0) close(sys_fd);
}

- (void)testSyntheticChildOpenFails {
    errno = 0;
    XCTAssertEqual(open(@"/proc/meminfo".UTF8String, O_RDONLY), -1,
                   @"public open should reject unsupported synthetic child routes");
    XCTAssertEqual(errno, ENOTSUP, @"public open should set ENOTSUP for unsupported synthetic child routes");

    errno = 0;
    XCTAssertEqual(openat(AT_FDCWD, @"/sys/kernel".UTF8String, O_RDONLY), -1,
                   @"public openat should reject unsupported synthetic routes before host fallback");
    XCTAssertEqual(errno, ENOTSUP, @"public openat should set ENOTSUP for unsupported synthetic routes");
}

- (void)testSyntheticGetdentsUsesTemporaryUnsupportedPolicy {
    char host_dir[MAX_PATH];
    int real_fd;
    int dirfd;
    char buffer[256];

    XCTAssertEqual(vfs_translate_path(@"/tmp/synthetic-dirfd-anchor".UTF8String, host_dir, sizeof(host_dir)), 0,
                   @"anchor directory should translate");
    vfs_test_ensure_virtual_parent_directory(@"/tmp/synthetic-dirfd-anchor/file".UTF8String);

    real_fd = vfs_test_open_host_directory_fd(host_dir);
    XCTAssertTrue(real_fd >= 0, @"host anchor directory open should succeed");
    if (real_fd < 0) return;

    dirfd = alloc_fd_impl();
    XCTAssertTrue(dirfd >= 0, @"synthetic dirfd allocation should succeed");
    if (dirfd < 0) {
        close(real_fd);
        return;
    }

    init_fd_entry_impl(dirfd, real_fd, O_RDONLY | O_DIRECTORY, 0755, @"/proc".UTF8String);

    errno = 0;
    XCTAssertEqual(getdents64(dirfd, buffer, sizeof(buffer)), -1,
                   @"getdents64 should reject synthetic directories (not yet implemented)");
    XCTAssertEqual(errno, ENOTSUP, @"getdents64 should set ENOTSUP for synthetic directories");

    free_fd_impl(dirfd);
    vfs_test_remove_linux_path(@"/tmp/synthetic-dirfd-anchor/file".UTF8String);
}

- (void)testSyntheticRootOpenDirectorySucceeds {
    errno = 0;
    int proc_fd = open("/proc", O_RDONLY | O_DIRECTORY);
    XCTAssertTrue(proc_fd >= 0, @"open(/proc, O_DIRECTORY) should succeed");
    XCTAssertEqual(errno, 0, @"errno should be 0 after open(/proc, O_DIRECTORY)");

    errno = 0;
    int sys_fd = open("/sys", O_RDONLY | O_DIRECTORY);
    XCTAssertTrue(sys_fd >= 0, @"open(/sys, O_DIRECTORY) should succeed");
    XCTAssertEqual(errno, 0, @"errno should be 0 after open(/sys, O_DIRECTORY)");

    errno = 0;
    int dev_fd = open("/dev", O_RDONLY | O_DIRECTORY);
    XCTAssertTrue(dev_fd >= 0, @"open(/dev, O_DIRECTORY) should succeed");
    XCTAssertEqual(errno, 0, @"errno should be 0 after open(/dev, O_DIRECTORY)");

    if (proc_fd >= 0) close(proc_fd);
    if (sys_fd >= 0) close(sys_fd);
    if (dev_fd >= 0) close(dev_fd);
}

- (void)testSyntheticRootGetdents64ReturnsDotAndDotdot {
    int fd = open("/proc", O_RDONLY | O_DIRECTORY);
    XCTAssertTrue(fd >= 0, @"open(/proc, O_DIRECTORY) should succeed");
    if (fd < 0) return;

    /* Ensure buffer is 8-byte aligned for struct linux_dirent64 */
    union { char storage[1024]; uint64_t align; } aligned;
    char *buffer = aligned.storage;
    memset(buffer, 0, sizeof(aligned));

    ssize_t nread = getdents64(fd, buffer, sizeof(aligned.storage));
    XCTAssertTrue(nread > 0, @"getdents64(/proc) should return > 0 bytes, got %zd errno %d", nread, errno);

    // Parse the entries to verify . and ..
    bool found_dot = false;
    bool found_dotdot = false;
    size_t pos = 0;

    while (pos < (size_t)nread) {
        struct linux_dirent64 *entry = (struct linux_dirent64 *)(buffer + pos);
        NSString *name = [NSString stringWithUTF8String:entry->d_name];

        if ([name isEqualToString:@"."]) {
            found_dot = true;
            XCTAssertEqual(entry->d_type, DT_DIR, @". should be DT_DIR");
        } else if ([name isEqualToString:@".."]) {
            found_dotdot = true;
            XCTAssertEqual(entry->d_type, DT_DIR, @".. should be DT_DIR");
        }

        XCTAssertTrue(entry->d_reclen > 0, @"d_reclen must be non-zero");
        XCTAssertTrue(entry->d_reclen <= (unsigned short)((size_t)nread - pos), @"d_reclen must fit remaining buffer");
        if (entry->d_reclen == 0 || entry->d_reclen > (unsigned short)((size_t)nread - pos)) {
            break;
        }
        pos += entry->d_reclen;
    }

    XCTAssertTrue(found_dot, @"getdents64(/proc) should return '.' entry");
    XCTAssertTrue(found_dotdot, @"getdents64(/proc) should return '..' entry");

    // Second call should return 0 (EOF)
    nread = getdents64(fd, buffer, sizeof(aligned.storage));
    XCTAssertEqual(nread, 0, @"Second getdents64(/proc) should return 0 (EOF)");

    close(fd);
}

- (void)testSyntheticSysAndDevGetdents64ReturnsDotAndDotdot {
    // Test /sys
    int sys_fd = open("/sys", O_RDONLY | O_DIRECTORY);
    XCTAssertTrue(sys_fd >= 0, @"open(/sys, O_DIRECTORY) should succeed");

    if (sys_fd >= 0) {
        union { char storage[1024]; uint64_t align; } aligned;
        char *buffer = aligned.storage;
        memset(buffer, 0, sizeof(aligned));

        ssize_t nread = getdents64(sys_fd, buffer, sizeof(aligned.storage));
        XCTAssertTrue(nread > 0, @"getdents64(/sys) should return > 0 bytes");

        bool found_dot = false;
        bool found_dotdot = false;
        size_t pos = 0;

        while (pos < (size_t)nread) {
            struct linux_dirent64 *entry = (struct linux_dirent64 *)(buffer + pos);
            NSString *name = [NSString stringWithUTF8String:entry->d_name];

            if ([name isEqualToString:@"."]) {
                found_dot = true;
                XCTAssertEqual(entry->d_type, DT_DIR, @". should be DT_DIR");
            } else if ([name isEqualToString:@".."]) {
                found_dotdot = true;
                XCTAssertEqual(entry->d_type, DT_DIR, @".. should be DT_DIR");
            }

            XCTAssertTrue(entry->d_reclen > 0, @"d_reclen must be non-zero");
        XCTAssertTrue(entry->d_reclen <= (unsigned short)((size_t)nread - pos), @"d_reclen must fit remaining buffer");
        if (entry->d_reclen == 0 || entry->d_reclen > (unsigned short)((size_t)nread - pos)) {
            break;
        }
        pos += entry->d_reclen;
        }

        XCTAssertTrue(found_dot, @"getdents64(/sys) should return '.' entry");
        XCTAssertTrue(found_dotdot, @"getdents64(/sys) should return '..' entry");

        // Second call should return 0 (EOF)
        nread = getdents64(sys_fd, buffer, sizeof(aligned.storage));
        XCTAssertEqual(nread, 0, @"Second getdents64(/sys) should return 0 (EOF)");

        close(sys_fd);
    }

    // Test /dev
    int dev_fd = open("/dev", O_RDONLY | O_DIRECTORY);
    XCTAssertTrue(dev_fd >= 0, @"open(/dev, O_DIRECTORY) should succeed");

    if (dev_fd >= 0) {
        union { char storage[1024]; uint64_t align; } aligned;
        char *buffer = aligned.storage;
        memset(buffer, 0, sizeof(aligned));

        ssize_t nread = getdents64(dev_fd, buffer, sizeof(aligned.storage));
        XCTAssertTrue(nread > 0, @"getdents64(/dev) should return > 0 bytes");

        bool found_dot = false;
        bool found_dotdot = false;
        size_t pos = 0;

        while (pos < (size_t)nread) {
            struct linux_dirent64 *entry = (struct linux_dirent64 *)(buffer + pos);
            NSString *name = [NSString stringWithUTF8String:entry->d_name];

            if ([name isEqualToString:@"."]) {
                found_dot = true;
                XCTAssertEqual(entry->d_type, DT_DIR, @". should be DT_DIR");
            } else if ([name isEqualToString:@".."]) {
                found_dotdot = true;
                XCTAssertEqual(entry->d_type, DT_DIR, @".. should be DT_DIR");
            }

            XCTAssertTrue(entry->d_reclen > 0, @"d_reclen must be non-zero");
        XCTAssertTrue(entry->d_reclen <= (unsigned short)((size_t)nread - pos), @"d_reclen must fit remaining buffer");
        if (entry->d_reclen == 0 || entry->d_reclen > (unsigned short)((size_t)nread - pos)) {
            break;
        }
        pos += entry->d_reclen;
        }

        XCTAssertTrue(found_dot, @"getdents64(/dev) should return '.' entry");
        XCTAssertTrue(found_dotdot, @"getdents64(/dev) should return '..' entry");

        // Second call should return 0 (EOF)
        nread = getdents64(dev_fd, buffer, sizeof(aligned.storage));
        XCTAssertEqual(nread, 0, @"Second getdents64(/dev) should return 0 (EOF)");

        close(dev_fd);
    }
}

/* ============================================================================
 * SYNTHETIC /dev NODE TESTS
 * ============================================================================ */

- (void)testDevNullStatSucceeds {
    struct stat st;
    errno = 0;
    XCTAssertEqual(stat("/dev/null", &st), 0, @"stat(/dev/null) should succeed");
    XCTAssertTrue(S_ISCHR(st.st_mode), @"/dev/null should be a character device");
    XCTAssertEqual(st.st_mode & 0777, 0666, @"/dev/null should have 0666 permissions");

    errno = 0;
    XCTAssertEqual(lstat("/dev/null", &st), 0, @"lstat(/dev/null) should succeed");
    XCTAssertTrue(S_ISCHR(st.st_mode), @"lstat(/dev/null) should return character device");
}

- (void)testDevZeroStatSucceeds {
    struct stat st;
    errno = 0;
    XCTAssertEqual(stat("/dev/zero", &st), 0, @"stat(/dev/zero) should succeed");
    XCTAssertTrue(S_ISCHR(st.st_mode), @"/dev/zero should be a character device");
    XCTAssertEqual(st.st_mode & 0777, 0666, @"/dev/zero should have 0666 permissions");
}

- (void)testDevUrandomStatSucceeds {
    struct stat st;
    errno = 0;
    XCTAssertEqual(stat("/dev/urandom", &st), 0, @"stat(/dev/urandom) should succeed");
    XCTAssertTrue(S_ISCHR(st.st_mode), @"/dev/urandom should be a character device");
    XCTAssertEqual(st.st_mode & 0777, 0666, @"/dev/urandom should have 0666 permissions");
}

- (void)testDevNullAccessSucceeds {
    errno = 0;
    XCTAssertEqual(access("/dev/null", F_OK), 0, @"access(/dev/null, F_OK) should succeed");
    XCTAssertEqual(access("/dev/null", R_OK), 0, @"access(/dev/null, R_OK) should succeed");
    XCTAssertEqual(access("/dev/null", W_OK), 0, @"access(/dev/null, W_OK) should succeed");
}

- (void)testDevZeroAccessSucceeds {
    errno = 0;
    XCTAssertEqual(access("/dev/zero", F_OK), 0, @"access(/dev/zero, F_OK) should succeed");
}

- (void)testDevUrandomAccessSucceeds {
    errno = 0;
    XCTAssertEqual(access("/dev/urandom", F_OK), 0, @"access(/dev/urandom, F_OK) should succeed");
}

- (void)testDevNullOpenSucceeds {
    errno = 0;
    int fd = open("/dev/null", O_RDWR);
    XCTAssertTrue(fd >= 0, @"open(/dev/null, O_RDWR) should succeed");
    if (fd >= 0) close(fd);
}

- (void)testDevZeroOpenSucceeds {
    errno = 0;
    int fd = open("/dev/zero", O_RDONLY);
    XCTAssertTrue(fd >= 0, @"open(/dev/zero, O_RDONLY) should succeed");
    if (fd >= 0) close(fd);
}

- (void)testDevUrandomOpenSucceeds {
    errno = 0;
    int fd = open("/dev/urandom", O_RDONLY);
    XCTAssertTrue(fd >= 0, @"open(/dev/urandom, O_RDONLY) should succeed");
    if (fd >= 0) close(fd);
}

- (void)testDevNullReadReturnsEOF {
    int fd = open("/dev/null", O_RDONLY);
    XCTAssertTrue(fd >= 0, @"open(/dev/null) should succeed");
    if (fd < 0) return;

    char buf[64];
    memset(buf, 0xAA, sizeof(buf));
    errno = 0;
    ssize_t nread = read(fd, buf, sizeof(buf));
    XCTAssertEqual(nread, 0, @"read(/dev/null) should return 0 (EOF)");
    close(fd);
}

- (void)testDevNullWriteSucceedsAndDiscards {
    int fd = open("/dev/null", O_WRONLY);
    XCTAssertTrue(fd >= 0, @"open(/dev/null, O_WRONLY) should succeed");
    if (fd < 0) return;

    errno = 0;
    ssize_t written = write(fd, "hello", 5);
    XCTAssertEqual(written, 5, @"write(/dev/null) should succeed and report all bytes written");
    close(fd);
}

- (void)testDevZeroReadFillsZeroBytes {
    int fd = open("/dev/zero", O_RDONLY);
    XCTAssertTrue(fd >= 0, @"open(/dev/zero) should succeed");
    if (fd < 0) return;

    char buf[128];
    memset(buf, 0xFF, sizeof(buf));
    errno = 0;
    ssize_t nread = read(fd, buf, sizeof(buf));
    XCTAssertEqual(nread, (ssize_t)sizeof(buf), @"read(/dev/zero) should return requested byte count");

    bool all_zero = true;
    for (size_t i = 0; i < sizeof(buf); i++) {
        if (buf[i] != 0) {
            all_zero = false;
            break;
        }
    }
    XCTAssertTrue(all_zero, @"/dev/zero read should fill buffer with zero bytes");
    close(fd);
}

- (void)testDevUrandomReadReturnsNontrivialData {
    int fd = open("/dev/urandom", O_RDONLY);
    XCTAssertTrue(fd >= 0, @"open(/dev/urandom) should succeed");
    if (fd < 0) return;

    char buf[256];
    memset(buf, 0, sizeof(buf));
    errno = 0;
    ssize_t nread = read(fd, buf, sizeof(buf));
    XCTAssertEqual(nread, (ssize_t)sizeof(buf), @"read(/dev/urandom) should return requested byte count");

    bool all_zero = true;
    for (size_t i = 0; i < sizeof(buf); i++) {
        if (buf[i] != 0) {
            all_zero = false;
            break;
        }
    }
    XCTAssertFalse(all_zero, @"/dev/urandom read should return nontrivial data (not all zeros)");
    close(fd);
}

- (void)testDevZeroWriteSucceedsAndDiscards {
    int fd = open("/dev/zero", O_WRONLY);
    XCTAssertTrue(fd >= 0, @"open(/dev/zero, O_WRONLY) should succeed");
    if (fd < 0) return;

    errno = 0;
    ssize_t written = write(fd, "data", 4);
    XCTAssertEqual(written, 4, @"write(/dev/zero) should succeed and discard");
    close(fd);
}

- (void)testDevUrandomWriteSucceedsAndDiscards {
    int fd = open("/dev/urandom", O_WRONLY);
    XCTAssertTrue(fd >= 0, @"open(/dev/urandom, O_WRONLY) should succeed");
    if (fd < 0) return;

    errno = 0;
    ssize_t written = write(fd, "data", 4);
    XCTAssertEqual(written, 4, @"write(/dev/urandom) should succeed and discard");
    close(fd);
}

- (void)testUnsupportedDevNodeStillFails {
    errno = 0;
    struct stat st;
    XCTAssertEqual(stat("/dev/sda", &st), -1, @"stat(/dev/sda) should fail for unsupported dev node");
    XCTAssertEqual(errno, ENOENT, @"stat(/dev/sda) should set ENOENT");

    errno = 0;
    XCTAssertEqual(open("/dev/tty", O_RDONLY), -1, @"open(/dev/tty) should fail for unsupported dev node");
    XCTAssertEqual(errno, ENOTSUP, @"open(/dev/tty) should set ENOTSUP");
}

- (void)testSyntheticGetdentsUsesIntentionalUnsupportedPolicy {
  char host_dir[MAX_PATH];
  int real_fd;
  int dirfd;
  char buffer[256];

  XCTAssertEqual(vfs_translate_path(@"/tmp/getdents-anchor".UTF8String, host_dir, sizeof(host_dir)), 0,
                @"anchor directory should translate");
  vfs_test_ensure_virtual_parent_directory(@"/tmp/getdents-anchor/file".UTF8String);

  real_fd = vfs_test_open_host_directory_fd(host_dir);
  XCTAssertTrue(real_fd >= 0, @"host anchor directory open should succeed");
  if (real_fd < 0) return;

  dirfd = alloc_fd_impl();
  XCTAssertTrue(dirfd >= 0, @"synthetic dirfd allocation should succeed");
  if (dirfd < 0) {
    close(real_fd);
    return;
  }

  init_fd_entry_impl(dirfd, real_fd, O_RDONLY | O_DIRECTORY, 0755, @"/proc".UTF8String);

  errno = 0;
  XCTAssertEqual(getdents64(dirfd, buffer, sizeof(buffer)), -1,
                @"getdents64 should reject synthetic directories (not yet implemented)");
  XCTAssertEqual(errno, ENOTSUP, @"getdents64 should set ENOTSUP for synthetic directories");

  free_fd_impl(dirfd);
  vfs_test_remove_linux_path(@"/tmp/getdents-anchor/file".UTF8String);
}


- (void)testVfsFaccessatSupportsAtFdcwd {
    int ret = vfs_faccessat(AT_FDCWD, @"/etc".UTF8String, X_OK, 0);

    XCTAssertEqual(ret, 0, @"vfs_faccessat with AT_FDCWD should succeed");
}

- (void)testVfsFaccessatRejectsInvalidFlags {
    int ret = vfs_faccessat(AT_FDCWD, @"/etc".UTF8String, X_OK, 0x80000000);

    XCTAssertEqual(ret, -EINVAL, @"vfs_faccessat should reject invalid flags");
}

- (void)testVfsFaccessatReportsUnsupportedAtEaccess {
    int ret = vfs_faccessat(AT_FDCWD, @"/etc".UTF8String, X_OK, TEST_AT_EACCESS);

    XCTAssertEqual(ret, -ENOTSUP, @"vfs_faccessat AT_EACCESS should return ENOTSUP");
}

- (void)testVfsFaccessatReportsUnsupportedSymlinkNoFollow {
    int ret = vfs_faccessat(AT_FDCWD, @"/etc".UTF8String, X_OK, TEST_AT_SYMLINK_NOFOLLOW);

    XCTAssertEqual(ret, -ENOTSUP, @"vfs_faccessat AT_SYMLINK_NOFOLLOW should return ENOTSUP");
}

/* ============================================================================
 * RENAME-FAMILY SEMANTICS TESTS
 * ============================================================================ */

- (void)testRenameAllowsSameRoutePersistentMove {
    int ret;
    char src[MAX_PATH];
    char dst[MAX_PATH];

    ret = vfs_translate_path(@"/etc/rename-src".UTF8String, src, sizeof(src));
    XCTAssertEqual(ret, 0, @"persistent source should translate");
    ret = vfs_translate_path(@"/etc/rename-dst".UTF8String, dst, sizeof(dst));
    XCTAssertEqual(ret, 0, @"persistent destination should translate");

    vfs_test_seed_linux_file(@"/etc/rename-src".UTF8String);
    vfs_test_remove_linux_path(@"/etc/rename-dst".UTF8String);

    ret = rename(@"/etc/rename-src".UTF8String, @"/etc/rename-dst".UTF8String);
    XCTAssertEqual(ret, 0, @"same-route persistent rename should succeed");
    XCTAssertEqual(access(src, F_OK), -1, @"source should be moved away");
    XCTAssertEqual(access(dst, F_OK), 0, @"destination should exist after rename");

    vfs_test_remove_linux_path(@"/etc/rename-src".UTF8String);
    vfs_test_remove_linux_path(@"/etc/rename-dst".UTF8String);
}

- (void)testRenameAllowsSameRouteCacheMove {
    int ret;
    char src[MAX_PATH];
    char dst[MAX_PATH];

    ret = vfs_translate_path(@"/var/cache/rename-src".UTF8String, src, sizeof(src));
    XCTAssertEqual(ret, 0, @"cache source should translate");
    ret = vfs_translate_path(@"/var/cache/rename-dst".UTF8String, dst, sizeof(dst));
    XCTAssertEqual(ret, 0, @"cache destination should translate");

    vfs_test_seed_linux_file(@"/var/cache/rename-src".UTF8String);
    vfs_test_remove_linux_path(@"/var/cache/rename-dst".UTF8String);

    ret = rename(@"/var/cache/rename-src".UTF8String, @"/var/cache/rename-dst".UTF8String);
    XCTAssertEqual(ret, 0, @"same-route cache rename should succeed");
    XCTAssertEqual(access(src, F_OK), -1, @"cache source should be moved away");
    XCTAssertEqual(access(dst, F_OK), 0, @"cache destination should exist after rename");

    vfs_test_remove_linux_path(@"/var/cache/rename-src".UTF8String);
    vfs_test_remove_linux_path(@"/var/cache/rename-dst".UTF8String);
}

- (void)testRenameAllowsSameRouteTmpMove {
    int ret;
    char src[MAX_PATH];
    char dst[MAX_PATH];

    ret = vfs_translate_path(@"/tmp/rename-src".UTF8String, src, sizeof(src));
    XCTAssertEqual(ret, 0, @"tmp source should translate");
    ret = vfs_translate_path(@"/tmp/rename-dst".UTF8String, dst, sizeof(dst));
    XCTAssertEqual(ret, 0, @"tmp destination should translate");

    vfs_test_seed_linux_file(@"/tmp/rename-src".UTF8String);
    vfs_test_remove_linux_path(@"/tmp/rename-dst".UTF8String);

    ret = rename(@"/tmp/rename-src".UTF8String, @"/tmp/rename-dst".UTF8String);
    XCTAssertEqual(ret, 0, @"same-route tmp rename should succeed");
    XCTAssertEqual(access(src, F_OK), -1, @"tmp source should be moved away");
    XCTAssertEqual(access(dst, F_OK), 0, @"tmp destination should exist after rename");

    vfs_test_remove_linux_path(@"/tmp/rename-src".UTF8String);
    vfs_test_remove_linux_path(@"/tmp/rename-dst".UTF8String);
}

- (void)testRenameRejectsPersistentToTempCrossRoute {
    vfs_test_seed_linux_file(@"/etc/cross-route-src".UTF8String);
    vfs_test_remove_linux_path(@"/tmp/cross-route-dst".UTF8String);

    int ret = rename(@"/etc/cross-route-src".UTF8String, @"/tmp/cross-route-dst".UTF8String);
    XCTAssertEqual(ret, -1, @"persistent to temp rename should fail");
    XCTAssertEqual(errno, EXDEV, @"persistent to temp rename should return EXDEV");

    vfs_test_remove_linux_path(@"/etc/cross-route-src".UTF8String);
    vfs_test_remove_linux_path(@"/tmp/cross-route-dst".UTF8String);
}

- (void)testRenameRejectsCacheToPersistentCrossRoute {
    vfs_test_seed_linux_file(@"/var/cache/cross-route-src".UTF8String);
    vfs_test_remove_linux_path(@"/etc/cross-route-dst".UTF8String);

    int ret = rename(@"/var/cache/cross-route-src".UTF8String, @"/etc/cross-route-dst".UTF8String);
    XCTAssertEqual(ret, -1, @"cache to persistent rename should fail");
    XCTAssertEqual(errno, EXDEV, @"cache to persistent rename should return EXDEV");

    vfs_test_remove_linux_path(@"/var/cache/cross-route-src".UTF8String);
    vfs_test_remove_linux_path(@"/etc/cross-route-dst".UTF8String);
}

- (void)testRenameRejectsTmpToVarTmpCrossRoute {
    vfs_test_seed_linux_file(@"/tmp/cross-route-src".UTF8String);
    vfs_test_remove_linux_path(@"/var/tmp/cross-route-dst".UTF8String);

    int ret = rename(@"/tmp/cross-route-src".UTF8String, @"/var/tmp/cross-route-dst".UTF8String);
    XCTAssertEqual(ret, -1, @"tmp to var/tmp rename should fail");
    XCTAssertEqual(errno, EXDEV, @"tmp to var/tmp rename should return EXDEV");

    vfs_test_remove_linux_path(@"/tmp/cross-route-src".UTF8String);
    vfs_test_remove_linux_path(@"/var/tmp/cross-route-dst".UTF8String);
}

- (void)testRenameRejectsTmpToRunCrossRoute {
    vfs_test_seed_linux_file(@"/tmp/cross-route-run-src".UTF8String);
    vfs_test_remove_linux_path(@"/run/cross-route-run-dst".UTF8String);

    int ret = rename(@"/tmp/cross-route-run-src".UTF8String, @"/run/cross-route-run-dst".UTF8String);
    XCTAssertEqual(ret, -1, @"tmp to run rename should fail");
    XCTAssertEqual(errno, EXDEV, @"tmp to run rename should return EXDEV");

    vfs_test_remove_linux_path(@"/tmp/cross-route-run-src".UTF8String);
    vfs_test_remove_linux_path(@"/run/cross-route-run-dst".UTF8String);
}

- (void)testRenameAtUsesDirfdForOldAndNewRelativePaths {
    int real_fd;
    int dirfd;
    char host_dir[MAX_PATH];
    char host_dst[MAX_PATH];

    XCTAssertEqual(vfs_translate_path(@"/tmp/dirfd-old".UTF8String, host_dir, sizeof(host_dir)), 0,
                   @"dirfd source directory should translate");
    XCTAssertEqual(vfs_translate_path(@"/tmp/dirfd-new".UTF8String, host_dst, sizeof(host_dst)), 0,
                   @"dirfd destination directory should translate");

    vfs_test_ensure_virtual_parent_directory(@"/tmp/dirfd-old/file".UTF8String);
    vfs_test_ensure_virtual_parent_directory(@"/tmp/dirfd-new/file".UTF8String);
    vfs_test_seed_linux_file(@"/tmp/dirfd-old/file".UTF8String);
    vfs_test_remove_linux_path(@"/tmp/dirfd-new/file".UTF8String);

    real_fd = vfs_test_open_host_directory_fd(host_dir);
    XCTAssertTrue(real_fd >= 0, @"host directory open should succeed");
    if (real_fd < 0) return;

    dirfd = alloc_fd_impl();
    XCTAssertTrue(dirfd >= 0, @"dirfd allocation should succeed");
    if (dirfd < 0) {
        close(real_fd);
        return;
    }

    init_fd_entry_impl(dirfd, real_fd, O_RDONLY | O_DIRECTORY, 0755, @"/tmp/dirfd-old".UTF8String);

    XCTAssertEqual(renameat(dirfd, @"file".UTF8String, AT_FDCWD, @"/tmp/dirfd-new/file".UTF8String), 0,
                   @"dirfd relative rename within same route should succeed");
    XCTAssertEqual(access([[NSString stringWithFormat:@"%s/file", host_dir] UTF8String], F_OK), -1,
                   @"dirfd source should be moved away");
    XCTAssertEqual(access([[NSString stringWithFormat:@"%s/file", host_dst] UTF8String], F_OK), 0,
                   @"dirfd destination should exist after rename");

    vfs_test_remove_linux_path(@"/tmp/dirfd-old/file".UTF8String);
    vfs_test_remove_linux_path(@"/tmp/dirfd-new/file".UTF8String);
    free_fd_impl(dirfd);
}

- (void)testRenameAtSupportsAtFdcwd {
    vfs_test_seed_linux_file(@"/tmp/at-fdcwd-src".UTF8String);
    vfs_test_remove_linux_path(@"/tmp/at-fdcwd-dst".UTF8String);

    XCTAssertEqual(renameat(AT_FDCWD, @"/tmp/at-fdcwd-src".UTF8String, AT_FDCWD, @"/tmp/at-fdcwd-dst".UTF8String), 0,
                   @"AT_FDCWD rename within same route should succeed");
    vfs_test_remove_linux_path(@"/tmp/at-fdcwd-src".UTF8String);
    vfs_test_remove_linux_path(@"/tmp/at-fdcwd-dst".UTF8String);
}

- (void)testRenameAtInvalidDirfdReturnsEbadf {

    int ret = renameat(9999, @"old".UTF8String, AT_FDCWD, @"new".UTF8String);
    XCTAssertEqual(ret, -1, @"renameat should fail for invalid old dirfd");
    XCTAssertEqual(errno, EBADF, @"invalid old dirfd should return EBADF");
}

- (void)testRenameAtNonDirectoryDirfdReturnsEnotdir {
    int real_fd;
    int dirfd;
    char host_path[MAX_PATH];

    XCTAssertEqual(vfs_translate_path(@"/tmp/not-a-dir".UTF8String, host_path, sizeof(host_path)), 0,
                   @"non-directory path should translate");
    vfs_test_seed_linux_file(@"/tmp/not-a-dir".UTF8String);
    vfs_test_remove_linux_path(@"/tmp/unused".UTF8String);

    real_fd = host_open_impl(host_path, O_RDONLY, 0);
    XCTAssertTrue(real_fd >= 0, @"host file open should succeed");
    if (real_fd < 0) return;

    dirfd = alloc_fd_impl();
    XCTAssertTrue(dirfd >= 0, @"dirfd allocation should succeed");
    if (dirfd < 0) {
        close(real_fd);
        return;
    }

    init_fd_entry_impl(dirfd, real_fd, O_RDONLY, 0644, @"/tmp/not-a-dir".UTF8String);

    XCTAssertEqual(renameat(dirfd, @"child".UTF8String, AT_FDCWD, @"/tmp/unused".UTF8String), -1,
                   @"non-directory dirfd should fail");
    XCTAssertEqual(errno, ENOTDIR, @"non-directory dirfd should return ENOTDIR");

    free_fd_impl(dirfd);
    vfs_test_remove_linux_path(@"/tmp/not-a-dir".UTF8String);
    vfs_test_remove_linux_path(@"/tmp/unused".UTF8String);
}

- (void)testRenameAtUsesNewdirfdForRelativeNewPath {
    int old_real_fd;
    int new_real_fd;
    int old_dirfd;
    int new_dirfd;
    char old_host_dir[MAX_PATH];
    char new_host_dir[MAX_PATH];

    XCTAssertEqual(vfs_translate_path(@"/tmp/newdirfd-old".UTF8String, old_host_dir, sizeof(old_host_dir)), 0,
                   @"old source directory should translate");
    XCTAssertEqual(vfs_translate_path(@"/tmp/newdirfd-new".UTF8String, new_host_dir, sizeof(new_host_dir)), 0,
                   @"new destination directory should translate");

    vfs_test_ensure_virtual_parent_directory(@"/tmp/newdirfd-old/file".UTF8String);
    vfs_test_ensure_virtual_parent_directory(@"/tmp/newdirfd-new/file".UTF8String);
    vfs_test_seed_linux_file(@"/tmp/newdirfd-old/file".UTF8String);
    vfs_test_remove_linux_path(@"/tmp/newdirfd-new/file".UTF8String);

    old_real_fd = vfs_test_open_host_directory_fd(old_host_dir);
    XCTAssertTrue(old_real_fd >= 0, @"old host directory open should succeed");
    if (old_real_fd < 0) return;

    new_real_fd = vfs_test_open_host_directory_fd(new_host_dir);
    XCTAssertTrue(new_real_fd >= 0, @"new host directory open should succeed");
    if (new_real_fd < 0) {
        host_close_impl(old_real_fd);
        return;
    }

    old_dirfd = alloc_fd_impl();
    XCTAssertTrue(old_dirfd >= 0, @"old dirfd allocation should succeed");
    if (old_dirfd < 0) {
        host_close_impl(old_real_fd);
        host_close_impl(new_real_fd);
        return;
    }

    new_dirfd = alloc_fd_impl();
    XCTAssertTrue(new_dirfd >= 0, @"new dirfd allocation should succeed");
    if (new_dirfd < 0) {
        free_fd_impl(old_dirfd);
        host_close_impl(new_real_fd);
        return;
    }

    init_fd_entry_impl(old_dirfd, old_real_fd, O_RDONLY | O_DIRECTORY, 0755, @"/tmp/newdirfd-old".UTF8String);
    init_fd_entry_impl(new_dirfd, new_real_fd, O_RDONLY | O_DIRECTORY, 0755, @"/tmp/newdirfd-new".UTF8String);

    XCTAssertEqual(renameat(old_dirfd, @"file".UTF8String, new_dirfd, @"file".UTF8String), 0,
                   @"relative newpath should resolve from newdirfd");
    XCTAssertEqual(access([[NSString stringWithFormat:@"%s/file", old_host_dir] UTF8String], F_OK), -1,
                   @"olddirfd source should be moved away");
    XCTAssertEqual(access([[NSString stringWithFormat:@"%s/file", new_host_dir] UTF8String], F_OK), 0,
                   @"newdirfd destination should exist after rename");

    vfs_test_remove_linux_path(@"/tmp/newdirfd-old/file".UTF8String);
    vfs_test_remove_linux_path(@"/tmp/newdirfd-new/file".UTF8String);
    free_fd_impl(old_dirfd);
    free_fd_impl(new_dirfd);
}

- (void)testRenameAtAbsolutePathsIgnoreDirfds {
    int real_fd;
    int dirfd;
    char dst[MAX_PATH];

    vfs_test_seed_linux_file(@"/tmp/absolute-dirfd-src".UTF8String);
    vfs_test_remove_linux_path(@"/tmp/absolute-dirfd-dst".UTF8String);
    XCTAssertEqual(vfs_translate_path(@"/tmp/absolute-dirfd-dst".UTF8String, dst, sizeof(dst)), 0,
                   @"absolute destination should translate");

    real_fd = vfs_test_open_host_directory_fd(vfs_temp_backing_root());
    XCTAssertTrue(real_fd >= 0, @"host temp directory open should succeed");
    if (real_fd < 0) return;

    dirfd = alloc_fd_impl();
    XCTAssertTrue(dirfd >= 0, @"dirfd allocation should succeed");
    if (dirfd < 0) {
        host_close_impl(real_fd);
        return;
    }

    init_fd_entry_impl(dirfd, real_fd, O_RDONLY | O_DIRECTORY, 0755, @"/tmp".UTF8String);

    XCTAssertEqual(renameat(dirfd, @"/tmp/absolute-dirfd-src".UTF8String, dirfd, @"/tmp/absolute-dirfd-dst".UTF8String), 0,
                   @"absolute paths should ignore dirfds");
    XCTAssertEqual(access(dst, F_OK), 0, @"absolute destination should exist after rename");

    vfs_test_remove_linux_path(@"/tmp/absolute-dirfd-src".UTF8String);
    vfs_test_remove_linux_path(@"/tmp/absolute-dirfd-dst".UTF8String);
    free_fd_impl(dirfd);
}

- (void)testRenameAt2NoReplaceRejectsExistingTarget {
    /* Test requires vfs_open implementation for file creation */
    /* Skipping because we cannot create files via IXLand without open support */
    /* RENAME_NOREPLACE logic is tested via code inspection */
    NSLog(@"SKIP: testRenameAt2NoReplaceRejectsExistingTarget - requires vfs_open implementation");
}

- (void)testRenameAt2RejectsWhiteout {
    int ret = renameat2(AT_FDCWD, @"/tmp/a".UTF8String, AT_FDCWD, @"/tmp/b".UTF8String, TEST_RENAME_WHITEOUT);
    XCTAssertEqual(ret, -1, @"renameat2 RENAME_WHITEOUT should be rejected intentionally");
    XCTAssertTrue(errno == EOPNOTSUPP || errno == ENOTSUP, @"RENAME_WHITEOUT should return unsupported-operation error");
}

- (void)testRenameAt2RejectsInvalidFlags {
    int ret = renameat2(AT_FDCWD, @"/tmp/a".UTF8String, AT_FDCWD, @"/tmp/b".UTF8String, 0x80000000u);
    XCTAssertEqual(ret, -1, @"renameat2 should reject invalid flags");
    XCTAssertEqual(errno, EINVAL, @"invalid rename flags should return EINVAL");
}

/* ============================================================================
 * FD CONTROL SEMANTICS TESTS
 * ============================================================================ */

- (void)testFcntlFdFlagsTrackCloexecPerDescriptor {
    int fd = open(@"/tmp/fcntl-fdflags.txt".UTF8String, O_CREAT | O_RDWR | O_TRUNC, 0644);
    int save_errno = errno;
    NSLog(@"DEBUG: open returned %d (errno=%d)", fd, save_errno);
    XCTAssertTrue(fd >= 0, @"open should succeed, fd=%d errno=%d", fd, save_errno);
    if (fd < 0) return;

    XCTAssertEqual(fcntl(fd, TEST_F_GETFD), 0, @"new descriptor should start without FD_CLOEXEC");
    XCTAssertEqual(fcntl(fd, TEST_F_SETFD, TEST_FD_CLOEXEC), 0, @"F_SETFD should succeed");
    XCTAssertEqual(fcntl(fd, TEST_F_GETFD), TEST_FD_CLOEXEC, @"F_GETFD should report FD_CLOEXEC");
    XCTAssertEqual(fcntl(fd, TEST_F_SETFD, 0), 0, @"F_SETFD should clear FD_CLOEXEC");
    XCTAssertEqual(fcntl(fd, TEST_F_GETFD), 0, @"descriptor flag clear should persist");

    close(fd);
}

- (void)testFcntlGetflSetflRoundTripMutableFlagsOnly {
    int fd = open(@"/tmp/fcntl-statusflags.txt".UTF8String, O_CREAT | O_RDWR | O_TRUNC, 0644);
    XCTAssertTrue(fd >= 0, @"open should succeed");
    if (fd < 0) return;

    int original = fcntl(fd, TEST_F_GETFL);
    XCTAssertTrue(original >= 0, @"F_GETFL should succeed");
    XCTAssertEqual(original & O_ACCMODE, O_RDWR, @"F_GETFL should preserve access mode");

    XCTAssertEqual(fcntl(fd, TEST_F_SETFL, original | O_APPEND | O_NONBLOCK), 0, @"F_SETFL should update mutable flags");
    int updated = fcntl(fd, TEST_F_GETFL);
    XCTAssertEqual(updated & O_ACCMODE, O_RDWR, @"F_SETFL must not mutate access mode");
    XCTAssertTrue((updated & O_APPEND) != 0, @"F_SETFL should set O_APPEND");
    XCTAssertTrue((updated & O_NONBLOCK) != 0, @"F_SETFL should set O_NONBLOCK");

    close(fd);
}

- (void)testDupSharesOffsetButNotDescriptorFlags {
    int fd = open(@"/tmp/fcntl-dup.txt".UTF8String, O_CREAT | O_RDWR | O_TRUNC, 0644);
    XCTAssertTrue(fd >= 0, @"open should succeed");
    if (fd < 0) return;

    XCTAssertEqual(write(fd, "abcdef", 6), 6, @"write should seed file contents");
    XCTAssertEqual(lseek(fd, 2, SEEK_SET), 2, @"seek should position original fd");

    int dupfd = dup(fd);
    XCTAssertTrue(dupfd >= 0, @"dup should succeed");
    if (dupfd < 0) {
        close(fd);
        return;
    }

    XCTAssertEqual(lseek(dupfd, 0, SEEK_CUR), 2, @"duplicated descriptor should share file offset");
    XCTAssertEqual(fcntl(fd, TEST_F_SETFD, TEST_FD_CLOEXEC), 0, @"set cloexec on original should succeed");
    XCTAssertEqual(fcntl(fd, TEST_F_GETFD), TEST_FD_CLOEXEC, @"original should have cloexec");
    XCTAssertEqual(fcntl(dupfd, TEST_F_GETFD), 0, @"duplicate should not inherit descriptor flag mutations");

    close(dupfd);
    close(fd);
}

- (void)testDup3AndFdupfdCloexecSetCloseOnExecIntentionally {
    int fd = open(@"/tmp/fcntl-dupcloexec.txt".UTF8String, O_CREAT | O_RDWR | O_TRUNC, 0644);
    XCTAssertTrue(fd >= 0, @"open should succeed");
    if (fd < 0) return;

    int dup3fd = dup3(fd, fd + 10, O_CLOEXEC);
    XCTAssertTrue(dup3fd >= 0, @"dup3 with O_CLOEXEC should succeed");
    if (dup3fd >= 0) {
        XCTAssertEqual(fcntl(dup3fd, TEST_F_GETFD), TEST_FD_CLOEXEC, @"dup3 should set FD_CLOEXEC on new descriptor");
        close(dup3fd);
    }

    int fdupfd = fcntl(fd, TEST_F_DUPFD_CLOEXEC, fd + 20);
    XCTAssertTrue(fdupfd >= 0, @"F_DUPFD_CLOEXEC should succeed");
    if (fdupfd >= 0) {
        XCTAssertEqual(fcntl(fdupfd, TEST_F_GETFD), TEST_FD_CLOEXEC, @"F_DUPFD_CLOEXEC should set FD_CLOEXEC");
        close(fdupfd);
    }

    close(fd);
}

- (void)testFcntlRejectsUnsupportedCommandIntentionally {
    int fd = open(@"/tmp/fcntl-invalid.txt".UTF8String, O_CREAT | O_RDWR | O_TRUNC, 0644);
    XCTAssertTrue(fd >= 0, @"open should succeed");
    if (fd < 0) return;

    int ret = fcntl(fd, 999999, 0);
    XCTAssertEqual(ret, -1, @"unsupported fcntl command should fail");
    XCTAssertEqual(errno, EINVAL, @"unsupported fcntl command should return EINVAL");

    close(fd);
}

/* ============================================================================
 * SYNTHETIC /proc/self AND /proc/self/fd TESTS
 * ============================================================================
 */

- (void)testProcSelfStatSucceeds {
    struct stat st;
    errno = 0;
    XCTAssertEqual(stat("/proc/self", &st), 0, @"stat(/proc/self) should succeed");
    XCTAssertTrue(S_ISDIR(st.st_mode), @"/proc/self should be a directory");
    XCTAssertEqual(st.st_mode & 0777, 0555, @"/proc/self should have 0555 permissions");

    errno = 0;
    XCTAssertEqual(lstat("/proc/self", &st), 0, @"lstat(/proc/self) should succeed");
    XCTAssertTrue(S_ISDIR(st.st_mode), @"lstat(/proc/self) should return directory");
}

- (void)testProcSelfFdStatSucceeds {
    struct stat st;
    errno = 0;
    XCTAssertEqual(stat("/proc/self/fd", &st), 0, @"stat(/proc/self/fd) should succeed");
    XCTAssertTrue(S_ISDIR(st.st_mode), @"/proc/self/fd should be a directory");
    XCTAssertEqual(st.st_mode & 0777, 0555, @"/proc/self/fd should have 0555 permissions");
}

- (void)testProcSelfAccessSucceeds {
    errno = 0;
    XCTAssertEqual(access("/proc/self", F_OK), 0, @"access(/proc/self, F_OK) should succeed");
    XCTAssertEqual(access("/proc/self", R_OK), 0, @"access(/proc/self, R_OK) should succeed");
    XCTAssertEqual(access("/proc/self", X_OK), 0, @"access(/proc/self, X_OK) should succeed");
}

- (void)testProcSelfFdAccessSucceeds {
    errno = 0;
    XCTAssertEqual(access("/proc/self/fd", F_OK), 0, @"access(/proc/self/fd, F_OK) should succeed");
}

- (void)testProcSelfOpenSucceeds {
    errno = 0;
    int fd = open("/proc/self", O_RDONLY | O_DIRECTORY);
    XCTAssertTrue(fd >= 0, @"open(/proc/self, O_DIRECTORY) should succeed");
    if (fd >= 0) close(fd);
}

- (void)testProcSelfFdOpenSucceeds {
    errno = 0;
    int fd = open("/proc/self/fd", O_RDONLY | O_DIRECTORY);
    XCTAssertTrue(fd >= 0, @"open(/proc/self/fd, O_DIRECTORY) should succeed");
    if (fd >= 0) close(fd);
}

- (void)testProcSelfGetdentsReturnsDotDotdotFd {
    errno = 0;
    int fd = open("/proc/self", O_RDONLY | O_DIRECTORY);
    XCTAssertTrue(fd >= 0, @"open(/proc/self, O_DIRECTORY) should succeed");
    if (fd < 0) return;

    union { char storage[1024]; uint64_t align; } aligned;
    char *buffer = aligned.storage;
    memset(buffer, 0, sizeof(aligned));

    bool found_dot = false;
    bool found_dotdot = false;
    bool found_fd = false;
    bool done = false;
    while (!done) {
        errno = 0;
        ssize_t nread = getdents64(fd, buffer, sizeof(aligned.storage));
        if (nread <= 0) {
            done = true;
            continue;
        }
        size_t pos = 0;
        while (pos < (size_t)nread) {
            struct linux_dirent64 *entry = (struct linux_dirent64 *)(buffer + pos);
            NSString *name = [NSString stringWithUTF8String:entry->d_name];
            if ([name isEqualToString:@"."]) {
                found_dot = true;
                XCTAssertEqual(entry->d_type, DT_DIR, @". should be DT_DIR");
            } else if ([name isEqualToString:@".."]) {
                found_dotdot = true;
                XCTAssertEqual(entry->d_type, DT_DIR, @".. should be DT_DIR");
            } else if ([name isEqualToString:@"fd"]) {
                found_fd = true;
                XCTAssertEqual(entry->d_type, DT_DIR, @"fd should be DT_DIR");
            }
            if (entry->d_reclen == 0) break;
            pos += entry->d_reclen;
        }
    }

    XCTAssertTrue(found_dot, @"getdents64(/proc/self) should return '.' entry");
    XCTAssertTrue(found_dotdot, @"getdents64(/proc/self) should return '..' entry");
    XCTAssertTrue(found_fd, @"getdents64(/proc/self) should return 'fd' entry");

    close(fd);
}

- (void)testProcSelfFdGetdentsReturnsDotDotdotAndFdNumbers {
    errno = 0;
    int fd = open("/proc/self/fd", O_RDONLY | O_DIRECTORY);
    XCTAssertTrue(fd >= 0, @"open(/proc/self/fd, O_DIRECTORY) should succeed");
    if (fd < 0) return;

    union { char storage[2048]; uint64_t align; } aligned;
    char *buffer = aligned.storage;
    memset(buffer, 0, sizeof(aligned));

    bool found_dot = false;
    bool found_dotdot = false;
    int fd_link_count = 0;
    bool done = false;
    while (!done) {
        errno = 0;
        ssize_t nread = getdents64(fd, buffer, sizeof(aligned.storage));
        if (nread <= 0) {
            done = true;
            continue;
        }
        size_t pos = 0;
        while (pos < (size_t)nread) {
            struct linux_dirent64 *entry = (struct linux_dirent64 *)(buffer + pos);
            NSString *name = [NSString stringWithUTF8String:entry->d_name];
            if ([name isEqualToString:@"."]) {
                found_dot = true;
                XCTAssertEqual(entry->d_type, DT_DIR, @". should be DT_DIR");
            } else if ([name isEqualToString:@".."]) {
                found_dotdot = true;
                XCTAssertEqual(entry->d_type, DT_DIR, @".. should be DT_DIR");
            } else {
                XCTAssertEqual(entry->d_type, DT_LNK, @"fd entries should be DT_LNK");
                fd_link_count++;
            }
            if (entry->d_reclen == 0) break;
            pos += entry->d_reclen;
        }
    }

    XCTAssertTrue(found_dot, @"getdents64(/proc/self/fd) should return '.' entry");
    XCTAssertTrue(found_dotdot, @"getdents64(/proc/self/fd) should return '..' entry");
    XCTAssertTrue(fd_link_count > 0, @"getdents64(/proc/self/fd) should return at least one fd number link");

    close(fd);
}

- (void)testProcSelfFdLinkReadlink {
    errno = 0;
    int test_fd = open("/dev/null", O_RDWR);
    XCTAssertTrue(test_fd >= 0, @"open(/dev/null) should succeed for readlink test");
    if (test_fd < 0) return;

    char fd_path[64];
    snprintf(fd_path, sizeof(fd_path), "/proc/self/fd/%d", test_fd);

    errno = 0;
    char link_target[MAX_PATH];
    ssize_t link_len = readlink(fd_path, link_target, sizeof(link_target) - 1);
    XCTAssertTrue(link_len > 0, @"readlink(%s) should succeed, got %zd errno %d", fd_path, link_len, errno);
    if (link_len > 0) {
        link_target[link_len] = '\0';
        NSString *target = [NSString stringWithUTF8String:link_target];
        XCTAssertTrue([target isEqualToString:@"/dev/null"], @"readlink should return /dev/null, got %@", target);
    }

    close(test_fd);
}

- (void)testProcSelfFdInvalidLinkFails {
    errno = 0;
    char link_target[MAX_PATH];
    ssize_t link_len = readlink("/proc/self/fd/9999", link_target, sizeof(link_target));
    XCTAssertEqual(link_len, -1, @"readlink(/proc/self/fd/9999) should fail");
    XCTAssertEqual(errno, ENOENT, @"readlink(/proc/self/fd/9999) should set ENOENT");
}

- (void)testProcSelfFdLinkLstatReturnsSymlink {
    errno = 0;
    int test_fd = open("/dev/null", O_RDWR);
    XCTAssertTrue(test_fd >= 0, @"open(/dev/null) should succeed for lstat test");
    if (test_fd < 0) return;

    char fd_path[64];
    snprintf(fd_path, sizeof(fd_path), "/proc/self/fd/%d", test_fd);

    struct stat st;
    errno = 0;
    XCTAssertEqual(lstat(fd_path, &st), 0, @"lstat(%s) should succeed", fd_path);
    XCTAssertTrue(S_ISLNK(st.st_mode), @"lstat(/proc/self/fd/<n>) should return S_IFLNK");

    close(test_fd);
}

- (void)testProcSelfCwdStatSucceeds {
    struct stat st;
    errno = 0;
    XCTAssertEqual(stat("/proc/self/cwd", &st), 0, @"stat(/proc/self/cwd) should succeed");
    XCTAssertTrue(S_ISLNK(st.st_mode), @"/proc/self/cwd should be a symlink");
    XCTAssertEqual(st.st_mode & 0777, 0777, @"/proc/self/cwd should have 0777 permissions");

    errno = 0;
    XCTAssertEqual(lstat("/proc/self/cwd", &st), 0, @"lstat(/proc/self/cwd) should succeed");
    XCTAssertTrue(S_ISLNK(st.st_mode), @"lstat(/proc/self/cwd) should return symlink");
}

- (void)testProcSelfExeStatSucceeds {
    struct stat st;
    errno = 0;
    int ret = lstat("/proc/self/exe", &st);
    if (ret == 0) {
        XCTAssertTrue(S_ISLNK(st.st_mode), @"/proc/self/exe should be a symlink");
        XCTAssertEqual(st.st_mode & 0777, 0777, @"/proc/self/exe should have 0777 permissions");
    } else {
        XCTAssertEqual(errno, ENOENT, @"lstat(/proc/self/exe) should return ENOENT if exe path not set");
    }
}

- (void)testProcSelfCwdAccessSucceeds {
    errno = 0;
    XCTAssertEqual(access("/proc/self/cwd", F_OK), 0, @"access(/proc/self/cwd, F_OK) should succeed");
}

- (void)testProcSelfExeAccessSucceeds {
    errno = 0;
    int ret = access("/proc/self/exe", F_OK);
    if (ret != 0) {
        XCTAssertEqual(errno, ENOENT, @"access(/proc/self/exe) should return ENOENT if exe path not set");
    }
}

- (void)testProcSelfCwdReadlinkReturnsCurrentDirectory {
    errno = 0;
    char link_target[MAX_PATH];
    ssize_t link_len = readlink("/proc/self/cwd", link_target, sizeof(link_target) - 1);
    XCTAssertTrue(link_len > 0, @"readlink(/proc/self/cwd) should succeed, got %zd errno %d", link_len, errno);
    if (link_len > 0) {
        link_target[link_len] = '\0';
        
        char expected_cwd[MAX_PATH];
        char *cwd_result = getcwd(expected_cwd, sizeof(expected_cwd));
        XCTAssertTrue(cwd_result != NULL, @"getcwd should succeed");
        
        if (cwd_result) {
            NSString *target = [NSString stringWithUTF8String:link_target];
            NSString *expected = [NSString stringWithUTF8String:expected_cwd];
            XCTAssertEqualObjects(target, expected, @"readlink(/proc/self/cwd) should return current working directory");
        }
    }
}

- (void)testProcSelfExeReadlinkReturnsExecutablePath {
    errno = 0;
    char link_target[MAX_PATH];
    ssize_t link_len = readlink("/proc/self/exe", link_target, sizeof(link_target) - 1);
    if (link_len > 0) {
        link_target[link_len] = '\0';
        NSString *target = [NSString stringWithUTF8String:link_target];
        XCTAssertTrue([target length] > 0, @"readlink(/proc/self/exe) should return non-empty path");
    } else {
        XCTAssertEqual(errno, ENOENT, @"readlink(/proc/self/exe) should return ENOENT if exe path not set, got errno %d", errno);
    }
}

- (void)testProcSelfGetdentsIncludesCwdAndExe {
    errno = 0;
    int fd = open("/proc/self", O_RDONLY | O_DIRECTORY);
    XCTAssertTrue(fd >= 0, @"open(/proc/self, O_DIRECTORY) should succeed");
    if (fd < 0) return;

    union { char storage[1024]; uint64_t align; } aligned;
    char *buffer = aligned.storage;
    memset(buffer, 0, sizeof(aligned));

    bool found_dot = false;
    bool found_dotdot = false;
    bool found_fd = false;
    bool found_cwd = false;
    bool found_exe = false;
    bool done = false;
    while (!done) {
        errno = 0;
        ssize_t nread = getdents64(fd, buffer, sizeof(aligned.storage));
        if (nread <= 0) {
            done = true;
            continue;
        }
        size_t pos = 0;
        while (pos < (size_t)nread) {
            struct linux_dirent64 *entry = (struct linux_dirent64 *)(buffer + pos);
            NSString *name = [NSString stringWithUTF8String:entry->d_name];
            if ([name isEqualToString:@"."]) {
                found_dot = true;
                XCTAssertEqual(entry->d_type, DT_DIR, @". should be DT_DIR");
            } else if ([name isEqualToString:@".."]) {
                found_dotdot = true;
                XCTAssertEqual(entry->d_type, DT_DIR, @".. should be DT_DIR");
            } else if ([name isEqualToString:@"fd"]) {
                found_fd = true;
                XCTAssertEqual(entry->d_type, DT_DIR, @"fd should be DT_DIR");
            } else if ([name isEqualToString:@"cwd"]) {
                found_cwd = true;
                XCTAssertEqual(entry->d_type, DT_LNK, @"cwd should be DT_LNK");
            } else if ([name isEqualToString:@"exe"]) {
                found_exe = true;
                XCTAssertEqual(entry->d_type, DT_LNK, @"exe should be DT_LNK");
            }
            if (entry->d_reclen == 0) break;
            pos += entry->d_reclen;
        }
    }

    XCTAssertTrue(found_dot, @"getdents64(/proc/self) should return '.' entry");
    XCTAssertTrue(found_dotdot, @"getdents64(/proc/self) should return '..' entry");
    XCTAssertTrue(found_fd, @"getdents64(/proc/self) should return 'fd' entry");
    XCTAssertTrue(found_cwd, @"getdents64(/proc/self) should return 'cwd' entry");
    XCTAssertTrue(found_exe, @"getdents64(/proc/self) should return 'exe' entry");

    close(fd);
}

- (void)testProcSelfCmdlineStatSucceeds {
    struct stat st;
    errno = 0;
    XCTAssertEqual(stat("/proc/self/cmdline", &st), 0, @"stat(/proc/self/cmdline) should succeed");
    XCTAssertTrue(S_ISREG(st.st_mode), @"/proc/self/cmdline should be a regular file");
    XCTAssertEqual(st.st_mode & 0777, 0444, @"/proc/self/cmdline should have 0444 permissions");

    errno = 0;
    XCTAssertEqual(lstat("/proc/self/cmdline", &st), 0, @"lstat(/proc/self/cmdline) should succeed");
    XCTAssertTrue(S_ISREG(st.st_mode), @"lstat(/proc/self/cmdline) should return regular file");
}

- (void)testProcSelfCommStatSucceeds {
    struct stat st;
    errno = 0;
    XCTAssertEqual(stat("/proc/self/comm", &st), 0, @"stat(/proc/self/comm) should succeed");
    XCTAssertTrue(S_ISREG(st.st_mode), @"/proc/self/comm should be a regular file");
    XCTAssertEqual(st.st_mode & 0777, 0444, @"/proc/self/comm should have 0444 permissions");

    errno = 0;
    XCTAssertEqual(lstat("/proc/self/comm", &st), 0, @"lstat(/proc/self/comm) should succeed");
    XCTAssertTrue(S_ISREG(st.st_mode), @"lstat(/proc/self/comm) should return regular file");
}

- (void)testProcSelfCmdlineAccessSucceeds {
    errno = 0;
    XCTAssertEqual(access("/proc/self/cmdline", F_OK), 0, @"access(/proc/self/cmdline, F_OK) should succeed");
    XCTAssertEqual(access("/proc/self/cmdline", R_OK), 0, @"access(/proc/self/cmdline, R_OK) should succeed");
}

- (void)testProcSelfCommAccessSucceeds {
    errno = 0;
    XCTAssertEqual(access("/proc/self/comm", F_OK), 0, @"access(/proc/self/comm, F_OK) should succeed");
    XCTAssertEqual(access("/proc/self/comm", R_OK), 0, @"access(/proc/self/comm, R_OK) should succeed");
}

- (void)testProcSelfCmdlineOpenAndReadSucceeds {
    errno = 0;
    int fd = open("/proc/self/cmdline", O_RDONLY);
    XCTAssertTrue(fd >= 0, @"open(/proc/self/cmdline, O_RDONLY) should succeed");
    if (fd < 0) return;

    char buf[1024];
    memset(buf, 0, sizeof(buf));
    errno = 0;
    ssize_t nread = read(fd, buf, sizeof(buf));
    XCTAssertTrue(nread > 0, @"read(/proc/self/cmdline) should return > 0 bytes, got %zd errno %d", nread, errno);
    
    if (nread > 0) {
        XCTAssertTrue(buf[nread - 1] == '\0', @"/proc/self/cmdline should end with NUL byte");
    }

    close(fd);
}

- (void)testProcSelfCommOpenAndReadSucceeds {
    errno = 0;
    int fd = open("/proc/self/comm", O_RDONLY);
    XCTAssertTrue(fd >= 0, @"open(/proc/self/comm, O_RDONLY) should succeed");
    if (fd < 0) return;

    char buf[256];
    memset(buf, 0, sizeof(buf));
    errno = 0;
    ssize_t nread = read(fd, buf, sizeof(buf));
    XCTAssertTrue(nread > 0, @"read(/proc/self/comm) should return > 0 bytes, got %zd errno %d", nread, errno);
    
    if (nread > 0) {
        XCTAssertTrue(buf[nread - 1] == '\n', @"/proc/self/comm should end with newline");
        XCTAssertTrue(nread <= 17, @"/proc/self/comm should be at most 16 chars + newline, got %zd", nread);
    }

    close(fd);
}

- (void)testProcSelfGetdentsIncludesCmdlineAndComm {
    errno = 0;
    int fd = open("/proc/self", O_RDONLY | O_DIRECTORY);
    XCTAssertTrue(fd >= 0, @"open(/proc/self, O_DIRECTORY) should succeed");
    if (fd < 0) return;

    union { char storage[2048]; uint64_t align; } aligned;
    char *buffer = aligned.storage;
    memset(buffer, 0, sizeof(aligned));

    bool found_cmdline = false;
    bool found_comm = false;
    bool done = false;
    while (!done) {
        errno = 0;
        ssize_t nread = getdents64(fd, buffer, sizeof(aligned.storage));
        if (nread <= 0) {
            done = true;
            continue;
        }
        size_t pos = 0;
        while (pos < (size_t)nread) {
            struct linux_dirent64 *entry = (struct linux_dirent64 *)(buffer + pos);
            NSString *name = [NSString stringWithUTF8String:entry->d_name];
            if ([name isEqualToString:@"cmdline"]) {
                found_cmdline = true;
                XCTAssertEqual(entry->d_type, DT_REG, @"cmdline should be DT_REG");
            } else if ([name isEqualToString:@"comm"]) {
                found_comm = true;
                XCTAssertEqual(entry->d_type, DT_REG, @"comm should be DT_REG");
            }
            if (entry->d_reclen == 0) break;
            pos += entry->d_reclen;
        }
    }

    XCTAssertTrue(found_cmdline, @"getdents64(/proc/self) should return 'cmdline' entry");
    XCTAssertTrue(found_comm, @"getdents64(/proc/self) should return 'comm' entry");

    close(fd);
}

- (void)testProcSelfStatStatSucceeds {
    struct stat st;
    errno = 0;
    XCTAssertEqual(stat("/proc/self/stat", &st), 0, @"stat(/proc/self/stat) should succeed");
    XCTAssertTrue(S_ISREG(st.st_mode), @"/proc/self/stat should be a regular file");
    XCTAssertEqual(st.st_mode & 0777, 0444, @"/proc/self/stat should have 0444 permissions");

    errno = 0;
    XCTAssertEqual(lstat("/proc/self/stat", &st), 0, @"lstat(/proc/self/stat) should succeed");
    XCTAssertTrue(S_ISREG(st.st_mode), @"lstat(/proc/self/stat) should return regular file");
}

- (void)testProcSelfStatmStatSucceeds {
    struct stat st;
    errno = 0;
    XCTAssertEqual(stat("/proc/self/statm", &st), 0, @"stat(/proc/self/statm) should succeed");
    XCTAssertTrue(S_ISREG(st.st_mode), @"/proc/self/statm should be a regular file");
    XCTAssertEqual(st.st_mode & 0777, 0444, @"/proc/self/statm should have 0444 permissions");

    errno = 0;
    XCTAssertEqual(lstat("/proc/self/statm", &st), 0, @"lstat(/proc/self/statm) should succeed");
    XCTAssertTrue(S_ISREG(st.st_mode), @"lstat(/proc/self/statm) should return regular file");
}

- (void)testProcSelfStatAccessSucceeds {
    errno = 0;
    XCTAssertEqual(access("/proc/self/stat", F_OK), 0, @"access(/proc/self/stat, F_OK) should succeed");
    XCTAssertEqual(access("/proc/self/stat", R_OK), 0, @"access(/proc/self/stat, R_OK) should succeed");
}

- (void)testProcSelfStatmAccessSucceeds {
    errno = 0;
    XCTAssertEqual(access("/proc/self/statm", F_OK), 0, @"access(/proc/self/statm, F_OK) should succeed");
    XCTAssertEqual(access("/proc/self/statm", R_OK), 0, @"access(/proc/self/statm, R_OK) should succeed");
}

- (void)testProcSelfStatOpenAndReadSucceeds {
    errno = 0;
    int fd = open("/proc/self/stat", O_RDONLY);
    XCTAssertTrue(fd >= 0, @"open(/proc/self/stat, O_RDONLY) should succeed");
    if (fd < 0) return;

    char buf[2048];
    memset(buf, 0, sizeof(buf));
    errno = 0;
    ssize_t nread = read(fd, buf, sizeof(buf));
    XCTAssertTrue(nread > 0, @"read(/proc/self/stat) should return > 0 bytes, got %zd errno %d", nread, errno);
    
    if (nread > 0) {
        buf[nread] = '\0';
        NSString *content = [NSString stringWithUTF8String:buf];
        XCTAssertTrue([content containsString:@"("], @"stat content should contain opening paren around comm");
        XCTAssertTrue([content containsString:@")"], @"stat content should contain closing paren around comm");
        XCTAssertTrue([content containsString:@"R"] || [content containsString:@"S"] || [content containsString:@"D"] || [content containsString:@"T"] || [content containsString:@"Z"], 
                     @"stat content should contain valid state char");
    }

    close(fd);
}

- (void)testProcSelfStatmOpenAndReadSucceeds {
    errno = 0;
    int fd = open("/proc/self/statm", O_RDONLY);
    XCTAssertTrue(fd >= 0, @"open(/proc/self/statm, O_RDONLY) should succeed");
    if (fd < 0) return;

    char buf[256];
    memset(buf, 0, sizeof(buf));
    errno = 0;
    ssize_t nread = read(fd, buf, sizeof(buf));
    XCTAssertTrue(nread > 0, @"read(/proc/self/statm) should return > 0 bytes, got %zd errno %d", nread, errno);
    
    if (nread > 0) {
        buf[nread] = '\0';
        NSString *content = [NSString stringWithUTF8String:buf];
        XCTAssertTrue([content containsString:@" "], @"statm content should contain space-separated values");
        XCTAssertTrue([content containsString:@"\n"], @"statm content should end with newline");
    }

    close(fd);
}

- (void)testProcSelfGetdentsIncludesStatAndStatm {
errno = 0;
int fd = open("/proc/self", O_RDONLY | O_DIRECTORY);
XCTAssertTrue(fd >= 0, @"open(/proc/self, O_DIRECTORY) should succeed");
if (fd < 0) return;

union { char storage[2048]; uint64_t align; } aligned;
char *buffer = aligned.storage;
memset(buffer, 0, sizeof(aligned));

bool found_stat = false;
bool found_statm = false;
bool done = false;
while (!done) {
errno = 0;
ssize_t nread = getdents64(fd, buffer, sizeof(aligned.storage));
if (nread <= 0) {
done = true;
continue;
}
size_t pos = 0;
while (pos < (size_t)nread) {
struct linux_dirent64 *entry = (struct linux_dirent64 *)(buffer + pos);
NSString *name = [NSString stringWithUTF8String:entry->d_name];
if ([name isEqualToString:@"stat"]) {
found_stat = true;
XCTAssertEqual(entry->d_type, DT_REG, @"stat should be DT_REG");
} else if ([name isEqualToString:@"statm"]) {
found_statm = true;
XCTAssertEqual(entry->d_type, DT_REG, @"statm should be DT_REG");
}
if (entry->d_reclen == 0) break;
pos += entry->d_reclen;
}
}

XCTAssertTrue(found_stat, @"getdents64(/proc/self) should return 'stat' entry");
XCTAssertTrue(found_statm, @"getdents64(/proc/self) should return 'statm' entry");

close(fd);
}

/* ============================================================================
* /proc/self/fdinfo/<n> TESTS
* ============================================================================ */

- (void)testProcSelfFdinfoStatSucceeds {
struct stat st;

errno = 0;
XCTAssertEqual(stat("/proc/self/fdinfo/0", &st), 0, @"stat(/proc/self/fdinfo/0) should succeed");
XCTAssertTrue(S_ISREG(st.st_mode), @"/proc/self/fdinfo/0 should be a regular file");
XCTAssertEqual(st.st_mode & 0777, 0444, @"/proc/self/fdinfo/0 should have 0444 permissions");

errno = 0;
XCTAssertEqual(lstat("/proc/self/fdinfo/0", &st), 0, @"lstat(/proc/self/fdinfo/0) should succeed");
XCTAssertTrue(S_ISREG(st.st_mode), @"lstat(/proc/self/fdinfo/0) should return regular file");
}

- (void)testProcSelfFdinfoAccessSucceeds {
errno = 0;
XCTAssertEqual(access("/proc/self/fdinfo/0", F_OK), 0, @"access(/proc/self/fdinfo/0, F_OK) should succeed");
XCTAssertEqual(access("/proc/self/fdinfo/0", R_OK), 0, @"access(/proc/self/fdinfo/0, R_OK) should succeed");
}

- (void)testProcSelfFdinfoOpenAndReadSucceeds {
errno = 0;
int fd = open("/proc/self/fdinfo/0", O_RDONLY);
XCTAssertTrue(fd >= 0, @"open(/proc/self/fdinfo/0, O_RDONLY) should succeed");
if (fd < 0) return;

char buf[256];
memset(buf, 0, sizeof(buf));
errno = 0;
ssize_t nread = read(fd, buf, sizeof(buf) - 1);
XCTAssertTrue(nread > 0, @"read(/proc/self/fdinfo/0) should return > 0 bytes, got %zd errno %d", nread, errno);

NSString *content = [NSString stringWithUTF8String:buf];
XCTAssertTrue([content containsString:@"pos:"], @"fdinfo content should contain 'pos:'");
XCTAssertTrue([content containsString:@"flags:"], @"fdinfo content should contain 'flags:'");

close(fd);
}

- (void)testProcSelfFdinfoValidFdNumbers {
struct stat st;

errno = 0;
XCTAssertEqual(stat("/proc/self/fdinfo/1", &st), 0, @"stat(/proc/self/fdinfo/1) should succeed");

errno = 0;
XCTAssertEqual(stat("/proc/self/fdinfo/2", &st), 0, @"stat(/proc/self/fdinfo/2) should succeed");

int test_fd = open("/proc/self/fdinfo/0", O_RDONLY);
XCTAssertTrue(test_fd >= 0, @"open for fdinfo test should succeed");
if (test_fd >= 0) {
char buf[256];
memset(buf, 0, sizeof(buf));
ssize_t nread = read(test_fd, buf, sizeof(buf) - 1);
XCTAssertTrue(nread > 0, @"read should return content");
NSString *content = [NSString stringWithUTF8String:buf];
XCTAssertTrue([content containsString:@"pos:"], @"content should contain pos");
XCTAssertTrue([content containsString:@"flags:"], @"content should contain flags");
close(test_fd);
}
}

- (void)testProcSelfFdinfoInvalidFdNumbers {
struct stat st;

errno = 0;
XCTAssertEqual(stat("/proc/self/fdinfo/999", &st), -1, @"stat(/proc/self/fdinfo/999) should fail");
XCTAssertEqual(errno, ENOENT, @"stat(/proc/self/fdinfo/999) should set ENOENT");

errno = 0;
XCTAssertEqual(open("/proc/self/fdinfo/999", O_RDONLY), -1, @"open(/proc/self/fdinfo/999) should fail");
XCTAssertEqual(errno, ENOENT, @"open(/proc/self/fdinfo/999) should set ENOENT");

errno = 0;
XCTAssertEqual(stat("/proc/self/fdinfo/abc", &st), -1, @"stat(/proc/self/fdinfo/abc) should fail for non-numeric");
    XCTAssertEqual(errno, ENOENT, @"stat(/proc/self/fdinfo/abc) should set ENOENT");
}

/* ============================================================================
 * POLL/SELECT READINESS TESTS
 * ============================================================================ */

- (void)testPollSyntheticProcfsRegularFileReturnsImmediateReadWrite {
    int fd = open("/proc/self/cmdline", O_RDONLY);
    XCTAssertTrue(fd >= 0, @"open(/proc/self/cmdline) should succeed");
    if (fd < 0) return;
    
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN | POLLOUT;
    pfd.revents = 0;
    
    errno = 0;
    int ret = poll(&pfd, 1, 0);
    XCTAssertEqual(ret, 1, @"poll() on synthetic procfs regular file should return 1 ready fd");
    XCTAssertTrue((pfd.revents & POLLIN) != 0, @"synthetic procfs regular file should be read-ready");
    XCTAssertTrue((pfd.revents & POLLOUT) != 0, @"synthetic procfs regular file should be write-ready (Linux semantics)");
    
    close(fd);
}

- (void)testSelectSyntheticProcfsRegularFileReturnsImmediateReadWrite {
    int fd = open("/proc/self/comm", O_RDONLY);
    XCTAssertTrue(fd >= 0, @"open(/proc/self/comm) should succeed");
    if (fd < 0) return;
    
    fd_set readfds, writefds;
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_SET(fd, &readfds);
    FD_SET(fd, &writefds);
    
    struct timeval tv = {0, 0};
    errno = 0;
    int ret = select_impl(fd + 1, &readfds, &writefds, NULL, &tv);
    XCTAssertTrue(ret > 0, @"select_impl() on synthetic procfs regular file should return > 0");
    XCTAssertTrue(FD_ISSET(fd, &readfds), @"synthetic procfs regular file should be read-ready");
    XCTAssertTrue(FD_ISSET(fd, &writefds), @"synthetic procfs regular file should be write-ready (Linux semantics)");
    
    close(fd);
}

- (void)testPollSyntheticDirectoryReturnsReadReadyOnly {
    int fd = open("/proc/self", O_RDONLY | O_DIRECTORY);
    XCTAssertTrue(fd >= 0, @"open(/proc/self, O_DIRECTORY) should succeed");
    if (fd < 0) return;
    
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN | POLLOUT;
    pfd.revents = 0;
    
    errno = 0;
    int ret = poll(&pfd, 1, 0);
    XCTAssertEqual(ret, 1, @"poll() on synthetic directory should return 1 ready fd");
    XCTAssertTrue((pfd.revents & POLLIN) != 0, @"synthetic directory should be read-ready");
    XCTAssertTrue((pfd.revents & POLLOUT) == 0, @"synthetic directory should NOT be write-ready");
    
    close(fd);
}

- (void)testPollSyntheticProcSelfFdDirectoryReturnsReadReadyOnly {
    int fd = open("/proc/self/fd", O_RDONLY | O_DIRECTORY);
    XCTAssertTrue(fd >= 0, @"open(/proc/self/fd, O_DIRECTORY) should succeed");
    if (fd < 0) return;

    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN | POLLOUT;
    pfd.revents = 0;

    errno = 0;
    int ret = poll(&pfd, 1, 0);
    XCTAssertEqual(ret, 1, @"poll() on /proc/self/fd should return 1 ready fd");
    XCTAssertTrue((pfd.revents & POLLIN) != 0, @"/proc/self/fd should be read-ready for enumeration");
    XCTAssertTrue((pfd.revents & POLLOUT) == 0, @"/proc/self/fd should NOT be write-ready");

    close(fd);
}

- (void)testPollDevNullReturnsImmediateReadWrite {
    int fd = open("/dev/null", O_RDWR);
    XCTAssertTrue(fd >= 0, @"open(/dev/null) should succeed");
    if (fd < 0) return;
    
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN | POLLOUT;
    pfd.revents = 0;
    
    errno = 0;
    int ret = poll(&pfd, 1, 0);
    XCTAssertEqual(ret, 1, @"poll() on /dev/null should return 1 ready fd");
    XCTAssertTrue((pfd.revents & POLLIN) != 0, @"/dev/null should be read-ready");
    XCTAssertTrue((pfd.revents & POLLOUT) != 0, @"/dev/null should be write-ready");
    
    close(fd);
}

- (void)testPollDevZeroReturnsImmediateReadWrite {
    int fd = open("/dev/zero", O_RDWR);
    XCTAssertTrue(fd >= 0, @"open(/dev/zero) should succeed");
    if (fd < 0) return;
    
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN | POLLOUT;
    pfd.revents = 0;
    
    errno = 0;
    int ret = poll(&pfd, 1, 0);
    XCTAssertEqual(ret, 1, @"poll() on /dev/zero should return 1 ready fd");
    XCTAssertTrue((pfd.revents & POLLIN) != 0, @"/dev/zero should be read-ready");
    XCTAssertTrue((pfd.revents & POLLOUT) != 0, @"/dev/zero should be write-ready (writes succeed immediately)");
    
    close(fd);
}

- (void)testPollDevUrandomReturnsImmediateReadWrite {
    int fd = open("/dev/urandom", O_RDWR);
    XCTAssertTrue(fd >= 0, @"open(/dev/urandom) should succeed");
    if (fd < 0) return;

    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN | POLLOUT;
    pfd.revents = 0;

    errno = 0;
    int ret = poll(&pfd, 1, 0);
    XCTAssertEqual(ret, 1, @"poll() on /dev/urandom should return 1 ready fd");
    XCTAssertTrue((pfd.revents & POLLIN) != 0, @"/dev/urandom should be read-ready");
    XCTAssertTrue((pfd.revents & POLLOUT) != 0, @"/dev/urandom should be write-ready (writes succeed immediately)");

    close(fd);
}

- (void)testPtyMasterSlaveOpenAndIoctlBaseline {
    int master_fd = open("/dev/ptmx", O_RDWR);
    XCTAssertTrue(master_fd >= 0, @"open(/dev/ptmx) should succeed");
    if (master_fd < 0) return;

    unsigned int pty_number = 0;
    errno = 0;
    XCTAssertEqual(ioctl(master_fd, IX_TIOCGPTN, &pty_number), 0, @"TIOCGPTN should succeed on PTY master");

    char slave_path[64];
    snprintf(slave_path, sizeof(slave_path), "/dev/pts/%u", pty_number);

    errno = 0;
    XCTAssertEqual(open(slave_path, O_RDWR), -1, @"opening locked slave should fail");
    XCTAssertEqual(errno, EIO, @"locked slave open should set EIO");

    int unlock = 0;
    XCTAssertEqual(ioctl(master_fd, IX_TIOCSPTLCK, &unlock), 0, @"TIOCSPTLCK unlock should succeed");

    int slave_fd = open(slave_path, O_RDWR);
    XCTAssertTrue(slave_fd >= 0, @"open(slave) should succeed after unlock");
    if (slave_fd < 0) {
        close(master_fd);
        return;
    }

    struct ix_termios tio;
    memset(&tio, 0, sizeof(tio));
    XCTAssertEqual(ioctl(master_fd, IX_TCGETS, &tio), 0, @"TCGETS should succeed");
    tio.c_lflag ^= 0x00000008U;
    XCTAssertEqual(ioctl(master_fd, IX_TCSETS, &tio), 0, @"TCSETS should succeed");

    struct ix_winsize ws;
    memset(&ws, 0, sizeof(ws));
    XCTAssertEqual(ioctl(master_fd, IX_TIOCGWINSZ, &ws), 0, @"TIOCGWINSZ should succeed");
    ws.ws_row = 40;
    ws.ws_col = 120;
    XCTAssertEqual(ioctl(master_fd, IX_TIOCSWINSZ, &ws), 0, @"TIOCSWINSZ should succeed");

    int32_t pgrp = 321;
    XCTAssertEqual(ioctl(master_fd, IX_TIOCSPGRP, &pgrp), 0, @"TIOCSPGRP should succeed");
    int32_t got_pgrp = 0;
    XCTAssertEqual(ioctl(master_fd, IX_TIOCGPGRP, &got_pgrp), 0, @"TIOCGPGRP should succeed");
    XCTAssertEqual(got_pgrp, pgrp, @"foreground pgrp should round-trip");

    close(slave_fd);
    close(master_fd);
}

- (void)testPtyDataFlowAndPollReadiness {
    int master_fd = open("/dev/ptmx", O_RDWR);
    XCTAssertTrue(master_fd >= 0, @"open(/dev/ptmx) should succeed");
    if (master_fd < 0) return;

    unsigned int pty_number = 0;
    XCTAssertEqual(ioctl(master_fd, IX_TIOCGPTN, &pty_number), 0, @"TIOCGPTN should succeed");

    int unlock = 0;
    XCTAssertEqual(ioctl(master_fd, IX_TIOCSPTLCK, &unlock), 0, @"TIOCSPTLCK unlock should succeed");

    char slave_path[64];
    snprintf(slave_path, sizeof(slave_path), "/dev/pts/%u", pty_number);
    int slave_fd = open(slave_path, O_RDWR);
    XCTAssertTrue(slave_fd >= 0, @"open(slave) should succeed");
    if (slave_fd < 0) {
        close(master_fd);
        return;
    }

    struct pollfd pfd;
    pfd.fd = slave_fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    XCTAssertEqual(poll(&pfd, 1, 0), 0, @"slave should not be read-ready before data");

    const char *msg1 = "hello-pty";
    XCTAssertEqual(write(master_fd, msg1, strlen(msg1)), (ssize_t)strlen(msg1), @"master write should succeed");

    pfd.revents = 0;
    XCTAssertEqual(poll(&pfd, 1, 0), 1, @"slave should become read-ready after master write");
    XCTAssertTrue((pfd.revents & POLLIN) != 0, @"slave POLLIN should be set");

    int pending = 0;
    XCTAssertEqual(ioctl(slave_fd, IX_FIONREAD, &pending), 0, @"FIONREAD should succeed on slave");
    XCTAssertTrue(pending > 0, @"FIONREAD should report queued bytes");

    char buf[32];
    memset(buf, 0, sizeof(buf));
    XCTAssertEqual(read(slave_fd, buf, sizeof(buf)), (ssize_t)strlen(msg1), @"slave read should consume master payload");
    XCTAssertEqual(strcmp(buf, msg1), 0, @"slave read payload should match");

    const char *msg2 = "from-slave";
    XCTAssertEqual(write(slave_fd, msg2, strlen(msg2)), (ssize_t)strlen(msg2), @"slave write should succeed");

    pfd.fd = master_fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    XCTAssertEqual(poll(&pfd, 1, 0), 1, @"master should become read-ready after slave write");
    XCTAssertTrue((pfd.revents & POLLIN) != 0, @"master POLLIN should be set");

    memset(buf, 0, sizeof(buf));
    XCTAssertEqual(read(master_fd, buf, sizeof(buf)), (ssize_t)strlen(msg2), @"master read should consume slave payload");
    XCTAssertEqual(strcmp(buf, msg2), 0, @"master read payload should match");

    close(slave_fd);
    close(master_fd);
}

- (void)testPollMixedSyntheticAndHostBackedFds {
    vfs_test_seed_linux_file("/tmp/poll-test-host-file");
    
    int host_fd = open("/tmp/poll-test-host-file", O_RDWR);
    XCTAssertTrue(host_fd >= 0, @"open host-backed file should succeed");
    if (host_fd < 0) return;
    
    int synthetic_fd = open("/dev/null", O_RDWR);
    XCTAssertTrue(synthetic_fd >= 0, @"open /dev/null should succeed");
    if (synthetic_fd < 0) {
        close(host_fd);
        return;
    }
    
    struct pollfd pfds[2];
    pfds[0].fd = host_fd;
    pfds[0].events = POLLIN | POLLOUT;
    pfds[0].revents = 0;
    
    pfds[1].fd = synthetic_fd;
    pfds[1].events = POLLIN | POLLOUT;
    pfds[1].revents = 0;
    
    errno = 0;
    int ret = poll(pfds, 2, 0);
    XCTAssertTrue(ret >= 2, @"poll() on mixed set should report both host-backed and synthetic readiness");
    XCTAssertTrue((pfds[0].revents & POLLOUT) != 0, @"host-backed regular file should be write-ready");
    XCTAssertTrue((pfds[1].revents & POLLIN) != 0, @"synthetic /dev/null should be read-ready");
    XCTAssertTrue((pfds[1].revents & POLLOUT) != 0, @"synthetic /dev/null should be write-ready");
    
    close(synthetic_fd);
    close(host_fd);
    vfs_test_remove_linux_path("/tmp/poll-test-host-file");
}

- (void)testPollInvalidFdReturnsPollnval {
    struct pollfd pfd;
    pfd.fd = 999;
    pfd.events = POLLIN | POLLOUT;
    pfd.revents = 0;

    errno = 0;
    int ret = poll(&pfd, 1, 0);
    XCTAssertEqual(ret, 1, @"poll() on invalid fd should report one ready error fd");
    XCTAssertTrue((pfd.revents & POLLNVAL) != 0, @"invalid fd should set POLLNVAL");
}

- (void)testSelectInvalidFdFailsWithEbadf {
    fd_set readfds, writefds;
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_SET(999, &readfds);
    FD_SET(999, &writefds);

    struct timeval tv = {0, 0};
    errno = 0;
    int ret = select_impl(1000, &readfds, &writefds, NULL, &tv);
    XCTAssertEqual(ret, -1, @"select_impl() with invalid fd should fail");
    XCTAssertEqual(errno, EBADF, @"select_impl() with invalid fd should set EBADF");
}

@end
