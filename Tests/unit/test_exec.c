#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../fs/fdtable.h"
#include "../../kernel/exec/exec.h"
#include "../../kernel/task/task.h"
#include "../../runtime/native/registry.h"
#include "../harness/ixland_test.h"

typedef struct {
    ixland_task_t *task;
    int argc;
    char *argv[IXLAND_MAX_ARGS];
    char *envp[IXLAND_MAX_ARGS];
    int called;
} stub_capture_t;

static stub_capture_t g_capture = {0};

static void stub_capture_reset(void) {
    for (int i = 0; i < IXLAND_MAX_ARGS && g_capture.argv[i]; i++) {
        free(g_capture.argv[i]);
        g_capture.argv[i] = NULL;
    }

    for (int i = 0; i < IXLAND_MAX_ARGS && g_capture.envp[i]; i++) {
        free(g_capture.envp[i]);
        g_capture.envp[i] = NULL;
    }

    g_capture.task = NULL;
    g_capture.argc = 0;
    g_capture.called = 0;
}

static int test_stub_cmd(ixland_task_t *task, int argc, char **argv, char **envp) {
    stub_capture_reset();
    g_capture.task = task;
    g_capture.argc = argc;

    if (argv) {
        for (int i = 0; i < IXLAND_MAX_ARGS - 1 && argv[i]; i++) {
            g_capture.argv[i] = strdup(argv[i]);
        }
    }

    if (envp) {
        for (int i = 0; i < IXLAND_MAX_ARGS - 1 && envp[i]; i++) {
            g_capture.envp[i] = strdup(envp[i]);
        }
    }

    g_capture.called = 1;
    return 0;
}

IXLAND_TEST(exec_native_happy_path) {
    /* Initialize task system */
    IXLAND_ASSERT(ixland_task_init() == 0);

    /* Register stub command */
    IXLAND_ASSERT(ixland_native_register("/bin/testcmd", test_stub_cmd) == 0);

    /* Get current task */
    ixland_task_t *task = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(task);

    /* Capture pre-exec identity */
    pid_t pid_before = task->pid;
    pid_t pgid_before = task->pgid;
    pid_t sid_before = task->sid;

    /* Prepare args and env */
    char *argv[] = {"testcmd", "arg1", "arg2", NULL};
    char *envp[] = {"FOO=bar", NULL};

    /* Clear capture */
    memset(&g_capture, 0, sizeof(g_capture));

    /* Execute */
    int ret = ixland_execve("/bin/testcmd", argv, envp);

    /* Verify exec succeeded */
    IXLAND_ASSERT_EQ(ret, 0);
    IXLAND_ASSERT_EQ(g_capture.called, 1);

    /* Verify task identity preserved */
    IXLAND_ASSERT_EQ(g_capture.task, task);
    IXLAND_ASSERT_EQ(task->pid, pid_before);
    IXLAND_ASSERT_EQ(task->pgid, pgid_before);
    IXLAND_ASSERT_EQ(task->sid, sid_before);

    /* Verify exec image metadata updated */
    IXLAND_ASSERT_NOT_NULL(task->exec_image);
    IXLAND_ASSERT_EQ(task->exec_image->type, IXLAND_IMAGE_NATIVE);
    IXLAND_ASSERT_NOT_NULL(task->exec_image->path);
    IXLAND_ASSERT(strcmp(task->exec_image->path, "/bin/testcmd") == 0);

    /* Verify argv and env passed correctly */
    IXLAND_ASSERT_EQ(g_capture.argc, 3);
    IXLAND_ASSERT_NOT_NULL(g_capture.argv);
    IXLAND_ASSERT(strcmp(g_capture.argv[0], "testcmd") == 0);
    IXLAND_ASSERT(strcmp(g_capture.argv[1], "arg1") == 0);
    IXLAND_ASSERT(strcmp(g_capture.argv[2], "arg2") == 0);
    IXLAND_ASSERT_NOT_NULL(g_capture.envp);
    IXLAND_ASSERT(strcmp(g_capture.envp[0], "FOO=bar") == 0);

    /* Cleanup */
    ixland_native_registry_clear();
    return true;
}

IXLAND_TEST(exec_command_not_found) {
    /* Initialize */
    IXLAND_ASSERT(ixland_task_init() == 0);

    /* Try to execute unknown command */
    char *argv[] = {"unknown", NULL};
    int ret = ixland_execve("/bin/unknown", argv, NULL);

    /* Should fail with ENOENT */
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT(errno == ENOENT);

    return true;
}

IXLAND_TEST(exec_invalid_path) {
    /* Initialize */
    IXLAND_ASSERT(ixland_task_init() == 0);

    /* Try NULL path */
    int ret = ixland_execve(NULL, NULL, NULL);
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT(errno == EFAULT);

    return true;
}

IXLAND_TEST(exec_cloexec_behavior) {
    /* Initialize */
    IXLAND_ASSERT(ixland_task_init() == 0);
    ixland_task_t *task = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(task);
    IXLAND_ASSERT_NOT_NULL(task->files);

    /* Register stub command */
    IXLAND_ASSERT(ixland_native_register("/bin/cloexec_test", test_stub_cmd) == 0);

    /* Create a pipe to get two FDs */
    int pipe_fds[2];
    IXLAND_ASSERT(pipe(pipe_fds) == 0);

    /* Add both FDs to task's file table */
    ixland_file_t *file0 = ixland_file_alloc();
    ixland_file_t *file1 = ixland_file_alloc();
    IXLAND_ASSERT_NOT_NULL(file0);
    IXLAND_ASSERT_NOT_NULL(file1);

    file0->fd = pipe_fds[0];
    file1->fd = pipe_fds[1];

    /* Mark file1 as CLOEXEC */
    file1->flags = FD_CLOEXEC;

    /* Allocate in table (use high FD numbers to avoid stdin/stdout/stderr) */
    int fd0 = 10;
    int fd1 = 11;

    pthread_mutex_lock(&task->files->lock);
    if (fd0 < (int)task->files->max_fds) {
        task->files->fd[fd0] = file0;
        atomic_fetch_add(&file0->refs, 1);
    }
    if (fd1 < (int)task->files->max_fds) {
        task->files->fd[fd1] = file1;
        atomic_fetch_add(&file1->refs, 1);
    }
    pthread_mutex_unlock(&task->files->lock);

    /* Verify both FDs exist */
    IXLAND_ASSERT(task->files->fd[fd0] != NULL);
    IXLAND_ASSERT(task->files->fd[fd1] != NULL);

    /* Clear capture */
    memset(&g_capture, 0, sizeof(g_capture));

    /* Execute */
    char *argv[] = {"cloexec_test", NULL};
    int ret = ixland_execve("/bin/cloexec_test", argv, NULL);

    /* Verify exec succeeded */
    IXLAND_ASSERT_EQ(ret, 0);
    IXLAND_ASSERT_EQ(g_capture.called, 1);

    /* Verify non-CLOEXEC fd still exists */
    IXLAND_ASSERT(task->files->fd[fd0] != NULL);

    /* Verify CLOEXEC fd was closed */
    IXLAND_ASSERT(task->files->fd[fd1] == NULL);

    /* Cleanup */
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    ixland_native_registry_clear();
    return true;
}

IXLAND_TEST(exec_image_classification_wasm) {
    /* Create temp file with WASM magic */
    char path[] = "ixland_test_wasm_XXXXXX";
    int fd = mkstemp(path);
    IXLAND_ASSERT(fd >= 0);

    /* Write WASM magic: \0asm */
    unsigned char wasm_magic[] = {0x00, 0x61, 0x73, 0x6d};
    ssize_t written = write(fd, wasm_magic, 4);
    IXLAND_ASSERT(written == 4);

    /* Ensure data is written before classification reads it */
    fsync(fd);
    close(fd);

    /* Clear registry first so file magic takes precedence */
    ixland_native_registry_clear();

    /* Classify */
    ixland_image_type_t type = ixland_exec_classify(path);
    IXLAND_ASSERT_EQ(type, IXLAND_IMAGE_WASI);

    /* Cleanup */
    unlink(path);
    return true;
}

IXLAND_TEST(exec_image_classification_script) {
    /* Create temp file with shebang - use CWD for iOS simulator compatibility */
    char path[] = "ixland_test_script_XXXXXX";
    int fd = mkstemp(path);
    IXLAND_ASSERT(fd >= 0);

    IXLAND_ASSERT(write(fd, "#!/bin/sh\n", 10) == 10);
    fsync(fd);
    close(fd);

    /* Classify */
    ixland_image_type_t type = ixland_exec_classify(path);
    IXLAND_ASSERT_EQ(type, IXLAND_IMAGE_SCRIPT);

    /* Cleanup */
    unlink(path);
    return true;
}

IXLAND_TEST(exec_image_classification_native) {
    /* Initialize */
    IXLAND_ASSERT(ixland_task_init() == 0);

    /* Register a command */
    IXLAND_ASSERT(ixland_native_register("/bin/nativetest", test_stub_cmd) == 0);

    /* Create temp file (no special magic) */
    char path[] = "/tmp/ixland_test_native_XXXXXX";
    int fd = mkstemp(path);
    IXLAND_ASSERT(fd >= 0);
    IXLAND_ASSERT(write(fd, "binary content\n", 15) == 15);
    close(fd);

    /* Register this path */
    IXLAND_ASSERT(ixland_native_register(path, test_stub_cmd) == 0);

    /* Classify - should find in registry */
    ixland_image_type_t type = ixland_exec_classify(path);
    IXLAND_ASSERT_EQ(type, IXLAND_IMAGE_NATIVE);

    /* Cleanup */
    unlink(path);
    ixland_native_registry_clear();
    return true;
}

IXLAND_TEST(exec_image_classification_none) {
    /* Try non-existent file */
    ixland_image_type_t type = ixland_exec_classify("/nonexistent/path");
    IXLAND_ASSERT_EQ(type, IXLAND_IMAGE_NONE);

    return true;
}
