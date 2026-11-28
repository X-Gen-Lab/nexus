/**
 * \file            connectivity_test.cpp
 * \brief           Exact F105/F107 adapter with the shared production engine.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "connectivity_model.h"
#include <gtest/gtest.h>

extern "C" {
#include "drivers/can/connectivity.h"
#include "system/platform.h"
}

RCC_TypeDef nx_f1_conn_model_rcc;
nx_native_can_t nx_f1_conn_model_ports[2];

namespace {
constexpr uint32_t both_gates = RCC_APB1ENR_CAN1EN | RCC_APB1ENR_CAN2EN;
constexpr uint32_t silent_bits = CAN_BTR_SILM | CAN_BTR_LBKM;
constexpr uint32_t reset_tsr = CAN_TSR_TME0 | CAN_TSR_TME1 | CAN_TSR_TME2;
IRQn_Type foreign_irq;
bool system_live;

class ConnectivityCan : public testing::Test {
  protected:
    nx_native_can_t& can1 = nx_f1_conn_model_ports[0];
    nx_native_can_t& can2 = nx_f1_conn_model_ports[1];
    nx_can_filter_t filters[2]{
        {0x123U, 0x7FFU, NX_CAN_STANDARD, 11U, 0U, 0U, 0U, 0U, false, true},
        {0x321U, 0x7FFU, NX_CAN_STANDARD, 22U, 14U, 1U, 1U, 0U, false, true}};
    nx_can_filter_domain_t domain{};
    nx_can_event_t events[2][4]{};
    nx_stm32f1_connectivity_can_config_t configurations[2]{};
    nx_can_port_t ports[2]{{&nx_classic_can_ops, &can1.controller},
                           {&nx_classic_can_ops, &can2.controller}};

    void SetUp() override {
        nx_f1_conn_model_rcc = {};
        can1 = can2 = {};
        foreign_irq = NonMaskableInt_IRQn;
        system_live = true;
        domain.filters = filters;
        domain.filter_count = 2U;
        domain.split = 14U;
        for (unsigned i = 0U; i < 2U; ++i) {
            nx_f1_conn_model_ports[i].hardware.registers[2] = reset_tsr;
            auto& config = configurations[i];
            config.domain = &domain;
            config.events = events[i];
            config.capacity = 4U;
            config.timing = {500000U, 1U, 13U, 2U, 1U};
            config.irq_policy = {NX_IRQ_KERNEL_NONE, 68U, 4U, 0U};
            config.drain_timeout_us = 1000U;
            config.poll_limit = 32U;
            config.index = static_cast<uint8_t>(i);
            config.priority = 5U;
        }
    }

    nx_result_t start(unsigned index) {
        return nx_stm32f1_connectivity_can_prepare(
            &nx_f1_conn_model_ports[index].controller, &configurations[index]);
    }

    void transfer(unsigned index, uint32_t id, uint16_t tag) {
        auto* model = &nx_f1_conn_model_ports[index];
        ASSERT_NE(0U, domain.active_mask & (1U << index));
        const nx_can_frame_t frame{
            id, NX_CAN_STANDARD, NX_CAN_DATA, 1U, {0xA5U}};
        nx_can_tx_request_t request{};
        nx_request_initialize(&request.base);
        ASSERT_EQ(NX_SUCCESS, nx_can_tx_prepare(&request, &frame, 1000U));
        ASSERT_EQ(NX_SUCCESS, nx_can_port_submit(&ports[index], &request));
        ASSERT_EQ(NX_SUCCESS, nx_native_can_complete(model, 0U, NX_SUCCESS));
        ASSERT_EQ(NX_SUCCESS,
                  nx_native_can_receive(model, index, &frame, 1U, 0U));
        nx_can_controller_irq(&model->controller);
        nx_result_t outcome = NX_ERROR_STATE;
        size_t count = 0U;
        ASSERT_EQ(NX_SUCCESS,
                  nx_request_result(&request.base, &outcome, &count));
        EXPECT_EQ(NX_SUCCESS, outcome);
        EXPECT_EQ(1U, count);
        nx_can_event_t observed{};
        ASSERT_EQ(NX_SUCCESS, nx_can_port_read_events(&ports[index], &observed,
                                                      1U, &count));
        EXPECT_EQ(id, observed.frame.id);
        EXPECT_EQ(tag, observed.filter_tag);
        EXPECT_EQ(0xA5U, observed.frame.data[0]);
    }
};
} /* namespace */

extern "C" bool nx_f1_conn_model_irq_busy(IRQn_Type irq) {
    return irq == foreign_irq;
}

extern "C" bool nx_arch_is_privileged(void) {
    return true;
}
extern "C" bool nx_arch_in_isr(void) {
    return false;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return false;
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    return {};
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t state) {
    (void)state;
}
extern "C" void nx_arch_dsb(void) {
}
extern "C" bool nx_stm32f1_system_live(void) {
    return system_live;
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    return 100U;
}

TEST_F(ConnectivityCan, Can2AcquiresPrimaryDomainAndBothUseTheirOwnFilterFifo) {
    RCC->APB1ENR = RCC_APB1ENR_TIM2EN;
    ASSERT_EQ(NX_SUCCESS, start(1U));
    EXPECT_EQ(2U, domain.active_mask);
    EXPECT_EQ(both_gates | RCC_APB1ENR_TIM2EN, RCC->APB1ENR);
    EXPECT_FALSE(can1.controller.snapshot.started);
    EXPECT_EQ(14U << CAN_FMR_CAN2SB_Pos, nx_native_can_read(CAN1_BASE, 0x200U));
    EXPECT_EQ((1U << 14U) | 1U, nx_native_can_read(CAN1_BASE, 0x21CU));
    EXPECT_EQ(1U << 14U, nx_native_can_read(CAN1_BASE, 0x214U));
    EXPECT_EQ(0U, nx_native_can_read(CAN2_BASE, 0x21CU));
    EXPECT_EQ(0x123U << CAN_RI0R_STID_Pos,
              nx_native_can_read(CAN1_BASE, 0x240U));
    EXPECT_EQ(0x321U << CAN_RI0R_STID_Pos,
              nx_native_can_read(CAN1_BASE, 0x2B0U));
    ASSERT_EQ(NX_SUCCESS, start(0U));
    EXPECT_EQ(3U, domain.active_mask);
    for (const uintptr_t base : {CAN1_BASE, CAN2_BASE}) {
        EXPECT_EQ(0U, nx_native_can_read(base, 0U) & CAN_MCR_TTCM);
        EXPECT_EQ(silent_bits, nx_native_can_read(base, 0x1CU) & silent_bits);
    }
    EXPECT_EQ(CAN1_RX0_IRQn, can1.controller.sources[1].irq_number);
    EXPECT_EQ(CAN2_RX1_IRQn, can2.controller.sources[2].irq_number);
    transfer(0U, 0x123U, 11U);
    transfer(1U, 0x321U, 22U);

    EXPECT_EQ(NX_SUCCESS, nx_can_port_stop(&ports[0]));
    EXPECT_EQ(2U, domain.active_mask);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_connectivity_can_domain_close(&domain));
    EXPECT_EQ(both_gates | RCC_APB1ENR_TIM2EN, RCC->APB1ENR);
    EXPECT_EQ(0U, can1.hardware.enabled_irqs);
    EXPECT_EQ(15U, can2.hardware.enabled_irqs);
    transfer(1U, 0x321U, 22U);
    EXPECT_EQ(NX_SUCCESS, nx_can_port_stop(&ports[1]));
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_connectivity_can_domain_close(&domain));
    EXPECT_EQ(RCC_APB1ENR_TIM2EN, RCC->APB1ENR);
    EXPECT_EQ(0U, RCC->APB1RSTR);
}

TEST_F(ConnectivityCan, ForeignGateVectorAndTimebaseRejectBeforeClockMutation) {
    RCC->APB1ENR = RCC_APB1ENR_CAN2EN;
    EXPECT_EQ(NX_ERROR_BUSY, start(0U));
    EXPECT_EQ(RCC_APB1ENR_CAN2EN, RCC->APB1ENR);
    RCC->APB1ENR = 0U;
    foreign_irq = CAN2_RX0_IRQn;
    EXPECT_EQ(NX_ERROR_BUSY, start(1U));
    foreign_irq = NonMaskableInt_IRQn;
    system_live = false;
    EXPECT_EQ(NX_ERROR_STATE, start(0U));
    EXPECT_EQ(0U, RCC->APB1ENR);
    EXPECT_EQ(0U, domain.primary);
}

TEST_F(ConnectivityCan, StopFailureKeepsBothClockLeasesAndPrimaryCanRun) {
    ASSERT_EQ(NX_SUCCESS, start(0U));
    ASSERT_EQ(NX_SUCCESS, start(1U));
    can2.hardware.hold_irq_disable = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_can_port_stop(&ports[1]));
    EXPECT_EQ(3U, domain.active_mask);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_connectivity_can_domain_close(&domain));
    EXPECT_EQ(both_gates, RCC->APB1ENR);
    transfer(0U, 0x123U, 11U);
    can2.hardware.hold_irq_disable = false;
    EXPECT_EQ(NX_SUCCESS, nx_can_port_stop(&ports[1]));
    EXPECT_EQ(1U, domain.active_mask);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_connectivity_can_domain_close(&domain));
    transfer(0U, 0x123U, 11U);
    EXPECT_EQ(NX_SUCCESS, nx_can_port_stop(&ports[0]));
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_connectivity_can_domain_close(&domain));
    EXPECT_EQ(0U, RCC->APB1ENR);
}
