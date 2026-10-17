/**
 * \file            h743_flash_ram_generated_test.cpp
 * \brief           Normal TOML high-profile assembly and retained cleanup
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_flash_ram_closure_support.h"
#include "h743_flash_ram_high_model.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32h743_provider.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>
/* GNU link observation only: capture the real generated constructor's
 * arguments, establish the external hardware address map, and call the actual
 * implementation unchanged. No constructor result or branch is mocked. */
extern "C" nx_result_t
__real_nx_stm32h743_flash_ram_initialize(nx_stm32h743_flash_state_t*,
                                         nx_stm32h743_flash_ram_workspace_t*);
extern "C" nx_result_t __wrap_nx_stm32h743_flash_ram_initialize(
    nx_stm32h743_flash_state_t* port,
    nx_stm32h743_flash_ram_workspace_t* execution) {
    auto& cpu = nexus_test::h743_ram::cpu;
    cpu.execution = execution;
    cpu.owner_port = port;
    g_h743_model.physical_flash = port;
    return __real_nx_stm32h743_flash_ram_initialize(port, execution);
}
extern "C" {
USART_TypeDef g_h743_factory_uart;
uint32_t g_h743_high_factory_flash_words[524288U];
void USART3_IRQHandler(void);
}
class H743FlashRamGenerated : public testing::Test {
  protected:
    uint8_t tx_data = 0x5aU;
    nx_uart_tx_request_t request{};
    void SetUp() override {
        nx_h743_model_reset();
        nexus_test::h743_ram::ResetRamCpu(nullptr);
        g_h743_model.core->VTOR = nexus_test::h743_ram::cpu.original_vtor;
        nx_request_initialize(&request.base);
        PWR->D3CR = PWR_D3CR_VOS_0 | PWR_D3CR_VOSRDY;
        PWR->CSR1 = PWR_CSR1_ACTVOS_0 | PWR_CSR1_ACTVOSRDY;
        RCC->HSICFGR = 0x40000000U;
        RCC->PLLCKSELR = 0x02020200U;
        RCC->PLLCFGR = 0x01ff0000U;
        RCC->PLL1DIVR = 0x01010280U;
        SysTick->CTRL = 0U;
        SysTick->LOAD = SysTick->VAL = 0U;
        g_h743_clock_ctrl_reads = 0U;
        g_h743_clock_behavior = {};
        g_h743_high_behavior = {};
        std::memset(&g_h743_factory_uart, 0, sizeof(g_h743_factory_uart));
        g_h743_model.uart = USART3;
        USART3->ISR = USART_ISR_TXE_TXFNF | USART_ISR_TC;
        g_h743_model.pwm = TIM3;
        g_h743_model.spi = SPI4;
        SPI4->SR = SPI_SR_TXP | SPI_SR_TXC;
        g_h743_model.adc = ADC1;
        ADC1->CR = ADC2->CR = ADC_CR_DEEPPWD;
        ADC1->CFGR = ADC_CFGR_JQDIS;
        std::fill_n(g_h743_high_factory_flash_words, 524288U, UINT32_MAX);
    }
    void TearDown() override {
        auto& cpu = nexus_test::h743_ram::cpu;
        const uint32_t pending =
            (g_h743_model.flash.SR1 | g_h743_model.flash.SR2) &
            nexus_test::h743_ram::busy_flags;
        const bool active = cpu.execution != nullptr && cpu.execution->active;
        EXPECT_EQ(0U, pending);
        EXPECT_FALSE(active);
        if (pending != 0U || active ||
            (cpu.owner_port != nullptr && cpu.owner_port->active)) {
            std::abort();
        }
        /* Every intentional external hold is released in its own test, where
         * actual stop/retry is asserted. Teardown does not clear SR or loans.
         */
        const nx_result_t stopped = nx_platform_stop();
        EXPECT_EQ(NX_SUCCESS, stopped);
        if (stopped != NX_SUCCESS) {
            std::abort();
        }
        if (cpu.execution != nullptr) {
            EXPECT_FALSE(cpu.execution->prepared);
            EXPECT_FALSE(cpu.execution->active);
            if (cpu.execution->prepared || cpu.execution->active) {
                std::abort();
            }
        }
    }
    void Start() {
        const auto result = nx_platform_start();
        ASSERT_EQ(NX_SUCCESS, result.primary);
        ASSERT_EQ(NX_SUCCESS, result.cleanup);
        ASSERT_EQ(0U, result.remaining_effects);
        TIM2->CNT = 100U;
    }
};
TEST_F(H743FlashRamGenerated, NormalLookupIsColdAndStartCopiesSelectedFlash) {
    auto& cpu = nexus_test::h743_ram::cpu;
    const auto* flash = nx_factory_flash(NX_FLASH_ID_STORAGE);
    ASSERT_NE(nullptr, flash);
    EXPECT_EQ(0U, g_h743_model.reads);
    EXPECT_EQ(0U, g_h743_model.writes);
    EXPECT_EQ(0U, cpu.copy_writes);
    EXPECT_EQ(nullptr, cpu.execution);
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_NE(nullptr, cpu.execution);
    EXPECT_EQ(flash->context, cpu.owner_port);
    EXPECT_TRUE(cpu.execution->prepared);
    EXPECT_FALSE(cpu.execution->active);
    EXPECT_GT(cpu.copy_writes, 0U);
    EXPECT_EQ(cpu.code_source, cpu.code_run);
    EXPECT_EQ(0U, cpu.vtor_writes);
}
TEST_F(H743FlashRamGenerated, FailedPrepareAndBusyStopPreserveOrderedCleanup) {
    auto& cpu = nexus_test::h743_ram::cpu;
    cpu.corrupt_copy = true;
    const auto failed = nx_platform_start();
    EXPECT_EQ(NX_ERROR_IO, failed.primary);
    EXPECT_EQ(NX_SUCCESS, failed.cleanup);
    EXPECT_EQ(0U, failed.remaining_effects);
    EXPECT_FALSE(g_nx_stm32h743_system.started);
    EXPECT_EQ(64000000U, g_nx_stm32h743_system.core_hz);
    EXPECT_EQ(0U, g_h743_model.flash_store_count);
    cpu.corrupt_copy = false;
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_NE(nullptr, cpu.execution);
    ASSERT_TRUE(cpu.execution->prepared);
    g_h743_model.flash.SR2 = FLASH_SR_QW;
    EXPECT_EQ(NX_ERROR_BUSY, nx_platform_stop());
    EXPECT_TRUE(cpu.execution->prepared);
    EXPECT_TRUE(g_nx_stm32h743_system.started);
    EXPECT_EQ(384000000U, g_nx_stm32h743_system.core_hz);
    EXPECT_NE(0U, RCC->CR & RCC_CR_PLL1ON);
    g_h743_model.flash.SR2 = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_FALSE(cpu.execution->prepared);
    EXPECT_FALSE(g_nx_stm32h743_system.started);
    EXPECT_EQ(64000000U, g_nx_stm32h743_system.core_hz);
}
TEST_F(H743FlashRamGenerated, NormalPulsesAndRestartKeepTheOriginalCpuContext) {
    auto& cpu = nexus_test::h743_ram::cpu;
    std::array<uint8_t, 32U> data{};
    data.fill(0x5aU);
    for (unsigned iteration = 0U; iteration < 2U; ++iteration) {
        ASSERT_NO_FATAL_FAILURE(Start());
        ASSERT_NE(nullptr, cpu.execution);
        cpu.control = iteration == 0U ? 0U : 6U;
        cpu.msp = 0x2407f000U;
        cpu.psp = 0x2407e000U;
        const uint32_t control = cpu.control;
        const auto* flash = nx_factory_flash(NX_FLASH_ID_STORAGE);
        ASSERT_NE(nullptr, flash);
        ASSERT_EQ(NX_SUCCESS,
                  nx_flash_port_program(flash, 131072U + iteration * 32U,
                                        data.data(), data.size(), 10000U));
        EXPECT_EQ(0x2407f000U, cpu.msp);
        EXPECT_EQ(0x2407e000U, cpu.psp);
        EXPECT_EQ(control, cpu.control);
        EXPECT_EQ(0U, cpu.primask);
        EXPECT_EQ(cpu.original_vtor, g_h743_model.core->VTOR);
        EXPECT_FALSE(cpu.execution->active);
        EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
        EXPECT_FALSE(cpu.execution->prepared);
    }
    /* This is generated C/provider integration against a CPU port. The real
     * stack register transitions and FP-opcode absence still require the
     * separately linked ARM checker tests and the actual production ELF. */
}
