/**
 * \file            can_test.cpp
 * \brief           F103xB mailbox, FIFO and PMA ownership contracts.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "can_model.h"
#include <gtest/gtest.h>

extern "C" {
#include "drivers/can/instance.h"
#include "nexus/arch/arch.h"
}

RCC_TypeDef nx_can_model_rcc;
CAN_TypeDef nx_can_model_can1;

namespace {
constexpr uint32_t all_empty = CAN_TSR_TME0 | CAN_TSR_TME1 | CAN_TSR_TME2;
constexpr uint32_t completion =
    CAN_TSR_RQCP0 | CAN_TSR_TXOK0 | CAN_TSR_ALST0 | CAN_TSR_TERR0;
constexpr uint32_t test_timing = CAN_BTR_SILM | CAN_BTR_LBKM |
                                 (12U << CAN_BTR_TS1_Pos) |
                                 (1U << CAN_BTR_TS2_Pos);
unsigned writes;
unsigned can_resets;
uint32_t irq;
uint64_t now_us;
bool immediate;
bool abort_held;
bool init_held;
bool gate_held;
bool fifo_held;
uint32_t arriving_id;
bool ack_held;
bool launch_held;
bool privileged;

void idle() {
    nx_can_model_can1 = {};
    CAN1->MCR = CAN_MCR_SLEEP | CAN_MCR_DBF;
    CAN1->MSR = CAN_MSR_SLAK;
    CAN1->TSR = all_empty;
    CAN1->FMR = CAN_FMR_FINIT;
}

void complete_tx() {
    auto& tx = CAN1->sTxMailBox[0];
    auto& rx = CAN1->sFIFOMailBox[0];
    rx.RIR = tx.TIR & ~static_cast<uint32_t>(CAN_TI0R_TXRQ);
    rx.RDTR = tx.TDTR;
    rx.RDLR = tx.TDLR;
    rx.RDHR = tx.TDHR;
    tx.TIR &= ~static_cast<uint32_t>(CAN_TI0R_TXRQ);
    CAN1->RF0R = 1U;
    CAN1->TSR = all_empty | CAN_TSR_RQCP0 | CAN_TSR_TXOK0;
}

class Can : public testing::Test {
  protected:
    nx_stm32f1_can_t state{};
    nx_can_port_t port{&nx_stm32f1_can_ops, &state};
    nx_can_tx_request_t tx{};

    void SetUp() override {
        nx_can_model_rcc = {};
        idle();
        writes = can_resets = irq = arriving_id = 0U;
        now_us = 100U;
        immediate = privileged = true;
        abort_held = init_held = gate_held = fifo_held = launch_held =
            ack_held = false;
        nx_request_initialize(&tx.base);
    }

    void ready(nx_can_format_t format, uint32_t id, uint64_t deadline = 2000U) {
        nx_can_frame_t frame{
            id, format, NX_CAN_DATA, 8U, {1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U}};
        ASSERT_EQ(NX_SUCCESS, nx_can_tx_prepare(&tx, &frame, deadline));
    }

    void result(nx_result_t expected, size_t bytes) {
        nx_result_t actual = NX_ERROR_STATE;
        size_t transferred = SIZE_MAX;
        ASSERT_EQ(NX_SUCCESS,
                  nx_request_result(&tx.base, &actual, &transferred));
        EXPECT_EQ(expected, actual);
        EXPECT_EQ(bytes, transferred);
    }
};
} /* namespace */

extern "C" uint32_t nx_can_model_irq(void) {
    return irq;
}
extern "C" bool nx_arch_is_privileged(void) {
    return privileged;
}
extern "C" bool nx_arch_in_isr(void) {
    return false;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return false;
}
extern "C" void nx_arch_dsb(void) {
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    return now_us;
}

extern "C" void nx_can_model_write(volatile uint32_t* reg, uint32_t value) {
    ++writes;
    if (reg == &RCC->APB1ENR && gate_held) {
        return;
    }
    if (reg == &RCC->APB1RSTR && (value & RCC_APB1RSTR_CAN1RST) != 0U) {
        ++can_resets;
        EXPECT_EQ(0U, value & RCC_APB1RSTR_USBRST);
        idle();
    }
    if (reg == &CAN1->MCR && !init_held) {
        CAN1->MSR = (value & CAN_MCR_INRQ) != 0U ? CAN_MSR_INAK : 0U;
    }
    if (reg == &CAN1->TSR) {
        if ((value & CAN_TSR_RQCP0) != 0U && !ack_held) {
            CAN1->TSR &= ~completion;
        }
        if ((value & CAN_TSR_ABRQ0) != 0U && !abort_held) {
            CAN1->sTxMailBox[0].TIR &= ~static_cast<uint32_t>(CAN_TI0R_TXRQ);
            CAN1->TSR = all_empty | CAN_TSR_RQCP0;
        }
        return;
    }
    if (reg == &CAN1->RF0R) {
        if ((value & CAN_RF0R_RFOM0) != 0U) {
            constexpr uint32_t fields = CAN_RF0R_FMP0 | CAN_RF0R_RFOM0;
            if (fifo_held) {
                CAN1->RF0R |= CAN_RF0R_RFOM0;
                return;
            }
            const uint32_t count = CAN1->RF0R & CAN_RF0R_FMP0;
            if (count != 0U) {
                if (arriving_id == 0U) {
                    CAN1->RF0R = (CAN1->RF0R & ~fields) | (count - 1U);
                } else {
                    CAN1->sFIFOMailBox[0].RIR = arriving_id
                                                << CAN_RI0R_STID_Pos;
                    CAN1->RF0R &= ~static_cast<uint32_t>(CAN_RF0R_RFOM0);
                    arriving_id = 0U;
                }
            }
        }
        CAN1->RF0R &= ~(value & static_cast<uint32_t>(CAN_RF0R_FOVR0));
        return;
    }
    if (reg == &CAN1->sTxMailBox[0].TIR && launch_held)
        return;
    *reg = value;
    if (reg == &CAN1->sTxMailBox[0].TIR && (value & CAN_TI0R_TXRQ) != 0U) {
        CAN1->TSR &= ~static_cast<uint32_t>(CAN_TSR_TME0);
        if (immediate) {
            complete_tx();
        }
    }
}

TEST_F(Can, HardwareConfigIsSilentNoIrqAndStandardExtendedFramesRoundTrip) {
    RCC->APB1ENR = RCC_APB1ENR_TIM2EN;
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_can_prepare(&state));
    EXPECT_EQ(test_timing, CAN1->BTR);
    EXPECT_EQ(0U, CAN1->IER);
    EXPECT_EQ(CAN_FA1R_FACT0, CAN1->FA1R);
    EXPECT_EQ(0U, CAN1->sFilterRegister[0].FR1 | CAN1->sFilterRegister[0].FR2);
    for (auto format : {NX_CAN_STANDARD, NX_CAN_EXTENDED}) {
        const uint32_t id = format == NX_CAN_STANDARD ? 0x7FFU : 0x1FFFFFFFU;
        ready(format, id);
        ASSERT_EQ(NX_SUCCESS, nx_can_port_submit(&port, &tx));
        EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&tx.base));
        ASSERT_EQ(NX_SUCCESS, nx_can_port_service(&port));
        result(NX_SUCCESS, 8U);
        ready(format, id);
        EXPECT_EQ(NX_ERROR_BUSY, nx_can_port_submit(&port, &tx));
        EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&tx.base));
        nx_can_event_t event{};
        size_t count = 0U;
        ASSERT_EQ(NX_SUCCESS,
                  nx_can_port_read_events(&port, &event, 1U, &count));
        EXPECT_EQ(1U, count);
        EXPECT_EQ(id, event.frame.id);
        EXPECT_EQ(format, event.frame.format);
        EXPECT_EQ(8U, event.frame.dlc);
        EXPECT_EQ(8U, event.frame.data[7]);
        EXPECT_EQ(0U, event.filter_tag);
        EXPECT_NE(0U, event.flags & NX_CAN_FILTER_UNKNOWN);
        EXPECT_EQ(NX_ERROR_EMPTY,
                  nx_can_port_read_events(&port, &event, 1U, &count));
        EXPECT_EQ(0U, count);
    }
    EXPECT_EQ(NX_SUCCESS, nx_can_port_stop(&port));
    EXPECT_EQ(RCC_APB1ENR_TIM2EN, RCC->APB1ENR);
    EXPECT_EQ(1U, can_resets);
}

TEST_F(Can, ANewArrivalCanKeepFifoCountUnchangedDuringRelease) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_can_prepare(&state));
    CAN1->RF0R = 1U;
    CAN1->sFIFOMailBox[0].RIR = 0x142U << CAN_RI0R_STID_Pos;
    arriving_id = 0x2A5U;
    nx_can_event_t event{};
    size_t count = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_can_port_read_events(&port, &event, 1U, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(0x142U, event.frame.id);
    EXPECT_EQ(1U, CAN1->RF0R & CAN_RF0R_FMP0);
    ASSERT_EQ(NX_SUCCESS, nx_can_port_read_events(&port, &event, 1U, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(0x2A5U, event.frame.id);
    EXPECT_EQ(0U, CAN1->RF0R & CAN_RF0R_FMP0);
}

TEST_F(Can, UnacknowledgedFifoReleaseRequiresCloseBeforeAnotherRead) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_can_prepare(&state));
    CAN1->RF0R = 1U;
    fifo_held = true;
    nx_can_event_t event{};
    size_t count = 0U;
    EXPECT_EQ(NX_ERROR_IO, nx_can_port_read_events(&port, &event, 1U, &count));
    EXPECT_EQ(0U, count);
    EXPECT_EQ(0U, event.frame.id);
    EXPECT_NE(0U, CAN1->RF0R & CAN_RF0R_RFOM0);
    const unsigned failed_write_count = writes;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_can_port_read_events(&port, &event, 1U, &count));
    EXPECT_EQ(failed_write_count, writes);
    EXPECT_EQ(NX_SUCCESS, nx_can_port_stop(&port));
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_can_prepare(&state));
}

TEST_F(Can, RejectsExistingCanUsbSharedVectorAndForeignControllerState) {
    constexpr uint32_t gates[] = {RCC_APB1ENR_USBEN, RCC_APB1ENR_CAN1EN};
    for (uint32_t mask : gates) {
        RCC->APB1ENR = mask;
        EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_can_prepare(&state));
    }
    RCC->APB1ENR = 0U;
    irq = 1U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_can_prepare(&state));
    irq = 0U;
    EXPECT_EQ(0U, writes);
    CAN1->IER = CAN_IER_TMEIE;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_can_prepare(&state));
    EXPECT_EQ(CAN_IER_TMEIE, CAN1->IER);
    EXPECT_EQ(0U, RCC->APB1ENR);
}

TEST_F(Can, DeadlineQuarantinesUntilExclusiveHardwareIsWithdrawn) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_can_prepare(&state));
    immediate = false;
    abort_held = true;
    ready(NX_CAN_STANDARD, 0x123U, 200U);
    ASSERT_EQ(NX_SUCCESS, nx_can_port_submit(&port, &tx));
    now_us = 200U;
    EXPECT_EQ(NX_SUCCESS, nx_can_port_service(&port));
    EXPECT_EQ(NX_REQUEST_DRAINING, nx_request_state(&tx.base));
    now_us += 1001U;
    EXPECT_EQ(NX_SUCCESS, nx_can_port_service(&port));
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&tx.base));
    RCC->APB1ENR |= RCC_APB1ENR_USBEN;
    EXPECT_EQ(NX_ERROR_BUSY, nx_can_port_stop(&port));
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&tx.base));
    RCC->APB1ENR &= ~static_cast<uint32_t>(RCC_APB1ENR_USBEN);
    EXPECT_EQ(NX_SUCCESS, nx_can_port_stop(&port));
    result(NX_ERROR_TIMEOUT, 0U);
    EXPECT_EQ(0U, RCC->APB1ENR);
}

TEST_F(Can, InternalCompletionBeforeCancelWinsAndClearPreservesOtherMailbox) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_can_prepare(&state));
    ready(NX_CAN_STANDARD, 11U);
    ASSERT_EQ(NX_SUCCESS, nx_can_port_submit(&port, &tx));
    CAN1->TSR |= CAN_TSR_RQCP1;
    EXPECT_EQ(NX_SUCCESS, nx_can_port_cancel(&port, &tx));
    result(NX_SUCCESS, 8U);
    EXPECT_NE(0U, CAN1->TSR & CAN_TSR_RQCP1);
}

TEST_F(Can, UnavailableInitTimeoutRetainsRecoverableGate) {
    init_held = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_stm32f1_can_prepare(&state));
    EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_CAN1EN);
    gate_held = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f1_can_close(&state));
    gate_held = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_can_close(&state));
    EXPECT_EQ(0U, RCC->APB1ENR);
}

TEST_F(Can, FailedLaunchRemainsAcceptedAndSettlesWithoutBorrow) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_can_prepare(&state));
    ready(NX_CAN_STANDARD, 0x721U);
    launch_held = true;
    EXPECT_EQ(NX_SUCCESS, nx_can_port_submit(&port, &tx));
    result(NX_ERROR_IO, 0U);
    EXPECT_EQ(0U, CAN1->RF0R & CAN_RF0R_FMP0);
    EXPECT_EQ(NX_SUCCESS, nx_can_port_stop(&port));
}

TEST_F(Can, RetiredTxSucceedsEvenWhenItsCompletionLatchCannotBeCleared) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_can_prepare(&state));
    ready(NX_CAN_STANDARD, 42U);
    ASSERT_EQ(NX_SUCCESS, nx_can_port_submit(&port, &tx));
    ack_held = true;
    EXPECT_EQ(NX_ERROR_IO, nx_can_port_service(&port));
    result(NX_SUCCESS, 8U);
    EXPECT_EQ(NX_SUCCESS, nx_can_port_stop(&port));
    EXPECT_EQ(0U, RCC->APB1ENR);
}
