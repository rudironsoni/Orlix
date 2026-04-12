#ifndef IXLAND_DIRENT_H
#define IXLAND_DIRENT_H

#include <stddef.h>
#include <stdint.h>

/* Directory entry types */
#define IXLAND_DT_UNKNOWN 0
#define IXLAND_DT_FIFO 1
#define IXLAND_DT_CHR 2
#define IXLAND_DT_DIR 4
#define IXLAND_DT_BLK 6
#define IXLAND_DT_REG 8
#define IXLAND_DT_LNK 10
#define IXLAND_DT_SOCK 12
#define IXLAND_DT_WHT 14

struct ixland_dirent_64 {
    uint64_t d_ino;
    int64_t d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[];
};

#endif
