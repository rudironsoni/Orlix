/* IXLandSystemHostBridgeTests/PTYTestSupport.c
 * PTY test helpers placeholder
 *
 * Host bridge PTY tests are currently disabled pending
 * approved project helpers for path construction.
 *
 * This file exists as a placeholder for future implementation.
 */

#include "PTYTestSupport.h"

int ixland_test_pty_get_number(int master_fd, unsigned int *pty_number) {
    (void)master_fd;
    (void)pty_number;
    return -1;
}

int ixland_test_pty_unlock_slave(int master_fd) {
    (void)master_fd;
    return -1;
}

int ixland_test_pty_open_pair(int *master_fd, int *slave_fd) {
    if (!master_fd || !slave_fd) {
        return -1;
    }
    *master_fd = -1;
    *slave_fd = -1;
    return -1;
}

int ixland_test_tty_disassociate(int fd) {
    (void)fd;
    return -1;
}
