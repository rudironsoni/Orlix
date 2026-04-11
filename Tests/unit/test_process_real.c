/* Real process test - verifies proper implementation
 * This tests actual functionality, not stubs
 */

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* Need to include internal header to test __ixland functions */
#include "ixland_internal.h"

static int test_count = 0;
static int pass_count = 0;

#define TEST(name) printf("\nTest %d: %s\n", ++test_count, name)
#define PASS()                \
    do {                      \
        printf("  [PASS]\n"); \
        pass_count++;         \
    } while (0)
#define FAIL(msg)                     \
    do {                              \
        printf("  [FAIL] %s\n", msg); \
    } while (0)

void test_pid_allocation(void) {
    TEST("PID Allocation");

    pid_t p1 = __ixland_alloc_pid();
    if (p1 < IXLAND_MIN_PID) {
        FAIL("PID below minimum");
        return;
    }

    pid_t p2 = __ixland_alloc_pid();
    if (p2 != p1 + 1) {
        FAIL("PIDs not sequential");
        __ixland_free_pid(p1);
        __ixland_free_pid(p2);
        return;
    }

    __ixland_free_pid(p1);
    __ixland_free_pid(p2);
    PASS();
}

void test_process_create(void) {
    TEST("Process Creation");

    __ixland_process_t *proc = __ixland_process_create("test", 0);
    if (!proc) {
        FAIL("Failed to create process");
        return;
    }

    if (proc->pid < IXLAND_MIN_PID) {
        FAIL("Invalid PID");
        __ixland_process_destroy(proc);
        return;
    }

    if (strcmp(proc->name, "test") != 0) {
        FAIL("Name not set correctly");
        __ixland_process_destroy(proc);
        return;
    }

    __ixland_process_destroy(proc);
    PASS();
}

void test_process_lookup(void) {
    TEST("Process Table Lookup");

    __ixland_process_t *proc = __ixland_process_create("lookup", 0);
    if (!proc) {
        FAIL("Failed to create process");
        return;
    }

    pid_t pid = proc->pid;

    __ixland_process_t *found = __ixland_get_process(pid);
    if (!found) {
        FAIL("Failed to lookup process");
        __ixland_process_destroy(proc);
        return;
    }

    if (found->pid != pid) {
        FAIL("Wrong process returned");
        __ixland_process_destroy(proc);
        return;
    }

    __ixland_process_destroy(proc);
    PASS();
}

void test_signal_queue(void) {
    TEST("Signal Queue");

    __ixland_sigqueue_t queue;
    memset(&queue, 0, sizeof(queue));
    pthread_mutex_init(&queue.lock, NULL);

    /* Queue some signals */
    siginfo_t info;
    memset(&info, 0, sizeof(info));
    info.si_signo = SIGUSR1;

    int ret = __ixland_sigqueue_enqueue(&queue, SIGUSR1, &info);
    if (ret != 0) {
        FAIL("Failed to enqueue signal");
        pthread_mutex_destroy(&queue.lock);
        return;
    }

    if (queue.count != 1) {
        FAIL("Queue count wrong");
        __ixland_sigqueue_flush(&queue);
        pthread_mutex_destroy(&queue.lock);
        return;
    }

    __ixland_sigqueue_flush(&queue);
    pthread_mutex_destroy(&queue.lock);
    PASS();
}

void test_fork_simulation(void) {
    TEST("Fork Simulation");

    pid_t pid = ixland_fork();
    if (pid < 0) {
        FAIL("Fork failed");
        return;
    }

    if (pid == 0) {
        /* Child process */
        printf("    Child: PID=%d\n", ixland_getpid());
        ixland_exit(42);
    } else {
        /* Parent process */
        printf("    Parent: Child PID=%d\n", pid);

        int status;
        pid_t waited = ixland_waitpid(pid, &status, 0);

        if (waited != pid) {
            FAIL("Wait returned wrong PID");
            return;
        }

        if (!WIFEXITED(status)) {
            FAIL("Child did not exit normally");
            return;
        }

        if (WEXITSTATUS(status) != 42) {
            FAIL("Wrong exit status");
            return;
        }

        PASS();
    }
}

void test_process_groups(void) {
    TEST("Process Groups");

    pid_t pid = ixland_fork();
    if (pid < 0) {
        FAIL("Fork failed");
        return;
    }

    if (pid == 0) {
        /* Child - create new process group */
        pid_t new_pgid = ixland_getpid();
        if (ixland_setpgid(0, new_pgid) < 0) {
            printf("    [CHILD] setpgid failed\n");
            ixland_exit(1);
        }

        pid_t pgid = ixland_getpgid(0);
        if (pgid != new_pgid) {
            printf("    [CHILD] pgid wrong: expected %d, got %d\n", new_pgid, pgid);
            ixland_exit(1);
        }

        ixland_exit(0);
    } else {
        /* Parent */
        int status;
        ixland_waitpid(pid, &status, 0);

        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            FAIL("Child failed process group test");
            return;
        }

        PASS();
    }
}

void test_setsid_creates_new_session(void) {
    TEST("setsid creates new session");

    pid_t pid = ixland_fork();
    if (pid < 0) {
        FAIL("Fork failed");
        return;
    }

    if (pid == 0) {
        /* Child - get current session and pgid before setsid */
        pid_t before_sid = ixland_getsid(0);
        pid_t before_pgid = ixland_getpgrp();
        pid_t my_pid = ixland_getpid();

        /* Child should inherit parent's session initially */
        if (before_sid == my_pid) {
            printf("    [CHILD] Already session leader before setsid\n");
            ixland_exit(1);
        }

        /* Create new session */
        pid_t new_sid = ixland_setsid();
        if (new_sid < 0) {
            printf("    [CHILD] setsid failed: errno=%d\n", errno);
            ixland_exit(1);
        }

        /* Verify setsid returned our PID */
        if (new_sid != my_pid) {
            printf("    [CHILD] setsid returned wrong sid: expected %d, got %d\n", my_pid, new_sid);
            ixland_exit(1);
        }

        /* Verify we are now session leader */
        pid_t after_sid = ixland_getsid(0);
        pid_t after_pgid = ixland_getpgrp();

        if (after_sid != my_pid) {
            printf("    [CHILD] sid wrong after setsid: expected %d, got %d\n", my_pid, after_sid);
            ixland_exit(1);
        }

        if (after_pgid != my_pid) {
            printf("    [CHILD] pgid wrong after setsid: expected %d, got %d\n", my_pid,
                   after_pgid);
            ixland_exit(1);
        }

        ixland_exit(0);
    } else {
        /* Parent */
        int status;
        ixland_waitpid(pid, &status, 0);

        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            FAIL("Child failed setsid test");
            return;
        }

        PASS();
    }
}

void test_setsid_fails_for_group_leader(void) {
    TEST("setsid fails with EPERM for group leader");

    pid_t pid = ixland_fork();
    if (pid < 0) {
        FAIL("Fork failed");
        return;
    }

    if (pid == 0) {
        /* Child - become process group leader */
        pid_t my_pid = ixland_getpid();

        if (ixland_setpgid(0, my_pid) < 0) {
            printf("    [CHILD] setpgid failed: errno=%d\n", errno);
            ixland_exit(1);
        }

        /* Verify we are now group leader */
        if (ixland_getpgrp() != my_pid) {
            printf("    [CHILD] Failed to become group leader\n");
            ixland_exit(1);
        }

        /* setsid should fail with EPERM since we're already group leader */
        pid_t result = ixland_setsid();
        if (result != -1) {
            printf("    [CHILD] setsid should have failed\n");
            ixland_exit(1);
        }

        if (errno != EPERM) {
            printf("    [CHILD] setsid failed with wrong errno: expected EPERM (%d), got %d\n",
                   EPERM, errno);
            ixland_exit(1);
        }

        ixland_exit(0);
    } else {
        /* Parent */
        int status;
        ixland_waitpid(pid, &status, 0);

        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            FAIL("Child failed EPERM test");
            return;
        }

        PASS();
    }
}

void test_getsid_returns_correct_session(void) {
    TEST("getsid returns correct session");

    pid_t parent_sid = ixland_getsid(0);
    pid_t parent_pgrp = ixland_getpgrp();

    pid_t pid = ixland_fork();
    if (pid < 0) {
        FAIL("Fork failed");
        return;
    }

    if (pid == 0) {
        /* Child - verify inherited session */
        pid_t child_sid = ixland_getsid(0);
        pid_t child_pgrp = ixland_getpgrp();

        /* Should inherit parent's session */
        if (child_sid != parent_sid) {
            printf("    [CHILD] sid mismatch: expected %d, got %d\n", parent_sid, child_sid);
            ixland_exit(1);
        }

        if (child_pgrp != parent_pgrp) {
            printf("    [CHILD] pgrp mismatch: expected %d, got %d\n", parent_pgrp, child_pgrp);
            ixland_exit(1);
        }

        /* Create new session */
        pid_t new_sid = ixland_setsid();
        if (new_sid < 0) {
            printf("    [CHILD] setsid failed\n");
            ixland_exit(1);
        }

        /* Verify getsid reflects new session */
        pid_t after_sid = ixland_getsid(0);
        if (after_sid != new_sid) {
            printf("    [CHILD] getsid mismatch after setsid: expected %d, got %d\n", new_sid,
                   after_sid);
            ixland_exit(1);
        }

        /* Verify getsid with explicit pid works */
        pid_t sid_by_pid = ixland_getsid(ixland_getpid());
        if (sid_by_pid != new_sid) {
            printf("    [CHILD] getsid(pid) mismatch: expected %d, got %d\n", new_sid, sid_by_pid);
            ixland_exit(1);
        }

        ixland_exit(0);
    } else {
        /* Parent */
        int status;
        ixland_waitpid(pid, &status, 0);

        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            FAIL("Child failed getsid test");
            return;
        }

        PASS();
    }
}

void test_getsid_fails_for_invalid_pid(void) {
    TEST("getsid fails with ESRCH for invalid pid");

    /* Try to get session for non-existent PID */
    pid_t result = ixland_getsid(99999);
    if (result != -1) {
        FAIL("getsid should have failed for invalid pid");
        return;
    }

    if (errno != ESRCH) {
        printf("  Wrong errno: expected ESRCH (%d), got %d\n", ESRCH, errno);
        FAIL("getsid failed with wrong errno");
        return;
    }

    PASS();
}

void test_setpgid_fails_cross_session(void) {
    TEST("setpgid fails with EPERM for cross-session move");

    pid_t pid = ixland_fork();
    if (pid < 0) {
        FAIL("Fork failed");
        return;
    }

    if (pid == 0) {
        /* Child - create new session */
        pid_t new_sid = ixland_setsid();
        if (new_sid < 0) {
            printf("    [CHILD] setsid failed\n");
            ixland_exit(1);
        }

        /* Now in different session from parent - setpgid to parent's pgrp should fail */
        pid_t parent_pgrp = getppid(); /* Parent's PID is its pgrp (usually) */

        int result = ixland_setpgid(0, parent_pgrp);
        if (result != -1) {
            printf("    [CHILD] setpgid should have failed for cross-session move\n");
            ixland_exit(1);
        }

        if (errno != EPERM) {
            printf("    [CHILD] setpgid failed with wrong errno: expected EPERM (%d), got %d\n",
                   EPERM, errno);
            ixland_exit(1);
        }

        ixland_exit(0);
    } else {
        /* Parent */
        int status;
        ixland_waitpid(pid, &status, 0);

        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            FAIL("Child failed cross-session EPERM test");
            return;
        }

        PASS();
    }
}

void test_session_in_fork_exec(void) {
    TEST("Session inheritance in fork/exec cycle");

    pid_t original_sid = ixland_getsid(0);

    pid_t pid = ixland_fork();
    if (pid < 0) {
        FAIL("Fork failed");
        return;
    }

    if (pid == 0) {
        /* Child - verify session inherited */
        pid_t child_sid = ixland_getsid(0);
        if (child_sid != original_sid) {
            printf("    [CHILD] Session not inherited: expected %d, got %d\n", original_sid,
                   child_sid);
            ixland_exit(1);
        }

        /* Create new session */
        pid_t new_sid = ixland_setsid();
        if (new_sid < 0) {
            printf("    [CHILD] setsid failed\n");
            ixland_exit(1);
        }

        /* Fork again to verify session inheritance works after setsid */
        pid_t grandchild = ixland_fork();
        if (grandchild < 0) {
            printf("    [CHILD] grandchild fork failed\n");
            ixland_exit(1);
        }

        if (grandchild == 0) {
            /* Grandchild - should inherit child's new session */
            pid_t gc_sid = ixland_getsid(0);
            if (gc_sid != new_sid) {
                printf("    [GRANDCHILD] Session not inherited: expected %d, got %d\n", new_sid,
                       gc_sid);
                ixland_exit(1);
            }
            ixland_exit(0);
        } else {
            /* Child waits for grandchild */
            int gc_status;
            ixland_waitpid(grandchild, &gc_status, 0);

            if (!WIFEXITED(gc_status) || WEXITSTATUS(gc_status) != 0) {
                printf("    [CHILD] Grandchild failed\n");
                ixland_exit(1);
            }
            ixland_exit(0);
        }
    } else {
        /* Parent */
        int status;
        ixland_waitpid(pid, &status, 0);

        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            FAIL("Child failed session inheritance test");
            return;
        }

        PASS();
    }
}

int main(void) {
    printf("=== libixland Process Implementation Test ===\n");
    printf("Testing production-quality process simulation\n\n");

    /* Initialize ixland subsystem */
    if (__ixland_init() != 0) {
        printf("FAILED: Could not initialize ixland\n");
        return 1;
    }

    test_pid_allocation();
    test_process_create();
    test_process_lookup();
    test_signal_queue();
    test_fork_simulation();
    test_process_groups();
    test_setsid_creates_new_session();
    test_setsid_fails_for_group_leader();
    test_getsid_returns_correct_session();
    test_getsid_fails_for_invalid_pid();
    test_setpgid_fails_cross_session();
    test_session_in_fork_exec();

    printf("\n=== Results ===\n");
    printf("Passed: %d/%d\n", pass_count, test_count);

    if (pass_count == test_count) {
        printf("\nAll tests PASSED!\n");
        return 0;
    } else {
        printf("\nSome tests FAILED!\n");
        return 1;
    }
}
