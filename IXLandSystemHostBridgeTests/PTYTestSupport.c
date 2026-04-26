/* IXLandSystemTests/PTYTestSupport.c
 * PTY test helpers using Linux UAPI ioctl definitions
 *
 * This file uses Linux UAPI headers for PTY ioctls.
 * Darwin syscalls are forward-declared.
 */

/* Linux UAPI ioctl definitions */
#include <asm-generic/ioctls.h>

/* Forward declare Darwin syscalls */
extern int ioctl(int, unsigned long, ...);
extern int open(const char *, int, ...);
extern int close(int);
extern int snprintf(char *, unsigned long, const char *, ...);

/* ============================================================================
 * PTY test helpers (using Linux UAPI ioctl values on Darwin host)
 * ============================================================================ */

int ixland_test_pty_get_number(int master_fd, unsigned int *pty_number) {
    return ioctl(master_fd, TIOCGPTN, pty_number);
}

int ixland_test_pty_unlock_slave(int master_fd) {
    int unlock = 0;
    return ioctl(master_fd, TIOCSPTLCK, &unlock);
}

int ixland_test_pty_open_pair(int *master_fd, int *slave_fd) {
    int master;
    unsigned int pty_number = 0;
    char slave_path[64];

    if (!master_fd || !slave_fd) {
        return -1;
    }

    /* O_RDWR = 2 on both Linux and Darwin */
    master = open("/dev/ptmx", 2);
    if (master < 0) {
        return -1;
    }

    if (ioctl(master, TIOCGPTN, &pty_number) != 0) {
        close(master);
        return -1;
    }

    if (ixland_test_pty_unlock_slave(master) != 0) {
        close(master);
        return -1;
    }

    /* Build slave path: /dev/pts/<n> */
    snprintf(slave_path, sizeof(slave_path), "/dev/pts/%u", pty_number);

    *slave_fd = open(slave_path, 2);
    if (*slave_fd < 0) {
        close(master);
        return -1;
    }

    *master_fd = master;
    return 0;
}

int ixland_test_tty_disassociate(int fd) {
    return ioctl(fd, TIOCNOTTY, 0);
}
