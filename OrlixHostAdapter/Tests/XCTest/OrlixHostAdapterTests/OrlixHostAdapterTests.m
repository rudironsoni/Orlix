#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#include <string.h>
#include <unistd.h>
#include "OrlixHostAdapter/boot/resources.h"
#include "OrlixHostAdapter/memory/kernel_mapping.h"

#include <limits.h>
#include <mach/mach.h>
#include <mach/vm_page_size.h>
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

@interface OrlixHostAdapterTests : XCTestCase
@end

@implementation OrlixHostAdapterTests

- (void)testUserMappingAdaptsLinuxPageInsideHostPage
{
    const unsigned long linuxPageSize = ORLIX_HOST_ADAPTER_TEST_LINUX_PAGE_SIZE;
    vm_address_t reserved = 0;
    kern_return_t status = vm_allocate(mach_task_self(),
                                       &reserved,
                                       (vm_size_t)vm_page_size,
                                       VM_FLAGS_ANYWHERE);
    XCTAssertEqual(status, KERN_SUCCESS);
    XCTAssertNotEqual(reserved, (vm_address_t)0);
    if (status != KERN_SUCCESS || !reserved) {
        return;
    }

    status = vm_deallocate(mach_task_self(), reserved, (vm_size_t)vm_page_size);
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
    vm_address_t reserved = 0;
    kern_return_t status = vm_allocate(mach_task_self(),
                                       &reserved,
                                       (vm_size_t)vm_page_size,
                                       VM_FLAGS_ANYWHERE);
    XCTAssertEqual(status, KERN_SUCCESS);
    XCTAssertNotEqual(reserved, (vm_address_t)0);
    if (status != KERN_SUCCESS || !reserved) {
        return;
    }

    status = vm_deallocate(mach_task_self(), reserved, (vm_size_t)vm_page_size);
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
                                             (unsigned long)vm_page_size,
                                             segments,
                                             sizeof(segments) / sizeof(segments[0]));
    XCTAssertEqual(ret, 0);

    unsigned char firstByte = ((volatile unsigned char *)reserved)[0];
    unsigned char secondByte = ((volatile unsigned char *)reserved)[linuxPageSize];
    XCTAssertEqual(firstByte, 0x11);
    XCTAssertEqual(secondByte, 0x22);

    orlix_host_user_unmap_pages((unsigned long)reserved,
                                (unsigned long)vm_page_size);
    free(first);
    free(second);
}

- (void)testUserWindowRefreshMapsWritableLinuxPageAfterHoles
{
    if (vm_page_size < 8192) {
        return;
    }

    const unsigned long linuxPageSize = ORLIX_HOST_ADAPTER_TEST_LINUX_PAGE_SIZE;
    vm_address_t reserved = 0;
    kern_return_t status = vm_allocate(mach_task_self(),
                                       &reserved,
                                       (vm_size_t)vm_page_size,
                                       VM_FLAGS_ANYWHERE);
    XCTAssertEqual(status, KERN_SUCCESS);
    XCTAssertNotEqual(reserved, (vm_address_t)0);
    if (status != KERN_SUCCESS || !reserved) {
        return;
    }

    status = vm_deallocate(mach_task_self(), reserved, (vm_size_t)vm_page_size);
    XCTAssertEqual(status, KERN_SUCCESS);

    unsigned char *source = malloc((size_t)linuxPageSize);
    XCTAssertTrue(source != NULL);
    if (!source) {
        return;
    }
    memset(source, 0x33, (size_t)linuxPageSize);

    unsigned long target = (unsigned long)reserved + (unsigned long)vm_page_size - linuxPageSize;
    struct orlix_host_user_page_segment segment = {
        .target_address = target,
        .source_page = source,
        .length = linuxPageSize,
        .writable = 1,
        .executable = 0,
    };

    int ret = orlix_host_user_refresh_window((unsigned long)reserved,
                                             (unsigned long)vm_page_size,
                                             &segment,
                                             1);
    XCTAssertEqual(ret, 0);

    ((volatile unsigned char *)target)[0] = 0x44;
    ((volatile unsigned char *)target)[linuxPageSize - 1] = 0x55;
    orlix_host_user_sync_writable_mappings();
    XCTAssertEqual(source[0], 0x44);
    XCTAssertEqual(source[linuxPageSize - 1], 0x55);

    orlix_host_user_unmap_pages((unsigned long)reserved,
                                (unsigned long)vm_page_size);
    free(source);
}

- (void)testKernelMappingAdaptsMultipleLinuxPagesInsideHostPage
{
    if (vm_page_size < 8192) {
        return;
    }

    const unsigned long linuxPageSize = ORLIX_HOST_ADAPTER_TEST_LINUX_PAGE_SIZE;
    vm_address_t reserved = 0;
    kern_return_t status = vm_allocate(mach_task_self(),
                                       &reserved,
                                       (vm_size_t)vm_page_size,
                                       VM_FLAGS_ANYWHERE);
    XCTAssertEqual(status, KERN_SUCCESS);
    XCTAssertNotEqual(reserved, (vm_address_t)0);
    if (status != KERN_SUCCESS || !reserved) {
        return;
    }

    status = vm_deallocate(mach_task_self(), reserved, (vm_size_t)vm_page_size);
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
    orlix_host_kernel_unmap_pages(target, (unsigned long)vm_page_size);
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
    const unsigned long vmallocStart = 0x0000700000000000UL;
    const unsigned long vmallocEnd = 0x0000780000000000UL;
    void *mapping = orlix_host_ioremap(0x10000000UL, 0x200UL);
    unsigned long address = (unsigned long)mapping;
    unsigned long physicalAddress = 0;

    XCTAssertNotEqual(mapping, NULL);
    if (!mapping) {
        return;
    }

    XCTAssertFalse(address >= vmallocStart && address < vmallocEnd);
    XCTAssertEqual(orlix_host_iomem_physical_address(mapping,
                                                     &physicalAddress),
                   0);
    XCTAssertEqual(physicalAddress, 0x10000000UL);

    orlix_host_iounmap(mapping);
}

@end
