/**
 * \file            amp_ipc_test.cpp
 * \brief           Production C shared loans at cache and peer-stop boundaries
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/amp_ipc_storage.h"
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
namespace {
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
class Port {
  public:
    MOCK_METHOD(nx_result_t, Claim,
                (const void*, const nx_amp_wire_slot_t*, nx_amp_role_t,
                 uint32_t));
    MOCK_METHOD(void, Unclaim, (const void*));
    MOCK_METHOD(nx_result_t, Clean, (const void*, size_t));
    MOCK_METHOD(nx_result_t, Invalidate, (const void*, size_t));
    MOCK_METHOD(uint32_t, Acquire, (const uint32_t*));
    MOCK_METHOD(void, Release, (uint32_t*, uint32_t));
    MOCK_METHOD(nx_result_t, Stopped, (uint32_t));
    const void* owners[2] = {};
    uint32_t last_epoch[2] = {};
    void defaults() {
        ON_CALL(*this, Claim)
            .WillByDefault(
                Invoke([this](const void* owner, const nx_amp_wire_slot_t*,
                              nx_amp_role_t role, uint32_t local_epoch) {
                    if (owners[role] != nullptr) {
                        return NX_ERROR_BUSY;
                    }
                    if (local_epoch <= last_epoch[role]) {
                        return NX_ERROR_STATE;
                    }
                    owners[role] = owner;
                    last_epoch[role] = local_epoch;
                    return NX_SUCCESS;
                }));
        ON_CALL(*this, Unclaim).WillByDefault(Invoke([this](const void* owner) {
            for (auto& entry : owners) {
                if (entry == owner) {
                    entry = nullptr;
                }
            }
        }));
        ON_CALL(*this, Clean).WillByDefault(Return(NX_SUCCESS));
        ON_CALL(*this, Invalidate).WillByDefault(Return(NX_SUCCESS));
        ON_CALL(*this, Acquire).WillByDefault(Invoke([](const uint32_t* word) {
            return __atomic_load_n(word, __ATOMIC_ACQUIRE);
        }));
        ON_CALL(*this, Release)
            .WillByDefault(Invoke([](uint32_t* word, uint32_t value) {
                __atomic_store_n(word, value, __ATOMIC_RELEASE);
            }));
        ON_CALL(*this, Stopped).WillByDefault(Return(NX_ERROR_BUSY));
    }
};
nx_result_t claim(void* p, const void* o, const nx_amp_wire_slot_t* l,
                  nx_amp_role_t r, uint32_t e) {
    return static_cast<Port*>(p)->Claim(o, l, r, e);
}
void unclaim(void* p, const void* o) {
    static_cast<Port*>(p)->Unclaim(o);
}
nx_result_t clean(void* p, const void* a, size_t n) {
    return static_cast<Port*>(p)->Clean(a, n);
}
nx_result_t invalidate(void* p, const void* a, size_t n) {
    return static_cast<Port*>(p)->Invalidate(a, n);
}
uint32_t acquire(void* p, const uint32_t* w) {
    return static_cast<Port*>(p)->Acquire(w);
}
void release(void* p, uint32_t* w, uint32_t v) {
    static_cast<Port*>(p)->Release(w, v);
}
nx_result_t stopped(void* p, uint32_t e) {
    return static_cast<Port*>(p)->Stopped(e);
}
const nx_amp_port_ops_t ops = {claim,   unclaim, clean,  invalidate,
                               acquire, release, stopped};
class AmpIpc : public ::testing::Test {
  protected:
    NiceMock<Port> port;
    alignas(64) nx_amp_wire_slot_t wire[2] = {};
    alignas(64) nx_amp_word_t sender_boot = {}, receiver_boot = {};
    alignas(64) uint8_t payload[256] = {};
    nx_amp_local_slot_t tx_slots[2] = {}, rx_slots[2] = {};
    nx_amp_ipc_t tx = {}, rx = {};
    nx_amp_config_t tx_config = {}, rx_config = {};
    void SetUp() override {
        port.defaults();
        sender_boot.value = 7;
        receiver_boot.value = 11;
        tx_config = {&ops,
                     &port,
                     wire,
                     tx_slots,
                     2,
                     payload,
                     sizeof(payload),
                     &sender_boot,
                     &receiver_boot,
                     7,
                     11,
                     32,
                     NX_AMP_SENDER};
        rx_config = tx_config;
        rx_config.slots = rx_slots;
        rx_config.local_boot = &receiver_boot;
        rx_config.peer_boot = &sender_boot;
        rx_config.local_epoch = 11;
        rx_config.peer_epoch = 7;
        rx_config.role = NX_AMP_RECEIVER;
        ASSERT_EQ(nx_amp_ipc_construct(&tx, &tx_config), NX_SUCCESS);
        ASSERT_EQ(nx_amp_ipc_construct(&rx, &rx_config), NX_SUCCESS);
    }
    nx_amp_ticket_t send(uint32_t offset = 0) {
        nx_amp_ticket_t ticket = {};
        EXPECT_EQ(nx_amp_ipc_send(&tx, offset, 64, &ticket), NX_SUCCESS);
        return ticket;
    }
    nx_amp_view_t receive() {
        nx_amp_view_t view = {};
        EXPECT_EQ(nx_amp_ipc_receive(&rx, &view), NX_SUCCESS);
        return view;
    }
};
TEST_F(AmpIpc, ExactAckJoinsBorrowThenSlotGetsNewIdentity) {
    payload[0] = 0x5a;
    auto first = send();
    EXPECT_EQ(nx_amp_ipc_poll(first), NX_ERROR_BUSY);
    auto view = receive();
    EXPECT_EQ(view.data, payload);
    EXPECT_EQ(view.bytes, 64U);
    ASSERT_NE(view.data, nullptr);
    EXPECT_EQ(*static_cast<const uint8_t*>(view.data), 0x5a);
    ASSERT_EQ(nx_amp_ipc_finish(view.ticket), NX_SUCCESS);
    ASSERT_EQ(nx_amp_ipc_poll(first), NX_SUCCESS);
    auto next = send(64);
    EXPECT_EQ(next.slot, first.slot);
    EXPECT_GT(next.sequence, first.sequence);
    EXPECT_EQ(nx_amp_ipc_poll(first), NX_ERROR_STATE);
    EXPECT_EQ(nx_amp_ipc_finish(view.ticket), NX_ERROR_STATE);
}
TEST_F(AmpIpc, PayloadAndDescriptorCleanPrecedeReleasePublication) {
    InSequence order;
    EXPECT_CALL(port, Clean(payload, 64)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(port, Clean(&wire[0].offer, 64)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(port, Release(&wire[0].published.value, 1));
    auto ticket = send();
    EXPECT_EQ(ticket.sequence, 1U);
}
TEST_F(AmpIpc, DescriptorAndPayloadInvalidateBeforeBorrowAndAckClean) {
    auto ticket = send();
    EXPECT_CALL(port, Acquire(_)).Times(AnyNumber());
    {
        InSequence order;
        EXPECT_CALL(port, Acquire(&wire[0].published.value));
        EXPECT_CALL(port, Invalidate(&wire[0].offer, 64))
            .WillOnce(Return(NX_SUCCESS));
        EXPECT_CALL(port, Invalidate(payload, 64)).WillOnce(Return(NX_SUCCESS));
        EXPECT_CALL(port, Clean(&wire[0].receipt, 64))
            .WillOnce(Return(NX_SUCCESS));
        EXPECT_CALL(port, Release(&wire[0].acknowledged.value, 1));
        auto view = receive();
        EXPECT_EQ(nx_amp_ipc_finish(view.ticket), NX_SUCCESS);
    }
    ::testing::Mock::VerifyAndClearExpectations(&port);
    EXPECT_EQ(nx_amp_ipc_poll(ticket), NX_SUCCESS);
}
TEST_F(AmpIpc, PeerResetAndStaleAckCannotReleaseAcceptedPayload) {
    auto ticket = send();
    auto view = receive();
    ASSERT_EQ(nx_amp_ipc_finish(view.ticket), NX_SUCCESS);
    port.Release(&receiver_boot.value, 12);
    EXPECT_EQ(nx_amp_ipc_poll(ticket), NX_ERROR_STATE);
    EXPECT_TRUE(tx_slots[ticket.slot].active);
    nx_amp_ticket_t rejected = {};
    EXPECT_EQ(nx_amp_ipc_send(&tx, 64, 64, &rejected), NX_ERROR_STATE);
    nx_amp_close_t closed = {};
    ASSERT_EQ(nx_amp_ipc_close(&tx, &closed), NX_SUCCESS);
    EXPECT_EQ(nx_amp_ipc_join(closed), NX_ERROR_STATE);
    EXPECT_EQ(nx_amp_ipc_reset_join(closed), NX_ERROR_BUSY);
    EXPECT_TRUE(tx_slots[ticket.slot].active);
    EXPECT_EQ(port.owners[NX_AMP_SENDER], &tx);
    EXPECT_CALL(port, Stopped(11)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(port, Unclaim(&tx));
    EXPECT_EQ(nx_amp_ipc_reset_join(closed), NX_SUCCESS);
    EXPECT_FALSE(tx_slots[ticket.slot].active);
    EXPECT_EQ(nx_amp_ipc_poll(ticket), NX_ERROR_STATE);
    EXPECT_EQ(nx_amp_ipc_reset_join(closed), NX_ERROR_STATE);
}
TEST_F(AmpIpc, OldEpochReceiptWithMatchingSequenceDoesNotJoin) {
    auto ticket = send();
    wire[0].receipt = wire[0].offer;
    wire[0].receipt.receiver_epoch = 10;
    port.Release(&wire[0].acknowledged.value, ticket.sequence);
    EXPECT_EQ(nx_amp_ipc_poll(ticket), NX_ERROR_BUSY);
    EXPECT_TRUE(tx_slots[0].active);
}
TEST_F(AmpIpc, UniquePortRoleClaimRejectsSecondOwner) {
    nx_amp_ipc_t other = {};
    nx_amp_local_slot_t slots[2] = {};
    auto config = tx_config;
    config.slots = slots;
    EXPECT_EQ(nx_amp_ipc_construct(&other, &config), NX_ERROR_BUSY);
    EXPECT_FALSE(other.initialized);
    EXPECT_EQ(port.owners[NX_AMP_SENDER], &tx);
    EXPECT_EQ(nx_amp_ipc_construct(&tx, &tx_config), NX_ERROR_STATE);
}
TEST_F(AmpIpc, FullLaneDoesNotPublishOrRetainRejectedSend) {
    auto first = send();
    auto second = send(64);
    nx_amp_ticket_t rejected = {};
    EXPECT_CALL(port, Release(_, _)).Times(0);
    EXPECT_EQ(nx_amp_ipc_send(&tx, 128, 64, &rejected), NX_ERROR_EXHAUSTED);
    EXPECT_EQ(rejected.owner, nullptr);
    EXPECT_TRUE(tx_slots[first.slot].active);
    EXPECT_TRUE(tx_slots[second.slot].active);
}
TEST_F(AmpIpc, OverlappingActivePayloadIsRejectedUntilExactAck) {
    auto ticket = send();
    nx_amp_ticket_t rejected = {};
    EXPECT_EQ(nx_amp_ipc_send(&tx, 32, 64, &rejected), NX_ERROR_BUSY);
    auto view = receive();
    ASSERT_EQ(nx_amp_ipc_finish(view.ticket), NX_SUCCESS);
    ASSERT_EQ(nx_amp_ipc_poll(ticket), NX_SUCCESS);
    EXPECT_EQ(nx_amp_ipc_send(&tx, 32, 64, &rejected), NX_SUCCESS);
}
TEST_F(AmpIpc, FinalSequenceRetiresSlotWithoutIdentityRollover) {
    tx_slots[0].sequence = UINT32_MAX - 1;
    tx_slots[1].sequence = UINT32_MAX;
    rx_slots[0].sequence = UINT32_MAX - 1;
    auto final = send();
    EXPECT_EQ(final.sequence, UINT32_MAX);
    auto view = receive();
    ASSERT_EQ(nx_amp_ipc_finish(view.ticket), NX_SUCCESS);
    ASSERT_EQ(nx_amp_ipc_poll(final), NX_SUCCESS);
    nx_amp_ticket_t rejected = {};
    EXPECT_EQ(nx_amp_ipc_send(&tx, 0, 64, &rejected), NX_ERROR_EXHAUSTED);
    EXPECT_EQ(tx_slots[0].sequence, UINT32_MAX);
}
TEST_F(AmpIpc, PayloadCleanFailureRejectsWithoutPublishingOrBorrow) {
    EXPECT_CALL(port, Clean(payload, 64)).WillOnce(Return(NX_ERROR_IO));
    EXPECT_CALL(port, Release(_, _)).Times(0);
    nx_amp_ticket_t ticket = {};
    EXPECT_EQ(nx_amp_ipc_send(&tx, 0, 64, &ticket), NX_ERROR_IO);
    EXPECT_FALSE(tx_slots[0].active);
    EXPECT_EQ(ticket.owner, nullptr);
}
TEST_F(AmpIpc, DescriptorCleanFailureBurnsIdentityWithoutPublishingBorrow) {
    EXPECT_CALL(port, Clean(_, _)).Times(AnyNumber());
    EXPECT_CALL(port, Clean(&wire[0].offer, 64)).WillOnce(Return(NX_ERROR_IO));
    nx_amp_ticket_t ticket = {};
    EXPECT_EQ(nx_amp_ipc_send(&tx, 0, 64, &ticket), NX_ERROR_IO);
    EXPECT_FALSE(tx_slots[0].active);
    EXPECT_EQ(wire[0].published.value, 0U);
    ::testing::Mock::VerifyAndClearExpectations(&port);
    ticket = send();
    EXPECT_EQ(ticket.sequence, 2U);
}
TEST_F(AmpIpc, PayloadInvalidateFailureReturnsNoViewAndCanRetry) {
    send();
    EXPECT_CALL(port, Invalidate(_, _)).Times(AnyNumber());
    EXPECT_CALL(port, Invalidate(payload, 64)).WillOnce(Return(NX_ERROR_IO));
    nx_amp_view_t view = {};
    EXPECT_EQ(nx_amp_ipc_receive(&rx, &view), NX_ERROR_IO);
    EXPECT_EQ(view.data, nullptr);
    EXPECT_FALSE(rx_slots[0].active);
    ::testing::Mock::VerifyAndClearExpectations(&port);
    EXPECT_EQ(nx_amp_ipc_receive(&rx, &view), NX_SUCCESS);
}
TEST_F(AmpIpc, AckCleanFailureRetainsBothEndpointsUntilRetry) {
    auto ticket = send();
    auto view = receive();
    EXPECT_CALL(port, Clean(&wire[0].receipt, 64))
        .WillOnce(Return(NX_ERROR_IO));
    EXPECT_EQ(nx_amp_ipc_finish(view.ticket), NX_ERROR_IO);
    EXPECT_TRUE(rx_slots[0].active);
    EXPECT_TRUE(rx_slots[0].returned);
    EXPECT_EQ(nx_amp_ipc_poll(ticket), NX_ERROR_BUSY);
    ::testing::Mock::VerifyAndClearExpectations(&port);
    EXPECT_EQ(nx_amp_ipc_finish(view.ticket), NX_SUCCESS);
    EXPECT_EQ(nx_amp_ipc_poll(ticket), NX_SUCCESS);
}
TEST_F(AmpIpc, AckInvalidateFailureRetainsSenderBorrow) {
    auto ticket = send();
    auto view = receive();
    ASSERT_EQ(nx_amp_ipc_finish(view.ticket), NX_SUCCESS);
    EXPECT_CALL(port, Invalidate(&wire[0].receipt, 64))
        .WillOnce(Return(NX_ERROR_IO));
    EXPECT_EQ(nx_amp_ipc_poll(ticket), NX_ERROR_IO);
    EXPECT_TRUE(tx_slots[0].active);
    ::testing::Mock::VerifyAndClearExpectations(&port);
    EXPECT_EQ(nx_amp_ipc_poll(ticket), NX_SUCCESS);
}
TEST_F(AmpIpc, CloseJoinKeepsReceiverUntilCallerFinishes) {
    auto ticket = send();
    auto view = receive();
    nx_amp_close_t tx_close = {}, rx_close = {};
    ASSERT_EQ(nx_amp_ipc_close(&tx, &tx_close), NX_SUCCESS);
    ASSERT_EQ(nx_amp_ipc_close(&rx, &rx_close), NX_SUCCESS);
    EXPECT_EQ(nx_amp_ipc_join(tx_close), NX_ERROR_BUSY);
    EXPECT_EQ(nx_amp_ipc_join(rx_close), NX_ERROR_BUSY);
    EXPECT_EQ(nx_amp_ipc_reset_join(rx_close), NX_ERROR_BUSY);
    ASSERT_EQ(nx_amp_ipc_finish(view.ticket), NX_SUCCESS);
    EXPECT_EQ(nx_amp_ipc_join(rx_close), NX_SUCCESS);
    EXPECT_EQ(nx_amp_ipc_join(tx_close), NX_SUCCESS);
    EXPECT_EQ(nx_amp_ipc_poll(ticket), NX_ERROR_STATE);
    EXPECT_EQ(nx_amp_ipc_join(tx_close), NX_ERROR_STATE);
    EXPECT_EQ(nx_amp_ipc_construct(&tx, &tx_config), NX_ERROR_STATE);
}
TEST_F(AmpIpc, ReceiverResetJoinNeedsActualLocalReaderJoinAndPeerProof) {
    send();
    auto view = receive();
    port.Release(&sender_boot.value, 8);
    nx_amp_close_t closed = {};
    ASSERT_EQ(nx_amp_ipc_close(&rx, &closed), NX_SUCCESS);
    EXPECT_CALL(port, Stopped(_)).Times(0);
    EXPECT_EQ(nx_amp_ipc_reset_join(closed), NX_ERROR_BUSY);
    EXPECT_EQ(nx_amp_ipc_finish(view.ticket), NX_ERROR_STATE);
    EXPECT_TRUE(rx_slots[0].active);
    EXPECT_TRUE(rx_slots[0].returned);
    ::testing::Mock::VerifyAndClearExpectations(&port);
    EXPECT_CALL(port, Stopped(7)).WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(nx_amp_ipc_reset_join(closed), NX_SUCCESS);
}
TEST_F(AmpIpc, ForeignTicketAndCloseControlsCannotChangeInstance) {
    auto ticket = send();
    auto foreign = ticket;
    foreign.owner = &rx;
    EXPECT_EQ(nx_amp_ipc_poll(foreign), NX_ERROR_STATE);
    auto view = receive();
    foreign = view.ticket;
    foreign.owner = &tx;
    EXPECT_EQ(nx_amp_ipc_finish(foreign), NX_ERROR_STATE);
    nx_amp_close_t closed = {};
    ASSERT_EQ(nx_amp_ipc_close(&tx, &closed), NX_SUCCESS);
    ++closed.peer_epoch;
    EXPECT_EQ(nx_amp_ipc_reset_join(closed), NX_ERROR_STATE);
    EXPECT_TRUE(tx_slots[0].active);
}
TEST_F(AmpIpc, MalformedOfferAndWrongBootAreNeverBorrowed) {
    send();
    wire[0].offer.offset = UINT32_MAX;
    nx_amp_view_t view = {};
    EXPECT_EQ(nx_amp_ipc_receive(&rx, &view), NX_ERROR_INVALID);
    EXPECT_EQ(view.data, nullptr);
    EXPECT_FALSE(rx_slots[0].active);
    wire[0].offer.offset = 0;
    wire[0].offer.sender_epoch = 6;
    EXPECT_EQ(nx_amp_ipc_receive(&rx, &view), NX_ERROR_EMPTY);
    EXPECT_FALSE(rx_slots[0].active);
}
TEST_F(AmpIpc, OutputAliasesAndInvalidSpanHaveNoPublication) {
    EXPECT_CALL(port, Release(_, _)).Times(0);
    nx_amp_ticket_t ticket = {};
    EXPECT_EQ(nx_amp_ipc_send(&tx, UINT32_MAX, 32, &ticket), NX_ERROR_INVALID);
    EXPECT_EQ(nx_amp_ipc_send(&tx, 0, 0, &ticket), NX_ERROR_INVALID);
    EXPECT_EQ(nx_amp_ipc_send(&tx, 1, 32, &ticket), NX_ERROR_INVALID);
    EXPECT_EQ(nx_amp_ipc_send(&tx, 0, 32,
                              reinterpret_cast<nx_amp_ticket_t*>(payload)),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_amp_ipc_close(&tx, reinterpret_cast<nx_amp_close_t*>(&tx)),
              NX_ERROR_INVALID);
    EXPECT_FALSE(tx.closed);
}
TEST_F(AmpIpc, ConfigurationOverlapAndUnalignedCacheSpansRejectBeforeClaim) {
    nx_amp_ipc_t other = {};
    nx_amp_local_slot_t slots[2] = {};
    auto config = tx_config;
    config.slots = slots;
    config.payload = &wire[0];
    EXPECT_CALL(port, Claim(_, _, _, _)).Times(0);
    EXPECT_EQ(nx_amp_ipc_construct(&other, &config), NX_ERROR_INVALID);
    config.payload = payload + 1;
    EXPECT_EQ(nx_amp_ipc_construct(&other, &config), NX_ERROR_INVALID);
    config.payload = payload;
    config.granule = 128;
    EXPECT_EQ(nx_amp_ipc_construct(&other, &config), NX_ERROR_INVALID);
    config.granule = 32;
    config.local_epoch = 0;
    EXPECT_EQ(nx_amp_ipc_construct(&other, &config), NX_ERROR_INVALID);
}
TEST_F(AmpIpc, IndependentLaneDoesNotShareOwnershipSlotsOrControl) {
    NiceMock<Port> other_port;
    other_port.defaults();
    alignas(64) nx_amp_wire_slot_t other_wire[1] = {};
    alignas(64) nx_amp_word_t local = {}, peer = {};
    alignas(64) uint8_t other_payload[64] = {};
    nx_amp_local_slot_t local_slot[1] = {};
    nx_amp_ipc_t other = {};
    local.value = 19;
    peer.value = 23;
    auto config = tx_config;
    config.context = &other_port;
    config.wire = other_wire;
    config.slots = local_slot;
    config.capacity = 1;
    config.local_boot = &local;
    config.peer_boot = &peer;
    config.local_epoch = 19;
    config.peer_epoch = 23;
    config.payload = other_payload;
    config.payload_bytes = sizeof(other_payload);
    ASSERT_EQ(nx_amp_ipc_construct(&other, &config), NX_SUCCESS);
    auto original = send();
    nx_amp_ticket_t independent = {};
    ASSERT_EQ(nx_amp_ipc_send(&other, 0, 64, &independent), NX_SUCCESS);
    auto wrong = original;
    wrong.owner = &other;
    EXPECT_EQ(nx_amp_ipc_poll(wrong), NX_ERROR_STATE);
    EXPECT_TRUE(local_slot[0].active);
    EXPECT_TRUE(tx_slots[0].active);
    EXPECT_EQ(other_wire[0].offer.sender_epoch, 19U);
    EXPECT_EQ(wire[0].offer.sender_epoch, 7U);
}
TEST_F(AmpIpc, RetainedOldBootOfferCannotStarveFreshLaterSlot) {
    tx_slots[0].sequence = UINT32_MAX;
    wire[0].offer = {6, 10, 99, 0, 64, {}};
    port.Release(&wire[0].published.value, 99);
    auto current = send(64);
    ASSERT_EQ(current.slot, 1U);
    auto view = receive();
    EXPECT_EQ(view.ticket.slot, 1U);
    EXPECT_EQ(view.data, payload + 64);
    EXPECT_EQ(nx_amp_ipc_finish(view.ticket), NX_SUCCESS);
    EXPECT_EQ(nx_amp_ipc_poll(current), NX_SUCCESS);
}
TEST_F(AmpIpc, ResetDuringAckInvalidateStillRetainsBorrow) {
    auto ticket = send();
    auto view = receive();
    ASSERT_EQ(nx_amp_ipc_finish(view.ticket), NX_SUCCESS);
    EXPECT_CALL(port, Invalidate(&wire[0].receipt, 64))
        .WillOnce(Invoke([this](const void*, size_t) {
            port.Release(&receiver_boot.value, 12);
            return NX_SUCCESS;
        }));
    EXPECT_EQ(nx_amp_ipc_poll(ticket), NX_ERROR_STATE);
    EXPECT_TRUE(tx_slots[0].active);
    port.Release(&receiver_boot.value, 11);
    EXPECT_EQ(nx_amp_ipc_poll(ticket), NX_ERROR_STATE);
    EXPECT_TRUE(tx.faulted);
}
TEST_F(AmpIpc, StopProofTimeoutNeverRevokesPayloadBorrow) {
    auto ticket = send();
    nx_amp_close_t closed = {};
    ASSERT_EQ(nx_amp_ipc_close(&tx, &closed), NX_SUCCESS);
    EXPECT_CALL(port, Stopped(11)).WillOnce(Return(NX_ERROR_TIMEOUT));
    EXPECT_CALL(port, Unclaim(_)).Times(0);
    EXPECT_EQ(nx_amp_ipc_reset_join(closed), NX_ERROR_TIMEOUT);
    EXPECT_TRUE(tx_slots[ticket.slot].active);
    EXPECT_FALSE(tx.joined);
}
TEST_F(AmpIpc, ReplacementCannotReuseOrdinarilyJoinedBootIdentity) {
    nx_amp_close_t closed = {};
    ASSERT_EQ(nx_amp_ipc_close(&tx, &closed), NX_SUCCESS);
    ASSERT_EQ(nx_amp_ipc_join(closed), NX_SUCCESS);
    nx_amp_ipc_t replacement = {};
    nx_amp_local_slot_t replacement_slots[2] = {};
    auto config = tx_config;
    config.slots = replacement_slots;
    EXPECT_EQ(nx_amp_ipc_construct(&replacement, &config), NX_ERROR_STATE);
    EXPECT_FALSE(replacement.initialized);
    config.local_epoch = 8;
    port.Release(&sender_boot.value, 8);
    EXPECT_EQ(nx_amp_ipc_construct(&replacement, &config), NX_SUCCESS);
    EXPECT_EQ(nx_amp_ipc_join(closed), NX_ERROR_STATE);
}
TEST_F(AmpIpc, TypedStorageAndOutputsRejectMisalignmentBeforeAccess) {
    alignas(64) uint8_t output[sizeof(nx_amp_ticket_t) + 1] = {};
    auto* unaligned = reinterpret_cast<nx_amp_ticket_t*>(output + 1);
    EXPECT_EQ(nx_amp_ipc_send(&tx, 0, 64, unaligned), NX_ERROR_INVALID);
    nx_amp_ipc_t other = {};
    alignas(64) uint8_t slots[sizeof(tx_slots) + 1] = {};
    auto config = tx_config;
    config.slots = reinterpret_cast<nx_amp_local_slot_t*>(slots + 1);
    EXPECT_EQ(nx_amp_ipc_construct(&other, &config), NX_ERROR_INVALID);
    config.slots = tx_slots;
    alignas(64) uint8_t packed_config[sizeof(config) + 1] = {};
    std::memcpy(packed_config + 1, &config, sizeof(config));
    EXPECT_EQ(nx_amp_ipc_construct(
                  &other,
                  reinterpret_cast<const nx_amp_config_t*>(packed_config + 1)),
              NX_ERROR_INVALID);
    alignas(64) uint8_t packed_ipc[sizeof(other) + 1] = {};
    EXPECT_EQ(nx_amp_ipc_construct(
                  reinterpret_cast<nx_amp_ipc_t*>(packed_ipc + 1), &config),
              NX_ERROR_INVALID);
}
TEST_F(AmpIpc, EveryOutputRejectsSharedAndLocalBorrowedStorage) {
    auto ticket = send();
    EXPECT_EQ(
        nx_amp_ipc_receive(&rx, reinterpret_cast<nx_amp_view_t*>(payload + 64)),
        NX_ERROR_INVALID);
    EXPECT_EQ(
        nx_amp_ipc_receive(&rx, reinterpret_cast<nx_amp_view_t*>(rx_slots)),
        NX_ERROR_INVALID);
    EXPECT_EQ(nx_amp_ipc_close(&tx, reinterpret_cast<nx_amp_close_t*>(payload)),
              NX_ERROR_INVALID);
    EXPECT_EQ(
        nx_amp_ipc_close(&tx, reinterpret_cast<nx_amp_close_t*>(tx_slots)),
        NX_ERROR_INVALID);
    EXPECT_EQ(
        nx_amp_ipc_send(&tx, 64, 64, reinterpret_cast<nx_amp_ticket_t*>(wire)),
        NX_ERROR_INVALID);
    EXPECT_TRUE(tx_slots[ticket.slot].active);
    EXPECT_FALSE(tx.closed);
}
} /* namespace */
