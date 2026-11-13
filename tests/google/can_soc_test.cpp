/**
 * \file            can_soc_test.cpp
 * \brief           Two actual SDK bindings execute the production CAN engine
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "can.h"
#include "nexus/io/native/model.h"
#include "stm32f407_can.h"
#include "stm32f407xx.h"
}
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>

class CanSoc : public testing::Test {
  protected:
    void* mapping = MAP_FAILED;
    nx_can_controller_t controller = {};
    nx_can_filter_domain_t domain = {};
    nx_can_filter_t filters[2] = {
        {0x123U, 0x7FFU, NX_CAN_STANDARD, 1U, 0U, 0U, 0U, 0U, false, true},
        {0x321U, 0x7FFU, NX_CAN_STANDARD, 1U, 14U, 1U, 1U, 0U, false, true}};
    nx_can_event_t events[8] = {};
    const nx_can_port_t port = {&nx_classic_can_ops, &controller};
    nx_can_tx_request_t request = {};
    nx_can_tx_request_t second_request = {};
    nx_can_frame_t frame = {
        0x1234567U, NX_CAN_EXTENDED, NX_CAN_DATA, 8U, {1, 2, 3, 4, 5, 6, 7, 8}};
    nx_native_can_registers_t* hardware = nullptr;

    nx_result_t Initialize(nx_can_controller_t* state) {
        return nx_stm32_can_initialize(state);
    }
    nx_result_t SelectController(unsigned index) {
        nx_result_t result = nx_can_port_stop(&port);
        if (result != NX_SUCCESS) {
            return result;
        }
        controller.index = static_cast<uint8_t>(index);
        controller.registers = 0U;
        hardware = reinterpret_cast<nx_native_can_registers_t*>(domain.primary +
                                                                index * 0x400U);
        hardware->registers[2] = 7U << 26U;
        return Initialize(&controller);
    }
    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(UINT32_C(0x40000000)));
        ASSERT_EQ(nx_native_clock_configure(true, 100U), NX_SUCCESS);
        domain.filters = filters;
        domain.filter_count = 2U;
        domain.split = 14U;
        ASSERT_EQ(nx_stm32_can_domain_initialize(&domain), NX_SUCCESS);
        controller.timing = {500000U, 6U, 11U, 2U, 2U};
        hardware = reinterpret_cast<nx_native_can_registers_t*>(domain.primary);
        hardware->registers[2] = 7U << 26U;
        controller.domain = &domain;
        controller.events = events;
        controller.capacity = 8U;
        controller.poll_limit = 4U;
        controller.drain_timeout_us = 20U;
        controller.irq_policy = {NX_IRQ_KERNEL_BASEPRI, 82U, 4U, 5U};
        for (unsigned i = 0; i < 4U; ++i) {
            controller.sources[i].priority = 5U;
        }
        nx_request_initialize(&request.base);
        nx_request_initialize(&second_request.base);
        ASSERT_EQ(nx_can_tx_prepare(&request, &frame, 200U), NX_SUCCESS);
        ASSERT_EQ(Initialize(&controller), NX_SUCCESS);
    }
    void TearDown() override {
        if (mapping == MAP_FAILED) {
            return;
        }
        hardware->hold_abort = false;
        hardware->hold_enter = false;
        hardware->hold_exit = false;
        hardware->hold_irq_disable = false;
        EXPECT_EQ(nx_can_port_stop(&port), NX_SUCCESS);
        EXPECT_EQ(nx_can_filter_domain_stop(&domain), NX_SUCCESS);
        EXPECT_EQ(munmap(mapping, 0x80000U), 0);
    }
};

TEST_F(CanSoc, ActualSdkClockModeTimingAndVectorsMatchTheMechanism) {
    EXPECT_EQ(controller.registers, CAN1_BASE);
    EXPECT_EQ(controller.clock_hz, 42000000U);
    EXPECT_NE(RCC->APB1ENR & RCC_APB1ENR_CAN1EN, 0U);
    EXPECT_EQ(CAN1->BTR, 5U | 10U << 16U | 1U << 20U | 1U << 24U);
    EXPECT_NE(CAN1->MCR & CAN_MCR_NART, 0U);
    EXPECT_EQ(CAN1->MCR & CAN_MCR_ABOM, 0U);
    EXPECT_EQ(hardware->enabled_irqs, 15U);
    for (unsigned i = 0; i < 4U; ++i) {
        EXPECT_EQ(controller.sources[i].irq_number, 19U + i);
    }
}
TEST_F(CanSoc, MailboxPackingAndSdkCompletionProveActualWireSuccess) {
    ASSERT_EQ(nx_can_port_submit(&port, &request), NX_SUCCESS);
    EXPECT_EQ(CAN1->sTxMailBox[0].TIR, (frame.id << 3U) | 5U);
    EXPECT_EQ(CAN1->sTxMailBox[0].TDLR, 0x04030201U);
    EXPECT_EQ(CAN1->sTxMailBox[0].TDHR, 0x08070605U);
    CAN1->sTxMailBox[0].TIR &= ~static_cast<uint32_t>(CAN_TI0R_TXRQ);
    CAN1->TSR |= CAN_TSR_RQCP0 | CAN_TSR_TXOK0 | CAN_TSR_TME0;
    nx_can_controller_irq(&controller);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, 8U);
}
TEST_F(CanSoc, MaximumExtendedIdentifierRespectsExactFamilyMailboxRestriction) {
    for (unsigned index = 0U; index < 2U; ++index) {
        ASSERT_EQ(SelectController(index), NX_SUCCESS);
        ASSERT_EQ(nx_can_tx_prepare(&request, &frame, 200U), NX_SUCCESS);
        ASSERT_EQ(nx_can_port_submit(&port, &request), NX_SUCCESS);
        nx_can_frame_t maximum = frame;
        maximum.id = 0x1FFFFFFFU;
        ASSERT_EQ(nx_can_tx_prepare(&second_request, &maximum, 200U),
                  NX_SUCCESS);
        ASSERT_EQ(nx_can_port_submit(&port, &second_request), NX_SUCCESS);
        EXPECT_EQ(controller.slots[1].request, &second_request);
        ASSERT_EQ(nx_can_port_stop(&port), NX_SUCCESS);
    }
}
TEST_F(CanSoc, MaximumExtendedIdentifierUsesMailboxZeroWithoutReducingPeers) {
    for (unsigned index = 0U; index < 2U; ++index) {
        ASSERT_EQ(SelectController(index), NX_SUCCESS);
        nx_can_frame_t maximum = frame;
        maximum.id = 0x1FFFFFFFU;
        ASSERT_EQ(nx_can_tx_prepare(&request, &maximum, 200U), NX_SUCCESS);
        ASSERT_EQ(nx_can_port_submit(&port, &request), NX_SUCCESS);
        EXPECT_EQ(controller.slots[0].request, &request);
        EXPECT_EQ(hardware->registers[0x180U / 4U], 0xFFFFFFFDU);
        ASSERT_EQ(nx_can_tx_prepare(&second_request, &frame, 200U), NX_SUCCESS);
        ASSERT_EQ(nx_can_port_submit(&port, &second_request), NX_SUCCESS);
        EXPECT_EQ(controller.slots[1].request, &second_request);
        ASSERT_EQ(nx_can_port_stop(&port), NX_SUCCESS);
    }
}
TEST_F(CanSoc, MaximumExtendedRemoteIdentifierUsesTheSamePhysicalRestriction) {
    for (unsigned index = 0U; index < 2U; ++index) {
        ASSERT_EQ(SelectController(index), NX_SUCCESS);
        ASSERT_EQ(nx_can_tx_prepare(&request, &frame, 200U), NX_SUCCESS);
        ASSERT_EQ(nx_can_port_submit(&port, &request), NX_SUCCESS);
        nx_can_frame_t maximum = frame;
        maximum.id = 0x1FFFFFFFU;
        maximum.kind = NX_CAN_REMOTE;
        ASSERT_EQ(nx_can_tx_prepare(&second_request, &maximum, 200U),
                  NX_SUCCESS);
        ASSERT_EQ(nx_can_port_submit(&port, &second_request), NX_SUCCESS);
        EXPECT_EQ(controller.slots[1].request, &second_request);
        ASSERT_EQ(nx_can_port_stop(&port), NX_SUCCESS);
    }
}
TEST_F(CanSoc, FifoOrderAdmissionMatchesReviewedFamilyAbortGuarantees) {
    ASSERT_EQ(nx_can_port_stop(&port), NX_SUCCESS);
    controller.fifo_order = true;
    ASSERT_EQ(Initialize(&controller), NX_SUCCESS);
    EXPECT_NE(hardware->registers[0] & 4U, 0U);
}
TEST_F(CanSoc, UnacknowledgedAbortRetainsRequestUntilActualSdkCompletion) {
    hardware->hold_abort = true;
    ASSERT_EQ(nx_can_port_submit(&port, &request), NX_SUCCESS);
    ASSERT_EQ(nx_can_port_cancel(&port, &request), NX_SUCCESS);
    ASSERT_EQ(nx_native_clock_advance(20U), NX_SUCCESS);
    ASSERT_EQ(nx_can_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(nx_can_port_stop(&port), NX_ERROR_BUSY);
    hardware->hold_abort = false;
    EXPECT_EQ(nx_can_port_stop(&port), NX_SUCCESS);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
}
TEST_F(CanSoc, SecondControllerDependsOnPrimaryClockWithoutResettingPeer) {
    nx_can_controller_t peer = controller;
    peer.snapshot = {};
    peer.index = 1U;
    peer.registers = 0U;
    nx_can_event_t peer_events[2] = {};
    peer.events = peer_events;
    peer.capacity = 2U;
    auto* peer_hardware =
        reinterpret_cast<nx_native_can_registers_t*>(domain.primary + 0x400U);
    peer_hardware->registers[2] = 7U << 26U;
    ASSERT_EQ(Initialize(&peer), NX_SUCCESS);
    EXPECT_EQ(domain.active_mask, 3U);
    EXPECT_EQ(nx_can_filter_domain_stop(&domain), NX_ERROR_BUSY);
    EXPECT_EQ(hardware->registers[0x31CU / 4U], 0U);
    ASSERT_EQ(nx_can_port_stop(&port), NX_SUCCESS);
    EXPECT_EQ(RCC->APB1ENR & (RCC_APB1ENR_CAN1EN | RCC_APB1ENR_CAN2EN),
              RCC_APB1ENR_CAN1EN | RCC_APB1ENR_CAN2EN);
    EXPECT_EQ(CAN1->sFilterRegister[14].FR1, 0x321U << 21U);
    EXPECT_EQ(domain.active_mask, 2U);
    nx_can_port_t peer_port = {&nx_classic_can_ops, &peer};
    EXPECT_EQ(nx_can_port_stop(&peer_port), NX_SUCCESS);
}
TEST_F(CanSoc, FifoMailboxDlcAndReleaseUseExactSdkOffsets) {
    hardware->receive_count[1] = 1U;
    hardware->receive[1][0][0] = (0x456U << 21U) | 2U;
    hardware->receive[1][0][1] = 15U | 14U << 8U;
    hardware->registers[0x10U / 4U] = 1U;
    for (unsigned i = 0; i < 4U; ++i) {
        hardware->registers[0x1C0U / 4U + i] = hardware->receive[1][0][i];
    }
    nx_can_controller_irq(&controller);
    EXPECT_EQ(hardware->receive_count[1], 0U);
    nx_can_event_t event = {};
    size_t count = 0U;
    ASSERT_EQ(nx_can_port_read_events(&port, &event, 1U, &count), NX_SUCCESS);
    EXPECT_EQ(event.frame.kind, NX_CAN_REMOTE);
    EXPECT_EQ(event.frame.id, 0x456U);
    EXPECT_EQ(event.raw_dlc, 15U);
    EXPECT_EQ(event.frame.dlc, 8U);
    EXPECT_EQ(event.frame.data[0], 0U);
}
TEST_F(CanSoc, FailedExitAcknowledgementRetainsDomainResponsibility) {
    ASSERT_EQ(nx_can_port_stop(&port), NX_SUCCESS);
    hardware->hold_exit = true;
    EXPECT_EQ(Initialize(&controller), NX_ERROR_IO);
    EXPECT_TRUE(domain.retained);
    EXPECT_EQ(domain.active_mask, 1U);
    EXPECT_EQ(nx_can_filter_domain_stop(&domain), NX_ERROR_BUSY);
    hardware->hold_exit = false;
    EXPECT_EQ(nx_can_port_stop(&port), NX_SUCCESS);
}

TEST_F(CanSoc, FalseCpuPriorityFactsRejectBeforeReenablingHardware) {
    ASSERT_EQ(nx_can_port_stop(&port), NX_SUCCESS);
    const uint32_t mode = hardware->registers[0];
    controller.irq_policy.priority_bits = 5U;
    EXPECT_EQ(Initialize(&controller), NX_ERROR_INVALID);
    EXPECT_FALSE(controller.snapshot.started);
    EXPECT_EQ(hardware->registers[0], mode);
    controller.irq_policy.priority_bits = 4U;
}
TEST_F(CanSoc, ForeignPrimaryFilterDomainRejectsWithoutControllerEffects) {
    ASSERT_EQ(nx_can_port_stop(&port), NX_SUCCESS);
    domain.primary += 0x400U;
    const uint32_t mode = hardware->registers[0];
    EXPECT_EQ(Initialize(&controller), NX_ERROR_INVALID);
    EXPECT_FALSE(controller.snapshot.started);
    EXPECT_EQ(hardware->registers[0], mode);
    domain.primary -= 0x400U;
}
