#include "OrlixHostAdapter/boot/resources.h"
#include "OrlixHostAdapter/execution/host_tls.h"

#include <CoreFoundation/CoreFoundation.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <os/lock.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#define ORLIX_HOST_BLOCK_SECTOR_SIZE 512ULL
#define ORLIX_HOST_MAX_BLOCK_DEVICES 8
#define ORLIX_HOST_MAX_ROOT_IMAGES 8
#define ORLIX_HOST_MAX_HOST_DIRECTORIES 16
#define ORLIX_HOST_MAX_HOST_DIRECTORY_XATTRS 128

static char OrlixHostSelectedBlockPaths[ORLIX_HOST_MAX_BLOCK_DEVICES][PATH_MAX];
static unsigned long long OrlixHostSelectedBlockBytes[ORLIX_HOST_MAX_BLOCK_DEVICES];
static int OrlixHostSelectedBlockWritable[ORLIX_HOST_MAX_BLOCK_DEVICES];
static os_unfair_lock OrlixHostPayloadRootLock = OS_UNFAIR_LOCK_INIT;
static char OrlixHostPayloadRootPath[PATH_MAX];
static os_unfair_lock OrlixHostRootImagesLock = OS_UNFAIR_LOCK_INIT;
static os_unfair_lock OrlixHostDirectoriesLock = OS_UNFAIR_LOCK_INIT;

struct OrlixHostRootImage {
    char identifier[PATH_MAX];
    char initrd_bundle_name[PATH_MAX];
    char initrd_bundle_extension[PATH_MAX];
    char initrd_resource[PATH_MAX];
    char base_block_resource[PATH_MAX];
    char state_block_resource[PATH_MAX];
    int block_images_are_files;
    unsigned int base_block_device;
    unsigned int state_block_device;
    unsigned long long state_block_minimum_bytes;
};

static struct OrlixHostRootImage
    OrlixHostRootImages[ORLIX_HOST_MAX_ROOT_IMAGES];
static unsigned int OrlixHostRootImageCount;

struct OrlixHostDirectoryResource {
    char identifier[PATH_MAX];
    char host_path[PATH_MAX];
    unsigned int read_only;
};

struct OrlixHostDirectoryXattrResource {
    char identifier[PATH_MAX];
    char relative_path[PATH_MAX];
    char name[ORLIX_HOST_DIRECTORY_XATTR_NAME_MAX + 1];
    uint8_t value[ORLIX_HOST_DIRECTORY_XATTR_VALUE_MAX];
    uint32_t value_length;
};

static struct OrlixHostDirectoryResource
    OrlixHostDirectories[ORLIX_HOST_MAX_HOST_DIRECTORIES];
static unsigned int OrlixHostDirectoryCount;
static struct OrlixHostDirectoryXattrResource
    OrlixHostDirectoryXattrs[ORLIX_HOST_MAX_HOST_DIRECTORY_XATTRS];
static unsigned int OrlixHostDirectoryXattrCount;

static int OrlixHostCopyRequiredDirectoryPath(char *target,
                                              size_t target_size,
                                              const char *source);

static int OrlixHostCopyRequiredOpaqueIdentifier(char *target,
                                                 size_t target_size,
                                                 const char *source);
static int OrlixHostCopyRequiredRelativePath(char *target,
                                             size_t target_size,
                                             const char *source);
static int OrlixHostCopyRequiredLinuxXattrName(char *target,
                                               size_t target_size,
                                               const char *source);
static int OrlixHostDirectoryIdentifierIndexLocked(const char *identifier);

static int OrlixHostPathContainsParentReference(const char *path)
{
    const char *cursor;

    if (!path) {
        return 1;
    }
    if (path[0] == '/' || strcmp(path, "..") == 0 ||
        strncmp(path, "../", 3) == 0) {
        return 1;
    }
    cursor = path;
    while ((cursor = strstr(cursor, "/..")) != 0) {
        if (cursor[3] == '\0' || cursor[3] == '/') {
            return 1;
        }
        cursor += 3;
    }

    return 0;
}

static int OrlixHostCopyRequiredString(char *target,
                                       size_t target_size,
                                       const char *source)
{
    size_t length;

    if (!target || target_size == 0 || !source || source[0] == '\0') {
        return -1;
    }
    length = strlen(source);
    if (length >= target_size) {
        return -1;
    }
    memcpy(target, source, length + 1);
    return 0;
}

static int OrlixHostCopyOptionalString(char *target,
                                       size_t target_size,
                                       const char *source)
{
    size_t length;

    if (!target || target_size == 0) {
        return -1;
    }
    if (!source || source[0] == '\0') {
        target[0] = '\0';
        return 0;
    }
    length = strlen(source);
    if (length >= target_size) {
        return -1;
    }
    memcpy(target, source, length + 1);
    return 0;
}

static int OrlixHostCopyRequiredResource(char *target,
                                         size_t target_size,
                                         const char *source)
{
    if (OrlixHostPathContainsParentReference(source)) {
        return -1;
    }
    return OrlixHostCopyRequiredString(target, target_size, source);
}

static int OrlixHostCopyRequiredOpaqueIdentifier(char *target,
                                                 size_t target_size,
                                                 const char *source)
{
    if (!source || strchr(source, '/') || strchr(source, '\\') ||
        OrlixHostPathContainsParentReference(source)) {
        return -1;
    }

    return OrlixHostCopyRequiredString(target, target_size, source);
}

static int OrlixHostAbsolutePathContainsParentReference(const char *path)
{
    const char *cursor;

    if (!path || path[0] != '/') {
        return 1;
    }
    cursor = path;
    while ((cursor = strstr(cursor, "/..")) != 0) {
        if (cursor[3] == '\0' || cursor[3] == '/') {
            return 1;
        }
        cursor += 3;
    }
    return 0;
}

static int OrlixHostCopyRequiredBlockFilePath(char *target,
                                              size_t target_size,
                                              const char *source)
{
    struct stat state;

    if (OrlixHostAbsolutePathContainsParentReference(source) ||
        OrlixHostCopyRequiredString(target, target_size, source) != 0) {
        return -1;
    }
    if (stat(target, &state) != 0 || !S_ISREG(state.st_mode)) {
        return -1;
    }
    return 0;
}

static int OrlixHostCopyRequiredRelativePath(char *target,
                                             size_t target_size,
                                             const char *source)
{
    size_t length;

    if (!target || target_size == 0 || !source || source[0] == '\0') {
        return -1;
    }
    if (source[0] == '/' || OrlixHostPathContainsParentReference(source)) {
        return -1;
    }

    length = strlen(source);
    if (length >= target_size) {
        return -1;
    }

    memcpy(target, source, length + 1);
    return 0;
}

static int OrlixHostCopyDirectoryPathForIndex(unsigned int directory,
                                              char *target,
                                              size_t target_size)
{
    int result = -1;

    if (!target || target_size == 0)
        return -1;

    os_unfair_lock_lock(&OrlixHostDirectoriesLock);
    if (directory < OrlixHostDirectoryCount &&
        snprintf(target, target_size, "%s",
                 OrlixHostDirectories[directory].host_path) <
            (int)target_size)
        result = 0;
    os_unfair_lock_unlock(&OrlixHostDirectoriesLock);

    return result;
}

static int OrlixHostCopyDirectoryRelativeEntryPath(
    unsigned int directory,
    const char *relative_path,
    char *target,
    size_t target_size)
{
    char directory_path[PATH_MAX];
    char normalized_relative_path[PATH_MAX];

    if (!target || target_size == 0 ||
        OrlixHostCopyDirectoryPathForIndex(directory, directory_path,
                                           sizeof(directory_path)) != 0 ||
        OrlixHostCopyRequiredRelativePath(normalized_relative_path,
                                          sizeof(normalized_relative_path),
                                          relative_path) != 0)
        return -1;

    if (strcmp(normalized_relative_path, ".") == 0) {
        if (snprintf(target, target_size, "%s", directory_path) >=
            (int)target_size)
            return -1;
    } else if (snprintf(target, target_size, "%s/%s", directory_path,
                       normalized_relative_path) >= (int)target_size) {
        return -1;
    }

    return 0;
}

static int OrlixHostFillDirectoryEntryFromPath(
    const char *entry_path,
    const char *linux_name,
    struct OrlixHostDirectoryEntry *entry)
{
    struct stat state;

    if (!entry_path || !linux_name || !entry ||
        strlen(linux_name) > ORLIX_HOST_DIRECTORY_NAME_MAX ||
        lstat(entry_path, &state) != 0)
        return -1;

    memset(entry, 0, sizeof(*entry));
    entry->inode = (uint64_t)state.st_ino;
    entry->size = (uint64_t)state.st_size;
    entry->mode = (uint32_t)state.st_mode;
    if (S_ISREG(state.st_mode))
        entry->type = OrlixHostDirectoryEntryRegular;
    else if (S_ISDIR(state.st_mode))
        entry->type = OrlixHostDirectoryEntryDirectory;
    else if (S_ISLNK(state.st_mode))
        entry->type = OrlixHostDirectoryEntrySymlink;
    else
        entry->type = OrlixHostDirectoryEntryUnknown;
    snprintf(entry->name, sizeof(entry->name), "%s", linux_name);

    return 0;
}

static int OrlixHostLinuxXattrNameHasAllowedPrefix(const char *source)
{
    static const char *const prefixes[] = {
        "security.",
        "system.",
        "trusted.",
        "user.",
    };
    size_t index;

    for (index = 0; index < sizeof(prefixes) / sizeof(prefixes[0]); index++) {
        size_t prefix_length = strlen(prefixes[index]);

        if (strncmp(source, prefixes[index], prefix_length) == 0 &&
            source[prefix_length] != '\0') {
            return 1;
        }
    }

    return 0;
}

static int OrlixHostCopyRequiredLinuxXattrName(char *target,
                                               size_t target_size,
                                               const char *source)
{
    size_t length;

    if (!target || target_size == 0 || !source || source[0] == '\0') {
        return -1;
    }
    if (!OrlixHostLinuxXattrNameHasAllowedPrefix(source)) {
        return -1;
    }

    length = strlen(source);
    if (length >= target_size) {
        return -1;
    }

    memcpy(target, source, length + 1);
    return 0;
}

static int OrlixHostDirectoryIdentifierIndexLocked(const char *identifier)
{
    unsigned int index;

    if (!identifier || identifier[0] == '\0') {
        return -1;
    }

    for (index = 0; index < OrlixHostDirectoryCount; index++) {
        if (strcmp(OrlixHostDirectories[index].identifier, identifier) == 0) {
            return (int)index;
        }
    }

    return -1;
}

static int OrlixHostCopyRootImageForIdentifier(
    const char *identifier,
    struct OrlixHostRootImage *root_image)
{
    unsigned int index;

    if (!identifier || !root_image) {
        return -1;
    }

    os_unfair_lock_lock(&OrlixHostRootImagesLock);
    for (index = 0; index < OrlixHostRootImageCount; index++) {
        if (strcmp(OrlixHostRootImages[index].identifier, identifier) == 0) {
            *root_image = OrlixHostRootImages[index];
            os_unfair_lock_unlock(&OrlixHostRootImagesLock);
            return 0;
        }
    }
    os_unfair_lock_unlock(&OrlixHostRootImagesLock);
    return -1;
}

__attribute__((visibility("default"))) int orlix_host_resources_set_payload_root_path(
    const char *path)
{
    struct stat state;
    size_t length;

    if (!path || path[0] == '\0') {
        return -1;
    }
    length = strlen(path);
    if (length >= sizeof(OrlixHostPayloadRootPath)) {
        return -1;
    }
    if (stat(path, &state) != 0 || !S_ISDIR(state.st_mode)) {
        return -1;
    }

    os_unfair_lock_lock(&OrlixHostPayloadRootLock);
    memcpy(OrlixHostPayloadRootPath, path, length + 1);
    os_unfair_lock_unlock(&OrlixHostPayloadRootLock);
    return 0;
}

__attribute__((visibility("default"))) int orlix_host_resources_clear_root_images(void)
{
    os_unfair_lock_lock(&OrlixHostRootImagesLock);
    memset(OrlixHostRootImages, 0, sizeof(OrlixHostRootImages));
    OrlixHostRootImageCount = 0;
    os_unfair_lock_unlock(&OrlixHostRootImagesLock);
    return 0;
}

__attribute__((visibility("default"))) int orlix_host_resources_clear_host_directories(void)
{
    os_unfair_lock_lock(&OrlixHostDirectoriesLock);
    memset(OrlixHostDirectories, 0, sizeof(OrlixHostDirectories));
    OrlixHostDirectoryCount = 0;
    memset(OrlixHostDirectoryXattrs, 0, sizeof(OrlixHostDirectoryXattrs));
    OrlixHostDirectoryXattrCount = 0;
    os_unfair_lock_unlock(&OrlixHostDirectoriesLock);
    return 0;
}

__attribute__((visibility("default"))) int orlix_host_resources_register_host_directory(
    const char *identifier,
    const char *host_path,
    unsigned int read_only)
{
    struct OrlixHostDirectoryResource resource = { 0 };
    unsigned int index;
    unsigned int target_index;

    if (OrlixHostCopyRequiredOpaqueIdentifier(resource.identifier,
                                              sizeof(resource.identifier),
                                              identifier) != 0 ||
        OrlixHostCopyRequiredDirectoryPath(resource.host_path,
                                           sizeof(resource.host_path),
                                           host_path) != 0) {
        return -1;
    }
    resource.read_only = read_only ? 1U : 0U;

    os_unfair_lock_lock(&OrlixHostDirectoriesLock);
    target_index = OrlixHostDirectoryCount;
    for (index = 0; index < OrlixHostDirectoryCount; index++) {
        if (strcmp(OrlixHostDirectories[index].identifier,
                   resource.identifier) == 0) {
            target_index = index;
            break;
        }
    }
    if (target_index >= ORLIX_HOST_MAX_HOST_DIRECTORIES) {
        os_unfair_lock_unlock(&OrlixHostDirectoriesLock);
        return -1;
    }

    OrlixHostDirectories[target_index] = resource;
    if (target_index == OrlixHostDirectoryCount) {
        OrlixHostDirectoryCount++;
    }
    os_unfair_lock_unlock(&OrlixHostDirectoriesLock);
    return 0;
}

__attribute__((visibility("default"))) int orlix_host_resources_register_host_directory_xattr(
    const char *identifier,
    const char *relative_path,
    const char *name,
    const void *value,
    uint32_t value_length)
{
    struct OrlixHostDirectoryXattrResource resource;
    unsigned int target_index;
    unsigned int index;

    memset(&resource, 0, sizeof(resource));
    if (OrlixHostCopyRequiredOpaqueIdentifier(resource.identifier,
                                              sizeof(resource.identifier),
                                              identifier) != 0 ||
        OrlixHostCopyRequiredRelativePath(resource.relative_path,
                                          sizeof(resource.relative_path),
                                          relative_path) != 0 ||
        OrlixHostCopyRequiredLinuxXattrName(resource.name,
                                            sizeof(resource.name),
                                            name) != 0 ||
        value_length > sizeof(resource.value) ||
        (value_length > 0 && !value)) {
        return -1;
    }

    if (value_length > 0) {
        memcpy(resource.value, value, value_length);
    }
    resource.value_length = value_length;

    os_unfair_lock_lock(&OrlixHostDirectoriesLock);
    if (OrlixHostDirectoryIdentifierIndexLocked(resource.identifier) < 0) {
        os_unfair_lock_unlock(&OrlixHostDirectoriesLock);
        return -1;
    }

    target_index = OrlixHostDirectoryXattrCount;
    for (index = 0; index < OrlixHostDirectoryXattrCount; index++) {
        if (strcmp(OrlixHostDirectoryXattrs[index].identifier,
                   resource.identifier) == 0 &&
            strcmp(OrlixHostDirectoryXattrs[index].relative_path,
                   resource.relative_path) == 0 &&
            strcmp(OrlixHostDirectoryXattrs[index].name,
                   resource.name) == 0) {
            target_index = index;
            break;
        }
    }

    if (target_index >= ORLIX_HOST_MAX_HOST_DIRECTORY_XATTRS) {
        os_unfair_lock_unlock(&OrlixHostDirectoriesLock);
        return -1;
    }

    OrlixHostDirectoryXattrs[target_index] = resource;
    if (target_index == OrlixHostDirectoryXattrCount) {
        OrlixHostDirectoryXattrCount++;
    }
    os_unfair_lock_unlock(&OrlixHostDirectoriesLock);
    return 0;
}

__attribute__((visibility("hidden"))) int OrlixHostCopyHostDirectoryPath(
    const char *identifier,
    char *path,
    unsigned long path_size,
    unsigned int *read_only)
{
    unsigned int index;

    if (!identifier || !path || path_size == 0) {
        return -1;
    }

    os_unfair_lock_lock(&OrlixHostDirectoriesLock);
    for (index = 0; index < OrlixHostDirectoryCount; index++) {
        if (strcmp(OrlixHostDirectories[index].identifier, identifier) == 0) {
            if (OrlixHostCopyRequiredString(
                    path,
                    (size_t)path_size,
                    OrlixHostDirectories[index].host_path) != 0) {
                os_unfair_lock_unlock(&OrlixHostDirectoriesLock);
                return -1;
            }
            if (read_only) {
                *read_only = OrlixHostDirectories[index].read_only;
            }
            os_unfair_lock_unlock(&OrlixHostDirectoriesLock);
            return 0;
        }
    }
    os_unfair_lock_unlock(&OrlixHostDirectoriesLock);
    return -1;
}

__attribute__((visibility("hidden"))) int orlix_host_directory_is_read_only(
    unsigned int directory,
    unsigned int *read_only)
{
    int result = -1;

    if (!read_only) {
        return -1;
    }

    os_unfair_lock_lock(&OrlixHostDirectoriesLock);
    if (directory < OrlixHostDirectoryCount &&
        OrlixHostDirectories[directory].identifier[0] != '\0') {
        *read_only = OrlixHostDirectories[directory].read_only;
        result = 0;
    }
    os_unfair_lock_unlock(&OrlixHostDirectoriesLock);
    return result;
}

static int OrlixHostRegisterRootImage(struct OrlixHostRootImage *root_image)
{
    unsigned int index;
    unsigned int target_index;

    if (!root_image ||
        root_image->base_block_device >= ORLIX_HOST_MAX_BLOCK_DEVICES ||
        root_image->state_block_device >= ORLIX_HOST_MAX_BLOCK_DEVICES ||
        root_image->base_block_device == root_image->state_block_device ||
        root_image->state_block_minimum_bytes == 0) {
        return -1;
    }
    if ((root_image->initrd_bundle_name[0] == '\0') !=
        (root_image->initrd_bundle_extension[0] == '\0')) {
        return -1;
    }
    if (strchr(root_image->initrd_bundle_name, '/') ||
        OrlixHostPathContainsParentReference(root_image->initrd_bundle_name) ||
        strchr(root_image->initrd_bundle_extension, '/') ||
        OrlixHostPathContainsParentReference(
            root_image->initrd_bundle_extension)) {
        return -1;
    }

    os_unfair_lock_lock(&OrlixHostRootImagesLock);
    target_index = OrlixHostRootImageCount;
    for (index = 0; index < OrlixHostRootImageCount; index++) {
        if (strcmp(OrlixHostRootImages[index].identifier,
                   root_image->identifier) == 0) {
            target_index = index;
            break;
        }
    }
    if (target_index == OrlixHostRootImageCount) {
        if (OrlixHostRootImageCount >= ORLIX_HOST_MAX_ROOT_IMAGES) {
            os_unfair_lock_unlock(&OrlixHostRootImagesLock);
            return -1;
        }
        OrlixHostRootImageCount++;
    }
    OrlixHostRootImages[target_index] = *root_image;
    os_unfair_lock_unlock(&OrlixHostRootImagesLock);
    return 0;
}

__attribute__((visibility("default"))) int orlix_host_resources_register_root_image(
    const char *identifier,
    const char *initrd_bundle_name,
    const char *initrd_bundle_extension,
    const char *initrd_resource,
    const char *base_block_resource,
    const char *state_block_resource,
    unsigned int base_block_device,
    unsigned int state_block_device,
    unsigned long long state_block_minimum_bytes)
{
    struct OrlixHostRootImage root_image = { 0 };

    if (OrlixHostCopyRequiredString(root_image.identifier,
                                    sizeof(root_image.identifier),
                                    identifier) != 0 ||
        OrlixHostCopyOptionalString(root_image.initrd_bundle_name,
                                    sizeof(root_image.initrd_bundle_name),
                                    initrd_bundle_name) != 0 ||
        OrlixHostCopyOptionalString(root_image.initrd_bundle_extension,
                                    sizeof(root_image.initrd_bundle_extension),
                                    initrd_bundle_extension) != 0 ||
        OrlixHostCopyRequiredResource(root_image.initrd_resource,
                                      sizeof(root_image.initrd_resource),
                                      initrd_resource) != 0 ||
        OrlixHostCopyRequiredResource(root_image.base_block_resource,
                                      sizeof(root_image.base_block_resource),
                                      base_block_resource) != 0 ||
        OrlixHostCopyRequiredResource(root_image.state_block_resource,
                                      sizeof(root_image.state_block_resource),
                                      state_block_resource) != 0) {
        return -1;
    }
    root_image.base_block_device = base_block_device;
    root_image.state_block_device = state_block_device;
    root_image.state_block_minimum_bytes = state_block_minimum_bytes;
    return OrlixHostRegisterRootImage(&root_image);
}

__attribute__((visibility("default"))) int orlix_host_resources_register_root_image_files(
    const char *identifier,
    const char *initrd_bundle_name,
    const char *initrd_bundle_extension,
    const char *initrd_resource,
    const char *base_block_path,
    const char *state_block_path,
    unsigned int base_block_device,
    unsigned int state_block_device,
    unsigned long long state_block_minimum_bytes)
{
    struct OrlixHostRootImage root_image = { 0 };

    if (OrlixHostCopyRequiredString(root_image.identifier,
                                    sizeof(root_image.identifier),
                                    identifier) != 0 ||
        OrlixHostCopyOptionalString(root_image.initrd_bundle_name,
                                    sizeof(root_image.initrd_bundle_name),
                                    initrd_bundle_name) != 0 ||
        OrlixHostCopyOptionalString(root_image.initrd_bundle_extension,
                                    sizeof(root_image.initrd_bundle_extension),
                                    initrd_bundle_extension) != 0 ||
        OrlixHostCopyRequiredResource(root_image.initrd_resource,
                                      sizeof(root_image.initrd_resource),
                                      initrd_resource) != 0 ||
        OrlixHostCopyRequiredBlockFilePath(root_image.base_block_resource,
                                           sizeof(root_image.base_block_resource),
                                           base_block_path) != 0 ||
        OrlixHostCopyRequiredBlockFilePath(root_image.state_block_resource,
                                           sizeof(root_image.state_block_resource),
                                           state_block_path) != 0) {
        return -1;
    }
    root_image.block_images_are_files = 1;
    root_image.base_block_device = base_block_device;
    root_image.state_block_device = state_block_device;
    root_image.state_block_minimum_bytes = state_block_minimum_bytes;
    return OrlixHostRegisterRootImage(&root_image);
}

static int OrlixHostCopyPayloadRootPath(char *path, size_t path_size)
{
    size_t length;

    if (!path || path_size == 0) {
        return -1;
    }

    os_unfair_lock_lock(&OrlixHostPayloadRootLock);
    length = strlen(OrlixHostPayloadRootPath);
    if (length == 0 || length >= path_size) {
        os_unfair_lock_unlock(&OrlixHostPayloadRootLock);
        return -1;
    }
    memcpy(path, OrlixHostPayloadRootPath, length + 1);
    os_unfair_lock_unlock(&OrlixHostPayloadRootLock);
    return 0;
}

static int OrlixHostCopyPayloadResourcePath(const char *resource,
                                            char *path,
                                            size_t path_size)
{
    char payload_root[PATH_MAX];
    int written;

    if (!resource || resource[0] == '\0' || !path || path_size == 0) {
        return -1;
    }
    if (OrlixHostCopyPayloadRootPath(payload_root, sizeof(payload_root)) != 0) {
        return -1;
    }

    written = snprintf(path, path_size, "%s/%s", payload_root, resource);
    if (written < 0 || (size_t)written >= path_size) {
        return -1;
    }

    return 0;
}

static int OrlixHostCopyRequiredDirectoryPath(char *target,
                                              size_t target_size,
                                              const char *source)
{
    struct stat state;

    if (OrlixHostAbsolutePathContainsParentReference(source) ||
        OrlixHostCopyRequiredString(target, target_size, source) != 0) {
        return -1;
    }

    if (stat(target, &state) != 0 || !S_ISDIR(state.st_mode)) {
        return -1;
    }

    return 0;
}

static int OrlixHostCopyMainBundleResourceRootPath(const char *name,
                                                   const char *extension,
                                                   char *path,
                                                   size_t path_size)
{
    CFBundleRef main_bundle;
    CFStringRef resource_name;
    CFStringRef resource_extension;
    CFURLRef resource_url;
    Boolean ok;

    if (!name || !extension || !path || path_size == 0) {
        return -1;
    }

    main_bundle = CFBundleGetMainBundle();
    if (!main_bundle) {
        return -1;
    }

    resource_name = CFStringCreateWithCString(
        kCFAllocatorDefault,
        name,
        kCFStringEncodingUTF8);
    resource_extension = CFStringCreateWithCString(
        kCFAllocatorDefault,
        extension,
        kCFStringEncodingUTF8);
    if (!resource_name || !resource_extension) {
        if (resource_name) {
            CFRelease(resource_name);
        }
        if (resource_extension) {
            CFRelease(resource_extension);
        }
        return -1;
    }

    resource_url = CFBundleCopyResourceURL(
        main_bundle,
        resource_name,
        resource_extension,
        0);
    CFRelease(resource_name);
    CFRelease(resource_extension);
    if (!resource_url) {
        return -1;
    }

    ok = CFURLGetFileSystemRepresentation(
        resource_url,
        true,
        (UInt8 *)path,
        path_size);
    CFRelease(resource_url);
    return ok ? 0 : -1;
}

static int OrlixHostCopyTestBundleResourcePath(const char *bundle_name,
                                               const char *bundle_extension,
                                               const char *resource,
                                               char *path,
                                               size_t path_size)
{
    char bundle_root[PATH_MAX];
    int written;

    if (!bundle_name || !bundle_extension || !resource || !path ||
        path_size == 0) {
        return -1;
    }
    if (OrlixHostCopyMainBundleResourceRootPath(bundle_name,
                                               bundle_extension,
                                               bundle_root,
                                               sizeof(bundle_root)) != 0) {
        return -1;
    }

    written = snprintf(path, path_size, "%s/%s", bundle_root, resource);
    if (written < 0 || (size_t)written >= path_size) {
        return -1;
    }

    return 0;
}

static int OrlixHostResourceFileSize(const char *path,
                                     unsigned long long *size)
{
    FILE *file;
    long length;

    if (!path || !size) {
        return -1;
    }

    file = fopen(path, "rb");
    if (!file) {
        return -1;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return -1;
    }
    length = ftell(file);
    fclose(file);
    if (length <= 0) {
        return -1;
    }

    *size = (unsigned long long)length;
    return 0;
}

static void OrlixHostClearSelectedBlockImages(void)
{
    memset(OrlixHostSelectedBlockPaths, 0, sizeof(OrlixHostSelectedBlockPaths));
    memset(OrlixHostSelectedBlockBytes, 0, sizeof(OrlixHostSelectedBlockBytes));
    memset(OrlixHostSelectedBlockWritable, 0, sizeof(OrlixHostSelectedBlockWritable));
}

static int OrlixHostCopySelectedBlockPath(unsigned int device, const char *path)
{
    size_t length;

    if (device >= ORLIX_HOST_MAX_BLOCK_DEVICES || !path) {
        return -1;
    }

    length = strlen(path);
    if (length >= PATH_MAX) {
        return -1;
    }

    memcpy(OrlixHostSelectedBlockPaths[device], path, length + 1);
    return 0;
}

static int OrlixHostBlockDeviceIsSelected(unsigned int device)
{
    return device < ORLIX_HOST_MAX_BLOCK_DEVICES &&
           OrlixHostSelectedBlockPaths[device][0] != '\0' &&
           OrlixHostSelectedBlockBytes[device] != 0;
}

static int OrlixHostEnsureDirectory(const char *path)
{
    struct stat state;

    if (!path || path[0] == '\0') {
        return -1;
    }

    if (mkdir(path, 0700) == 0) {
        return 0;
    }
    if (errno != EEXIST) {
        return -1;
    }

    return stat(path, &state) == 0 && S_ISDIR(state.st_mode) ? 0 : -1;
}

static int OrlixHostCopyStateBlockPath(const char *identifier,
                                       char *path,
                                       size_t path_size)
{
    const char *home = getenv("HOME");
    char library[PATH_MAX];
    char application_support[PATH_MAX];
    char orlix[PATH_MAX];
    char state_name[192];
    size_t index;
    size_t out_index = 0;
    int written;

    if (!home || home[0] == '\0' || !identifier ||
        identifier[0] == '\0' || !path || path_size == 0) {
        return -1;
    }

    written = snprintf(library, sizeof(library), "%s/Library", home);
    if (written < 0 || (size_t)written >= sizeof(library)) {
        return -1;
    }
    if (OrlixHostEnsureDirectory(library) != 0) {
        return -1;
    }

    written = snprintf(application_support,
                       sizeof(application_support),
                       "%s/Application Support",
                       library);
    if (written < 0 || (size_t)written >= sizeof(application_support)) {
        return -1;
    }
    if (OrlixHostEnsureDirectory(application_support) != 0) {
        return -1;
    }

    written = snprintf(orlix, sizeof(orlix), "%s/Orlix", application_support);
    if (written < 0 || (size_t)written >= sizeof(orlix)) {
        return -1;
    }
    if (OrlixHostEnsureDirectory(orlix) != 0) {
        return -1;
    }

    for (index = 0; identifier[index] != '\0' &&
                    out_index + 1 < sizeof(state_name);
         index++) {
        char c = identifier[index];
        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_') {
            state_name[out_index++] = c;
        } else {
            state_name[out_index++] = '-';
        }
    }
    state_name[out_index] = '\0';
    if (identifier[index] != '\0' || state_name[0] == '\0') {
        return -1;
    }

    written = snprintf(path, path_size, "%s/%s-state.img", orlix, state_name);
    return written >= 0 && (size_t)written < path_size ? 0 : -1;
}

static int OrlixHostCopyFile(const char *source, const char *target)
{
    unsigned char buffer[16384];
    FILE *input;
    FILE *output;
    size_t count;
    int result = -1;

    if (!source || !target) {
        return -1;
    }

    input = fopen(source, "rb");
    if (!input) {
        return -1;
    }
    output = fopen(target, "wb");
    if (!output) {
        fclose(input);
        return -1;
    }

    while ((count = fread(buffer, 1, sizeof(buffer), input)) > 0) {
        if (fwrite(buffer, 1, count, output) != count) {
            goto out;
        }
    }
    if (ferror(input) || fflush(output) != 0) {
        goto out;
    }

    result = 0;

out:
    if (fclose(output) != 0) {
        result = -1;
    }
    fclose(input);
    return result;
}

static int OrlixHostStateBlockHasExt4Magic(const char *path)
{
    unsigned char magic[2];
    FILE *file;
    size_t count;

    if (!path) {
        return 0;
    }

    file = fopen(path, "rb");
    if (!file) {
        return 0;
    }
    if (fseeko(file, 1080, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }

    count = fread(magic, 1, sizeof(magic), file);
    fclose(file);

    return count == sizeof(magic) && magic[0] == 0x53 && magic[1] == 0xef;
}

static int OrlixHostEnsureStateBlockFile(const char *path,
                                         const char *template_path,
                                         unsigned long long minimum_bytes,
                                         unsigned long long *size)
{
    unsigned long long template_size;
    unsigned long long target_size;
    struct stat state;
    int fd;

    if (!path || !template_path || minimum_bytes == 0 || !size) {
        return -1;
    }
    if (OrlixHostResourceFileSize(template_path, &template_size) != 0 ||
        template_size == 0) {
        return -1;
    }

    if (!OrlixHostStateBlockHasExt4Magic(path) &&
        OrlixHostCopyFile(template_path, path) != 0) {
        return -1;
    }

    fd = open(path, O_RDWR | O_CREAT, 0600);
    if (fd < 0) {
        return -1;
    }
    if (fstat(fd, &state) != 0 ||
        !S_ISREG(state.st_mode) ||
        state.st_size < 0) {
        close(fd);
        return -1;
    }

    target_size = (unsigned long long)state.st_size;
    if (target_size < minimum_bytes) {
        target_size = minimum_bytes;
    }
    if (target_size < template_size) {
        target_size = template_size;
    }
    if (target_size % ORLIX_HOST_BLOCK_SECTOR_SIZE) {
        target_size = ((target_size + ORLIX_HOST_BLOCK_SECTOR_SIZE - 1) /
                       ORLIX_HOST_BLOCK_SECTOR_SIZE) *
                      ORLIX_HOST_BLOCK_SECTOR_SIZE;
    }
    if (target_size > (unsigned long long)LLONG_MAX ||
        (unsigned long long)state.st_size != target_size) {
        if (target_size > (unsigned long long)LLONG_MAX ||
            ftruncate(fd, (off_t)target_size) != 0) {
            close(fd);
            return -1;
        }
    }

    close(fd);
    *size = target_size;
    return target_size ? 0 : -1;
}

static int OrlixHostEnsureExistingStateBlockFile(const char *path,
                                                 unsigned long long minimum_bytes,
                                                 unsigned long long *size)
{
    unsigned long long target_size;
    struct stat state;
    int fd;

    if (!path || minimum_bytes == 0 || !size) {
        return -1;
    }

    fd = open(path, O_RDWR);
    if (fd < 0) {
        return -1;
    }
    if (fstat(fd, &state) != 0 ||
        !S_ISREG(state.st_mode) ||
        state.st_size < 0) {
        close(fd);
        return -1;
    }

    target_size = (unsigned long long)state.st_size;
    if (target_size < minimum_bytes) {
        target_size = minimum_bytes;
    }
    if (target_size % ORLIX_HOST_BLOCK_SECTOR_SIZE) {
        target_size = ((target_size + ORLIX_HOST_BLOCK_SECTOR_SIZE - 1) /
                       ORLIX_HOST_BLOCK_SECTOR_SIZE) *
                      ORLIX_HOST_BLOCK_SECTOR_SIZE;
    }
    if (target_size > (unsigned long long)LLONG_MAX ||
        (unsigned long long)state.st_size != target_size) {
        if (target_size > (unsigned long long)LLONG_MAX ||
            ftruncate(fd, (off_t)target_size) != 0) {
            close(fd);
            return -1;
        }
    }

    close(fd);
    *size = target_size;
    return target_size ? 0 : -1;
}

static int OrlixHostReadResourceFile(const char *path,
                                     struct OrlixHostResource *resource)
{
    FILE *file;
    long length;
    void *data;
    size_t read_count;

    if (!path || !resource) {
        return -1;
    }

    file = fopen(path, "rb");
    if (!file) {
        return -1;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return -1;
    }
    length = ftell(file);
    if (length <= 0) {
        fclose(file);
        return -1;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return -1;
    }

    data = malloc((size_t)length);
    if (!data) {
        fclose(file);
        return -1;
    }

    read_count = fread(data, 1, (size_t)length, file);
    fclose(file);
    if (read_count != (size_t)length) {
        free(data);
        return -1;
    }

    resource->data = data;
    resource->size = (unsigned long)length;
    return 0;
}

__attribute__((visibility("hidden"))) int OrlixHostLoadKernelPayloadResource(
    const char *resource,
    struct OrlixHostResource *loaded)
{
    char path[PATH_MAX];

    if (!loaded) {
        return -1;
    }

    loaded->data = 0;
    loaded->size = 0;
    if (OrlixHostCopyPayloadResourcePath(resource, path, sizeof(path)) != 0) {
        return -1;
    }

    return OrlixHostReadResourceFile(path, loaded);
}

__attribute__((visibility("hidden"))) int OrlixHostLoadInitrdResource(
    const char *identifier,
    struct OrlixHostResource *loaded)
{
    struct OrlixHostRootImage root_image;
    char path[PATH_MAX];

    if (!loaded ||
        OrlixHostCopyRootImageForIdentifier(identifier, &root_image) != 0) {
        return -1;
    }

    loaded->data = 0;
    loaded->size = 0;
    if (root_image.initrd_bundle_name[0] != '\0') {
        if (OrlixHostCopyTestBundleResourcePath(root_image.initrd_bundle_name,
                                                root_image.initrd_bundle_extension,
                                                root_image.initrd_resource,
                                                path,
                                                sizeof(path)) != 0) {
            return -1;
        }
        return OrlixHostReadResourceFile(path, loaded);
    }

    if (OrlixHostCopyPayloadResourcePath(root_image.initrd_resource,
                                         path,
                                         sizeof(path)) != 0) {
        return -1;
    }
    return OrlixHostReadResourceFile(path, loaded);
}

__attribute__((visibility("hidden"))) int OrlixHostSelectBootBlockImages(
    const char *identifier)
{
    struct OrlixHostRootImage root_image;
    unsigned long long base_size = 0;
    unsigned long long state_size = 0;
    char base_path[PATH_MAX];
    char state_template_path[PATH_MAX];
    char state_path[PATH_MAX];

    OrlixHostClearSelectedBlockImages();

    if (OrlixHostCopyRootImageForIdentifier(identifier, &root_image) != 0) {
        return -1;
    }
    if (root_image.block_images_are_files) {
        if (OrlixHostCopySelectedBlockPath(root_image.base_block_device,
                                           root_image.base_block_resource) != 0 ||
            OrlixHostCopySelectedBlockPath(root_image.state_block_device,
                                           root_image.state_block_resource) != 0 ||
            OrlixHostResourceFileSize(root_image.base_block_resource,
                                      &base_size) != 0 ||
            OrlixHostEnsureExistingStateBlockFile(
                root_image.state_block_resource,
                root_image.state_block_minimum_bytes,
                &state_size) != 0) {
            OrlixHostClearSelectedBlockImages();
            return -1;
        }
        OrlixHostSelectedBlockBytes[root_image.base_block_device] = base_size;
        OrlixHostSelectedBlockBytes[root_image.state_block_device] = state_size;
        OrlixHostSelectedBlockWritable[root_image.state_block_device] = 1;
        return 0;
    }
    if (OrlixHostCopyPayloadResourcePath(root_image.base_block_resource,
                                         base_path,
                                         sizeof(base_path)) != 0 ||
        OrlixHostCopyPayloadResourcePath(root_image.state_block_resource,
                                         state_template_path,
                                         sizeof(state_template_path)) != 0) {
        return -1;
    }
    if (OrlixHostResourceFileSize(base_path, &base_size) != 0) {
        return -1;
    }
    if (OrlixHostCopyStateBlockPath(identifier, state_path, sizeof(state_path)) != 0) {
        return -1;
    }
    if (OrlixHostEnsureStateBlockFile(state_path,
                                      state_template_path,
                                      root_image.state_block_minimum_bytes,
                                      &state_size) != 0) {
        return -1;
    }
    if (OrlixHostCopySelectedBlockPath(root_image.base_block_device,
                                       base_path) != 0 ||
        OrlixHostCopySelectedBlockPath(root_image.state_block_device,
                                       state_path) != 0) {
        OrlixHostClearSelectedBlockImages();
        return -1;
    }

    OrlixHostSelectedBlockBytes[root_image.base_block_device] = base_size;
    OrlixHostSelectedBlockBytes[root_image.state_block_device] = state_size;
    OrlixHostSelectedBlockWritable[root_image.state_block_device] = 1;
    return 0;
}

__attribute__((visibility("hidden"))) int orlix_host_directory_read_entry(
    unsigned int directory,
    unsigned int entry_index,
    struct OrlixHostDirectoryEntry *entry)
{
    char directory_path[PATH_MAX];
    DIR *stream;
    struct dirent *dirent;
    unsigned int visible_index = 0;
    unsigned long active_tls;
    int result = -1;

    if (!entry || directory >= ORLIX_HOST_MAX_HOST_DIRECTORIES)
        return -1;

    os_unfair_lock_lock(&OrlixHostDirectoriesLock);
    if (directory >= OrlixHostDirectoryCount ||
        OrlixHostDirectories[directory].host_path[0] == '\0') {
        os_unfair_lock_unlock(&OrlixHostDirectoriesLock);
        return -1;
    }
    strlcpy(directory_path,
            OrlixHostDirectories[directory].host_path,
            sizeof(directory_path));
    os_unfair_lock_unlock(&OrlixHostDirectoriesLock);

    active_tls = OrlixHostEnterHostTls();
    stream = opendir(directory_path);
    if (!stream) {
        OrlixHostLeaveHostTls(active_tls);
        return -1;
    }

    while ((dirent = readdir(stream)) != NULL) {
        char entry_path[PATH_MAX];
        struct stat status;
        size_t name_length;

        if (strcmp(dirent->d_name, ".") == 0 ||
            strcmp(dirent->d_name, "..") == 0)
            continue;

        if (visible_index++ != entry_index)
            continue;

        name_length = strlen(dirent->d_name);
        if (name_length == 0 || name_length > ORLIX_HOST_DIRECTORY_NAME_MAX)
            break;

        if (snprintf(entry_path, sizeof(entry_path), "%s/%s",
                     directory_path, dirent->d_name) >= (int)sizeof(entry_path))
            break;

        if (lstat(entry_path, &status) != 0)
            break;

        memset(entry, 0, sizeof(*entry));
        entry->inode = status.st_ino;
        entry->size = (uint64_t)status.st_size;
        entry->mode = (uint32_t)(status.st_mode & 07777);
        if (S_ISDIR(status.st_mode))
            entry->type = OrlixHostDirectoryEntryDirectory;
        else if (S_ISREG(status.st_mode))
            entry->type = OrlixHostDirectoryEntryRegular;
        else if (S_ISLNK(status.st_mode))
            entry->type = OrlixHostDirectoryEntrySymlink;
        else
            entry->type = OrlixHostDirectoryEntryUnknown;
        strlcpy(entry->name, dirent->d_name, sizeof(entry->name));
        result = 0;
        break;
    }

    closedir(stream);
    OrlixHostLeaveHostTls(active_tls);
    return result;
}

__attribute__((visibility("hidden"))) int orlix_host_directory_read_entry_at_path(
    unsigned int directory,
    const char *relative_path,
    struct OrlixHostDirectoryEntry *entry)
{
    char entry_path[PATH_MAX];
    const char *name;

    if (OrlixHostCopyDirectoryRelativeEntryPath(
            directory, relative_path, entry_path, sizeof(entry_path)) != 0)
        return -1;

    name = strrchr(relative_path, '/');
    if (name)
        name++;
    else
        name = relative_path;
    if (!name || strcmp(relative_path, ".") == 0)
        name = ".";

    return OrlixHostFillDirectoryEntryFromPath(entry_path, name, entry);
}

__attribute__((visibility("hidden"))) int
orlix_host_directory_read_directory_entry_at_path(
    unsigned int directory,
    const char *relative_path,
    unsigned int entry_index,
    struct OrlixHostDirectoryEntry *entry)
{
    char directory_path[PATH_MAX];
    char entry_path[PATH_MAX];
    DIR *stream;
    struct dirent *dirent;
    unsigned int visible_index = 0;
    unsigned long active_tls;
    int result = -1;

    if (OrlixHostCopyDirectoryRelativeEntryPath(
            directory, relative_path, directory_path, sizeof(directory_path)) !=
        0)
        return -1;

    active_tls = OrlixHostEnterHostTls();
    stream = opendir(directory_path);
    if (!stream) {
        OrlixHostLeaveHostTls(active_tls);
        return -1;
    }

    while ((dirent = readdir(stream)) != NULL) {
        if (strcmp(dirent->d_name, ".") == 0 ||
            strcmp(dirent->d_name, "..") == 0)
            continue;
        if (visible_index++ != entry_index)
            continue;
        if (snprintf(entry_path, sizeof(entry_path), "%s/%s", directory_path,
                     dirent->d_name) < (int)sizeof(entry_path))
            result = OrlixHostFillDirectoryEntryFromPath(entry_path,
                                                         dirent->d_name, entry);
        break;
    }

    closedir(stream);
    OrlixHostLeaveHostTls(active_tls);
    return result;
}

__attribute__((visibility("hidden"))) long orlix_host_directory_read_file(
    unsigned int directory,
    unsigned int entry_index,
    uint64_t offset,
    void *buffer,
    uint32_t length)
{
    char directory_path[PATH_MAX];
    char entry_path[PATH_MAX];
    struct OrlixHostDirectoryEntry entry;
    struct stat status;
    FILE *file;
    unsigned long active_tls;
    size_t read_count;
    long result = -1;

    if (!buffer || length == 0)
        return 0;

    if (orlix_host_directory_read_entry(directory, entry_index, &entry) != 0 ||
        entry.type != OrlixHostDirectoryEntryRegular)
        return -1;

    os_unfair_lock_lock(&OrlixHostDirectoriesLock);
    if (directory >= OrlixHostDirectoryCount ||
        OrlixHostDirectories[directory].host_path[0] == '\0') {
        os_unfair_lock_unlock(&OrlixHostDirectoriesLock);
        return -1;
    }
    strlcpy(directory_path,
            OrlixHostDirectories[directory].host_path,
            sizeof(directory_path));
    os_unfair_lock_unlock(&OrlixHostDirectoriesLock);

    if (snprintf(entry_path, sizeof(entry_path), "%s/%s",
                 directory_path, entry.name) >= (int)sizeof(entry_path))
        return -1;

    active_tls = OrlixHostEnterHostTls();
    if (lstat(entry_path, &status) != 0 ||
        !S_ISREG(status.st_mode)) {
        OrlixHostLeaveHostTls(active_tls);
        return -1;
    }

    file = fopen(entry_path, "rb");
    if (!file) {
        OrlixHostLeaveHostTls(active_tls);
        return -1;
    }

    if (fseeko(file, (off_t)offset, SEEK_SET) == 0) {
        read_count = fread(buffer, 1, length, file);
        if (read_count > 0 || feof(file))
            result = (long)read_count;
    }

    fclose(file);
    OrlixHostLeaveHostTls(active_tls);
    return result;
}

__attribute__((visibility("hidden"))) int orlix_host_directory_read_child_entry(
    unsigned int directory,
    unsigned int parent_entry_index,
    unsigned int entry_index,
    struct OrlixHostDirectoryEntry *entry)
{
    char directory_path[PATH_MAX];
    char parent_path[PATH_MAX];
    DIR *stream;
    struct dirent *dirent;
    struct OrlixHostDirectoryEntry parent_entry;
    unsigned int visible_index = 0;
    unsigned long active_tls;
    int result = -1;

    if (!entry || directory >= ORLIX_HOST_MAX_HOST_DIRECTORIES)
        return -1;

    if (orlix_host_directory_read_entry(directory, parent_entry_index,
                                       &parent_entry) != 0 ||
        parent_entry.type != OrlixHostDirectoryEntryDirectory)
        return -1;

    os_unfair_lock_lock(&OrlixHostDirectoriesLock);
    if (directory >= OrlixHostDirectoryCount ||
        OrlixHostDirectories[directory].host_path[0] == '\0') {
        os_unfair_lock_unlock(&OrlixHostDirectoriesLock);
        return -1;
    }
    strlcpy(directory_path,
            OrlixHostDirectories[directory].host_path,
            sizeof(directory_path));
    os_unfair_lock_unlock(&OrlixHostDirectoriesLock);

    if (snprintf(parent_path, sizeof(parent_path), "%s/%s",
                 directory_path, parent_entry.name) >= (int)sizeof(parent_path))
        return -1;

    active_tls = OrlixHostEnterHostTls();
    stream = opendir(parent_path);
    if (!stream) {
        OrlixHostLeaveHostTls(active_tls);
        return -1;
    }

    while ((dirent = readdir(stream)) != NULL) {
        char entry_path[PATH_MAX];
        struct stat status;
        size_t name_length;

        if (strcmp(dirent->d_name, ".") == 0 ||
            strcmp(dirent->d_name, "..") == 0)
            continue;

        if (visible_index++ != entry_index)
            continue;

        name_length = strlen(dirent->d_name);
        if (name_length == 0 || name_length > ORLIX_HOST_DIRECTORY_NAME_MAX)
            break;

        if (snprintf(entry_path, sizeof(entry_path), "%s/%s",
                     parent_path, dirent->d_name) >= (int)sizeof(entry_path))
            break;

        if (lstat(entry_path, &status) != 0)
            break;

        memset(entry, 0, sizeof(*entry));
        entry->inode = status.st_ino;
        entry->size = (uint64_t)status.st_size;
        entry->mode = (uint32_t)(status.st_mode & 07777);
        if (S_ISDIR(status.st_mode))
            entry->type = OrlixHostDirectoryEntryDirectory;
        else if (S_ISREG(status.st_mode))
            entry->type = OrlixHostDirectoryEntryRegular;
        else if (S_ISLNK(status.st_mode))
            entry->type = OrlixHostDirectoryEntrySymlink;
        else
            entry->type = OrlixHostDirectoryEntryUnknown;
        strlcpy(entry->name, dirent->d_name, sizeof(entry->name));
        result = 0;
        break;
    }

    closedir(stream);
    OrlixHostLeaveHostTls(active_tls);
    return result;
}

__attribute__((visibility("hidden"))) long orlix_host_directory_read_file_at_path(
    unsigned int directory,
    const char *relative_path,
    uint64_t offset,
    void *buffer,
    uint32_t length)
{
    char entry_path[PATH_MAX];
    FILE *file;
    unsigned long active_tls;
    size_t read_count;
    long result = -1;

    if (!buffer ||
        OrlixHostCopyDirectoryRelativeEntryPath(
            directory, relative_path, entry_path, sizeof(entry_path)) != 0)
        return -1;

    active_tls = OrlixHostEnterHostTls();
    file = fopen(entry_path, "rb");
    if (!file) {
        OrlixHostLeaveHostTls(active_tls);
        return -1;
    }

    if (fseeko(file, (off_t)offset, SEEK_SET) == 0) {
        read_count = fread(buffer, 1, length, file);
        if (read_count > 0 || feof(file))
            result = (long)read_count;
    }

    fclose(file);
    OrlixHostLeaveHostTls(active_tls);
    return result;
}

__attribute__((visibility("hidden"))) int orlix_host_directory_create_file_at_path(
    unsigned int directory,
    const char *relative_path,
    uint32_t mode)
{
    char entry_path[PATH_MAX];
    unsigned int read_only = 1;
    unsigned long active_tls;
    int fd;
    int result = -1;

    if (orlix_host_directory_is_read_only(directory, &read_only) != 0) {
        return -1;
    }
    if (read_only) {
        return -2;
    }
    if (OrlixHostCopyDirectoryRelativeEntryPath(
            directory, relative_path, entry_path, sizeof(entry_path)) != 0) {
        return -1;
    }

    active_tls = OrlixHostEnterHostTls();
    fd = open(entry_path, O_WRONLY | O_CREAT | O_EXCL, mode & 0777);
    if (fd >= 0) {
        result = close(fd) == 0 ? 0 : -1;
    }
    OrlixHostLeaveHostTls(active_tls);
    return result;
}

__attribute__((visibility("hidden"))) long orlix_host_directory_write_file_at_path(
    unsigned int directory,
    const char *relative_path,
    uint64_t offset,
    const void *buffer,
    uint32_t length)
{
    char entry_path[PATH_MAX];
    unsigned int read_only = 1;
    unsigned long active_tls;
    FILE *file;
    size_t write_count;
    long result = -1;

    if (!buffer) {
        return -1;
    }
    if (length == 0) {
        return 0;
    }
    if (orlix_host_directory_is_read_only(directory, &read_only) != 0) {
        return -1;
    }
    if (read_only) {
        return -2;
    }
    if (offset > (uint64_t)LLONG_MAX ||
        OrlixHostCopyDirectoryRelativeEntryPath(
            directory, relative_path, entry_path, sizeof(entry_path)) != 0) {
        return -1;
    }

    active_tls = OrlixHostEnterHostTls();
    file = fopen(entry_path, "r+b");
    if (!file) {
        goto out;
    }
    if (fseeko(file, (off_t)offset, SEEK_SET) != 0) {
        fclose(file);
        goto out;
    }
    write_count = fwrite(buffer, 1, length, file);
    if (fclose(file) != 0) {
        goto out;
    }
    if (write_count == length) {
        result = (long)write_count;
    }

out:
    OrlixHostLeaveHostTls(active_tls);
    return result;
}

__attribute__((visibility("hidden"))) int orlix_host_directory_truncate_file_at_path(
    unsigned int directory,
    const char *relative_path,
    uint64_t size)
{
    char entry_path[PATH_MAX];
    unsigned int read_only = 1;
    unsigned long active_tls;
    int result;

    if (orlix_host_directory_is_read_only(directory, &read_only) != 0) {
        return -1;
    }
    if (read_only) {
        return -2;
    }
    if (size > (uint64_t)LLONG_MAX ||
        OrlixHostCopyDirectoryRelativeEntryPath(
            directory, relative_path, entry_path, sizeof(entry_path)) != 0) {
        return -1;
    }

    active_tls = OrlixHostEnterHostTls();
    result = truncate(entry_path, (off_t)size) == 0 ? 0 : -1;
    OrlixHostLeaveHostTls(active_tls);
    return result;
}

__attribute__((visibility("hidden"))) long orlix_host_directory_read_link_at_path(
    unsigned int directory,
    const char *relative_path,
    void *buffer,
    uint32_t length)
{
    char entry_path[PATH_MAX];
    unsigned long active_tls;
    ssize_t read_count;

    if (!buffer ||
        OrlixHostCopyDirectoryRelativeEntryPath(
            directory, relative_path, entry_path, sizeof(entry_path)) != 0)
        return -1;

    active_tls = OrlixHostEnterHostTls();
    read_count = readlink(entry_path, buffer, length);
    OrlixHostLeaveHostTls(active_tls);

    return read_count >= 0 ? (long)read_count : -1;
}

__attribute__((visibility("hidden"))) long orlix_host_directory_read_link(
    unsigned int directory,
    unsigned int entry_index,
    void *buffer,
    uint32_t length)
{
    char directory_path[PATH_MAX];
    char entry_path[PATH_MAX];
    struct OrlixHostDirectoryEntry entry;
    struct stat status;
    unsigned long active_tls;
    ssize_t read_count;

    if (!buffer || length == 0)
        return 0;

    if (orlix_host_directory_read_entry(directory, entry_index, &entry) != 0 ||
        entry.type != OrlixHostDirectoryEntrySymlink)
        return -1;

    os_unfair_lock_lock(&OrlixHostDirectoriesLock);
    if (directory >= OrlixHostDirectoryCount ||
        OrlixHostDirectories[directory].host_path[0] == '\0') {
        os_unfair_lock_unlock(&OrlixHostDirectoriesLock);
        return -1;
    }
    strlcpy(directory_path,
            OrlixHostDirectories[directory].host_path,
            sizeof(directory_path));
    os_unfair_lock_unlock(&OrlixHostDirectoriesLock);

    if (snprintf(entry_path, sizeof(entry_path), "%s/%s",
                 directory_path, entry.name) >= (int)sizeof(entry_path))
        return -1;

    active_tls = OrlixHostEnterHostTls();
    if (lstat(entry_path, &status) != 0 ||
        !S_ISLNK(status.st_mode)) {
        OrlixHostLeaveHostTls(active_tls);
        return -1;
    }

    read_count = readlink(entry_path, buffer, length);
    OrlixHostLeaveHostTls(active_tls);
    return read_count >= 0 ? (long)read_count : -1;
}

__attribute__((visibility("hidden"))) long orlix_host_directory_list_xattr(
    unsigned int directory,
    const char *relative_path,
    char *buffer,
    uint64_t capacity)
{
    char checked_relative_path[PATH_MAX];
    uint64_t required = 0;
    unsigned int index;

    if (OrlixHostCopyRequiredRelativePath(checked_relative_path,
                                          sizeof(checked_relative_path),
                                          relative_path) != 0) {
        return -1;
    }

    os_unfair_lock_lock(&OrlixHostDirectoriesLock);
    if (directory >= OrlixHostDirectoryCount ||
        OrlixHostDirectories[directory].identifier[0] == '\0') {
        os_unfair_lock_unlock(&OrlixHostDirectoriesLock);
        return -1;
    }

    for (index = 0; index < OrlixHostDirectoryXattrCount; index++) {
        const struct OrlixHostDirectoryXattrResource *xattr =
            &OrlixHostDirectoryXattrs[index];

        if (strcmp(xattr->identifier,
                   OrlixHostDirectories[directory].identifier) != 0 ||
            strcmp(xattr->relative_path, checked_relative_path) != 0) {
            continue;
        }

        required += strlen(xattr->name) + 1;
    }

    if (buffer && capacity > 0) {
        uint64_t offset = 0;

        if (capacity < required) {
            os_unfair_lock_unlock(&OrlixHostDirectoriesLock);
            return -2;
        }

        for (index = 0; index < OrlixHostDirectoryXattrCount; index++) {
            const struct OrlixHostDirectoryXattrResource *xattr =
                &OrlixHostDirectoryXattrs[index];
            size_t name_length;

            if (strcmp(xattr->identifier,
                       OrlixHostDirectories[directory].identifier) != 0 ||
                strcmp(xattr->relative_path, checked_relative_path) != 0) {
                continue;
            }

            name_length = strlen(xattr->name) + 1;
            memcpy(buffer + offset, xattr->name, name_length);
            offset += name_length;
        }
    }

    os_unfair_lock_unlock(&OrlixHostDirectoriesLock);
    return (long)required;
}

__attribute__((visibility("hidden"))) long orlix_host_directory_read_xattr(
    unsigned int directory,
    const char *relative_path,
    const char *name,
    void *buffer,
    uint64_t capacity)
{
    char checked_relative_path[PATH_MAX];
    char checked_name[ORLIX_HOST_DIRECTORY_XATTR_NAME_MAX + 1];
    unsigned int index;

    if (OrlixHostCopyRequiredRelativePath(checked_relative_path,
                                          sizeof(checked_relative_path),
                                          relative_path) != 0 ||
        OrlixHostCopyRequiredLinuxXattrName(checked_name,
                                            sizeof(checked_name),
                                            name) != 0) {
        return -1;
    }

    os_unfair_lock_lock(&OrlixHostDirectoriesLock);
    if (directory >= OrlixHostDirectoryCount ||
        OrlixHostDirectories[directory].identifier[0] == '\0') {
        os_unfair_lock_unlock(&OrlixHostDirectoriesLock);
        return -1;
    }

    for (index = 0; index < OrlixHostDirectoryXattrCount; index++) {
        const struct OrlixHostDirectoryXattrResource *xattr =
            &OrlixHostDirectoryXattrs[index];

        if (strcmp(xattr->identifier,
                   OrlixHostDirectories[directory].identifier) != 0 ||
            strcmp(xattr->relative_path, checked_relative_path) != 0 ||
            strcmp(xattr->name, checked_name) != 0) {
            continue;
        }

        if (buffer && capacity > 0) {
            if (capacity < xattr->value_length) {
                os_unfair_lock_unlock(&OrlixHostDirectoriesLock);
                return -2;
            }
            memcpy(buffer, xattr->value, xattr->value_length);
        }

        os_unfair_lock_unlock(&OrlixHostDirectoriesLock);
        return (long)xattr->value_length;
    }

    os_unfair_lock_unlock(&OrlixHostDirectoriesLock);
    return -1;
}

__attribute__((visibility("hidden"))) long orlix_host_directory_read_child_file(
    unsigned int directory,
    unsigned int parent_entry_index,
    unsigned int entry_index,
    uint64_t offset,
    void *buffer,
    uint32_t length)
{
    char directory_path[PATH_MAX];
    char entry_path[PATH_MAX];
    struct OrlixHostDirectoryEntry parent_entry;
    struct OrlixHostDirectoryEntry entry;
    struct stat status;
    FILE *file;
    unsigned long active_tls;
    size_t read_count;
    long result = -1;

    if (!buffer || length == 0)
        return 0;

    if (orlix_host_directory_read_entry(directory, parent_entry_index,
                                       &parent_entry) != 0 ||
        parent_entry.type != OrlixHostDirectoryEntryDirectory ||
        orlix_host_directory_read_child_entry(directory, parent_entry_index,
                                             entry_index, &entry) != 0 ||
        entry.type != OrlixHostDirectoryEntryRegular)
        return -1;

    os_unfair_lock_lock(&OrlixHostDirectoriesLock);
    if (directory >= OrlixHostDirectoryCount ||
        OrlixHostDirectories[directory].host_path[0] == '\0') {
        os_unfair_lock_unlock(&OrlixHostDirectoriesLock);
        return -1;
    }
    strlcpy(directory_path,
            OrlixHostDirectories[directory].host_path,
            sizeof(directory_path));
    os_unfair_lock_unlock(&OrlixHostDirectoriesLock);

    if (snprintf(entry_path, sizeof(entry_path), "%s/%s/%s",
                 directory_path, parent_entry.name, entry.name) >=
        (int)sizeof(entry_path))
        return -1;

    active_tls = OrlixHostEnterHostTls();
    if (lstat(entry_path, &status) != 0 ||
        !S_ISREG(status.st_mode)) {
        OrlixHostLeaveHostTls(active_tls);
        return -1;
    }

    file = fopen(entry_path, "rb");
    if (!file) {
        OrlixHostLeaveHostTls(active_tls);
        return -1;
    }

    if (fseeko(file, (off_t)offset, SEEK_SET) == 0) {
        read_count = fread(buffer, 1, length, file);
        if (read_count > 0 || feof(file))
            result = (long)read_count;
    }

    fclose(file);
    OrlixHostLeaveHostTls(active_tls);
    return result;
}

__attribute__((visibility("hidden"))) long orlix_host_directory_read_child_link(
    unsigned int directory,
    unsigned int parent_entry_index,
    unsigned int entry_index,
    void *buffer,
    uint32_t length)
{
    char directory_path[PATH_MAX];
    char entry_path[PATH_MAX];
    struct OrlixHostDirectoryEntry parent_entry;
    struct OrlixHostDirectoryEntry entry;
    struct stat status;
    unsigned long active_tls;
    ssize_t read_count;

    if (!buffer || length == 0)
        return 0;

    if (orlix_host_directory_read_entry(directory, parent_entry_index,
                                       &parent_entry) != 0 ||
        parent_entry.type != OrlixHostDirectoryEntryDirectory ||
        orlix_host_directory_read_child_entry(directory, parent_entry_index,
                                             entry_index, &entry) != 0 ||
        entry.type != OrlixHostDirectoryEntrySymlink)
        return -1;

    os_unfair_lock_lock(&OrlixHostDirectoriesLock);
    if (directory >= OrlixHostDirectoryCount ||
        OrlixHostDirectories[directory].host_path[0] == '\0') {
        os_unfair_lock_unlock(&OrlixHostDirectoriesLock);
        return -1;
    }
    strlcpy(directory_path,
            OrlixHostDirectories[directory].host_path,
            sizeof(directory_path));
    os_unfair_lock_unlock(&OrlixHostDirectoriesLock);

    if (snprintf(entry_path, sizeof(entry_path), "%s/%s/%s",
                 directory_path, parent_entry.name, entry.name) >=
        (int)sizeof(entry_path))
        return -1;

    active_tls = OrlixHostEnterHostTls();
    if (lstat(entry_path, &status) != 0 ||
        !S_ISLNK(status.st_mode)) {
        OrlixHostLeaveHostTls(active_tls);
        return -1;
    }

    read_count = readlink(entry_path, buffer, length);
    OrlixHostLeaveHostTls(active_tls);
    return read_count >= 0 ? (long)read_count : -1;
}

__attribute__((visibility("hidden"))) int orlix_host_block_capacity(
    unsigned int device,
    unsigned long long *sectors)
{
    if (!sectors || !OrlixHostBlockDeviceIsSelected(device)) {
        return -1;
    }

    *sectors = (OrlixHostSelectedBlockBytes[device] +
                ORLIX_HOST_BLOCK_SECTOR_SIZE - 1) /
               ORLIX_HOST_BLOCK_SECTOR_SIZE;
    return *sectors ? 0 : -1;
}

__attribute__((visibility("hidden"))) int orlix_host_block_read(
    unsigned int device,
    unsigned long long sector,
    void *buffer,
    unsigned int length)
{
    FILE *file;
    unsigned long long offset;
    unsigned long long capacity_bytes;
    unsigned long long available;
    unsigned int file_read_length;
    size_t read_count;
    unsigned long active_tls;
    int result = -1;

    if (!OrlixHostBlockDeviceIsSelected(device) || !buffer || !length ||
        sector > ULLONG_MAX / ORLIX_HOST_BLOCK_SECTOR_SIZE) {
        return -1;
    }

    offset = sector * ORLIX_HOST_BLOCK_SECTOR_SIZE;
    capacity_bytes = ((OrlixHostSelectedBlockBytes[device] +
                       ORLIX_HOST_BLOCK_SECTOR_SIZE - 1) /
                      ORLIX_HOST_BLOCK_SECTOR_SIZE) *
                     ORLIX_HOST_BLOCK_SECTOR_SIZE;
    if (offset > capacity_bytes ||
        length > capacity_bytes - offset ||
        offset > (unsigned long long)LLONG_MAX) {
        return -1;
    }

    active_tls = OrlixHostEnterHostTls();
    memset(buffer, 0, length);
    if (offset >= OrlixHostSelectedBlockBytes[device]) {
        result = 0;
        goto out;
    }

    available = OrlixHostSelectedBlockBytes[device] - offset;
    file_read_length = length;
    if (available < file_read_length) {
        file_read_length = (unsigned int)available;
    }
    file = fopen(OrlixHostSelectedBlockPaths[device], "rb");
    if (!file) {
        goto out;
    }
    if (fseeko(file, (off_t)offset, SEEK_SET) != 0) {
        fclose(file);
        goto out;
    }

    read_count = fread(buffer, 1, file_read_length, file);
    fclose(file);
    result = read_count == file_read_length ? 0 : -1;

out:
    OrlixHostLeaveHostTls(active_tls);
    return result;
}

__attribute__((visibility("hidden"))) int orlix_host_block_write(
    unsigned int device,
    unsigned long long sector,
    const void *buffer,
    unsigned int length)
{
    FILE *file;
    unsigned long long offset;
    unsigned long long capacity_bytes;
    size_t write_count;
    unsigned long active_tls;
    int result = -1;

    if (!OrlixHostBlockDeviceIsSelected(device) ||
        !OrlixHostSelectedBlockWritable[device] ||
        !buffer || !length ||
        sector > ULLONG_MAX / ORLIX_HOST_BLOCK_SECTOR_SIZE) {
        return -1;
    }

    offset = sector * ORLIX_HOST_BLOCK_SECTOR_SIZE;
    capacity_bytes = ((OrlixHostSelectedBlockBytes[device] +
                       ORLIX_HOST_BLOCK_SECTOR_SIZE - 1) /
                      ORLIX_HOST_BLOCK_SECTOR_SIZE) *
                     ORLIX_HOST_BLOCK_SECTOR_SIZE;
    if (offset > capacity_bytes ||
        length > capacity_bytes - offset ||
        offset > (unsigned long long)LLONG_MAX) {
        return -1;
    }

    active_tls = OrlixHostEnterHostTls();
    file = fopen(OrlixHostSelectedBlockPaths[device], "r+b");
    if (!file) {
        goto out;
    }
    if (fseeko(file, (off_t)offset, SEEK_SET) != 0) {
        fclose(file);
        goto out;
    }

    write_count = fwrite(buffer, 1, length, file);
    if (write_count != length) {
        fclose(file);
        goto out;
    }

    result = fclose(file) == 0 ? 0 : -1;

out:
    OrlixHostLeaveHostTls(active_tls);
    return result;
}

__attribute__((visibility("hidden"))) int orlix_host_block_flush(
    unsigned int device)
{
    unsigned long active_tls;
    int fd;
    int result = -1;

    if (!OrlixHostBlockDeviceIsSelected(device)) {
        return -1;
    }

    active_tls = OrlixHostEnterHostTls();
    fd = open(OrlixHostSelectedBlockPaths[device],
              OrlixHostSelectedBlockWritable[device] ? O_RDWR : O_RDONLY);
    if (fd < 0) {
        goto out;
    }

    result = fsync(fd) == 0 ? 0 : -1;
    if (close(fd) != 0) {
        result = -1;
    }

out:
    OrlixHostLeaveHostTls(active_tls);
    return result;
}

__attribute__((visibility("hidden"))) void OrlixHostFreeResource(
    struct OrlixHostResource *resource)
{
    unsigned long active_tls;

    if (!resource) {
        return;
    }

    active_tls = OrlixHostEnterHostTls();
    free(resource->data);
    OrlixHostLeaveHostTls(active_tls);
    resource->data = 0;
    resource->size = 0;
}
