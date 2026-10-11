/**
 * \file            threadx_linux_port_test.cpp
 * \brief           Actual port mechanism with mocked external boundaries
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "tx_api.h"
#include <cerrno>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mutex>
#include <semaphore.h>
#include <time.h>
extern "C" void* _tx_linux_timer_interrupt(void*);
extern "C" void _tx_linux_thread_suspend_handler(int);
extern "C" void _tx_linux_thread_init();
extern "C" void tx_application_define(void*) {
}
namespace {
struct PortDone {};
::testing::StrictMock<::testing::MockFunction<void(clockid_t)>>* clocks;
::testing::StrictMock<::testing::MockFunction<void(const char*)>>* kernel;
struct timespec supplied, observed;
unsigned suspend_calls, post_calls;
bool reenter_suspend;
int nested_errno;
sigset_t resume_wait_mask;
thread_local volatile sig_atomic_t immediate_resume;
thread_local volatile sig_atomic_t immediate_resume_sent;
std::once_flag signals_initialized;
void initialize_signals() {
    std::call_once(signals_initialized, _tx_linux_thread_init);
}
} /* namespace */
extern "C" int __real_sem_post(sem_t*);
extern "C" int __real_sigwait(const sigset_t*, int*);
extern "C" int __wrap_nice(int) {
    return 0;
}
extern "C" int __wrap_sem_wait(sem_t*) {
    return 0;
}
extern "C" int __wrap_sem_post(sem_t* semaphore) {
    if (immediate_resume) {
        const int result = __real_sem_post(semaphore);
        /* Queue a real resume before the production handler starts its wait.
         * Its full handler mask must retain the signal until consumption. */
        if (result != 0 || pthread_kill(pthread_self(), SIGUSR2) != 0) {
            std::_Exit(75);
        }
        immediate_resume_sent = 1;
        return result;
    }
    ++post_calls;
    errno = ERANGE;
    return 0;
}
extern "C" int __wrap_sigsuspend(const sigset_t* mask) {
    resume_wait_mask = *mask;
    ++suspend_calls;
    if (reenter_suspend) {
        errno = EDOM;
        _tx_linux_thread_suspend_handler(SIGUSR1);
        nested_errno = errno;
    }
    errno = EINTR;
    return -1;
}
extern "C" int __wrap_sigwait(const sigset_t* mask, int* signal) {
    if (immediate_resume) {
        return __real_sigwait(mask, signal);
    }
    resume_wait_mask = *mask;
    ++suspend_calls;
    if (reenter_suspend) {
        errno = EDOM;
        _tx_linux_thread_suspend_handler(SIGUSR1);
        nested_errno = errno;
    }
    *signal = SIGUSR2;
    errno = EINTR;
    return 0;
}
extern "C" int __wrap_clock_gettime(clockid_t clock, struct timespec* now) {
    clocks->Call(clock);
    *now = supplied;
    return 0;
}
extern "C" int __wrap_sem_timedwait(sem_t*, const struct timespec* deadline) {
    clocks->Call(CLOCK_REALTIME);
    observed = *deadline;
    errno = ETIMEDOUT;
    return -1;
}
extern "C" int __wrap_sem_clockwait(sem_t*, clockid_t clock,
                                    const struct timespec* deadline) {
    clocks->Call(clock);
    observed = *deadline;
    errno = ETIMEDOUT;
    return -1;
}
extern "C" void __wrap__tx_thread_context_save() {
    kernel->Call("save");
}
extern "C" void __wrap__tx_timer_interrupt() {
    kernel->Call("tick");
}
extern "C" void __wrap__tx_thread_context_restore() {
    kernel->Call("restore");
    throw PortDone{};
}
TEST(ThreadXLinuxPort, ProductionTimerUsesOneMonotonicDeadlineDomain) {
    ::testing::StrictMock<::testing::MockFunction<void(clockid_t)>> clock_calls;
    ::testing::StrictMock<::testing::MockFunction<void(const char*)>>
        context_calls;
    clocks = &clock_calls;
    kernel = &context_calls;
    supplied = {100, 250000000};
    observed = {};
    EXPECT_CALL(clock_calls, Call(CLOCK_MONOTONIC)).Times(2);
    ::testing::InSequence ordered;
    EXPECT_CALL(context_calls, Call("save"));
    EXPECT_CALL(context_calls, Call("tick"));
    EXPECT_CALL(context_calls, Call("restore"));
    EXPECT_THROW(_tx_linux_timer_interrupt(nullptr), PortDone);
    EXPECT_EQ(observed.tv_sec, 100);
    EXPECT_EQ(observed.tv_nsec, 251000000);
}
TEST(ThreadXLinuxPort, ExactNanosecondCarryCannotProduceInvalidTimespec) {
    ::testing::StrictMock<::testing::MockFunction<void(clockid_t)>> clock_calls;
    ::testing::StrictMock<::testing::MockFunction<void(const char*)>>
        context_calls;
    clocks = &clock_calls;
    kernel = &context_calls;
    supplied = {100, 999000000};
    observed = {};
    EXPECT_CALL(clock_calls, Call(CLOCK_MONOTONIC)).Times(2);
    ::testing::InSequence ordered;
    EXPECT_CALL(context_calls, Call("save"));
    EXPECT_CALL(context_calls, Call("tick"));
    EXPECT_CALL(context_calls, Call("restore"));
    EXPECT_THROW(_tx_linux_timer_interrupt(nullptr), PortDone);
    EXPECT_EQ(observed.tv_sec, 101);
    EXPECT_EQ(observed.tv_nsec, 0);
}
TEST(ThreadXLinuxPort, SuspendSignalPreservesInterruptedCallerErrno) {
    suspend_calls = post_calls = 0;
    reenter_suspend = false;
    errno = ENOENT;
    _tx_linux_thread_suspend_handler(SIGUSR1);
    const int returned_errno = errno;
    EXPECT_EQ(returned_errno, ENOENT);
    EXPECT_EQ(post_calls, 1u);
    EXPECT_EQ(suspend_calls, 1u);
}
TEST(ThreadXLinuxPort, SuspendConsumesOnlyBlockedResumeSignal) {
    initialize_signals();
    suspend_calls = post_calls = 0;
    reenter_suspend = false;
    _tx_linux_thread_suspend_handler(SIGUSR1);
    EXPECT_EQ(sigismember(&resume_wait_mask, SIGUSR2), 1);
    EXPECT_EQ(sigismember(&resume_wait_mask, SIGUSR1), 0);
    EXPECT_EQ(sigismember(&resume_wait_mask, SIGTERM), 0);
    EXPECT_EQ(post_calls, 1u);
    EXPECT_EQ(suspend_calls, 1u);
}
TEST(ThreadXLinuxPort, RealPendingResumeBeforeWaitPreservesCompleteMask) {
    initialize_signals();
    sigset_t before{}, after{};
    ASSERT_EQ(pthread_sigmask(SIG_SETMASK, nullptr, &before), 0);
    immediate_resume_sent = 0;
    immediate_resume = 1;
    const int sent = pthread_kill(pthread_self(), SIGUSR1);
    const int observed = pthread_sigmask(SIG_SETMASK, nullptr, &after);
    immediate_resume = 0;
    ASSERT_EQ(sent, 0);
    ASSERT_EQ(observed, 0);
    EXPECT_EQ(immediate_resume_sent, 1);
    for (int signal = 1; signal < NSIG; ++signal) {
        SCOPED_TRACE(signal);
        EXPECT_EQ(sigismember(&after, signal), sigismember(&before, signal));
    }
}
TEST(ThreadXLinuxPort, NestedSuspendPreservesErrnoBeforeEarlyReturn) {
    suspend_calls = post_calls = 0;
    reenter_suspend = true;
    nested_errno = 0;
    errno = EACCES;
    _tx_linux_thread_suspend_handler(SIGUSR1);
    const int returned_errno = errno;
    EXPECT_EQ(returned_errno, EACCES);
    EXPECT_EQ(nested_errno, EDOM);
    EXPECT_EQ(post_calls, 2u);
    EXPECT_EQ(suspend_calls, 1u);
}
