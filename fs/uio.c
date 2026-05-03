#include <linux/uio.h>

#include <errno.h>
#include <stddef.h>

extern long read_impl(int fd, void *buf, size_t count);
extern long write_impl(int fd, const void *buf, size_t count);

long readv_impl(int fd, const struct iovec *iov, int iovcnt) {
    long total = 0;

    if (iovcnt < 0 || iovcnt > UIO_MAXIOV) {
        errno = EINVAL;
        return -1;
    }
    if (iovcnt == 0) {
        return 0;
    }
    if (!iov) {
        errno = EFAULT;
        return -1;
    }

    for (int i = 0; i < iovcnt; i++) {
        long nread;

        if (iov[i].iov_len != 0 && !iov[i].iov_base) {
            errno = EFAULT;
            return total > 0 ? total : -1;
        }
        nread = read_impl(fd, iov[i].iov_base, iov[i].iov_len);
        if (nread < 0) {
            return total > 0 ? total : -1;
        }
        total += nread;
        if ((__kernel_size_t)nread < iov[i].iov_len) {
            break;
        }
    }

    return total;
}

long writev_impl(int fd, const struct iovec *iov, int iovcnt) {
    long total = 0;

    if (iovcnt < 0 || iovcnt > UIO_MAXIOV) {
        errno = EINVAL;
        return -1;
    }
    if (iovcnt == 0) {
        return 0;
    }
    if (!iov) {
        errno = EFAULT;
        return -1;
    }

    for (int i = 0; i < iovcnt; i++) {
        long nwritten;

        if (iov[i].iov_len != 0 && !iov[i].iov_base) {
            errno = EFAULT;
            return total > 0 ? total : -1;
        }
        nwritten = write_impl(fd, iov[i].iov_base, iov[i].iov_len);
        if (nwritten < 0) {
            return total > 0 ? total : -1;
        }
        total += nwritten;
        if ((__kernel_size_t)nwritten < iov[i].iov_len) {
            break;
        }
    }

    return total;
}

__attribute__((visibility("default"))) long readv(int fd, const struct iovec *iov, int iovcnt) {
    return readv_impl(fd, iov, iovcnt);
}

__attribute__((visibility("default"))) long writev(int fd, const struct iovec *iov, int iovcnt) {
    return writev_impl(fd, iov, iovcnt);
}
