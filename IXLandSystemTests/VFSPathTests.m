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

#include <errno.h>
#include <string.h>

#include "fs/vfs.h"
#include "fs/path.h"
#include "kernel/task.h"

extern char *getcwd_impl(char *buf, size_t size);

@interface VFSPathTests : XCTestCase
@end

@implementation VFSPathTests

- (void)testVirtualRootTranslatesToBackingRoot {
    char host_path[MAX_PATH];
    int ret = vfs_translate_path(@"/".UTF8String, host_path, sizeof(host_path));

    XCTAssertEqual(ret, 0, @"vfs_translate_path should accept virtual root");
    XCTAssertEqual(strcmp(host_path, vfs_host_backing_root()), 0,
                   @"virtual root should map to deterministic backing root");
}

- (void)testAbsoluteVirtualPathMapsUnderBackingRoot {
    char host_path[MAX_PATH];
    int ret = vfs_translate_path(@"/tmp/demo".UTF8String, host_path, sizeof(host_path));
    NSString *actualPath = [NSString stringWithUTF8String:host_path];
    NSString *expectedPath = [NSString stringWithFormat:@"%s/tmp/demo", vfs_host_backing_root()];

    XCTAssertEqual(ret, 0, @"absolute virtual path should translate");
    XCTAssertEqualObjects(actualPath, expectedPath, @"translated path should live under backing root");
    XCTAssertNotEqual(strcmp(host_path, "/tmp/demo"), 0,
                      @"translation must not be identity mapping");
}

- (void)testRelativeVirtualPathMapsUnderBackingRoot {
    char host_path[MAX_PATH];
    int ret = vfs_translate_path(@"etc/passwd".UTF8String, host_path, sizeof(host_path));
    NSString *actualPath = [NSString stringWithUTF8String:host_path];
    NSString *expectedPath = [NSString stringWithFormat:@"%s/etc/passwd", vfs_host_backing_root()];

    XCTAssertEqual(ret, 0, @"relative virtual path should translate");
    XCTAssertEqualObjects(actualPath, expectedPath, @"relative path should resolve under virtual root baseline");
}

- (void)testUnmappableHostPathIsRejected {
    char virtual_path[MAX_PATH];
    int ret = vfs_reverse_translate(@"/private/var/mobile".UTF8String, virtual_path, sizeof(virtual_path));

    XCTAssertEqual(ret, -EXDEV, @"unmapped host path should be rejected");
}

- (void)testBackingRootReverseTranslatesToVirtualRoot {
    char virtual_path[MAX_PATH];
    int ret = vfs_reverse_translate(vfs_host_backing_root(), virtual_path, sizeof(virtual_path));

    XCTAssertEqual(ret, 0, @"backing root should reverse translate");
    XCTAssertEqual(strcmp(virtual_path, vfs_virtual_root()), 0,
                   @"backing root should map back to virtual root");
}

- (void)testMappedHostPathReverseTranslatesToVirtualPath {
    char virtual_path[MAX_PATH];
    NSString *hostPath = [NSString stringWithFormat:@"%s/var/log", vfs_host_backing_root()];
    int ret = vfs_reverse_translate(hostPath.UTF8String, virtual_path, sizeof(virtual_path));

    XCTAssertEqual(ret, 0, @"mapped host path should reverse translate");
    XCTAssertEqualObjects([NSString stringWithUTF8String:virtual_path], @"/var/log",
                          @"reverse translation should return virtual path");
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

@end
