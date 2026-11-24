/**
 * \file            cia402_position_test.cpp
 * \brief           Position setpoint handshake and actual settling over CANopen
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "cia402_test_support.h"
#include "nexus/components/cia402_position.h"
#include <array>
#include <gtest/gtest.h>

namespace {
using nexus_drive_test::DriveBus;
class PositionDrive : public testing::Test {
  protected:
    DriveBus bus;
    nx_cia402_position_t drive{};
    nx_canopen_t node{};
    nx_canopen_config_t transport{};
    nx_canopen_report_t report{};
    nx_cia402_position_feedback_t feedback{};
    nx_cia402_position_action_t action{};
    std::array<nx_canopen_tx_slot_t, 8> tx{};
    std::vector<max_align_t> workspace;
    void SetUp() override {
        nx_cia402_position_config_t config{};
        config.axis = {0.01F, 1000U, 200U, 300U, 500U};
        config.position_unit_rad = 0.001F;
        config.minimum_position = -10000;
        config.maximum_position = 10000;
        config.profile_velocity = 500U;
        config.position_window = 10U;
        config.position_window_ms = 5U;
        config.velocity_window_rad_s = 0.02F;
        config.epoch = 42U;
        ASSERT_EQ(nx_cia402_position_initialize(&drive, &config), NX_SUCCESS);
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
        const auto bytes =
            nx_canopen_workspace_bytes(NX_CIA402_POSITION_OBJECT_COUNT);
        workspace.resize((bytes + sizeof(max_align_t) - 1U) /
                         sizeof(max_align_t));
        ASSERT_EQ(nx_cia402_position_node_init(
                      &drive, &node, &transport, workspace.data(),
                      workspace.size() * sizeof(max_align_t)),
                  NX_SUCCESS);
        feedback.axis.valid = true;
        feedback.axis.ready = true;
        feedback.axis.reset_permitted = true;
        feedback.axis.mode = NX_CIA402_PROFILE_POSITION;
        feedback.position_valid = true;
        feedback.acknowledgement_epoch = config.epoch;
    }
    nx_result_t step() {
        bus.now += 1000U;
        feedback.observed_at = bus.now;
        ++feedback.sequence;
        const auto result = nx_canopen_step(&node, bus.now + 50000U, &report);
        if (result != NX_SUCCESS) {
            return result;
        }
        return nx_cia402_position_update(&drive, &report, &feedback, &action);
    }
    void target(uint16_t control, int32_t position) {
        const uint32_t raw = static_cast<uint32_t>(position);
        bus.push(0x212U,
                 {static_cast<uint8_t>(control),
                  static_cast<uint8_t>(control >> 8U), 1U,
                  static_cast<uint8_t>(raw), static_cast<uint8_t>(raw >> 8U),
                  static_cast<uint8_t>(raw >> 16U),
                  static_cast<uint8_t>(raw >> 24U)});
    }
    void enable() {
        ASSERT_EQ(step(), NX_SUCCESS);
        bus.push(0U, {1U, 18U});
        ASSERT_EQ(step(), NX_SUCCESS);
        target(6U, 0);
        ASSERT_EQ(step(), NX_SUCCESS);
        target(7U, 0);
        ASSERT_EQ(step(), NX_SUCCESS);
        feedback.axis.voltage_enabled = true;
        ASSERT_EQ(step(), NX_SUCCESS);
        target(15U, 0);
        ASSERT_EQ(step(), NX_SUCCESS);
        feedback.axis.operation_enabled = true;
        ASSERT_EQ(step(), NX_SUCCESS);
    }
    void TearDown() override {
        EXPECT_EQ(nx_canopen_stop(&node), NX_SUCCESS);
        EXPECT_EQ(nx_cia402_position_release(&drive), NX_SUCCESS);
    }
};

TEST_F(PositionDrive, StandardPositionOdHasRealWireTrafficAndFixedRampLimits) {
    enable();
    bus.push(0x612U, {0x23U, 0x7aU, 0x60U, 0U, 0xd0U, 7U, 0U, 0U});
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_EQ(drive.target_position, 2000);
    EXPECT_FALSE(action.new_setpoint);
    ASSERT_NE(bus.last(0x592U), nullptr);
    EXPECT_EQ(bus.last(0x592U)->data[0], 0x60U);
    bus.push(0x612U, {0x40U, 0x81U, 0x60U, 0U, 0U, 0U, 0U, 0U});
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_EQ(bus.last(0x592U)->data[0], 0x43U);
    EXPECT_EQ(bus.last(0x592U)->data[4], 0xf4U);
    EXPECT_EQ(bus.last(0x592U)->data[5], 1U);
    bus.push(0x612U, {0x40U, 0x02U, 0x65U, 0U, 0U, 0U, 0U, 0U});
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_EQ(bus.last(0x592U)->data[0], 0x43U);
    EXPECT_EQ(bus.last(0x592U)->data[4], 1U);
}

TEST_F(PositionDrive, SetpointAckRequiresTheAxesMatchingAdmissionGeneration) {
    enable();
    target(0x003fU, 2000);
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_TRUE(action.new_setpoint);
    EXPECT_FLOAT_EQ(action.position_rad, 2.0F);
    EXPECT_EQ(action.generation, 1U);
    EXPECT_EQ(drive.core.statusword & 0x1000U, 0U);
    feedback.accepted_generation = 99U;
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_TRUE(action.new_setpoint);
    EXPECT_EQ(action.generation, 1U);
    EXPECT_EQ(drive.core.statusword & 0x1000U, 0U);
    feedback.accepted_generation = 1U;
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_FALSE(action.new_setpoint);
    EXPECT_NE(drive.core.statusword & 0x1000U, 0U);
    target(0x002fU, 2000);
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_EQ(drive.core.statusword & 0x1000U, 0U);
}

TEST_F(PositionDrive, FeedbackPredatingTheFirstRequestCannotAcknowledgeIt) {
    enable();
    feedback.accepted_generation = 1U;
    target(0x003fU, 1000);
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_TRUE(action.new_setpoint);
    EXPECT_EQ(drive.core.statusword & 0x1000U, 0U);
}

TEST_F(PositionDrive, APreviousSessionAcknowledgmentCannotAdmitTheNewPoint) {
    enable();
    target(0x003fU, 1000);
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_EQ(action.epoch, 42U);
    feedback.accepted_generation = action.generation;
    feedback.acknowledgement_epoch = 41U;
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_TRUE(action.new_setpoint);
    EXPECT_EQ(drive.core.statusword & 0x1000U, 0U);
    feedback.acknowledgement_epoch = 42U;
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_FALSE(action.new_setpoint);
    EXPECT_NE(drive.core.statusword & 0x1000U, 0U);
}

TEST_F(PositionDrive, ActualPositionIsPublishedByTheRealSevenByteTpdo) {
    enable();
    feedback.position_rad = 1.25F;
    ASSERT_EQ(step(), NX_SUCCESS);
    ASSERT_EQ(nx_canopen_request_tpdo(&node), NX_SUCCESS);
    ASSERT_EQ(step(), NX_SUCCESS);
    const auto* frame = bus.last(0x192U);
    ASSERT_NE(frame, nullptr);
    EXPECT_EQ(frame->dlc, 7U);
    EXPECT_EQ(frame->data[2], 1U);
    EXPECT_EQ(frame->data[3], 0xe2U);
    EXPECT_EQ(frame->data[4], 4U);
}

TEST_F(PositionDrive, MovingReplanningIsRejectedAsAnUnselectedProfile) {
    enable();
    target(0x003fU, 1000);
    ASSERT_EQ(step(), NX_SUCCESS);
    feedback.accepted_generation = action.generation;
    ASSERT_EQ(step(), NX_SUCCESS);
    target(0x002fU, 1000);
    ASSERT_EQ(step(), NX_SUCCESS);
    target(0x003fU, 2000);
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_TRUE(drive.core.fault_latched);
    EXPECT_FALSE(action.new_setpoint);
}

TEST_F(PositionDrive, TrajectoryFlagDoesNotReplaceMeasuredPositionAndDwell) {
    enable();
    target(0x003fU, 2000);
    ASSERT_EQ(step(), NX_SUCCESS);
    feedback.accepted_generation = action.generation;
    feedback.axis.target_reached = true;
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_EQ(drive.core.statusword & 0x0400U, 0U);
    feedback.position_rad = 2.0F;
    feedback.axis.velocity_rad_s = 0.0F;
    ASSERT_EQ(step(), NX_SUCCESS);
    for (unsigned i = 0; i < 4U; ++i) {
        ASSERT_EQ(step(), NX_SUCCESS);
        EXPECT_EQ(drive.core.statusword & 0x0400U, 0U);
    }
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_NE(drive.core.statusword & 0x0400U, 0U);
    feedback.axis.velocity_rad_s = 0.03F;
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_EQ(drive.core.statusword & 0x0400U, 0U);
}

TEST_F(PositionDrive, OneHeldNewSetpointBitNeverGeneratesRepeatedMoves) {
    enable();
    target(0x003fU, 1000);
    ASSERT_EQ(step(), NX_SUCCESS);
    feedback.accepted_generation = action.generation;
    ASSERT_EQ(step(), NX_SUCCESS);
    target(0x003fU, 9000);
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_EQ(action.generation, 1U);
    EXPECT_FALSE(action.new_setpoint);
    EXPECT_FLOAT_EQ(action.position_rad, 1.0F);
}

TEST_F(PositionDrive, RelativeRequestIsRejectedWithoutChangingItsMeaning) {
    enable();
    target(0x007fU, 1000);
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_TRUE(drive.core.fault_latched);
    EXPECT_FALSE(action.new_setpoint);
    EXPECT_FALSE(action.axis.enable_operation);
}

TEST_F(PositionDrive, InvalidOrRepeatedPositionObservationCannotProveArrival) {
    enable();
    target(0x003fU, 0);
    ASSERT_EQ(step(), NX_SUCCESS);
    feedback.accepted_generation = action.generation;
    ASSERT_EQ(step(), NX_SUCCESS);
    ASSERT_EQ(nx_cia402_position_update(&drive, &report, &feedback, &action),
              NX_SUCCESS);
    EXPECT_TRUE(drive.core.fault_latched);
    EXPECT_EQ(drive.core.statusword & 0x0400U, 0U);
}

TEST_F(PositionDrive, AnUnacceptedSetpointHasABoundedAdmissionWindow) {
    enable();
    target(0x003fU, 1000);
    ASSERT_EQ(step(), NX_SUCCESS);
    bus.now += 99000U;
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_TRUE(drive.core.fault_latched);
    EXPECT_FALSE(action.new_setpoint);
    EXPECT_FALSE(action.axis.enable_operation);
}
} /* namespace */
