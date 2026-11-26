/**
 * \file            cia402_test.cpp
 * \brief           Real CANopen object traffic and selected drive transitions
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "cia402_test_support.h"
#include "nexus/components/cia402.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <deque>
#include <gtest/gtest.h>
#include <vector>

namespace {
using nexus_drive_test::DriveBus;

class Drive : public testing::Test {
  protected:
    DriveBus bus;
    nx_cia402_t drive{};
    nx_canopen_t node{};
    nx_canopen_config_t transport{};
    nx_canopen_report_t report{};
    nx_cia402_feedback_t feedback{};
    nx_cia402_action_t action{};
    std::array<nx_canopen_tx_slot_t, 8> tx{};
    std::vector<max_align_t> workspace;
    void SetUp() override {
        const nx_cia402_config_t profile{0.01F, 1000U, 200U, 300U, 500U};
        ASSERT_EQ(nx_cia402_initialize(&drive, &profile), NX_SUCCESS);
        transport.port = &bus.port;
        transport.clock = {DriveBus::clock, &bus};
        transport.tx_slots = tx.data();
        transport.tx_capacity = tx.size();
        transport.receive_budget = 4U;
        transport.maximum_step_us = 100000U;
        transport.producer_heartbeat_ms = 100U;
        transport.consumer_heartbeat_ms = 100U;
        transport.sdo_timeout_ms = 200U;
        transport.node_id = 18U;
        transport.peer_node_id = 19U;
        const auto bytes = nx_canopen_workspace_bytes(NX_CIA402_OBJECT_COUNT);
        workspace.resize((bytes + sizeof(max_align_t) - 1U) /
                         sizeof(max_align_t));
        ASSERT_EQ(nx_cia402_node_init(&drive, &node, &transport,
                                      workspace.data(),
                                      workspace.size() * sizeof(max_align_t)),
                  NX_SUCCESS);
        feedback.valid = true;
        feedback.ready = true;
        feedback.reset_permitted = true;
    }
    nx_result_t step() {
        bus.now += 1000U;
        const auto result = nx_canopen_step(&node, bus.now + 50000U, &report);
        if (result == NX_SUCCESS) {
            return nx_cia402_update(&drive, &report, &feedback, &action);
        }
        return result;
    }
    void operational() {
        ASSERT_EQ(step(), NX_SUCCESS);
        bus.push(0U, {1U, 18U});
        ASSERT_EQ(step(), NX_SUCCESS);
        ASSERT_EQ(report.nmt, NX_CANOPEN_OPERATIONAL);
    }
    void target(uint16_t control, int8_t mode, int32_t velocity) {
        const uint32_t raw = static_cast<uint32_t>(velocity);
        bus.push(0x212U, {static_cast<uint8_t>(control),
                          static_cast<uint8_t>(control >> 8U),
                          static_cast<uint8_t>(mode), static_cast<uint8_t>(raw),
                          static_cast<uint8_t>(raw >> 8U),
                          static_cast<uint8_t>(raw >> 16U),
                          static_cast<uint8_t>(raw >> 24U)});
    }
    void enable() {
        operational();
        feedback.mode = NX_CIA402_PROFILE_VELOCITY;
        target(6U, 3, 250);
        ASSERT_EQ(step(), NX_SUCCESS);
        EXPECT_EQ(drive.core.statusword & 0x6fU, 0x21U);
        target(7U, 3, 250);
        ASSERT_EQ(step(), NX_SUCCESS);
        EXPECT_TRUE(action.enable_voltage);
        EXPECT_EQ(drive.core.statusword & 0x6fU, 0x21U);
        feedback.voltage_enabled = true;
        ASSERT_EQ(step(), NX_SUCCESS);
        EXPECT_EQ(drive.core.statusword & 0x6fU, 0x23U);
        target(15U, 3, 250);
        ASSERT_EQ(step(), NX_SUCCESS);
        EXPECT_TRUE(action.enable_operation);
        EXPECT_EQ(drive.core.statusword & 0x6fU, 0x23U);
        feedback.operation_enabled = true;
        ASSERT_EQ(step(), NX_SUCCESS);
        EXPECT_EQ(drive.core.statusword & 0x6fU, 0x27U);
    }
    void TearDown() override {
        EXPECT_EQ(nx_canopen_stop(&node), NX_SUCCESS);
        EXPECT_EQ(nx_cia402_release(&drive), NX_SUCCESS);
    }
};

TEST_F(Drive, StandardObjectsUseRealExpeditedSdoAndReadOnlyAccess) {
    ASSERT_EQ(step(), NX_SUCCESS);
    bus.push(0x612U, {0x2fU, 0x60U, 0x60U, 0U, 3U, 0U, 0U, 0U});
    ASSERT_EQ(step(), NX_SUCCESS);
    ASSERT_NE(bus.last(0x592U), nullptr);
    EXPECT_EQ(bus.last(0x592U)->data[0], 0x60U);
    EXPECT_EQ(drive.core.requested_mode, 3);
    EXPECT_EQ(drive.core.actual_mode, 0);
    bus.push(0x612U, {0x2bU, 0x41U, 0x60U, 0U, 0U, 0U, 0U, 0U});
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_EQ(bus.last(0x592U)->data[0], 0x80U);
    bus.push(0x612U, {0x40U, 0x83U, 0x60U, 0U, 0U, 0U, 0U, 0U});
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_EQ(bus.last(0x592U)->data[0], 0x43U);
    EXPECT_EQ(bus.last(0x592U)->data[4], 200U);
    bus.push(0x612U, {0x40U, 0x02U, 0x65U, 0U, 0U, 0U, 0U, 0U});
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_EQ(bus.last(0x592U)->data[0], 0x43U);
    EXPECT_EQ(bus.last(0x592U)->data[4], 4U);
}

TEST_F(Drive, SevenBytePdoAdmitsSelectedVelocityOnlyAfterObservedEnabling) {
    enable();
    EXPECT_FLOAT_EQ(action.velocity_rad_s, 2.5F);
    EXPECT_FLOAT_EQ(action.acceleration_rad_s2, 2.0F);
    EXPECT_FLOAT_EQ(action.deceleration_rad_s2, 3.0F);
    EXPECT_EQ(drive.core.actual_mode, 3);
    feedback.velocity_rad_s = -1.25F;
    feedback.target_reached = true;
    ASSERT_EQ(step(), NX_SUCCESS);
    ASSERT_EQ(nx_canopen_request_tpdo(&node), NX_SUCCESS);
    ASSERT_EQ(step(), NX_SUCCESS);
    const auto* frame = bus.last(0x192U);
    ASSERT_NE(frame, nullptr);
    EXPECT_EQ(frame->dlc, 7U);
    EXPECT_EQ(frame->data[2], 3U);
    EXPECT_EQ(frame->data[3], 0x83U);
    EXPECT_EQ(frame->data[6], 0xffU);
    EXPECT_NE(drive.core.statusword & (1U << 10U), 0U);
}

TEST_F(Drive, ModeRequestIsNeverSubstitutedForActualMode) {
    operational();
    target(6U, 3, 100);
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_EQ(drive.core.actual_mode, 0);
    EXPECT_FALSE(action.enable_operation);
    target(15U, 1, 100);
    feedback.mode = NX_CIA402_PROFILE_VELOCITY;
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_EQ(drive.core.actual_mode, 3);
    EXPECT_FALSE(action.enable_operation);
    EXPECT_EQ(action.velocity_rad_s, 0.0F);
}

TEST_F(Drive, HaltAndQuickStopRemainExplicitRequestsWhileOutputIsLive) {
    enable();
    target(0x010fU, 3, 250);
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_TRUE(action.halt);
    EXPECT_EQ(action.velocity_rad_s, 0.0F);
    EXPECT_TRUE(action.enable_voltage);
    target(0x000bU, 3, 250);
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_TRUE(action.quick_stop);
    EXPECT_FLOAT_EQ(action.deceleration_rad_s2, 5.0F);
    feedback.quick_stopping = true;
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_EQ(drive.core.statusword & 0x6fU, 0x07U);
}

TEST_F(Drive, FaultResetRequestsRecoveryAndNeverAutomaticallyEnables) {
    enable();
    feedback.fault = true;
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_EQ(drive.core.statusword & 0x6fU, 0x0fU);
    EXPECT_FALSE(action.enable_voltage);
    feedback.voltage_enabled = false;
    feedback.operation_enabled = false;
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_EQ(drive.core.statusword & 0x6fU, 0x08U);
    target(0x0080U, 3, 250);
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_TRUE(action.reset_fault);
    EXPECT_EQ(drive.core.statusword & 0x6fU, 0x08U);
    feedback.fault = false;
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_EQ(drive.core.statusword & 0x6fU, 0x40U);
    EXPECT_FALSE(action.enable_operation);
    target(15U, 3, 250);
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_FALSE(action.enable_operation);
}

TEST_F(Drive, NetworkStopLatchesAdmissionAndDoesNotReplayTheOldEnable) {
    enable();
    bus.push(0U, {2U, 18U});
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_FALSE(action.enable_operation);
    EXPECT_TRUE(drive.core.fault_latched);
    bus.push(0U, {1U, 18U});
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_FALSE(action.enable_operation);
    EXPECT_EQ(drive.core.statusword & 0x6fU, 0x0fU);
}

TEST_F(Drive, OutOfRangeTargetAndInvalidFeedbackNeverDriveTheActuator) {
    enable();
    target(15U, 3, 1001);
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_TRUE(drive.core.fault_latched);
    EXPECT_FALSE(action.enable_operation);
    EXPECT_EQ(action.velocity_rad_s, 0.0F);
    feedback.valid = false;
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_FALSE(action.enable_voltage);
}

TEST_F(Drive, GenericCanopenStillRejectsStandardDriveObjects) {
    auto generic = transport;
    generic.objects = drive.objects;
    generic.object_count = NX_CIA402_OBJECT_COUNT;
    nx_canopen_t other{};
    std::vector<max_align_t> other_workspace(workspace.size());
    EXPECT_EQ(nx_canopen_init(&other, &generic, other_workspace.data(),
                              other_workspace.size() * sizeof(max_align_t)),
              NX_ERROR_INVALID);
    EXPECT_EQ(other.state, nullptr);
}

TEST_F(Drive, LosingActualReadinessLatchesFaultRatherThanReusingOldEnable) {
    enable();
    feedback.ready = false;
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_TRUE(drive.core.fault_latched);
    EXPECT_FALSE(action.enable_operation);
    feedback.ready = true;
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_FALSE(action.enable_operation);
}

TEST_F(Drive, UnrequestedPowerLossCannotAutomaticallyReenableTheOldTarget) {
    enable();
    feedback.voltage_enabled = false;
    feedback.operation_enabled = false;
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_TRUE(drive.core.fault_latched);
    EXPECT_FALSE(action.enable_voltage);
    EXPECT_FALSE(action.enable_operation);
}

TEST_F(Drive, PeerTimeoutAndResetAreSeparateFromOperationalNmt) {
    enable();
    report.peer = NX_CANOPEN_PEER_TIMEOUT;
    ASSERT_EQ(nx_cia402_update(&drive, &report, &feedback, &action),
              NX_SUCCESS);
    EXPECT_TRUE(drive.core.fault_latched);
    EXPECT_FALSE(action.enable_operation);
}

TEST_F(Drive, LiveProtocolPreventsDriveStorageRelease) {
    EXPECT_EQ(nx_cia402_release(&drive), NX_ERROR_BUSY);
    EXPECT_NE(node.state, nullptr);
    EXPECT_EQ(nx_canopen_stop(&node), NX_SUCCESS);
    EXPECT_EQ(nx_cia402_release(&drive), NX_SUCCESS);
    EXPECT_FALSE(drive.core.initialized);
}

TEST(Cia402, InvalidConfigurationRejectsWithoutChangingCallerStorage) {
    nx_cia402_t drive{};
    nx_cia402_config_t config{0.0F, 1000U, 200U, 300U, 500U};
    EXPECT_EQ(nx_cia402_initialize(&drive, &config), NX_ERROR_INVALID);
    EXPECT_FALSE(drive.core.initialized);
    config.velocity_unit_rad_s = 0.01F;
    config.maximum_velocity = 0U;
    EXPECT_EQ(nx_cia402_initialize(&drive, &config), NX_ERROR_INVALID);
}
} /* namespace */
