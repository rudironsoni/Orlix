#ifndef ORLIX_MLIBC_SYS_SELECT_H
#define ORLIX_MLIBC_SYS_SELECT_H

#ifndef _SYS_SELECT_H_
#define _SYS_SELECT_H_
#endif

#ifndef FD_SETSIZE
#define FD_SETSIZE 1024
#endif

#define __ORLIX_NFDBITS ((int)(8U * sizeof(unsigned long)))
#define __ORLIX_FDSET_WORDS ((FD_SETSIZE + __ORLIX_NFDBITS - 1) / __ORLIX_NFDBITS)

#ifndef _FD_SET
#define _FD_SET
typedef struct fd_set {
    unsigned long fds_bits[__ORLIX_FDSET_WORDS];
} fd_set;
#endif

#ifndef FD_ZERO
#define FD_ZERO(set) __orlix_fd_zero((set))
#endif
#ifndef FD_SET
#define FD_SET(fd, set) __orlix_fd_set((fd), (set))
#endif
#ifndef FD_CLR
#define FD_CLR(fd, set) __orlix_fd_clr((fd), (set))
#endif
#ifndef FD_ISSET
#define FD_ISSET(fd, set) __orlix_fd_isset((fd), (set))
#endif

static inline void __orlix_fd_zero(fd_set *set) {
    int i;

    if (!set) {
        return;
    }
    for (i = 0; i < __ORLIX_FDSET_WORDS; i++) {
        set->fds_bits[i] = 0;
    }
}

static inline void __orlix_fd_set(int fd, fd_set *set) {
    if (!set || fd < 0 || fd >= FD_SETSIZE) {
        return;
    }
    set->fds_bits[fd / __ORLIX_NFDBITS] |= (1UL << (fd % __ORLIX_NFDBITS));
}

static inline void __orlix_fd_clr(int fd, fd_set *set) {
    if (!set || fd < 0 || fd >= FD_SETSIZE) {
        return;
    }
    set->fds_bits[fd / __ORLIX_NFDBITS] &= ~(1UL << (fd % __ORLIX_NFDBITS));
}

static inline int __orlix_fd_isset(int fd, const fd_set *set) {
    if (!set || fd < 0 || fd >= FD_SETSIZE) {
        return 0;
    }
    return (set->fds_bits[fd / __ORLIX_NFDBITS] & (1UL << (fd % __ORLIX_NFDBITS))) != 0;
}

#endif
