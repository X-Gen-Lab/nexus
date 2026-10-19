/**
 * \file            u575_lptim_factory_test.cpp
 * \brief           Real generated U575 LPTIM admission before global clocks
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32u575_lptim.h"
#include "u575_lptim_factory_model.h"
#include <gtest/gtest.h>
class U575LptimFactory : public testing::Test {
  protected:
    nx_lptim_request_t request{};
    void SetUp() override {
        nx_u575_lptim_model_reset();
        g_nx_stm32u575_system.started = false;
        g_nx_stm32u575_system.core_hz = 0U;
        nx_request_initialize(&request.base);
    }
    void TearDown() override {
        const nx_lptim_port_t* port = nx_factory_lptim(NX_LPTIM_ID_COUNTER);
        (void)nx_lptim_port_cancel(port, &request);
        for (unsigned n = 0U; n < 80U; ++n) {
            nx_u575_lptim_model_step(1U);
            (void)nx_lptim_port_service(port);
        }
        EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
        EXPECT_EQ(0U, g_u575_lptim_model.bad_access);
    }
};
TEST_F(U575LptimFactory, LookupIsPureAndStartConstructsCanonicalInterface) {
    const auto* port = nx_factory_lptim(NX_LPTIM_ID_COUNTER);
    ASSERT_NE(nullptr, port);
    EXPECT_EQ(&nx_stm32u575_lptim_ops, port->ops);
    EXPECT_EQ(0U, g_u575_model.writes);
    EXPECT_EQ(0U, g_u575_lptim_model.writes);
    ASSERT_EQ(NX_SUCCESS, nx_platform_start().primary);
    nx_lptim_sample_t sample{};
    ASSERT_EQ(NX_SUCCESS, nx_lptim_port_sample(port, &sample));
    ASSERT_EQ(NX_SUCCESS, nx_lptim_request_prepare(&request, &sample.domain, 2U,
                                                   NX_DEADLINE_NEVER));
    ASSERT_EQ(NX_SUCCESS, nx_lptim_port_submit(port, &request));
    for (unsigned n = 0U;
         n < 80U && nx_request_state(&request.base) != NX_REQUEST_SETTLED;
         ++n) {
        nx_u575_lptim_model_step(1U);
        (void)nx_lptim_port_service(port);
    }
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(2U, request.base.transferred);
}
TEST_F(U575LptimFactory,
       ForeignMuxResetAndActiveVectorRejectBeforeGlobalWrites) {
    for (unsigned which = 0U; which < 3U; ++which) {
        g_u575_model.rcc.CCIPR1 = which == 0U ? RCC_CCIPR1_LPTIM2SEL_0 : 0U;
        g_u575_model.rcc.APB1RSTR2 = which == 1U ? RCC_APB1RSTR2_LPTIM2RST : 0U;
        g_u575_model.irq_active[68] = which == 2U;
        auto before = g_u575_model.writes;
        EXPECT_EQ(NX_ERROR_BUSY, nx_platform_start().primary);
        EXPECT_EQ(before, g_u575_model.writes);
        EXPECT_FALSE(g_nx_stm32u575_system.started);
    }
    g_u575_model.rcc.CCIPR1 = g_u575_model.rcc.APB1RSTR2 = 0U;
    g_u575_model.irq_active[68] = false;
}
TEST_F(U575LptimFactory, PoweredForeignTimerRejectsBeforeGlobalClockEffects) {
    g_u575_model.rcc.APB1ENR2 = RCC_APB1ENR2_LPTIM2EN;
    g_u575_lptim_model.registers.CFGR = LPTIM_CFGR_TRIGEN_0;
    EXPECT_EQ(NX_ERROR_BUSY, nx_platform_start().primary);
    EXPECT_EQ(0U, g_u575_model.writes);
    EXPECT_EQ(LPTIM_CFGR_TRIGEN_0, g_u575_lptim_model.registers.CFGR);
}
TEST_F(U575LptimFactory, GateRevealedForeignStateNeverGrantsResetOwnership) {
    g_u575_lptim_model.registers.CR = LPTIM_CR_ENABLE;
    EXPECT_EQ(NX_ERROR_BUSY, nx_platform_start().primary);
    EXPECT_EQ(LPTIM_CR_ENABLE, g_u575_lptim_model.registers.CR);
    EXPECT_EQ(0U, g_u575_model.rcc.APB1ENR2 & RCC_APB1ENR2_LPTIM2EN);
    EXPECT_FALSE(g_nx_stm32u575_system.started);
}
TEST_F(U575LptimFactory, StopPreservesPreexistingGateAndOtherApb1Owners) {
    g_u575_model.rcc.APB1ENR2 = RCC_APB1ENR2_LPTIM2EN | RCC_APB1ENR2_I2C4EN;
    ASSERT_EQ(NX_SUCCESS, nx_platform_start().primary);
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(RCC_APB1ENR2_LPTIM2EN | RCC_APB1ENR2_I2C4EN,
              g_u575_model.rcc.APB1ENR2);
}
