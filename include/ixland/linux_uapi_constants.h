/* include/ixland/linux_uapi_constants.h
 * Linux UAPI-sourced constants for IXLandSystem
 *
 * These constants are sourced from Linux UAPI headers.
 * They are safe to use in both kernel code and tests.
 */

#ifndef IXLAND_LINUX_UAPI_CONSTANTS_H
#define IXLAND_LINUX_UAPI_CONSTANTS_H

/* Signal constants from Linux UAPI (asm-generic/signal-defs.h, asm-generic/signal.h) */
#define IX_SIG_BLOCK      0
#define IX_SIG_UNBLOCK    1
#define IX_SIG_SETMASK    2

#define IX_SIGHUP         1
#define IX_SIGINT         2
#define IX_SIGQUIT        3
#define IX_SIGILL         4
#define IX_SIGTRAP        5
#define IX_SIGABRT        6
#define IX_SIGBUS         7
#define IX_SIGFPE         8
#define IX_SIGKILL        9
#define IX_SIGUSR1        10
#define IX_SIGSEGV        11
#define IX_SIGUSR2        12
#define IX_SIGPIPE        13
#define IX_SIGALRM        14
#define IX_SIGTERM        15
#define IX_SIGSTKFLT      16
#define IX_SIGCHLD        17
#define IX_SIGCONT        18
#define IX_SIGSTOP        19
#define IX_SIGTSTP        20
#define IX_SIGTTIN        21
#define IX_SIGTTOU        22
#define IX_SIGURG         23
#define IX_SIGXCPU        24
#define IX_SIGXFSZ        25
#define IX_SIGVTALRM      26
#define IX_SIGPROF        27
#define IX_SIGWINCH       28
#define IX_SIGIO          29
#define IX_SIGPWR         30
#define IX_SIGSYS         31

/* AT_* constants from Linux UAPI (linux/fcntl.h) */
#define IX_AT_SYMLINK_NOFOLLOW    0x100
#define IX_AT_EACCESS             0x200
#define IX_AT_REMOVEDIR           0x200
#define IX_AT_EMPTY_PATH          0x1000

/* renameat2 flags from Linux UAPI (linux/fs.h) */
#define IX_RENAME_NOREPLACE       (1 << 0)
#define IX_RENAME_EXCHANGE        (1 << 1)
#define IX_RENAME_WHITEOUT        (1 << 2)

/* fcntl constants from Linux UAPI (linux/fcntl.h) */
#define IX_F_DUPFD                0
#define IX_F_GETFD                1
#define IX_F_SETFD                2
#define IX_F_GETFL                3
#define IX_F_SETFL                4
#define IX_F_DUPFD_CLOEXEC        1030
#define IX_FD_CLOEXEC             1

/* Termios constants from Linux UAPI (asm-generic/termbits.h) */
#define IX_ISIG                   0x0001
#define IX_ICANON                 0x0002
#define IX_ECHO                   0x0008
#define IX_TOSTOP                 0x0100
#define IX_VMIN                   6
#define IX_VTIME                  5

#endif /* IXLAND_LINUX_UAPI_CONSTANTS_H */
