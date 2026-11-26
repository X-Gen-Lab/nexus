/**
 * \file            common_canopen_test.cpp
 * \brief           Real CANopenNode objects through public classic CAN ports
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/canopen.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <deque>
#include <gtest/gtest.h>
#include <vector>

namespace {
class CanBus {
  public:
    std::deque<nx_can_event_t> receive;
    std::vector<nx_can_frame_t> sent;
    nx_can_tx_request_t* pending{nullptr};
    nx_time_us_t now{1000U};
    bool complete{true};
    bool busy{false};
    bool fail_cancel{false};
    bool short_completion{false};
    bool reenter{false};
    nx_canopen_t* active{nullptr};
    nx_result_t nested{NX_SUCCESS};
    unsigned clock_calls{0U};
    static nx_time_us_t clock(void* context) {
        auto& self = *static_cast<CanBus*>(context);
        ++self.clock_calls;
        if (self.reenter && self.active != nullptr) {
            self.nested = nx_canopen_stop(self.active);
        }
        return self.now;
    }
    static nx_result_t submit(void* context, nx_can_tx_request_t* request) {
        auto& self = *static_cast<CanBus*>(context);
        if (self.busy || self.pending != nullptr) {
            return NX_ERROR_BUSY;
        }
        const auto result = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
        if (result == NX_SUCCESS) {
            self.pending = request;
            self.sent.push_back(request->frame);
        }
        return result;
    }
    static nx_result_t cancel(void* context, nx_can_tx_request_t* request) {
        auto& self = *static_cast<CanBus*>(context);
        if (self.pending != request) {
            return NX_ERROR_STATE;
        }
        if (self.fail_cancel) {
            (void)nx_request_transition(&request->base, NX_REQUEST_QUARANTINED);
            return NX_ERROR_IO;
        }
        self.pending = nullptr;
        nx_request_settle(&request->base, NX_ERROR_CANCELLED, 0U);
        return NX_SUCCESS;
    }
    static nx_result_t service(void* context) {
        auto& self = *static_cast<CanBus*>(context);
        if (self.pending != nullptr && self.complete) {
            auto* request = self.pending;
            self.pending = nullptr;
            nx_request_settle(&request->base, NX_SUCCESS,
                              self.short_completion ? 0U : request->frame.dlc);
        }
        return NX_SUCCESS;
    }
    static nx_result_t read(void* context, nx_can_event_t* events,
                            size_t capacity, size_t* count) {
        auto& self = *static_cast<CanBus*>(context);
        *count = 0U;
        while (*count < capacity && !self.receive.empty()) {
            events[*count] = self.receive.front();
            self.receive.pop_front();
            ++*count;
        }
        return NX_SUCCESS;
    }
    static nx_result_t state(void*, nx_can_state_t* output) {
        *output = {};
        output->started = true;
        return NX_SUCCESS;
    }
    void push(uint32_t id, std::initializer_list<uint8_t> data) {
        nx_can_event_t event{};
        event.kind = NX_CAN_EVENT_FRAME;
        event.frame = {id,
                       NX_CAN_STANDARD,
                       NX_CAN_DATA,
                       static_cast<uint8_t>(data.size()),
                       {}};
        std::copy(data.begin(), data.end(), event.frame.data);
        receive.push_back(event);
    }
    nx_can_ops_t ops{submit, cancel,  service, read,
                     state,  nullptr, nullptr, nullptr};
    nx_can_port_t port{&ops, this};
};
class Canopen : public testing::Test {
  protected:
    CanBus bus;
    uint32_t command{0U};
    uint32_t feedback{0x12345678U};
    nx_canopen_object_t objects[2]{
        {0x2000U, &command, 4U, true, true, false},
        {0x2001U, &feedback, 4U, false, false, true}};
    nx_canopen_config_t config{};
    nx_canopen_t node{};
    std::vector<max_align_t> workspace;
    std::array<nx_canopen_tx_slot_t, 4> tx{};
    nx_canopen_report_t report{};
    void SetUp() override {
        config.port = &bus.port;
        config.clock = {CanBus::clock, &bus};
        config.objects = objects;
        config.object_count = 2U;
        config.node_id = 18U;
        config.peer_node_id = 19U;
        config.producer_heartbeat_ms = 100U;
        config.consumer_heartbeat_ms = 100U;
        config.sdo_timeout_ms = 200U;
        config.maximum_step_us = 200000U;
        config.receive_budget = 4U;
        config.tx_slots = tx.data();
        config.tx_capacity = tx.size();
        const size_t bytes = nx_canopen_workspace_bytes(config.object_count);
        workspace.resize((bytes + sizeof(max_align_t) - 1U) /
                         sizeof(max_align_t));
        ASSERT_EQ(nx_canopen_init(&node, &config, workspace.data(),
                                  workspace.size() * sizeof(max_align_t)),
                  NX_SUCCESS);
    }
    nx_result_t step() {
        bus.now += 1000U;
        return nx_canopen_step(&node, bus.now + 100000U, &report);
    }
    void operational() {
        ASSERT_EQ(step(), NX_SUCCESS);
        bus.push(0U, {1U, 18U});
        ASSERT_EQ(step(), NX_SUCCESS);
        ASSERT_EQ(report.nmt, NX_CANOPEN_OPERATIONAL);
    }
    const nx_can_frame_t* last(uint32_t id) {
        for (auto item = bus.sent.rbegin(); item != bus.sent.rend(); ++item) {
            if (item->id == id) {
                return &*item;
            }
        }
        return nullptr;
    }
};

TEST_F(Canopen, RealStackProducesBootupAndHonorsNmtCommands) {
    ASSERT_EQ(step(), NX_SUCCESS);
    const auto* boot = last(0x712U);
    ASSERT_NE(boot, nullptr);
    EXPECT_EQ(boot->dlc, 1U);
    EXPECT_EQ(boot->data[0], 0U);
    EXPECT_EQ(report.nmt, NX_CANOPEN_PRE_OPERATIONAL);
    bus.push(0U, {1U, 18U});
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_EQ(report.nmt, NX_CANOPEN_OPERATIONAL);
    bus.push(0U, {2U, 18U});
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_EQ(report.nmt, NX_CANOPEN_STOPPED);
}

TEST_F(Canopen, RealSdoUploadAndDownloadUseApplicationStagingObjects) {
    ASSERT_EQ(step(), NX_SUCCESS);
    bus.push(0x612U, {0x40U, 1U, 0x20U, 0U, 0U, 0U, 0U, 0U});
    ASSERT_EQ(step(), NX_SUCCESS);
    const auto* response = last(0x592U);
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->data[0], 0x43U);
    EXPECT_EQ(response->data[4], 0x78U);
    EXPECT_EQ(response->data[7], 0x12U);
    bus.push(0x612U, {0x23U, 0U, 0x20U, 0U, 9U, 0U, 0U, 0U});
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_EQ(command, 9U);
    ASSERT_NE(last(0x592U), nullptr);
    EXPECT_EQ(last(0x592U)->data[0], 0x60U);
}

TEST_F(Canopen, ReadOnlyObjectRejectsRemoteMutationWithSdoAbort) {
    ASSERT_EQ(step(), NX_SUCCESS);
    bus.push(0x612U, {0x23U, 1U, 0x20U, 0U, 9U, 0U, 0U, 0U});
    ASSERT_EQ(step(), NX_SUCCESS);
    const auto* response = last(0x592U);
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->data[0], 0x80U);
    EXPECT_EQ(feedback, 0x12345678U);
}

TEST_F(Canopen, AsyncPdoUsesOnlySelectedMappingAndOperationalState) {
    bus.push(0x212U, {7U, 0U, 0U, 0U});
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_EQ(command, 0U);
    operational();
    bus.push(0x212U, {8U, 0U, 0U, 0U});
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_EQ(command, 8U);
    ASSERT_EQ(nx_canopen_request_tpdo(&node), NX_SUCCESS);
    ASSERT_EQ(step(), NX_SUCCESS);
    const auto* frame = last(0x192U);
    ASSERT_NE(frame, nullptr);
    EXPECT_EQ(frame->dlc, 4U);
    EXPECT_EQ(frame->data[0], 0x78U);
}

TEST_F(Canopen, PeerHeartbeatTimeoutIsExplicitAndDoesNotRestartTheNode) {
    operational();
    bus.push(0x713U, {127U});
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_EQ(report.peer, NX_CANOPEN_PEER_ACTIVE);
    bus.now += 101000U;
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_EQ(report.peer, NX_CANOPEN_PEER_TIMEOUT);
    EXPECT_NE(report.nmt, NX_CANOPEN_OPERATIONAL);
}

TEST_F(Canopen, FailedCancellationRetainsTheAcceptedRequestAndWorkspace) {
    bus.complete = false;
    ASSERT_EQ(step(), NX_SUCCESS);
    ASSERT_NE(bus.pending, nullptr);
    bus.fail_cancel = true;
    EXPECT_EQ(nx_canopen_stop(&node), NX_ERROR_IO);
    EXPECT_NE(node.state, nullptr);
    EXPECT_NE(bus.pending, nullptr);
    bus.fail_cancel = false;
    EXPECT_EQ(nx_canopen_stop(&node), NX_SUCCESS);
    EXPECT_EQ(bus.pending, nullptr);
    EXPECT_EQ(node.state, nullptr);
}

TEST_F(Canopen, RejectedTxKeepsOriginalQueueDeadlineAcrossLaterSteps) {
    bus.busy = true;
    bus.now = 2000U;
    ASSERT_EQ(nx_canopen_step(&node, 3000U, &report), NX_SUCCESS);
    bus.now = 4000U;
    EXPECT_EQ(nx_canopen_step(&node, 100000U, &report), NX_ERROR_TIMEOUT);
    EXPECT_TRUE(bus.sent.empty());
    EXPECT_EQ(nx_canopen_stop(&node), NX_SUCCESS);
}

TEST_F(Canopen, ReceiveLossClosesProtocolAdmissionWithoutReplayingCommands) {
    nx_can_event_t loss{};
    loss.kind = NX_CAN_EVENT_LOSS;
    loss.lost = 1U;
    bus.receive.push_back(loss);
    EXPECT_EQ(step(), NX_ERROR_IO);
    EXPECT_EQ(command, 0U);
    EXPECT_EQ(nx_canopen_request_tpdo(&node), NX_ERROR_STATE);
    EXPECT_EQ(nx_canopen_stop(&node), NX_SUCCESS);
}

TEST_F(Canopen, InvalidNodeAndMappingRejectBeforeClockOrTransportAccess) {
    auto invalid = config;
    invalid.node_id = 0U;
    const auto previous = bus.clock_calls;
    nx_canopen_t unused{};
    EXPECT_EQ(nx_canopen_init(&unused, &invalid, workspace.data(),
                              workspace.size() * sizeof(max_align_t)),
              NX_ERROR_INVALID);
    EXPECT_EQ(bus.clock_calls, previous);
    EXPECT_EQ(unused.state, nullptr);
    EXPECT_EQ(nx_canopen_workspace_bytes(0U), 0U);
    EXPECT_EQ(nx_canopen_workspace_bytes(SIZE_MAX), 0U);
}

TEST_F(Canopen, ProviderShortSuccessIsNotACompletedProtocolFrame) {
    ASSERT_EQ(step(), NX_SUCCESS);
    ASSERT_NE(bus.pending, nullptr);
    bus.short_completion = true;
    EXPECT_EQ(step(), NX_ERROR_IO);
    EXPECT_FALSE(report.borrowed);
    EXPECT_EQ(nx_canopen_stop(&node), NX_SUCCESS);
}

TEST_F(Canopen, FailedCancelStillAdvancesServiceToObserveFinalWithdrawal) {
    bus.complete = false;
    ASSERT_EQ(step(), NX_SUCCESS);
    ASSERT_NE(bus.pending, nullptr);
    bus.fail_cancel = true;
    bus.complete = true;
    EXPECT_EQ(nx_canopen_stop(&node), NX_ERROR_IO);
    EXPECT_EQ(bus.pending, nullptr);
    EXPECT_EQ(nx_canopen_stop(&node), NX_SUCCESS);
}

TEST_F(Canopen, CallbackReentryCannotTearDownTheLiveProtocolStep) {
    bus.active = &node;
    bus.reenter = true;
    EXPECT_EQ(step(), NX_SUCCESS);
    EXPECT_EQ(bus.nested, NX_ERROR_BUSY);
    EXPECT_NE(node.state, nullptr);
}

TEST_F(Canopen, TwoInstancesKeepObjectDictionaryAndTransportStateIndependent) {
    CanBus second_bus;
    auto second_config = config;
    uint32_t second_command = 0U;
    uint32_t second_feedback = 42U;
    nx_canopen_object_t second_objects[2]{
        {0x2000U, &second_command, 4U, true, true, false},
        {0x2001U, &second_feedback, 4U, false, false, true}};
    std::array<nx_canopen_tx_slot_t, 4> second_tx{};
    std::vector<max_align_t> second_workspace(workspace.size());
    second_config.port = &second_bus.port;
    second_config.clock = {CanBus::clock, &second_bus};
    second_config.objects = second_objects;
    second_config.tx_slots = second_tx.data();
    nx_canopen_t second{};
    ASSERT_EQ(nx_canopen_init(&second, &second_config, second_workspace.data(),
                              second_workspace.size() * sizeof(max_align_t)),
              NX_SUCCESS);
    operational();
    bus.push(0x212U, {9U, 0U, 0U, 0U});
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_EQ(command, 9U);
    EXPECT_EQ(second_command, 0U);
    nx_canopen_report_t second_report{};
    second_bus.now += 1000U;
    ASSERT_EQ(nx_canopen_step(&second, second_bus.now + 10000U, &second_report),
              NX_SUCCESS);
    EXPECT_EQ(second_report.nmt, NX_CANOPEN_PRE_OPERATIONAL);
    EXPECT_EQ(nx_canopen_stop(&second), NX_SUCCESS);
    EXPECT_NE(node.state, nullptr);
}

TEST_F(Canopen, MultipleRpdosInOneTurnReportLossBeforePublishingStagingData) {
    operational();
    bus.push(0x212U, {7U, 0U, 0U, 0U});
    bus.push(0x212U, {8U, 0U, 0U, 0U});
    EXPECT_EQ(step(), NX_ERROR_EXHAUSTED);
    EXPECT_EQ(command, 0U);
    EXPECT_TRUE(report.closing);
}

TEST_F(Canopen, WrongPdoLengthDoesNotPublishAValidPrefix) {
    operational();
    bus.push(0x212U, {7U, 0U, 0U, 0U, 9U});
    EXPECT_EQ(step(), NX_ERROR_IO);
    EXPECT_EQ(command, 0U);
}

TEST_F(Canopen, RemoteResetIsReportedAndClosesAdmissionWithoutResettingTheMcu) {
    ASSERT_EQ(step(), NX_SUCCESS);
    bus.push(0U, {130U, 18U});
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_EQ(report.reset, NX_CANOPEN_RESET_COMMUNICATION);
    EXPECT_TRUE(report.closing);
    EXPECT_EQ(nx_canopen_request_tpdo(&node), NX_ERROR_STATE);
}

TEST_F(Canopen,
       BoundedTransmitPressureReportsExhaustionAndKeepsBorrowedStorage) {
    ASSERT_EQ(nx_canopen_stop(&node), NX_SUCCESS);
    config.tx_capacity = 1U;
    ASSERT_EQ(nx_canopen_init(&node, &config, workspace.data(),
                              workspace.size() * sizeof(max_align_t)),
              NX_SUCCESS);
    bus.complete = false;
    operational();
    ASSERT_EQ(nx_canopen_request_tpdo(&node), NX_SUCCESS);
    EXPECT_EQ(step(), NX_ERROR_EXHAUSTED);
    EXPECT_TRUE(report.borrowed);
    EXPECT_TRUE(report.closing);
    EXPECT_EQ(nx_canopen_stop(&node), NX_SUCCESS);
}

TEST_F(Canopen, PipelinedSdoOverrunIsReportedBeforeEitherCommandIsApplied) {
    ASSERT_EQ(step(), NX_SUCCESS);
    bus.push(0x612U, {0x23U, 0U, 0x20U, 0U, 9U, 0U, 0U, 0U});
    bus.push(0x612U, {0x23U, 0U, 0x20U, 0U, 10U, 0U, 0U, 0U});
    EXPECT_EQ(step(), NX_ERROR_EXHAUSTED);
    EXPECT_EQ(command, 0U);
}
TEST_F(Canopen, ResetClosesAdmissionBeforeSameTurnSdoAndPdoWrites) {
    operational();
    bus.push(0U, {130U, 18U});
    bus.push(0x612U, {0x23U, 0U, 0x20U, 0U, 9U, 0U, 0U, 0U});
    bus.push(0x212U, {7U, 0U, 0U, 0U});
    ASSERT_EQ(step(), NX_SUCCESS);
    EXPECT_EQ(report.reset, NX_CANOPEN_RESET_COMMUNICATION);
    EXPECT_TRUE(report.closing);
    EXPECT_EQ(command, 0U);
    EXPECT_EQ(nx_canopen_stop(&node), NX_SUCCESS);
}

TEST_F(Canopen, AResetCannotBeOverwrittenByAnotherNmtCommandInOneTurn) {
    operational();
    bus.push(0U, {130U, 18U});
    bus.push(0U, {1U, 18U});
    const nx_result_t result = step();
    EXPECT_TRUE(report.closing);
    EXPECT_TRUE(report.reset == NX_CANOPEN_RESET_COMMUNICATION ||
                result == NX_ERROR_EXHAUSTED);
    EXPECT_EQ(nx_canopen_request_tpdo(&node), NX_ERROR_STATE);
    EXPECT_EQ(nx_canopen_stop(&node), NX_SUCCESS);
}

} /* namespace */
