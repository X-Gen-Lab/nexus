/**
 * \file            h563_i3c_port_test.cpp
 * \brief           Real H563 C11 polling transport over consuming FIFO models
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-06
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32h563_i3c_port.h"
#include <array>
#include <deque>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>
namespace {
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
struct Model {
    I3C_TypeDef registers{};
    std::deque<uint8_t> receive;
    std::vector<uint8_t> transmit;
    std::vector<uint32_t> commands;
    size_t writes = 0;
    size_t reads = 0;
    bool flush_stuck = false;
    uint32_t error_on_receive = 0;
    int evr_reads_before_error = -1;
    bool refuse_event_close = false;
    Model() {
        registers.CFGR = I3C_CFGR_EN | I3C_CFGR_CRINIT;
        registers.DEVR0 = I3C_DEVR0_DAVAL | (0x08U << I3C_DEVR0_DA_Pos);
        registers.EVR = I3C_EVR_CFEF | I3C_EVR_TXFEF;
    }
    void feed(std::initializer_list<uint8_t> bytes) {
        receive.insert(receive.end(), bytes);
        registers.EVR |= I3C_EVR_RXFNEF;
    }
    void complete(uint16_t bytes = 0) {
        registers.SR = bytes;
        registers.EVR |= I3C_EVR_FCF | I3C_EVR_CFEF | I3C_EVR_TXFEF;
    }
};
std::vector<Model*> models;
class Resources {
  public:
    MOCK_METHOD(nx_result_t, Acquire, (const void*));
    MOCK_METHOD(void, Release, (const void*));
    MOCK_METHOD(nx_result_t, Quiescent, ());
    MOCK_METHOD(nx_result_t, ResetTargets, ());
};
nx_result_t acquire(void* p, const void* owner) {
    return static_cast<Resources*>(p)->Acquire(owner);
}
void release(void* p, const void* owner) {
    static_cast<Resources*>(p)->Release(owner);
}
nx_result_t quiescent(void* p) {
    return static_cast<Resources*>(p)->Quiescent();
}
nx_result_t reset_targets(void* p) {
    return static_cast<Resources*>(p)->ResetTargets();
}
const nx_h563_i3c_resources_t resources_ops = {acquire, release, quiescent,
                                               reset_targets};
class H563I3c : public ::testing::Test {
  protected:
    Model hardware;
    NiceMock<Resources> resources;
    nx_h563_i3c_port_t port{};
    int owner = 0;
    void SetUp() override {
        models.push_back(&hardware);
        ON_CALL(resources, Acquire).WillByDefault(Return(NX_SUCCESS));
        ON_CALL(resources, Quiescent).WillByDefault(Return(NX_SUCCESS));
        ON_CALL(resources, ResetTargets)
            .WillByDefault(Return(NX_ERROR_UNSUPPORTED));
        const nx_h563_i3c_port_config_t config = {&hardware.registers,
                                                  &resources_ops, &resources};
        ASSERT_EQ(nx_h563_i3c_port_bind(&port, &config), NX_SUCCESS);
    }
    void TearDown() override {
        models.clear();
    }
    void claim() {
        ASSERT_EQ(nx_h563_i3c_port_ops.claim(&port, &owner), NX_SUCCESS);
    }
    void begin(nx_i3c_kind_t kind, uint32_t bytes = 0, bool read = false,
               uint8_t ccc = 0, bool broadcast = false) {
        nx_i3c_command_t command = {kind, bytes, 0x12, ccc, read, broadcast};
        nx_h563_i3c_port_ops.begin(&port, &command);
    }
    nx_i3c_observation_t pump(const uint8_t* tx = nullptr, size_t n = 0,
                              uint8_t* rx = nullptr, size_t r = 0) {
        nx_i3c_observation_t observation{};
        nx_h563_i3c_port_ops.pump(&port, tx, n, rx, r, &observation);
        return observation;
    }
    void identity(uint8_t bcr = 6) {
        hardware.feed({0x11, 0x22, 0x33, 0x44, 0x55, 0x66, bcr, 0x9a});
        hardware.registers.EVR |= I3C_EVR_TXFNFF;
    }
};
TEST_F(H563I3c, ClaimRequiresProvenBaselineWithoutRejectedWrites) {
    EXPECT_CALL(resources, Acquire(&owner))
        .WillOnce(Return(NX_ERROR_PERMISSION));
    const auto writes = hardware.writes;
    EXPECT_EQ(nx_h563_i3c_port_ops.claim(&port, &owner), NX_ERROR_PERMISSION);
    EXPECT_EQ(hardware.writes, writes);
    EXPECT_EQ(port.owner, nullptr);
}
TEST_F(H563I3c, RejectsErratumAndDmaIrqModesBeforeResourceClaim) {
    EXPECT_CALL(resources, Acquire).Times(0);
    for (auto bit : {I3C_CFGR_NOARBH, I3C_CFGR_RXDMAEN, I3C_CFGR_TMODE,
                     I3C_CFGR_RXTHRES}) {
        hardware.registers.CFGR =
            static_cast<uint32_t>(I3C_CFGR_EN | I3C_CFGR_CRINIT | bit);
        EXPECT_EQ(nx_h563_i3c_port_ops.claim(&port, &owner),
                  NX_ERROR_UNSUPPORTED);
    }
    hardware.registers.CFGR = I3C_CFGR_EN | I3C_CFGR_CRINIT;
    hardware.registers.IER = I3C_IER_FCIE;
    EXPECT_EQ(nx_h563_i3c_port_ops.claim(&port, &owner), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(hardware.writes, 0U);
}
TEST_F(H563I3c, PrivateTxIsCapacityBoundedAndCompletionIsNotFifoEnqueue) {
    claim();
    begin(NX_I3C_PRIVATE, 5);
    ASSERT_EQ(hardware.commands.size(), 1U);
    EXPECT_EQ(hardware.commands[0],
              (0x12U << I3C_CR_ADD_Pos) | 5U | I3C_CR_MTYPE_1 | I3C_CR_MEND);
    hardware.registers.EVR |= I3C_EVR_TXFNFF;
    const uint8_t tx[] = {1, 2, 3, 4, 5};
    auto observation = pump(tx, 2);
    EXPECT_EQ(observation.transmitted, 2U);
    EXPECT_FALSE(observation.complete);
    EXPECT_EQ(hardware.transmit, (std::vector<uint8_t>{1, 2}));
    observation = pump(tx + 2, 3);
    EXPECT_EQ(observation.transmitted, 3U);
    EXPECT_FALSE(observation.complete);
    EXPECT_EQ(nx_h563_i3c_port_ops.quiescent(&port), NX_ERROR_BUSY);
    hardware.complete(5);
    observation = pump();
    EXPECT_TRUE(observation.complete);
    EXPECT_EQ(observation.error, NX_SUCCESS);
    EXPECT_EQ(nx_h563_i3c_port_ops.quiescent(&port), NX_SUCCESS);
}
TEST_F(H563I3c, ReceiveConsumesOnlySuppliedSpanIncludingAfterFrameComplete) {
    claim();
    begin(NX_I3C_PRIVATE, 5, true);
    hardware.feed({10, 11, 12, 13, 14});
    hardware.complete(5);
    uint8_t rx[] = {0xee, 0xee, 0xee, 0xee};
    auto observation = pump(nullptr, 0, rx + 1, 2);
    EXPECT_EQ(observation.received, 2U);
    EXPECT_FALSE(observation.complete);
    EXPECT_EQ(rx[0], 0xee);
    EXPECT_EQ(rx[1], 10);
    EXPECT_EQ(rx[2], 11);
    EXPECT_EQ(rx[3], 0xee);
    EXPECT_EQ(hardware.receive.size(), 3U);
    observation = pump(nullptr, 0, rx, 3);
    EXPECT_EQ(observation.received, 3U);
    EXPECT_TRUE(observation.complete);
    EXPECT_TRUE(hardware.receive.empty());
}
TEST_F(H563I3c, DirectCccSequencesOneHeaderAndAddressedReadWithStop) {
    claim();
    begin(NX_I3C_CCC, 6, true, 0x8d);
    ASSERT_EQ(hardware.commands.size(), 1U);
    EXPECT_EQ(hardware.commands[0],
              (0x8dU << I3C_CR_CCC_Pos) | I3C_CR_MTYPE_2 | I3C_CR_MTYPE_1);
    hardware.registers.EVR |= I3C_EVR_CFNFF;
    (void)pump();
    ASSERT_EQ(hardware.commands.size(), 2U);
    EXPECT_EQ(hardware.commands[1], 6U | (0x12U << I3C_CR_ADD_Pos) |
                                        I3C_CR_RNW | I3C_CR_MTYPE_1 |
                                        I3C_CR_MTYPE_0 | I3C_CR_MEND);
    (void)pump();
    EXPECT_EQ(hardware.commands.size(), 2U);
}
TEST_F(H563I3c, DaaPublishesCopiedIdentityAndOnlyWireAcknowledgedAssignment) {
    claim();
    begin(NX_I3C_DISCOVER);
    identity();
    auto observation = pump();
    EXPECT_TRUE(observation.identity);
    EXPECT_EQ(observation.pid, 0x112233445566ULL);
    EXPECT_EQ(observation.bcr, 6);
    EXPECT_EQ(observation.dcr, 0x9a);
    EXPECT_FALSE(observation.assigned);
    nx_h563_i3c_port_ops.assign(&port, 0x12);
    ASSERT_FALSE(hardware.transmit.empty());
    EXPECT_EQ(hardware.transmit.back(), 0x12);
    EXPECT_FALSE(pump().assigned);
    hardware.complete();
    observation = pump();
    EXPECT_TRUE(observation.assigned);
    EXPECT_TRUE(observation.complete);
    EXPECT_EQ((hardware.registers.DEVRX[0] & I3C_DEVRX_DA) >> I3C_DEVRX_DA_Pos,
              0x12U);
    EXPECT_NE(hardware.registers.DEVRX[0] & I3C_DEVRX_IBIACK, 0U);
}
TEST_F(H563I3c, DaaErrorNeverConfirmsTentativeAddress) {
    claim();
    begin(NX_I3C_DISCOVER);
    identity();
    (void)pump();
    nx_h563_i3c_port_ops.assign(&port, 0x12);
    hardware.complete();
    hardware.registers.EVR |= I3C_EVR_ERRF;
    hardware.registers.SER = I3C_SER_DNACK;
    const auto observation = pump();
    EXPECT_EQ(observation.error, NX_ERROR_NACK);
    EXPECT_FALSE(observation.assigned);
    EXPECT_EQ(hardware.registers.DEVRX[0], 0U);
}
TEST_F(H563I3c, CopiesIbiPayloadAndHotJoinOnceWithoutDiscovery) {
    claim();
    hardware.registers.RMR = (0x12U << I3C_RMR_RADD_Pos) | 4U;
    hardware.registers.IBIDR = 0x44332211U;
    hardware.registers.EVR |= I3C_EVR_IBIF | I3C_EVR_HJF;
    const auto observation = pump();
    ASSERT_TRUE(observation.ibi);
    EXPECT_EQ(observation.ibi_address, 0x12);
    EXPECT_EQ(observation.ibi_bytes, 4);
    EXPECT_EQ(observation.ibi_payload[0], 0x11);
    EXPECT_EQ(observation.ibi_payload[3], 0x44);
    EXPECT_TRUE(observation.hot_join);
    EXPECT_FALSE(pump().ibi);
    EXPECT_FALSE(pump().hot_join);
    EXPECT_TRUE(hardware.commands.empty());
}
TEST_F(H563I3c, WithdrawalWaitsForFrameFlushAndExternalProof) {
    claim();
    begin(NX_I3C_PRIVATE, 3);
    nx_h563_i3c_port_ops.withdraw(&port);
    EXPECT_EQ(nx_h563_i3c_port_ops.quiescent(&port), NX_ERROR_BUSY);
    hardware.complete();
    EXPECT_CALL(resources, Quiescent)
        .WillOnce(Return(NX_ERROR_BUSY))
        .WillRepeatedly(Return(NX_SUCCESS));
    EXPECT_EQ(nx_h563_i3c_port_ops.quiescent(&port), NX_ERROR_BUSY);
    EXPECT_EQ(port.owner, &owner);
    EXPECT_EQ(nx_h563_i3c_port_ops.quiescent(&port), NX_SUCCESS);
}
TEST_F(H563I3c, PeripheralFlushCannotPretendWholeTargetReset) {
    claim();
    EXPECT_CALL(resources, ResetTargets).WillOnce(Return(NX_ERROR_UNSUPPORTED));
    EXPECT_EQ(nx_h563_i3c_port_ops.reset(&port), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(port.owner, &owner);
    EXPECT_CALL(resources, ResetTargets).WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(nx_h563_i3c_port_ops.reset(&port), NX_SUCCESS);
    EXPECT_EQ(port.owner, &owner);
}
TEST_F(H563I3c, ReleaseRequiresMatchingOwnerAndFinalQuiescence) {
    claim();
    begin(NX_I3C_PRIVATE, 2);
    EXPECT_CALL(resources, Release).Times(0);
    nx_h563_i3c_port_ops.release(&port, &owner);
    EXPECT_EQ(port.owner, &owner);
    ::testing::Mock::VerifyAndClearExpectations(&resources);
    nx_h563_i3c_port_ops.withdraw(&port);
    hardware.complete();
    EXPECT_EQ(nx_h563_i3c_port_ops.quiescent(&port), NX_SUCCESS);
    EXPECT_CALL(resources, Release(&owner)).Times(1);
    nx_h563_i3c_port_ops.release(&port, &owner);
    EXPECT_EQ(port.owner, nullptr);
}
TEST_F(H563I3c, SeparatePortsNeverWriteAnotherControllerOrBorrowedSpan) {
    claim();
    Model other;
    models.push_back(&other);
    nx_h563_i3c_port_t other_port{};
    const nx_h563_i3c_port_config_t config = {&other.registers, &resources_ops,
                                              &resources};
    ASSERT_EQ(nx_h563_i3c_port_bind(&other_port, &config), NX_SUCCESS);
    ASSERT_EQ(nx_h563_i3c_port_ops.claim(&other_port, &other), NX_SUCCESS);
    begin(NX_I3C_PRIVATE, 1);
    hardware.registers.EVR |= I3C_EVR_TXFNFF;
    uint8_t byte = 0x12;
    EXPECT_EQ(pump(&byte, 1).transmitted, 1U);
    byte = 0xa5;
    (void)pump();
    EXPECT_EQ(hardware.transmit, (std::vector<uint8_t>{0x12}));
    EXPECT_TRUE(other.transmit.empty());
    EXPECT_TRUE(other.commands.empty());
}
TEST_F(H563I3c, BindRejectsMissingProofLeavesAndCannotOverwriteBoundStorage) {
    nx_h563_i3c_port_t fresh{};
    nx_h563_i3c_port_config_t config = {&hardware.registers, &resources_ops,
                                        &resources};
    EXPECT_EQ(nx_h563_i3c_port_bind(nullptr, &config), NX_ERROR_INVALID);
    EXPECT_EQ(nx_h563_i3c_port_bind(&fresh, nullptr), NX_ERROR_INVALID);
    config.registers = nullptr;
    EXPECT_EQ(nx_h563_i3c_port_bind(&fresh, &config), NX_ERROR_INVALID);
    config.registers = &hardware.registers;
    auto incomplete = resources_ops;
    incomplete.reset_targets = nullptr;
    config.resources = &incomplete;
    EXPECT_EQ(nx_h563_i3c_port_bind(&fresh, &config), NX_ERROR_INVALID);
    EXPECT_EQ(nx_h563_i3c_port_bind(&port, &config), NX_ERROR_STATE);
    EXPECT_EQ(hardware.writes, 0U);
}
TEST_F(H563I3c, SecondClaimAndReadyModeDriftHaveNoHardwareEffects) {
    claim();
    const auto writes = hardware.writes;
    EXPECT_EQ(nx_h563_i3c_port_ops.claim(&port, &hardware), NX_ERROR_BUSY);
    EXPECT_EQ(hardware.writes, writes);
    EXPECT_EQ(nx_h563_i3c_port_ops.ready(&port), NX_SUCCESS);
    hardware.registers.CFGR |= I3C_CFGR_TXDMAEN;
    EXPECT_EQ(nx_h563_i3c_port_ops.ready(&port), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(hardware.writes, writes);
}
TEST_F(H563I3c, DaaMetadataWaitsForAllEightBytesAndAcknowledgesBeforeNextPid) {
    claim();
    begin(NX_I3C_DISCOVER);
    hardware.feed({0x11, 0x22, 0x33, 0x44});
    hardware.registers.EVR |= I3C_EVR_TXFNFF;
    EXPECT_FALSE(pump().identity);
    hardware.feed({0x55, 0x66, 0, 1});
    ASSERT_TRUE(pump().identity);
    nx_h563_i3c_port_ops.assign(&port, 0x12);
    hardware.feed({0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0, 2});
    hardware.registers.EVR |= I3C_EVR_TXFNFF;
    const auto observation = pump();
    EXPECT_TRUE(observation.assigned);
    EXPECT_TRUE(observation.identity);
    EXPECT_EQ(observation.pid, 0x665544332211ULL);
    EXPECT_EQ(observation.dcr, 2);
}
TEST_F(H563I3c, UnexpectedFrameByteCountAndOverlengthIbiAreErrors) {
    claim();
    begin(NX_I3C_PRIVATE, 2, true);
    hardware.feed({0x31});
    hardware.complete(1);
    uint8_t rx[2]{};
    const auto observation = pump(nullptr, 0, rx, 2);
    EXPECT_EQ(observation.received, 1U);
    EXPECT_EQ(observation.error, NX_ERROR_IO);
    EXPECT_FALSE(observation.complete);
    nx_h563_i3c_port_ops.withdraw(&port);
    EXPECT_EQ(nx_h563_i3c_port_ops.quiescent(&port), NX_SUCCESS);
    hardware.registers.RMR = 7;
    hardware.registers.EVR |= I3C_EVR_IBIF;
    const auto event = pump();
    EXPECT_EQ(event.error, NX_ERROR_OVERFLOW);
    EXPECT_FALSE(event.ibi);
}
TEST_F(H563I3c, FlushRequestsAloneDoNotProveFifoWithdrawal) {
    claim();
    begin(NX_I3C_PRIVATE, 1);
    hardware.flush_stuck = true;
    hardware.complete();
    nx_h563_i3c_port_ops.withdraw(&port);
    EXPECT_EQ(nx_h563_i3c_port_ops.quiescent(&port), NX_ERROR_BUSY);
    EXPECT_EQ(port.owner, &owner);
}
TEST_F(H563I3c, BroadcastCccWritesOneByteWithNoAddressedFollowup) {
    claim();
    begin(NX_I3C_CCC, 1, false, 0x00, true);
    ASSERT_EQ(hardware.commands.size(), 1U);
    EXPECT_EQ(hardware.commands[0],
              1U | I3C_CR_MTYPE_2 | I3C_CR_MTYPE_1 | I3C_CR_MEND);
    hardware.registers.EVR |= I3C_EVR_TXFNFF | I3C_EVR_CFNFF;
    const uint8_t event_mask = 3;
    EXPECT_EQ(pump(&event_mask, 1).transmitted, 1U);
    EXPECT_EQ(hardware.commands.size(), 1U);
}
TEST_F(H563I3c, ActualCoreRetainsBorrowAndAddressesUntilPortProof) {
    nx_i3c_controller_t controller{};
    const nx_i3c_target_definition_t definition = {0x112233445566ULL, 0x12};
    nx_i3c_target_state_t target{};
    nx_i3c_event_t events[2]{};
    nx_time_us_t now = 10;
    const nx_i3c_controller_config_t config = {
        &nx_h563_i3c_port_ops,
        &port,
        {[](void* p) { return *static_cast<nx_time_us_t*>(p); }, &now},
        &definition,
        &target,
        1,
        events,
        2,
        1};
    ASSERT_EQ(nx_i3c_controller_construct(&controller, &config), NX_SUCCESS);
    nx_i3c_request_t request{};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_i3c_request_prepare(&request, NX_I3C_DISCOVER, {}, 0, nullptr,
                                     0, false, false, 100),
              NX_SUCCESS);
    nx_i3c_ticket_t ticket{};
    ASSERT_EQ(nx_i3c_submit(&controller, &request, &ticket), NX_SUCCESS);
    identity();
    ASSERT_EQ(nx_i3c_service(&controller), NX_SUCCESS);
    ASSERT_TRUE(target.tentative);
    EXPECT_FALSE(target.assigned);
    hardware.complete();
    ASSERT_EQ(nx_i3c_service(&controller), NX_SUCCESS);
    EXPECT_TRUE(target.assigned);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    EXPECT_CALL(resources, Quiescent)
        .WillOnce(Return(NX_ERROR_BUSY))
        .WillRepeatedly(Return(NX_SUCCESS));
    EXPECT_EQ(nx_i3c_service(&controller), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(nx_i3c_service(&controller), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    nx_i3c_endpoint_t endpoint{};
    ASSERT_EQ(nx_i3c_endpoint(&controller, 0, &endpoint), NX_SUCCESS);
    uint8_t bytes[] = {0x41, 0x42};
    ASSERT_EQ(nx_i3c_request_prepare(&request, NX_I3C_PRIVATE, endpoint, 0,
                                     bytes, 2, false, false, 100),
              NX_SUCCESS);
    ASSERT_EQ(nx_i3c_submit(&controller, &request, &ticket), NX_SUCCESS);
    ASSERT_EQ(nx_i3c_cancel(ticket), NX_SUCCESS);
    EXPECT_EQ(nx_i3c_service(&controller), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    hardware.complete();
    EXPECT_EQ(nx_i3c_service(&controller), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    ASSERT_EQ(nx_i3c_close(&controller), NX_SUCCESS);
    EXPECT_EQ(nx_i3c_reset(&controller), NX_ERROR_UNSUPPORTED);
    EXPECT_TRUE(target.assigned);
    EXPECT_CALL(resources, ResetTargets).WillOnce(Return(NX_SUCCESS));
    ASSERT_EQ(nx_i3c_reset(&controller), NX_SUCCESS);
    EXPECT_FALSE(target.assigned);
    EXPECT_EQ(nx_i3c_stop(&controller), NX_SUCCESS);
}
TEST_F(H563I3c, CountBoundariesKeepSixteenBitsAndEightByteServiceBudget) {
    for (const uint32_t bytes : {1U, 7U, 8U, 9U, 65535U}) {
        SCOPED_TRACE(bytes);
        claim();
        hardware.commands.clear();
        hardware.transmit.clear();
        begin(NX_I3C_PRIVATE, bytes);
        ASSERT_EQ(hardware.commands.size(), 1U);
        EXPECT_EQ(hardware.commands[0] & I3C_CR_DCNT, bytes);
        hardware.registers.EVR |= I3C_EVR_TXFNFF;
        const uint8_t tx[16] = {1, 2, 3, 4, 5, 6, 7, 8};
        size_t moved = 0;
        while (moved < bytes) {
            const auto observation = pump(tx, sizeof(tx));
            ASSERT_GT(observation.transmitted, 0U);
            EXPECT_LE(observation.transmitted, 8U);
            EXPECT_LE(observation.transmitted, bytes - moved);
            EXPECT_FALSE(observation.complete);
            moved += observation.transmitted;
        }
        hardware.complete(static_cast<uint16_t>(bytes));
        EXPECT_TRUE(pump().complete);
        ASSERT_EQ(nx_h563_i3c_port_ops.quiescent(&port), NX_SUCCESS);
        nx_h563_i3c_port_ops.release(&port, &owner);
    }
}
TEST_F(H563I3c, FifthIbiCapableTargetRejectsWithoutAliasingHardwareSlots) {
    claim();
    begin(NX_I3C_DISCOVER);
    for (uint8_t i = 0; i < 4; ++i) {
        identity();
        const auto observation = pump();
        ASSERT_TRUE(observation.identity);
        EXPECT_EQ(observation.assigned, i != 0);
        nx_h563_i3c_port_ops.assign(&port, static_cast<uint8_t>(0x12U + i));
    }
    identity();
    const auto observation = pump();
    EXPECT_TRUE(observation.assigned);
    EXPECT_EQ(observation.error, NX_ERROR_OVERFLOW);
    EXPECT_FALSE(observation.identity);
    for (uint32_t i = 0; i < 4; ++i) {
        EXPECT_EQ((hardware.registers.DEVRX[i] & I3C_DEVRX_DA) >>
                      I3C_DEVRX_DA_Pos,
                  0x12U + i);
    }
}
TEST_F(H563I3c, MissingControllerAddressValidityRejectsBeforeResourceClaim) {
    hardware.registers.DEVR0 = 0;
    EXPECT_CALL(resources, Acquire).Times(0);
    EXPECT_EQ(nx_h563_i3c_port_ops.claim(&port, &owner), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(hardware.writes, 0U);
}
TEST_F(H563I3c, ExtraRxFifoDataIsErrorBeforeEnteringDrain) {
    claim();
    begin(NX_I3C_PRIVATE, 1, true);
    hardware.feed({0x31, 0x32});
    hardware.complete(1);
    uint8_t rx = 0;
    const auto observation = pump(nullptr, 0, &rx, 1);
    EXPECT_EQ(rx, 0x31);
    EXPECT_EQ(observation.received, 1U);
    EXPECT_EQ(observation.error, NX_ERROR_IO);
    EXPECT_FALSE(observation.complete);
    EXPECT_EQ(hardware.receive.size(), 1U);
}
TEST_F(H563I3c, ErrorDuringLastFifoReadSuppressesSuccessfulCompletion) {
    claim();
    begin(NX_I3C_PRIVATE, 1, true);
    hardware.feed({0x31});
    hardware.complete(1);
    hardware.error_on_receive = I3C_SER_PERR;
    uint8_t rx = 0;
    const auto observation = pump(nullptr, 0, &rx, 1);
    EXPECT_EQ(observation.received, 1U);
    EXPECT_EQ(observation.error, NX_ERROR_IO);
    EXPECT_FALSE(observation.complete);
}
TEST_F(H563I3c, DaaCannotAssignTheControllersOwnDynamicAddress) {
    claim();
    begin(NX_I3C_DISCOVER);
    identity();
    ASSERT_TRUE(pump().identity);
    nx_h563_i3c_port_ops.assign(&port, 0x08);
    EXPECT_TRUE(hardware.transmit.empty());
    EXPECT_EQ(pump().error, NX_ERROR_INVALID);
}
TEST_F(H563I3c, WholeBusResetDiscardsOldHardwareEventLatches) {
    claim();
    hardware.registers.RMR = (0x12U << I3C_RMR_RADD_Pos) | 1U;
    hardware.registers.IBIDR = 0x55;
    hardware.registers.EVR |= I3C_EVR_IBIF | I3C_EVR_HJF;
    EXPECT_CALL(resources, ResetTargets).WillOnce(Return(NX_SUCCESS));
    ASSERT_EQ(nx_h563_i3c_port_ops.reset(&port), NX_SUCCESS);
    const auto observation = pump();
    EXPECT_FALSE(observation.ibi);
    EXPECT_FALSE(observation.hot_join);
}
TEST_F(H563I3c, CoreStopConsumesOneSuccessfulProofAndReleasesInfallibly) {
    nx_i3c_controller_t controller{};
    const nx_i3c_target_definition_t definition = {0x112233445566ULL, 0x12};
    nx_i3c_target_state_t target{};
    nx_i3c_event_t events[1]{};
    nx_time_us_t now = 10;
    const nx_i3c_controller_config_t config = {
        &nx_h563_i3c_port_ops,
        &port,
        {[](void* p) { return *static_cast<nx_time_us_t*>(p); }, &now},
        &definition,
        &target,
        1,
        events,
        1,
        1};
    ASSERT_EQ(nx_i3c_controller_construct(&controller, &config), NX_SUCCESS);
    EXPECT_CALL(resources, Quiescent)
        .WillOnce(Return(NX_SUCCESS))
        .WillRepeatedly(Return(NX_ERROR_BUSY));
    EXPECT_CALL(resources, Release(&controller)).Times(1);
    EXPECT_EQ(nx_i3c_stop(&controller), NX_SUCCESS);
    EXPECT_EQ(port.owner, nullptr);
    EXPECT_TRUE(controller.joined);
}
TEST_F(H563I3c, QuiescenceClosesEventAdmissionBeforeProofAndPumpResumesIt) {
    claim();
    begin(NX_I3C_DISCOVER);
    identity();
    ASSERT_TRUE(pump().identity);
    nx_h563_i3c_port_ops.assign(&port, 0x12);
    hardware.complete();
    ASSERT_TRUE(pump().complete);
    EXPECT_CALL(resources, Quiescent).WillOnce(::testing::Invoke([this] {
        EXPECT_EQ(hardware.registers.CFGR & I3C_CFGR_HJACK, 0U);
        EXPECT_EQ(hardware.registers.DEVRX[0] & I3C_DEVRX_IBIACK, 0U);
        return NX_SUCCESS;
    }));
    ASSERT_EQ(nx_h563_i3c_port_ops.quiescent(&port), NX_SUCCESS);
    (void)pump();
    EXPECT_NE(hardware.registers.CFGR & I3C_CFGR_HJACK, 0U);
    EXPECT_NE(hardware.registers.DEVRX[0] & I3C_DEVRX_IBIACK, 0U);
}
TEST_F(H563I3c, ErrorAndFrameRaisedBetweenReadsCannotConfirmDaaAssignment) {
    claim();
    begin(NX_I3C_DISCOVER);
    identity();
    ASSERT_TRUE(pump().identity);
    nx_h563_i3c_port_ops.assign(&port, 0x12);
    hardware.registers.EVR &= ~static_cast<uint32_t>(I3C_EVR_FCF);
    hardware.evr_reads_before_error = 3;
    const auto observation = pump();
    EXPECT_FALSE(observation.assigned);
    EXPECT_EQ(observation.error, NX_ERROR_NACK);
    EXPECT_FALSE(observation.complete);
    EXPECT_EQ(hardware.registers.DEVRX[0], 0U);
}
TEST_F(H563I3c, RefusedEventDisableCannotReachExternalProofOrRelease) {
    claim();
    hardware.refuse_event_close = true;
    EXPECT_CALL(resources, Quiescent).Times(0);
    EXPECT_CALL(resources, Release).Times(0);
    EXPECT_EQ(nx_h563_i3c_port_ops.quiescent(&port), NX_ERROR_BUSY);
    nx_h563_i3c_port_ops.release(&port, &owner);
    EXPECT_EQ(port.owner, &owner);
}
TEST_F(H563I3c, ClaimDoesNotProgramTargetRoleIbiPayloadLength) {
    hardware.registers.MAXRLR = 0x12345;
    claim();
    EXPECT_EQ(hardware.registers.MAXRLR, 0x12345U);
}
} /* namespace */

extern "C" uint32_t nx_h563_i3c_model_read(const volatile uint32_t* address) {
    for (auto* model : models) {
        auto& registers = model->registers;
        const auto offset = reinterpret_cast<uintptr_t>(address) -
                            reinterpret_cast<uintptr_t>(&registers);
        if (offset >= sizeof(registers)) {
            continue;
        }
        ++model->reads;
        if (address == &registers.EVR && model->evr_reads_before_error >= 0) {
            if (model->evr_reads_before_error-- == 0) {
                registers.SER = I3C_SER_DNACK;
                registers.EVR |=
                    I3C_EVR_ERRF | I3C_EVR_FCF | I3C_EVR_TXFEF | I3C_EVR_CFEF;
            }
        }
        if (address == &registers.RDR) {
            EXPECT_FALSE(model->receive.empty());
            if (model->receive.empty()) {
                return 0;
            }
            const auto byte = model->receive.front();
            model->receive.pop_front();
            if (model->receive.empty()) {
                registers.EVR &= ~static_cast<uint32_t>(I3C_EVR_RXFNEF);
            }
            if (model->error_on_receive != 0) {
                registers.SER = model->error_on_receive;
                registers.EVR |= I3C_EVR_ERRF;
            }
            return byte;
        }
        return *address;
    }
    ADD_FAILURE() << "Unexpected register read";
    return 0;
}
extern "C" void nx_h563_i3c_model_write(volatile uint32_t* address,
                                        uint32_t value) {
    for (auto* model : models) {
        auto& registers = model->registers;
        const auto offset = reinterpret_cast<uintptr_t>(address) -
                            reinterpret_cast<uintptr_t>(&registers);
        if (offset >= sizeof(registers)) {
            continue;
        }
        ++model->writes;
        if (address == &registers.CEVR) {
            registers.EVR &= ~value;
        } else if (address == &registers.TDR) {
            model->transmit.push_back(static_cast<uint8_t>(value));
            registers.EVR &= ~static_cast<uint32_t>(I3C_EVR_TXFEF);
            if (!model->commands.empty() &&
                ((model->commands.back() >> I3C_CR_CCC_Pos) & 0xffU) == 7U) {
                registers.EVR &= ~static_cast<uint32_t>(I3C_EVR_TXFNFF);
            }
        } else if (address == &registers.CR) {
            model->commands.push_back(value);
            *address = value;
            registers.EVR &= ~static_cast<uint32_t>(I3C_EVR_FCF);
        } else if (address == &registers.CFGR && !model->flush_stuck) {
            const uint32_t flush = I3C_CFGR_RXFLUSH | I3C_CFGR_TXFLUSH |
                                   I3C_CFGR_CFLUSH | I3C_CFGR_SFLUSH;
            *address = value & ~flush;
            if (model->refuse_event_close) {
                registers.CFGR |= I3C_CFGR_HJACK;
            }
            if ((value & I3C_CFGR_RXFLUSH) != 0) {
                model->receive.clear();
                registers.EVR &= ~static_cast<uint32_t>(I3C_EVR_RXFNEF);
            }
            if ((value & I3C_CFGR_TXFLUSH) != 0) {
                registers.EVR |= I3C_EVR_TXFEF;
            }
            if ((value & I3C_CFGR_CFLUSH) != 0) {
                registers.EVR |= I3C_EVR_CFEF;
            }
            if ((value & I3C_CFGR_SFLUSH) != 0) {
                registers.EVR &= ~static_cast<uint32_t>(I3C_EVR_SFNEF);
            }
        } else {
            *address = value;
        }
        return;
    }
    ADD_FAILURE() << "Unexpected register write";
}
