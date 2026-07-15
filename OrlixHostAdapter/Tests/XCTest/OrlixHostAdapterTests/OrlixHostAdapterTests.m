#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#include <string.h>
#include <unistd.h>
#include "OrlixHostAdapter/boot/progress.h"
#include "OrlixHostAdapter/boot/resources.h"
#include "OrlixHostAdapter/memory/kernel_mapping.h"
#include "OrlixHostAdapter/terminal/console.h"

#include <limits.h>
#include <mach/mach.h>
#include <mach/vm_page_size.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ORLIX_HOST_ADAPTER_TEST_LINUX_PAGE_SIZE
#error ORLIX_HOST_ADAPTER_TEST_LINUX_PAGE_SIZE must come from target settings.
#endif

unsigned long OrlixHostEnterHostTls(void)
{
    return 0;
}

void OrlixHostLeaveHostTls(unsigned long active_tls)
{
    (void)active_tls;
}

static unsigned long OrlixHostAdapterTestAlignUp(unsigned long value,
                                                 unsigned long alignment)
{
    return (value + alignment - 1UL) & ~(alignment - 1UL);
}

static int OrlixHostAdapterTestCreateDiscoveredGap(unsigned long length,
                                                   unsigned long alignment,
                                                   unsigned long *minimumAddress,
                                                   unsigned long *maximumAddress)
{
    vm_address_t allocation = 0;
    vm_size_t allocationLength = (vm_size_t)(length + alignment);
    unsigned long aligned;

    if (!minimumAddress || !maximumAddress || length == 0 || alignment == 0) {
        return -1;
    }

    if (vm_allocate(mach_task_self(),
                    &allocation,
                    allocationLength,
                    VM_FLAGS_ANYWHERE) != KERN_SUCCESS) {
        return -1;
    }

    aligned = OrlixHostAdapterTestAlignUp((unsigned long)allocation,
                                          alignment);
    if (aligned > (unsigned long)allocation + allocationLength - length) {
        vm_deallocate(mach_task_self(), allocation, allocationLength);
        return -1;
    }

    vm_deallocate(mach_task_self(), allocation, allocationLength);
    *minimumAddress = aligned;
    *maximumAddress = aligned + length;
    return 0;
}

@interface OrlixHostAdapterTests : XCTestCase
@end

@implementation OrlixHostAdapterTests

- (void)testConfiguredLinuxPageSizeCanRepresentHostPageGranularity
{
    const unsigned long linuxPageSize = ORLIX_HOST_ADAPTER_TEST_LINUX_PAGE_SIZE;
    const unsigned long hostPageSize = orlix_host_memory_page_size();

    XCTAssertGreaterThan(hostPageSize, 0UL);
    XCTAssertEqual(hostPageSize & (hostPageSize - 1UL), 0UL);
    XCTAssertGreaterThanOrEqual(linuxPageSize, hostPageSize);
    XCTAssertEqual(linuxPageSize % hostPageSize, 0UL);
}

- (void)testUserMappingAdaptsLinuxPageInsideHostPage
{
    const unsigned long linuxPageSize = ORLIX_HOST_ADAPTER_TEST_LINUX_PAGE_SIZE;
    const unsigned long mappingLength = linuxPageSize * 2UL;
    vm_address_t reserved = 0;
    kern_return_t status = vm_allocate(mach_task_self(),
                                       &reserved,
                                       (vm_size_t)mappingLength,
                                       VM_FLAGS_ANYWHERE);
    XCTAssertEqual(status, KERN_SUCCESS);
    XCTAssertNotEqual(reserved, (vm_address_t)0);
    if (status != KERN_SUCCESS || !reserved) {
        return;
    }

    status = vm_deallocate(mach_task_self(), reserved, (vm_size_t)mappingLength);
    XCTAssertEqual(status, KERN_SUCCESS);

    unsigned char *source = malloc((size_t)linuxPageSize);
    XCTAssertTrue(source != NULL);
    if (!source) {
        return;
    }
    memset(source, 0x5a, (size_t)linuxPageSize);

    unsigned long target = (unsigned long)reserved;
    if (vm_page_size > linuxPageSize) {
        target += linuxPageSize;
    }

    int ret = orlix_host_user_map_page(target,
                                       source,
                                       linuxPageSize,
                                       1,
                                       0);
    XCTAssertEqual(ret, 0);
    unsigned char mappedByte = ((volatile unsigned char *)target)[0];
    XCTAssertEqual(mappedByte, 0x5a);

    ((volatile unsigned char *)target)[0] = 0xa5;
    orlix_host_user_sync_writable_mappings();
    XCTAssertEqual(source[0], 0xa5);

    orlix_host_user_unmap_pages(target, linuxPageSize);
    free(source);
}

- (void)testUserWindowRefreshCopiesMultipleLinuxPagesInsideHostPage
{
    if (vm_page_size < 8192) {
        return;
    }

    const unsigned long linuxPageSize = ORLIX_HOST_ADAPTER_TEST_LINUX_PAGE_SIZE;
    const unsigned long mappingLength = linuxPageSize * 2UL;
    vm_address_t reserved = 0;
    kern_return_t status = vm_allocate(mach_task_self(),
                                       &reserved,
                                       (vm_size_t)mappingLength,
                                       VM_FLAGS_ANYWHERE);
    XCTAssertEqual(status, KERN_SUCCESS);
    XCTAssertNotEqual(reserved, (vm_address_t)0);
    if (status != KERN_SUCCESS || !reserved) {
        return;
    }

    status = vm_deallocate(mach_task_self(), reserved, (vm_size_t)mappingLength);
    XCTAssertEqual(status, KERN_SUCCESS);

    unsigned char *first = malloc((size_t)linuxPageSize);
    unsigned char *second = malloc((size_t)linuxPageSize);
    XCTAssertTrue(first != NULL);
    XCTAssertTrue(second != NULL);
    if (!first || !second) {
        free(first);
        free(second);
        return;
    }
    memset(first, 0x11, (size_t)linuxPageSize);
    memset(second, 0x22, (size_t)linuxPageSize);

    struct orlix_host_user_page_segment segments[] = {
        {
            .target_address = (unsigned long)reserved,
            .source_page = first,
            .length = linuxPageSize,
            .writable = 0,
            .executable = 0,
        },
        {
            .target_address = (unsigned long)reserved + linuxPageSize,
            .source_page = second,
            .length = linuxPageSize,
            .writable = 0,
            .executable = 0,
        },
    };

    int ret = orlix_host_user_refresh_window((unsigned long)reserved,
                                                 mappingLength,
                                             segments,
                                             sizeof(segments) / sizeof(segments[0]));
    XCTAssertEqual(ret, 0);

    unsigned char firstByte = ((volatile unsigned char *)reserved)[0];
    unsigned char secondByte = ((volatile unsigned char *)reserved)[linuxPageSize];
    XCTAssertEqual(firstByte, 0x11);
    XCTAssertEqual(secondByte, 0x22);

    orlix_host_user_unmap_pages((unsigned long)reserved, mappingLength);
    free(first);
    free(second);
}

- (void)testUserWindowRefreshCopiesExecutableLinuxPage
{
    const unsigned long linuxPageSize = ORLIX_HOST_ADAPTER_TEST_LINUX_PAGE_SIZE;
    vm_address_t reserved = 0;
    kern_return_t status = vm_allocate(mach_task_self(),
                                       &reserved,
                                       (vm_size_t)linuxPageSize,
                                       VM_FLAGS_ANYWHERE);
    XCTAssertEqual(status, KERN_SUCCESS);
    XCTAssertNotEqual(reserved, (vm_address_t)0);
    if (status != KERN_SUCCESS || !reserved) {
        return;
    }

    status = vm_deallocate(mach_task_self(), reserved, (vm_size_t)linuxPageSize);
    XCTAssertEqual(status, KERN_SUCCESS);

    unsigned char *source = malloc((size_t)linuxPageSize);
    XCTAssertTrue(source != NULL);
    if (!source) {
        return;
    }

    memset(source, 0xd5, (size_t)linuxPageSize);

    struct orlix_host_user_page_segment segments[] = {
        {
            .target_address = (unsigned long)reserved,
            .source_page = source,
            .length = linuxPageSize,
            .writable = 0,
            .executable = 1,
        },
    };

    int ret = orlix_host_user_refresh_window((unsigned long)reserved,
                                             linuxPageSize,
                                             segments,
                                             sizeof(segments) / sizeof(segments[0]));
    XCTAssertEqual(ret, 0);

    if (ret == 0) {
        unsigned char mappedByte = ((volatile unsigned char *)reserved)[0];
        XCTAssertEqual(mappedByte, 0xd5);
        orlix_host_user_unmap_pages((unsigned long)reserved, linuxPageSize);
    }

    free(source);
}

- (void)testUserWindowRefreshMapsWritableLinuxPageAfterHoles
{
    if (vm_page_size < 8192) {
        return;
    }

    const unsigned long linuxPageSize = ORLIX_HOST_ADAPTER_TEST_LINUX_PAGE_SIZE;
    const unsigned long mappingLength = linuxPageSize * 2UL;
    vm_address_t reserved = 0;
    kern_return_t status = vm_allocate(mach_task_self(),
                                       &reserved,
                                       (vm_size_t)mappingLength,
                                       VM_FLAGS_ANYWHERE);
    XCTAssertEqual(status, KERN_SUCCESS);
    XCTAssertNotEqual(reserved, (vm_address_t)0);
    if (status != KERN_SUCCESS || !reserved) {
        return;
    }

    status = vm_deallocate(mach_task_self(), reserved, (vm_size_t)mappingLength);
    XCTAssertEqual(status, KERN_SUCCESS);

    unsigned char *source = malloc((size_t)linuxPageSize);
    XCTAssertTrue(source != NULL);
    if (!source) {
        return;
    }
    memset(source, 0x33, (size_t)linuxPageSize);

    unsigned long target = (unsigned long)reserved + mappingLength - linuxPageSize;
    struct orlix_host_user_page_segment segment = {
        .target_address = target,
        .source_page = source,
        .length = linuxPageSize,
        .writable = 1,
        .executable = 0,
    };

    int ret = orlix_host_user_refresh_window((unsigned long)reserved,
                                             mappingLength,
                                             &segment,
                                             1);
    XCTAssertEqual(ret, 0);

    ((volatile unsigned char *)target)[0] = 0x44;
    ((volatile unsigned char *)target)[linuxPageSize - 1] = 0x55;
    orlix_host_user_sync_writable_mappings();
    XCTAssertEqual(source[0], 0x44);
    XCTAssertEqual(source[linuxPageSize - 1], 0x55);

    orlix_host_user_unmap_pages((unsigned long)reserved, mappingLength);
    free(source);
}

- (void)testKernelMappingAdaptsMultipleLinuxPagesInsideHostPage
{
    if (vm_page_size < 8192) {
        return;
    }

    const unsigned long linuxPageSize = ORLIX_HOST_ADAPTER_TEST_LINUX_PAGE_SIZE;
    const unsigned long mappingLength = linuxPageSize * 2UL;
    vm_address_t reserved = 0;
    kern_return_t status = vm_allocate(mach_task_self(),
                                       &reserved,
                                       (vm_size_t)mappingLength,
                                       VM_FLAGS_ANYWHERE);
    XCTAssertEqual(status, KERN_SUCCESS);
    XCTAssertNotEqual(reserved, (vm_address_t)0);
    if (status != KERN_SUCCESS || !reserved) {
        return;
    }

    status = vm_deallocate(mach_task_self(), reserved, (vm_size_t)mappingLength);
    XCTAssertEqual(status, KERN_SUCCESS);

    unsigned char *first = malloc((size_t)linuxPageSize);
    unsigned char *second = malloc((size_t)linuxPageSize);
    XCTAssertTrue(first != NULL);
    XCTAssertTrue(second != NULL);
    if (!first || !second) {
        free(first);
        free(second);
        return;
    }
    memset(first, 0x61, (size_t)linuxPageSize);
    memset(second, 0x62, (size_t)linuxPageSize);

    unsigned long target = (unsigned long)reserved;
    XCTAssertEqual(orlix_host_kernel_map_page(target,
                                              first,
                                              linuxPageSize),
                   0);
    XCTAssertEqual(orlix_host_kernel_map_page(target + linuxPageSize,
                                              second,
                                              linuxPageSize),
                   0);

    unsigned char firstByte = ((volatile unsigned char *)target)[0];
    unsigned char secondByte = ((volatile unsigned char *)target)[linuxPageSize];
    XCTAssertEqual(firstByte, 0x61);
    XCTAssertEqual(secondByte, 0x62);

    ((volatile unsigned char *)target)[0] = 0x71;
    ((volatile unsigned char *)target)[linuxPageSize] = 0x72;
    orlix_host_kernel_unmap_pages(target, mappingLength);
    XCTAssertEqual(first[0], 0x71);
    XCTAssertEqual(second[0], 0x72);

    free(first);
    free(second);
}

- (void)testAppPrivateRootImageFilesSelectReadableBaseAndWritableStateBlocks
{
    NSURL *root = [NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES];
    root = [root URLByAppendingPathComponent:NSUUID.UUID.UUIDString
                                 isDirectory:YES];
    NSFileManager *fileManager = NSFileManager.defaultManager;
    XCTAssertTrue([fileManager createDirectoryAtURL:root
                        withIntermediateDirectories:YES
                                         attributes:nil
                                              error:nil]);

    NSURL *baseURL = [root URLByAppendingPathComponent:@"base.ext4"];
    NSURL *stateURL = [root URLByAppendingPathComponent:@"state.ext4"];
    NSMutableData *base = [NSMutableData dataWithLength:512];
    NSMutableData *state = [NSMutableData dataWithLength:512];
    ((unsigned char *)base.mutableBytes)[0] = 0xba;
    ((unsigned char *)state.mutableBytes)[0] = 0x5a;
    XCTAssertTrue([base writeToURL:baseURL atomically:YES]);
    XCTAssertTrue([state writeToURL:stateURL atomically:YES]);

    XCTAssertEqual(orlix_host_resources_clear_root_images(), 0);
    XCTAssertEqual(
        orlix_host_resources_register_root_image_files(
            "orlix.env.test",
            "",
            "",
            "initramfs.cpio.gz",
            baseURL.fileSystemRepresentation,
            stateURL.fileSystemRepresentation,
            0,
            1,
            1024),
        0);
    XCTAssertEqual(OrlixHostSelectBootBlockImages("orlix.env.test"), 0);

    unsigned long long sectors = 0;
    XCTAssertEqual(orlix_host_block_capacity(0, &sectors), 0);
    XCTAssertEqual(sectors, 1ULL);
    XCTAssertEqual(orlix_host_block_capacity(1, &sectors), 0);
    XCTAssertEqual(sectors, 2ULL);

    unsigned char buffer[512] = { 0 };
    XCTAssertEqual(orlix_host_block_read(0, 0, buffer, sizeof(buffer)), 0);
    XCTAssertEqual(buffer[0], 0xba);
    memset(buffer, 0, sizeof(buffer));
    XCTAssertEqual(orlix_host_block_read(1, 0, buffer, sizeof(buffer)), 0);
    XCTAssertEqual(buffer[0], 0x5a);

    unsigned char replacement[512] = { 0 };
    replacement[0] = 0xc3;
    XCTAssertNotEqual(orlix_host_block_write(0, 0, replacement, sizeof(replacement)), 0);
    XCTAssertEqual(orlix_host_block_write(1, 1, replacement, sizeof(replacement)), 0);
    memset(buffer, 0, sizeof(buffer));
    XCTAssertEqual(orlix_host_block_read(1, 1, buffer, sizeof(buffer)), 0);
    XCTAssertEqual(buffer[0], 0xc3);

    [fileManager removeItemAtURL:root error:nil];
    XCTAssertEqual(orlix_host_resources_clear_root_images(), 0);
}

- (void)testPayloadRootImageExpandsPersistedStateBlockToTemplateSize
{
    NSURL *root = [NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES];
    root = [root URLByAppendingPathComponent:NSUUID.UUID.UUIDString
                                 isDirectory:YES];
    NSURL *home = [root URLByAppendingPathComponent:@"home" isDirectory:YES];
    NSURL *payload = [root URLByAppendingPathComponent:@"payload"
                                           isDirectory:YES];
    NSURL *rootfs = [payload URLByAppendingPathComponent:@"rootfs"
                                             isDirectory:YES];
    NSFileManager *fileManager = NSFileManager.defaultManager;
    XCTAssertTrue([fileManager createDirectoryAtURL:home
                        withIntermediateDirectories:YES
                                         attributes:nil
                                              error:nil]);
    XCTAssertTrue([fileManager createDirectoryAtURL:rootfs
                        withIntermediateDirectories:YES
                                         attributes:nil
                                              error:nil]);

    NSURL *baseURL = [rootfs URLByAppendingPathComponent:@"base.ext4"];
    NSURL *stateURL = [rootfs URLByAppendingPathComponent:@"state.ext4"];
    NSMutableData *base = [NSMutableData dataWithLength:512];
    NSMutableData *state = [NSMutableData dataWithLength:2048];
    ((unsigned char *)base.mutableBytes)[0] = 0xba;
    ((unsigned char *)state.mutableBytes)[1080] = 0x53;
    ((unsigned char *)state.mutableBytes)[1081] = 0xef;
    XCTAssertTrue([base writeToURL:baseURL atomically:YES]);
    XCTAssertTrue([state writeToURL:stateURL atomically:YES]);

    const char *oldHome = getenv("HOME");
    NSString *oldHomeString = oldHome ? [NSString stringWithUTF8String:oldHome] : nil;
    setenv("HOME", home.fileSystemRepresentation, 1);

    @try {
        XCTAssertEqual(orlix_host_resources_clear_root_images(), 0);
        XCTAssertEqual(
            orlix_host_resources_set_payload_root_path(
                payload.fileSystemRepresentation),
            0);
        XCTAssertEqual(
            orlix_host_resources_register_root_image(
                "orlix.env.template",
                "",
                "",
                "rootfs/initramfs.cpio.gz",
                "rootfs/base.ext4",
                "rootfs/state.ext4",
                0,
                1,
                1024),
            0);

        XCTAssertEqual(OrlixHostSelectBootBlockImages("orlix.env.template"), 0);
        unsigned long long sectors = 0;
        XCTAssertEqual(orlix_host_block_capacity(1, &sectors), 0);
        XCTAssertEqual(sectors, 4ULL);

        NSURL *persistedState = [home URLByAppendingPathComponent:
            @"Library/Application Support/Orlix/orlix-env-template-state.img"];
        XCTAssertEqual(truncate(persistedState.fileSystemRepresentation, 1536), 0);
        XCTAssertEqual(OrlixHostSelectBootBlockImages("orlix.env.template"), 0);
        XCTAssertEqual(orlix_host_block_capacity(1, &sectors), 0);
        XCTAssertEqual(sectors, 4ULL);
    } @finally {
        if (oldHomeString) {
            setenv("HOME", oldHomeString.fileSystemRepresentation, 1);
        } else {
            unsetenv("HOME");
        }
        [fileManager removeItemAtURL:root error:nil];
        XCTAssertEqual(orlix_host_resources_clear_root_images(), 0);
    }
}

- (void)testAppPrivateRootImageFilesRejectRelativeAndParentPaths
{
    XCTAssertEqual(orlix_host_resources_clear_root_images(), 0);
    XCTAssertNotEqual(
        orlix_host_resources_register_root_image_files(
            "orlix.env.bad",
            "",
            "",
            "initramfs.cpio.gz",
            "base.ext4",
            "/tmp/../state.ext4",
            0,
            1,
            1024),
        0);
}

- (void)testHostDirectoryResourcesRegisterOpaqueIdentifiers {
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSURL *root = [NSURL fileURLWithPath:[NSTemporaryDirectory()
        stringByAppendingPathComponent:NSUUID.UUID.UUIDString]
                            isDirectory:YES];
    NSURL *hostDirectory = [root URLByAppendingPathComponent:@"Shared"
                                                 isDirectory:YES];
    XCTAssertTrue([fileManager createDirectoryAtURL:hostDirectory
                        withIntermediateDirectories:YES
                                         attributes:nil
                                              error:nil]);

    XCTAssertEqual(orlix_host_resources_clear_host_directories(), 0);
    XCTAssertEqual(orlix_host_resources_register_host_directory(
                       "documents", hostDirectory.fileSystemRepresentation, 1),
                   0);

    char path[PATH_MAX] = { 0 };
    unsigned int readOnly = 0;
    XCTAssertEqual(OrlixHostCopyHostDirectoryPath("documents",
                                                 path,
                                                 sizeof(path),
                                                 &readOnly),
                   0);
    XCTAssertEqual(strcmp(path, hostDirectory.fileSystemRepresentation), 0);
    XCTAssertEqual(readOnly, 1U);

    [fileManager removeItemAtURL:root error:nil];
    XCTAssertEqual(orlix_host_resources_clear_host_directories(), 0);
}

- (void)testHostDirectoryResourcesRegisterLinuxXattrMetadata {
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSURL *root = [[NSURL fileURLWithPath:NSTemporaryDirectory()]
        URLByAppendingPathComponent:[[NSUUID UUID] UUIDString]];
    const uint8_t capability[] = { 0x01, 0x02, 0x03, 0x04 };
    const char comment[] = "hello";
    char list[64];
    uint8_t value[8];

    XCTAssertTrue([fileManager createDirectoryAtURL:root
                        withIntermediateDirectories:YES
                                         attributes:nil
                                              error:nil]);
    XCTAssertEqual(orlix_host_resources_clear_host_directories(), 0);
    XCTAssertEqual(orlix_host_resources_register_host_directory(
                       "oci-root",
                       [[root path] UTF8String],
                       1),
                   0);

    XCTAssertEqual(orlix_host_resources_register_host_directory_xattr(
                       "oci-root",
                       "bin/tool",
                       "security.capability",
                       capability,
                       sizeof(capability)),
                   0);
    XCTAssertEqual(orlix_host_resources_register_host_directory_xattr(
                       "oci-root",
                       "bin/tool",
                       "user.comment",
                       comment,
                       (uint32_t)(sizeof(comment) - 1)),
                   0);
    XCTAssertNotEqual(orlix_host_resources_register_host_directory_xattr(
                          "oci-root",
                          "bin/tool",
                          "com.apple.quarantine",
                          comment,
                          (uint32_t)(sizeof(comment) - 1)),
                      0);
    XCTAssertNotEqual(orlix_host_resources_register_host_directory_xattr(
                          "missing",
                          "bin/tool",
                          "user.comment",
                          comment,
                          (uint32_t)(sizeof(comment) - 1)),
                      0);
    XCTAssertNotEqual(orlix_host_resources_register_host_directory_xattr(
                          "oci-root",
                          "/bin/tool",
                          "user.comment",
                          comment,
                          (uint32_t)(sizeof(comment) - 1)),
                      0);

    long listLength = orlix_host_directory_list_xattr(0, "bin/tool", NULL, 0);
    XCTAssertEqual(listLength,
                   (long)(strlen("security.capability") + 1 +
                          strlen("user.comment") + 1));
    XCTAssertEqual(orlix_host_directory_list_xattr(0, "bin/tool", list, 4), -2);
    XCTAssertEqual(orlix_host_directory_list_xattr(0,
                                                   "bin/tool",
                                                   list,
                                                   sizeof(list)),
                   listLength);
    XCTAssertNotEqual(memmem(list,
                             (size_t)listLength,
                             "security.capability",
                             strlen("security.capability")),
                      NULL);
    XCTAssertNotEqual(memmem(list,
                             (size_t)listLength,
                             "user.comment",
                             strlen("user.comment")),
                      NULL);

    XCTAssertEqual(orlix_host_directory_read_xattr(
                       0, "bin/tool", "security.capability", NULL, 0),
                   (long)sizeof(capability));
    XCTAssertEqual(orlix_host_directory_read_xattr(
                       0, "bin/tool", "security.capability", value, 1),
                   -2);
    XCTAssertEqual(orlix_host_directory_read_xattr(
                       0,
                       "bin/tool",
                       "security.capability",
                       value,
                       sizeof(value)),
                   (long)sizeof(capability));
    XCTAssertEqual(memcmp(value, capability, sizeof(capability)), 0);
    XCTAssertEqual(orlix_host_directory_read_xattr(
                       0, "bin/tool", "user.missing", value, sizeof(value)),
                   -1);

    XCTAssertEqual(orlix_host_resources_clear_host_directories(), 0);
	XCTAssertEqual(orlix_host_directory_list_xattr(0, "bin/tool", NULL, 0), -1);
	[fileManager removeItemAtURL:root error:nil];
}

- (void)testHostDirectoryWritableRegistrationCreatesAndWritesFiles {
	NSFileManager *fileManager = [NSFileManager defaultManager];
	NSURL *root = [[NSURL fileURLWithPath:NSTemporaryDirectory()]
		URLByAppendingPathComponent:[[NSUUID UUID] UUIDString]];
	const char payload[] = "host directory write";
	uint8_t buffer[64] = {0};
	unsigned int readOnly = 1;

	XCTAssertTrue([fileManager createDirectoryAtURL:root
					   withIntermediateDirectories:YES
									attributes:nil
										 error:nil]);
	XCTAssertEqual(orlix_host_resources_clear_host_directories(), 0);
	XCTAssertEqual(orlix_host_resources_register_host_directory(
			   "oci-root", root.path.UTF8String, 0),
		       0);
	XCTAssertEqual(orlix_host_directory_is_read_only(0, &readOnly), 0);
	XCTAssertEqual(readOnly, 0U);
	XCTAssertEqual(orlix_host_directory_create_file_at_path(
			   0, "created.txt", 0644),
		       0);
	XCTAssertEqual(orlix_host_directory_write_file_at_path(
			   0,
			   "created.txt",
			   0,
			   payload,
			   (uint32_t)(sizeof(payload) - 1)),
		       (long)(sizeof(payload) - 1));
	XCTAssertEqual(orlix_host_directory_read_file_at_path(
			   0,
			   "created.txt",
			   0,
			   buffer,
			   sizeof(buffer)),
		       (long)(sizeof(payload) - 1));
	XCTAssertEqual(memcmp(buffer, payload, sizeof(payload) - 1), 0);

	XCTAssertEqual(orlix_host_resources_clear_host_directories(), 0);
	XCTAssertEqual(orlix_host_resources_register_host_directory(
			   "oci-root", root.path.UTF8String, 1),
		       0);
	XCTAssertEqual(orlix_host_directory_create_file_at_path(
			   0, "readonly.txt", 0644),
		       -2);
	XCTAssertEqual(orlix_host_directory_write_file_at_path(
			   0,
			   "created.txt",
			   0,
			   payload,
			   (uint32_t)(sizeof(payload) - 1)),
		       -2);

	[fileManager removeItemAtURL:root error:nil];
	XCTAssertEqual(orlix_host_resources_clear_host_directories(), 0);
}

- (void)testHostDirectoryResourcesRejectUnsafePathsAndIdentifiers {
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSURL *root = [NSURL fileURLWithPath:[NSTemporaryDirectory()
        stringByAppendingPathComponent:NSUUID.UUID.UUIDString]
                            isDirectory:YES];
    NSURL *hostDirectory = [root URLByAppendingPathComponent:@"Shared"
                                                 isDirectory:YES];
    NSURL *hostFile = [root URLByAppendingPathComponent:@"file.txt"
                                           isDirectory:NO];
    XCTAssertTrue([fileManager createDirectoryAtURL:hostDirectory
                        withIntermediateDirectories:YES
                                         attributes:nil
                                              error:nil]);
    XCTAssertTrue([@"x" writeToURL:hostFile
                        atomically:YES
                          encoding:NSUTF8StringEncoding
                             error:nil]);

    XCTAssertEqual(orlix_host_resources_clear_host_directories(), 0);
    XCTAssertNotEqual(orlix_host_resources_register_host_directory(
                          "../documents",
                          hostDirectory.fileSystemRepresentation,
                          0),
                      0);
    XCTAssertNotEqual(orlix_host_resources_register_host_directory(
                          "mnt/documents",
                          hostDirectory.fileSystemRepresentation,
                          0),
                      0);
    XCTAssertNotEqual(orlix_host_resources_register_host_directory(
                          "documents",
                          "relative/path",
                          0),
                      0);
    XCTAssertNotEqual(orlix_host_resources_register_host_directory(
                          "documents",
                          "/tmp/../Documents",
                          0),
                      0);
    XCTAssertNotEqual(orlix_host_resources_register_host_directory(
                          "documents",
                          hostFile.fileSystemRepresentation,
                          0),
                      0);

    char path[PATH_MAX] = { 0 };
    XCTAssertNotEqual(OrlixHostCopyHostDirectoryPath("documents",
                                                    path,
                                                    sizeof(path),
                                                    NULL),
                      0);

    [fileManager removeItemAtURL:root error:nil];
    XCTAssertEqual(orlix_host_resources_clear_host_directories(), 0);
}

- (void)testHostDirectorySupportsNestedRelativePathReads
{
    NSFileManager *fileManager = NSFileManager.defaultManager;
    NSURL *root = [fileManager.temporaryDirectory
        URLByAppendingPathComponent:[NSUUID UUID].UUIDString
                        isDirectory:YES];
    NSURL *single = [root URLByAppendingPathComponent:@"single"
                                          isDirectory:YES];
    NSURL *nested = [root URLByAppendingPathComponent:@"nested/deeper"
                                           isDirectory:YES];
    NSURL *fileURL = [nested URLByAppendingPathComponent:@"file.txt"
                                             isDirectory:NO];
    NSURL *singleFileURL = [single URLByAppendingPathComponent:@"only.txt"
                                                   isDirectory:NO];
    NSURL *linkURL = [nested URLByAppendingPathComponent:@"file-link"
                                             isDirectory:NO];
    NSData *payload = [@"nested-data" dataUsingEncoding:NSUTF8StringEncoding];
    NSData *singlePayload = [@"only" dataUsingEncoding:NSUTF8StringEncoding];
    struct OrlixHostDirectoryEntry entry;
    uint8_t buffer[32] = {0};
    char linkBuffer[32] = {0};

    XCTAssertTrue([fileManager createDirectoryAtURL:single
                        withIntermediateDirectories:YES
                                         attributes:nil
                                              error:nil]);
    XCTAssertTrue([fileManager createDirectoryAtURL:nested
                        withIntermediateDirectories:YES
                                         attributes:nil
                                              error:nil]);
    XCTAssertTrue([payload writeToURL:fileURL atomically:YES]);
    XCTAssertTrue([singlePayload writeToURL:singleFileURL atomically:YES]);
    XCTAssertEqual(symlink("file.txt", linkURL.path.UTF8String), 0);
    XCTAssertEqual(orlix_host_resources_clear_host_directories(), 0);
    XCTAssertEqual(orlix_host_resources_register_host_directory(
                       "orlix-host0", root.path.UTF8String, 1),
                   0);

    XCTAssertEqual(orlix_host_directory_read_entry_at_path(
                       0, "nested/deeper/file.txt", &entry),
                   0);
    XCTAssertEqual(entry.type, OrlixHostDirectoryEntryRegular);
    XCTAssertEqual(strcmp(entry.name, "file.txt"), 0);
    XCTAssertEqual(entry.size, payload.length);

    XCTAssertEqual(orlix_host_directory_read_directory_entry_at_path(
                       0, "single", 0, &entry),
                   0);
    XCTAssertEqual(entry.type, OrlixHostDirectoryEntryRegular);
    XCTAssertEqual(strcmp(entry.name, "only.txt"), 0);

    XCTAssertEqual(orlix_host_directory_read_file_at_path(
                       0, "nested/deeper/file.txt", 0, buffer,
                       (uint32_t)sizeof(buffer)),
                   (long)payload.length);
    XCTAssertEqual(memcmp(buffer, payload.bytes, payload.length), 0);

    XCTAssertEqual(orlix_host_directory_read_link_at_path(
                       0, "nested/deeper/file-link", linkBuffer,
                       (uint32_t)sizeof(linkBuffer)),
                   (long)strlen("file.txt"));
    XCTAssertEqual(strncmp(linkBuffer, "file.txt", strlen("file.txt")), 0);

    XCTAssertNotEqual(orlix_host_directory_read_entry_at_path(
                          0, "/absolute", &entry),
                      0);
    XCTAssertNotEqual(orlix_host_directory_read_file_at_path(
                          0, "../escape", 0, buffer, (uint32_t)sizeof(buffer)),
                      0);

    [fileManager removeItemAtURL:root error:nil];
    XCTAssertEqual(orlix_host_resources_clear_host_directories(), 0);
}

- (void)testIOMappingAvoidsHostedKernelVmallocRange
{
    const unsigned long hostedLinuxStart = 0x0000000100000000UL;
    const unsigned long hostedLinuxEnd = 0x0000000300000000UL;
    void *mapping = orlix_host_ioremap(0x10000000UL, 0x200UL);
    unsigned long address = (unsigned long)mapping;
    unsigned long physicalAddress = 0;

    XCTAssertNotEqual(mapping, NULL);
    if (!mapping) {
        return;
    }

    XCTAssertFalse(address >= hostedLinuxStart && address < hostedLinuxEnd);
    XCTAssertEqual(orlix_host_iomem_physical_address(mapping,
                                                     &physicalAddress),
                   0);
    XCTAssertEqual(physicalAddress, 0x10000000UL);

    orlix_host_iounmap(mapping);
}

- (void)testKernelReservationDiscoversHostMappableWindow
{
    const unsigned long pageSize = orlix_host_memory_page_size();
    const unsigned long length = pageSize * 2UL;
    unsigned long minimumAddress = 0;
    unsigned long maximumAddress = 0;
    unsigned long base = 0;
    void *page = NULL;

    XCTAssertEqual(OrlixHostAdapterTestCreateDiscoveredGap(length,
                                                           pageSize,
                                                           &minimumAddress,
                                                           &maximumAddress),
                   0);
    XCTAssertEqual(posix_memalign(&page, pageSize, pageSize), 0);
    XCTAssertNotEqual(page, NULL);
    if (!page) {
        return;
    }

    memset(page, 0x5a, pageSize);
    XCTAssertEqual(orlix_host_kernel_reserve_window(minimumAddress,
                                                    maximumAddress,
                                                    length,
                                                    pageSize,
                                                    &base),
                   0);
    XCTAssertGreaterThanOrEqual(base, minimumAddress);
    XCTAssertLessThanOrEqual(base + length, maximumAddress);
    XCTAssertEqual(base & (pageSize - 1UL), 0UL);
    XCTAssertEqual(orlix_host_kernel_map_page(base, page, pageSize), 0);
    XCTAssertEqual(((unsigned char *)base)[0], 0x5a);
    XCTAssertEqual(((unsigned char *)base)[pageSize - 1UL], 0x5a);

    orlix_host_kernel_unmap_pages(base, length);
    free(page);
}

- (void)testKernelReservationSkipsUnavailableAddressesInDiscoveredGap
{
    const unsigned long pageSize = orlix_host_memory_page_size();
    const unsigned long gapLength = pageSize * 8UL;
    unsigned long minimumAddress = 0;
    unsigned long maximumAddress = 0;
    vm_address_t occupied[7] = {0};
    unsigned long base = 0;

    XCTAssertEqual(OrlixHostAdapterTestCreateDiscoveredGap(gapLength,
                                                           pageSize,
                                                           &minimumAddress,
                                                           &maximumAddress),
                   0);
    for (unsigned long index = 0; index < 7; index++) {
        occupied[index] = (vm_address_t)(minimumAddress + pageSize * index);
        XCTAssertEqual(vm_allocate(mach_task_self(),
                                   &occupied[index],
                                   (vm_size_t)pageSize,
                                   VM_FLAGS_FIXED),
                       KERN_SUCCESS);
        XCTAssertEqual(occupied[index],
                       (vm_address_t)(minimumAddress + pageSize * index));
    }

    XCTAssertEqual(orlix_host_kernel_reserve_window(minimumAddress,
                                                    maximumAddress,
                                                    pageSize,
                                                    pageSize,
                                                    &base),
                   0);
    XCTAssertEqual(base, minimumAddress + pageSize * 7UL);
    XCTAssertLessThanOrEqual(base + pageSize, maximumAddress);
    XCTAssertEqual(base & (pageSize - 1UL), 0UL);

    orlix_host_kernel_unmap_pages(base, pageSize);
    for (unsigned long index = 0; index < 7; index++) {
        vm_deallocate(mach_task_self(), occupied[index], (vm_size_t)pageSize);
    }
}

- (void)testKernelReservationFallsBackToHostChosenRangeWhenPreferredGapIsUnavailable
{
    const unsigned long pageSize = orlix_host_memory_page_size();
    unsigned long minimumAddress = 0;
    unsigned long maximumAddress = 0;
    vm_address_t occupied = 0;
    unsigned long base = 0;
    void *page = NULL;

    XCTAssertEqual(OrlixHostAdapterTestCreateDiscoveredGap(pageSize,
                                                           pageSize,
                                                           &minimumAddress,
                                                           &maximumAddress),
                   0);
    occupied = (vm_address_t)minimumAddress;
    XCTAssertEqual(vm_allocate(mach_task_self(),
                               &occupied,
                               (vm_size_t)pageSize,
                               VM_FLAGS_FIXED),
                   KERN_SUCCESS);
    XCTAssertEqual(occupied, (vm_address_t)minimumAddress);
    XCTAssertEqual(posix_memalign(&page, pageSize, pageSize), 0);
    XCTAssertNotEqual(page, NULL);
    if (!page) {
        vm_deallocate(mach_task_self(), occupied, (vm_size_t)pageSize);
        return;
    }

    memset(page, 0xa5, pageSize);
    XCTAssertEqual(orlix_host_kernel_reserve_window(minimumAddress,
                                                    maximumAddress,
                                                    pageSize,
                                                    pageSize,
                                                    &base),
                   0);
    XCTAssertNotEqual(base, 0UL);
    XCTAssertNotEqual(base, minimumAddress);
    XCTAssertEqual(base & (pageSize - 1UL), 0UL);
    XCTAssertEqual(orlix_host_kernel_map_page(base, page, pageSize), 0);
    XCTAssertEqual(((unsigned char *)base)[0], 0xa5);

    orlix_host_kernel_unmap_pages(base, pageSize);
    vm_deallocate(mach_task_self(), occupied, (vm_size_t)pageSize);
    free(page);
}


- (void)testBootProgressRecordsOrderedSequence
{
    orlix_host_boot_progress_event_t events[4] = {0};
    orlix_host_boot_progress_reset();

    orlix_host_boot_progress_record(ORLIX_HOST_BOOT_STAGE_SESSION_CREATED, 0, 0, 0);
    orlix_host_boot_progress_record(ORLIX_HOST_BOOT_STAGE_PAYLOAD_REGISTERING, 0, 0, 0);

    XCTAssertEqual(orlix_host_boot_progress_snapshot(events, 4), 2U);
    XCTAssertEqual(events[0].sequence, 1ULL);
    XCTAssertEqual(events[0].stage, ORLIX_HOST_BOOT_STAGE_SESSION_CREATED);
    XCTAssertEqual(events[1].sequence, 2ULL);
    XCTAssertEqual(events[1].stage, ORLIX_HOST_BOOT_STAGE_PAYLOAD_REGISTERING);
    XCTAssertGreaterThanOrEqual(events[1].monotonic_ns, events[0].monotonic_ns);
}

- (void)testBootProgressRingBufferIsBoundedAndOldestToNewest
{
    orlix_host_boot_progress_event_t events[64] = {0};
    orlix_host_boot_progress_reset();

    for (uint32_t index = 0; index < 70; index++) {
        orlix_host_boot_progress_record(index, (int32_t)index, 0, 0);
    }

    XCTAssertEqual(orlix_host_boot_progress_snapshot(events, 64), 64U);
    XCTAssertEqual(events[0].sequence, 7ULL);
    XCTAssertEqual(events[0].stage, 6U);
    XCTAssertEqual(events[63].sequence, 70ULL);
    XCTAssertEqual(events[63].stage, 69U);
}

- (void)testBootProgressLatestReturnsNewestEvent
{
    orlix_host_boot_progress_event_t event = {0};
    orlix_host_boot_progress_reset();

    XCTAssertEqual(orlix_host_boot_progress_latest(&event), 0);
    orlix_host_boot_progress_record(ORLIX_HOST_BOOT_STAGE_PAYLOAD_REGISTERED, 0, 0, 0);
    orlix_host_boot_progress_record(ORLIX_HOST_BOOT_STAGE_BOOTLOADER_ENTERED, 0, 0, 0);

    XCTAssertEqual(orlix_host_boot_progress_latest(&event), 1);
    XCTAssertEqual(event.sequence, 2ULL);
    XCTAssertEqual(event.stage, ORLIX_HOST_BOOT_STAGE_BOOTLOADER_ENTERED);
}

- (void)testBootProgressResetClearsEvents
{
    orlix_host_boot_progress_event_t event = {0};
    orlix_host_boot_progress_reset();
    orlix_host_boot_progress_record(ORLIX_HOST_BOOT_STAGE_SESSION_CREATED, 0, 0, 0);

    XCTAssertEqual(orlix_host_boot_progress_latest(&event), 1);
    orlix_host_boot_progress_reset();
    XCTAssertEqual(orlix_host_boot_progress_latest(&event), 0);
}

- (void)testBootProgressConcurrentRecordsDoNotCorruptState
{
    orlix_host_boot_progress_event_t events[64] = {0};
    orlix_host_boot_progress_reset();

    dispatch_apply(128, dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^(size_t index) {
        orlix_host_boot_progress_record(
            ORLIX_HOST_BOOT_STAGE_HOST_RESOURCES_READY,
            (int32_t)index,
            0,
            0
        );
    });

    uint32_t count = orlix_host_boot_progress_snapshot(events, 64);
    XCTAssertEqual(count, 64U);
    for (uint32_t index = 1; index < count; index++) {
        XCTAssertGreaterThan(events[index].sequence, events[index - 1].sequence);
    }
}

- (void)testBootProgressFailureDetailsArePreserved
{
    orlix_host_boot_progress_event_t event = {0};
    orlix_host_boot_progress_reset();

    orlix_host_boot_progress_record(
        ORLIX_HOST_BOOT_STAGE_FAILED,
        -3,
        5,
        22
    );

    XCTAssertEqual(orlix_host_boot_progress_latest(&event), 1);
    XCTAssertEqual(event.stage, ORLIX_HOST_BOOT_STAGE_FAILED);
    XCTAssertEqual(event.status, -3);
    XCTAssertEqual(event.mach_kern_return, 5);
    XCTAssertEqual(event.posix_errno, 22);
}

- (void)testBootProgressFirstConsoleOutputRecordsOnlyOncePerReset
{
    orlix_host_boot_progress_event_t events[4] = {0};
    orlix_host_boot_progress_reset();

    orlix_host_boot_progress_record(ORLIX_HOST_BOOT_STAGE_FIRST_CONSOLE_OUTPUT, 0, 0, 0);
    orlix_host_boot_progress_record(ORLIX_HOST_BOOT_STAGE_FIRST_CONSOLE_OUTPUT, 0, 0, 0);

    XCTAssertEqual(orlix_host_boot_progress_snapshot(events, 4), 1U);
    XCTAssertEqual(events[0].stage, ORLIX_HOST_BOOT_STAGE_FIRST_CONSOLE_OUTPUT);

    orlix_host_boot_progress_reset();
    XCTAssertEqual(orlix_host_boot_progress_snapshot(events, 4), 0U);
    orlix_host_boot_progress_record(ORLIX_HOST_BOOT_STAGE_FIRST_CONSOLE_OUTPUT, 0, 0, 0);
    XCTAssertEqual(orlix_host_boot_progress_snapshot(events, 4), 1U);
}

- (void)testConsoleRecentOutputClearMakesSnapshotEmpty
{
    unsigned char buffer[32] = {0};
    const char payload[] = "linux console boot\n";

    orlix_host_console_recent_output_clear(ORLIX_HOST_CONSOLE_SOURCE_VIRTIO);
    orlix_host_console_write(ORLIX_HOST_CONSOLE_SOURCE_VIRTIO,
                             payload, sizeof(payload) - 1);
    XCTAssertGreaterThan(orlix_host_console_recent_output_snapshot(
                             ORLIX_HOST_CONSOLE_SOURCE_VIRTIO,
                             buffer,
                             sizeof(buffer)),
                         0UL);

    orlix_host_console_recent_output_clear(ORLIX_HOST_CONSOLE_SOURCE_VIRTIO);
    XCTAssertEqual(orlix_host_console_recent_output_snapshot(
                                                             ORLIX_HOST_CONSOLE_SOURCE_VIRTIO,
                                                             buffer,
                                                             sizeof(buffer)),
                   0UL);
}

- (void)testConsoleRecentOutputSnapshotReturnsWrittenBytes
{
    unsigned char buffer[64] = {0};
    const char payload[] = "console mirror line\n";

    orlix_host_console_recent_output_clear(ORLIX_HOST_CONSOLE_SOURCE_VIRTIO);
    orlix_host_console_write(ORLIX_HOST_CONSOLE_SOURCE_VIRTIO,
                             payload, sizeof(payload) - 1);

    unsigned long count =
        orlix_host_console_recent_output_snapshot(
            ORLIX_HOST_CONSOLE_SOURCE_VIRTIO, buffer, sizeof(buffer));
    XCTAssertEqual(count, sizeof(payload) - 1);
    XCTAssertEqual(memcmp(buffer, payload, sizeof(payload) - 1), 0);
}

- (void)testConsoleRecentOutputPreservesMultipleWriteOrdering
{
    unsigned char buffer[64] = {0};
    const char first[] = "first";
    const char second[] = "-second";
    const char expected[] = "first-second";

    orlix_host_console_recent_output_clear(ORLIX_HOST_CONSOLE_SOURCE_VIRTIO);
    orlix_host_console_write(ORLIX_HOST_CONSOLE_SOURCE_VIRTIO,
                             first, sizeof(first) - 1);
    orlix_host_console_write(ORLIX_HOST_CONSOLE_SOURCE_VIRTIO,
                             second, sizeof(second) - 1);

    unsigned long count =
        orlix_host_console_recent_output_snapshot(
            ORLIX_HOST_CONSOLE_SOURCE_VIRTIO, buffer, sizeof(buffer));
    XCTAssertEqual(count, sizeof(expected) - 1);
    XCTAssertEqual(memcmp(buffer, expected, sizeof(expected) - 1), 0);
}

- (void)testConsoleRecentOutputSnapshotRespectsCapacity
{
    unsigned char buffer[5] = {0};
    const char payload[] = "abcdef";

    orlix_host_console_recent_output_clear(ORLIX_HOST_CONSOLE_SOURCE_VIRTIO);
    orlix_host_console_write(ORLIX_HOST_CONSOLE_SOURCE_VIRTIO,
                             payload, sizeof(payload) - 1);

    XCTAssertEqual(orlix_host_console_recent_output_snapshot(
                                                             ORLIX_HOST_CONSOLE_SOURCE_VIRTIO,
                                                             buffer,
                                                             sizeof(buffer)),
                   5UL);
    XCTAssertEqual(memcmp(buffer, "abcde", 5), 0);
}

- (void)testConsoleRecentOutputRingBufferIsBoundedAndOldestToNewest
{
    enum { payloadLength = 65536 + 17 };
    NSMutableData *payload = [NSMutableData dataWithLength:payloadLength];
    NSMutableData *snapshot = [NSMutableData dataWithLength:65536];
    unsigned char *payloadBytes = payload.mutableBytes;

    for (NSUInteger index = 0; index < payloadLength; index++) {
        payloadBytes[index] = (unsigned char)('a' + (index % 26));
    }

    orlix_host_console_recent_output_clear(ORLIX_HOST_CONSOLE_SOURCE_VIRTIO);
    orlix_host_console_write(ORLIX_HOST_CONSOLE_SOURCE_VIRTIO,
                             payload.bytes, payload.length);

    unsigned long count = orlix_host_console_recent_output_snapshot(
        ORLIX_HOST_CONSOLE_SOURCE_VIRTIO,
        snapshot.mutableBytes,
        snapshot.length);
    XCTAssertEqual(count, 65536UL);
    XCTAssertEqual(memcmp(snapshot.bytes,
                          payloadBytes + 17,
                          65536),
                   0);
}

- (void)testConsoleRecentOutputConcurrentWritesDoNotCorruptState
{
    unsigned char buffer[65536] = {0};

    orlix_host_console_recent_output_clear(ORLIX_HOST_CONSOLE_SOURCE_VIRTIO);
    dispatch_apply(128,
                   dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
                   ^(size_t index) {
                       char payload[32];
                       int length = snprintf(payload,
                                             sizeof(payload),
                                             "line-%03zu\n",
                                             index);
                       orlix_host_console_write(ORLIX_HOST_CONSOLE_SOURCE_VIRTIO,
                                                payload,
                                                (unsigned long)length);
                   });

    unsigned long count =
        orlix_host_console_recent_output_snapshot(
            ORLIX_HOST_CONSOLE_SOURCE_VIRTIO, buffer, sizeof(buffer));
    XCTAssertGreaterThan(count, 0UL);
    XCTAssertLessThanOrEqual(count, (unsigned long)sizeof(buffer));
}

- (void)testConsoleWriteRecordsFirstConsoleOutputBootProgress
{
    orlix_host_boot_progress_event_t events[4] = {0};
    const char payload[] = "first console byte";

    orlix_host_console_recent_output_clear(ORLIX_HOST_CONSOLE_SOURCE_VIRTIO);
    orlix_host_boot_progress_reset();
    orlix_host_console_write(ORLIX_HOST_CONSOLE_SOURCE_VIRTIO,
                             payload, sizeof(payload) - 1);
    orlix_host_console_write(ORLIX_HOST_CONSOLE_SOURCE_VIRTIO,
                             payload, sizeof(payload) - 1);

    XCTAssertEqual(orlix_host_boot_progress_snapshot(events, 4), 1U);
    XCTAssertEqual(events[0].stage, ORLIX_HOST_BOOT_STAGE_FIRST_CONSOLE_OUTPUT);
}

- (void)testConsoleSourcesKeepRecentOutputIndependent
{
    unsigned char serialBuffer[32] = {0};
    unsigned char virtioBuffer[32] = {0};
    const char serial[] = "serial";
    const char virtio[] = "virtio";

    orlix_host_console_recent_output_clear(ORLIX_HOST_CONSOLE_SOURCE_SERIAL);
    orlix_host_console_recent_output_clear(ORLIX_HOST_CONSOLE_SOURCE_VIRTIO);
    orlix_host_console_write(ORLIX_HOST_CONSOLE_SOURCE_SERIAL,
                             serial, sizeof(serial) - 1);
    orlix_host_console_write(ORLIX_HOST_CONSOLE_SOURCE_VIRTIO,
                             virtio, sizeof(virtio) - 1);

    unsigned long serialCount = orlix_host_console_recent_output_snapshot(
        ORLIX_HOST_CONSOLE_SOURCE_SERIAL, serialBuffer, sizeof(serialBuffer));
    unsigned long virtioCount = orlix_host_console_recent_output_snapshot(
        ORLIX_HOST_CONSOLE_SOURCE_VIRTIO, virtioBuffer, sizeof(virtioBuffer));
    XCTAssertEqual(serialCount, sizeof(serial) - 1);
    XCTAssertEqual(virtioCount, sizeof(virtio) - 1);
    XCTAssertEqual(memcmp(serialBuffer, serial, sizeof(serial) - 1), 0);
    XCTAssertEqual(memcmp(virtioBuffer, virtio, sizeof(virtio) - 1), 0);
}

- (void)testConsoleInputClearRemovesStaleSessionBytes
{
    const char input[] = "stale";
    unsigned char buffer[sizeof(input)] = {0};

    orlix_host_console_clear_input(ORLIX_HOST_CONSOLE_SOURCE_VIRTIO);
    XCTAssertEqual(orlix_host_console_enqueue_input(
                       ORLIX_HOST_CONSOLE_SOURCE_VIRTIO, input, sizeof(input)),
                   sizeof(input));
    XCTAssertEqual(orlix_host_console_pending_input(
                       ORLIX_HOST_CONSOLE_SOURCE_VIRTIO), sizeof(input));
    orlix_host_console_clear_input(ORLIX_HOST_CONSOLE_SOURCE_VIRTIO);
    XCTAssertEqual(orlix_host_console_pending_input(
                       ORLIX_HOST_CONSOLE_SOURCE_VIRTIO), 0UL);
    XCTAssertEqual(orlix_host_console_read_input(
                       ORLIX_HOST_CONSOLE_SOURCE_VIRTIO, buffer, sizeof(buffer)),
                   0UL);
}

- (void)testConsoleInputSourcesRemainIndependent
{
    const char serial[] = "serial-input";
    unsigned char buffer[sizeof(serial)] = {0};

    orlix_host_console_clear_input(ORLIX_HOST_CONSOLE_SOURCE_SERIAL);
    orlix_host_console_clear_input(ORLIX_HOST_CONSOLE_SOURCE_VIRTIO);
    XCTAssertEqual(orlix_host_console_enqueue_input(
                       ORLIX_HOST_CONSOLE_SOURCE_SERIAL,
                       serial, sizeof(serial)), sizeof(serial));
    XCTAssertEqual(orlix_host_console_read_input(
                       ORLIX_HOST_CONSOLE_SOURCE_VIRTIO,
                       buffer, sizeof(buffer)), 0UL);
    XCTAssertEqual(orlix_host_console_read_input(
                       ORLIX_HOST_CONSOLE_SOURCE_SERIAL,
                       buffer, sizeof(buffer)), sizeof(serial));
    XCTAssertEqual(memcmp(buffer, serial, sizeof(serial)), 0);
}

@end
