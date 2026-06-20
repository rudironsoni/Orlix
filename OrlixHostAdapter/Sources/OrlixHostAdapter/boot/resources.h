#ifndef ORLIX_HOST_BOOT_RESOURCES_H
#define ORLIX_HOST_BOOT_RESOURCES_H

struct OrlixHostResource {
    void *data;
    unsigned long size;
};

/* App-private HostAdapter SPI; OrlixOS resolves its own target bundle. */
__attribute__((visibility("default"))) int orlix_host_resources_set_payload_root_path(
    const char *path);

__attribute__((visibility("default"))) int orlix_host_resources_clear_root_images(void);

#include <stdint.h>

#define ORLIX_HOST_DIRECTORY_NAME_MAX 255
#define ORLIX_HOST_DIRECTORY_XATTR_NAME_MAX 255
#define ORLIX_HOST_DIRECTORY_XATTR_VALUE_MAX 4096

enum OrlixHostDirectoryEntryType {
    OrlixHostDirectoryEntryUnknown = 0,
    OrlixHostDirectoryEntryRegular = 1,
    OrlixHostDirectoryEntryDirectory = 2,
    OrlixHostDirectoryEntrySymlink = 3,
};

struct OrlixHostDirectoryEntry {
    uint64_t inode;
    uint64_t size;
    uint32_t mode;
    uint8_t type;
    char name[ORLIX_HOST_DIRECTORY_NAME_MAX + 1];
};

__attribute__((visibility("default"))) int orlix_host_resources_register_root_image(
    const char *identifier,
    const char *initrd_bundle_name,
    const char *initrd_bundle_extension,
    const char *initrd_resource,
    const char *base_block_resource,
    const char *state_block_resource,
    unsigned int base_block_device,
    unsigned int state_block_device,
    unsigned long long state_block_minimum_bytes);

__attribute__((visibility("default"))) int orlix_host_resources_register_root_image_files(
    const char *identifier,
    const char *initrd_bundle_name,
    const char *initrd_bundle_extension,
    const char *initrd_resource,
    const char *base_block_path,
    const char *state_block_path,
    unsigned int base_block_device,
    unsigned int state_block_device,
    unsigned long long state_block_minimum_bytes);

__attribute__((visibility("default"))) int orlix_host_resources_clear_host_directories(void);

__attribute__((visibility("default"))) int orlix_host_resources_register_host_directory(
    const char *identifier,
    const char *host_path,
    unsigned int read_only);
__attribute__((visibility("default"))) int orlix_host_resources_register_host_directory_xattr(
    const char *identifier,
    const char *relative_path,
    const char *name,
    const void *value,
    uint32_t value_length);

__attribute__((visibility("hidden"))) int OrlixHostLoadKernelPayloadResource(
    const char *resource,
    struct OrlixHostResource *loaded);

__attribute__((visibility("hidden"))) int OrlixHostLoadInitrdResource(
    const char *identifier,
    struct OrlixHostResource *loaded);

__attribute__((visibility("hidden"))) int OrlixHostSelectBootBlockImages(
    const char *identifier);

__attribute__((visibility("hidden"))) int OrlixHostCopyHostDirectoryPath(
    const char *identifier,
    char *path,
    unsigned long path_size,
    unsigned int *read_only);

__attribute__((visibility("hidden"))) int orlix_host_directory_read_entry(
    unsigned int directory,
    unsigned int entry_index,
    struct OrlixHostDirectoryEntry *entry);
__attribute__((visibility("hidden"))) int orlix_host_directory_read_child_entry(
    unsigned int directory,
    unsigned int parent_entry_index,
    unsigned int entry_index,
    struct OrlixHostDirectoryEntry *entry);
__attribute__((visibility("hidden"))) long orlix_host_directory_read_file(
    unsigned int directory,
    unsigned int entry_index,
    uint64_t offset,
    void *buffer,
    uint32_t length);
__attribute__((visibility("hidden"))) long orlix_host_directory_read_child_file(
    unsigned int directory,
    unsigned int parent_entry_index,
    unsigned int entry_index,
    uint64_t offset,
    void *buffer,
    uint32_t length);
__attribute__((visibility("hidden"))) long orlix_host_directory_read_link(
    unsigned int directory,
    unsigned int entry_index,
    void *buffer,
    uint32_t length);
__attribute__((visibility("hidden"))) long orlix_host_directory_read_child_link(
    unsigned int directory,
    unsigned int parent_entry_index,
    unsigned int entry_index,
    void *buffer,
    uint32_t length);
__attribute__((visibility("hidden"))) long orlix_host_directory_list_xattr(
    unsigned int directory,
    const char *relative_path,
    char *buffer,
    uint64_t capacity);
__attribute__((visibility("hidden"))) long orlix_host_directory_read_xattr(
    unsigned int directory,
    const char *relative_path,
    const char *name,
    void *buffer,
    uint64_t capacity);

__attribute__((visibility("hidden"))) int orlix_host_block_capacity(
    unsigned int device,
    unsigned long long *sectors);

__attribute__((visibility("hidden"))) int orlix_host_block_read(
    unsigned int device,
    unsigned long long sector,
    void *buffer,
    unsigned int length);

__attribute__((visibility("hidden"))) int orlix_host_block_write(
    unsigned int device,
    unsigned long long sector,
    const void *buffer,
    unsigned int length);

__attribute__((visibility("hidden"))) int orlix_host_block_flush(
    unsigned int device);

__attribute__((visibility("hidden"))) void OrlixHostFreeResource(
    struct OrlixHostResource *resource);

#endif
