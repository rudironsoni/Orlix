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

/* Minimal standard headers */
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>

/* Standard system headers */
#include <dirent.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>

/* IXLand VFS types */
#include "fs/vfs.h"

/* Linux UAPI test support - provides Linux-sourced constants */
#include "IXLandSystemTests/LinuxUAPITestSupport.h"

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
#include "kernel/signal.h"
#include "internal/ios/fs/backing_io.h"
#include "runtime/native/registry.h"

extern char *getcwd_impl(char *buf, size_t size);
extern int openat_impl(int dirfd, const char *pathname, int flags, mode_t mode);
extern int renameat2(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, unsigned int flags);
extern int renameat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath);
extern int rename(const char *oldpath, const char *newpath);
extern int alloc_fd_impl(void);
extern void free_fd_impl(int fd);
extern void init_fd_entry_impl(int fd, int real_fd, int flags, uint32_t mode, const char *path);
extern off_t lseek(int fd, off_t offset, int whence);
extern int fcntl(int fd, int cmd, ...);
extern int dup(int oldfd);
extern int dup2(int oldfd, int newfd);
extern int dup3(int oldfd, int newfd, int flags);
extern int close(int fd);
extern ssize_t getdents64(int fd, void *dirp, size_t count);
extern int poll(struct pollfd *fds, nfds_t nfds, int timeout);
extern int select_impl(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds, struct timeval *timeout);
extern int execve(const char *pathname, char *const argv[], char *const envp[]);
extern int exec_build_script_argv_from_line(const char *shebang_line, const char *path, int argc, char **argv,
                                             char *interpreter_path, size_t interpreter_path_len,
                                             char **script_argv, int *script_argc);

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

static bool __attribute__((unused)) vfs_test_open_pty_pair(int *master_fd, int *slave_fd) {
    return ixland_test_pty_open_pair(master_fd, slave_fd) == 0;
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

/* IXLand internal stat-family functions using linux_stat */
extern int stat_impl(const char *pathname, struct linux_stat *statbuf);
extern int lstat_impl(const char *pathname, struct linux_stat *statbuf);
extern int fstat_impl(int fd, struct linux_stat *statbuf);
extern int fstatat_impl(int dirfd, const char *pathname, struct linux_stat *statbuf, int flags);
extern int vfs_fstatat(int dirfd, const char *pathname, struct linux_stat *statbuf, int flags);
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
    struct linux_stat st;
    int ret = vfs_fstatat(AT_FDCWD, @"/etc/passwd".UTF8String, &st, 0);

    XCTAssertEqual(ret, 0, @"vfs_fstatat with AT_FDCWD should succeed");
}

- (void)testVfsFstatatSupportsSymlinkNoFollow {
    struct linux_stat st;
    int ret = vfs_fstatat(AT_FDCWD, @"/etc/passwd".UTF8String, &st, TEST_AT_SYMLINK_NOFOLLOW);

    XCTAssertEqual(ret, 0, @"vfs_fstatat with AT_SYMLINK_NOFOLLOW should succeed");
}

- (void)testVfsFstatatRejectsInvalidFlags {
    struct linux_stat st;
    int ret = vfs_fstatat(AT_FDCWD, @"/etc/passwd".UTF8String, &st, 0x80000000);

    XCTAssertEqual(ret, -EINVAL, @"vfs_fstatat should reject invalid flags");
}

- (void)testSyntheticRootStatSucceeds {
    struct linux_stat st;

    XCTAssertEqual(vfs_fstatat(AT_FDCWD, @"/proc".UTF8String, &st, 0), 0,
                   @"synthetic root vfs_fstatat should succeed");
    XCTAssertTrue(ixland_test_uapi_mode_is_directory(st.st_mode), @"/proc root should be a directory");
    XCTAssertEqual(st.st_mode & 0777, 0555, @"/proc root should have 0555 permissions");

    XCTAssertEqual(vfs_fstatat(AT_FDCWD, @"/sys".UTF8String, &st, 0), 0,
                   @"synthetic root vfs_fstatat should succeed for /sys");
    XCTAssertTrue(ixland_test_uapi_mode_is_directory(st.st_mode), @"/sys root should be a directory");

    XCTAssertEqual(vfs_fstatat(AT_FDCWD, @"/dev".UTF8String, &st, 0), 0,
                   @"synthetic root vfs_fstatat should succeed for /dev");
    XCTAssertTrue(ixland_test_uapi_mode_is_directory(st.st_mode), @"/dev root should be a directory");

    errno = 0;
    XCTAssertEqual(stat_impl(@"/proc".UTF8String, &st), 0,
                   @"public stat should succeed for synthetic root");
    XCTAssertTrue(ixland_test_uapi_mode_is_directory(st.st_mode), @"public stat should return directory for /proc");

    errno = 0;
    XCTAssertEqual(lstat_impl(@"/sys".UTF8String, &st), 0,
                   @"public lstat should succeed for synthetic root");
    XCTAssertTrue(ixland_test_uapi_mode_is_directory(st.st_mode), @"public lstat should return directory for /sys");
}

- (void)testSyntheticChildStatFails {
    struct linux_stat st;

    XCTAssertEqual(vfs_fstatat(AT_FDCWD, @"/proc/meminfo".UTF8String, &st, 0), -ENOENT,
                   @"synthetic child vfs_fstatat should reject through descriptor policy");
    XCTAssertEqual(vfs_fstatat(AT_FDCWD, @"/sys/kernel".UTF8String, &st, TEST_AT_SYMLINK_NOFOLLOW), -ENOENT,
                   @"synthetic child vfs_fstatat lstat path should reject through descriptor policy");

    errno = 0;
    XCTAssertEqual(stat_impl(@"/proc/meminfo".UTF8String, &st), -1,
                   @"IXLand stat should reject unsupported synthetic child paths");
    XCTAssertEqual(errno, ENOENT, @"IXLand stat should set ENOENT for unsupported synthetic child paths");

    errno = 0;
    XCTAssertEqual(lstat_impl(@"/sys/kernel".UTF8String, &st), -1,
                   @"IXLand lstat should reject unsupported synthetic child paths");
    XCTAssertEqual(errno, ENOENT, @"IXLand lstat should set ENOENT for unsupported synthetic child paths");
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
    struct linux_stat st;
    errno = 0;
    XCTAssertEqual(stat_impl("/dev/null", &st), 0, @"stat(/dev/null) should succeed");
    XCTAssertTrue(ixland_test_uapi_mode_is_char_device(st.st_mode), @"/dev/null should be a character device");
    XCTAssertEqual(st.st_mode & 0777, 0666, @"/dev/null should have 0666 permissions");

    errno = 0;
    XCTAssertEqual(lstat_impl("/dev/null", &st), 0, @"lstat(/dev/null) should succeed");
    XCTAssertTrue(ixland_test_uapi_mode_is_char_device(st.st_mode), @"lstat(/dev/null) should return character device");
}

- (void)testDevZeroStatSucceeds {
    struct linux_stat st;
    errno = 0;
    XCTAssertEqual(stat_impl("/dev/zero", &st), 0, @"stat(/dev/zero) should succeed");
    XCTAssertTrue(ixland_test_uapi_mode_is_char_device(st.st_mode), @"/dev/zero should be a character device");
    XCTAssertEqual(st.st_mode & 0777, 0666, @"/dev/zero should have 0666 permissions");
}

- (void)testDevUrandomStatSucceeds {
    struct linux_stat st;
    errno = 0;
    XCTAssertEqual(stat_impl("/dev/urandom", &st), 0, @"stat(/dev/urandom) should succeed");
    XCTAssertTrue(ixland_test_uapi_mode_is_char_device(st.st_mode), @"/dev/urandom should be a character device");
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
    struct linux_stat st;
    XCTAssertEqual(stat_impl("/dev/sda", &st), -1, @"stat(/dev/sda) should fail for unsupported dev node");
    XCTAssertEqual(errno, ENOENT, @"stat(/dev/sda) should set ENOENT");

    struct task_struct *original_task = get_current();
    struct task_struct *isolated_task = alloc_task();
    XCTAssertTrue(isolated_task != NULL, @"task allocation should succeed");
    if (!isolated_task) return;

    isolated_task->fs = alloc_fs_struct();
    XCTAssertTrue(isolated_task->fs != NULL, @"fs_struct allocation should succeed");
    if (!isolated_task->fs) {
        free_task(isolated_task);
        return;
    }

    isolated_task->signal = alloc_signal_struct();
    XCTAssertTrue(isolated_task->signal != NULL, @"signal_struct allocation should succeed");
    if (!isolated_task->signal) {
        free_task(isolated_task);
        return;
    }

    fs_init_root(isolated_task->fs, @"/".UTF8String);
    fs_init_pwd(isolated_task->fs, @"/".UTF8String);
    set_current(isolated_task);

    errno = 0;
    XCTAssertEqual(open("/dev/tty", O_RDONLY), -1, @"open(/dev/tty) should fail without usable controlling tty");
    XCTAssertTrue(errno == ENXIO || errno == EIO, @"open(/dev/tty) should set ENXIO (no controlling tty) or EIO (unusable controlling tty)");

    set_current(original_task);
    free_task(isolated_task);
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

    vfs_test_seed_linux_file("/etc/rename-src");

    ret = rename("/etc/rename-src", "/etc/rename-dst");
    XCTAssertEqual(ret, 0, @"rename within persistent route should succeed");

    struct linux_stat st;
    XCTAssertEqual(stat_impl("/etc/rename-dst", &st), 0, @"rename destination should exist");
    XCTAssertEqual(stat_impl("/etc/rename-src", &st), -ENOENT, @"rename source should be gone");

    vfs_test_remove_linux_path("/etc/rename-dst");
}

- (void)testRenameatAllowsSameRoutePersistentMove {
    int ret;
    char src[MAX_PATH];
    char dst[MAX_PATH];

    ret = vfs_translate_path(@"/etc/renameat-src".UTF8String, src, sizeof(src));
    XCTAssertEqual(ret, 0, @"persistent source should translate");
    ret = vfs_translate_path(@"/etc/renameat-dst".UTF8String, dst, sizeof(dst));
    XCTAssertEqual(ret, 0, @"persistent destination should translate");

    vfs_test_seed_linux_file("/etc/renameat-src");

    ret = renameat(AT_FDCWD, "/etc/renameat-src", AT_FDCWD, "/etc/renameat-dst");
    XCTAssertEqual(ret, 0, @"renameat within persistent route should succeed");

    struct linux_stat st;
    XCTAssertEqual(stat_impl("/etc/renameat-dst", &st), 0, @"renameat destination should exist");
    XCTAssertEqual(stat_impl("/etc/renameat-src", &st), -ENOENT, @"renameat source should be gone");

    vfs_test_remove_linux_path("/etc/renameat-dst");
}

- (void)testRenameCrossRouteFails {
    int ret;
    char src[MAX_PATH];

    ret = vfs_translate_path(@"/etc/cross-src".UTF8String, src, sizeof(src));
    XCTAssertEqual(ret, 0, @"persistent source should translate");

    vfs_test_seed_linux_file("/etc/cross-src");

    ret = rename("/etc/cross-src", "/tmp/cross-dst");
    XCTAssertEqual(ret, -EXDEV, @"rename across routes should fail with EXDEV");

    vfs_test_remove_linux_path("/etc/cross-src");
}

- (void)testRenameatCrossRouteFails {
    int ret;
    char src[MAX_PATH];

    ret = vfs_translate_path(@"/etc/crossat-src".UTF8String, src, sizeof(src));
    XCTAssertEqual(ret, 0, @"persistent source should translate");

    vfs_test_seed_linux_file("/etc/crossat-src");

    ret = renameat(AT_FDCWD, "/etc/crossat-src", AT_FDCWD, "/tmp/crossat-dst");
    XCTAssertEqual(ret, -EXDEV, @"renameat across routes should fail with EXDEV");

    vfs_test_remove_linux_path("/etc/crossat-src");
}

- (void)testRenameat2SameRouteNoReplaceSucceeds {
    int ret;
    char src[MAX_PATH];
    char dst[MAX_PATH];

    ret = vfs_translate_path(@"/etc/rn2-src".UTF8String, src, sizeof(src));
    XCTAssertEqual(ret, 0, @"persistent source should translate");
    ret = vfs_translate_path(@"/etc/rn2-dst".UTF8String, dst, sizeof(dst));
    XCTAssertEqual(ret, 0, @"persistent destination should translate");

    vfs_test_seed_linux_file("/etc/rn2-src");

    ret = renameat2(AT_FDCWD, "/etc/rn2-src", AT_FDCWD, "/etc/rn2-dst", TEST_RENAME_NOREPLACE);
    XCTAssertEqual(ret, 0, @"renameat2 RENAME_NOREPLACE within persistent route should succeed");

    struct linux_stat st;
    XCTAssertEqual(stat_impl("/etc/rn2-dst", &st), 0, @"renameat2 destination should exist");
    XCTAssertEqual(stat_impl("/etc/rn2-src", &st), -ENOENT, @"renameat2 source should be gone");

    vfs_test_remove_linux_path("/etc/rn2-dst");
}

- (void)testRenameat2NoReplaceWithExistingDstFails {
    int ret;
    char src[MAX_PATH];
    char dst[MAX_PATH];

    ret = vfs_translate_path(@"/etc/rn2-exist-src".UTF8String, src, sizeof(src));
    XCTAssertEqual(ret, 0, @"persistent source should translate");
    ret = vfs_translate_path(@"/etc/rn2-exist-dst".UTF8String, dst, sizeof(dst));
    XCTAssertEqual(ret, 0, @"persistent destination should translate");

    vfs_test_seed_linux_file("/etc/rn2-exist-src");
    vfs_test_seed_linux_file("/etc/rn2-exist-dst");

    ret = renameat2(AT_FDCWD, "/etc/rn2-exist-src", AT_FDCWD, "/etc/rn2-exist-dst", TEST_RENAME_NOREPLACE);
    XCTAssertEqual(ret, -EEXIST, @"renameat2 RENAME_NOREPLACE should fail if destination exists");

    struct linux_stat st;
    XCTAssertEqual(stat_impl("/etc/rn2-exist-src", &st), 0, @"renameat2 source should still exist");
    XCTAssertEqual(stat_impl("/etc/rn2-exist-dst", &st), 0, @"renameat2 destination should still exist");

    vfs_test_remove_linux_path("/etc/rn2-exist-src");
    vfs_test_remove_linux_path("/etc/rn2-exist-dst");
}

- (void)testRenameat2CrossRouteFails {
    int ret;
    char src[MAX_PATH];

    ret = vfs_translate_path(@"/etc/rn2-cross-src".UTF8String, src, sizeof(src));
    XCTAssertEqual(ret, 0, @"persistent source should translate");

    vfs_test_seed_linux_file("/etc/rn2-cross-src");

    ret = renameat2(AT_FDCWD, "/etc/rn2-cross-src", AT_FDCWD, "/tmp/rn2-cross-dst", 0);
    XCTAssertEqual(ret, -EXDEV, @"renameat2 across routes should fail with EXDEV");

    vfs_test_remove_linux_path("/etc/rn2-cross-src");
}

- (void)testRenameat2UnsupportedFlagsFail {
    int ret = renameat2(AT_FDCWD, "/etc/src", AT_FDCWD, "/etc/dst", TEST_RENAME_EXCHANGE);
    XCTAssertEqual(ret, -ENOTSUP, @"renameat2 RENAME_EXCHANGE should be rejected");

    ret = renameat2(AT_FDCWD, "/etc/src", AT_FDCWD, "/etc/dst", TEST_RENAME_WHITEOUT);
    XCTAssertEqual(ret, -ENOTSUP, @"renameat2 RENAME_WHITEOUT should be rejected");
}

- (void)testRenameat2UnknownFlagFails {
    int ret = renameat2(AT_FDCWD, "/etc/src", AT_FDCWD, "/etc/dst", 0x80000000);
    XCTAssertEqual(ret, -EINVAL, @"renameat2 with unknown flag should fail with EINVAL");
}

- (void)testRenameat2EmptyPathRequiresAtEmptyPathFlag {
    int ret = renameat2(AT_FDCWD, "/etc/empty-src", AT_FDCWD, "/etc/empty-dst", 0);
    XCTAssertEqual(ret, -ENOENT, @"renameat2 without RENAME_EMPTY_PATH should behave normally");
}

/* ============================================================================
 * FCNTL SEMANTICS TESTS
 * ============================================================================ */

- (void)testFcntlDupFdSucceeds {
    int fd = open("/etc/passwd", O_RDONLY);
    XCTAssertTrue(fd >= 0, @"open should succeed");
    if (fd < 0) return;

    int new_fd = fcntl(fd, TEST_F_DUPFD, 10);
    XCTAssertTrue(new_fd >= 10, @"F_DUPFD should return fd >= 10");

    close(fd);
    if (new_fd >= 0) close(new_fd);
}

- (void)testFcntlGetFdSucceeds {
    int fd = open("/etc/passwd", O_RDONLY);
    XCTAssertTrue(fd >= 0, @"open should succeed");
    if (fd < 0) return;

    int flags = fcntl(fd, TEST_F_GETFD);
    XCTAssertTrue(flags >= 0, @"F_GETFD should succeed");

    close(fd);
}

- (void)testFcntlSetFdCloexecSucceeds {
    int fd = open("/etc/passwd", O_RDONLY);
    XCTAssertTrue(fd >= 0, @"open should succeed");
    if (fd < 0) return;

    int ret = fcntl(fd, TEST_F_SETFD, TEST_FD_CLOEXEC);
    XCTAssertEqual(ret, 0, @"F_SETFD with FD_CLOEXEC should succeed");

    int flags = fcntl(fd, TEST_F_GETFD);
    XCTAssertTrue(flags & TEST_FD_CLOEXEC, @"FD_CLOEXEC should be set");

    close(fd);
}

- (void)testFcntlGetFlSucceeds {
    int fd = open("/etc/passwd", O_RDONLY);
    XCTAssertTrue(fd >= 0, @"open should succeed");
    if (fd < 0) return;

    int flags = fcntl(fd, TEST_F_GETFL);
    XCTAssertTrue(flags >= 0, @"F_GETFL should succeed");
    XCTAssertTrue(flags & O_RDONLY, @"flags should include O_RDONLY");

    close(fd);
}

- (void)testFcntlDupFdCloexecSucceeds {
    int fd = open("/etc/passwd", O_RDONLY);
    XCTAssertTrue(fd >= 0, @"open should succeed");
    if (fd < 0) return;

    int new_fd = fcntl(fd, TEST_F_DUPFD_CLOEXEC, 10);
    XCTAssertTrue(new_fd >= 10, @"F_DUPFD_CLOEXEC should return fd >= 10");

    int flags = fcntl(new_fd, TEST_F_GETFD);
    XCTAssertTrue(flags & TEST_FD_CLOEXEC, @"duped fd should have FD_CLOEXEC");

    close(fd);
    if (new_fd >= 0) close(new_fd);
}

/* ============================================================================
 * SIGNAL-FAMILY SEMANTICS TESTS
 * ============================================================================ */

- (void)testSignalKillSucceeds {
    pid_t pid = getpid();
    int ret = kill(pid, 0);
    XCTAssertEqual(ret, 0, @"kill(self, 0) should succeed");
}

- (void)testSignalSigactionInstallAndRestore {
    int ret = ixland_test_signal_install_sigint_ign();
    XCTAssertEqual(ret, 0, @"sigaction should succeed for SIGINT");

    ret = ixland_test_signal_restore_sigint();
    XCTAssertEqual(ret, 0, @"sigaction restore should succeed");
}

- (void)testSignalSigprocmaskBlockAndUnblock {
    int ret = ixland_test_signal_block_sigint();
    XCTAssertEqual(ret, 0, @"sigprocmask block should succeed");

    sigset_t pending;
    ret = sigpending(&pending);
    XCTAssertEqual(ret, 0, @"sigpending should succeed");

    ret = ixland_test_signal_restore_mask();
    XCTAssertEqual(ret, 0, @"sigprocmask restore should succeed");
}

/* ============================================================================
 * PROC/SELF ABSTRACTION TESTS
 * ============================================================================ */

- (void)testProcSelfStatSucceeds {
    struct linux_stat st;
    errno = 0;
    XCTAssertEqual(stat_impl("/proc/self", &st), 0, @"stat(/proc/self) should succeed");
    XCTAssertTrue(ixland_test_uapi_mode_is_directory(st.st_mode), @"/proc/self should be a directory");
}

- (void)testProcSelfStatFileSucceeds {
    struct linux_stat st;
    errno = 0;
    XCTAssertEqual(stat_impl("/proc/self/stat", &st), 0, @"stat(/proc/self/stat) should succeed");
    XCTAssertTrue(ixland_test_uapi_mode_is_regular(st.st_mode), @"/proc/self/stat should be a regular file");
    XCTAssertEqual(st.st_mode & 0777, 0444, @"/proc/self/stat should have 0444 permissions");
}

- (void)testProcSelfCmdlineSucceeds {
    struct linux_stat st;
    errno = 0;
    XCTAssertEqual(stat_impl("/proc/self/cmdline", &st), 0, @"stat(/proc/self/cmdline) should succeed");
    XCTAssertTrue(ixland_test_uapi_mode_is_regular(st.st_mode), @"/proc/self/cmdline should be a regular file");
    XCTAssertEqual(st.st_mode & 0777, 0444, @"/proc/self/cmdline should have 0444 permissions");
}

- (void)testProcSelfCommSucceeds {
    struct linux_stat st;
    errno = 0;
    XCTAssertEqual(stat_impl("/proc/self/comm", &st), 0, @"stat(/proc/self/comm) should succeed");
    XCTAssertTrue(ixland_test_uapi_mode_is_regular(st.st_mode), @"/proc/self/comm should be a regular file");
    XCTAssertEqual(st.st_mode & 0777, 0444, @"/proc/self/comm should have 0444 permissions");
}

- (void)testProcSelfStatmSucceeds {
    struct linux_stat st;
    errno = 0;
    XCTAssertEqual(stat_impl("/proc/self/statm", &st), 0, @"stat(/proc/self/statm) should succeed");
    XCTAssertTrue(ixland_test_uapi_mode_is_regular(st.st_mode), @"/proc/self/statm should be a regular file");
    XCTAssertEqual(st.st_mode & 0777, 0444, @"/proc/self/statm should have 0444 permissions");
}

- (void)testProcSelfExeIsSymlink {
    struct linux_stat st;
    errno = 0;
    XCTAssertEqual(lstat_impl("/proc/self/exe", &st), 0, @"lstat(/proc/self/exe) should succeed");
    XCTAssertTrue(ixland_test_uapi_mode_is_symlink(st.st_mode), @"/proc/self/exe should be a symlink");
}

- (void)testProcSelfCwdIsSymlink {
    struct linux_stat st;
    errno = 0;
    XCTAssertEqual(lstat_impl("/proc/self/cwd", &st), 0, @"lstat(/proc/self/cwd) should succeed");
    XCTAssertTrue(ixland_test_uapi_mode_is_symlink(st.st_mode), @"/proc/self/cwd should be a symlink");
}

- (void)testProcSelfFdIsDirectory {
    struct linux_stat st;
    errno = 0;
    XCTAssertEqual(stat_impl("/proc/self/fd", &st), 0, @"stat(/proc/self/fd) should succeed");
    XCTAssertTrue(ixland_test_uapi_mode_is_directory(st.st_mode), @"/proc/self/fd should be a directory");
}

- (void)testProcSelfFdinfoIsDirectory {
    struct linux_stat st;
    errno = 0;
    XCTAssertEqual(stat_impl("/proc/self/fdinfo", &st), 0, @"stat(/proc/self/fdinfo) should succeed");
    XCTAssertTrue(ixland_test_uapi_mode_is_directory(st.st_mode), @"/proc/self/fdinfo should be a directory");
}

/* ============================================================================
 * /PROC/SELF/FD/ SYMLINK TESTS
 * ============================================================================ */

- (void)testProcSelfFdSymlinksExist {
    struct linux_stat st;
    
    // Get stdin fd (0) info via /proc/self/fd/0
    errno = 0;
    int ret = lstat_impl("/proc/self/fd/0", &st);
    XCTAssertEqual(ret, 0, @"lstat(/proc/self/fd/0) should succeed");
    XCTAssertTrue(ixland_test_uapi_mode_is_symlink(st.st_mode), @"/proc/self/fd/0 should be a symlink");
    
    // Get stdout fd (1) info via /proc/self/fd/1
    errno = 0;
    ret = lstat_impl("/proc/self/fd/1", &st);
    XCTAssertEqual(ret, 0, @"lstat(/proc/self/fd/1) should succeed");
    XCTAssertTrue(ixland_test_uapi_mode_is_symlink(st.st_mode), @"/proc/self/fd/1 should be a symlink");
    
    // Get stderr fd (2) info via /proc/self/fd/2
    errno = 0;
    ret = lstat_impl("/proc/self/fd/2", &st);
    XCTAssertEqual(ret, 0, @"lstat(/proc/self/fd/2) should succeed");
    XCTAssertTrue(ixland_test_uapi_mode_is_symlink(st.st_mode), @"/proc/self/fd/2 should be a symlink");
}

- (void)testProcSelfFdSymlinksPointToValidPaths {
    char link_target[MAX_PATH];
    ssize_t len;
    
    // Read symlink target for fd 0
    len = readlink("/proc/self/fd/0", link_target, sizeof(link_target) - 1);
    XCTAssertTrue(len > 0, @"readlink(/proc/self/fd/0) should return a path");
    if (len > 0) {
        link_target[len] = '\0';
        XCTAssertTrue(strlen(link_target) > 0, @"fd 0 should point to a valid path");
    }
}

- (void)testProcSelfFdSymlinksReflectActualFdState {
    // Create a temporary file to get a real fd
    int test_fd = open("/tmp/test_fd_symlink", O_CREAT | O_RDWR, 0644);
    XCTAssertTrue(test_fd >= 0, @"open should succeed for test file");
    if (test_fd < 0) return;
    
    // Construct the /proc/self/fd path for this fd
    char fd_path[64];
    snprintf(fd_path, sizeof(fd_path), "/proc/self/fd/%d", test_fd);
    
    // Verify the symlink exists and points to the expected path
    char link_target[MAX_PATH];
    ssize_t len = readlink(fd_path, link_target, sizeof(link_target) - 1);
    XCTAssertTrue(len > 0, @"readlink should succeed for newly created fd");
    
    if (len > 0) {
        link_target[len] = '\0';
        // Should contain "test_fd_symlink" somewhere in the path
        XCTAssertTrue(strstr(link_target, "test_fd_symlink") != NULL,
                      @"fd symlink should point to the test file");
    }
    
    // Clean up
    close(test_fd);
    unlink("/tmp/test_fd_symlink");
}

- (void)testProcSelfFdInvalidFdNumbersFail {
    struct linux_stat st;
    
    // Try to stat a non-existent fd
    errno = 0;
    int ret = stat_impl("/proc/self/fd/999", &st);
    XCTAssertEqual(ret, -ENOENT, @"stat(/proc/self/fd/999) should fail with ENOENT");
    XCTAssertEqual(errno, ENOENT, @"errno should be ENOENT for invalid fd");
    
    // Try to stat a non-numeric fd "name"
    errno = 0;
    ret = stat_impl("/proc/self/fd/abc", &st);
    XCTAssertEqual(ret, -ENOENT, @"stat(/proc/self/fd/abc) should fail with ENOENT");
    XCTAssertEqual(errno, ENOENT, @"errno should be ENOENT for non-numeric fd");
}

- (void)testProcSelfFdinfoFilesExist {
    struct linux_stat st;
    
    // fdinfo/0 should exist and be a regular file
    errno = 0;
    int ret = stat_impl("/proc/self/fdinfo/0", &st);
    XCTAssertEqual(ret, 0, @"stat(/proc/self/fdinfo/0) should succeed");
    XCTAssertTrue(ixland_test_uapi_mode_is_regular(st.st_mode), @"/proc/self/fdinfo/0 should be a regular file");
    XCTAssertEqual(st.st_mode & 0777, 0444, @"/proc/self/fdinfo/0 should have 0444 permissions");
    
    // fdinfo/1 should also exist
    errno = 0;
    ret = stat_impl("/proc/self/fdinfo/1", &st);
    XCTAssertEqual(ret, 0, @"stat(/proc/self/fdinfo/1) should succeed");
    XCTAssertTrue(ixland_test_uapi_mode_is_regular(st.st_mode), @"/proc/self/fdinfo/1 should be a regular file");
}

- (void)testProcSelfFdinfoInvalidFdNumbersFail {
    struct linux_stat st;
    
    // Try to stat a non-existent fdinfo entry
    errno = 0;
    int ret = stat_impl("/proc/self/fdinfo/999", &st);
    XCTAssertEqual(ret, -ENOENT, @"stat(/proc/self/fdinfo/999) should fail with ENOENT");
    XCTAssertEqual(errno, ENOENT, @"errno should be ENOENT for invalid fdinfo");
}

/* ============================================================================
 * HOST PATH VALIDATION AND FAILURE CASES
 * ============================================================================ */

- (void)testVfsTranslatePathRejectsAbsoluteHostPath {
    char host_path[MAX_PATH];
    int ret = vfs_translate_path(@"/private/var/mobile/test".UTF8String, host_path, sizeof(host_path));

    XCTAssertEqual(ret, -EXDEV, @"vfs_translate_path should reject unmapped host absolute path");
}

- (void)testVfsTranslatePathRejectsNonRoutePath {
    char host_path[MAX_PATH];
    int ret = vfs_translate_path(@"/nonexistent/path".UTF8String, host_path, sizeof(host_path));

    XCTAssertEqual(ret, 0, @"vfs_translate_path should fallback to persistent route for unknown paths");
}

- (void)testVfsTranslatePathRejectsInvalidVirtualPath {
    char host_path[MAX_PATH];
    int ret = vfs_translate_path(@"".UTF8String, host_path, sizeof(host_path));

    XCTAssertEqual(ret, -ENOENT, @"vfs_translate_path should reject empty path");
}

- (void)testVfsTranslatePathRejectsNullPath {
    char host_path[MAX_PATH];
    int ret = vfs_translate_path(NULL, host_path, sizeof(host_path));

    XCTAssertEqual(ret, -EFAULT, @"vfs_translate_path should reject NULL path");
}

- (void)testVfsTranslatePathRejectsNullBuffer {
    int ret = vfs_translate_path(@"/etc/passwd".UTF8String, NULL, MAX_PATH);

    XCTAssertEqual(ret, -EFAULT, @"vfs_translate_path should reject NULL buffer");
}

- (void)testVfsTranslatePathRejectsZeroBufferSize {
    char host_path[MAX_PATH];
    int ret = vfs_translate_path(@"/etc/passwd".UTF8String, host_path, 0);

    XCTAssertEqual(ret, -EINVAL, @"vfs_translate_path should reject zero buffer size");
}

/* ============================================================================
 * SYNTHETIC /dev/tty TESTS
 * ============================================================================ */

- (void)testDevTtyOpenFailsWithoutControllingTty {
    struct task_struct *original_task = get_current();
    struct task_struct *isolated_task = alloc_task();
    XCTAssertTrue(isolated_task != NULL, @"task allocation should succeed");
    if (!isolated_task) return;

    isolated_task->fs = alloc_fs_struct();
    XCTAssertTrue(isolated_task->fs != NULL, @"fs_struct allocation should succeed");
    if (!isolated_task->fs) {
        free_task(isolated_task);
        return;
    }

    isolated_task->signal = alloc_signal_struct();
    XCTAssertTrue(isolated_task->signal != NULL, @"signal_struct allocation should succeed");
    if (!isolated_task->signal) {
        free_task(isolated_task);
        return;
    }

    fs_init_root(isolated_task->fs, @"/".UTF8String);
    fs_init_pwd(isolated_task->fs, @"/".UTF8String);
    set_current(isolated_task);

    errno = 0;
    int fd = open("/dev/tty", O_RDWR);
    XCTAssertEqual(fd, -1, @"open(/dev/tty) should fail without controlling tty");
    XCTAssertTrue(errno == ENXIO || errno == EIO, @"open(/dev/tty) should set ENXIO or EIO");

    set_current(original_task);
    free_task(isolated_task);
}

- (void)testDevTtyStatFails {
    struct linux_stat st;
    errno = 0;
    int ret = stat_impl("/dev/tty", &st);
    XCTAssertEqual(ret, -ENOENT, @"stat(/dev/tty) should fail for unsupported synthetic node");
    XCTAssertEqual(errno, ENOENT, @"stat(/dev/tty) should set ENOENT");
}

- (void)testDevTtyAccessFails {
    errno = 0;
    int ret = access("/dev/tty", F_OK);
    XCTAssertEqual(ret, -ENOENT, @"access(/dev/tty) should fail for unsupported synthetic node");
    XCTAssertEqual(errno, ENOENT, @"access(/dev/tty) should set ENOENT");
}

/* ============================================================================
 * PERSISTENT FILESYSTEM TESTS
 * ============================================================================ */

- (void)testPersistentFileCreationAndReadWrite {
    const char *test_path = "/etc/test_persistent_file";
    char host_path[MAX_PATH];
    int fd;
    
    // Translate path
    XCTAssertEqual(vfs_translate_path(test_path, host_path, sizeof(host_path)), 0,
                   @"path should translate");
    
    // Create file
    fd = host_open_impl(host_path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    XCTAssertTrue(fd >= 0, @"file creation should succeed");
    if (fd < 0) return;
    
    // Write data
    const char *write_data = "test data";
    ssize_t written = host_write_impl(fd, write_data, strlen(write_data));
    XCTAssertEqual(written, (ssize_t)strlen(write_data), @"write should succeed");
    
    // Seek to beginning
    off_t pos = host_lseek_impl(fd, 0, SEEK_SET);
    XCTAssertEqual(pos, 0, @"seek should succeed");
    
    // Read back
    char read_buf[64];
    ssize_t nread = host_read_impl(fd, read_buf, sizeof(read_buf));
    XCTAssertEqual(nread, (ssize_t)strlen(write_data), @"read should return written amount");
    XCTAssertEqual(memcmp(read_buf, write_data, strlen(write_data)), 0,
                    @"read data should match written data");
    
    host_close_impl(fd);
    
    // Clean up
    host_unlink_impl(host_path);
}

- (void)testPersistentDirectoryCreation {
    const char *test_dir = "/etc/test_persistent_dir";
    char host_path[MAX_PATH];
    
    // Translate path
    XCTAssertEqual(vfs_translate_path(test_dir, host_path, sizeof(host_path)), 0,
                   @"directory path should translate");
    
    // Create directory
    int ret = host_mkdir_impl(host_path, 0755);
    XCTAssertTrue(ret == 0 || errno == EEXIST, @"directory creation should succeed");
    
    // Verify it exists via stat
    struct linux_stat st;
    XCTAssertEqual(stat_impl(test_dir, &st), 0, @"stat should succeed for created directory");
    XCTAssertTrue(ixland_test_uapi_mode_is_directory(st.st_mode), @"created path should be a directory");
    
    // Clean up
    host_rmdir_impl(host_path);
}

- (void)testPersistentSymbolicLink {
    const char *link_path = "/etc/test_symlink";
    const char *target = "/etc/passwd";
    char host_link[MAX_PATH];
    
    // Translate link path
    XCTAssertEqual(vfs_translate_path(link_path, host_link, sizeof(host_link)), 0,
                   @"link path should translate");
    
    // Remove any existing link
    host_unlink_impl(host_link);
    
    // Create symlink
    int ret = host_symlink_impl(target, host_link);
    XCTAssertEqual(ret, 0, @"symlink creation should succeed");
    
    // Verify via lstat
    struct linux_stat st;
    XCTAssertEqual(lstat_impl(link_path, &st), 0, @"lstat should succeed for symlink");
    XCTAssertTrue(ixland_test_uapi_mode_is_symlink(st.st_mode), @"created path should be a symlink");
    
    // Clean up
    host_unlink_impl(host_link);
}

/* ============================================================================
 * BUFFER SIZE VALIDATION TESTS
 * ============================================================================ */

- (void)testVfsTranslatePathRejectsTooSmallBuffer {
    char small_buf[1];
    int ret = vfs_translate_path(@"/etc/passwd".UTF8String, small_buf, sizeof(small_buf));
    
    XCTAssertEqual(ret, -ENAMETOOLONG, @"vfs_translate_path should reject too-small buffer");
}

- (void)testPathResolveRejectsNullPath {
    char resolved[MAX_PATH];
    int ret = path_resolve(NULL, resolved, sizeof(resolved));
    
    XCTAssertEqual(ret, -EFAULT, @"path_resolve should reject NULL path");
}

- (void)testPathResolveRejectsNullBuffer {
    int ret = path_resolve(@"/etc/passwd".UTF8String, NULL, MAX_PATH);
    
    XCTAssertEqual(ret, -EFAULT, @"path_resolve should reject NULL buffer");
}

- (void)testPathResolveRejectsZeroBufferSize {
    char resolved[MAX_PATH];
    int ret = path_resolve(@"/etc/passwd".UTF8String, resolved, 0);
    
    XCTAssertEqual(ret, -EINVAL, @"path_resolve should reject zero buffer size");
}

@end
