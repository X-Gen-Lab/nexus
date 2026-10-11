/**
 * \file            threadx_m4_external_test.cpp
 * \brief           Actual port mechanism with mocked external boundaries
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "m4_model.h"
#include "nexus/os/threadx.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
extern "C" {
void* _tx_initialize_unused_memory;
void _tx_initialize_low_level(void);
}
namespace {
struct Stop {};
::testing::StrictMock<::testing::MockFunction<void()>>* mask;
::testing::StrictMock<::testing::MockFunction<nx_result_t(uint32_t)>>* timer;
::testing::StrictMock<::testing::MockFunction<void(nx_result_t)>>* stop;
uint32_t priorities;
} /* namespace */
extern "C" volatile uint32_t* nx_threadx_m4_model_register(uintptr_t address) {
    if (address != 0xe000ed20)
        std::abort();
    return &priorities;
}
extern "C" void nx_threadx_m4_model_disable(void) {
    mask->Call();
}
extern "C" nx_result_t nx_threadx_external_tick_setup(uint32_t hz) {
    return timer->Call(hz);
}
extern "C" void nx_threadx_fail_stop(nx_result_t reason) {
    stop->Call(reason);
    throw Stop{};
}
TEST(ThreadXM4External,
     ActualExternalHookLeavesSysTickAndOtherPrioritiesOwned) {
    ::testing::StrictMock<::testing::MockFunction<void()>> masking;
    ::testing::StrictMock<::testing::MockFunction<nx_result_t(uint32_t)>>
        timing;
    mask = &masking;
    timer = &timing;
    priorities = 0x87651234;
    EXPECT_CALL(masking, Call());
    EXPECT_CALL(timing, Call(1000)).WillOnce(::testing::Return(NX_SUCCESS));
    _tx_initialize_low_level();
    EXPECT_EQ(priorities, 0x87f01234U);
    EXPECT_EQ(_tx_initialize_unused_memory, nullptr);
}
TEST(ThreadXM4External, TimerSetupFailureCallsRetainingProductFailStop) {
    ::testing::StrictMock<::testing::MockFunction<void()>> masking;
    ::testing::StrictMock<::testing::MockFunction<nx_result_t(uint32_t)>>
        timing;
    ::testing::StrictMock<::testing::MockFunction<void(nx_result_t)>> stopping;
    mask = &masking;
    timer = &timing;
    stop = &stopping;
    priorities = 0x87651234;
    EXPECT_CALL(masking, Call());
    EXPECT_CALL(timing, Call(1000)).WillOnce(::testing::Return(NX_ERROR_IO));
    EXPECT_CALL(stopping, Call(NX_ERROR_IO));
    EXPECT_THROW(_tx_initialize_low_level(), Stop);
    EXPECT_EQ(priorities, 0x87f01234U);
}
