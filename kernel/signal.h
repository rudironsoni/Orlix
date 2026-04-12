#ifndef IXLAND_SIGNAL_INTERNAL_H
#define IXLAND_SIGNAL_INTERNAL_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <signal.h>
#include <stdbool.h>
#include <time.h>

#include "task.h"

/* Include public signal types */
#include "../include/ixland/ixland_signal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Signal queue entry */
typedef struct ixland_sigqueue_entry {
    int sig;
    siginfo_t info;
    struct ixland_sigqueue_entry *next;
} ixland_sigqueue_entry_t;

/* Signal queue */
typedef struct ixland_sigqueue {
    ixland_sigqueue_entry_t *head;
    ixland_sigqueue_entry_t *tail;
    int count;
    pthread_mutex_t lock;
} ixland_sigqueue_t;

/* Signal handling state */
struct ixland_sighand {
    struct sigaction action[IXLAND_NSIG];
    sigset_t blocked;
    sigset_t pending;
    ixland_sigqueue_t queue;
    atomic_int refs;
};

ixland_sighand_t *ixland_sighand_alloc(void);
void ixland_sighand_free(ixland_sighand_t *sighand);
ixland_sighand_t *ixland_sighand_dup(ixland_sighand_t *parent);

int ixland_sigaction(int sig, const struct sigaction *act, struct sigaction *oldact);
int ixland_kill(pid_t pid, int sig);
int ixland_killpg(pid_t pgrp, int sig);
int ixland_sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
int ixland_sigpending(sigset_t *set);
int ixland_sigsuspend(const sigset_t *mask);
int ixland_sigqueue(pid_t pid, int sig, const union sigval value);
int ixland_sigtimedwait(const sigset_t *set, siginfo_t *info, const struct timespec *timeout);

/* Signal handler installation (simplified interface) */
typedef void (*ixland_sighandler_t)(int);
ixland_sighandler_t ixland_signal(int signum, ixland_sighandler_t handler);

/* Signal to current process */
int ixland_raise(int sig);

/* Alarm timer */
unsigned int ixland_alarm(unsigned int seconds);

/* Wait for signal */
int ixland_pause(void);

void ixland_signal_init(void);
void ixland_signal_deinit(void);

#ifdef __cplusplus
}
#endif

#endif
