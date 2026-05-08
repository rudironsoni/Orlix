#ifndef IXLAND_INTERNAL_PRIVATE_BACKING_DIR_H
#define IXLAND_INTERNAL_PRIVATE_BACKING_DIR_H

#include <stdint.h>

#define BACKING_DIR_NAME_MAX 256

struct backing_dir_stream;

struct backing_dir_record {
    uint64_t ino;
    int64_t off;
    unsigned char type;
    char name[BACKING_DIR_NAME_MAX];
};

int backing_dir_open(int fd, int64_t offset, struct backing_dir_stream **out_stream);
int backing_dir_read(struct backing_dir_stream *stream, struct backing_dir_record *record);
void backing_dir_close(struct backing_dir_stream *stream);

#endif
