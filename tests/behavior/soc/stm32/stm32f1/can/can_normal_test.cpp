/**
 * \file            can_normal_test.cpp
 * \brief           F103RB external CAN admission, wire status and safe stop.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "can_normal_model.h"
#include <gtest/gtest.h>

extern "C" {
#include "drivers/can/normal.h"
#include "system/platform.h"
}

RCC_TypeDef nx_can_model_rcc;
CAN_TypeDef nx_can_model_can1;
GPIO_TypeDef nx_can_model_gpioa;
GPIO_TypeDef nx_can_model_gpioc;
AFIO_TypeDef nx_can_model_afio;

namespace {
constexpr unsigned rs = 1U << 2U;
constexpr uint32_t normal_timing =
    (12U << CAN_BTR_TS1_Pos) | (3U << CAN_BTR_TS2_Pos) | 3U;
constexpr uint32_t tx_empty = CAN_TSR_TME0 | CAN_TSR_TME1 | CAN_TSR_TME2;
constexpr uint32_t floating = 0x44444444U;
unsigned phy_sessions;
unsigned can_resets;
unsigned can_launches;
nx_time_us_t now;
nx_time_us_t phy_activation;
bool phy_active;
bool rs_short;
bool time_advances;
bool life;
bool no_ack;
bool hold_mailbox;

void can_idle() {
    nx_can_model_can1 = {};
    CAN1->MSR = CAN_MSR_SLAK;
    CAN1->MCR = CAN_MCR_SLEEP | CAN_MCR_DBF;
    CAN1->TSR = tx_empty;
    CAN1->FMR = CAN_FMR_FINIT;
}

class CanNormal : public testing::Test {
  protected:
    nx_stm32f1_can_normal_t owner{};
    nx_can_port_t port{&nx_stm32f1_can_ops, &owner};
    nx_can_tx_request_t tx{};

    void SetUp() override {
        nx_can_model_rcc = {};
        nx_can_model_rcc.CFGR = RCC_CFGR_SWS_PLL | RCC_CFGR_PPRE1_DIV2 |
                                RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9;
        nx_can_model_rcc.CR = RCC_CR_PLLON | RCC_CR_PLLRDY | RCC_CR_HSEON |
                              RCC_CR_HSERDY | RCC_CR_HSEBYP;
        nx_can_model_gpioa = {};
        nx_can_model_gpioc = {};
        GPIOA->CRL = GPIOA->CRH = GPIOC->CRL = GPIOC->CRH = floating;
        GPIOC->IDR = rs;
        nx_can_model_afio = {};
        can_idle();
        now = 100U;
        phy_sessions = can_resets = can_launches = 0U;
        phy_active = rs_short = no_ack = hold_mailbox = false;
        life = time_advances = true;
        nx_request_initialize(&tx.base);
    }

    void submit() {
        constexpr nx_can_frame_t payload{
            0x123U, NX_CAN_STANDARD, NX_CAN_DATA, 1U, {0xA5U}};
        ASSERT_EQ(NX_SUCCESS, nx_can_tx_prepare(&tx, &payload, now + 1000U));
        ASSERT_EQ(NX_SUCCESS, nx_can_port_submit(&port, &tx));
    }
};
} /* namespace */

extern "C" uint32_t nx_can_model_irq(void) {
    return 0U;
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
extern "C" void nx_arch_dsb(void) {
}
extern "C" bool nx_stm32f1_system_live(void) {
    return life;
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    if (time_advances)
        ++now;
    return now;
}

extern "C" void nx_can_model_bsrr(GPIO_TypeDef* registers, uint32_t value) {
    registers->ODR = (registers->ODR | (value & UINT16_MAX)) & ~(value >> 16U);
    if (registers != GPIOC)
        return;
    if ((value & (rs << 16U)) != 0U && ((GPIOC->CRL >> 8U) & 0xFU) == 6U) {
        EXPECT_EQ(normal_timing, CAN1->BTR);
        EXPECT_EQ(0U, CAN1->MSR & (CAN_MSR_INAK | CAN_MSR_SLAK));
        phy_active = true;
        phy_activation = now;
        ++phy_sessions;
    }
    if ((value & rs) != 0U)
        phy_active = false;
    GPIOC->IDR = phy_active || rs_short ? 0U : rs;
}

extern "C" void nx_can_model_write(volatile uint32_t* reg, uint32_t value) {
    if (reg == &RCC->APB1RSTR && (value & RCC_APB1RSTR_CAN1RST) != 0U) {
        EXPECT_FALSE(phy_active);
        ++can_resets;
        can_idle();
    }
    if (reg == &CAN1->MCR)
        CAN1->MSR = (value & CAN_MCR_INRQ) != 0U ? CAN_MSR_INAK : 0U;
    if (reg == &CAN1->TSR) {
        if ((value & CAN_TSR_RQCP0) != 0U)
            CAN1->TSR = tx_empty;
        return;
    }
    if (reg == &CAN1->RF0R && (value & CAN_RF0R_RFOM0) != 0U) {
        CAN1->RF0R = 0U;
        return;
    }
    *reg = value;
    if (reg == &CAN1->sTxMailBox[0].TIR && (value & CAN_TI0R_TXRQ) != 0U) {
        EXPECT_TRUE(phy_active);
        EXPECT_GE(now - phy_activation, 3U);
        ++can_launches;
        if (hold_mailbox) {
            CAN1->TSR &= ~static_cast<uint32_t>(CAN_TSR_TME0);
            return;
        }
        CAN1->sTxMailBox[0].TIR &= ~static_cast<uint32_t>(CAN_TI0R_TXRQ);
        CAN1->TSR =
            tx_empty | CAN_TSR_RQCP0 | (no_ack ? CAN_TSR_TERR0 : CAN_TSR_TXOK0);
        if (no_ack)
            CAN1->ESR = (1U << CAN_ESR_TEC_Pos) | (3U << CAN_ESR_LEC_Pos);
    }
}

TEST_F(CanNormal, RejectsWrongClockRemapUsartAndMissingStandby) {
    const uint32_t run = RCC->CFGR;
    RCC->CFGR = 0U;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32f1_can_normal_prepare(&owner));
    RCC->CFGR = run;
    RCC->APB2ENR = RCC_APB2ENR_USART1EN;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_can_normal_prepare(&owner));
    RCC->APB2ENR = 0U;
    AFIO->MAPR = AFIO_MAPR_CAN_REMAP_1;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_can_normal_prepare(&owner));
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_can_normal_close(&owner));
    AFIO->MAPR = 0U;
    GPIOC->IDR = 0U;
    rs_short = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f1_can_normal_prepare(&owner));
    EXPECT_EQ(0U, can_resets | phy_sessions);
    EXPECT_EQ(0U, RCC->APB1ENR & RCC_APB1ENR_CAN1EN);
}

TEST_F(CanNormal, SendsWithPendingIngressAndReportsMissingAck) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_can_normal_prepare(&owner));
    EXPECT_EQ(1U, phy_sessions);
    EXPECT_EQ(normal_timing, CAN1->BTR);
    EXPECT_EQ(CAN_MCR_NART, CAN1->MCR & CAN_MCR_NART);
    CAN1->RF0R = 1U;
    CAN1->sFIFOMailBox[0].RIR = 0x342U << CAN_RI0R_STID_Pos;
    no_ack = true;
    submit();
    ASSERT_EQ(NX_SUCCESS, nx_can_port_service(&port));
    EXPECT_EQ(1U, can_launches);
    nx_result_t outcome = NX_SUCCESS;
    size_t transferred = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_request_result(&tx.base, &outcome, &transferred));
    EXPECT_EQ(NX_ERROR_IO, outcome);
    EXPECT_EQ(0U, transferred);
    nx_can_state_t state{};
    ASSERT_EQ(NX_SUCCESS, nx_can_port_state(&port, &state));
    EXPECT_EQ(NX_CAN_PROTOCOL_ACK, state.last_error);
    EXPECT_EQ(1U, state.transmit_errors);
    nx_can_event_t value{};
    size_t count = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_can_port_read_events(&port, &value, 1U, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(0x342U, value.frame.id);
    EXPECT_EQ(NX_SUCCESS, nx_can_port_stop(&port));
    EXPECT_FALSE(phy_active);
    EXPECT_EQ(1U, can_resets);
    EXPECT_EQ(floating, GPIOC->CRL);
    EXPECT_EQ(floating, GPIOA->CRH);
}

TEST_F(CanNormal, UncertainWakeKeepsResourcesUntilSafeStop) {
    time_advances = false;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_stm32f1_can_normal_prepare(&owner));
    EXPECT_FALSE(phy_active);
    EXPECT_EQ(0U, can_launches);
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_can_normal_close(&owner));
    EXPECT_EQ(0U, RCC->APB1ENR & RCC_APB1ENR_CAN1EN);
    EXPECT_EQ(0U, RCC->APB2ENR);
}

TEST_F(CanNormal, StuckRsOnStopPreventsResetAndRetainsPins) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_can_normal_prepare(&owner));
    rs_short = true;
    EXPECT_EQ(NX_ERROR_IO, nx_can_port_stop(&port));
    EXPECT_EQ(0U, can_resets);
    EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_CAN1EN);
    rs_short = false;
    EXPECT_EQ(NX_SUCCESS, nx_can_port_stop(&port));
    EXPECT_EQ(1U, can_resets);
}

TEST_F(CanNormal, BusOffBlocksAdmissionAndStopProvesRequestWithdrawal) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_can_normal_prepare(&owner));
    hold_mailbox = true;
    submit();
    CAN1->ESR = CAN_ESR_BOFF;
    ASSERT_EQ(NX_SUCCESS, nx_can_port_service(&port));
    nx_can_state_t state{};
    ASSERT_EQ(NX_SUCCESS, nx_can_port_state(&port, &state));
    EXPECT_TRUE(state.bus_off);
    EXPECT_EQ(1U, state.pending);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&tx.base));
    EXPECT_EQ(NX_SUCCESS, nx_can_port_stop(&port));
    EXPECT_EQ(1U, can_resets);
    EXPECT_FALSE(phy_active);
    nx_result_t outcome = NX_SUCCESS;
    size_t transferred = 1U;
    EXPECT_EQ(NX_SUCCESS, nx_request_result(&tx.base, &outcome, &transferred));
    EXPECT_EQ(NX_ERROR_CANCELLED, outcome);
    EXPECT_EQ(0U, transferred);
}
