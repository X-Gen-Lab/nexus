/**
 * \file            threadx_m4_test.cpp
 * \brief           Actual port mechanism with mocked external boundaries
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "m4_model.h"
#include "nexus/os/threadx.h"
#include <array>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
extern "C" {
void* _tx_initialize_unused_memory;
void _tx_initialize_low_level(void);
}
namespace {
::testing::StrictMock<::testing::MockFunction<void()>>* mask;
std::array<uint32_t, 4> registers;
unsigned register_reads;
} /* namespace */
extern "C" volatile uint32_t* nx_threadx_m4_model_register(uintptr_t address) {
    ++register_reads;
    switch (address) {
        case 0xe000e010:
            return &registers[0];
        case 0xe000e014:
            return &registers[1];
        case 0xe000e018:
            return &registers[2];
        case 0xe000ed20:
            return &registers[3];
        default:
            std::abort();
    }
}
extern "C" void nx_threadx_m4_model_disable(void) {
    mask->Call();
}
extern "C" void nx_threadx_fail_stop(nx_result_t) {
    std::abort();
}
TEST(ThreadXM4, ActualClockConfiguresOnlyOwnedTickAndPendSVFields) {
    ::testing::StrictMock<::testing::MockFunction<void()>> masking;
    mask = &masking;
    registers = {0xdeadbeef, 0x1234, 99, 0x87651234};
    register_reads = 0;
    _tx_initialize_unused_memory = reinterpret_cast<void*>(0x20010000);
    EXPECT_CALL(masking, Call());
    _tx_initialize_low_level();
    EXPECT_EQ(_tx_initialize_unused_memory, nullptr);
    EXPECT_EQ(registers[0], 7U);
    EXPECT_EQ(registers[1], 167999U);
    EXPECT_EQ(registers[2], 0U);
    EXPECT_EQ(registers[3], 0x50f01234U);
    EXPECT_GT(register_reads, 0U);
}
