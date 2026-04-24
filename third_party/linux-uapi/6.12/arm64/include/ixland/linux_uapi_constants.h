/* IXLandSystem/include/linux_uapi_constants.h
 *
 * Minimal Linux UAPI constant definitions for Linux-owner code.
 *
 * This header provides ONLY the constant values (macros) from Linux UAPI,
 * without any type definitions that would conflict with Darwin headers.
 *
 * Include this header FIRST, before any system headers, to get Linux
 * constant values like O_CREAT=0x40, AT_FDCWD=-100, etc.
 */

#ifndef LINUX_UAPI_CONSTANTS_H
#define LINUX_UAPI_CONSTANTS_H

/* ============================================================================
 * fcntl.h - File control constants (from Linux 6.12 UAPI)
 * ============================================================================ */

/* file open modes */
#define O_ACCMODE       00000003
#define O_RDONLY        00000000
#define O_WRONLY        00000001
#define O_RDWR          00000002

/* file creation flags */
#define O_CREAT         00000100        /* not fcntl */
#define O_EXCL          00000200        /* not fcntl */
#define O_NOCTTY        00000400        /* not fcntl */
#define O_TRUNC         00001000        /* not fcntl */
#define O_APPEND        00002000
#define O_NONBLOCK      00004000
#define O_DSYNC         00010000
#define O_DIRECT        00040000
#define O_DIRECTORY     00200000
#define O_NOFOLLOW      00400000
#define O_NOATIME       01000000
#define O_CLOEXEC       02000000
#define O_SYNC          (04000000|O_DSYNC)
#define O_PATH          010000000

#define O_NDELAY        O_NONBLOCK

/* fcntl commands */
#define F_DUPFD         0               /* dup */
#define F_GETFD         1               /* get close_on_exec */
#define F_SETFD         2               /* set/clear close_on_exec */
#define F_GETFL         3               /* get file->f_flags */
#define F_SETFL         4               /* set file->f_flags */
#define F_DUPFD_CLOEXEC 1030            /* dup with FD_CLOEXEC set */

/* for F_[GET|SET]FD */
#define FD_CLOEXEC      1

/* for posix fcntl() and lockf() */
#define F_RDLCK         0
#define F_WRLCK         1
#define F_UNLCK         2

/* operations for bsd flock() */
#define LOCK_SH         1
#define LOCK_EX         2
#define LOCK_NB         4
#define LOCK_UN         8

/* ============================================================================
 * linux/fcntl.h - Linux-specific fcntl constants
 * ============================================================================ */

#define AT_FDCWD                -100

/* Generic flags for the *at(2) family of syscalls */
#define AT_SYMLINK_NOFOLLOW     0x100
#define AT_SYMLINK_FOLLOW       0x400
#define AT_NO_AUTOMOUNT         0x800
#define AT_EMPTY_PATH           0x1000

/* Flags for renameat2(2) */
#define AT_RENAME_NOREPLACE     0x0001
#define AT_RENAME_EXCHANGE      0x0002
#define AT_RENAME_WHITEOUT      0x0004

/* Flag for faccessat(2) */
#define AT_EACCESS              0x200

/* Flag for unlinkat(2) */
#define AT_REMOVEDIR            0x200

/* ============================================================================
 * ioctls.h - Terminal ioctl constants
 * ============================================================================ */

#define TCGETS          0x5401
#define TCSETS          0x5402
#define TCSETSW         0x5403
#define TCSETSF         0x5404
#define TIOCEXCL        0x540C
#define TIOCNXCL        0x540D
#define TIOCSCTTY       0x540E
#define TIOCGPGRP       0x540F
#define TIOCSPGRP       0x5410
#define TIOCOUTQ        0x5411
#define TIOCSTI         0x5412
#define TIOCGWINSZ      0x5413
#define TIOCSWINSZ      0x5414
#define FIONREAD        0x541B
#define TIOCNOTTY       0x5422
#define TIOCGSID        0x5429
#define TIOCGPTN        0x80045430UL
#define TIOCSPTLCK      0x40045431UL

/* ============================================================================
 * signal.h - Signal operation constants
 * ============================================================================ */

/* sigprocmask how values */
#define SIG_BLOCK       0
#define SIG_UNBLOCK     1
#define SIG_SETMASK     2

/* Number of signals */
#define _NSIG           64

#endif /* LINUX_UAPI_CONSTANTS_H */
