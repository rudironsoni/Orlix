#include "SyscallUioContract.h"

#include <asm/unistd.h>
#include <linux/fcntl.h>
#include <linux/uio.h>

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "runtime/syscall.h"

extern int close_impl(int fd);
extern int unlink_impl(const char *pathname);

static int close_if_open(int fd) {
    if (fd >= 0) {
        return close_impl(fd);
    }
    return 0;
}

int syscall_uio_contract_readv_writev_round_trip(void) {
    const char *path = "/tmp/syscall-uio-round-trip";
    char left[] = "shell";
    char right[] = "-uio";
    char out_left[6];
    char out_right[5];
    struct iovec write_iov[2];
    struct iovec read_iov[2];
    int fd = -1;
    long ret;

    unlink_impl(path);
    fd = (int)syscall_dispatch_impl(__NR_openat, AT_FDCWD, (long)(uintptr_t)path,
                                    O_CREAT | O_RDWR | O_TRUNC, 0600, 0, 0);
    if (fd < 0) {
        errno = (int)-fd;
        goto out;
    }

    write_iov[0].iov_base = left;
    write_iov[0].iov_len = 5;
    write_iov[1].iov_base = right;
    write_iov[1].iov_len = 4;
    ret = syscall_dispatch_impl(__NR_writev, fd, (long)(uintptr_t)write_iov, 2, 0, 0, 0);
    if (ret != 9) {
        errno = ret < 0 ? (int)-ret : EIO;
        goto out;
    }
    close_if_open(fd);

    fd = (int)syscall_dispatch_impl(__NR_openat, AT_FDCWD, (long)(uintptr_t)path,
                                    O_RDONLY, 0, 0, 0);
    if (fd < 0) {
        errno = (int)-fd;
        goto out;
    }
    memset(out_left, 0, sizeof(out_left));
    memset(out_right, 0, sizeof(out_right));
    read_iov[0].iov_base = out_left;
    read_iov[0].iov_len = 5;
    read_iov[1].iov_base = out_right;
    read_iov[1].iov_len = 4;
    ret = syscall_dispatch_impl(__NR_readv, fd, (long)(uintptr_t)read_iov, 2, 0, 0, 0);
    if (ret != 9 || strcmp(out_left, "shell") != 0 || strcmp(out_right, "-uio") != 0) {
        errno = ret < 0 ? (int)-ret : ENODATA;
        goto out;
    }

    close_if_open(fd);
    fd = -1;
    unlink_impl(path);
    return 0;

out:
    close_if_open(fd);
    unlink_impl(path);
    return -1;
}

int syscall_uio_contract_rejects_invalid_iov_count(void) {
    long ret = syscall_dispatch_impl(__NR_readv, 0, 0, UIO_MAXIOV + 1, 0, 0, 0);

    if (ret != -EINVAL) {
        errno = ret < 0 ? (int)-ret : EPROTO;
        return -1;
    }
    return 0;
}
