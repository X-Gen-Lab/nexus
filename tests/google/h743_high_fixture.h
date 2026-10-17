/**
 * \file            h743_high_fixture.h
 * \brief           Shared cold reference and isolated provider prerequisites
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_H743_HIGH_FIXTURE_H
#define NX_H743_HIGH_FIXTURE_H
#include "h743_high_profile_model.h"
#include "stm32h743_clock.h"
#include "stm32h743_provider.h"
#include <gtest/gtest.h>
class H743HighReference : public testing::Test {
  protected:
    nx_stm32h743_pll384_state_t owner{};
    bool isolated_provider_timebase = false;
    void SetUp() override {
        nx_h743_model_reset();
        g_h743_model.power.D3CR = PWR_D3CR_VOS_0 | PWR_D3CR_VOSRDY;
        g_h743_model.power.CSR1 = PWR_CSR1_ACTVOS_0 | PWR_CSR1_ACTVOSRDY;
        g_h743_model.rcc.HSICFGR = 0x40000000U;
        g_h743_model.rcc.PLLCKSELR = 0x02020200U;
        g_h743_model.rcc.PLLCFGR = 0x01ff0000U;
        g_h743_model.rcc.PLL1DIVR = 0x01010280U;
        SysTick->CTRL = 0U;
        SysTick->LOAD = SysTick->VAL = 0U;
        g_h743_clock_ctrl_reads = 0U;
        g_h743_clock_behavior = {};
        g_h743_high_behavior = {};
        /* RM0433 Rev9 50.11.6: actual cold SPI_SR resets to0x00001002. */
        g_h743_model.spi_registers.SR = SPI_SR_TXP | SPI_SR_TXC;
    }
    void TearDown() override {
        if (isolated_provider_timebase) {
            g_nx_stm32h743_system.started = false;
        } else {
            EXPECT_EQ(NX_SUCCESS,
                      nx_stm32h743_system_stop(&g_nx_stm32h743_system, 8U));
        }
        EXPECT_EQ(NX_SUCCESS, nx_stm32h743_pll384_stop(&owner, 8U));
        EXPECT_EQ(0U, owner.remaining_effects);
    }
    nx_stm32h743_start_result_t StartClock(uint32_t revision = 0x2003U) {
        g_h743_model.silicon_id = (revision << 16U) | 0x450U;
        g_h743_model.rcc.HSICFGR =
            revision == 0x1003U ? 0x00020000U : 0x40000000U;
        return nx_stm32h743_pll384_start(&owner, 8U);
    }
    static uint32_t KernelD2() {
        return (3U << RCC_D2CCIP2R_USART28SEL_Pos) |
               (2U << RCC_D2CCIP2R_I2C123SEL_Pos);
    }
    void PublishIsolatedProviderPrerequisites() {
        /* Authored external fixture only. These tests isolate provider
         * admission from the separately tested real high-System lifecycle.
         * No generated/startup or physical qualification follows from it. */
        g_h743_model.rcc.D2CCIP2R = KernelD2();
        g_h743_model.rcc.D3CCIPR = RCC_D3CCIPR_ADCSEL_1;
        g_nx_stm32h743_system.started = true;
        isolated_provider_timebase = true;
    }
    void WithdrawIsolatedKernelFixture() {
        /* Isolated provider tests restore their explicit fixture selectors.
         * The clock owner also accepts already-restored owned fields. */
        g_h743_model.rcc.D2CCIP2R = 0U;
        g_h743_model.rcc.D3CCIPR = 0U;
    }
};
#endif
