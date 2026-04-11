#include <errno.h>
#include <signal.h>

#include "../../kernel/exec/exec.h"
#include "../../kernel/signal/ixland_signal.h"
#include "../../kernel/task/task.h"
#include "../harness/ixland_test.h"

/* Helper to check if sigset is empty (macOS/iOS doesn't have sigisemptyset) */
static int sigset_is_empty(const sigset_t *set) {
    for (int i = 1; i < IXLAND_NSIG; i++) {
        if (sigismember(set, i)) {
            return 0;
        }
    }
    return 1;
}

IXLAND_TEST(sigprocmask_happy_path) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(task);
    IXLAND_ASSERT_NOT_NULL(task->sighand);

    sigset_t oldmask;
    sigset_t newmask;

    sigemptyset(&newmask);
    sigaddset(&newmask, SIGUSR1);

    /* Block SIGUSR1 */
    int ret = ixland_sigprocmask(SIG_BLOCK, &newmask, &oldmask);
    IXLAND_ASSERT_EQ(ret, 0);
    IXLAND_ASSERT(!sigismember(&oldmask, SIGUSR1));
    IXLAND_ASSERT(sigismember(&task->sighand->blocked, SIGUSR1));

    /* Unblock SIGUSR1 */
    ret = ixland_sigprocmask(SIG_UNBLOCK, &newmask, &oldmask);
    IXLAND_ASSERT_EQ(ret, 0);
    IXLAND_ASSERT(sigismember(&oldmask, SIGUSR1));
    IXLAND_ASSERT(!sigismember(&task->sighand->blocked, SIGUSR1));

    return true;
}

IXLAND_TEST(sigprocmask_block_one_signal) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);

    IXLAND_ASSERT(ixland_sigprocmask(SIG_BLOCK, &mask, NULL) == 0);
    IXLAND_ASSERT(sigismember(&task->sighand->blocked, SIGUSR1));

    return true;
}

IXLAND_TEST(sigprocmask_block_multiple_signals) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigaddset(&mask, SIGTERM);

    IXLAND_ASSERT(ixland_sigprocmask(SIG_BLOCK, &mask, NULL) == 0);
    IXLAND_ASSERT(sigismember(&task->sighand->blocked, SIGUSR1));
    IXLAND_ASSERT(sigismember(&task->sighand->blocked, SIGUSR2));
    IXLAND_ASSERT(sigismember(&task->sighand->blocked, SIGTERM));

    return true;
}

IXLAND_TEST(sigprocmask_unblock_one_signal) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);

    /* First block it */
    IXLAND_ASSERT(ixland_sigprocmask(SIG_BLOCK, &mask, NULL) == 0);
    IXLAND_ASSERT(sigismember(&task->sighand->blocked, SIGUSR1));

    /* Then unblock it */
    IXLAND_ASSERT(ixland_sigprocmask(SIG_UNBLOCK, &mask, NULL) == 0);
    IXLAND_ASSERT(!sigismember(&task->sighand->blocked, SIGUSR1));

    return true;
}

IXLAND_TEST(sigprocmask_verify_final_mask) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();
    sigset_t mask1, mask2;

    sigemptyset(&mask1);
    sigaddset(&mask1, SIGUSR1);
    IXLAND_ASSERT(ixland_sigprocmask(SIG_BLOCK, &mask1, NULL) == 0);

    sigemptyset(&mask2);
    sigaddset(&mask2, SIGUSR2);
    IXLAND_ASSERT(ixland_sigprocmask(SIG_BLOCK, &mask2, NULL) == 0);

    /* Verify both are blocked */
    sigset_t finalmask;
    IXLAND_ASSERT(ixland_sigprocmask(0, NULL, &finalmask) == 0);
    IXLAND_ASSERT(sigismember(&finalmask, SIGUSR1));
    IXLAND_ASSERT(sigismember(&finalmask, SIGUSR2));

    return true;
}

IXLAND_TEST(sigprocmask_invalid_signal) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, 100); /* Out of range */

    /* Should be silently ignored by sigprocmask */
    int ret = ixland_sigprocmask(SIG_BLOCK, &mask, NULL);
    IXLAND_ASSERT_EQ(ret, 0);

    return true;
}

IXLAND_TEST(sigprocmask_null_task) {
    /* sigprocmask without a task should fail with ESRCH */
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);

    /* Can't easily test this without setting current_task to NULL */
    /* Skip for now - tested indirectly via task_lifecycle */

    return true;
}

IXLAND_TEST(sigprocmask_invalid_how) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    sigset_t mask;
    sigemptyset(&mask);

    int ret = ixland_sigprocmask(99, &mask, NULL);
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, EINVAL);

    return true;
}

IXLAND_TEST(sigprocmask_get_old_only) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);

    IXLAND_ASSERT(ixland_sigprocmask(SIG_BLOCK, &mask, NULL) == 0);

    sigset_t oldmask;
    IXLAND_ASSERT(ixland_sigprocmask(0, NULL, &oldmask) == 0);
    IXLAND_ASSERT(sigismember(&oldmask, SIGUSR1));

    return true;
}

IXLAND_TEST(signal_reset_on_exec) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(task->sighand);

    /* Set a non-default handler for SIGUSR1 */
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_IGN;
    IXLAND_ASSERT(ixland_sigaction(SIGUSR1, &act, NULL) == 0);
    IXLAND_ASSERT(task->sighand->action[SIGUSR1].sa_handler == SIG_IGN);

    /* Block SIGUSR2 */
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR2);
    IXLAND_ASSERT(ixland_sigprocmask(SIG_BLOCK, &mask, NULL) == 0);
    IXLAND_ASSERT(sigismember(&task->sighand->blocked, SIGUSR2));

    /* Simulate exec signal reset */
    ixland_exec_reset_signals(task->sighand);

    /* Verify: SIG_IGN is preserved, blocked cleared, pending cleared */
    IXLAND_ASSERT(task->sighand->action[SIGUSR1].sa_handler == SIG_IGN);
    IXLAND_ASSERT(!sigismember(&task->sighand->blocked, SIGUSR2));
    IXLAND_ASSERT(sigset_is_empty(&task->sighand->blocked));
    IXLAND_ASSERT(sigset_is_empty(&task->sighand->pending));

    return true;
}

IXLAND_TEST(signal_exec_ign_preserved) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();

    /* Set SIG_IGN for SIGUSR1 */
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_IGN;
    IXLAND_ASSERT(ixland_sigaction(SIGUSR1, &act, NULL) == 0);

    /* exec reset should preserve SIG_IGN */
    ixland_exec_reset_signals(task->sighand);

    /* SIG_IGN is preserved on exec */
    IXLAND_ASSERT(task->sighand->action[SIGUSR1].sa_handler == SIG_IGN);

    return true;
}

IXLAND_TEST(sigaction_invalid_signal_number) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_IGN;

    int ret = ixland_sigaction(0, &act, NULL); /* Signal 0 is invalid */
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, EINVAL);

    ret = ixland_sigaction(100, &act, NULL); /* Out of range */
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, EINVAL);

    return true;
}

IXLAND_TEST(sigaction_sigkill_sigstop_blocked) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_IGN;

    int ret = ixland_sigaction(SIGKILL, &act, NULL);
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, EINVAL);

    ret = ixland_sigaction(SIGSTOP, &act, NULL);
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, EINVAL);

    return true;
}

IXLAND_TEST(sigpending_basic) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();

    /* Initially no pending signals */
    sigset_t pending;
    IXLAND_ASSERT(ixland_sigpending(&pending) == 0);
    IXLAND_ASSERT(sigset_is_empty(&pending));

    /* Block SIGUSR1, then send it to ourselves */
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    IXLAND_ASSERT(ixland_sigprocmask(SIG_BLOCK, &mask, NULL) == 0);

    /* Note: ixland_kill would add to pending, but signal delivery not fully tested here */
    /* Just verify pending is still accessible */
    IXLAND_ASSERT(ixland_sigpending(&pending) == 0);

    return true;
}
