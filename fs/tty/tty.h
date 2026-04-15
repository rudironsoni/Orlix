#ifndef TTY_H
#define TTY_H

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <termios.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_PTS 64

typedef struct tty tty_t;
typedef struct pty pty_t;

struct tty {
int tty_id;
pid_t foreground_pgrp;
struct termios termios;
struct winsize winsize;
bool is_session_leader;
atomic_int refs;
pthread_mutex_t lock;
};

struct pty {
int master_fd;
int slave_fd;
char slave_name[64];
pid_t session_leader;
tty_t *tty;
atomic_int refs;
};

tty_t *tty_alloc(int id);
void tty_free(tty_t *tty);
int tty_set_foreground(tty_t *tty, pid_t pgrp);
pid_t tty_get_foreground(tty_t *tty);

pty_t *pty_open(void);
void pty_close(pty_t *pty);
int pty_master_read(pty_t *pty, void *buf, size_t count);
int pty_master_write(pty_t *pty, const void *buf, size_t count);
int pty_slave_read(pty_t *pty, void *buf, size_t count);
int pty_slave_write(pty_t *pty, const void *buf, size_t count);
int pty_set_size(pty_t *pty, int rows, int cols);

static int tcgetattr_impl(int fd, struct termios *termios_p);
static int tcsetattr_impl(int fd, int optional_actions, const struct termios *termios_p);
int tcgetpgrp(int fd);
int tcsetpgrp(int fd, pid_t pgrp);
int tcflush(int fd, int queue_selector);
int tcdrain(int fd);
int tcsendbreak(int fd, int duration);

__attribute__((visibility("default"))) int tcgetattr(int fd, struct termios *termios_p);
__attribute__((visibility("default"))) int tcsetattr(int fd, int optional_actions,
const struct termios *termios_p);

int tty_init(void);
void tty_deinit(void);

#ifdef __cplusplus
}
#endif

#endif
