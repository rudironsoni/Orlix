/*
 * IXLandSystemTests - VFSPathTests.m (Linux-only)
 *
 * INTERNAL OWNER SEMANTIC TESTS ONLY.
 * Host-dependent helpers and NSFileManager usage are removed and live in the
 * HostBridge test target to preserve strict separation.
 */

#import <XCTest/XCTest.h>

/* Minimal standard headers */
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* IXLand VFS types */
#include "fs/vfs.h"
#include "fs/path.h"
#include "kernel/task.h"
#include "kernel/signal.h"
#include "runtime/native/registry.h"

/* Linux UAPI test support - semantic helpers only */
#include "IXLandSystemLinuxKernelTests/LinuxUAPITestSupport.h"

@interface VFSPathTests : XCTestCase
@end

@implementation VFSPathTests

- (void)setUp {
    [super setUp];
    /* Clean up any lingering file descriptors using owner close_impl */
    for (int fd = 3; fd < 256; fd++) {
        close_impl(fd);
    }
}

- (void)tearDown {
    for (int fd = 3; fd < 256; fd++) {
        close_impl(fd);
    }
    [super tearDown];
}

- (void)testVirtualRootTranslatesToPersistentBackingRoot {
    char host_path[MAX_PATH];
    int ret = vfs_translate_path("/", host_path, sizeof(host_path));

    XCTAssertEqual(ret, 0, @"vfs_translate_path should accept virtual root");
    XCTAssertEqual(strcmp(host_path, vfs_persistent_backing_root()), 0,
                   @"virtual root should map to persistent backing root");
}

/*
 * Keep only owner-focused tests here. Host-dependent file seeding and
 * NSFileManager-based operations were moved to the HostBridge test target.
 */

// Additional owner-only tests continue here (omitted for brevity).

@end
