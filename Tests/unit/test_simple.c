/* Simple test for libixland */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main() {
    printf("Test 1: getpid\n");
    pid_t pid = getpid();
    printf("  PID: %d\n", pid);

    printf("\nTest 2: getcwd\n");
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd))) {
        printf("  CWD: %s\n", cwd);
    } else {
        printf("  Error: %s\n", strerror(errno));
    }

    printf("\nTest 3: open/write\n");
    int fd = open("/tmp/test.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd >= 0) {
        const char *msg = "Hello\n";
        write(fd, msg, strlen(msg));
        close(fd);
        printf("  Written successfully\n");
    } else {
        printf("  Error: %s\n", strerror(errno));
    }

    printf("\nAll tests passed!\n");
    return 0;
}
