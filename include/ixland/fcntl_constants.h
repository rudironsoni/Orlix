/* include/ixland/fcntl_constants.h
 * Linux fcntl constants without struct definitions
 *
 * This header provides only the Linux UAPI constant values for fcntl flags,
 * avoiding the struct flock conflict with Darwin headers.
 */

#ifndef IXLAND_FCNTL_CONSTANTS_H
#define IXLAND_FCNTL_CONSTANTS_H

/* File access modes */
#ifndef O_RDONLY
#define O_RDONLY        00000000
#endif

#ifndef O_WRONLY
#define O_WRONLY        00000001
#endif

#ifndef O_RDWR
#define O_RDWR          00000002
#endif

#ifndef O_ACCMODE
#define O_ACCMODE       00000003
#endif

/* File creation flags */
#ifndef O_CREAT
#define O_CREAT         00000100
#endif

#ifndef O_EXCL
#define O_EXCL          00000200
#endif

#ifndef O_NOCTTY
#define O_NOCTTY        00000400
#endif

#ifndef O_TRUNC
#define O_TRUNC         00001000
#endif

/* File status flags */
#ifndef O_APPEND
#define O_APPEND        00002000
#endif

#ifndef O_NONBLOCK
#define O_NONBLOCK      00004000
#endif

#ifndef O_DSYNC
#define O_DSYNC         00010000
#endif

#ifndef O_DIRECT
#define O_DIRECT        00040000
#endif

#ifndef O_LARGEFILE
#define O_LARGEFILE     00100000
#endif

#ifndef O_DIRECTORY
#define O_DIRECTORY     00200000
#endif

#ifndef O_NOFOLLOW
#define O_NOFOLLOW      00400000
#endif

#ifndef O_NOATIME
#define O_NOATIME       01000000
#endif

#ifndef O_CLOEXEC
#define O_CLOEXEC       02000000
#endif

/* fcntl file descriptor flags */
#ifndef FD_CLOEXEC
#define FD_CLOEXEC      1
#endif

/* fcntl commands - minimal set */
#ifndef F_DUPFD
#define F_DUPFD         0
#endif

#ifndef F_GETFD
#define F_GETFD         1
#endif

#ifndef F_SETFD
#define F_SETFD         2
#endif

#ifndef F_GETFL
#define F_GETFL         3
#endif

#ifndef F_SETFL
#define F_SETFL         4
#endif

#ifndef F_DUPFD_CLOEXEC
#define F_DUPFD_CLOEXEC 1030
#endif

/* SEEK constants */
#ifndef SEEK_SET
#define SEEK_SET        0
#endif

#ifndef SEEK_CUR
#define SEEK_CUR        1
#endif

#ifndef SEEK_END
#define SEEK_END        2
#endif

#endif /* IXLAND_FCNTL_CONSTANTS_H */
