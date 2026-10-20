/**
 * \file            usb_control_test.cpp
 * \brief           Public-port ownership and control-transfer contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/usb_control.h"
#include "nexus/io/native/model.h"
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using testing::_;
using testing::NiceMock;
using testing::Return;

namespace {
class UsbPorts {
  public:
    MOCK_METHOD(nx_result_t, Submit, (bool, nx_usb_request_t*));
    MOCK_METHOD(nx_result_t, Cancel, (bool, nx_usb_request_t*));
    MOCK_METHOD(nx_result_t, Stall, (bool, bool));
    MOCK_METHOD(nx_result_t, Service, ());
    MOCK_METHOD(nx_result_t, State, (nx_usb_device_state_t*));
    MOCK_METHOD(nx_result_t, Address, (uint64_t, uint8_t));
    MOCK_METHOD(nx_result_t, Stop, ());
    MOCK_METHOD(nx_result_t, RejectSetup, (uint64_t));
};
struct Direction {
    UsbPorts* ports;
    bool in;
};
nx_result_t Submit(void* context, nx_usb_request_t* request) {
    auto* direction = static_cast<Direction*>(context);
    return direction->ports->Submit(direction->in, request);
}
nx_result_t Cancel(void* context, nx_usb_request_t* request) {
    auto* direction = static_cast<Direction*>(context);
    return direction->ports->Cancel(direction->in, request);
}
nx_result_t Stall(void* context, bool value) {
    auto* direction = static_cast<Direction*>(context);
    return direction->ports->Stall(direction->in, value);
}
nx_result_t Service(void* context) {
    return static_cast<UsbPorts*>(context)->Service();
}
nx_result_t State(void* context, nx_usb_device_state_t* state) {
    return static_cast<UsbPorts*>(context)->State(state);
}
nx_result_t Address(void* context, uint64_t epoch, uint8_t value) {
    return static_cast<UsbPorts*>(context)->Address(epoch, value);
}
nx_result_t Stop(void* context) {
    return static_cast<UsbPorts*>(context)->Stop();
}
nx_result_t RejectSetup(void* context, uint64_t epoch) {
    return static_cast<UsbPorts*>(context)->RejectSetup(epoch);
}
const nx_usb_setup_decision_ops_t setup_ops = {RejectSetup};
const nx_usb_endpoint_ops_t endpoint_ops = {Submit, Cancel, Stall};
const nx_usb_device_ops_t device_ops = {Service, nullptr, State,
                                        Address, nullptr, Stop};

class UsbControl : public testing::Test {
  protected:
    NiceMock<UsbPorts> ports;
    Direction input{&ports, true};
    Direction output{&ports, false};
    nx_usb_endpoint_port_t in{&endpoint_ops, &input};
    nx_usb_endpoint_port_t out{&endpoint_ops, &output};
    nx_usb_device_port_t device{&device_ops, &ports};
    nx_usb_endpoint_profile_t endpoints[2] = {
        {&in, 64U, 0U, true, NX_USB_PROFILE_CONTROL},
        {&out, 64U, 0U, false, NX_USB_PROFILE_CONTROL}};
    nx_usb_device_profile_t profile{&device,
                                    endpoints,
                                    2U,
                                    48000000U,
                                    320U,
                                    96U,
                                    4U,
                                    NX_USB_PROFILE_DEVICE,
                                    NX_USB_PROFILE_FULL_SPEED,
                                    NX_USB_PROFILE_INTERNAL_FS,
                                    NX_USB_PROFILE_PIO,
                                    {&setup_ops, &ports}};
    nx_usb_device_state_t observed{};
    nx_usb_control_t control{};
    nx_usb_control_request_t request{};
    nx_usb_request_t* inner = nullptr;
    uint8_t payload[8]{};

    void SetUp() override {
        ASSERT_EQ(nx_native_clock_configure(true, 100U), NX_SUCCESS);
        observed.started = true;
        observed.control_pending = true;
        observed.epoch = observed.setup.epoch = 7U;
        nx_request_initialize(&request.base);
        ON_CALL(ports, RejectSetup(_)).WillByDefault([this](uint64_t epoch) {
            return epoch == observed.epoch ? NX_SUCCESS : NX_ERROR_STATE;
        });
        ON_CALL(ports, Service()).WillByDefault([this] {
            return observed.started ? NX_SUCCESS : NX_ERROR_STATE;
        });
        ON_CALL(ports, State(_)).WillByDefault([this](auto* state) {
            *state = observed;
            return NX_SUCCESS;
        });
    }
    void TearDown() override {
        EXPECT_EQ(nx_native_clock_configure(false, 0U), NX_SUCCESS);
    }
    nx_result_t Admit(nx_usb_request_t* packet) {
        inner = packet;
        return nx_request_admit(&packet->base, NX_REQUEST_ACTIVE);
    }
};

TEST_F(UsbControl, StatusWireAckStillPublishesAuthoritativeClosingState) {
    /* A public packet ACK is not a promise that an address write committed. */
    observed.setup.request = 5U;
    observed.setup.value = 7U;
    /* The external standard-request policy explicitly stages this address. */
    EXPECT_CALL(ports, Address(7U, 7U)).WillOnce(Return(NX_SUCCESS));
    ASSERT_EQ(nx_usb_device_address(&device, 7U, 7U), NX_SUCCESS);
    ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&ports));
    EXPECT_CALL(ports, Address(_, _)).Times(0);
    EXPECT_CALL(ports, Stop()).Times(0);
    EXPECT_CALL(ports, Submit(true, _)).WillOnce([this](bool, auto* packet) {
        EXPECT_EQ(packet->stage, NX_USB_STATUS);
        EXPECT_EQ(packet->length, 0U);
        return Admit(packet);
    });
    ASSERT_EQ(nx_usb_control_initialize(&control, &profile), NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_status_prepare(&request, 7U, 100000U), NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_begin(&control, &request), NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_step(&control), NX_SUCCESS);
    ASSERT_NE(inner, nullptr);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    observed.control_pending = false;
    observed.closing = true;
    observed.address = 0U;
    ASSERT_NE(inner, nullptr);
    nx_request_settle(&inner->base, NX_SUCCESS, 0U);
    ASSERT_EQ(nx_usb_control_step(&control), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS); /* Wire-transfer outcome. */
    nx_usb_control_snapshot_t snapshot{};
    ASSERT_EQ(nx_usb_control_snapshot(&control, &snapshot), NX_SUCCESS);
    EXPECT_TRUE(snapshot.device.closing);
    EXPECT_EQ(snapshot.device.address, 0U); /* No invented address commit. */
}

TEST_F(UsbControl, Out0QuarantineRequiresExplicitOwnerRecovery) {
    observed.setup.length = sizeof(payload);
    EXPECT_CALL(ports, Stop()).Times(0);
    EXPECT_CALL(ports, Submit(false, _)).WillOnce([this](bool, auto* packet) {
        return Admit(packet);
    });
    EXPECT_CALL(ports, Cancel(false, _)).WillOnce([](bool, auto* packet) {
        EXPECT_EQ(nx_request_transition(&packet->base, NX_REQUEST_DRAINING),
                  NX_SUCCESS);
        EXPECT_EQ(nx_request_transition(&packet->base, NX_REQUEST_QUARANTINED),
                  NX_SUCCESS);
        return NX_SUCCESS;
    });
    ASSERT_EQ(nx_usb_control_initialize(&control, &profile), NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_out_prepare(&request, payload, sizeof(payload), 7U,
                                         100000U),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_begin(&control, &request), NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_step(&control), NX_SUCCESS);
    ASSERT_NE(inner, nullptr);
    ASSERT_EQ(nx_usb_control_cancel(&control, &request, 7U), NX_SUCCESS);
    (void)nx_usb_control_step(&control);
    (void)nx_usb_control_step(&control);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    nx_usb_control_snapshot_t snapshot{};
    ASSERT_EQ(nx_usb_control_snapshot(&control, &snapshot), NX_SUCCESS);
    EXPECT_EQ(snapshot.phase, NX_USB_CONTROL_NEEDS_DEVICE_STOP);
    EXPECT_TRUE(snapshot.outer_active);
    ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&ports));
    /* The explicit external owner, not the adapter, authorizes global stop. */
    EXPECT_CALL(ports, Stop()).WillOnce([this] {
        observed.started = false;
        observed.closing = true;
        nx_request_settle(&inner->base, NX_ERROR_CANCELLED, 0U);
        return NX_SUCCESS;
    });
    ASSERT_EQ(nx_usb_device_stop(&device), NX_SUCCESS);
    (void)nx_usb_control_step(
        &control); /* Fold SETTLED despite service STATE. */
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
}

TEST_F(UsbControl, OutDataRequiresExternalApprovalBeforeStatusAcknowledgement) {
    observed.setup.length = sizeof(payload);
    EXPECT_CALL(ports, Stop()).Times(0);
    EXPECT_CALL(ports, Submit(true, _)).Times(0);
    EXPECT_CALL(ports, Submit(false, _)).WillOnce([this](bool, auto* packet) {
        return Admit(packet);
    });
    ASSERT_EQ(nx_usb_control_initialize(&control, &profile), NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_out_prepare(&request, payload, sizeof(payload), 7U,
                                         100000U),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_begin(&control, &request), NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_step(&control), NX_SUCCESS);
    ASSERT_NE(inner, nullptr);
    ASSERT_NE(inner, nullptr);
    nx_request_settle(&inner->base, NX_SUCCESS, sizeof(payload));
    ASSERT_EQ(nx_usb_control_step(&control), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    nx_usb_control_out_view_t view{};
    ASSERT_EQ(nx_usb_control_out_view(&control, &view), NX_SUCCESS);
    EXPECT_EQ(view.bytes, payload);
    EXPECT_EQ(view.length, sizeof(payload));
    EXPECT_EQ(view.setup_epoch, 7U);
    ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&ports));
    EXPECT_CALL(ports, Stop()).Times(0);
    EXPECT_CALL(ports, Submit(true, _)).WillOnce([this](bool, auto* packet) {
        EXPECT_EQ(packet->stage, NX_USB_STATUS);
        EXPECT_EQ(packet->length, 0U);
        return Admit(packet);
    });
    ASSERT_EQ(nx_usb_control_approve(&control, 7U, true), NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_step(&control), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    observed.control_pending = false;
    ASSERT_NE(inner, nullptr);
    nx_request_settle(&inner->base, NX_SUCCESS, 0U);
    ASSERT_EQ(nx_usb_control_step(&control), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
}

TEST_F(UsbControl, ShortFullPacketReplyNeedsAnAcknowledgedDataZlp) {
    uint8_t descriptor[64]{};
    observed.setup.request_type = 128U;
    observed.setup.length = 128U;
    EXPECT_CALL(ports, Submit(true, _))
        .WillOnce([this](bool, auto* p) {
            EXPECT_EQ(p->stage, NX_USB_DATA);
            EXPECT_EQ(p->length, 64U);
            return Admit(p);
        })
        .WillOnce([this](bool, auto* p) {
            EXPECT_EQ(p->stage, NX_USB_DATA);
            EXPECT_EQ(p->length, 0U);
            return Admit(p);
        });
    EXPECT_CALL(ports, Submit(false, _)).WillOnce([this](bool, auto* p) {
        EXPECT_EQ(p->stage, NX_USB_STATUS);
        EXPECT_EQ(p->length, 0U);
        return Admit(p);
    });
    ASSERT_EQ(nx_usb_control_initialize(&control, &profile), NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_in_prepare(&request, descriptor,
                                        sizeof(descriptor), 7U,
                                        NX_DEADLINE_NEVER),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_begin(&control, &request), NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_step(&control), NX_SUCCESS);
    ASSERT_NE(inner, nullptr);
    nx_request_settle(&inner->base, NX_SUCCESS, 64U);
    ASSERT_EQ(nx_usb_control_step(&control), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    /* FIFO/admission of the ZLP is insufficient: it needs its own wire ACK. */
    ASSERT_EQ(nx_usb_control_step(&control), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    ASSERT_NE(inner, nullptr);
    nx_request_settle(&inner->base, NX_SUCCESS, 0U);
    ASSERT_EQ(nx_usb_control_step(&control), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    ASSERT_NE(inner, nullptr);
    nx_request_settle(&inner->base, NX_SUCCESS, 0U);
    observed.control_pending = false;
    ASSERT_EQ(nx_usb_control_step(&control), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, 64U);
}

TEST_F(UsbControl, ExactLengthReplySkipsDataZlpAndDoesNotPipelinePackets) {
    uint8_t descriptor[64]{};
    observed.setup.request_type = 128U;
    observed.setup.length = 64U;
    EXPECT_CALL(ports, Submit(true, _)).WillOnce([this](bool, auto* p) {
        EXPECT_EQ(p->stage, NX_USB_DATA);
        EXPECT_EQ(p->length, 64U);
        return Admit(p);
    });
    EXPECT_CALL(ports, Submit(false, _)).WillOnce([this](bool, auto* p) {
        EXPECT_EQ(p->stage, NX_USB_STATUS);
        return Admit(p);
    });
    ASSERT_EQ(nx_usb_control_initialize(&control, &profile), NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_in_prepare(&request, descriptor, 64U, 7U,
                                        NX_DEADLINE_NEVER),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_begin(&control, &request), NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_step(&control), NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_step(&control), NX_SUCCESS);
    ASSERT_NE(inner, nullptr);
    nx_request_settle(&inner->base, NX_SUCCESS, 64U);
    ASSERT_EQ(nx_usb_control_step(&control), NX_SUCCESS);
    EXPECT_EQ(inner->stage, NX_USB_STATUS);
    ASSERT_NE(inner, nullptr);
    nx_request_settle(&inner->base, NX_SUCCESS, 0U);
    ASSERT_EQ(nx_usb_control_step(&control), NX_SUCCESS);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
}

TEST_F(UsbControl, SupersedingSetupDrainsOldPacketBeforeNewAdmission) {
    observed.setup.request_type = 128U;
    observed.setup.length = 8U;
    EXPECT_CALL(ports, Submit(true, _)).WillOnce([this](bool, auto* p) {
        return Admit(p);
    });
    EXPECT_CALL(ports, Cancel(true, _)).WillOnce([](bool, auto* p) {
        return nx_request_transition(&p->base, NX_REQUEST_DRAINING);
    });
    EXPECT_CALL(ports, Stop()).Times(0);
    ASSERT_EQ(nx_usb_control_initialize(&control, &profile), NX_SUCCESS);
    ASSERT_EQ(
        nx_usb_control_in_prepare(&request, payload, 8U, 7U, NX_DEADLINE_NEVER),
        NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_begin(&control, &request), NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_step(&control), NX_SUCCESS);
    observed.epoch = observed.setup.epoch = 8U;
    observed.lost_events =
        999U; /* Authoritative state survives a full event ring. */
    ASSERT_EQ(nx_usb_control_step(&control), NX_SUCCESS);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    nx_usb_control_request_t next{};
    nx_request_initialize(&next.base);
    ASSERT_EQ(
        nx_usb_control_in_prepare(&next, payload, 8U, 8U, NX_DEADLINE_NEVER),
        NX_SUCCESS);
    EXPECT_EQ(nx_usb_control_begin(&control, &next), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&next.base), NX_REQUEST_READY);
    ASSERT_NE(inner, nullptr);
    nx_request_settle(&inner->base, NX_ERROR_CANCELLED, 0U);
    ASSERT_EQ(nx_usb_control_step(&control), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    /* No old packet is re-prepared until acquire-observed drain. */
    EXPECT_EQ(nx_usb_control_begin(&control, &next), NX_SUCCESS);
    EXPECT_EQ(nx_usb_control_cancel(&control, &next, 7U), NX_ERROR_STATE);
    ASSERT_EQ(nx_usb_control_cancel(&control, &next, 8U), NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_step(&control), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&next.base), NX_REQUEST_SETTLED);
}

TEST_F(UsbControl, StatusAckWinsBeforeLaterSetupAndCancellation) {
    EXPECT_CALL(ports, Submit(true, _)).WillOnce([this](bool, auto* p) {
        return Admit(p);
    });
    EXPECT_CALL(ports, Cancel(_, _)).Times(0);
    ASSERT_EQ(nx_usb_control_initialize(&control, &profile), NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_status_prepare(&request, 7U, NX_DEADLINE_NEVER),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_begin(&control, &request), NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_step(&control), NX_SUCCESS);
    ASSERT_NE(inner, nullptr);
    nx_request_settle(&inner->base, NX_SUCCESS, 0U);
    observed.epoch = observed.setup.epoch = 8U;
    ASSERT_EQ(nx_usb_control_cancel(&control, &request, 7U), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
}

TEST_F(UsbControl, ExternalPolicyRejectionStallsWithoutAcknowledgingStatus) {
    observed.setup.length = 8U;
    EXPECT_CALL(ports, Submit(false, _)).WillOnce([this](bool, auto* p) {
        return Admit(p);
    });
    EXPECT_CALL(ports, Submit(true, _)).Times(0);
    EXPECT_CALL(ports, Stall(_, _)).Times(0);
    EXPECT_CALL(ports, RejectSetup(7U)).WillOnce(Return(NX_SUCCESS));
    ASSERT_EQ(nx_usb_control_initialize(&control, &profile), NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_out_prepare(&request, payload, 8U, 7U,
                                         NX_DEADLINE_NEVER),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_begin(&control, &request), NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_step(&control), NX_SUCCESS);
    ASSERT_NE(inner, nullptr);
    nx_request_settle(&inner->base, NX_SUCCESS, 8U);
    ASSERT_EQ(nx_usb_control_step(&control), NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_approve(&control, 7U, false), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    ASSERT_EQ(nx_usb_control_step(&control), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_PERMISSION);
}

TEST_F(UsbControl, StaleOutApprovalCannotAcknowledgeOrStallNewSetup) {
    for (bool after_snapshot : {false, true}) {
        SCOPED_TRACE(after_snapshot);
        nx_usb_control_t adapter{};
        nx_usb_control_request_t reply{};
        observed.epoch = observed.setup.epoch = 7U;
        observed.setup.length = 8U;
        nx_request_initialize(&reply.base);
        EXPECT_CALL(ports, Submit(false, _)).WillOnce([this](bool, auto* p) {
            return Admit(p);
        });
        EXPECT_CALL(ports, Submit(true, _)).Times(0);
        EXPECT_CALL(ports, Stall(_, _)).Times(0);
        EXPECT_CALL(ports, RejectSetup(7U)).Times(after_snapshot ? 1 : 0);
        ASSERT_EQ(nx_usb_control_initialize(&adapter, &profile), NX_SUCCESS);
        ASSERT_EQ(nx_usb_control_out_prepare(&reply, payload, 8U, 7U,
                                             NX_DEADLINE_NEVER),
                  NX_SUCCESS);
        ASSERT_EQ(nx_usb_control_begin(&adapter, &reply), NX_SUCCESS);
        ASSERT_EQ(nx_usb_control_step(&adapter), NX_SUCCESS);
        ASSERT_NE(inner, nullptr);
        nx_request_settle(&inner->base, NX_SUCCESS, 8U);
        ASSERT_EQ(nx_usb_control_step(&adapter), NX_SUCCESS);
        if (after_snapshot) {
            ASSERT_EQ(nx_usb_control_approve(&adapter, 7U, false), NX_SUCCESS);
            EXPECT_CALL(ports, State(_))
                .WillOnce([this](auto* value) {
                    *value = observed;
                    /* Model the normal IRQ publication after the copied
                     * snapshot.
                     */
                    observed.epoch = observed.setup.epoch = 8U;
                    return NX_SUCCESS;
                })
                .WillRepeatedly([this](auto* value) {
                    *value = observed;
                    return NX_SUCCESS;
                });
            EXPECT_EQ(nx_usb_control_step(&adapter), NX_ERROR_STATE);
            EXPECT_NE(nx_request_state(&reply.base), NX_REQUEST_SETTLED);
        } else {
            observed.epoch = observed.setup.epoch = 8U;
            EXPECT_EQ(nx_usb_control_approve(&adapter, 7U, false),
                      NX_ERROR_STATE);
        }
        ASSERT_EQ(nx_usb_control_step(&adapter), NX_SUCCESS);
        EXPECT_EQ(nx_request_state(&reply.base), NX_REQUEST_SETTLED);
        EXPECT_EQ(reply.base.result, NX_ERROR_CANCELLED);
        ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&ports));
    }
}

TEST_F(UsbControl, DeadlineRetainsPacketUntilItsIndependentDrainCompletes) {
    observed.setup.request_type = 128U;
    observed.setup.length = 8U;
    EXPECT_CALL(ports, Submit(true, _)).WillOnce([this](bool, auto* p) {
        return Admit(p);
    });
    EXPECT_CALL(ports, Cancel(true, _)).WillOnce([](bool, auto* p) {
        return nx_request_transition(&p->base, NX_REQUEST_DRAINING);
    });
    EXPECT_CALL(ports, Stop()).Times(0);
    ASSERT_EQ(nx_usb_control_initialize(&control, &profile), NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_in_prepare(&request, payload, 8U, 7U, 110U),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_begin(&control, &request), NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_step(&control), NX_SUCCESS);
    ASSERT_EQ(nx_native_clock_advance(10U), NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_step(&control), NX_SUCCESS);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(
        nx_usb_control_in_prepare(&request, payload, 8U, 7U, NX_DEADLINE_NEVER),
        NX_ERROR_STATE);
    ASSERT_NE(inner, nullptr);
    nx_request_settle(&inner->base, NX_ERROR_TIMEOUT, 0U);
    ASSERT_EQ(nx_usb_control_step(&control), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_TIMEOUT);
}

TEST_F(UsbControl, AliasedPayloadRejectsBeforeAnyProviderCallOrBorrow) {
    EXPECT_CALL(ports, State(_)).Times(0);
    EXPECT_CALL(ports, Submit(_, _)).Times(0);
    ASSERT_EQ(nx_usb_control_initialize(&control, &profile), NX_SUCCESS);
    auto* bytes = reinterpret_cast<const uint8_t*>(&control);
    ASSERT_EQ(
        nx_usb_control_in_prepare(&request, bytes, 8U, 7U, NX_DEADLINE_NEVER),
        NX_SUCCESS);
    EXPECT_EQ(nx_usb_control_begin(&control, &request), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(control.active, nullptr);
}

TEST_F(UsbControl, SeparateControllerOwnersCannotCancelEachOthersLoan) {
    observed.setup.request_type = 128U;
    observed.setup.length = 8U;
    NiceMock<UsbPorts> other_ports;
    Direction other_input{&other_ports, true};
    Direction other_output{&other_ports, false};
    nx_usb_endpoint_port_t other_in{&endpoint_ops, &other_input};
    nx_usb_endpoint_port_t other_out{&endpoint_ops, &other_output};
    nx_usb_device_port_t other_device{&device_ops, &other_ports};
    nx_usb_endpoint_profile_t other_endpoints[2] = {
        {&other_in, 64U, 0U, true, NX_USB_PROFILE_CONTROL},
        {&other_out, 64U, 0U, false, NX_USB_PROFILE_CONTROL}};
    nx_usb_device_profile_t other_profile = profile;
    other_profile.port = &other_device;
    other_profile.endpoints = other_endpoints;
    other_profile.setup_decision = {&setup_ops, &other_ports};
    nx_usb_device_state_t other_state = observed;
    other_state.epoch = other_state.setup.epoch = 9U;
    nx_usb_control_t other{};
    nx_usb_control_request_t other_request{};
    uint8_t other_payload[8]{};
    nx_usb_request_t* other_packet = nullptr;
    ON_CALL(other_ports, State(_)).WillByDefault([&](auto* value) {
        *value = other_state;
        return NX_SUCCESS;
    });
    EXPECT_CALL(ports, Submit(true, _)).WillOnce([this](bool, auto* p) {
        return Admit(p);
    });
    EXPECT_CALL(other_ports, Submit(true, _)).WillOnce([&](bool, auto* p) {
        other_packet = p;
        return nx_request_admit(&p->base, NX_REQUEST_ACTIVE);
    });
    EXPECT_CALL(ports, Cancel(_, _)).Times(0);
    EXPECT_CALL(other_ports, Cancel(true, _)).WillOnce([](bool, auto* p) {
        nx_request_settle(&p->base, NX_ERROR_CANCELLED, 0U);
        return NX_SUCCESS;
    });
    EXPECT_CALL(ports, Stop()).Times(0);
    EXPECT_CALL(other_ports, Stop()).Times(0);
    nx_request_initialize(&other_request.base);
    ASSERT_EQ(nx_usb_control_initialize(&control, &profile), NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_initialize(&other, &other_profile), NX_SUCCESS);
    ASSERT_EQ(
        nx_usb_control_in_prepare(&request, payload, 8U, 7U, NX_DEADLINE_NEVER),
        NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_in_prepare(&other_request, other_payload, 8U, 9U,
                                        NX_DEADLINE_NEVER),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_begin(&control, &request), NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_begin(&other, &other_request), NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_step(&control), NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_step(&other), NX_SUCCESS);
    ASSERT_NE(inner, nullptr);
    ASSERT_NE(other_packet, nullptr);
    EXPECT_EQ(nx_usb_control_cancel(&other, &request, 7U), NX_ERROR_STATE);
    ASSERT_EQ(nx_usb_control_cancel(&other, &other_request, 9U), NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_step(&other), NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_step(&other), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&other_request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(other_request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    nx_request_settle(&inner->base, NX_ERROR_IO, 0U);
    ASSERT_EQ(nx_usb_control_step(&control), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
}

TEST_F(UsbControl, UnalignedOutputsAreRejectedWithoutOverwritingTheirStorage) {
    observed.setup.length = sizeof(payload);
    EXPECT_CALL(ports, Submit(false, _)).WillOnce([this](bool, auto* p) {
        return Admit(p);
    });
    EXPECT_CALL(ports, Submit(true, _)).Times(0);
    EXPECT_CALL(ports, Stop()).Times(0);
    ASSERT_EQ(nx_usb_control_initialize(&control, &profile), NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_out_prepare(&request, payload, sizeof(payload), 7U,
                                         NX_DEADLINE_NEVER),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_begin(&control, &request), NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_step(&control), NX_SUCCESS);
    ASSERT_NE(inner, nullptr);
    nx_request_settle(&inner->base, NX_SUCCESS, sizeof(payload));
    ASSERT_EQ(nx_usb_control_step(&control), NX_SUCCESS);
    EXPECT_CALL(ports, State(_)).Times(0);
    alignas(nx_usb_control_out_view_t)
        uint8_t out_raw[sizeof(nx_usb_control_out_view_t) +
                        alignof(nx_usb_control_out_view_t)];
    uint8_t out_before[sizeof(out_raw)];
    std::memset(out_raw, 0xA5, sizeof(out_raw));
    std::memcpy(out_before, out_raw, sizeof(out_raw));
    auto* unaligned_out =
        reinterpret_cast<nx_usb_control_out_view_t*>(out_raw + 1U);
    EXPECT_EQ(nx_usb_control_out_view(&control, unaligned_out),
              NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(out_raw, out_before, sizeof(out_raw)), 0);
    alignas(nx_usb_control_snapshot_t)
        uint8_t state_raw[sizeof(nx_usb_control_snapshot_t) +
                          alignof(nx_usb_control_snapshot_t)];
    uint8_t state_before[sizeof(state_raw)];
    std::memset(state_raw, 0x5A, sizeof(state_raw));
    std::memcpy(state_before, state_raw, sizeof(state_raw));
    auto* unaligned_state =
        reinterpret_cast<nx_usb_control_snapshot_t*>(state_raw + 1U);
    EXPECT_EQ(nx_usb_control_snapshot(&control, unaligned_state),
              NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(state_raw, state_before, sizeof(state_raw)), 0);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    nx_usb_control_out_view_t valid{};
    ASSERT_EQ(nx_usb_control_out_view(&control, &valid), NX_SUCCESS);
    EXPECT_EQ(valid.bytes, payload);
    EXPECT_EQ(valid.length, sizeof(payload));
    ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&ports));
    ASSERT_EQ(nx_usb_control_cancel(&control, &request, 7U), NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_step(&control), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
}

TEST_F(UsbControl, UnalignedAdapterRejectsBeforeAnyProviderReadOrOutputWrite) {
    ASSERT_EQ(nx_usb_control_initialize(&control, &profile), NX_SUCCESS);
    ASSERT_EQ(nx_usb_control_step(&control), NX_SUCCESS);
    alignas(nx_usb_control_t)
        uint8_t raw[sizeof(nx_usb_control_t) + alignof(nx_usb_control_t)]{};
    std::memcpy(raw + 1U, &control, sizeof(control));
    uint8_t before[sizeof(raw)];
    std::memcpy(before, raw, sizeof(raw));
    auto* unaligned = reinterpret_cast<nx_usb_control_t*>(raw + 1U);
    EXPECT_CALL(ports, Service()).Times(0);
    EXPECT_CALL(ports, State(_)).Times(0);
    EXPECT_CALL(ports, Submit(_, _)).Times(0);
    EXPECT_EQ(nx_usb_control_step(unaligned), NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(raw, before, sizeof(raw)), 0);
    nx_usb_control_snapshot_t value{};
    uint8_t original[sizeof(value)];
    std::memcpy(original, &value, sizeof(value));
    EXPECT_EQ(nx_usb_control_snapshot(unaligned, &value), NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(original, &value, sizeof(value)), 0);
    EXPECT_EQ(std::memcmp(raw, before, sizeof(raw)), 0);
}
} /* namespace */
