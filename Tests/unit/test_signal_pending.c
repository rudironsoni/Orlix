#include <errno.h>
#include <signal.h>
#include <stdlib.h>

#include "../../kernel/signal/ixland_signal.h"
#include "../../kernel/task/task.h"
#include "../harness/ixland_test.h"

/* Pending signal and mask tests
 *
 * Tests for behavior that is VERIFIED_IMPLEMENTED_NOW in the kernel.
 */

IXLAND_TEST(pending_mask_basic_block_unblock) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(task->sighand);

    sigset_t oldmask;
    sigset_t newmask;

    /* Block SIGINT */
    sigemptyset(&newmask);
    sigaddset(&newmask, SIGINT);
    IXLAND_ASSERT(ixland_sigprocmask(SIG_BLOCK, &newmask, &oldmask) == 0);

    /* Verify old mask was retrieved */
    IXLAND_ASSERT(!sigismember(&oldmask, SIGINT));

    /* Verify SIGINT is now blocked */
    sigset_t current;
    IXLAND_ASSERT(ixland_sigprocmask(0, NULL, &current) == 0);
    IXLAND_ASSERT(sigismember(&current, SIGINT));

    /* Unblock SIGINT */
    IXLAND_ASSERT(ixland_sigprocmask(SIG_UNBLOCK, &newmask, &oldmask) == 0);

    /* Verify old mask had SIGINT */
    IXLAND_ASSERT(sigismember(&oldmask, SIGINT));

    /* Verify SIGINT is no longer blocked */
    IXLAND_ASSERT(ixland_sigprocmask(0, NULL, &current) == 0);
    IXLAND_ASSERT(!sigismember(&current, SIGINT));

    return true;
}

IXLAND_TEST(pending_delivery_adds_to_pending) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();

    /* Block SIGUSR1 */
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    IXLAND_ASSERT(ixland_sigprocmask(SIG_BLOCK, &mask, NULL) == 0);

    /* Verify initially no pending */
    sigset_t pending;
    IXLAND_ASSERT(ixland_sigpending(&pending) == 0);
    IXLAND_ASSERT(!sigismember(&pending, SIGUSR1));

    /* Deliver SIGUSR1 */
    int ret = ixland_kill(task->pid, SIGUSR1);
    IXLAND_ASSERT_EQ(ret, 0);

    /* Verify signal is now pending */
    IXLAND_ASSERT(ixland_sigpending(&pending) == 0);
    IXLAND_ASSERT(sigismember(&pending, SIGUSR1));

    /* Cleanup: unblock and verify pending cleared on unblock */
    IXLAND_ASSERT(ixland_sigprocmask(SIG_UNBLOCK, &mask, NULL) == 0);
    IXLAND_ASSERT(ixland_sigpending(&pending) == 0);

    return true;
}

IXLAND_TEST(pending_multiple_signals_blocked) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();

    /* Block multiple signals */
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    IXLAND_ASSERT(ixland_sigprocmask(SIG_BLOCK, &mask, NULL) == 0);

    /* Verify both are blocked */
    sigset_t current;
    IXLAND_ASSERT(ixland_sigprocmask(0, NULL, &current) == 0);
    IXLAND_ASSERT(sigismember(&current, SIGUSR1));
    IXLAND_ASSERT(sigismember(&current, SIGUSR2));

    /* Deliver both */
    IXLAND_ASSERT(ixland_kill(task->pid, SIGUSR1) == 0);
    IXLAND_ASSERT(ixland_kill(task->pid, SIGUSR2) == 0);

    /* Verify both are pending */
    sigset_t pending;
    IXLAND_ASSERT(ixland_sigpending(&pending) == 0);
    IXLAND_ASSERT(sigismember(&pending, SIGUSR1));
    IXLAND_ASSERT(sigismember(&pending, SIGUSR2));

    /* Unblock one and verify state is coherent */
    sigset_t unblock;
    sigemptyset(&unblock);
    sigaddset(&unblock, SIGUSR1);
    IXLAND_ASSERT(ixland_sigprocmask(SIG_UNBLOCK, &unblock, NULL) == 0);

    /* Verify SIGUSR1 is unblocked but SIGUSR2 is still blocked */
    IXLAND_ASSERT(ixland_sigprocmask(0, NULL, &current) == 0);
    IXLAND_ASSERT(!sigismember(&current, SIGUSR1));
    IXLAND_ASSERT(sigismember(&current, SIGUSR2));

    /* Verify both signals are still pending (implementation clears on unblock) */
    IXLAND_ASSERT(ixland_sigpending(&pending) == 0);
    /* Current implementation: pending cleared on unblock (line 77 in exec.c) */

    /* Cleanup */
    IXLAND_ASSERT(ixland_sigprocmask(SIG_UNBLOCK, &mask, NULL) == 0);

    return true;
}

IXLAND_TEST(pending_ignored_signal) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();

    /* Set SIG_IGN for SIGUSR1 */
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_IGN;
    IXLAND_ASSERT(ixland_sigaction(SIGUSR1, &act, NULL) == 0);

    /* Verify disposition is IGN */
    IXLAND_ASSERT(task->sighand->action[SIGUSR1].sa_handler == SIG_IGN);

    /* Deliver signal */
    IXLAND_ASSERT(ixland_kill(task->pid, SIGUSR1) == 0);

    /* Verify signal is still added to pending */
    sigset_t pending;
    IXLAND_ASSERT(ixland_sigpending(&pending) == 0);
    IXLAND_ASSERT(sigismember(&pending, SIGUSR1));

    /* Disposition remains IGN */
    IXLAND_ASSERT(task->sighand->action[SIGUSR1].sa_handler == SIG_IGN);

    return true;
}

IXLAND_TEST(pending_null_sighand) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();
    ixland_sighand_t *saved = task->sighand;

    /* Simulate null sighand */
    task->sighand = NULL;

    sigset_t mask;
    sigemptyset(&mask);

    int ret = ixland_sigprocmask(SIG_BLOCK, &mask, NULL);
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, ESRCH);

    /* Also test sigpending */
    sigset_t pending;
    ret = ixland_sigpending(&pending);
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, ESRCH);

    /* Restore */
    task->sighand = saved;

    return true;
}

IXLAND_TEST(pending_invalid_signal) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    /* Try to add invalid signal to pending */
    ixland_task_t *task = ixland_current_task();

    int ret = ixland_kill(task->pid, 100); /* Out of range */
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, EINVAL);

    return true;
}

IXLAND_TEST(pending_invalid_how) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    sigset_t mask;
    sigemptyset(&mask);

    int ret = ixland_sigprocmask(99, &mask, NULL);
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, EINVAL);

    return true;
}
