/* IXLandSystemTests/PTYTestSupport.h
 * PTY test helpers using Linux UAPI ioctl definitions
 *
 * This file uses Linux UAPI headers for PTY ioctls.
 * Darwin syscalls are forward-declared.
 */

#ifndef PTY_TEST_SUPPORT_H
#define PTY_TEST_SUPPORT_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * PTY test helpers (using Linux UAPI ioctl values on Darwin host)
 * ============================================================================ */

/* Open PTY master/slave pair, returns 0 on success, -1 on error */
int ixland_test_pty_open_pair(int *master_fd, int *slave_fd);

/* Get PTY number from master fd, returns 0 on success, -1 on error */
int ixland_test_pty_get_number(int master_fd, unsigned int *pty_number);

/* Unlock PTY slave, returns 0 on success, -1 on error */
int ixland_test_pty_unlock_slave(int master_fd);

/* ============================================================================
 * TTY ioctl helpers
 * ============================================================================ */

/* Disassociate controlling tty, returns 0 on success, -1 on error with errno set */
int ixland_test_tty_disassociate(int fd);

#ifdef __cplusplus
}
#endif

#endif /* PTY_TEST_SUPPORT_H */
