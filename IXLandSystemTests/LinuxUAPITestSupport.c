/* IXLandSystemTests/LinuxUAPITestSupport.c
 * Semantic test helpers for Linux UAPI-sensitive assertions
 *
 * This file implements semantic test helpers that internally use vendored
 * Linux UAPI headers with canonical names (S_ISDIR, TIOCNOTTY, SIGINT, etc.).
 *
 * The Objective-C tests call behavior-level helpers, not renamed constants.
 */

/* Forward declare host APIs to avoid Darwin header contamination */
extern int ioctl(int, unsigned long, ...);
extern int open(const char *, int, ...);
extern int close(int);
extern int sigaction(int, const void *, void *);
extern int sigprocmask(int, const void *, void *);
extern int sigemptyset(void *);
extern int sigaddset(void *, int);

/* Include vendored Linux UAPI headers - canonical names used internally */
#include <linux/stat.h>
#include <asm-generic/ioctls.h>
#include <asm-generic/signal.h>
#include <asm-generic/termbits.h>

/* ============================================================================
 * Stat mode semantic test helpers
 * ============================================================================ */

int ixland_test_uapi_mode_is_directory(unsigned int mode) {
    return S_ISDIR(mode);
}

int ixland_test_uapi_mode_is_symlink(unsigned int mode) {
    return S_ISLNK(mode);
}

int ixland_test_uapi_mode_is_regular(unsigned int mode) {
    return S_ISREG(mode);
}

int ixland_test_uapi_mode_is_char_device(unsigned int mode) {
    return S_ISCHR(mode);
}

int ixland_test_uapi_mode_is_block_device(unsigned int mode) {
    return S_ISBLK(mode);
}

int ixland_test_uapi_mode_is_fifo(unsigned int mode) {
    return S_ISFIFO(mode);
}

/* ============================================================================
 * Signal semantic test helpers
 * ============================================================================ */

static struct {
    int valid;
    void *old_handler;
} sigint_state = {0, 0};

int ixland_test_signal_install_sigint_ign(void) {
    struct {
        void *sa_handler;
        unsigned long sa_flags;
        void *sa_restorer;
        unsigned char sa_mask[128];
    } new_sa, old_sa;

    new_sa.sa_handler = (void *)1; /* SIG_IGN = 1 */
    new_sa.sa_flags = 0;
    new_sa.sa_restorer = 0;

    /* Clear sigset_t */
    for (int i = 0; i < 128; i++) {
        new_sa.sa_mask[i] = 0;
    }

    if (sigaction(SIGINT, &new_sa, &old_sa) != 0) {
        return -1;
    }

    sigint_state.valid = 1;
    sigint_state.old_handler = old_sa.sa_handler;
    return 0;
}

int ixland_test_signal_restore_sigint(void) {
    struct {
        void *sa_handler;
        unsigned long sa_flags;
        void *sa_restorer;
        unsigned char sa_mask[128];
    } new_sa;

    if (!sigint_state.valid) {
        return -1;
    }

    new_sa.sa_handler = sigint_state.old_handler;
    new_sa.sa_flags = 0;
    new_sa.sa_restorer = 0;
    for (int i = 0; i < 128; i++) {
        new_sa.sa_mask[i] = 0;
    }

    return sigaction(SIGINT, &new_sa, 0);
}

static struct {
    int valid;
    unsigned char old_mask[128];
} mask_state = {0, {0}};

int ixland_test_signal_block_sigint(void) {
    unsigned char set[128], oldset[128];

    for (int i = 0; i < 128; i++) {
        set[i] = 0;
        oldset[i] = 0;
    }

    /* sigaddset for SIGINT (signal 2) */
    set[0] |= (1 << (SIGINT - 1));

    if (sigprocmask(SIG_BLOCK, set, oldset) != 0) {
        return -1;
    }

    mask_state.valid = 1;
    for (int i = 0; i < 128; i++) {
        mask_state.old_mask[i] = oldset[i];
    }

    return 0;
}

int ixland_test_signal_restore_mask(void) {
    if (!mask_state.valid) {
        return -1;
    }

    return sigprocmask(SIG_SETMASK, mask_state.old_mask, 0);
}

/* ============================================================================
 * PTY test helpers
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

    master = open("/dev/ptmx", 2 /* O_RDWR */);
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

    /* Build slave path */
    slave_path[0] = '/';
    slave_path[1] = 'd';
    slave_path[2] = 'e';
    slave_path[3] = 'v';
    slave_path[4] = '/';
    slave_path[5] = 'p';
    slave_path[6] = 't';
    slave_path[7] = 's';
    slave_path[8] = '/';

    /* Convert pty_number to string (simple case for 0-999) */
    if (pty_number < 10) {
        slave_path[9] = '0' + pty_number;
        slave_path[10] = '\0';
    } else if (pty_number < 100) {
        slave_path[9] = '0' + (pty_number / 10);
        slave_path[10] = '0' + (pty_number % 10);
        slave_path[11] = '\0';
    } else {
        slave_path[9] = '0' + (pty_number / 100);
        slave_path[10] = '0' + ((pty_number / 10) % 10);
        slave_path[11] = '0' + (pty_number % 10);
        slave_path[12] = '\0';
    }

    *slave_fd = open(slave_path, 2 /* O_RDWR */);
    if (*slave_fd < 0) {
        close(master);
        return -1;
    }

    *master_fd = master;
    return 0;
}

/* ============================================================================
 * TTY ioctl helpers
 * ============================================================================ */

int ixland_test_tty_disassociate(int fd) {
    return ioctl(fd, TIOCNOTTY, 0);
}

/* ============================================================================
 * Termios semantic test helpers
 * ============================================================================ */

int ixland_test_termios_has_isig(unsigned int lflag) {
    return (lflag & ISIG) != 0;
}

int ixland_test_termios_has_icanon(unsigned int lflag) {
    return (lflag & ICANON) != 0;
}

int ixland_test_termios_has_echo(unsigned int lflag) {
    return (lflag & ECHO) != 0;
}

int ixland_test_termios_has_tostop(unsigned int lflag) {
    return (lflag & TOSTOP) != 0;
}

int ixland_test_termios_cc_vmin_index(void) {
    return VMIN;
}

int ixland_test_termios_cc_vtime_index(void) {
    return VTIME;
}
