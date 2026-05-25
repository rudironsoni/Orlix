#define _XOPEN_SOURCE 700

#include "OrlixHostAdapter/runtime/trap.h"
#include "OrlixHostAdapter/runtime/host_tls.h"

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <mach/mach.h>
#include <mach/thread_act.h>
#include <pthread.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

#define ORLIX_HOST_USER_TIMER_NS 1000000ULL
#define ORLIX_ARM_ESR_EC_MASK 0xfc000000U
#define ORLIX_ARM_ESR_EC_SHIFT 26U
#define ORLIX_ARM_ESR_EC_IABT_LOWER 0x20U
#define ORLIX_ARM_ESR_EC_IABT_CURRENT 0x21U
#define ORLIX_ARM_ESR_EC_DABT_LOWER 0x24U
#define ORLIX_ARM_ESR_EC_DABT_CURRENT 0x25U
#define ORLIX_ARM_ESR_DABT_WNR (1U << 6)

struct OrlixHostUserTrapState {
    unsigned long user_base;
    unsigned long user_limit;
    const unsigned long *kernel_sp;
    unsigned long *user_active;
    orlix_host_user_trap_entry_t entry;
    thread_act_t target_thread;
    unsigned long host_tls;
};

static struct OrlixHostUserTrapState OrlixHostUserTrap;
static struct orlix_host_user_trap_frame OrlixHostUserTrapFrame;
static struct orlix_host_user_trap_frame OrlixHostUserResumeFrame;
static orlix_host_user_resume_entry_t OrlixHostUserResumeEntry;
static atomic_bool OrlixHostUserResumePending;
static bool OrlixHostUserTrapInstalled;
static bool OrlixHostUserTimerStarted;
static unsigned long long OrlixHostUserTimerPeriodNs = ORLIX_HOST_USER_TIMER_NS;

static bool OrlixHostUserTrapContains(unsigned long pc)
{
    return OrlixHostUserTrap.entry &&
           OrlixHostUserTrap.kernel_sp &&
           OrlixHostUserTrap.user_base < OrlixHostUserTrap.user_limit &&
           pc >= OrlixHostUserTrap.user_base &&
           pc < OrlixHostUserTrap.user_limit;
}

static bool OrlixHostUserTrapIsUserActive(void)
{
    return OrlixHostUserTrap.user_active &&
           __atomic_load_n(OrlixHostUserTrap.user_active, __ATOMIC_ACQUIRE);
}

static unsigned long OrlixHostReadTls(void)
{
    unsigned long tls;

    __asm__ volatile("mrs %0, tpidr_el0" : "=r"(tls));
    return tls;
}

static void OrlixHostWriteTls(unsigned long tls)
{
    __asm__ volatile(
        "msr tpidr_el0, %0\n"
        "isb\n"
        :
        : "r"(tls)
        : "memory");
}

unsigned long OrlixHostEnterHostTls(void)
{
    unsigned long active_tls = OrlixHostReadTls();

    if (OrlixHostUserTrap.host_tls && active_tls != OrlixHostUserTrap.host_tls) {
        OrlixHostWriteTls(OrlixHostUserTrap.host_tls);
    }
    return active_tls;
}

void OrlixHostLeaveHostTls(unsigned long active_tls)
{
    if (OrlixHostUserTrap.host_tls && active_tls != OrlixHostUserTrap.host_tls) {
        OrlixHostWriteTls(active_tls);
    }
}

static unsigned long OrlixHostUserFaultFlags(int signal_number,
                                             mcontext_t machine_context)
{
    unsigned int esr = machine_context->__es.__esr;
    unsigned int ec = (esr & ORLIX_ARM_ESR_EC_MASK) >> ORLIX_ARM_ESR_EC_SHIFT;
    unsigned long flags = 0;

    if (signal_number == SIGBUS) {
        flags |= ORLIX_HOST_USER_FAULT_BUS;
    }
    if (ec == ORLIX_ARM_ESR_EC_IABT_LOWER ||
        ec == ORLIX_ARM_ESR_EC_IABT_CURRENT) {
        flags |= ORLIX_HOST_USER_FAULT_EXEC;
    }
    if ((ec == ORLIX_ARM_ESR_EC_DABT_LOWER ||
         ec == ORLIX_ARM_ESR_EC_DABT_CURRENT) &&
        (esr & ORLIX_ARM_ESR_DABT_WNR)) {
        flags |= ORLIX_HOST_USER_FAULT_WRITE;
    }

    return flags;
}

static void OrlixHostUserTrapReraise(int signal_number)
{
    signal(signal_number, SIG_DFL);
    raise(signal_number);
}

static void OrlixHostUserTrapSaveFrame(mcontext_t machine_context)
{
    for (unsigned int index = 0; index < 29; index++) {
        OrlixHostUserTrapFrame.regs[index] = (unsigned long)machine_context->__ss.__x[index];
    }
    OrlixHostUserTrapFrame.regs[29] = (unsigned long)machine_context->__ss.__fp;
    OrlixHostUserTrapFrame.regs[30] = (unsigned long)machine_context->__ss.__lr;
    OrlixHostUserTrapFrame.sp = (unsigned long)machine_context->__ss.__sp;
    OrlixHostUserTrapFrame.pc = (unsigned long)machine_context->__ss.__pc;
    OrlixHostUserTrapFrame.pstate = (unsigned long)machine_context->__ss.__cpsr;
    OrlixHostUserTrapFrame.fault_address = 0;
    OrlixHostUserTrapFrame.fault_flags = 0;
    OrlixHostUserTrapFrame.user_tls = 0;
    OrlixHostUserTrapFrame.frame_flags = 0;
}

static void OrlixHostUserTrapSaveMachFrame(const arm_thread_state64_t *state)
{
    for (unsigned int index = 0; index < 29; index++) {
        OrlixHostUserTrapFrame.regs[index] = (unsigned long)state->__x[index];
    }
    OrlixHostUserTrapFrame.regs[29] = (unsigned long)state->__fp;
    OrlixHostUserTrapFrame.regs[30] = (unsigned long)state->__lr;
    OrlixHostUserTrapFrame.sp = (unsigned long)state->__sp;
    OrlixHostUserTrapFrame.pc = (unsigned long)state->__pc;
    OrlixHostUserTrapFrame.pstate = (unsigned long)state->__cpsr;
    OrlixHostUserTrapFrame.fault_address = 0;
    OrlixHostUserTrapFrame.fault_flags = 0;
    OrlixHostUserTrapFrame.user_tls = 0;
    OrlixHostUserTrapFrame.frame_flags = 0;
}

static void OrlixHostUserTrapSetFrameTls(unsigned long user_tls)
{
    OrlixHostUserTrapFrame.user_tls = user_tls;
    OrlixHostUserTrapFrame.frame_flags |= ORLIX_HOST_USER_FRAME_HAS_TLS;
}

__attribute__((visibility("hidden"), noinline, noreturn))
static void OrlixHostUserTimerEntry(
    int trap_number,
    struct orlix_host_user_trap_frame *frame,
    orlix_host_user_trap_entry_t entry)
{
    unsigned long user_tls = OrlixHostReadTls();

    OrlixHostWriteTls(OrlixHostUserTrap.host_tls);
    frame->user_tls = user_tls;
    frame->frame_flags |= ORLIX_HOST_USER_FRAME_HAS_TLS;
    entry(trap_number, frame);
    __builtin_unreachable();
}

static void OrlixHostUserTrapHandler(int signal_number,
                                     siginfo_t *info,
                                     void *context)
{
    ucontext_t *user_context = (ucontext_t *)context;
    mcontext_t machine_context;
    int trap_number = signal_number;
    unsigned long user_pc;
    unsigned long kernel_sp;
    unsigned long fault_address;
    unsigned long user_tls;

    if (!user_context || !user_context->uc_mcontext) {
        OrlixHostUserTrapReraise(signal_number);
        return;
    }

    machine_context = user_context->uc_mcontext;
    user_pc = (unsigned long)machine_context->__ss.__pc;
    if (!OrlixHostUserTrapContains(user_pc)) {
        OrlixHostUserTrapReraise(signal_number);
        return;
    }

    user_tls = OrlixHostReadTls();
    OrlixHostWriteTls(OrlixHostUserTrap.host_tls);
    __atomic_store_n(OrlixHostUserTrap.user_active, 0, __ATOMIC_RELEASE);

    kernel_sp = *OrlixHostUserTrap.kernel_sp;
    if (!kernel_sp) {
        _exit(128 + signal_number);
    }

    OrlixHostUserTrapSaveFrame(machine_context);
    fault_address = (unsigned long)machine_context->__es.__far;
    if (!fault_address && info) {
        fault_address = (unsigned long)info->si_addr;
    }
    OrlixHostUserTrapFrame.fault_address = fault_address;
    OrlixHostUserTrapFrame.fault_flags =
        OrlixHostUserFaultFlags(signal_number, machine_context);
    OrlixHostUserTrapSetFrameTls(user_tls);

    machine_context->__ss.__x[0] = (uint64_t)trap_number;
    machine_context->__ss.__x[1] = (uint64_t)&OrlixHostUserTrapFrame;
    machine_context->__ss.__pc = (uint64_t)OrlixHostUserTrap.entry;
    machine_context->__ss.__sp = (uint64_t)kernel_sp;
}

static void OrlixHostUserTimerSleep(unsigned long long delay_ns)
{
    struct timespec delay = {
        .tv_sec = (time_t)(delay_ns / 1000000000ULL),
        .tv_nsec = (long)(delay_ns % 1000000000ULL),
    };

    while (nanosleep(&delay, &delay) == -1) {
    }
}

static bool OrlixHostUserResumeRedirect(void)
{
    arm_thread_state64_t state;
    mach_msg_type_number_t count = ARM_THREAD_STATE64_COUNT;
    kern_return_t status;

    if (!atomic_load_explicit(&OrlixHostUserResumePending, memory_order_acquire)) {
        return false;
    }

    status = thread_suspend(OrlixHostUserTrap.target_thread);
    if (status != KERN_SUCCESS) {
        return false;
    }

    status = thread_get_state(OrlixHostUserTrap.target_thread,
                              ARM_THREAD_STATE64,
                              (thread_state_t)&state,
                              &count);
    if (status == KERN_SUCCESS && count == ARM_THREAD_STATE64_COUNT &&
        OrlixHostUserResumeEntry) {
        state.__x[0] = (uint64_t)&OrlixHostUserResumeFrame;
        state.__pc = (uint64_t)OrlixHostUserResumeEntry;
        status = thread_set_state(OrlixHostUserTrap.target_thread,
                                  ARM_THREAD_STATE64,
                                  (thread_state_t)&state,
                                  ARM_THREAD_STATE64_COUNT);
    }

    if (status == KERN_SUCCESS) {
        atomic_store_explicit(&OrlixHostUserResumePending, false,
                              memory_order_release);
    }

    (void)thread_resume(OrlixHostUserTrap.target_thread);
    return status == KERN_SUCCESS;
}

static bool OrlixHostUserTimerRedirect(arm_thread_state64_t *state)
{
    unsigned long user_pc = (unsigned long)state->__pc;
    unsigned long kernel_sp;

    if (!OrlixHostUserTrapContains(user_pc)) {
        return false;
    }

    __atomic_store_n(OrlixHostUserTrap.user_active, 0, __ATOMIC_RELEASE);

    kernel_sp = *OrlixHostUserTrap.kernel_sp;
    if (!kernel_sp) {
        return false;
    }

    OrlixHostUserTrapSaveMachFrame(state);
    state->__x[0] = (uint64_t)ORLIX_HOST_USER_TRAP_TIMER;
    state->__x[1] = (uint64_t)&OrlixHostUserTrapFrame;
    state->__x[2] = (uint64_t)OrlixHostUserTrap.entry;
    state->__pc = (uint64_t)OrlixHostUserTimerEntry;
    state->__sp = (uint64_t)kernel_sp;
    return true;
}

static void *OrlixHostUserTimerMain(void *context)
{
    (void)context;

    for (;;) {
        arm_thread_state64_t state;
        mach_msg_type_number_t count = ARM_THREAD_STATE64_COUNT;
        kern_return_t status;

        if (OrlixHostUserResumeRedirect()) {
            continue;
        }

        OrlixHostUserTimerSleep(OrlixHostUserTimerPeriodNs);
        if (OrlixHostUserTrap.entry) {
            if (!OrlixHostUserTrapIsUserActive()) {
                continue;
            }

            status = thread_suspend(OrlixHostUserTrap.target_thread);
            if (status != KERN_SUCCESS) {
                continue;
            }

            if (!OrlixHostUserTrapIsUserActive()) {
                (void)thread_resume(OrlixHostUserTrap.target_thread);
                continue;
            }

            status = thread_get_state(OrlixHostUserTrap.target_thread,
                                      ARM_THREAD_STATE64,
                                      (thread_state_t)&state,
                                      &count);
            if (status == KERN_SUCCESS && count == ARM_THREAD_STATE64_COUNT &&
                OrlixHostUserTimerRedirect(&state)) {
                status = thread_set_state(OrlixHostUserTrap.target_thread,
                                          ARM_THREAD_STATE64,
                                          (thread_state_t)&state,
                                          ARM_THREAD_STATE64_COUNT);
            }

            (void)thread_resume(OrlixHostUserTrap.target_thread);
        }
    }
}

__attribute__((visibility("hidden"))) int orlix_host_user_trap_install(
    unsigned long user_base,
    unsigned long user_limit,
    const unsigned long *kernel_sp,
    unsigned long *user_active,
    orlix_host_user_trap_entry_t entry)
{
    const int signals[] = { SIGTRAP, SIGILL, SIGBUS, SIGSEGV, SIGABRT };
    struct sigaction action;

    if (!kernel_sp || !user_active || !entry || user_base >= user_limit) {
        return -1;
    }

    OrlixHostUserTrap.user_base = user_base;
    OrlixHostUserTrap.user_limit = user_limit;
    OrlixHostUserTrap.kernel_sp = kernel_sp;
    OrlixHostUserTrap.user_active = user_active;
    OrlixHostUserTrap.entry = entry;
    OrlixHostUserTrap.target_thread = pthread_mach_thread_np(pthread_self());
    OrlixHostUserTrap.host_tls = OrlixHostReadTls();

    if (OrlixHostUserTrapInstalled) {
        return 0;
    }

    sigemptyset(&action.sa_mask);
    action.sa_sigaction = OrlixHostUserTrapHandler;
    action.sa_flags = SA_SIGINFO;
    for (unsigned int index = 0; index < sizeof(signals) / sizeof(signals[0]); index++) {
        if (sigaction(signals[index], &action, NULL) != 0) {
            return -1;
        }
    }

    OrlixHostUserTrapInstalled = true;
    return 0;
}

__attribute__((visibility("hidden"))) int orlix_host_user_trap_start_timer(unsigned long long period_ns)
{
    pthread_t timer_thread;
    unsigned long active_tls;
    int create_status;

    if (!OrlixHostUserTrapInstalled || OrlixHostUserTimerStarted) {
        return OrlixHostUserTrapInstalled ? 0 : -1;
    }

    if (period_ns) {
        OrlixHostUserTimerPeriodNs = period_ns;
    }

    active_tls = OrlixHostEnterHostTls();
    create_status = pthread_create(&timer_thread, NULL, OrlixHostUserTimerMain, NULL);
    OrlixHostLeaveHostTls(active_tls);

    if (create_status != 0) {
        return -1;
    }
    (void)pthread_detach(timer_thread);

    OrlixHostUserTimerStarted = true;
    return 0;
}

__attribute__((visibility("hidden"), noreturn)) void orlix_host_user_trap_resume(
    orlix_host_user_resume_entry_t resume_entry,
    const struct orlix_host_user_trap_frame *frame)
{
    (void)OrlixHostEnterHostTls();

    if (!resume_entry || !frame) {
        _exit(127);
    }

    OrlixHostUserResumeFrame = *frame;
    OrlixHostUserResumeEntry = resume_entry;
    atomic_store_explicit(&OrlixHostUserResumePending, true,
                          memory_order_release);

    while (atomic_load_explicit(&OrlixHostUserResumePending,
                                memory_order_acquire)) {
        __asm__ volatile("yield");
    }

    _exit(127);
}
