/**
 * \file            h747_amp_test.cpp
 * \brief           Actual AMP borrow lifecycle over exact H747 hardware ports
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-06
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "hardware.h"
#include "nexus/components/amp_ipc_storage.h"
#include "nexus/components/stm32h747_amp.h"
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
namespace {
using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
class Hardware {
  public:
    MOCK_METHOD(bool, Ready, (const void*, size_t, uint8_t));
    MOCK_METHOD(bool, Take, (uint8_t, uint8_t));
    MOCK_METHOD(bool, Release, (uint8_t, uint8_t));
    bool owned[32] = {};
};
Hardware* hardware;
struct Authority {
    uint32_t last = 0;
    unsigned calls = 0;
};
nx_result_t admit(void* context, uint32_t epoch) {
    auto* authority = static_cast<Authority*>(context);
    ++authority->calls;
    if (epoch <= authority->last)
        return NX_ERROR_STATE;
    authority->last = epoch;
    return NX_SUCCESS;
}
struct alignas(4096) Shared {
    nx_amp_wire_slot_t wire[2];
    nx_amp_word_t sender;
    nx_amp_word_t receiver;
    uint8_t payload[128];
};
class H747Amp : public ::testing::Test {
  protected:
    NiceMock<Hardware> hw;
    Shared shared{};
    Authority tx_authority{}, rx_authority{};
    nx_h747_amp_t tx_port{}, rx_port{};
    nx_h747_amp_config_t tx_config{}, rx_config{};
    nx_amp_ipc_t tx{}, rx{};
    nx_amp_local_slot_t tx_slots[2]{}, rx_slots[2]{};
    void SetUp() override {
        hardware = &hw;
        ON_CALL(hw, Ready(_, _, _)).WillByDefault(Return(true));
        ON_CALL(hw, Take(_, _))
            .WillByDefault(Invoke([this](uint8_t sem, uint8_t) {
                if (hw.owned[sem])
                    return false;
                hw.owned[sem] = true;
                return true;
            }));
        ON_CALL(hw, Release(_, _))
            .WillByDefault(Invoke([this](uint8_t sem, uint8_t) {
                hw.owned[sem] = false;
                return true;
            }));
        shared.sender.value = 7U;
        shared.receiver.value = 11U;
        tx_config = {
            &shared, sizeof(shared), shared.wire, 2U, admit, &tx_authority,
            7U,      NX_AMP_SENDER,  1U,          3U, 1U,    5U};
        rx_config = tx_config;
        rx_config.authority = &rx_authority;
        rx_config.boot_epoch = 11U;
        rx_config.role = NX_AMP_RECEIVER;
        rx_config.claim_semaphore = 2U;
        rx_config.notify_semaphore = 4U;
    }
    nx_result_t construct(nx_amp_ipc_t* endpoint, nx_h747_amp_t* port,
                          nx_amp_local_slot_t* slots, bool sender) {
        nx_amp_config_t config{nullptr,
                               nullptr,
                               shared.wire,
                               slots,
                               2U,
                               shared.payload,
                               sizeof(shared.payload),
                               sender ? &shared.sender : &shared.receiver,
                               sender ? &shared.receiver : &shared.sender,
                               sender ? 7U : 11U,
                               sender ? 11U : 7U,
                               32U,
                               sender ? NX_AMP_SENDER : NX_AMP_RECEIVER};
        return nx_h747_amp_construct(endpoint, port, &config);
    }
    void start() {
        ASSERT_EQ(nx_h747_amp_attach(&tx_port, &tx_config), NX_SUCCESS);
        ASSERT_EQ(nx_h747_amp_attach(&rx_port, &rx_config), NX_SUCCESS);
        ASSERT_EQ(construct(&tx, &tx_port, tx_slots, true), NX_SUCCESS);
        ASSERT_EQ(construct(&rx, &rx_port, rx_slots, false), NX_SUCCESS);
    }
};
TEST_F(H747Amp, UnknownBootAndInvalidReservationRejectWithoutClaim) {
    EXPECT_CALL(hw, Take(_, _)).Times(0);
    EXPECT_EQ(nx_h747_amp_attach(&tx_port, nullptr), NX_ERROR_INVALID);
    tx_config.admit_epoch = nullptr;
    EXPECT_EQ(nx_h747_amp_attach(&tx_port, &tx_config), NX_ERROR_INVALID);
    tx_config.admit_epoch = admit;
    tx_config.notify_semaphore = tx_config.claim_semaphore;
    EXPECT_EQ(nx_h747_amp_attach(&tx_port, &tx_config), NX_ERROR_INVALID);
    tx_config.notify_semaphore = 3U;
    EXPECT_CALL(hw, Ready(_, _, _)).WillOnce(Return(false));
    EXPECT_EQ(nx_h747_amp_attach(&tx_port, &tx_config), NX_ERROR_STATE);
    EXPECT_FALSE(tx_port.initialized);
}
TEST_F(H747Amp, ActualCommonMessageRetainsUntilExactAckAndJoin) {
    start();
    shared.payload[0] = 0x5a;
    nx_amp_ticket_t ticket{};
    ASSERT_EQ(nx_amp_ipc_send(&tx, 0U, 64U, &ticket), NX_SUCCESS);
    EXPECT_EQ(nx_h747_amp_notify(&tx_port), NX_SUCCESS);
    EXPECT_EQ(nx_amp_ipc_poll(ticket), NX_ERROR_BUSY);
    EXPECT_EQ(nx_h747_amp_detach(&tx_port), NX_ERROR_BUSY);
    nx_amp_view_t view{};
    ASSERT_EQ(nx_amp_ipc_receive(&rx, &view), NX_SUCCESS);
    ASSERT_EQ(view.bytes, 64U);
    EXPECT_EQ(static_cast<const uint8_t*>(view.data)[0], 0x5a);
    EXPECT_EQ(nx_amp_ipc_finish(view.ticket), NX_SUCCESS);
    EXPECT_EQ(nx_amp_ipc_poll(ticket), NX_SUCCESS);
    nx_amp_close_t close_tx{}, close_rx{};
    ASSERT_EQ(nx_amp_ipc_close(&tx, &close_tx), NX_SUCCESS);
    ASSERT_EQ(nx_amp_ipc_close(&rx, &close_rx), NX_SUCCESS);
    EXPECT_EQ(nx_amp_ipc_join(close_tx), NX_SUCCESS);
    EXPECT_EQ(nx_amp_ipc_join(close_rx), NX_SUCCESS);
    EXPECT_EQ(nx_h747_amp_detach(&tx_port), NX_SUCCESS);
    EXPECT_EQ(nx_h747_amp_detach(&rx_port), NX_SUCCESS);
    EXPECT_FALSE(hw.owned[1]);
    EXPECT_FALSE(hw.owned[2]);
}
TEST_F(H747Amp, BusyHardwareNeverConsumesPersistentEpoch) {
    ASSERT_EQ(nx_h747_amp_attach(&tx_port, &tx_config), NX_SUCCESS);
    hw.owned[1] = true;
    EXPECT_EQ(construct(&tx, &tx_port, tx_slots, true), NX_ERROR_BUSY);
    EXPECT_EQ(tx_authority.calls, 0U);
    EXPECT_EQ(tx_port.owner, nullptr);
}
TEST_F(H747Amp, ReusedPersistentEpochRejectsAndReleasesOnlyTemporaryClaim) {
    ASSERT_EQ(nx_h747_amp_attach(&tx_port, &tx_config), NX_SUCCESS);
    tx_authority.last = 7U;
    EXPECT_EQ(construct(&tx, &tx_port, tx_slots, true), NX_ERROR_STATE);
    EXPECT_EQ(tx_authority.calls, 1U);
    EXPECT_FALSE(hw.owned[1]);
    EXPECT_EQ(tx_port.owner, nullptr);
}
TEST_F(H747Amp, ForeignLaneRoleAndEpochRejectBeforeHardware) {
    start();
    EXPECT_CALL(hw, Take(_, _)).Times(0);
    EXPECT_EQ(
        tx.config.ops->claim(&tx_port, &tx, shared.wire, NX_AMP_RECEIVER, 7U),
        NX_ERROR_INVALID);
    EXPECT_EQ(
        tx.config.ops->claim(&tx_port, &tx, shared.wire, NX_AMP_SENDER, 8U),
        NX_ERROR_INVALID);
    EXPECT_EQ(tx.config.ops->claim(&tx_port, &tx, shared.wire + 1U,
                                   NX_AMP_SENDER, 7U),
              NX_ERROR_INVALID);
}
TEST_F(H747Amp, DoorbellBusyOrNewPeerEpochCannotRevokeAcceptedPayload) {
    start();
    nx_amp_ticket_t ticket{};
    ASSERT_EQ(nx_amp_ipc_send(&tx, 0U, 64U, &ticket), NX_SUCCESS);
    hw.owned[3] = true;
    EXPECT_EQ(nx_h747_amp_notify(&tx_port), NX_ERROR_BUSY);
    shared.receiver.value = 12U;
    EXPECT_EQ(nx_amp_ipc_poll(ticket), NX_ERROR_STATE);
    nx_amp_close_t close{};
    ASSERT_EQ(nx_amp_ipc_close(&tx, &close), NX_SUCCESS);
    EXPECT_EQ(nx_amp_ipc_reset_join(close), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_h747_amp_detach(&tx_port), NX_ERROR_BUSY);
    EXPECT_TRUE(hw.owned[1]);
}
TEST_F(H747Amp, CachePortRejectsOutsideAndPartialLinesBeforeAnyPublication) {
    start();
    EXPECT_EQ(tx.config.ops->clean(&tx_port, shared.payload, 32U), NX_SUCCESS);
    EXPECT_EQ(tx.config.ops->invalidate(&tx_port, shared.payload + 1U, 32U),
              NX_ERROR_INVALID);
    EXPECT_EQ(tx.config.ops->clean(&tx_port, shared.payload, 31U),
              NX_ERROR_INVALID);
    EXPECT_EQ(tx.config.ops->clean(&tx_port, &tx, 64U), NX_ERROR_INVALID);
}
TEST_F(H747Amp, RefusedDoorbellReleaseKeepsReceiptForExplicitRetry) {
    start();
    EXPECT_CALL(hw, Release(3U, 1U)).WillOnce(Return(false));
    EXPECT_EQ(nx_h747_amp_notify(&tx_port), NX_ERROR_IO);
    EXPECT_TRUE(tx_port.notify_pending);
    EXPECT_TRUE(hw.owned[3]);
    EXPECT_EQ(nx_h747_amp_detach(&tx_port), NX_ERROR_BUSY);
    testing::Mock::VerifyAndClearExpectations(&hw);
    EXPECT_EQ(nx_h747_amp_notify(&tx_port), NX_SUCCESS);
    EXPECT_FALSE(tx_port.notify_pending);
    EXPECT_FALSE(hw.owned[3]);
}
TEST_F(H747Amp, ConstructorRejectsBootWordsOutsideTheReservedSharedWindow) {
    ASSERT_EQ(nx_h747_amp_attach(&tx_port, &tx_config), NX_SUCCESS);
    alignas(64) nx_amp_word_t local{}, peer{};
    local.value = 7U;
    peer.value = 11U;
    const nx_amp_config_t config{nullptr,
                                 nullptr,
                                 shared.wire,
                                 tx_slots,
                                 2U,
                                 shared.payload,
                                 sizeof(shared.payload),
                                 &local,
                                 &peer,
                                 7U,
                                 11U,
                                 32U,
                                 NX_AMP_SENDER};
    EXPECT_EQ(nx_h747_amp_construct(&tx, &tx_port, &config), NX_ERROR_INVALID);
    EXPECT_EQ(tx_port.owner, nullptr);
    EXPECT_FALSE(hw.owned[1]);
}
TEST_F(H747Amp, ConstructorRejectsADifferentWireCapacityBeforeClaim) {
    ASSERT_EQ(nx_h747_amp_attach(&tx_port, &tx_config), NX_SUCCESS);
    const nx_amp_config_t config{nullptr,
                                 nullptr,
                                 shared.wire,
                                 tx_slots,
                                 1U,
                                 shared.payload,
                                 sizeof(shared.payload),
                                 &shared.sender,
                                 &shared.receiver,
                                 7U,
                                 11U,
                                 32U,
                                 NX_AMP_SENDER};
    EXPECT_EQ(nx_h747_amp_construct(&tx, &tx_port, &config), NX_ERROR_INVALID);
    EXPECT_EQ(tx_port.owner, nullptr);
    EXPECT_FALSE(hw.owned[1]);
}
} /* namespace */
extern "C" bool nx_h747_amp_hw_ready(const void* address, size_t bytes,
                                     uint8_t region) {
    return hardware->Ready(address, bytes, region);
}
extern "C" bool nx_h747_amp_hw_take(uint8_t semaphore, uint8_t process) {
    return hardware->Take(semaphore, process);
}
extern "C" bool nx_h747_amp_hw_release(uint8_t semaphore, uint8_t process) {
    return hardware->Release(semaphore, process);
}
