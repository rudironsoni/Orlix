#include "OrlixHostAdapter/boot/resources.h"

#include <CoreFoundation/CoreFoundation.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char OrlixHostSelectedRootBlockPath[PATH_MAX];
static unsigned long long OrlixHostSelectedRootBlockBytes;

static const char *OrlixHostRootImageResourceForIdentifier(const char *identifier)
{
    if (!identifier) {
        return 0;
    }
    if (strcmp(identifier, "orlix.bundle.rootfs") == 0) {
        return "rootfs/initramfs.cpio.gz";
    }
    return 0;
}

static int OrlixHostCopyPayloadRootPath(char *path, size_t path_size)
{
    CFBundleRef kernel_bundle;
    CFURLRef payload_url;
    Boolean ok;

    if (!path || path_size == 0) {
        return -1;
    }

    kernel_bundle = CFBundleGetBundleWithIdentifier(CFSTR("org.orlix.OrlixKernel"));
    if (!kernel_bundle) {
        return -1;
    }

    payload_url = CFBundleCopyResourceURL(
        kernel_bundle,
        CFSTR("OrlixKernelPayload"),
        CFSTR("bundle"),
        0);
    if (!payload_url) {
        return -1;
    }

    ok = CFURLGetFileSystemRepresentation(payload_url, true, (UInt8 *)path, path_size);
    CFRelease(payload_url);

    return ok ? 0 : -1;
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

__attribute__((visibility("hidden"))) int OrlixHostLoadRootImageResource(
    const char *identifier,
    struct OrlixHostResource *loaded)
{
    const char *resource = OrlixHostRootImageResourceForIdentifier(identifier);

    if (!resource) {
        return -1;
    }

    return OrlixHostLoadKernelPayloadResource(resource, loaded);
}

__attribute__((visibility("hidden"))) int OrlixHostSelectRootBlockImage(
    const char *identifier)
{
    const char *resource = OrlixHostRootImageResourceForIdentifier(identifier);
    unsigned long long size = 0;
    char path[PATH_MAX];

    OrlixHostSelectedRootBlockPath[0] = '\0';
    OrlixHostSelectedRootBlockBytes = 0;

    if (!resource) {
        return -1;
    }
    if (OrlixHostCopyPayloadResourcePath(resource, path, sizeof(path)) != 0) {
        return -1;
    }
    if (OrlixHostResourceFileSize(path, &size) != 0) {
        return -1;
    }

    memcpy(OrlixHostSelectedRootBlockPath, path, strlen(path) + 1);
    OrlixHostSelectedRootBlockBytes = size;
    return 0;
}

__attribute__((visibility("hidden"))) int orlix_host_block_capacity(
    unsigned int device,
    unsigned long long *sectors)
{
    if (device != 0 || !sectors || OrlixHostSelectedRootBlockPath[0] == '\0') {
        return -1;
    }

    *sectors = (OrlixHostSelectedRootBlockBytes + 511ULL) / 512ULL;
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

    if (device != 0 || !buffer || !length ||
        OrlixHostSelectedRootBlockPath[0] == '\0' ||
        sector > ULLONG_MAX / 512ULL) {
        return -1;
    }

    offset = sector * 512ULL;
    capacity_bytes = ((OrlixHostSelectedRootBlockBytes + 511ULL) / 512ULL) * 512ULL;
    if (offset > capacity_bytes ||
        length > capacity_bytes - offset ||
        offset > (unsigned long long)LONG_MAX) {
        return -1;
    }

    memset(buffer, 0, length);
    if (offset >= OrlixHostSelectedRootBlockBytes) {
        return 0;
    }

    available = OrlixHostSelectedRootBlockBytes - offset;
    file_read_length = length;
    if (available < file_read_length) {
        file_read_length = (unsigned int)available;
    }
    file = fopen(OrlixHostSelectedRootBlockPath, "rb");
    if (!file) {
        return -1;
    }
    if (fseek(file, (long)offset, SEEK_SET) != 0) {
        fclose(file);
        return -1;
    }

    read_count = fread(buffer, 1, file_read_length, file);
    fclose(file);
    return read_count == file_read_length ? 0 : -1;
}

__attribute__((visibility("hidden"))) int orlix_host_block_write(
    unsigned int device,
    unsigned long long sector,
    const void *buffer,
    unsigned int length)
{
    (void)device;
    (void)sector;
    (void)buffer;
    (void)length;
    return -1;
}

__attribute__((visibility("hidden"))) void OrlixHostFreeResource(
    struct OrlixHostResource *resource)
{
    if (!resource) {
        return;
    }

    free(resource->data);
    resource->data = 0;
    resource->size = 0;
}
