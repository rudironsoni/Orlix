#ifndef IXLAND_TTY_H
#define IXLAND_TTY_H

#include <pthread.h>
#include <stdbool.h>
#include <termios.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IXLAND_MAX_PTS 64

typedef struct ixland_tty ixland_tty_t;
typedef struct ixland_pty ixland_pty_t;

struct ixland_tty {
    int tty_id;
    pid_t foreground_pgrp;
    struct termios termios;
    struct winsize winsize;
    bool is_session_leader;
    atomic_int refs;
    pthread_mutex_t lock;
};

struct ixland_pty {
    int master_fd;
    int slave_fd;
    char slave_name[64];
    pid_t session_leader;
    ixland_tty_t *tty;
    atomic_int refs;
};

ixland_tty_t *ixland_tty_alloc(int id);
void ixland_tty_free(ixland_tty_t *tty);
int ixland_tty_set_foreground(ixland_tty_t *tty, pid_t pgrp);
pid_t ixland_tty_get_foreground(ixland_tty_t *tty);

ixland_pty_t *ixland_pty_open(void);
void ixland_pty_close(ixland_pty_t *pty);
int ixland_pty_master_read(ixland_pty_t *pty, void *buf, size_t count);
int ixland_pty_master_write(ixland_pty_t *pty, const void *buf, size_t count);
int ixland_pty_slave_read(ixland_pty_t *pty, void *buf, size_t count);
int ixland_pty_slave_write(ixland_pty_t *pty, const void *buf, size_t count);
int ixland_pty_set_size(ixland_pty_t *pty, int rows, int cols);

int ixland_tcgetattr(int fd, struct termios *termios_p);
int ixland_tcsetattr(int fd, int optional_actions, const struct termios *termios_p);
int ixland_tcgetpgrp(int fd);
int ixland_tcsetpgrp(int fd, pid_t pgrp);
int ixland_tcflush(int fd, int queue_selector);
int ixland_tcdrain(int fd);
int ixland_tcsendbreak(int fd, int duration);
int ixland_tcdrain(int fd);

int ixland_tty_init(void);
void ixland_tty_deinit(void);

#ifdef __cplusplus
}
#endif

#endif
