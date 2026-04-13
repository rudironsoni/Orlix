/* iOS Subsystem for Linux - System Call Surface
 *
 * Minimal sys.c containing functions not in other canonical owners
 */

#include <errno.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../fs/exec.h"
#include "task.h"

/* extern declaration for environ (not available on all platforms) */
extern char **environ;

int ixland_setpgrp(void) {
    /* setpgrp() is equivalent to setpgid(0, 0) */
    return ixland_setpgid(0, 0);
}

int ixland_system(const char *command) {
    pid_t pid;
    int status;

    if (command == NULL) {
        return 1;
    }

    pid = ixland_fork();
    if (pid < 0) {
        return -1;
    }

    if (pid == 0) {
        /* Child process */
        const char *argv[] = {"sh", "-c", command, NULL};
        ixland_execve("/bin/sh", (char *const *)argv, environ);
        /* If exec fails, exit with error */
        ixland_exit(127);
        /* NOTREACHED */
    }

    /* Parent process - wait for child */
    pid_t ret = ixland_waitpid(pid, &status, 0);
    if (ret < 0) {
        return -1;
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }

    return -1;
}
