/* IXLandSystemHostBridgeTests/PTYTestSupport.h
 * PTY test helpers placeholder
 *
 * Host bridge PTY tests are currently disabled because proper PTY setup
 * requires snprintf and manual path construction which are forbidden.
 */

#ifndef PTY_TEST_SUPPORT_H
#define PTY_TEST_SUPPORT_H

#ifdef __cplusplus
extern "C" {
#endif

/* PTY test helpers - currently stubbed due to snprintf prohibition */
int ixland_test_pty_get_number(int master_fd, unsigned int *pty_number);
int ixland_test_pty_unlock_slave(int master_fd);
int ixland_test_pty_open_pair(int *master_fd, int *slave_fd);
int ixland_test_tty_disassociate(int fd);

#ifdef __cplusplus
}
#endif

#endif /* PTY_TEST_SUPPORT_H */
