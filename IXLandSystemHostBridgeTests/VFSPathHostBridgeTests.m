/*
 * Host bridge tests for VFS path operations that require host helpers
 * and NSFileManager. These tests live in IXLandSystemHostBridgeTests so
 * they may call host_open_impl/host_close_impl and use Foundation APIs.
 */

#import <XCTest/XCTest.h>
#import <Foundation/Foundation.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "HostTestSupport.h"
#include "fs/vfs.h"
/* host_open_impl/host_close_impl are declared in internal host backing IO headers */
#include "internal/ios/fs/backing_io.h"

@interface VFSPathHostBridgeTests : XCTestCase
@end

@implementation VFSPathHostBridgeTests

- (void)setUp {
    [super setUp];
}

- (void)tearDown {
    [super tearDown];
}

- (void)testVirtualEtcPasswdExists_HostBacked {
    char host_path[MAX_PATH];
    int ret = vfs_translate_path("/etc/passwd", host_path, sizeof(host_path));
    XCTAssertEqual(ret, 0, @"vfs_translate_path for /etc/passwd should succeed");

    /* Verify file is accessible via host helpers */
    int fd = host_open_impl(host_path, O_RDONLY, 0);
    XCTAssertTrue(fd >= 0, @"host_open_impl /etc/passwd should succeed");
    if (fd >= 0) host_close_impl(fd);
}

// More host-backed VFS tests can be added here.

@end
