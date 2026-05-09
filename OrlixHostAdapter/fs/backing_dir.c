#include "internal/fs/readdir.h"

#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "backing_io_internal.h"

struct backing_dir_stream {
    DIR *dir;
};

int backing_dir_open(int fd, int64_t offset, struct backing_dir_stream **out_stream) {
    struct backing_dir_stream *stream;
    int dup_fd;

    if (!out_stream) {
        errno = EFAULT;
        return -1;
    }
    *out_stream = NULL;

    dup_fd = backing_dup(fd);
    if (dup_fd < 0) {
        return -1;
    }

    stream = calloc(1, sizeof(*stream));
    if (!stream) {
        int saved_errno = errno;
        backing_close(dup_fd);
        errno = saved_errno;
        return -1;
    }

    stream->dir = fdopendir(dup_fd);
    if (!stream->dir) {
        int saved_errno = errno;
        backing_close(dup_fd);
        free(stream);
        errno = saved_errno;
        return -1;
    }

    if (offset > 0) {
        seekdir(stream->dir, (long)offset);
    }

    *out_stream = stream;
    return 0;
}

int backing_dir_read(struct backing_dir_stream *stream, struct backing_dir_record *record) {
    struct dirent *native;
    size_t name_len;

    if (!stream || !stream->dir || !record) {
        errno = EFAULT;
        return -1;
    }

    errno = 0;
    native = readdir(stream->dir);
    if (!native) {
        return (errno == 0) ? 0 : -1;
    }

    name_len = strlen(native->d_name);
    if (name_len >= sizeof(record->name)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    record->ino = native->d_ino;
    record->off = (int64_t)telldir(stream->dir);
    record->type = native->d_type;
    memcpy(record->name, native->d_name, name_len + 1);
    return 1;
}

void backing_dir_close(struct backing_dir_stream *stream) {
    if (!stream) {
        return;
    }
    if (stream->dir) {
        closedir(stream->dir);
    }
    free(stream);
}
