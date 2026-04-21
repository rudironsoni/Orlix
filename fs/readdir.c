/* IXLandSystem/fs/readdir.c
 * Virtual getdents/getdents64 implementation
 */
#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "fdtable.h"
#include "path.h"
#include "vfs.h"

/* Forward declarations from fdtable.c */
typedef enum {
    FD_TYPE_HOST,
    FD_TYPE_SYNTHETIC_DIR,
    FD_TYPE_PIPE
} fd_type_t;

typedef struct fd_description {
    fd_type_t type;
    int fd;
    int flags;
    mode_t mode;
    off_t offset;
    char path[MAX_PATH];
    bool is_dir;
    void *synthetic_state;
    atomic_int refs;
    pthread_mutex_t lock;
} fd_description_t;



/* Import synthetic directory state */
typedef struct synthetic_dir_state {
    off_t cursor;
    bool entries_emitted;
    synthetic_dir_class_t dir_class;
} synthetic_dir_state_t;

/* Linux dirent64 structure - matches Linux UAPI */
struct linux_dirent64 {
    uint64_t d_ino;
    int64_t d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[];
};

static unsigned char map_dtype(unsigned char dtype) {
    switch (dtype) {
    case DT_FIFO:
        return DT_FIFO;
    case DT_CHR:
        return DT_CHR;
    case DT_DIR:
        return DT_DIR;
    case DT_BLK:
        return DT_BLK;
    case DT_REG:
        return DT_REG;
    case DT_LNK:
        return DT_LNK;
    case DT_SOCK:
        return DT_SOCK;
#ifdef DT_WHT
    case DT_WHT:
        return DT_WHT;
#endif
    default:
        return DT_UNKNOWN;
    }
}

ssize_t getdents64_impl(int fd, void *dirp, size_t count) {
    if (dirp == NULL) {
        errno = EFAULT;
        return -1;
    }

    if (count == 0) {
        errno = EINVAL;
        return -1;
    }

    void *entry = get_fd_entry_impl(fd);
    if (entry == NULL) {
        errno = EBADF;
        return -1;
    }

    int real_fd = get_real_fd_impl(entry);
    off_t saved_offset = get_fd_offset_impl(entry);
    bool is_dir = get_fd_is_dir_impl(entry);
    char fd_path[MAX_PATH];

    if (is_dir && get_fd_path_impl(entry, fd_path, sizeof(fd_path)) == 0 &&
        vfs_path_is_synthetic(fd_path) && !get_fd_is_synthetic_dir_impl(entry)) {
        put_fd_entry_impl(entry);
        errno = ENOTSUP;
        return -1;
    }

    if (get_fd_is_synthetic_dir_impl(entry)) {
        synthetic_dir_state_t *state = NULL;
        fd_description_t *desc = ((fd_entry_t *)entry)->desc;
        size_t written = 0;
        size_t dot_record_len = sizeof(struct linux_dirent64) + 2;
        size_t dotdot_record_len = sizeof(struct linux_dirent64) + 3;
        size_t dot_len = (dot_record_len + 7U) & ~7U;
        size_t dotdot_len = (dotdot_record_len + 7U) & ~7U;

        if (desc && desc->synthetic_state) {
            state = (synthetic_dir_state_t *)desc->synthetic_state;
        } else {
            put_fd_entry_impl(entry);
            errno = EBADF;
            return -1;
        }

        if (state->cursor == 0) {
            if (count >= dot_len + dotdot_len) {
                struct linux_dirent64 *out = (struct linux_dirent64 *)((char *)dirp + written);
                out->d_ino = 1;
                out->d_off = 1;
                out->d_reclen = (unsigned short)dot_len;
                out->d_type = DT_DIR;
                memcpy(out->d_name, ".", 2);
                if (dot_len > dot_record_len) {
                    memset(((char *)out) + dot_record_len, 0, dot_len - dot_record_len);
                }
                written += dot_len;

                out = (struct linux_dirent64 *)((char *)dirp + written);
                out->d_ino = 1;
                out->d_off = 2;
                out->d_reclen = (unsigned short)dotdot_len;
                out->d_type = DT_DIR;
                memcpy(out->d_name, "..", 3);
                if (dotdot_len > dotdot_record_len) {
                    memset(((char *)out) + dotdot_record_len, 0, dotdot_len - dotdot_record_len);
                }
                written += dotdot_len;
                state->cursor = 2;

                put_fd_entry_impl(entry);
                return (ssize_t)written;
            } else if (count >= dot_len) {
                struct linux_dirent64 *out = (struct linux_dirent64 *)((char *)dirp + written);
                out->d_ino = 1;
                out->d_off = 1;
                out->d_reclen = (unsigned short)dot_len;
                out->d_type = DT_DIR;
                memcpy(out->d_name, ".", 2);
                if (dot_len > dot_record_len) {
                    memset(((char *)out) + dot_record_len, 0, dot_len - dot_record_len);
                }
                written += dot_len;
                state->cursor = 1;

                put_fd_entry_impl(entry);
                return (ssize_t)written;
            }
        } else if (state->cursor == 1) {
            if (count >= dotdot_len) {
                struct linux_dirent64 *out = (struct linux_dirent64 *)((char *)dirp + written);
                out->d_ino = 1;
                out->d_off = 2;
                out->d_reclen = (unsigned short)dotdot_len;
                out->d_type = DT_DIR;
                memcpy(out->d_name, "..", 3);
                if (dotdot_len > dotdot_record_len) {
                    memset(((char *)out) + dotdot_record_len, 0, dotdot_len - dotdot_record_len);
                }
                written += dotdot_len;
                state->cursor = 2;

                put_fd_entry_impl(entry);
                return (ssize_t)written;
            }
        } else if (state->cursor >= 2) {
            synthetic_dir_class_t dir_class = state->dir_class;

            if (dir_class == SYNTHETIC_DIR_PROC_SELF) {
                if (state->cursor == 2) {
                    size_t fd_record_len = sizeof(struct linux_dirent64) + 3;
                    size_t fd_aligned_len = (fd_record_len + 7U) & ~7U;
                    if (count >= fd_aligned_len) {
                        struct linux_dirent64 *out = (struct linux_dirent64 *)dirp;
                        out->d_ino = 1;
                        out->d_off = 3;
                        out->d_reclen = (unsigned short)fd_aligned_len;
                        out->d_type = DT_DIR;
                        memcpy(out->d_name, "fd", 3);
                        if (fd_aligned_len > fd_record_len) {
                            memset(((char *)out) + fd_record_len, 0, fd_aligned_len - fd_record_len);
                        }
                        state->cursor = 3;
                        put_fd_entry_impl(entry);
                        return (ssize_t)fd_aligned_len;
                    }
                    put_fd_entry_impl(entry);
                    errno = EINVAL;
                    return -1;
                } else if (state->cursor == 3) {
                    size_t cwd_record_len = sizeof(struct linux_dirent64) + 4;
                    size_t cwd_aligned_len = (cwd_record_len + 7U) & ~7U;
                    if (count >= cwd_aligned_len) {
                        struct linux_dirent64 *out = (struct linux_dirent64 *)dirp;
                        out->d_ino = 1;
                        out->d_off = 4;
                        out->d_reclen = (unsigned short)cwd_aligned_len;
                        out->d_type = DT_LNK;
                        memcpy(out->d_name, "cwd", 4);
                        if (cwd_aligned_len > cwd_record_len) {
                            memset(((char *)out) + cwd_record_len, 0, cwd_aligned_len - cwd_record_len);
                        }
                        state->cursor = 4;
                        put_fd_entry_impl(entry);
                        return (ssize_t)cwd_aligned_len;
                    }
                    put_fd_entry_impl(entry);
                    errno = EINVAL;
                    return -1;
                } else if (state->cursor == 4) {
                    size_t exe_record_len = sizeof(struct linux_dirent64) + 4;
                    size_t exe_aligned_len = (exe_record_len + 7U) & ~7U;
                    if (count >= exe_aligned_len) {
                        struct linux_dirent64 *out = (struct linux_dirent64 *)dirp;
                        out->d_ino = 1;
                        out->d_off = 5;
                        out->d_reclen = (unsigned short)exe_aligned_len;
                        out->d_type = DT_LNK;
                        memcpy(out->d_name, "exe", 4);
                        if (exe_aligned_len > exe_record_len) {
                            memset(((char *)out) + exe_record_len, 0, exe_aligned_len - exe_record_len);
                        }
                        state->cursor = 5;
                        put_fd_entry_impl(entry);
                        return (ssize_t)exe_aligned_len;
                    }
                    put_fd_entry_impl(entry);
                    errno = EINVAL;
                    return -1;
                } else if (state->cursor == 5) {
                    size_t cmdline_record_len = sizeof(struct linux_dirent64) + 8;
                    size_t cmdline_aligned_len = (cmdline_record_len + 7U) & ~7U;
                    if (count >= cmdline_aligned_len) {
                        struct linux_dirent64 *out = (struct linux_dirent64 *)dirp;
                        out->d_ino = 1;
                        out->d_off = 6;
                        out->d_reclen = (unsigned short)cmdline_aligned_len;
                        out->d_type = DT_REG;
                        memcpy(out->d_name, "cmdline", 8);
                        if (cmdline_aligned_len > cmdline_record_len) {
                            memset(((char *)out) + cmdline_record_len, 0, cmdline_aligned_len - cmdline_record_len);
                        }
                        state->cursor = 6;
                        put_fd_entry_impl(entry);
                        return (ssize_t)cmdline_aligned_len;
                    }
                    put_fd_entry_impl(entry);
                    errno = EINVAL;
                    return -1;
    } else if (state->cursor == 6) {
        size_t comm_record_len = sizeof(struct linux_dirent64) + 5;
        size_t comm_aligned_len = (comm_record_len + 7U) & ~7U;
        if (count >= comm_aligned_len) {
            struct linux_dirent64 *out = (struct linux_dirent64 *)dirp;
            out->d_ino = 1;
            out->d_off = 7;
            out->d_reclen = (unsigned short)comm_aligned_len;
            out->d_type = DT_REG;
            memcpy(out->d_name, "comm", 5);
            if (comm_aligned_len > comm_record_len) {
                memset(((char *)out) + comm_record_len, 0, comm_aligned_len - comm_record_len);
            }
            state->cursor = 7;
            put_fd_entry_impl(entry);
            return (ssize_t)comm_aligned_len;
        }
        put_fd_entry_impl(entry);
        errno = EINVAL;
        return -1;
    } else if (state->cursor == 7) {
        size_t stat_record_len = sizeof(struct linux_dirent64) + 5;
        size_t stat_aligned_len = (stat_record_len + 7U) & ~7U;
        if (count >= stat_aligned_len) {
            struct linux_dirent64 *out = (struct linux_dirent64 *)dirp;
            out->d_ino = 1;
            out->d_off = 8;
            out->d_reclen = (unsigned short)stat_aligned_len;
            out->d_type = DT_REG;
            memcpy(out->d_name, "stat", 5);
            if (stat_aligned_len > stat_record_len) {
                memset(((char *)out) + stat_record_len, 0, stat_aligned_len - stat_record_len);
            }
            state->cursor = 8;
            put_fd_entry_impl(entry);
            return (ssize_t)stat_aligned_len;
        }
        put_fd_entry_impl(entry);
        errno = EINVAL;
        return -1;
    } else if (state->cursor == 8) {
        size_t statm_record_len = sizeof(struct linux_dirent64) + 6;
        size_t statm_aligned_len = (statm_record_len + 7U) & ~7U;
        if (count >= statm_aligned_len) {
            struct linux_dirent64 *out = (struct linux_dirent64 *)dirp;
            out->d_ino = 1;
            out->d_off = 9;
            out->d_reclen = (unsigned short)statm_aligned_len;
            out->d_type = DT_REG;
            memcpy(out->d_name, "statm", 6);
            if (statm_aligned_len > statm_record_len) {
                memset(((char *)out) + statm_record_len, 0, statm_aligned_len - statm_record_len);
            }
            state->cursor = 9;
            put_fd_entry_impl(entry);
            return (ssize_t)statm_aligned_len;
        }
        put_fd_entry_impl(entry);
        errno = EINVAL;
        return -1;
    } else if (state->cursor == 9) {
        size_t fdinfo_record_len = sizeof(struct linux_dirent64) + 7;
        size_t fdinfo_aligned_len = (fdinfo_record_len + 7U) & ~7U;
        if (count >= fdinfo_aligned_len) {
            struct linux_dirent64 *out = (struct linux_dirent64 *)dirp;
            out->d_ino = 1;
            out->d_off = 10;
            out->d_reclen = (unsigned short)fdinfo_aligned_len;
            out->d_type = DT_DIR;
            memcpy(out->d_name, "fdinfo", 7);
            if (fdinfo_aligned_len > fdinfo_record_len) {
                memset(((char *)out) + fdinfo_record_len, 0, fdinfo_aligned_len - fdinfo_record_len);
            }
            state->cursor = 10;
            put_fd_entry_impl(entry);
            return (ssize_t)fdinfo_aligned_len;
        }
        put_fd_entry_impl(entry);
        errno = EINVAL;
        return -1;
    }
                put_fd_entry_impl(entry);
                return 0;
            } else if (dir_class == SYNTHETIC_DIR_PROC_SELF_FD) {
                size_t written = 0;
                int scan_fd;
                for (scan_fd = (int)state->cursor - 2 + 3; scan_fd < NR_OPEN_DEFAULT; scan_fd++) {
                    if (!fdtable_is_used_impl(scan_fd)) {
                        continue;
                    }
                    char fd_name[12];
                    int fd_name_len = 0;
                    {
                        int n = scan_fd;
                        char tmp[12];
                        int pos = 0;
                        if (n == 0) { tmp[pos++] = '0'; }
                        else { while (n > 0) { tmp[pos++] = '0' + (n % 10); n /= 10; } }
                        for (int j = 0; j < pos; j++) { fd_name[j] = tmp[pos - 1 - j]; }
                        fd_name_len = pos;
                        fd_name[fd_name_len] = '\0';
                    }
                    size_t record_len = sizeof(struct linux_dirent64) + fd_name_len + 1;
                    size_t aligned_len = (record_len + 7U) & ~7U;
                    if (aligned_len > count - written) {
                        break;
                    }
                    struct linux_dirent64 *out = (struct linux_dirent64 *)((char *)dirp + written);
                    out->d_ino = 1;
                    out->d_off = (int64_t)(scan_fd + 1);
                    out->d_reclen = (unsigned short)aligned_len;
                    out->d_type = DT_LNK;
                    memcpy(out->d_name, fd_name, fd_name_len + 1);
                    if (aligned_len > record_len) {
                        memset(((char *)out) + record_len, 0, aligned_len - record_len);
                    }
                    written += aligned_len;
                    state->cursor = (off_t)(scan_fd - 3 + 2 + 1);
                }
                if (scan_fd >= NR_OPEN_DEFAULT) {
                    state->cursor = (off_t)(NR_OPEN_DEFAULT - 3 + 2 + 1);
                }
                put_fd_entry_impl(entry);
                return (ssize_t)written;
            } else if (dir_class == SYNTHETIC_DIR_PROC_SELF_FDINFO) {
                size_t written = 0;
                int scan_fd;
                for (scan_fd = (int)state->cursor - 2 + 3; scan_fd < NR_OPEN_DEFAULT; scan_fd++) {
                    if (!fdtable_is_used_impl(scan_fd)) {
                        continue;
                    }
                    char fd_name[12];
                    int fd_name_len = 0;
                    {
                        int n = scan_fd;
                        char tmp[12];
                        int pos = 0;
                        if (n == 0) { tmp[pos++] = '0'; }
                        else { while (n > 0) { tmp[pos++] = '0' + (n % 10); n /= 10; } }
                        for (int j = 0; j < pos; j++) { fd_name[j] = tmp[pos - 1 - j]; }
                        fd_name_len = pos;
                        fd_name[fd_name_len] = '\0';
                    }
                    size_t record_len = sizeof(struct linux_dirent64) + fd_name_len + 1;
                    size_t aligned_len = (record_len + 7U) & ~7U;
                    if (aligned_len > count - written) {
                        break;
                    }
                    struct linux_dirent64 *out = (struct linux_dirent64 *)((char *)dirp + written);
                    out->d_ino = 1;
                    out->d_off = (int64_t)(scan_fd + 1);
                    out->d_reclen = (unsigned short)aligned_len;
                    out->d_type = DT_REG;
                    memcpy(out->d_name, fd_name, fd_name_len + 1);
                    if (aligned_len > record_len) {
                        memset(((char *)out) + record_len, 0, aligned_len - record_len);
                    }
                    written += aligned_len;
                    state->cursor = (off_t)(scan_fd - 3 + 2 + 1);
                }
                if (scan_fd >= NR_OPEN_DEFAULT) {
                    state->cursor = (off_t)(NR_OPEN_DEFAULT - 3 + 2 + 1);
                }
                put_fd_entry_impl(entry);
                return (ssize_t)written;
            }
            put_fd_entry_impl(entry);
            return 0;
        }

        put_fd_entry_impl(entry);
        errno = EINVAL;
        return -1;
    }
  int dup_fd = dup(real_fd);
  if (dup_fd < 0) {
    put_fd_entry_impl(entry);
    return -1;
  }

    DIR *dp = fdopendir(dup_fd);
    if (dp == NULL) {
        int saved_errno = errno;
        close(dup_fd);
        put_fd_entry_impl(entry);
        errno = saved_errno;
        return -1;
    }

    if (saved_offset > 0) {
        seekdir(dp, saved_offset);
    }

    size_t written = 0;
    off_t latest_offset = saved_offset;
    errno = 0;

    while (true) {
        struct dirent *native = readdir(dp);
        if (native == NULL) {
            if (errno != 0 && written == 0) {
                int saved_errno = errno;
                closedir(dp);
                put_fd_entry_impl(entry);
                errno = saved_errno;
                return -1;
            }
            break;
        }

        size_t name_len = strlen(native->d_name);
        size_t base_len = sizeof(struct linux_dirent64);
        size_t record_len = base_len + name_len + 1;
        size_t aligned_len = (record_len + 7U) & ~7U;

        if (aligned_len > count - written) {
            if (written == 0) {
                closedir(dp);
                put_fd_entry_impl(entry);
                errno = EINVAL;
                return -1;
            }
            break;
        }

        struct linux_dirent64 *out = (struct linux_dirent64 *)((char *)dirp + written);
        out->d_ino = native->d_ino;
        latest_offset = (off_t)telldir(dp);
        out->d_off = latest_offset;
        out->d_reclen = (unsigned short)aligned_len;
        out->d_type = map_dtype(native->d_type);
        memcpy(out->d_name, native->d_name, name_len + 1);

        if (aligned_len > record_len) {
            memset(((char *)out) + record_len, 0, aligned_len - record_len);
        }

        written += aligned_len;
    }

    set_fd_offset_impl(entry, latest_offset);
    closedir(dp);
    put_fd_entry_impl(entry);
    return (ssize_t)written;
}

ssize_t getdents_impl(int fd, void *dirp, size_t count) {
    return getdents64_impl(fd, dirp, count);
}

__attribute__((visibility("default"))) ssize_t getdents(int fd, void *dirp, size_t count) {
    return getdents_impl(fd, dirp, count);
}

__attribute__((visibility("default"))) ssize_t getdents64(int fd, void *dirp, size_t count) {
    return getdents64_impl(fd, dirp, count);
}
