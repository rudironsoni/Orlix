/* IXLandSystemTests/LinuxUAPITestSupport.h
 * Semantic test helpers for Linux UAPI-sensitive assertions
 *
 * This header declares semantic test helpers that internally use vendored
 * Linux UAPI headers with canonical names. The Objective-C tests call
 * behavior-level helpers, not renamed constants.
 *
 * FORBIDDEN in this file:
 * - Accessor functions that just return a constant (e.g., linux_tcgets())
 * - Renamed Linux UAPI constants (e.g., ixland_test_sigint())
 *
 * ALLOWED in this file:
 * - Semantic test helpers that express behavior or assertions
 * - Helper functions that encapsulate Linux UAPI operations
 */

#ifndef LINUX_UAPI_TEST_SUPPORT_H
#define LINUX_UAPI_TEST_SUPPORT_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Stat mode semantic test helpers
 * ============================================================================ */

/* Returns non-zero if mode represents a directory */
int ixland_test_uapi_mode_is_directory(unsigned int mode);

/* Returns non-zero if mode represents a symlink */
int ixland_test_uapi_mode_is_symlink(unsigned int mode);

/* Returns non-zero if mode represents a regular file */
int ixland_test_uapi_mode_is_regular(unsigned int mode);

/* Returns non-zero if mode represents a character device */
int ixland_test_uapi_mode_is_char_device(unsigned int mode);

/* Returns non-zero if mode represents a block device */
int ixland_test_uapi_mode_is_block_device(unsigned int mode);

/* Returns non-zero if mode represents a FIFO */
int ixland_test_uapi_mode_is_fifo(unsigned int mode);

/* ============================================================================
 * Signal semantic test helpers
 * ============================================================================ */

/* Install SIG_IGN handler for SIGINT, returns 0 on success, -1 on error */
int ixland_test_signal_install_sigint_ign(void);

/* Restore SIGINT handler to previous disposition, returns 0 on success */
int ixland_test_signal_restore_sigint(void);

/* Block SIGINT, returns 0 on success, -1 on error */
int ixland_test_signal_block_sigint(void);

/* Restore signal mask from previous blocked state, returns 0 on success */
int ixland_test_signal_restore_mask(void);

/* ============================================================================
 * PTY test helpers
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

/* ============================================================================
 * Termios semantic test helpers
 * ============================================================================ */

/* Returns non-zero if lflag has ISIG set */
int ixland_test_termios_has_isig(unsigned int lflag);

/* Returns non-zero if lflag has ICANON set */
int ixland_test_termios_has_icanon(unsigned int lflag);

/* Returns non-zero if lflag has ECHO set */
int ixland_test_termios_has_echo(unsigned int lflag);

/* Returns non-zero if lflag has TOSTOP set */
int ixland_test_termios_has_tostop(unsigned int lflag);

/* Returns the VMIN index for c_cc array */
int ixland_test_termios_cc_vmin_index(void);

/* Returns the VTIME index for c_cc array */
int ixland_test_termios_cc_vtime_index(void);

#ifdef __cplusplus
}
#endif

#endif /* LINUX_UAPI_TEST_SUPPORT_H */
