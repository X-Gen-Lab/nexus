/**
 * \file            h743_flash_fixture.cpp
 * \brief           External hardware prerequisites and checked RAM teardown
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_flash_fixture.h"
#include <cstdlib>
#include <gtest/gtest.h>
namespace nexus_test::h743_fixture {
void ResetExternalCpu() {
    h743_ram::ResetRamCpu(nullptr);
    h743_ram::cpu.msp = 0x2407f000U;
    h743_ram::cpu.psp = 0x2407e000U;
    g_h743_model.core->VTOR = h743_ram::cpu.original_vtor;
}
void SetIsolatedTimer(uint32_t input_hz) {
    /* These are explicit external hardware facts for existing isolated driver
     * fixtures. Real generated Platform/System initialization is not replaced.
     */
    ASSERT_NE(0U, input_hz);
    ASSERT_EQ(0U, input_hz % 1000000U);
    g_h743_model.rcc.APB1LENR |= RCC_APB1LENR_TIM2EN;
    g_h743_model.timer.PSC = input_hz / 1000000U - 1U;
    g_h743_model.timer.ARR = UINT32_MAX;
    g_h743_model.timer.CR1 = TIM_CR1_CEN;
    g_h743_model.timer.DIER = TIM_DIER_UIE;
    g_nx_stm32h743_system.remaining_effects |=
        NX_STM32H743_EFFECT_TIMER | NX_STM32H743_EFFECT_TIMER_GATE;
}
void StopIsolatedTimer() {
    const nx_result_t result =
        nx_stm32h743_system_stop(&g_nx_stm32h743_system, 8U);
    EXPECT_EQ(NX_SUCCESS, result);
    EXPECT_FALSE(g_nx_stm32h743_system.started);
    EXPECT_EQ(0U, g_nx_stm32h743_system.remaining_effects);
    if (result != NX_SUCCESS || g_nx_stm32h743_system.started ||
        g_nx_stm32h743_system.remaining_effects != 0U)
        std::abort();
}
nx_result_t Prepare(nx_stm32h743_flash_state_t* port,
                    nx_stm32h743_flash_ram_workspace_t* execution) {
    h743_ram::cpu.execution = execution;
    h743_ram::cpu.owner_port = port;
    h743_mpu::mpu.guard = &execution->guard;
    g_h743_model.physical_flash = port;
    return nx_stm32h743_flash_ram_initialize(port, execution);
}
void CheckedStop(nx_stm32h743_flash_ram_workspace_t* execution) {
    const uint32_t pending = (g_h743_model.flash.SR1 | g_h743_model.flash.SR2) &
                             h743_ram::busy_flags;
    const bool active = execution != nullptr && execution->active;
    const bool port_active =
        h743_ram::cpu.owner_port != nullptr && h743_ram::cpu.owner_port->active;
    const uint32_t effects =
        execution != nullptr ? execution->guard.remaining_effects : 0U;
    EXPECT_EQ(0U, pending);
    EXPECT_FALSE(active);
    EXPECT_FALSE(port_active);
    EXPECT_EQ(0U, effects);
    if (pending != 0U || active || port_active || effects != 0U)
        std::abort();
    if (execution != nullptr) {
        const nx_result_t result = nx_stm32h743_flash_ram_stop(execution);
        EXPECT_EQ(NX_SUCCESS, result);
        EXPECT_FALSE(execution->prepared);
        if (result != NX_SUCCESS || execution->prepared)
            std::abort();
    }
    h743_ram::cpu.execution = nullptr;
    h743_ram::cpu.owner_port = nullptr;
    h743_ram::cpu.observer = nullptr;
    h743_mpu::mpu.guard = nullptr;
}
} /* namespace nexus_test::h743_fixture */
