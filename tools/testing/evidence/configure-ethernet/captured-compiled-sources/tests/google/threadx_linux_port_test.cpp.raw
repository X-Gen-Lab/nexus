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
#include <semaphore.h>
#include <time.h>
extern "C" void* _tx_linux_timer_interrupt(void*);
extern "C" void tx_application_define(void*) {
}
namespace {
struct PortDone {};
::testing::StrictMock<::testing::MockFunction<void(clockid_t)>>* clocks;
::testing::StrictMock<::testing::MockFunction<void(const char*)>>* kernel;
struct timespec supplied, observed;
} /* namespace */
extern "C" int __wrap_nice(int) {
    return 0;
}
extern "C" int __wrap_sem_wait(sem_t*) {
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
