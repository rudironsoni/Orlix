/* IXLandSystem/include/ixland/linux_abi_constants.h
 *
 * Linux ABI constant values for Linux-owner code.
 *
 * This header is AUTO-GENERATED from vendored Linux UAPI headers to ensure
 * Linux-owner code uses correct Linux ABI constants (O_CREAT=0x40, etc.)
 * rather than Darwin values (O_CREAT=0x200).
 *
 * WHY NOT INCLUDE LINUX UAPI HEADERS DIRECTLY?
 * The vendored Linux UAPI headers (e.g., <asm-generic/fcntl.h>) contain
 * not only constants but also type definitions (sigset_t, struct flock)
 * that conflict with Darwin system headers. When both are included in the
 * same translation unit, ambiguous type errors occur.
 *
 * This header extracts ONLY the constant macro definitions from Linux UAPI
 * headers, avoiding all type definitions. It must be included BEFORE any
 * Darwin headers to ensure Linux constants take precedence.
 *
 * SOURCE OF TRUTH (Linux UAPI 6.12 arm64):
 *   third_party/linux-uapi/6.12/arm64/include/asm-generic/fcntl.h
 *   third_party/linux-uapi/6.12/arm64/include/linux/fcntl.h
 *   third_party/linux-uapi/6.12/arm64/include/asm-generic/ioctls.h
 *   third_party/linux-uapi/6.12/arm64/include/asm-generic/signal.h
 *   third_party/linux-uapi/6.12/arm64/include/asm-generic/poll.h
 *   third_party/linux-uapi/6.12/arm64/include/linux/eventpoll.h
 *
 * MAINTENANCE:
 * If Linux UAPI constants change, regenerate this file by extracting
 * #define lines from the above headers. The values MUST match the Linux
 * kernel ABI exactly.
 */

#ifndef IXLAND_LINUX_ABI_CONSTANTS_H
#define IXLAND_LINUX_ABI_CONSTANTS_H

/* ============================================================================
 * FILE OPEN MODES - from Linux UAPI asm-generic/fcntl.h
 * ============================================================================ */
#define O_ACCMODE       00000003
#define O_RDONLY        00000000
#define O_WRONLY        00000001
#define O_RDWR          00000002

/* File creation flags - from Linux UAPI asm-generic/fcntl.h */
#define O_CREAT         00000100
#define O_EXCL          00000200
#define O_NOCTTY        00000400
#define O_TRUNC         00001000
#define O_APPEND        00002000
#define O_NONBLOCK      00004000
#define O_DSYNC         00010000
#define FASYNC          00020000
#define O_DIRECT        00040000
#define O_LARGEFILE     00100000
#define O_DIRECTORY     00200000
#define O_NOFOLLOW      00400000
#define O_NOATIME       01000000
#define O_CLOEXEC       02000000
#define O_SYNC          04010000  /* __O_SYNC|O_DSYNC */
#define O_PATH          010000000
#define O_NDELAY        O_NONBLOCK

/* fcntl commands - from Linux UAPI asm-generic/fcntl.h */
#define F_DUPFD         0
#define F_GETFD         1
#define F_SETFD         2
#define F_GETFL         3
#define F_SETFL         4
#define F_GETLK         5
#define F_SETLK         6
#define F_SETLKW        7
#define F_SETOWN        8
#define F_GETOWN        9
#define F_SETSIG        10
#define F_GETSIG        11
#define F_DUPFD_CLOEXEC 1030  /* F_LINUX_SPECIFIC_BASE + 6 */
#define FD_CLOEXEC      1

/* AT_ constants - from Linux UAPI linux/fcntl.h */
#define AT_FDCWD                -100
#define AT_SYMLINK_NOFOLLOW     0x100
#define AT_SYMLINK_FOLLOW       0x400
#define AT_EMPTY_PATH           0x1000
#define AT_EACCESS              0x200
#define AT_REMOVEDIR            0x200
#define AT_RENAME_NOREPLACE     0x0001
#define AT_RENAME_EXCHANGE      0x0002
#define AT_RENAME_WHITEOUT      0x0004

/* ============================================================================
 * TERMINAL IOCTL CONSTANTS - from Linux UAPI asm-generic/ioctls.h
 * ============================================================================ */
#define TCGETS          0x5401
#define TCSETS          0x5402
#define TCSETSW         0x5403
#define TCSETSF         0x5404
#define TCGETA          0x5405
#define TCSETA          0x5406
#define TCSETAW         0x5407
#define TCSETAF         0x5408
#define TCSBRK          0x5409
#define TCXONC          0x540A
#define TCFLSH          0x540B
#define TIOCEXCL        0x540C
#define TIOCNXCL        0x540D
#define TIOCSCTTY       0x540E
#define TIOCGPGRP       0x540F
#define TIOCSPGRP       0x5410
#define TIOCOUTQ        0x5411
#define TIOCSTI         0x5412
#define TIOCGWINSZ      0x5413
#define TIOCSWINSZ      0x5414
#define TIOCMGET        0x5415
#define TIOCMBIS        0x5416
#define TIOCMBIC        0x5417
#define TIOCMSET        0x5418
#define TIOCGSOFTCAR    0x5419
#define TIOCSSOFTCAR    0x541A
#define FIONREAD        0x541B
#define TIOCINQ         FIONREAD
#define TIOCLINUX       0x541C
#define TIOCCONS        0x541D
#define TIOCGSERIAL     0x541E
#define TIOCSSERIAL     0x541F
#define TIOCPKT         0x5420
#define FIONBIO         0x5421
#define TIOCNOTTY       0x5422
#define TIOCSETD        0x5423
#define TIOCGETD        0x5424
#define TCSBRKP         0x5425
#define TIOCSBRK        0x5427
#define TIOCCBRK        0x5428
#define TIOCGSID        0x5429
#define FIONCLEX        0x5450
#define FIOCLEX         0x5451
#define FIOASYNC        0x5452

#define TIOCPKT_DATA            0
#define TIOCPKT_FLUSHREAD       1
#define TIOCPKT_FLUSHWRITE      2
#define TIOCPKT_STOP            4
#define TIOCPKT_START           8
#define TIOCPKT_NOSTOP          16
#define TIOCPKT_DOSTOP          32
#define TIOCPKT_IOCTL           64

/* PTY ioctls */
#define TIOCGPTN        0x80045430  /* _IOR('T', 0x30, unsigned int) */
#define TIOCSPTLCK      0x40045431  /* _IOW('T', 0x31, int) */

/* ============================================================================
 * SIGNAL CONSTANTS - from Linux UAPI asm-generic/signal.h
 * ============================================================================ */
#define _NSIG           64
#define SIGRTMIN        32
#define SIGRTMAX        _NSIG

/* Standard signals */
#define SIGHUP          1
#define SIGINT          2
#define SIGQUIT         3
#define SIGILL          4
#define SIGTRAP         5
#define SIGABRT         6
#define SIGIOT          6
#define SIGBUS          7
#define SIGFPE          8
#define SIGKILL         9
#define SIGUSR1         10
#define SIGSEGV         11
#define SIGUSR2         12
#define SIGPIPE         13
#define SIGALRM         14
#define SIGTERM         15
#define SIGSTKFLT       16
#define SIGCHLD         17
#define SIGCONT         18
#define SIGSTOP         19
#define SIGTSTP         20
#define SIGTTIN         21
#define SIGTTOU         22
#define SIGURG          23
#define SIGXCPU         24
#define SIGXFSZ         25
#define SIGVTALRM       26
#define SIGPROF         27
#define SIGWINCH        28
#define SIGIO           29
#define SIGPOLL         SIGIO
#define SIGPWR          30
#define SIGSYS          31
#define SIGUNUSED       31

/* sigprocmask how values - from Linux UAPI asm-generic/signal-defs.h */
#define SIG_BLOCK       0
#define SIG_UNBLOCK     1
#define SIG_SETMASK     2

/* ============================================================================
 * POLL CONSTANTS - from Linux UAPI asm-generic/poll.h
 * ============================================================================ */
#define POLLIN          0x0001
#define POLLPRI         0x0002
#define POLLOUT         0x0004
#define POLLERR         0x0008
#define POLLHUP         0x0010
#define POLLNVAL        0x0020
#define POLLRDNORM      0x0040
#define POLLRDBAND      0x0080
#define POLLWRNORM      0x0100
#define POLLWRBAND      0x0200

/* ============================================================================
 * EPOLL CONSTANTS - from Linux UAPI linux/eventpoll.h
 * ============================================================================ */
#define EPOLLIN         0x00000001
#define EPOLLPRI        0x00000002
#define EPOLLOUT        0x00000004
#define EPOLLERR        0x00000008
#define EPOLLHUP        0x00000010
#define EPOLLRDNORM     0x00000040
#define EPOLLRDBAND     0x00000080
#define EPOLLWRNORM     0x00000100
#define EPOLLWRBAND     0x00000200
#define EPOLLMSG        0x00000400
#define EPOLLRDHUP      0x00002000

#define EPOLL_CTL_ADD   1
#define EPOLL_CTL_DEL   2
#define EPOLL_CTL_MOD   3

#define EPOLL_CLOEXEC   02000000  /* O_CLOEXEC */

#endif /* IXLAND_LINUX_ABI_CONSTANTS_H */
