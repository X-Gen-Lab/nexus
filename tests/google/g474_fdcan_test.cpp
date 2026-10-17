/**
 * \file            g474_fdcan_test.cpp
 * \brief           Actual G474 fixed RAM, queue, cancellation and isolation
 * contracts \author          Nexus Team \version         1.0.0 \date 2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "g474_fdcan_model.h"
#include "nexus/io/native/model.h"
#include "stm32g474_fdcan.h"
#include "stm32g474_system.h"
}
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <unistd.h>
using testing::_;
using testing::NiceMock;
struct Leaves {
    MOCK_METHOD(void, Write, (volatile uint32_t*, uint32_t));
    MOCK_METHOD(nx_result_t, Irq, (int32_t, bool, uint8_t));
};
static Leaves* leaves;
static uint32_t irq_mask;
static uint8_t irq_priority[2];
static uint32_t irq_pending;
static uint32_t irq_active;
static bool hold_init;
static bool hold_add;
static bool recessive_rx;
static uint32_t tx_order[3];
static uint32_t next_order;
static uint8_t forced_put_index;
static size_t unclocked_fdcan_reads;
static bool FdcanAddress(const volatile uint32_t* reg) {
    const uintptr_t address = reinterpret_cast<uintptr_t>(reg);
    const uintptr_t controllers = reinterpret_cast<uintptr_t>(g_io33_fdcan);
    const uintptr_t configuration =
        reinterpret_cast<uintptr_t>(&g_io33_configuration);
    const uintptr_t ram = reinterpret_cast<uintptr_t>(g_io33_ram);
    return (address >= controllers &&
            address - controllers < sizeof(g_io33_fdcan)) ||
           (address >= configuration &&
            address - configuration < sizeof(g_io33_configuration)) ||
           (address >= ram && address - ram < sizeof(g_io33_ram));
}
/* The external controller scan, not the provider, computes physical put index.
 */
static uint32_t QueueStatus() {
    const auto& hw = g_io33_fdcan[0];
    const uint32_t pending = (hw.TXBRP | hw.TXBAR) & 7U;
    uint32_t free_index = 0U;
    while (free_index < 3U && (pending & (1U << free_index)) != 0U)
        ++free_index;
    const bool full = free_index == 3U;
    const uint32_t put_index = forced_put_index <= 3U ? forced_put_index
                               : full                 ? 0U
                                                      : free_index;
    uint32_t result =
        (full ? FDCAN_TXFQS_TFQF : 0U) | (put_index << FDCAN_TXFQS_TFQPI_Pos);
    if ((hw.TXBC & FDCAN_TXBC_TFQM) == 0U) {
        uint32_t free_count = 0U;
        for (uint32_t index = 0U; index < 3U; ++index)
            if ((pending & (1U << index)) == 0U)
                ++free_count;
        result |= free_count;
    }
    /* In queue mode TFGI and TFFL read zero, RM0440 p2006. */
    return result;
}
static void FinishAddScan() {
    auto& hw = g_io33_fdcan[0];
    hw.TXBRP |= hw.TXBAR & 7U;
    hw.TXBAR = 0U;
}
static uint32_t RunOneBusAttempt(bool acknowledged = true) {
    auto& hw = g_io33_fdcan[0];
    uint32_t selected = 3U;
    uint32_t priority = UINT32_MAX;
    for (uint32_t index = 0U; index < 3U; ++index) {
        if ((hw.TXBRP & (1U << index)) == 0U)
            continue;
        const uint32_t header = g_io33_ram[0x278U / 4U + index * 18U];
        const uint32_t key = (hw.TXBC & FDCAN_TXBC_TFQM) != 0U
                                 ? header & 0x1FFFFFFFU
                                 : tx_order[index];
        if (selected == 3U || key < priority) {
            selected = index;
            priority = key;
        }
    }
    if (selected == 3U)
        return 0U;
    const uint32_t bit = 1U << selected;
    const bool internal_ack =
        (hw.CCCR & FDCAN_CCCR_MON) != 0U && (hw.TEST & FDCAN_TEST_LBCK) != 0U;
    const bool transmitted = acknowledged || internal_ack;
    if (!transmitted && (hw.CCCR & FDCAN_CCCR_DAR) == 0U)
        return 0U;
    hw.TXBRP &= ~bit;
    if (transmitted) {
        hw.TXBTO |= bit;
        hw.IR |= FDCAN_IR_TC;
    } else {
        hw.PSR = (hw.PSR & ~static_cast<uint32_t>(FDCAN_PSR_LEC)) | 3U;
        hw.ECR = (hw.ECR & ~static_cast<uint32_t>(FDCAN_ECR_TEC)) | 8U;
        hw.IR |= FDCAN_IR_TCF;
    }
    if (!transmitted || (hw.TXBCR & bit) != 0U)
        hw.TXBCF |= bit;
    hw.TXBCR &= ~bit;
    return bit;
}
extern "C" {
nx_stm32g474_system_t g_nx_stm32g474_system;
RCC_TypeDef g_io33_rcc;
FDCAN_Config_TypeDef g_io33_configuration;
FDCAN_GlobalTypeDef g_io33_fdcan[3];
volatile uint32_t g_io33_ram[3U * 212U];
uint32_t nx_io33_read(volatile const uint32_t* reg) {
    if ((g_io33_rcc.APB1ENR1 & RCC_APB1ENR1_FDCANEN) == 0U &&
        FdcanAddress(reg)) {
        ++unclocked_fdcan_reads;
        return 0U;
    }
    if (reg == &g_io33_fdcan[0].TXFQS)
        return QueueStatus();
    if (reg == &g_io33_fdcan[0].TEST)
        return *reg |
               (recessive_rx ? static_cast<uint32_t>(FDCAN_TEST_RX) : 0U);
    return *reg;
}
void nx_io33_write(volatile uint32_t* reg, uint32_t value) {
    leaves->Write(reg, value);
    auto* hw = &g_io33_fdcan[0];
    if (reg == &hw->IR) {
        hw->IR &= ~value;
        return;
    }
    if (reg == &hw->TXBAR) {
        const uint32_t added = value & ~(hw->TXBRP | hw->TXBAR) & 7U;
        for (uint32_t index = 0U; index < 3U; ++index)
            if ((added & (1U << index)) != 0U)
                tx_order[index] = next_order++;
        hw->TXBAR |= added;
        hw->TXBTO &= ~added;
        hw->TXBCF &= ~added;
        if (!hold_add)
            FinishAddScan();
        return;
    }
    if (reg == &hw->TXBCR) {
        hw->TXBCR |= value & 7U;
        return;
    }
    if (reg == &hw->CCCR && hold_init) {
        const uint32_t init = static_cast<uint32_t>(FDCAN_CCCR_INIT);
        value = (value & ~init) | (hw->CCCR & init);
    }
    if (reg == &hw->CCCR && (value & FDCAN_CCCR_CCE) != 0U &&
        (hw->CCCR & FDCAN_CCCR_CCE) == 0U) {
        /* Actual CCE destroys outcome facts, so service must latch first. */
        hw->TXBRP = hw->TXBTO = hw->TXBCF = hw->TXBAR = hw->TXBCR = 0U;
        hw->RXF0S = hw->RXF1S = 0U;
    }
    if (reg == &hw->RXF0A || reg == &hw->RXF1A) {
        *reg = value;
        volatile uint32_t* status = reg == &hw->RXF0A ? &hw->RXF0S : &hw->RXF1S;
        const uint32_t count = *status & FDCAN_RXF0S_F0FL;
        const uint32_t next = (value + 1U) % 3U;
        *status =
            count == 0U ? 0U : (count - 1U) | (next << FDCAN_RXF0S_F0GI_Pos);
        return;
    }
    *reg = value;
}
bool nx_io33_irq_available(int32_t irq) {
    return irq >= 21 && irq <= 22 &&
           ((irq_mask | irq_pending | irq_active) & (1U << (irq - 21))) == 0U;
}
nx_result_t nx_io33_irq_control(int32_t irq, bool on, uint8_t priority) {
    const nx_result_t result = leaves->Irq(irq, on, priority);
    if (result != NX_SUCCESS || irq < 21 || irq > 22)
        return result;
    const uint32_t bit = 1U << (irq - 21);
    irq_mask = on ? irq_mask | bit : irq_mask & ~bit;
    irq_priority[irq - 21] = priority;
    return NX_SUCCESS;
}
uint8_t nx_io33_irq_priority(int32_t irq) {
    return irq >= 21 && irq <= 22 ? irq_priority[irq - 21] : UINT8_MAX;
}
}
class G474Fdcan : public testing::Test {
  protected:
    NiceMock<Leaves> external;
    nx_stm32g474_fdcan_domain_t domain = {};
    nx_stm32g474_fdcan_state_t controller = {};
    const nx_can_fd_filter_t filters[2] = {
        {0x123U, 0x7FFU, NX_CAN_STANDARD, 0U},
        {0x1ABCDEU, 0x1FFFFFFFU, NX_CAN_EXTENDED, 1U}};
    nx_can_fd_event_t events[8] = {};
    const nx_can_fd_port_t port = {&nx_stm32g474_fdcan_ops, &controller};
    nx_can_fd_tx_request_t request = {};
    nx_can_fd_tx_request_t other_request = {};
    nx_can_fd_tx_request_t third_request = {};
    nx_can_fd_frame_t frame = {};
    size_t old_hints = 0U;
    size_t new_hints = 0U;
    static nx_result_t Notify(void* context) {
        ++*static_cast<size_t*>(context);
        return NX_SUCCESS;
    }
    const nx_irq_wake_t old_wake = {&old_hints, Notify, true};
    const nx_irq_wake_t new_wake = {&new_hints, Notify, true};
    void DeliverLine0() {
        if ((irq_mask & 1U) != 0U && (FDCAN1->ILE & FDCAN_ILE_EINT0) != 0U &&
            FDCAN1->ILS == 0U && (FDCAN1->IR & FDCAN1->IE) != 0U)
            nx_stm32g474_fdcan_irq(&controller);
    }
    void SetUp() override {
        leaves = &external;
        ON_CALL(external, Irq(_, _, _))
            .WillByDefault(testing::Return(NX_SUCCESS));
        std::memset(&g_io33_rcc, 0, sizeof(g_io33_rcc));
        std::memset(&g_io33_configuration, 0, sizeof(g_io33_configuration));
        std::memset(g_io33_fdcan, 0, sizeof(g_io33_fdcan));
        for (auto& word : g_io33_ram)
            word = 0U;
        for (auto& hw : g_io33_fdcan) {
            hw.CREL = 0x32141218U;
            hw.ENDN = 0x87654321U;
            hw.CCCR = FDCAN_CCCR_INIT;
            hw.TXFQS = 3U;
        }
        irq_mask = irq_pending = irq_active = 0U;
        irq_priority[0] = irq_priority[1] = 5U;
        hold_init = hold_add = false;
        recessive_rx = false;
        tx_order[0] = tx_order[1] = tx_order[2] = 0U;
        next_order = 0U;
        forced_put_index = UINT8_MAX;
        unclocked_fdcan_reads = 0U;
        ASSERT_EQ(nx_native_clock_configure(true, 100U), NX_SUCCESS);
        g_io33_rcc.CR = RCC_CR_HSEON | RCC_CR_HSERDY;
        /* Platform owns the APB gate before any FDCAN register/RAM access. */
        g_io33_rcc.APB1ENR1 = RCC_APB1ENR1_FDCANEN;
        g_nx_stm32g474_system = {};
        g_nx_stm32g474_system.rcc = RCC;
        g_nx_stm32g474_system.core_hz = 170000000U;
        g_nx_stm32g474_system.started = true;
        domain.rcc = RCC;
        domain.configuration = FDCAN_CONFIG;
        domain.ram = g_io33_ram;
        domain.kernel_hz = 24000000U;
        controller.registers = FDCAN1;
        controller.domain = &domain;
        controller.filters = filters;
        controller.filter_count = 2U;
        controller.events = events;
        controller.capacity = 8U;
        controller.poll_limit = 4U;
        controller.drain_timeout_us = 20U;
        controller.irq_policy = {NX_IRQ_KERNEL_BASEPRI, 102U, 4U, 5U};
        frame.id = 0x123U;
        frame.format = NX_CAN_STANDARD;
        frame.kind = NX_CAN_DATA;
        frame.wire = NX_CAN_WIRE_FD;
        frame.payload_bytes = 64U;
        frame.brs = true;
        for (size_t i = 0U; i < sizeof(frame.data); ++i)
            frame.data[i] = static_cast<uint8_t>(i + 1U);
        nx_request_initialize(&request.base);
        nx_request_initialize(&other_request.base);
        nx_request_initialize(&third_request.base);
        ASSERT_EQ(nx_can_fd_tx_prepare(&request, &frame, 1000U), NX_SUCCESS);
    }
    void Start() {
        ASSERT_EQ(nx_stm32g474_fdcan_domain_initialize(&domain), NX_SUCCESS);
        ASSERT_EQ(nx_stm32g474_fdcan_initialize(&controller), NX_SUCCESS);
    }
    void TearDown() override {
        hold_init = hold_add = false;
        irq_pending = irq_active = 0U;
        FinishAddScan();
        g_io33_fdcan[0].TXBCF |= g_io33_fdcan[0].TXBRP;
        g_io33_fdcan[0].TXBRP = 0U;
        g_io33_fdcan[0].TXBCR = 0U;
        if (controller.retained) {
            EXPECT_EQ(nx_can_fd_port_stop(&port), NX_SUCCESS);
        }
        if (domain.retained) {
            EXPECT_EQ(nx_stm32g474_fdcan_domain_stop(&domain), NX_SUCCESS);
        }
        EXPECT_EQ(nx_native_clock_configure(false, 0U), NX_SUCCESS);
        leaves = nullptr;
    }
};
TEST_F(G474Fdcan, ExactKernelTimingQueueAndSixtyFourBytesReachPhysicalRam) {
    ASSERT_NO_FATAL_FAILURE(Start());
    EXPECT_EQ(FDCAN_CONFIG->CKDIV, 0U);
    EXPECT_EQ(FDCAN1->NBTP, 0x02020B02U);
    EXPECT_EQ(FDCAN1->DBTP, 0x00000811U);
    EXPECT_EQ(FDCAN1->TSCC & FDCAN_TSCC_TSS, 0U);
    EXPECT_EQ(nx_io33_read(&FDCAN1->TXFQS) &
                  (FDCAN_TXFQS_TFGI | FDCAN_TXFQS_TFFL),
              0U);
    EXPECT_NE(FDCAN1->TXBC & FDCAN_TXBC_TFQM, 0U);
    const uint32_t selected = FDCAN_CCCR_TEST | FDCAN_CCCR_MON |
                              FDCAN_CCCR_FDOE | FDCAN_CCCR_BRSE |
                              FDCAN_CCCR_DAR;
    EXPECT_EQ(FDCAN1->CCCR & selected, selected);
    EXPECT_NE(FDCAN1->TEST & FDCAN_TEST_LBCK, 0U);
    EXPECT_EQ(FDCAN1->CCCR & (FDCAN_CCCR_EFBI | FDCAN_CCCR_NISO), 0U);
    ASSERT_EQ(nx_can_fd_port_submit(&port, &request), NX_SUCCESS);
    const size_t tx = 0x278U / 4U;
    EXPECT_EQ(g_io33_ram[tx], frame.id << 18U);
    EXPECT_EQ((g_io33_ram[tx + 1U] >> 16U) & 15U, 15U);
    EXPECT_NE(g_io33_ram[tx + 1U] & (1U << 21U), 0U);
    EXPECT_NE(g_io33_ram[tx + 1U] & (1U << 20U), 0U);
    EXPECT_EQ(g_io33_ram[tx + 2U], 0x04030201U);
    EXPECT_EQ(g_io33_ram[tx + 17U], 0x403F3E3DU);
    EXPECT_NE(FDCAN1->TXBRP & 1U, 0U);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
}
TEST_F(G474Fdcan, GenuineTransmissionWinsCancellationWithoutReplaying) {
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_EQ(nx_can_fd_port_submit(&port, &request), NX_SUCCESS);
    ASSERT_EQ(nx_can_fd_port_cancel(&port, &request), NX_SUCCESS);
    EXPECT_EQ(FDCAN1->TXBCR & 1U, 1U);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    FDCAN1->TXBRP = 0U;
    FDCAN1->TXBCR = 0U;
    FDCAN1->TXBTO = 1U;
    FDCAN1->TXBCF = 1U;
    ASSERT_EQ(nx_can_fd_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, 64U);
}
TEST_F(G474Fdcan, MissingAbortProofQuarantinesAndNeverResetsTheSharedDomain) {
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_EQ(nx_can_fd_port_submit(&port, &request), NX_SUCCESS);
    ASSERT_EQ(nx_can_fd_port_cancel(&port, &request), NX_SUCCESS);
    ASSERT_EQ(nx_native_clock_advance(20U), NX_SUCCESS);
    ASSERT_EQ(nx_can_fd_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_CALL(external, Write(_, _)).Times(testing::AnyNumber());
    EXPECT_CALL(external, Write(&RCC->APB1RSTR1, _)).Times(0);
    EXPECT_EQ(nx_can_fd_port_stop(&port), NX_ERROR_BUSY);
    EXPECT_EQ(nx_can_fd_port_recover(&port), NX_ERROR_BUSY);
    EXPECT_EQ(nx_stm32g474_fdcan_domain_stop(&domain), NX_ERROR_BUSY);
}
TEST_F(G474Fdcan, FdRxDlcFifteenMeansSixtyFourBytesAndActualEsi) {
    ASSERT_NO_FATAL_FAILURE(Start());
    const size_t rx = 0xB0U / 4U;
    g_io33_ram[rx] = (0x123U << 18U) | (1U << 31U);
    g_io33_ram[rx + 1U] = (15U << 16U) | (1U << 21U) | (1U << 20U);
    for (size_t i = 0U; i < 16U; ++i)
        g_io33_ram[rx + 2U + i] =
            0x04030201U + static_cast<uint32_t>(i) * 0x04040404U;
    FDCAN1->RXF0S = 1U;
    FDCAN1->IR |= FDCAN_IR_RF0N;
    DeliverLine0();
    ASSERT_EQ(nx_can_fd_port_service(&port), NX_SUCCESS);
    nx_can_fd_event_t value = {};
    size_t count = 0U;
    ASSERT_EQ(nx_can_fd_port_read_events(&port, &value, 1U, &count),
              NX_SUCCESS);
    EXPECT_EQ(count, 1U);
    EXPECT_EQ(value.frame.payload_bytes, 64U);
    EXPECT_EQ(value.frame.data[63], 64U);
    EXPECT_EQ(value.raw_dlc, 15U);
    EXPECT_TRUE(value.error_passive);
    EXPECT_EQ(value.hardware_timestamp, 0U);
    EXPECT_EQ(value.acquired_us, 100U);
    EXPECT_NE(value.flags & NX_CAN_FD_TIMESTAMP_UNAVAILABLE, 0U);
    EXPECT_EQ(FDCAN1->RXF0S & FDCAN_RXF0S_F0FL, 0U);
}
TEST_F(G474Fdcan, ColdDomainRejectsLivePeerBeforeClockOrMessageRamWrites) {
    g_io33_fdcan[1].CCCR = 0U;
    g_io33_ram[212U] = 0xA5A5A5A5U;
    EXPECT_CALL(external, Write(_, _)).Times(0);
    EXPECT_EQ(nx_stm32g474_fdcan_domain_initialize(&domain), NX_ERROR_BUSY);
    EXPECT_EQ(g_io33_ram[212U], 0xA5A5A5A5U);
}
TEST_F(G474Fdcan, InvalidPayloadRejectsBeforeAnyHardwareEffectOrLoan) {
    ASSERT_NO_FATAL_FAILURE(Start());
    request.frame.payload_bytes = 9U;
    EXPECT_CALL(external, Write(_, _)).Times(0);
    EXPECT_EQ(nx_can_fd_port_submit(&port, &request), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    testing::Mock::VerifyAndClearExpectations(&external);
}

TEST_F(G474Fdcan, PendingRequestAndLatchedOutcomeAreIndependentDrainProofs) {
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_EQ(nx_can_fd_port_submit(&port, &request), NX_SUCCESS);
    ASSERT_EQ(nx_can_fd_port_cancel(&port, &request), NX_SUCCESS);
    FDCAN1->TXBTO = 1U;
    FDCAN1->TXBCF = 1U;
    /* A stale outcome cannot revoke a still-pending physical lease. */
    ASSERT_EQ(nx_can_fd_port_service(&port), NX_SUCCESS);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    FDCAN1->TXBRP = 0U;
    FDCAN1->TXBCR = 0U;
    FDCAN1->TXBTO = FDCAN1->TXBCF = 0U;
    /* Empty alone also lacks transmission/cancellation proof. */
    ASSERT_EQ(nx_can_fd_port_service(&port), NX_SUCCESS);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    FDCAN1->TXBCF = 1U;
    ASSERT_EQ(nx_can_fd_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(request.base.transferred, 0U);
}
TEST_F(G474Fdcan, CancellationFinishedWithoutTransmissionReturnsNoPayload) {
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_EQ(nx_can_fd_port_submit(&port, &request), NX_SUCCESS);
    ASSERT_EQ(nx_can_fd_port_cancel(&port, &request), NX_SUCCESS);
    EXPECT_EQ(FDCAN1->TXBCR & 1U, 1U);
    FDCAN1->TXBRP = 0U;
    FDCAN1->TXBCR = 0U;
    FDCAN1->TXBCF = 1U;
    ASSERT_EQ(nx_can_fd_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(request.base.transferred, 0U);
}
TEST_F(G474Fdcan, AnOldCompletionInterruptCannotSettleAReusedPhysicalSlot) {
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_EQ(nx_can_fd_port_submit(&port, &request), NX_SUCCESS);
    ASSERT_NE(RunOneBusAttempt(), 0U);
    ASSERT_EQ(nx_can_fd_port_service(&port), NX_SUCCESS);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    ASSERT_EQ(nx_can_fd_tx_prepare(&other_request, &frame, 1000U), NX_SUCCESS);
    ASSERT_EQ(nx_can_fd_port_submit(&port, &other_request), NX_SUCCESS);
    /* IR.TC may still be latched; new TXBAR cleared this slot's old TO/CF. */
    FDCAN1->IR |= FDCAN_IR_TC;
    DeliverLine0();
    ASSERT_EQ(nx_can_fd_port_service(&port), NX_SUCCESS);
    EXPECT_NE(nx_request_state(&other_request.base), NX_REQUEST_SETTLED);
    ASSERT_NE(RunOneBusAttempt(), 0U);
    ASSERT_EQ(nx_can_fd_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&other_request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(other_request.base.result, NX_SUCCESS);
}
TEST_F(G474Fdcan, QueueUsesPhysicalIdentifierPriorityRatherThanAdmissionOrder) {
    ASSERT_NO_FATAL_FAILURE(Start());
    frame.id = 0x700U;
    ASSERT_EQ(nx_can_fd_tx_prepare(&request, &frame, 1000U), NX_SUCCESS);
    frame.id = 0x100U;
    ASSERT_EQ(nx_can_fd_tx_prepare(&other_request, &frame, 1000U), NX_SUCCESS);
    frame.id = 0x500U;
    ASSERT_EQ(nx_can_fd_tx_prepare(&third_request, &frame, 1000U), NX_SUCCESS);
    ASSERT_EQ(nx_can_fd_port_submit(&port, &request), NX_SUCCESS);
    ASSERT_EQ(nx_can_fd_port_submit(&port, &other_request), NX_SUCCESS);
    ASSERT_EQ(nx_can_fd_port_submit(&port, &third_request), NX_SUCCESS);
    EXPECT_NE(nx_io33_read(&FDCAN1->TXFQS) & FDCAN_TXFQS_TFQF, 0U);
    ASSERT_NE(RunOneBusAttempt(), 0U);
    ASSERT_EQ(nx_can_fd_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&other_request.base), NX_REQUEST_SETTLED);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_NE(nx_request_state(&third_request.base), NX_REQUEST_SETTLED);
}
TEST_F(G474Fdcan, AddDuringHardwareScanIsNeitherEmptySuccessNorFreeStorage) {
    ASSERT_NO_FATAL_FAILURE(Start());
    hold_add = true;
    ASSERT_EQ(nx_can_fd_port_submit(&port, &request), NX_SUCCESS);
    EXPECT_EQ(FDCAN1->TXBRP, 0U);
    EXPECT_NE(FDCAN1->TXBAR & 1U, 0U);
    ASSERT_EQ(nx_can_fd_port_service(&port), NX_SUCCESS);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    hold_add = false;
    FinishAddScan();
    ASSERT_NE(RunOneBusAttempt(), 0U);
    ASSERT_EQ(nx_can_fd_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
}
TEST_F(G474Fdcan, OnlyLineZeroPublishesAndARejectedWakeKeepsTheOriginalSink) {
    ASSERT_NO_FATAL_FAILURE(Start());
    EXPECT_EQ(FDCAN1->ILS, 0U);
    EXPECT_EQ(FDCAN1->ILE, static_cast<uint32_t>(FDCAN_ILE_EINT0));
    EXPECT_EQ(irq_mask, 1U);
    /* Reserved but disabled line1 is not a kernel publisher. */
    irq_priority[1] = 0U;
    ASSERT_EQ(nx_can_fd_port_attach_wake(&port, &old_wake, 5U), NX_SUCCESS);
    irq_priority[0] = 4U;
    EXPECT_EQ(nx_can_fd_port_attach_wake(&port, &new_wake, 5U),
              NX_ERROR_PERMISSION);
    irq_priority[0] = 5U;
    ASSERT_EQ(nx_can_fd_port_submit(&port, &request), NX_SUCCESS);
    ASSERT_NE(RunOneBusAttempt(), 0U);
    DeliverLine0();
    ASSERT_EQ(nx_can_fd_port_service(&port), NX_SUCCESS);
    EXPECT_GT(old_hints, 0U);
    EXPECT_EQ(new_hints, 0U);
}
TEST_F(G474Fdcan, AForeignPendingLineIsRejectedBeforeControllerEffects) {
    ASSERT_EQ(nx_stm32g474_fdcan_domain_initialize(&domain), NX_SUCCESS);
    irq_pending = 2U;
    EXPECT_CALL(external, Write(_, _)).Times(0);
    EXPECT_EQ(nx_stm32g474_fdcan_initialize(&controller), NX_ERROR_BUSY);
    EXPECT_EQ(irq_mask, 0U);
    testing::Mock::VerifyAndClearExpectations(&external);
}

TEST_F(G474Fdcan, AnInvalidPhysicalPutIndexNeverWritesOutsideThreeElements) {
    ASSERT_NO_FATAL_FAILURE(Start());
    forced_put_index = 3U;
    g_io33_ram[212U] = 0xA5A5A5A5U;
    EXPECT_CALL(external, Write(_, _)).Times(0);
    EXPECT_EQ(nx_can_fd_port_submit(&port, &request), NX_ERROR_IO);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(g_io33_ram[212U], 0xA5A5A5A5U);
    testing::Mock::VerifyAndClearExpectations(&external);
}

TEST_F(G474Fdcan, AClosedPlatformClockRejectsBeforeAnyFdcanRegisterOrRamRead) {
    g_io33_rcc.APB1ENR1 = 0U;
    EXPECT_CALL(external, Write(_, _)).Times(0);
    EXPECT_EQ(nx_stm32g474_fdcan_domain_initialize(&domain), NX_ERROR_STATE);
    EXPECT_EQ(unclocked_fdcan_reads, 0U);
    EXPECT_FALSE(domain.retained);
    EXPECT_EQ(irq_mask, 0U);
    testing::Mock::VerifyAndClearExpectations(&external);
}

TEST_F(G474Fdcan, BorrowedRequestStorageCannotBecomeAnOutput) {
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_EQ(nx_can_fd_port_submit(&port, &request), NX_SUCCESS);
    const auto original = request.frame;
    EXPECT_EQ(nx_can_fd_port_state(
                  &port, reinterpret_cast<nx_can_fd_state_t*>(&request.frame)),
              NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&request.frame, &original, sizeof(original)), 0);
    /* Repair a faulty implementation's value overwrite before fixture drain. */
    request.frame = original;
    nx_can_fd_event_t output = {};
    EXPECT_EQ(nx_can_fd_port_read_events(&port, &output, 1U,
                                         &request.base.transferred),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
}

TEST_F(G474Fdcan, PeerClockAndReadonlyOpsCannotBecomeOutputs) {
    ASSERT_NO_FATAL_FAILURE(Start());
    uint8_t peer[sizeof(*FDCAN2)] = {};
    uint8_t system[sizeof(g_nx_stm32g474_system)] = {};
    std::memcpy(peer, FDCAN2, sizeof(peer));
    std::memcpy(system, &g_nx_stm32g474_system, sizeof(system));
    EXPECT_EQ(
        nx_can_fd_port_state(&port, reinterpret_cast<nx_can_fd_state_t*>(
                                        const_cast<uint32_t*>(&FDCAN2->CCCR))),
        NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(peer, FDCAN2, sizeof(peer)), 0);
    std::memcpy(FDCAN2, peer, sizeof(peer));
    EXPECT_EQ(nx_can_fd_port_state(&port, reinterpret_cast<nx_can_fd_state_t*>(
                                              &g_nx_stm32g474_system)),
              NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(system, &g_nx_stm32g474_system, sizeof(system)), 0);
    std::memcpy(&g_nx_stm32g474_system, system, sizeof(system));
    EXPECT_EXIT(
        {
            auto* output = reinterpret_cast<nx_can_fd_state_t*>(
                const_cast<nx_can_fd_ops_t*>(&nx_stm32g474_fdcan_ops));
            const auto result = nx_can_fd_port_state(&port, output);
            _exit(result == NX_ERROR_INVALID ? 0 : 4);
        },
        testing::ExitedWithCode(0), "");
}

TEST_F(G474Fdcan, PublicFaceAndNvicControlCannotBecomeOutputs) {
    ASSERT_NO_FATAL_FAILURE(Start());
    nx_can_fd_port_t face = port;
    static_assert(sizeof(nx_can_fd_state_t) <= sizeof(face));
    EXPECT_EQ(nx_can_fd_port_state(&face,
                                   reinterpret_cast<nx_can_fd_state_t*>(&face)),
              NX_ERROR_INVALID);
    EXPECT_EQ(face.ops, port.ops);
    EXPECT_EQ(face.context, port.context);
    EXPECT_EXIT(
        {
            void* page =
                mmap(reinterpret_cast<void*>(0xE000E000U), 4096U,
                     PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
            if (page == MAP_FAILED) {
                _exit(2);
            }
            auto* output = reinterpret_cast<nx_can_fd_state_t*>(0xE000E100U);
            std::memset(output, 0xA5, sizeof(*output));
            uint8_t before[sizeof(*output)] = {};
            std::memcpy(before, output, sizeof(before));
            const auto result = nx_can_fd_port_state(&port, output);
            const bool unchanged =
                std::memcmp(before, output, sizeof(before)) == 0;
            _exit(result == NX_ERROR_INVALID && unchanged ? 0 : 3);
        },
        testing::ExitedWithCode(0), "");
}

TEST_F(G474Fdcan, StopLatchesFinalProtocolStateBeforeConfigurationClearsFacts) {
    ASSERT_NO_FATAL_FAILURE(Start());
    FDCAN1->PSR = FDCAN_PSR_BO | FDCAN_PSR_EP | FDCAN_PSR_EW |
                  (3U << FDCAN_PSR_DLEC_Pos) | 5U;
    FDCAN1->ECR = (17U << FDCAN_ECR_REC_Pos) | 23U;
    ASSERT_EQ(nx_can_fd_port_stop(&port), NX_SUCCESS);
    nx_can_fd_state_t value = {};
    ASSERT_EQ(nx_can_fd_port_state(&port, &value), NX_SUCCESS);
    EXPECT_TRUE(value.controller.bus_off);
    EXPECT_TRUE(value.error_passive);
    EXPECT_TRUE(value.error_warning);
    EXPECT_EQ(value.controller.last_error, 5U);
    EXPECT_EQ(value.data_last_error, 3U);
    EXPECT_EQ(value.controller.transmit_errors, 23U);
    EXPECT_EQ(value.controller.receive_errors, 17U);
    EXPECT_FALSE(value.controller.started);
}

TEST_F(G474Fdcan,
       NormalModeUsesExternalAcknowledgementAndPreservesFormatAfterRecovery) {
    controller.mode = NX_STM32G474_FDCAN_NORMAL;
    ASSERT_NO_FATAL_FAILURE(Start());
    EXPECT_EQ(FDCAN1->CCCR & (FDCAN_CCCR_TEST | FDCAN_CCCR_MON |
                              FDCAN_CCCR_ASM | FDCAN_CCCR_NISO),
              0U);
    EXPECT_EQ(FDCAN1->TEST & FDCAN_TEST_LBCK, 0U);
    const uint32_t format = FDCAN_CCCR_FDOE | FDCAN_CCCR_BRSE | FDCAN_CCCR_DAR;
    EXPECT_EQ(FDCAN1->CCCR & format, format);
    ASSERT_EQ(nx_can_fd_port_submit(&port, &request), NX_SUCCESS);
    EXPECT_EQ(g_io33_ram[158U + 17U], 0x403F3E3DU);
    ASSERT_NE(RunOneBusAttempt(true), 0U);
    ASSERT_EQ(nx_can_fd_port_service(&port), NX_SUCCESS);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, 64U);
    ASSERT_EQ(nx_can_fd_port_recover(&port), NX_SUCCESS);
    EXPECT_EQ(FDCAN1->CCCR & (FDCAN_CCCR_TEST | FDCAN_CCCR_MON), 0U);
    EXPECT_EQ(FDCAN1->TEST & FDCAN_TEST_LBCK, 0U);
    EXPECT_EQ(FDCAN1->CCCR & format, format);
}

TEST_F(G474Fdcan, NormalModeAckErrorCompletesOnceWithoutReplay) {
    controller.mode = NX_STM32G474_FDCAN_NORMAL;
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_EQ(nx_can_fd_port_submit(&port, &request), NX_SUCCESS);
    ASSERT_NE(RunOneBusAttempt(false), 0U);
    DeliverLine0();
    ASSERT_EQ(nx_can_fd_port_service(&port), NX_SUCCESS);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
    EXPECT_EQ(FDCAN1->TXBTO, 0U);
    EXPECT_EQ(FDCAN1->TXBCF & 1U, 1U);
    nx_can_fd_state_t observed = {};
    ASSERT_EQ(nx_can_fd_port_state(&port, &observed), NX_SUCCESS);
    EXPECT_EQ(observed.controller.last_error, 3U);
    EXPECT_EQ(observed.controller.transmit_errors, 8U);
    EXPECT_CALL(external, Write(_, _)).Times(testing::AnyNumber());
    EXPECT_CALL(external, Write(&FDCAN1->TXBAR, _)).Times(0);
    EXPECT_EQ(nx_can_fd_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_can_fd_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(FDCAN1->TXBRP, 0U);
    testing::Mock::VerifyAndClearExpectations(&external);
}

TEST_F(G474Fdcan, NormalModeReceivesExtendedFdThroughSecondPhysicalFifo) {
    controller.mode = NX_STM32G474_FDCAN_NORMAL;
    ASSERT_NO_FATAL_FAILURE(Start());
    EXPECT_EQ(FDCAN1->CCCR & (FDCAN_CCCR_TEST | FDCAN_CCCR_MON), 0U);
    const size_t rx = 0x188U / 4U;
    g_io33_ram[rx] = 0x1ABCDEU | (1U << 30U) | (1U << 31U);
    g_io33_ram[rx + 1U] = (9U << 16U) | (1U << 21U) | (1U << 20U);
    g_io33_ram[rx + 2U] = 0x04030201U;
    g_io33_ram[rx + 3U] = 0x08070605U;
    g_io33_ram[rx + 4U] = 0x0C0B0A09U;
    FDCAN1->RXF1S = 1U;
    FDCAN1->IR |= FDCAN_IR_RF1N;
    DeliverLine0();
    ASSERT_EQ(nx_can_fd_port_service(&port), NX_SUCCESS);
    nx_can_fd_event_t event = {};
    size_t count = 0U;
    ASSERT_EQ(nx_can_fd_port_read_events(&port, &event, 1U, &count),
              NX_SUCCESS);
    ASSERT_EQ(count, 1U);
    EXPECT_EQ(event.fifo, 1U);
    EXPECT_EQ(event.frame.format, NX_CAN_EXTENDED);
    EXPECT_EQ(event.frame.id, 0x1ABCDEU);
    EXPECT_EQ(event.frame.payload_bytes, 12U);
    EXPECT_EQ(event.raw_dlc, 9U);
    EXPECT_TRUE(event.frame.brs);
    EXPECT_TRUE(event.error_passive);
    EXPECT_EQ(event.frame.data[11], 12U);
    EXPECT_EQ(event.acquired_us, 100U);
    EXPECT_EQ(FDCAN1->RXF1S & FDCAN_RXF1S_F1FL, 0U);
}

TEST_F(G474Fdcan, UnknownPhysicalModeRejectsBeforeEffectsOrBorrow) {
    ASSERT_EQ(nx_stm32g474_fdcan_domain_initialize(&domain), NX_SUCCESS);
    controller.mode = NX_STM32G474_FDCAN_MODE_COUNT;
    size_t writes = 0U;
    EXPECT_CALL(external, Write(_, _))
        .WillRepeatedly([&writes](volatile uint32_t*, uint32_t) { ++writes; });
    EXPECT_EQ(nx_stm32g474_fdcan_initialize(&controller), NX_ERROR_INVALID);
    EXPECT_EQ(writes, 0U);
    EXPECT_FALSE(controller.retained);
    EXPECT_EQ(domain.acquired_mask, 0U);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    testing::Mock::VerifyAndClearExpectations(&external);
}

TEST_F(G474Fdcan,
       RecessiveReadOnlyRxDoesNotInvalidateOwnedTestControlReadback) {
    recessive_rx = true;
    ASSERT_NO_FATAL_FAILURE(Start());
    EXPECT_NE(nx_io33_read(&FDCAN1->TEST) & FDCAN_TEST_RX, 0U);
    EXPECT_NE(nx_io33_read(&FDCAN1->TEST) & FDCAN_TEST_LBCK, 0U);
}
