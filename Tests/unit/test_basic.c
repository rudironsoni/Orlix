/* Basic test for libixland
 * Tests fundamental syscalls
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main() {
    printf("=== libixland Basic Test ===\n\n");

    /* Test getpid/getppid */
    printf("Testing getpid/getppid...\n");
    pid_t pid = getpid();
    pid_t ppid = getppid();
    printf("  PID: %d\n", pid);
    printf("  PPID: %d\n", ppid);
    printf("  ✓ Passed\n\n");

    /* Test getcwd */
    printf("Testing getcwd...\n");
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd))) {
        printf("  CWD: %s\n", cwd);
        printf("  ✓ Passed\n\n");
    } else {
        printf("  ✗ Failed: %s\n\n", strerror(errno));
    }

    /* Test open/write/close */
    printf("Testing open/write/close...\n");
    int fd = open("/tmp/ixland_test.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd >= 0) {
        const char *msg = "Hello from libixland!\n";
        ssize_t written = write(fd, msg, strlen(msg));
        if (written == (ssize_t)strlen(msg)) {
            printf("  Wrote %zd bytes\n", written);
            printf("  ✓ Passed\n\n");
        } else {
            printf("  ✗ Failed to write: %s\n\n", strerror(errno));
        }
        close(fd);
    } else {
        printf("  ✗ Failed to open: %s\n\n", strerror(errno));
    }

    /* Test read */
    printf("Testing open/read/close...\n");
    fd = open("/tmp/ixland_test.txt", O_RDONLY);
    if (fd >= 0) {
        char buf[256];
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            printf("  Read: %s", buf);
            printf("  ✓ Passed\n\n");
        } else {
            printf("  ✗ Failed to read: %s\n\n", strerror(errno));
        }
        close(fd);
    } else {
        printf("  ✗ Failed to open: %s\n\n", strerror(errno));
    }

    /* Test fork (thread-based simulation) */
    printf("Testing fork (thread simulation)...\n");
    pid = fork();
    if (pid < 0) {
        printf("  ✗ Fork failed: %s\n\n", strerror(errno));
    } else if (pid == 0) {
        /* Child */
        printf("  Child process: PID=%d\n", getpid());
        exit(0);
    } else {
        /* Parent */
        printf("  Parent process, child PID=%d\n", pid);
        int status;
        pid_t waited = waitpid(pid, &status, 0);
        if (waited == pid) {
            printf("  Child exited with status %d\n", WEXITSTATUS(status));
            printf("  ✓ Passed\n\n");
        } else {
            printf("  ✗ Wait failed: %s\n\n", strerror(errno));
        }
    }

    printf("=== Test Complete ===\n");
    return 0;
}
