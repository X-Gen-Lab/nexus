/**
 * \file            i3c_controller_test.cpp
 * \brief           Real bounded I3C core at aggregate FIFO and drain boundaries
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/i3c_controller_storage.h"
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
namespace {
using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
class Port {
  public:
    MOCK_METHOD(nx_result_t, Claim, (const void*));
    MOCK_METHOD(void, Release, (const void*));
    MOCK_METHOD(nx_result_t, Ready, ());
    MOCK_METHOD(void, Begin, (const nx_i3c_command_t*));
    MOCK_METHOD(void, Pump,
                (const uint8_t*, size_t, uint8_t*, size_t,
                 nx_i3c_observation_t*));
    MOCK_METHOD(void, Assign, (uint8_t));
    MOCK_METHOD(void, Withdraw, ());
    MOCK_METHOD(nx_result_t, Quiescent, ());
    MOCK_METHOD(nx_result_t, Reset, ());
    const void* owner = nullptr;
    void defaults() {
        ON_CALL(*this, Claim).WillByDefault(Invoke([this](const void* value) {
            if (owner != nullptr) {
                return NX_ERROR_BUSY;
            }
            owner = value;
            return NX_SUCCESS;
        }));
        ON_CALL(*this, Release).WillByDefault(Invoke([this](const void*) {
            owner = nullptr;
        }));
        ON_CALL(*this, Ready).WillByDefault(Return(NX_SUCCESS));
        ON_CALL(*this, Quiescent).WillByDefault(Return(NX_ERROR_BUSY));
        ON_CALL(*this, Reset).WillByDefault(Return(NX_ERROR_BUSY));
    }
};
nx_result_t claim(void* p, const void* o) {
    return static_cast<Port*>(p)->Claim(o);
}
void release(void* p, const void* o) {
    static_cast<Port*>(p)->Release(o);
}
nx_result_t ready(void* p) {
    return static_cast<Port*>(p)->Ready();
}
void begin(void* p, const nx_i3c_command_t* c) {
    static_cast<Port*>(p)->Begin(c);
}
void pump(void* p, const uint8_t* tx, size_t t, uint8_t* rx, size_t r,
          nx_i3c_observation_t* o) {
    static_cast<Port*>(p)->Pump(tx, t, rx, r, o);
}
void assign(void* p, uint8_t address) {
    static_cast<Port*>(p)->Assign(address);
}
void withdraw(void* p) {
    static_cast<Port*>(p)->Withdraw();
}
nx_result_t quiescent(void* p) {
    return static_cast<Port*>(p)->Quiescent();
}
nx_result_t reset(void* p) {
    return static_cast<Port*>(p)->Reset();
}
nx_time_us_t clock_read(void* p) {
    return *static_cast<nx_time_us_t*>(p);
}
const nx_i3c_port_ops_t ops = {claim,  release,  ready,     begin, pump,
                               assign, withdraw, quiescent, reset};
class I3cController : public ::testing::Test {
  protected:
    NiceMock<Port> port;
    nx_time_us_t now = 10;
    nx_i3c_target_definition_t definitions[2] = {{0x112233445566, 0x12},
                                                 {0x223344556677, 0x14}};
    nx_i3c_target_state_t targets[2] = {};
    nx_i3c_event_t events[2] = {};
    nx_i3c_controller_t controller = {};
    nx_i3c_controller_config_t config = {};
    nx_i3c_request_t request = {};
    alignas(64) uint8_t bytes[64] = {};
    void SetUp() override {
        port.defaults();
        config = {&ops,        &port,   {clock_read, &now},
                  definitions, targets, 2,
                  events,      2,       8};
        ASSERT_EQ(nx_i3c_controller_construct(&controller, &config),
                  NX_SUCCESS);
        nx_request_initialize(&request.base);
    }
    nx_i3c_endpoint_t known(size_t index = 0) {
        targets[index].epoch = 1;
        targets[index].address = definitions[index].address;
        targets[index].assigned = true;
        targets[index].bcr = 6;
        return {&controller, index, 1};
    }
    nx_i3c_ticket_t submit(bool read = false, size_t length = 12) {
        EXPECT_EQ(nx_i3c_request_prepare(&request, NX_I3C_PRIVATE, known(), 0,
                                         bytes, length, read, false, 100),
                  NX_SUCCESS);
        nx_i3c_ticket_t ticket = {};
        EXPECT_EQ(nx_i3c_submit(&controller, &request, &ticket), NX_SUCCESS);
        return ticket;
    }
    nx_i3c_ticket_t discover() {
        EXPECT_EQ(nx_i3c_request_prepare(&request, NX_I3C_DISCOVER, {}, 0,
                                         nullptr, 0, false, false, 100),
                  NX_SUCCESS);
        nx_i3c_ticket_t ticket = {};
        EXPECT_EQ(nx_i3c_submit(&controller, &request, &ticket), NX_SUCCESS);
        return ticket;
    }
    void observe(nx_i3c_observation_t value) {
        EXPECT_CALL(port, Pump(_, _, _, _, _))
            .WillOnce(
                Invoke([value](const uint8_t*, size_t, uint8_t*, size_t,
                               nx_i3c_observation_t* out) { *out = value; }));
    }
    void drain_success() {
        EXPECT_CALL(port, Quiescent()).WillOnce(Return(NX_SUCCESS));
        EXPECT_EQ(nx_i3c_service(&controller), NX_SUCCESS);
    }
};
TEST_F(I3cController, FiniteTxIsBatchedAndFrameCompleteStillNeedsWireProof) {
    auto ticket = submit();
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    EXPECT_CALL(port, Pump(bytes, 8, nullptr, 0, _))
        .WillOnce(
            Invoke([](const uint8_t*, size_t, uint8_t*, size_t,
                      nx_i3c_observation_t* out) { out->transmitted = 8; }));
    ASSERT_EQ(nx_i3c_service(&controller), NX_SUCCESS);
    EXPECT_CALL(port, Pump(bytes + 8, 4, nullptr, 0, _))
        .WillOnce(Invoke([](const uint8_t*, size_t, uint8_t*, size_t,
                            nx_i3c_observation_t* out) {
            out->transmitted = 4;
            out->complete = true;
        }));
    ASSERT_EQ(nx_i3c_service(&controller), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    EXPECT_EQ(nx_i3c_service(&controller), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_NE(controller.active, nullptr);
    drain_success();
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, 12U);
    EXPECT_EQ(nx_i3c_cancel(ticket), NX_ERROR_STATE);
}
TEST_F(I3cController, FiniteRxWritesOnlySuppliedAggregateSpan) {
    submit(true, 5);
    EXPECT_CALL(port, Pump(nullptr, 0, bytes, 5, _))
        .WillOnce(Invoke([](const uint8_t*, size_t, uint8_t* rx, size_t count,
                            nx_i3c_observation_t* out) {
            std::memset(rx, 0x5a, count);
            out->received = count;
            out->complete = true;
        }));
    ASSERT_EQ(nx_i3c_service(&controller), NX_SUCCESS);
    drain_success();
    EXPECT_EQ(bytes[4], 0x5a);
    EXPECT_EQ(bytes[5], 0);
    EXPECT_EQ(request.base.transferred, 5U);
}
TEST_F(I3cController, CancelAndTimeoutRetainBorrowUntilActualDrain) {
    auto ticket = submit();
    EXPECT_CALL(port, Withdraw()).Times(1);
    EXPECT_EQ(nx_i3c_cancel(ticket), NX_SUCCESS);
    EXPECT_EQ(nx_i3c_cancel(ticket), NX_SUCCESS);
    now = 1000;
    EXPECT_EQ(nx_i3c_service(&controller), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(controller.active, &request);
    drain_success();
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(controller.active, nullptr);
}
TEST_F(I3cController, DeadlineRequestsWithdrawalWithoutPublishingSettlement) {
    submit();
    now = 100;
    EXPECT_CALL(port, Withdraw()).Times(1);
    EXPECT_EQ(nx_i3c_service(&controller), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    EXPECT_NE(controller.active, nullptr);
    drain_success();
    EXPECT_EQ(request.base.result, NX_ERROR_TIMEOUT);
}
TEST_F(I3cController, ReadinessRejectsWithoutBorrowCommandOrTicketMutation) {
    ASSERT_EQ(nx_i3c_request_prepare(&request, NX_I3C_PRIVATE, known(), 0,
                                     bytes, 1, false, false, 100),
              NX_SUCCESS);
    EXPECT_CALL(port, Ready()).WillOnce(Return(NX_ERROR_BUSY));
    EXPECT_CALL(port, Begin(_)).Times(0);
    nx_i3c_ticket_t ticket = {};
    EXPECT_EQ(nx_i3c_submit(&controller, &request, &ticket), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(ticket.controller, nullptr);
    EXPECT_EQ(controller.active, nullptr);
}
TEST_F(I3cController, DiscoveryConfirmsAddressBeforeExposingEndpoint) {
    discover();
    nx_i3c_observation_t identity = {};
    identity.identity = true;
    identity.pid = definitions[0].pid;
    identity.bcr = 3;
    identity.dcr = 4;
    observe(identity);
    EXPECT_CALL(port, Assign(0x12)).Times(1);
    ASSERT_EQ(nx_i3c_service(&controller), NX_SUCCESS);
    EXPECT_TRUE(targets[0].tentative);
    EXPECT_FALSE(targets[0].assigned);
    nx_i3c_endpoint_t endpoint = {};
    EXPECT_EQ(nx_i3c_endpoint(&controller, 0, &endpoint), NX_ERROR_STATE);
    nx_i3c_observation_t ack = {};
    ack.assigned = true;
    ack.complete = true;
    observe(ack);
    ASSERT_EQ(nx_i3c_service(&controller), NX_SUCCESS);
    drain_success();
    EXPECT_EQ(nx_i3c_endpoint(&controller, 0, &endpoint), NX_SUCCESS);
    EXPECT_EQ(endpoint.epoch, 1U);
    EXPECT_EQ(targets[0].address, 0x12);
    EXPECT_EQ(targets[0].bcr, 3);
    EXPECT_EQ(targets[0].dcr, 4);
}
TEST_F(I3cController, DaaAckPrecedesNextIdentityInSameObservation) {
    discover();
    nx_i3c_observation_t o = {};
    o.identity = true;
    o.pid = definitions[0].pid;
    observe(o);
    EXPECT_CALL(port, Assign(0x12));
    ASSERT_EQ(nx_i3c_service(&controller), NX_SUCCESS);
    o.assigned = true;
    o.pid = definitions[1].pid;
    observe(o);
    EXPECT_CALL(port, Assign(0x14));
    ASSERT_EQ(nx_i3c_service(&controller), NX_SUCCESS);
    EXPECT_TRUE(targets[0].assigned);
    EXPECT_TRUE(targets[1].tentative);
}
TEST_F(I3cController, PartialDaaCancelAndFailedResetKeepAllWireAddresses) {
    known(0);
    auto ticket = discover();
    nx_i3c_observation_t o = {};
    o.identity = true;
    o.pid = definitions[1].pid;
    observe(o);
    EXPECT_CALL(port, Assign(0x14));
    ASSERT_EQ(nx_i3c_service(&controller), NX_SUCCESS);
    ASSERT_EQ(nx_i3c_cancel(ticket), NX_SUCCESS);
    EXPECT_TRUE(controller.faulted);
    EXPECT_TRUE(targets[0].assigned);
    EXPECT_TRUE(targets[1].tentative);
    ASSERT_EQ(nx_i3c_close(&controller), NX_SUCCESS);
    EXPECT_EQ(nx_i3c_reset(&controller), NX_ERROR_BUSY);
    EXPECT_TRUE(targets[0].assigned);
    EXPECT_EQ(targets[1].address, 0x14);
    EXPECT_EQ(controller.active, &request);
    EXPECT_CALL(port, Reset()).WillOnce(Return(NX_SUCCESS));
    ASSERT_EQ(nx_i3c_reset(&controller), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_FALSE(targets[0].assigned);
    EXPECT_FALSE(targets[1].tentative);
    EXPECT_EQ(targets[1].epoch, 1U);
    EXPECT_EQ(nx_i3c_reopen(&controller), NX_SUCCESS);
}
TEST_F(I3cController, UnknownDaaTargetFaultsWithoutCreatingAnObject) {
    discover();
    nx_i3c_observation_t o = {};
    o.identity = true;
    o.pid = 0xabcdef123456;
    observe(o);
    EXPECT_CALL(port, Assign(_)).Times(0);
    EXPECT_CALL(port, Withdraw()).Times(1);
    EXPECT_EQ(nx_i3c_service(&controller), NX_ERROR_NACK);
    EXPECT_TRUE(controller.faulted);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    EXPECT_FALSE(targets[0].assigned);
}
TEST_F(I3cController, UnacknowledgedDaaFrameCannotCommitTentativeAddress) {
    discover();
    nx_i3c_observation_t o = {};
    o.identity = true;
    o.pid = definitions[0].pid;
    observe(o);
    ASSERT_EQ(nx_i3c_service(&controller), NX_SUCCESS);
    o = {};
    o.complete = true;
    observe(o);
    EXPECT_EQ(nx_i3c_service(&controller), NX_ERROR_IO);
    EXPECT_TRUE(targets[0].tentative);
    EXPECT_FALSE(targets[0].assigned);
    EXPECT_TRUE(controller.faulted);
}
TEST_F(I3cController, TargetEpochExhaustionNeverReusesIdentity) {
    targets[0].epoch = UINT64_MAX;
    discover();
    nx_i3c_observation_t o = {};
    o.identity = true;
    o.pid = definitions[0].pid;
    observe(o);
    EXPECT_CALL(port, Assign(_)).Times(0);
    EXPECT_EQ(nx_i3c_service(&controller), NX_ERROR_EXHAUSTED);
    EXPECT_EQ(targets[0].epoch, UINT64_MAX);
    EXPECT_FALSE(targets[0].tentative);
}
TEST_F(I3cController, OperationEpochExhaustionRejectsBeforeBegin) {
    controller.slot.epoch = UINT64_MAX;
    ASSERT_EQ(nx_i3c_request_prepare(&request, NX_I3C_PRIVATE, known(), 0,
                                     bytes, 1, false, false, 100),
              NX_SUCCESS);
    EXPECT_CALL(port, Begin(_)).Times(0);
    nx_i3c_ticket_t ticket = {};
    EXPECT_EQ(nx_i3c_submit(&controller, &request, &ticket),
              NX_ERROR_EXHAUSTED);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
}
TEST_F(I3cController, ResetInvalidatesOldEndpointAndStaleCancelControl) {
    auto old_endpoint = known();
    auto old_ticket = submit();
    ASSERT_EQ(nx_i3c_close(&controller), NX_SUCCESS);
    EXPECT_CALL(port, Reset()).WillOnce(Return(NX_SUCCESS));
    ASSERT_EQ(nx_i3c_reset(&controller), NX_SUCCESS);
    ASSERT_EQ(nx_i3c_reopen(&controller), NX_SUCCESS);
    ASSERT_EQ(nx_i3c_request_prepare(&request, NX_I3C_PRIVATE, old_endpoint, 0,
                                     bytes, 1, false, false, 100),
              NX_SUCCESS);
    nx_i3c_ticket_t fresh = {};
    EXPECT_EQ(nx_i3c_submit(&controller, &request, &fresh), NX_ERROR_STATE);
    targets[0].epoch = 2;
    targets[0].assigned = true;
    targets[0].address = 0x12;
    request.endpoint.epoch = 2;
    ASSERT_EQ(nx_i3c_submit(&controller, &request, &fresh), NX_SUCCESS);
    EXPECT_GT(fresh.epoch, old_ticket.epoch);
    EXPECT_EQ(nx_i3c_cancel(old_ticket), NX_ERROR_STATE);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
}
TEST_F(I3cController, OverflowRetainsQueuedEventsAndNeverStartsDiscovery) {
    auto endpoint = known();
    nx_i3c_observation_t o = {};
    o.ibi = true;
    o.ibi_address = 0x12;
    o.ibi_bytes = 1;
    o.ibi_payload[0] = 0x5a;
    o.hot_join = true;
    observe(o);
    EXPECT_CALL(port, Begin(_)).Times(0);
    ASSERT_EQ(nx_i3c_service(&controller), NX_SUCCESS);
    o.ibi = false;
    observe(o);
    EXPECT_EQ(nx_i3c_service(&controller), NX_ERROR_OVERFLOW);
    nx_i3c_event_status_t status = {};
    ASSERT_EQ(nx_i3c_event_status(&controller, &status), NX_SUCCESS);
    EXPECT_EQ(status.overflow, 1U);
    nx_i3c_event_t event = {};
    ASSERT_EQ(nx_i3c_event_take(&controller, &event), NX_SUCCESS);
    EXPECT_EQ(event.kind, NX_I3C_EVENT_IBI);
    EXPECT_EQ(event.endpoint.epoch, endpoint.epoch);
    EXPECT_EQ(event.payload[0], 0x5a);
    ASSERT_EQ(nx_i3c_event_take(&controller, &event), NX_SUCCESS);
    EXPECT_EQ(event.kind, NX_I3C_EVENT_HOT_JOIN);
    EXPECT_EQ(event.endpoint.controller, nullptr);
    EXPECT_EQ(nx_i3c_event_take(&controller, &event), NX_ERROR_EMPTY);
}
TEST_F(I3cController, UnknownOrOversizedIbiIsRejectedWithoutBufferAccess) {
    nx_i3c_observation_t o = {};
    o.ibi = true;
    o.ibi_address = 0x12;
    o.ibi_bytes = 1;
    observe(o);
    EXPECT_EQ(nx_i3c_service(&controller), NX_SUCCESS);
    known();
    o.ibi_bytes = 5;
    observe(o);
    EXPECT_EQ(nx_i3c_service(&controller), NX_SUCCESS);
    EXPECT_EQ(controller.event_count, 0U);
    nx_i3c_event_status_t status = {};
    ASSERT_EQ(nx_i3c_event_status(&controller, &status), NX_SUCCESS);
    EXPECT_EQ(status.rejected_ibi, 2U);
}
TEST_F(I3cController, EventCountersSaturateWithoutWrapping) {
    controller.event_status.overflow = UINT32_MAX;
    controller.event_status.rejected_ibi = UINT32_MAX;
    controller.event_count = 2;
    nx_i3c_observation_t o = {};
    o.ibi = true;
    o.ibi_address = 0x12;
    o.hot_join = true;
    observe(o);
    EXPECT_EQ(nx_i3c_service(&controller), NX_ERROR_OVERFLOW);
    EXPECT_EQ(controller.event_status.overflow, UINT32_MAX);
    EXPECT_EQ(controller.event_status.rejected_ibi, UINT32_MAX);
}
TEST_F(I3cController, MalformedFifoProgressAndShortFrameQuarantine) {
    submit(false, 2);
    nx_i3c_observation_t o = {};
    o.transmitted = 3;
    observe(o);
    EXPECT_EQ(nx_i3c_service(&controller), NX_ERROR_IO);
    EXPECT_TRUE(controller.faulted);
    EXPECT_EQ(controller.progress, 0U);
    EXPECT_EQ(nx_i3c_service(&controller), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
}
TEST_F(I3cController, ShortReadCompletionNeverClaimsFullSuccess) {
    submit(true, 5);
    nx_i3c_observation_t o = {};
    o.received = 2;
    o.complete = true;
    observe(o);
    EXPECT_EQ(nx_i3c_service(&controller), NX_ERROR_IO);
    drain_success();
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 2U);
}
TEST_F(I3cController, CccWhitelistRejectsAddressMutationAndWrongLengths) {
    auto endpoint = known();
    EXPECT_EQ(nx_i3c_request_prepare(&request, NX_I3C_CCC, endpoint, 0x06,
                                     nullptr, 0, false, true, 100),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_i3c_request_prepare(&request, NX_I3C_CCC, endpoint, 0x8d,
                                     bytes, 5, true, false, 100),
              NX_ERROR_INVALID);
    ASSERT_EQ(nx_i3c_request_prepare(&request, NX_I3C_CCC, endpoint, 0x8d,
                                     bytes, 6, true, false, 100),
              NX_SUCCESS);
    EXPECT_CALL(port, Begin(_)).WillOnce(Invoke([](const nx_i3c_command_t* c) {
        EXPECT_EQ(c->kind, NX_I3C_CCC);
        EXPECT_EQ(c->address, 0x12);
        EXPECT_EQ(c->ccc, 0x8d);
    }));
    nx_i3c_ticket_t ticket = {};
    EXPECT_EQ(nx_i3c_submit(&controller, &request, &ticket), NX_SUCCESS);
}
TEST_F(I3cController, StopRetainsUniqueOwnerUntilRequestAndEventsDrain) {
    submit();
    EXPECT_CALL(port, Withdraw()).Times(1);
    EXPECT_EQ(nx_i3c_stop(&controller), NX_ERROR_BUSY);
    EXPECT_EQ(port.owner, &controller);
    EXPECT_EQ(nx_i3c_stop(&controller), NX_ERROR_BUSY);
    drain_success();
    controller.event_count = 1;
    EXPECT_EQ(nx_i3c_stop(&controller), NX_ERROR_BUSY);
    nx_i3c_event_t event = {};
    ASSERT_EQ(nx_i3c_event_take(&controller, &event), NX_SUCCESS);
    EXPECT_EQ(nx_i3c_stop(&controller), NX_ERROR_STATE);
    EXPECT_CALL(port, Reset()).WillOnce(Return(NX_SUCCESS));
    ASSERT_EQ(nx_i3c_reset(&controller), NX_SUCCESS);
    EXPECT_CALL(port, Quiescent()).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(port, Release(&controller)).Times(1);
    EXPECT_EQ(nx_i3c_stop(&controller), NX_SUCCESS);
    EXPECT_EQ(port.owner, nullptr);
    EXPECT_EQ(nx_i3c_service(&controller), NX_ERROR_STATE);
    EXPECT_EQ(nx_i3c_controller_construct(&controller, &config),
              NX_ERROR_STATE);
}
TEST_F(I3cController, ResetRequiresClosedAdmissionAndActualProof) {
    EXPECT_CALL(port, Reset()).Times(0);
    EXPECT_EQ(nx_i3c_reset(&controller), NX_ERROR_STATE);
    ASSERT_EQ(nx_i3c_close(&controller), NX_SUCCESS);
    EXPECT_EQ(nx_i3c_reopen(&controller), NX_SUCCESS);
}
TEST_F(I3cController, OutputsCannotOverwriteActiveBorrowOrControllerStorage) {
    submit();
    EXPECT_EQ(nx_i3c_event_take(&controller,
                                reinterpret_cast<nx_i3c_event_t*>(bytes)),
              NX_ERROR_INVALID);
    EXPECT_EQ(
        nx_i3c_event_status(&controller,
                            reinterpret_cast<nx_i3c_event_status_t*>(&request)),
        NX_ERROR_INVALID);
    EXPECT_EQ(nx_i3c_endpoint(&controller, 0,
                              reinterpret_cast<nx_i3c_endpoint_t*>(targets)),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
}
TEST_F(I3cController, PayloadCannotAliasDefinitionsOrEventStorage) {
    auto endpoint = known();
    ASSERT_EQ(nx_i3c_request_prepare(&request, NX_I3C_PRIVATE, endpoint, 0,
                                     reinterpret_cast<uint8_t*>(events), 1,
                                     false, false, 100),
              NX_SUCCESS);
    EXPECT_CALL(port, Begin(_)).Times(0);
    nx_i3c_ticket_t ticket = {};
    EXPECT_EQ(nx_i3c_submit(&controller, &request, &ticket), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
}
TEST_F(I3cController, ConstructorRejectsDuplicateReservedAndOverlappingPolicy) {
    nx_i3c_controller_t other = {};
    nx_i3c_target_state_t other_targets[2] = {};
    nx_i3c_target_definition_t other_definitions[2] = {};
    std::memcpy(other_definitions, definitions, sizeof(definitions));
    auto c = config;
    c.targets = other_targets;
    c.definitions = other_definitions;
    EXPECT_CALL(port, Claim(_)).Times(0);
    other_definitions[1].pid = other_definitions[0].pid;
    EXPECT_EQ(nx_i3c_controller_construct(&other, &c), NX_ERROR_INVALID);
    other_definitions[1].pid = definitions[1].pid;
    other_definitions[1].address = 0x3e;
    EXPECT_EQ(nx_i3c_controller_construct(&other, &c), NX_ERROR_INVALID);
    other_definitions[1].address = definitions[1].address;
    c.events = reinterpret_cast<nx_i3c_event_t*>(other_targets);
    EXPECT_EQ(nx_i3c_controller_construct(&other, &c), NX_ERROR_INVALID);
}
TEST_F(I3cController, TypedMisalignmentAndIntegerOverflowRejectBeforeAccess) {
    alignas(64) uint8_t packed[sizeof(nx_i3c_controller_t) + 1] = {};
    EXPECT_EQ(nx_i3c_controller_construct(
                  reinterpret_cast<nx_i3c_controller_t*>(packed + 1), &config),
              NX_ERROR_INVALID);
    EXPECT_EQ(
        nx_i3c_request_prepare(&request, NX_I3C_PRIVATE, known(), 0,
                               reinterpret_cast<uint8_t*>(UINTPTR_MAX - 2), 4,
                               false, false, 100),
        NX_ERROR_INVALID);
    nx_i3c_controller_t other = {};
    auto c = config;
    c.target_count = SIZE_MAX;
    EXPECT_EQ(nx_i3c_controller_construct(&other, &c), NX_ERROR_INVALID);
}
TEST_F(I3cController, IndependentInstanceCannotShareRequestsTargetsOrTickets) {
    NiceMock<Port> other_port;
    other_port.defaults();
    nx_i3c_controller_t other = {};
    nx_i3c_target_state_t other_targets[2] = {};
    nx_i3c_event_t other_events[2] = {};
    auto c = config;
    c.context = &other_port;
    c.targets = other_targets;
    c.events = other_events;
    ASSERT_EQ(nx_i3c_controller_construct(&other, &c), NX_SUCCESS);
    auto ticket = submit();
    ticket.controller = &other;
    EXPECT_EQ(nx_i3c_cancel(ticket), NX_ERROR_STATE);
    EXPECT_EQ(other.active, nullptr);
    EXPECT_EQ(controller.active, &request);
    EXPECT_FALSE(other_targets[0].assigned);
}
TEST_F(I3cController, ExpiredAdmissionHasNoBorrowOrHardwareEffect) {
    ASSERT_EQ(nx_i3c_request_prepare(&request, NX_I3C_PRIVATE, known(), 0,
                                     bytes, 1, false, false, now),
              NX_SUCCESS);
    EXPECT_CALL(port, Ready()).Times(0);
    EXPECT_CALL(port, Begin(_)).Times(0);
    nx_i3c_ticket_t ticket = {};
    EXPECT_EQ(nx_i3c_submit(&controller, &request, &ticket), NX_ERROR_TIMEOUT);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(controller.slot.request, nullptr);
}
TEST_F(I3cController, SecondRequestAndForeignEndpointRejectBeforeBegin) {
    submit();
    nx_i3c_request_t second = {};
    nx_request_initialize(&second.base);
    ASSERT_EQ(nx_i3c_request_prepare(&second, NX_I3C_PRIVATE, known(), 0, bytes,
                                     1, false, false, 100),
              NX_SUCCESS);
    nx_i3c_ticket_t ticket = {};
    EXPECT_CALL(port, Begin(_)).Times(0);
    EXPECT_EQ(nx_i3c_submit(&controller, &second, &ticket), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&second.base), NX_REQUEST_READY);
    EXPECT_EQ(controller.active, &request);
}
TEST_F(I3cController, AddressErrorCannotEraseEarlierDaaAssignments) {
    discover();
    nx_i3c_observation_t o = {};
    o.identity = true;
    o.pid = definitions[0].pid;
    observe(o);
    EXPECT_CALL(port, Assign(0x12));
    ASSERT_EQ(nx_i3c_service(&controller), NX_SUCCESS);
    o = {};
    o.assigned = true;
    o.error = NX_ERROR_ARBITRATION;
    observe(o);
    EXPECT_CALL(port, Withdraw()).Times(1);
    EXPECT_EQ(nx_i3c_service(&controller), NX_ERROR_ARBITRATION);
    EXPECT_TRUE(targets[0].assigned);
    EXPECT_FALSE(targets[0].tentative);
    drain_success();
    EXPECT_EQ(request.base.result, NX_ERROR_ARBITRATION);
    ASSERT_EQ(nx_i3c_close(&controller), NX_SUCCESS);
    EXPECT_CALL(port, Reset()).WillOnce(Return(NX_ERROR_UNSUPPORTED));
    EXPECT_EQ(nx_i3c_reset(&controller), NX_ERROR_UNSUPPORTED);
    EXPECT_TRUE(targets[0].assigned);
    EXPECT_EQ(nx_i3c_reopen(&controller), NX_ERROR_STATE);
}
TEST_F(I3cController, PeripheralDrainErrorRetainsRequestAndUniqueOwner) {
    auto ticket = submit();
    ASSERT_EQ(nx_i3c_cancel(ticket), NX_SUCCESS);
    EXPECT_CALL(port, Quiescent()).WillOnce(Return(NX_ERROR_IO));
    EXPECT_EQ(nx_i3c_service(&controller), NX_ERROR_IO);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(controller.slot.request, &request.base);
    EXPECT_EQ(port.owner, &controller);
    EXPECT_CALL(port, Release(_)).Times(0);
    EXPECT_EQ(nx_i3c_stop(&controller), NX_ERROR_BUSY);
}
TEST_F(I3cController, TicketOutputCannotOverwritePayloadOrMetadata) {
    ASSERT_EQ(nx_i3c_request_prepare(&request, NX_I3C_PRIVATE, known(), 0,
                                     bytes, sizeof(nx_i3c_ticket_t), false,
                                     false, 100),
              NX_SUCCESS);
    EXPECT_CALL(port, Ready()).Times(0);
    EXPECT_CALL(port, Begin(_)).Times(0);
    EXPECT_EQ(nx_i3c_submit(&controller, &request,
                            reinterpret_cast<nx_i3c_ticket_t*>(bytes)),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
}
TEST_F(I3cController, ClaimFailureLeavesAllCallerStorageReusable) {
    nx_i3c_controller_t other = {};
    nx_i3c_target_state_t other_targets[2] = {};
    nx_i3c_event_t other_events[2] = {};
    auto c = config;
    c.targets = other_targets;
    c.events = other_events;
    EXPECT_EQ(nx_i3c_controller_construct(&other, &c), NX_ERROR_BUSY);
    EXPECT_FALSE(other.initialized);
    EXPECT_FALSE(other_targets[0].assigned);
}
TEST_F(I3cController, DaaErrorConfirmsPreviousButNeverAssignsNextIdentity) {
    discover();
    nx_i3c_observation_t o = {};
    o.identity = true;
    o.pid = definitions[0].pid;
    observe(o);
    EXPECT_CALL(port, Assign(0x12));
    ASSERT_EQ(nx_i3c_service(&controller), NX_SUCCESS);
    ::testing::Mock::VerifyAndClearExpectations(&port);
    o.assigned = true;
    o.pid = definitions[1].pid;
    o.error = NX_ERROR_NACK;
    observe(o);
    EXPECT_CALL(port, Assign(_)).Times(0);
    EXPECT_CALL(port, Withdraw()).Times(1);
    EXPECT_EQ(nx_i3c_service(&controller), NX_ERROR_NACK);
    EXPECT_TRUE(targets[0].assigned);
    EXPECT_FALSE(targets[1].tentative);
    EXPECT_EQ(targets[1].epoch, 0U);
}
TEST_F(I3cController, IbiCoalescedWithDaaAckUsesConfirmedEpoch) {
    discover();
    nx_i3c_observation_t o = {};
    o.identity = true;
    o.pid = definitions[0].pid;
    o.bcr = 6;
    observe(o);
    ASSERT_EQ(nx_i3c_service(&controller), NX_SUCCESS);
    o = {};
    o.assigned = true;
    o.complete = true;
    o.ibi = true;
    o.ibi_address = 0x12;
    o.ibi_bytes = 1;
    o.ibi_payload[0] = 0x7b;
    observe(o);
    ASSERT_EQ(nx_i3c_service(&controller), NX_SUCCESS);
    nx_i3c_event_t event = {};
    ASSERT_EQ(nx_i3c_event_take(&controller, &event), NX_SUCCESS);
    EXPECT_EQ(event.endpoint.epoch, 1U);
    EXPECT_EQ(event.payload[0], 0x7b);
}
TEST_F(I3cController, MissingInitialBusBaselineRejectsWithoutMutation) {
    nx_i3c_controller_t other = {};
    nx_i3c_target_state_t other_targets[2] = {};
    nx_i3c_event_t other_events[2] = {};
    auto c = config;
    c.targets = other_targets;
    c.events = other_events;
    EXPECT_CALL(port, Claim(&other)).WillOnce(Return(NX_ERROR_STATE));
    EXPECT_CALL(port, Release(_)).Times(0);
    EXPECT_CALL(port, Reset()).Times(0);
    EXPECT_EQ(nx_i3c_controller_construct(&other, &c), NX_ERROR_STATE);
    EXPECT_FALSE(other.initialized);
    EXPECT_EQ(other_targets[0].epoch, 0U);
}
TEST_F(I3cController, EmptyEventReadPreservesOutputAndRingWrapKeepsOrder) {
    nx_i3c_event_t event = {};
    event.bytes = 3;
    EXPECT_EQ(nx_i3c_event_take(&controller, &event), NX_ERROR_EMPTY);
    EXPECT_EQ(event.bytes, 3);
    known();
    nx_i3c_observation_t o = {};
    o.hot_join = true;
    observe(o);
    ASSERT_EQ(nx_i3c_service(&controller), NX_SUCCESS);
    ASSERT_EQ(nx_i3c_event_take(&controller, &event), NX_SUCCESS);
    o.ibi = true;
    o.ibi_address = 0x12;
    o.ibi_bytes = 1;
    o.ibi_payload[0] = 7;
    observe(o);
    ASSERT_EQ(nx_i3c_service(&controller), NX_SUCCESS);
    ASSERT_EQ(nx_i3c_event_take(&controller, &event), NX_SUCCESS);
    EXPECT_EQ(event.kind, NX_I3C_EVENT_IBI);
    EXPECT_EQ(event.payload[0], 7);
    ASSERT_EQ(nx_i3c_event_take(&controller, &event), NX_SUCCESS);
    EXPECT_EQ(event.kind, NX_I3C_EVENT_HOT_JOIN);
}
TEST_F(I3cController, NullAndMisalignedMetadataRejectBeforePortAccess) {
    EXPECT_EQ(nx_i3c_service(nullptr), NX_ERROR_INVALID);
    EXPECT_EQ(nx_i3c_request_prepare(nullptr, NX_I3C_DISCOVER, {}, 0, nullptr,
                                     0, false, false, 100),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_i3c_controller_construct(nullptr, &config), NX_ERROR_INVALID);
    alignas(64) uint8_t packed[sizeof(nx_i3c_request_t) + 1] = {};
    EXPECT_EQ(nx_i3c_request_prepare(
                  reinterpret_cast<nx_i3c_request_t*>(packed + 1),
                  NX_I3C_DISCOVER, {}, 0, nullptr, 0, false, false, 100),
              NX_ERROR_INVALID);
}
TEST_F(I3cController, IbiRequiresAdvertisedCapabilityAndExactMdbShape) {
    known();
    nx_i3c_observation_t o = {};
    o.ibi = true;
    o.ibi_address = 0x12;
    const uint8_t bcr[] = {0, 2, 4, 6};
    const uint8_t lengths[] = {1, 1, 1, 0};
    for (size_t i = 0; i < 4; ++i) {
        targets[0].bcr = bcr[i];
        o.ibi_bytes = lengths[i];
        observe(o);
        ASSERT_EQ(nx_i3c_service(&controller), NX_SUCCESS);
    }
    EXPECT_EQ(controller.event_count, 0U);
    EXPECT_EQ(controller.event_status.rejected_ibi, 4U);
    targets[0].bcr = 2;
    o.ibi_bytes = 0;
    observe(o);
    ASSERT_EQ(nx_i3c_service(&controller), NX_SUCCESS);
    nx_i3c_event_t event = {};
    ASSERT_EQ(nx_i3c_event_take(&controller, &event), NX_SUCCESS);
    EXPECT_EQ(event.bytes, 0U);
}
TEST_F(I3cController, FaultedControllerCannotReleaseUnknownAddressBaseline) {
    discover();
    nx_i3c_observation_t o = {};
    o.identity = true;
    o.pid = 0xabcdef123456;
    observe(o);
    ASSERT_EQ(nx_i3c_service(&controller), NX_ERROR_NACK);
    drain_success();
    EXPECT_CALL(port, Quiescent()).Times(0);
    EXPECT_CALL(port, Release(_)).Times(0);
    EXPECT_EQ(nx_i3c_stop(&controller), NX_ERROR_STATE);
    EXPECT_EQ(port.owner, &controller);
}
TEST_F(I3cController, GetMrlLengthUsesConfirmedBcrPayloadBit) {
    auto endpoint = known();
    targets[0].bcr = 2;
    ASSERT_EQ(nx_i3c_request_prepare(&request, NX_I3C_CCC, endpoint, 0x8c,
                                     bytes, 3, true, false, 100),
              NX_SUCCESS);
    nx_i3c_ticket_t ticket = {};
    EXPECT_EQ(nx_i3c_submit(&controller, &request, &ticket), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    ASSERT_EQ(nx_i3c_request_prepare(&request, NX_I3C_CCC, endpoint, 0x8c,
                                     bytes, 2, true, false, 100),
              NX_SUCCESS);
    ASSERT_EQ(nx_i3c_submit(&controller, &request, &ticket), NX_SUCCESS);
    ASSERT_EQ(nx_i3c_cancel(ticket), NX_SUCCESS);
    drain_success();
    targets[0].bcr = 6;
    ASSERT_EQ(nx_i3c_request_prepare(&request, NX_I3C_CCC, endpoint, 0x8c,
                                     bytes, 2, true, false, 100),
              NX_SUCCESS);
    EXPECT_EQ(nx_i3c_submit(&controller, &request, &ticket), NX_ERROR_INVALID);
    ASSERT_EQ(nx_i3c_request_prepare(&request, NX_I3C_CCC, endpoint, 0x8c,
                                     bytes, 3, true, false, 100),
              NX_SUCCESS);
    EXPECT_EQ(nx_i3c_submit(&controller, &request, &ticket), NX_SUCCESS);
}
} /* namespace */
