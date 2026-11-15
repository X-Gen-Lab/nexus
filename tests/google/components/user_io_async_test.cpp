/**
 * \file            user_io_async_test.cpp
 * \brief           Copied mediation with real owner epochs and drain
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/arch/arch.h"
#include "nexus/components/user_io_async_storage.h"
}
#include <cstring>
#include <functional>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
using testing::_;
using testing::NiceMock;
using testing::Return;
struct Kernel {
    MOCK_METHOD(nx_result_t, claim,
                (uint32_t, uint32_t, uint64_t, nx_bus_owner_t*,
                 const nx_user_io_async_t*));
    MOCK_METHOD(nx_result_t, release, (const nx_user_io_async_t*, uint64_t));
    MOCK_METHOD(nx_result_t, drained, (const nx_user_io_async_t*, uint64_t));
    MOCK_METHOD(nx_result_t, identity, (uint32_t*));
    MOCK_METHOD(nx_result_t, copy_in, (uint32_t, void*, const void*, size_t));
    MOCK_METHOD(nx_result_t, copy_out, (uint32_t, void*, const void*, size_t));
    uint32_t principal = 7U;
    uint64_t history = 0U;
    nx_bus_owner_t* claimed_owner = nullptr;
    const nx_user_io_async_t* claimed_role = nullptr;
    bool outer_joined = true;
    nx_result_t claim_role(uint64_t session, nx_bus_owner_t* owner,
                           const nx_user_io_async_t* role) {
        if (claimed_role != nullptr) {
            return NX_ERROR_BUSY;
        }
        if (history == UINT64_MAX) {
            return NX_ERROR_EXHAUSTED;
        }
        if (session == 0U || session <= history) {
            return NX_ERROR_STATE;
        }
        history = session;
        claimed_owner = owner;
        claimed_role = role;
        return NX_SUCCESS;
    }
    Kernel() {
        ON_CALL(*this, claim(_, _, _, _, _))
            .WillByDefault([this](uint32_t, uint32_t, uint64_t session,
                                  nx_bus_owner_t* owner,
                                  const nx_user_io_async_t* role) {
                return claim_role(session, owner, role);
            });
        ON_CALL(*this, release(_, _))
            .WillByDefault(
                [this](const nx_user_io_async_t* role, uint64_t session) {
                    if (claimed_role != role || history != session) {
                        return NX_ERROR_STATE;
                    }
                    if (!outer_joined) {
                        return NX_ERROR_BUSY;
                    }
                    claimed_owner = nullptr;
                    claimed_role = nullptr;
                    return NX_SUCCESS;
                });
        ON_CALL(*this, drained(_, _))
            .WillByDefault(
                [this](const nx_user_io_async_t* role, uint64_t session) {
                    if (claimed_role != role || history != session) {
                        return NX_ERROR_STATE;
                    }
                    return outer_joined ? NX_SUCCESS : NX_ERROR_BUSY;
                });
        ON_CALL(*this, identity(_)).WillByDefault([this](uint32_t* out) {
            *out = principal;
            return NX_SUCCESS;
        });
        ON_CALL(*this, copy_in(_, _, _, _))
            .WillByDefault([](uint32_t, void* out, const void* in, size_t n) {
                std::memcpy(out, in, n);
                return NX_SUCCESS;
            });
        ON_CALL(*this, copy_out(_, _, _, _))
            .WillByDefault([](uint32_t, void* out, const void* in, size_t n) {
                std::memcpy(out, in, n);
                return NX_SUCCESS;
            });
    }
};
struct Hardware {
    MOCK_METHOD(nx_result_t, submit, (nx_uart_tx_request_t*));
    MOCK_METHOD(nx_result_t, cancel, (nx_uart_tx_request_t*));
    MOCK_METHOD(void, service, ());
    nx_uart_tx_request_t* active = nullptr;
    bool release = false;
    bool quarantine = false;
    bool cancelled = false;
    nx_result_t terminal = NX_SUCCESS;
    uint8_t wire[16] = {};
    Hardware() {
        ON_CALL(*this, submit(_))
            .WillByDefault([this](nx_uart_tx_request_t* request) {
                nx_result_t result =
                    nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
                if (result == NX_SUCCESS) {
                    active = request;
                }
                return result;
            });
        ON_CALL(*this, cancel(_))
            .WillByDefault([this](nx_uart_tx_request_t* request) {
                if (request != active) {
                    return NX_ERROR_STATE;
                }
                cancelled = true;
                return NX_SUCCESS;
            });
        ON_CALL(*this, service()).WillByDefault([this] {
            if (active == nullptr) {
                return;
            }
            if (release) {
                nx_uart_tx_request_t* request = active;
                active = nullptr;
                const size_t count =
                    terminal == NX_SUCCESS ? request->length : 0U;
                if (count <= sizeof(wire)) {
                    std::memcpy(wire, request->data, count);
                }
                nx_request_settle(&request->base, terminal, count);
            } else if (quarantine) {
                (void)nx_request_transition(&active->base,
                                            NX_REQUEST_QUARANTINED);
            }
        });
    }
};
struct Guard {
    std::function<void()> after_leave;
};
struct Clock {
    nx_time_us_t now = 1U;
};
uintptr_t enter(void*) {
    return nx_arch_irq_save().value;
}
void leave(void* context, uintptr_t value) {
    nx_arch_irq_restore({static_cast<uint32_t>(value)});
    auto& guard = *static_cast<Guard*>(context);
    const auto observation = guard.after_leave;
    if (observation) {
        observation();
    }
}
nx_time_us_t read_clock(void* context) {
    return static_cast<Clock*>(context)->now;
}
nx_result_t identify(void* context, uint32_t* principal) {
    return static_cast<Kernel*>(context)->identity(principal);
}
nx_result_t copy_in(void* context, uint32_t principal, void* destination,
                    const void* source, size_t bytes) {
    return static_cast<Kernel*>(context)->copy_in(principal, destination,
                                                  source, bytes);
}
nx_result_t copy_out(void* context, uint32_t principal, void* destination,
                     const void* source, size_t bytes) {
    return static_cast<Kernel*>(context)->copy_out(principal, destination,
                                                   source, bytes);
}
nx_result_t uart_submit(void* context, nx_uart_tx_request_t* request) {
    return static_cast<Hardware*>(context)->submit(request);
}
nx_result_t uart_cancel(void* context, nx_uart_tx_request_t* request) {
    return static_cast<Hardware*>(context)->cancel(request);
}
void uart_service(void* context) {
    static_cast<Hardware*>(context)->service();
}
nx_result_t claim(void* context, uint32_t principal, uint32_t device,
                  uint64_t session, nx_bus_owner_t* owner,
                  const nx_user_io_async_t* role) {
    return static_cast<Kernel*>(context)->claim(principal, device, session,
                                                owner, role);
}
nx_result_t release(void* context, const nx_user_io_async_t* role,
                    uint64_t session) {
    return static_cast<Kernel*>(context)->release(role, session);
}
nx_result_t drained(void* context, const nx_user_io_async_t* role,
                    uint64_t session) {
    return static_cast<Kernel*>(context)->drained(role, session);
}
const nx_user_io_copy_ops_t copy_ops = {claim,    release, drained,
                                        identify, copy_in, copy_out};
nx_uart_ops_t uart_operations() {
    nx_uart_ops_t ops = {};
    ops.submit = uart_submit;
    ops.cancel = uart_cancel;
    ops.service = uart_service;
    return ops;
}
const nx_uart_ops_t uart_ops = uart_operations();

class UserAsync : public testing::Test {
  protected:
    NiceMock<Kernel> kernel;
    NiceMock<Hardware> hardware;
    Guard guard;
    Clock clock;
    const nx_uart_port_t uart = {&uart_ops, &hardware};
    nx_uart_owner_executor_t executor = {};
    nx_bus_owner_t owner = {};
    nx_owner_slot_t owner_slots[2] = {};
    nx_user_io_async_t mediation = {};
    uint8_t copied[2][16] = {};
    nx_user_io_async_slot_t slots[2] = {};
    struct {
        nx_user_io_async_message_t message;
        uint8_t payload[32];
        nx_user_io_async_reply_t reply;
    } user = {};
    nx_user_io_copy_region_t region = {};
    nx_user_io_async_config_t config = {};
    void SetUp() override {
        const auto execution = nx_uart_owner_executor_port(&executor, &uart);
        ASSERT_EQ(nx_bus_owner_init(&owner, owner_slots, 2U,
                                    {&guard, enter, leave}, execution,
                                    {read_clock, &clock}, nullptr),
                  NX_SUCCESS);
        for (size_t i = 0U; i < 2U; ++i) {
            slots[i].data = copied[i];
            slots[i].capacity = sizeof(copied[i]);
        }
        region = {reinterpret_cast<uintptr_t>(&user), sizeof(user),
                  NX_USER_IO_COPY_READ | NX_USER_IO_COPY_WRITE};
        config.principal = 7U;
        config.device = 9U;
        config.session = 1U;
        config.owner = &owner;
        config.executor = &executor;
        config.port = &uart;
        config.port_context_bytes = sizeof(hardware);
        config.guard_context_bytes = sizeof(guard);
        config.clock_context_bytes = sizeof(clock);
        config.copy = {&copy_ops, &kernel, sizeof(kernel)};
        config.regions = &region;
        config.region_count = 1U;
    }
    nx_result_t construct() {
        return nx_user_io_async_construct(&mediation, slots, 2U, &config);
    }
    void command(uint32_t action, uint32_t slot = 0U, uint64_t epoch = 1U) {
        user.message = {};
        user.message.abi = NX_USER_IO_ASYNC_ABI;
        user.message.action = action;
        user.message.device = 9U;
        user.message.slot = slot;
        user.message.session_low = 1U;
        user.message.epoch_low = static_cast<uint32_t>(epoch);
        user.message.epoch_high = static_cast<uint32_t>(epoch >> 32U);
        if (action == NX_USER_IO_ASYNC_SUBMIT) {
            user.message.length = 4U;
            user.message.deadline_low = UINT32_MAX;
            user.message.deadline_high = UINT32_MAX;
            user.payload[0] = 0x11U;
            user.payload[1] = 0x22U;
            user.payload[2] = 0x33U;
            user.payload[3] = 0x44U;
        }
    }
    nx_result_t dispatch() {
        return nx_user_io_async_dispatch(
            &mediation, &user.message,
            user.message.action == NX_USER_IO_ASYNC_SUBMIT ? user.payload
                                                           : nullptr,
            &user.reply);
    }
    void submit() {
        command(NX_USER_IO_ASYNC_SUBMIT);
        ASSERT_EQ(dispatch(), NX_SUCCESS);
    }
    void finish(nx_result_t result = NX_SUCCESS, bool expected_idle = true) {
        if (hardware.active == nullptr) {
            ASSERT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
        }
        hardware.terminal = result;
        hardware.release = true;
        ASSERT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
        ASSERT_EQ(nx_bus_owner_idle(&owner), expected_idle);
    }
};

TEST_F(UserAsync, CopiesCompletePayloadBeforeExistingOwnerCanBorrowIt) {
    ASSERT_EQ(construct(), NX_SUCCESS);
    submit();
    EXPECT_EQ(nx_request_state(&slots[0].request), NX_REQUEST_QUEUED);
    EXPECT_EQ(user.reply.accepted, 1U);
    EXPECT_EQ(user.reply.epoch_low, 1U);
    std::memset(user.payload, 0xE1, sizeof(user.payload));
    EXPECT_EQ(copied[0][0], 0x11U);
    ASSERT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
    ASSERT_NE(hardware.active, nullptr);
    EXPECT_EQ(hardware.active->data, copied[0]);
    EXPECT_NE(hardware.active->data, user.payload);
    finish();
    EXPECT_EQ(hardware.wire[0], 0x11U);
    command(NX_USER_IO_ASYNC_POLL);
    ASSERT_EQ(dispatch(), NX_SUCCESS);
    EXPECT_EQ(user.reply.state, NX_REQUEST_SETTLED);
    EXPECT_EQ(user.reply.result, NX_SUCCESS);
    EXPECT_EQ(user.reply.transferred, 4U);
}

TEST_F(UserAsync, AuthenticatesImmutablePrincipalOnEveryCallBeforeCopy) {
    ASSERT_EQ(construct(), NX_SUCCESS);
    EXPECT_CALL(kernel, identity(_)).Times(2);
    submit();
    kernel.principal = 8U;
    command(NX_USER_IO_ASYNC_CANCEL);
    EXPECT_CALL(kernel, copy_in(_, _, _, _)).Times(0);
    EXPECT_EQ(dispatch(), NX_ERROR_PERMISSION);
    EXPECT_FALSE(owner_slots[0].cancel_requested);
    testing::Mock::VerifyAndClearExpectations(&kernel);
    kernel.principal = 7U;
    EXPECT_CALL(kernel, identity(_)).Times(1);
    command(NX_USER_IO_ASYNC_POLL);
    EXPECT_EQ(dispatch(), NX_SUCCESS);
    finish();
}

TEST_F(UserAsync, RejectsForgedDeviceSlotEpochAndReservedWordsWithoutEffects) {
    ASSERT_EQ(construct(), NX_SUCCESS);
    command(NX_USER_IO_ASYNC_SUBMIT);
    user.message.device = 10U;
    EXPECT_EQ(dispatch(), NX_ERROR_PERMISSION);
    user.message.device = 9U;
    user.message.slot = 2U;
    EXPECT_EQ(dispatch(), NX_ERROR_INVALID);
    user.message.slot = 0U;
    user.message.epoch_low = 2U;
    EXPECT_EQ(dispatch(), NX_ERROR_STATE);
    user.message.epoch_low = 1U;
    user.message.reserved = 1U;
    EXPECT_EQ(dispatch(), NX_ERROR_INVALID);
    EXPECT_EQ(slots[0].identity.epoch, 0U);
    EXPECT_EQ(slots[0].identity.request, nullptr);
    EXPECT_TRUE(nx_bus_owner_idle(&owner));
}

TEST_F(UserAsync, RejectsOversizedOrEmptyPayloadBeforeCopyOrEpochBinding) {
    ASSERT_EQ(construct(), NX_SUCCESS);
    command(NX_USER_IO_ASYNC_SUBMIT);
    user.message.length = 17U;
    EXPECT_EQ(dispatch(), NX_ERROR_INVALID);
    user.message.length = 0U;
    EXPECT_EQ(dispatch(), NX_ERROR_INVALID);
    EXPECT_EQ(slots[0].identity.epoch, 0U);
    EXPECT_TRUE(nx_bus_owner_idle(&owner));
}

TEST_F(UserAsync, CopyFailureWipesPartialPayloadWithoutAdmissionOrEpochBurn) {
    ASSERT_EQ(construct(), NX_SUCCESS);
    command(NX_USER_IO_ASYNC_SUBMIT);
    EXPECT_CALL(kernel, copy_in(_, _, _, _)).Times(testing::AnyNumber());
    EXPECT_CALL(kernel, copy_in(7U, copied[0], user.payload, 4U))
        .WillOnce([](uint32_t, void* out, const void* in, size_t) {
            std::memcpy(out, in, 2U);
            return NX_ERROR_IO;
        });
    EXPECT_EQ(dispatch(), NX_ERROR_IO);
    for (uint8_t byte : copied[0]) {
        EXPECT_EQ(byte, 0U);
    }
    EXPECT_EQ(slots[0].identity.epoch, 0U);
    EXPECT_EQ(slots[0].identity.request, nullptr);
    EXPECT_TRUE(nx_bus_owner_idle(&owner));
}

TEST_F(UserAsync, FailedSubmitReplyCopyIsRecoverableWithoutDuplicateExecution) {
    ASSERT_EQ(construct(), NX_SUCCESS);
    command(NX_USER_IO_ASYNC_SUBMIT);
    EXPECT_CALL(kernel, copy_out(_, _, _, _)).Times(testing::AnyNumber());
    EXPECT_CALL(kernel, copy_out(7U, &user.reply, _, sizeof(user.reply)))
        .WillOnce(Return(NX_ERROR_IO))
        .RetiresOnSaturation();
    EXPECT_EQ(dispatch(), NX_ERROR_IO);
    EXPECT_EQ(slots[0].identity.epoch, 1U);
    EXPECT_EQ(slots[0].identity.request, &slots[0].request);
    EXPECT_EQ(dispatch(), NX_ERROR_BUSY);
    command(NX_USER_IO_ASYNC_POLL);
    EXPECT_EQ(dispatch(), NX_SUCCESS);
    EXPECT_EQ(user.reply.epoch_low, 1U);
    EXPECT_EQ(user.reply.state, NX_REQUEST_QUEUED);
    finish();
}

TEST_F(UserAsync, ExistingOwnerRejectionIsRetainedAsRecoverableTerminalResult) {
    ASSERT_EQ(construct(), NX_SUCCESS);
    nx_bus_owner_stop(&owner);
    submit();
    EXPECT_EQ(user.reply.state, NX_REQUEST_SETTLED);
    EXPECT_EQ(user.reply.result, NX_ERROR_STATE);
    EXPECT_EQ(user.reply.accepted, 1U);
    EXPECT_EQ(slots[0].identity.epoch, 1U);
    command(NX_USER_IO_ASYNC_CONSUME);
    EXPECT_EQ(dispatch(), NX_SUCCESS);
    EXPECT_EQ(slots[0].identity.request, nullptr);
}

TEST_F(UserAsync, QueuedCancelSettlesThroughRealOwnerWithoutProviderStart) {
    ASSERT_EQ(construct(), NX_SUCCESS);
    submit();
    command(NX_USER_IO_ASYNC_CANCEL);
    EXPECT_EQ(dispatch(), NX_SUCCESS);
    EXPECT_CALL(hardware, submit(_)).Times(0);
    EXPECT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
    command(NX_USER_IO_ASYNC_POLL);
    ASSERT_EQ(dispatch(), NX_SUCCESS);
    EXPECT_EQ(user.reply.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(user.reply.transferred, 0U);
}

TEST_F(UserAsync, FailedActualDrainRetainsCopiedBytesAndRejectsConsume) {
    ASSERT_EQ(construct(), NX_SUCCESS);
    submit();
    ASSERT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
    command(NX_USER_IO_ASYNC_CANCEL);
    ASSERT_EQ(dispatch(), NX_SUCCESS);
    hardware.quarantine = true;
    EXPECT_EQ(nx_bus_owner_service(&owner), NX_ERROR_IO);
    EXPECT_TRUE(hardware.cancelled);
    EXPECT_EQ(nx_request_state(&slots[0].request), NX_REQUEST_QUARANTINED);
    command(NX_USER_IO_ASYNC_CONSUME);
    EXPECT_EQ(dispatch(), NX_ERROR_BUSY);
    EXPECT_EQ(copied[0][0], 0x11U);
    EXPECT_NE(slots[0].identity.request, nullptr);
    hardware.quarantine = false;
    finish(NX_ERROR_CANCELLED);
    EXPECT_EQ(dispatch(), NX_SUCCESS);
    EXPECT_EQ(slots[0].identity.request, nullptr);
    for (uint8_t byte : copied[0]) {
        EXPECT_EQ(byte, 0U);
    }
}

TEST_F(UserAsync, ConsumeWaitsForRealOwnerPublicationJoinAfterSettled) {
    ASSERT_EQ(construct(), NX_SUCCESS);
    submit();
    ASSERT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
    nx_result_t inside_publication = NX_SUCCESS;
    bool observed = false;
    guard.after_leave = [&] {
        if (nx_request_state(&slots[0].request) == NX_REQUEST_SETTLED &&
            owner.servicing) {
            guard.after_leave = nullptr;
            observed = true;
            command(NX_USER_IO_ASYNC_CONSUME);
            inside_publication = dispatch();
            EXPECT_NE(slots[0].identity.request, nullptr);
            EXPECT_EQ(copied[0][0], 0x11U);
        }
    };
    finish();
    EXPECT_TRUE(observed);
    EXPECT_EQ(inside_publication, NX_ERROR_BUSY);
    EXPECT_EQ(dispatch(), NX_SUCCESS);
    EXPECT_EQ(slots[0].identity.request, nullptr);
}

TEST_F(UserAsync, FailedConsumeReplyCopyNeverReleasesOrReplaysTheCell) {
    ASSERT_EQ(construct(), NX_SUCCESS);
    submit();
    finish();
    command(NX_USER_IO_ASYNC_CONSUME);
    EXPECT_CALL(kernel, copy_out(_, _, _, _)).Times(testing::AnyNumber());
    EXPECT_CALL(kernel, copy_out(7U, &user.reply, _, sizeof(user.reply)))
        .WillOnce(Return(NX_ERROR_IO))
        .RetiresOnSaturation();
    EXPECT_EQ(dispatch(), NX_ERROR_IO);
    EXPECT_NE(slots[0].identity.request, nullptr);
    EXPECT_EQ(copied[0][0], 0x11U);
    EXPECT_EQ(dispatch(), NX_SUCCESS);
    EXPECT_EQ(slots[0].identity.request, nullptr);
}

TEST_F(UserAsync, ConsumedEpochCannotPollCancelOrConsumeTheNextIncarnation) {
    ASSERT_EQ(construct(), NX_SUCCESS);
    submit();
    finish();
    command(NX_USER_IO_ASYNC_CONSUME);
    ASSERT_EQ(dispatch(), NX_SUCCESS);
    command(NX_USER_IO_ASYNC_SUBMIT, 0U, 2U);
    ASSERT_EQ(dispatch(), NX_SUCCESS);
    for (uint32_t action : {NX_USER_IO_ASYNC_POLL, NX_USER_IO_ASYNC_CANCEL,
                            NX_USER_IO_ASYNC_CONSUME}) {
        command(action, 0U, 1U);
        EXPECT_EQ(dispatch(), NX_ERROR_STATE);
    }
    EXPECT_EQ(slots[0].identity.epoch, 2U);
    finish();
}

TEST_F(UserAsync, EpochExhaustionNeverWrapsOrAllowsReinitialization) {
    ASSERT_EQ(construct(), NX_SUCCESS);
    slots[0].identity.epoch = UINT64_MAX;
    command(NX_USER_IO_ASYNC_SUBMIT, 0U, 0U);
    EXPECT_EQ(dispatch(), NX_ERROR_EXHAUSTED);
    EXPECT_EQ(slots[0].identity.epoch, UINT64_MAX);
    EXPECT_EQ(nx_user_io_async_close(&mediation), NX_SUCCESS);
    EXPECT_EQ(nx_user_io_async_join(&mediation), NX_SUCCESS);
    EXPECT_EQ(construct(), NX_ERROR_STATE);
    EXPECT_EQ(slots[0].identity.epoch, UINT64_MAX);
}

TEST_F(UserAsync, CloseCancelsWithoutWipingAndCleanupStillAuthenticates) {
    ASSERT_EQ(construct(), NX_SUCCESS);
    submit();
    EXPECT_EQ(nx_user_io_async_close(&mediation), NX_SUCCESS);
    EXPECT_EQ(nx_user_io_async_join(&mediation), NX_ERROR_BUSY);
    EXPECT_EQ(copied[0][0], 0x11U);
    EXPECT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
    command(NX_USER_IO_ASYNC_CONSUME);
    EXPECT_EQ(dispatch(), NX_SUCCESS);
    EXPECT_EQ(nx_user_io_async_join(&mediation), NX_SUCCESS);
    command(NX_USER_IO_ASYNC_SUBMIT, 0U, 2U);
    EXPECT_EQ(dispatch(), NX_ERROR_STATE);
    EXPECT_EQ(construct(), NX_ERROR_STATE);
}

TEST_F(UserAsync, OwnerGuardCallbackCannotRecursivelyCloseTheSameBinding) {
    ASSERT_EQ(construct(), NX_SUCCESS);
    submit();
    nx_result_t nested = NX_SUCCESS;
    guard.after_leave = [&] {
        guard.after_leave = nullptr;
        nested = nx_user_io_async_close(&mediation);
        EXPECT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
        EXPECT_EQ(nx_request_state(&slots[0].request), NX_REQUEST_SETTLED);
        command(NX_USER_IO_ASYNC_CONSUME);
        EXPECT_EQ(dispatch(), NX_ERROR_BUSY);
        EXPECT_EQ(nx_user_io_async_join(&mediation), NX_ERROR_BUSY);
        EXPECT_NE(slots[0].identity.request, nullptr);
        EXPECT_EQ(kernel.claimed_role, &mediation);
        EXPECT_EQ(copied[0][0], 0x11U);
    };
    EXPECT_EQ(nx_user_io_async_close(&mediation), NX_SUCCESS);
    EXPECT_EQ(nested, NX_ERROR_BUSY);
    EXPECT_NE(slots[0].identity.request, nullptr);
    EXPECT_EQ(copied[0][0], 0x11U);
    EXPECT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
    command(NX_USER_IO_ASYNC_CONSUME);
    EXPECT_EQ(dispatch(), NX_SUCCESS);
    EXPECT_EQ(nx_user_io_async_join(&mediation), NX_SUCCESS);
}

TEST_F(UserAsync, CloseDuringCopyPreventsAdmissionAndWipesTheCopiedBytes) {
    ASSERT_EQ(construct(), NX_SUCCESS);
    command(NX_USER_IO_ASYNC_SUBMIT);
    EXPECT_CALL(kernel, copy_in(_, _, _, _)).Times(testing::AnyNumber());
    EXPECT_CALL(kernel, copy_in(7U, copied[0], user.payload, 4U))
        .WillOnce([&](uint32_t, void* out, const void* in, size_t bytes) {
            std::memcpy(out, in, bytes);
            EXPECT_EQ(nx_user_io_async_close(&mediation), NX_SUCCESS);
            return NX_SUCCESS;
        });
    EXPECT_EQ(dispatch(), NX_ERROR_STATE);
    EXPECT_EQ(slots[0].identity.epoch, 0U);
    for (uint8_t byte : copied[0]) {
        EXPECT_EQ(byte, 0U);
    }
    EXPECT_TRUE(nx_bus_owner_idle(&owner));
}

TEST_F(UserAsync, CloseDuringSubmitReplyCopyRetainsAcceptedWorkUntilDrain) {
    ASSERT_EQ(construct(), NX_SUCCESS);
    command(NX_USER_IO_ASYNC_SUBMIT);
    EXPECT_CALL(kernel, copy_out(_, _, _, _)).Times(testing::AnyNumber());
    EXPECT_CALL(kernel, copy_out(7U, &user.reply, _, sizeof(user.reply)))
        .WillOnce([&](uint32_t, void* out, const void* in, size_t bytes) {
            EXPECT_EQ(nx_user_io_async_close(&mediation), NX_SUCCESS);
            EXPECT_EQ(nx_user_io_async_join(&mediation), NX_ERROR_BUSY);
            std::memcpy(out, in, bytes);
            return NX_SUCCESS;
        })
        .RetiresOnSaturation();
    ASSERT_EQ(dispatch(), NX_SUCCESS);
    EXPECT_NE(slots[0].identity.request, nullptr);
    EXPECT_EQ(copied[0][0], 0x11U);
    EXPECT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
    command(NX_USER_IO_ASYNC_CONSUME);
    EXPECT_EQ(dispatch(), NX_SUCCESS);
    EXPECT_EQ(nx_user_io_async_join(&mediation), NX_SUCCESS);
}

TEST_F(UserAsync, CloseDuringFailedConsumeCopyKeepsCellAndRoleUntilRetry) {
    ASSERT_EQ(construct(), NX_SUCCESS);
    submit();
    finish();
    command(NX_USER_IO_ASYNC_CONSUME);
    EXPECT_CALL(kernel, copy_out(_, _, _, _)).Times(testing::AnyNumber());
    EXPECT_CALL(kernel, copy_out(7U, &user.reply, _, sizeof(user.reply)))
        .WillOnce([&](uint32_t, void*, const void*, size_t) {
            EXPECT_EQ(nx_user_io_async_close(&mediation), NX_SUCCESS);
            EXPECT_EQ(nx_user_io_async_join(&mediation), NX_ERROR_BUSY);
            return NX_ERROR_IO;
        })
        .RetiresOnSaturation();
    EXPECT_EQ(dispatch(), NX_ERROR_IO);
    EXPECT_NE(slots[0].identity.request, nullptr);
    EXPECT_EQ(kernel.claimed_role, &mediation);
    EXPECT_EQ(copied[0][0], 0x11U);
    EXPECT_EQ(dispatch(), NX_SUCCESS);
    EXPECT_EQ(nx_user_io_async_join(&mediation), NX_SUCCESS);
}

TEST_F(UserAsync, CloseDuringHeaderCopyPreventsPayloadAccessAndAdmission) {
    ASSERT_EQ(construct(), NX_SUCCESS);
    command(NX_USER_IO_ASYNC_SUBMIT);
    EXPECT_CALL(kernel, copy_in(_, _, _, _)).Times(testing::AnyNumber());
    EXPECT_CALL(kernel, copy_in(7U, _, &user.message, sizeof(user.message)))
        .WillOnce([&](uint32_t, void* out, const void* in, size_t bytes) {
            std::memcpy(out, in, bytes);
            EXPECT_EQ(nx_user_io_async_close(&mediation), NX_SUCCESS);
            return NX_SUCCESS;
        });
    EXPECT_CALL(kernel, copy_in(7U, copied[0], user.payload, 4U)).Times(0);
    EXPECT_EQ(dispatch(), NX_ERROR_STATE);
    EXPECT_EQ(slots[0].identity.epoch, 0U);
    EXPECT_EQ(nx_user_io_async_join(&mediation), NX_SUCCESS);
}

TEST_F(UserAsync, LateCancellationPreservesObservedProviderSuccess) {
    ASSERT_EQ(construct(), NX_SUCCESS);
    submit();
    ASSERT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
    hardware.release = true;
    command(NX_USER_IO_ASYNC_CANCEL);
    ASSERT_EQ(dispatch(), NX_SUCCESS);
    EXPECT_CALL(hardware, cancel(_)).Times(0);
    EXPECT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
    command(NX_USER_IO_ASYNC_POLL);
    ASSERT_EQ(dispatch(), NX_SUCCESS);
    EXPECT_EQ(user.reply.result, NX_SUCCESS);
    EXPECT_EQ(user.reply.transferred, 4U);
}

TEST_F(UserAsync, DeadlineRemainsTheRealOwnersAbsoluteDomain) {
    ASSERT_EQ(construct(), NX_SUCCESS);
    command(NX_USER_IO_ASYNC_SUBMIT);
    user.message.deadline_low = 5U;
    user.message.deadline_high = 0U;
    ASSERT_EQ(dispatch(), NX_SUCCESS);
    clock.now = 5U;
    EXPECT_CALL(hardware, submit(_)).Times(0);
    EXPECT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
    command(NX_USER_IO_ASYNC_POLL);
    EXPECT_EQ(dispatch(), NX_SUCCESS);
    EXPECT_EQ(user.reply.result, NX_ERROR_TIMEOUT);
}

TEST_F(UserAsync, IndependentCellsKeepExactPayloadAndResultIdentities) {
    ASSERT_EQ(construct(), NX_SUCCESS);
    submit();
    command(NX_USER_IO_ASYNC_SUBMIT, 1U, 1U);
    user.payload[0] = 0xA3U;
    ASSERT_EQ(dispatch(), NX_SUCCESS);
    EXPECT_EQ(copied[0][0], 0x11U);
    EXPECT_EQ(copied[1][0], 0xA3U);
    finish(NX_SUCCESS, false);
    hardware.release = false;
    EXPECT_NE(slots[1].identity.request, nullptr);
    EXPECT_EQ(copied[1][0], 0xA3U);
    finish();
    command(NX_USER_IO_ASYNC_POLL, 1U, 1U);
    ASSERT_EQ(dispatch(), NX_SUCCESS);
    EXPECT_EQ(user.reply.slot, 1U);
    EXPECT_EQ(user.reply.result, NX_SUCCESS);
}

TEST_F(UserAsync, RejectsUserSpansThatAliasKernelControlsOrWrapWithoutCopy) {
    ASSERT_EQ(construct(), NX_SUCCESS);
    command(NX_USER_IO_ASYNC_SUBMIT);
    EXPECT_CALL(kernel, copy_in(_, _, _, _)).Times(0);
    EXPECT_EQ(
        nx_user_io_async_dispatch(
            &mediation,
            reinterpret_cast<nx_user_io_async_message_t*>(UINTPTR_MAX - 3U),
            user.payload, &user.reply),
        NX_ERROR_PERMISSION);
    EXPECT_EQ(nx_user_io_async_dispatch(
                  &mediation, &user.message, user.payload,
                  reinterpret_cast<nx_user_io_async_reply_t*>(&slots[0])),
              NX_ERROR_PERMISSION);
    EXPECT_EQ(slots[0].identity.epoch, 0U);
}

TEST_F(UserAsync, RejectsAliasedUserHeaderPayloadAndReplyBeforeEffects) {
    ASSERT_EQ(construct(), NX_SUCCESS);
    command(NX_USER_IO_ASYNC_SUBMIT);
    EXPECT_EQ(nx_user_io_async_dispatch(
                  &mediation, &user.message, user.payload,
                  reinterpret_cast<nx_user_io_async_reply_t*>(&user.message)),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_user_io_async_dispatch(
                  &mediation, &user.message,
                  reinterpret_cast<uint8_t*>(&user.message), &user.reply),
              NX_ERROR_INVALID);
    EXPECT_EQ(slots[0].identity.epoch, 0U);
    EXPECT_TRUE(nx_bus_owner_idle(&owner));
}

TEST_F(UserAsync, RejectsNestedDispatchAndMaskedGatewayWithoutSideEffects) {
    ASSERT_EQ(construct(), NX_SUCCESS);
    command(NX_USER_IO_ASYNC_SUBMIT);
    EXPECT_CALL(kernel, copy_in(_, _, _, _)).Times(testing::AnyNumber());
    EXPECT_CALL(kernel, copy_in(7U, copied[0], user.payload, 4U))
        .WillOnce([&](uint32_t, void* out, const void* in, size_t bytes) {
            EXPECT_EQ(dispatch(), NX_ERROR_BUSY);
            std::memcpy(out, in, bytes);
            return NX_SUCCESS;
        });
    EXPECT_EQ(dispatch(), NX_SUCCESS);
    const auto saved = nx_arch_irq_save();
    command(NX_USER_IO_ASYNC_POLL);
    EXPECT_EQ(dispatch(), NX_ERROR_CONTEXT);
    EXPECT_TRUE(nx_arch_irq_is_masked());
    nx_arch_irq_restore(saved);
    finish();
}

TEST_F(UserAsync, FreshStorageRequiresExclusiveLargerSessionAndRejectsOldWire) {
    ASSERT_EQ(construct(), NX_SUCCESS);
    nx_user_io_async_t newer = {};
    uint8_t fresh_bytes[2][16] = {};
    nx_user_io_async_slot_t fresh_slots[2] = {};
    for (size_t i = 0U; i < 2U; ++i) {
        fresh_slots[i].data = fresh_bytes[i];
        fresh_slots[i].capacity = sizeof(fresh_bytes[i]);
    }
    auto next = config;
    next.session = 2U;
    EXPECT_EQ(nx_user_io_async_construct(&newer, fresh_slots, 2U, &next),
              NX_ERROR_BUSY);
    EXPECT_FALSE(newer.initialized);
    EXPECT_EQ(kernel.history, 1U);
    submit();
    EXPECT_EQ(nx_user_io_async_close(&mediation), NX_SUCCESS);
    EXPECT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
    command(NX_USER_IO_ASYNC_CONSUME);
    ASSERT_EQ(dispatch(), NX_SUCCESS);
    ASSERT_EQ(nx_user_io_async_join(&mediation), NX_SUCCESS);
    next.session = 1U;
    EXPECT_EQ(nx_user_io_async_construct(&newer, fresh_slots, 2U, &next),
              NX_ERROR_STATE);
    next.session = 2U;
    ASSERT_EQ(nx_user_io_async_construct(&newer, fresh_slots, 2U, &next),
              NX_SUCCESS);
    for (uint32_t action :
         {NX_USER_IO_ASYNC_SUBMIT, NX_USER_IO_ASYNC_POLL,
          NX_USER_IO_ASYNC_CANCEL, NX_USER_IO_ASYNC_CONSUME}) {
        command(action);
        EXPECT_EQ(nx_user_io_async_dispatch(&newer, &user.message,
                                            action == NX_USER_IO_ASYNC_SUBMIT
                                                ? user.payload
                                                : nullptr,
                                            &user.reply),
                  NX_ERROR_STATE);
    }
    EXPECT_EQ(fresh_slots[0].identity.epoch, 0U);
    EXPECT_TRUE(nx_bus_owner_idle(&owner));
    EXPECT_EQ(nx_user_io_async_close(&newer), NX_SUCCESS);
    EXPECT_EQ(nx_user_io_async_join(&newer), NX_SUCCESS);
}

TEST_F(UserAsync, FailedRoleReleaseRetainsExclusivityUntilActualGatewayJoin) {
    ASSERT_EQ(construct(), NX_SUCCESS);
    ASSERT_EQ(nx_user_io_async_close(&mediation), NX_SUCCESS);
    kernel.outer_joined = false;
    EXPECT_EQ(nx_user_io_async_join(&mediation), NX_ERROR_BUSY);
    EXPECT_EQ(kernel.claimed_role, &mediation);
    EXPECT_FALSE(mediation.joined);
    nx_user_io_async_t newer = {};
    uint8_t bytes[16] = {};
    nx_user_io_async_slot_t fresh = {};
    fresh.data = bytes;
    fresh.capacity = sizeof(bytes);
    auto next = config;
    next.session = 2U;
    EXPECT_EQ(nx_user_io_async_construct(&newer, &fresh, 1U, &next),
              NX_ERROR_BUSY);
    kernel.outer_joined = true;
    EXPECT_EQ(nx_user_io_async_join(&mediation), NX_SUCCESS);
    EXPECT_EQ(kernel.claimed_role, nullptr);
    EXPECT_EQ(nx_user_io_async_construct(&newer, &fresh, 1U, &next),
              NX_SUCCESS);
    EXPECT_EQ(nx_user_io_async_close(&newer), NX_SUCCESS);
    EXPECT_EQ(nx_user_io_async_join(&newer), NX_SUCCESS);
}

TEST_F(UserAsync,
       ClaimAndReleaseCallbacksCannotRecursivelyReconstructOrRelease) {
    EXPECT_CALL(kernel, claim(7U, 9U, 1U, &owner, &mediation))
        .WillOnce([&](uint32_t, uint32_t, uint64_t session,
                      nx_bus_owner_t* actual_owner,
                      const nx_user_io_async_t* role) {
            EXPECT_EQ(construct(), NX_ERROR_BUSY);
            EXPECT_EQ(nx_user_io_async_close(&mediation), NX_ERROR_STATE);
            return kernel.claim_role(session, actual_owner, role);
        });
    ASSERT_EQ(construct(), NX_SUCCESS);
    ASSERT_EQ(nx_user_io_async_close(&mediation), NX_SUCCESS);
    EXPECT_CALL(kernel, release(&mediation, 1U))
        .WillOnce([&](const nx_user_io_async_t*, uint64_t) {
            EXPECT_EQ(nx_user_io_async_join(&mediation), NX_ERROR_BUSY);
            EXPECT_EQ(nx_user_io_async_close(&mediation), NX_ERROR_BUSY);
            command(NX_USER_IO_ASYNC_POLL);
            EXPECT_EQ(dispatch(), NX_ERROR_BUSY);
            kernel.claimed_owner = nullptr;
            kernel.claimed_role = nullptr;
            return NX_SUCCESS;
        });
    EXPECT_EQ(nx_user_io_async_join(&mediation), NX_SUCCESS);
}

TEST_F(UserAsync,
       ExhaustedSessionAuthorityRejectsFreshStorageWithoutEpochReset) {
    kernel.history = UINT64_MAX;
    config.session = UINT64_MAX;
    EXPECT_EQ(construct(), NX_ERROR_EXHAUSTED);
    EXPECT_FALSE(mediation.initialized);
    EXPECT_EQ(slots[0].identity.epoch, 0U);
    EXPECT_EQ(kernel.claimed_role, nullptr);
}

TEST_F(UserAsync, ConstructionRejectsKnownControlAndPayloadAliasesAtomically) {
    const auto before = slots[0];
    slots[1].data = slots[0].data;
    EXPECT_EQ(construct(), NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&slots[0], &before, sizeof(before)), 0);
    EXPECT_FALSE(mediation.initialized);
    slots[1].data = copied[1];
    region.begin = reinterpret_cast<uintptr_t>(&mediation);
    region.bytes = sizeof(mediation);
    EXPECT_EQ(construct(), NX_ERROR_INVALID);
    EXPECT_FALSE(mediation.initialized);
}

TEST_F(UserAsync,
       ConstructionRejectsUnsupportedBridgeAndOverflowBeforeMutation) {
    auto start = owner.executor.start;
    owner.executor.start = nullptr;
    EXPECT_EQ(construct(), NX_ERROR_INVALID);
    owner.executor.start = start;
    EXPECT_EQ(nx_user_io_async_construct(
                  &mediation, slots, SIZE_MAX / sizeof(*slots) + 1U, &config),
              NX_ERROR_INVALID);
    EXPECT_FALSE(mediation.initialized);
}

TEST_F(UserAsync, PrivilegedReapRequiresClosedBindingAndExactSettledCells) {
    EXPECT_EQ(nx_user_io_async_reap(&mediation), NX_ERROR_STATE);
    ASSERT_EQ(construct(), NX_SUCCESS);
    submit();
    EXPECT_CALL(kernel, drained(_, _)).Times(0);
    EXPECT_EQ(nx_user_io_async_reap(&mediation), NX_ERROR_STATE);
    ASSERT_EQ(nx_user_io_async_close(&mediation), NX_SUCCESS);
    EXPECT_EQ(nx_user_io_async_reap(&mediation), NX_ERROR_BUSY);
    EXPECT_NE(slots[0].identity.request, nullptr);
    EXPECT_EQ(copied[0][0], 0x11U);
}

TEST_F(UserAsync,
       ReapRetiresCopiedBytesWhenUserMappingIsPermanentlyUnavailable) {
    ASSERT_EQ(construct(), NX_SUCCESS);
    submit();
    finish();
    ASSERT_EQ(nx_user_io_async_close(&mediation), NX_SUCCESS);
    EXPECT_CALL(kernel, copy_out(_, _, _, _))
        .WillRepeatedly(Return(NX_ERROR_IO));
    command(NX_USER_IO_ASYNC_CONSUME);
    EXPECT_EQ(dispatch(), NX_ERROR_IO);
    EXPECT_NE(slots[0].identity.request, nullptr);
    EXPECT_EQ(nx_user_io_async_reap(&mediation), NX_SUCCESS);
    EXPECT_EQ(slots[0].identity.request, nullptr);
    EXPECT_EQ(slots[0].identity.epoch, 1U);
    EXPECT_EQ(kernel.claimed_role, &mediation);
    for (uint8_t byte : copied[0]) {
        EXPECT_EQ(byte, 0U);
    }
    EXPECT_CALL(kernel, identity(_)).Times(0);
    EXPECT_CALL(kernel, drained(_, _)).Times(0);
    EXPECT_EQ(dispatch(), NX_ERROR_STATE);
    EXPECT_EQ(nx_user_io_async_reap(&mediation), NX_SUCCESS);
    EXPECT_EQ(nx_user_io_async_join(&mediation), NX_SUCCESS);
    EXPECT_EQ(kernel.claimed_role, nullptr);
    EXPECT_EQ(nx_user_io_async_reap(&mediation), NX_SUCCESS);
}

TEST_F(UserAsync, ReapNeverWipesSettledCellWhileAnotherCellStillBorrowed) {
    ASSERT_EQ(construct(), NX_SUCCESS);
    submit();
    command(NX_USER_IO_ASYNC_SUBMIT, 1U);
    ASSERT_EQ(dispatch(), NX_SUCCESS);
    finish(NX_SUCCESS, false);
    hardware.release = false;
    ASSERT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
    ASSERT_EQ(nx_request_state(&slots[0].request), NX_REQUEST_SETTLED);
    ASSERT_EQ(nx_request_state(&slots[1].request), NX_REQUEST_ACTIVE);
    ASSERT_EQ(nx_user_io_async_close(&mediation), NX_SUCCESS);
    EXPECT_CALL(kernel, drained(_, _)).Times(0);
    EXPECT_EQ(nx_user_io_async_reap(&mediation), NX_ERROR_BUSY);
    for (size_t i = 0U; i < 2U; ++i) {
        EXPECT_NE(slots[i].identity.request, nullptr);
        EXPECT_EQ(copied[i][0], 0x11U);
    }
    hardware.quarantine = true;
    EXPECT_EQ(nx_bus_owner_service(&owner), NX_ERROR_IO);
    EXPECT_EQ(nx_user_io_async_reap(&mediation), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&slots[1].request), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(copied[0][0], 0x11U);
    EXPECT_EQ(copied[1][0], 0x11U);
}

TEST_F(UserAsync, ReapRequiresRealOwnerPublisherAndOuterObserversJoined) {
    ASSERT_EQ(construct(), NX_SUCCESS);
    submit();
    ASSERT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
    ASSERT_EQ(nx_user_io_async_close(&mediation), NX_SUCCESS);
    bool observed = false;
    guard.after_leave = [&] {
        if (nx_request_state(&slots[0].request) == NX_REQUEST_SETTLED &&
            owner.servicing) {
            guard.after_leave = nullptr;
            observed = true;
            EXPECT_EQ(nx_user_io_async_reap(&mediation), NX_ERROR_BUSY);
            EXPECT_EQ(copied[0][0], 0x11U);
        }
    };
    finish();
    EXPECT_TRUE(observed);
    kernel.outer_joined = false;
    EXPECT_EQ(nx_user_io_async_reap(&mediation), NX_ERROR_BUSY);
    EXPECT_NE(slots[0].identity.request, nullptr);
    EXPECT_EQ(slots[0].identity.epoch, 1U);
    EXPECT_EQ(copied[0][0], 0x11U);
    EXPECT_EQ(kernel.claimed_role, &mediation);
    kernel.outer_joined = true;
    EXPECT_EQ(nx_user_io_async_reap(&mediation), NX_SUCCESS);
    EXPECT_EQ(nx_user_io_async_join(&mediation), NX_SUCCESS);
}

TEST_F(UserAsync, ReapReservesControlBeforeOwnerGuardAndDrainedCallbacks) {
    ASSERT_EQ(construct(), NX_SUCCESS);
    submit();
    finish();
    ASSERT_EQ(nx_user_io_async_close(&mediation), NX_SUCCESS);
    command(NX_USER_IO_ASYNC_CONSUME);
    auto reenter = [&] {
        EXPECT_EQ(nx_user_io_async_reap(&mediation), NX_ERROR_BUSY);
        EXPECT_EQ(nx_user_io_async_close(&mediation), NX_ERROR_BUSY);
        EXPECT_EQ(nx_user_io_async_join(&mediation), NX_ERROR_BUSY);
        EXPECT_EQ(dispatch(), NX_ERROR_BUSY);
        EXPECT_NE(slots[0].identity.request, nullptr);
        EXPECT_EQ(copied[0][0], 0x11U);
        EXPECT_EQ(kernel.claimed_role, &mediation);
    };
    guard.after_leave = [&] {
        guard.after_leave = nullptr;
        reenter();
    };
    EXPECT_CALL(kernel, drained(&mediation, 1U))
        .WillOnce([&](const nx_user_io_async_t*, uint64_t) {
            reenter();
            return NX_SUCCESS;
        });
    EXPECT_EQ(nx_user_io_async_reap(&mediation), NX_SUCCESS);
    EXPECT_EQ(nx_user_io_async_join(&mediation), NX_SUCCESS);
}

TEST_F(UserAsync, FailedReleaseAfterReapRetainsExclusiveRoleAndEpochHistory) {
    ASSERT_EQ(construct(), NX_SUCCESS);
    submit();
    finish();
    ASSERT_EQ(nx_user_io_async_close(&mediation), NX_SUCCESS);
    ASSERT_EQ(nx_user_io_async_reap(&mediation), NX_SUCCESS);
    EXPECT_CALL(kernel, release(_, _)).Times(testing::AnyNumber());
    EXPECT_CALL(kernel, release(&mediation, 1U))
        .WillOnce(Return(NX_ERROR_IO))
        .RetiresOnSaturation();
    EXPECT_EQ(nx_user_io_async_join(&mediation), NX_ERROR_IO);
    EXPECT_FALSE(mediation.joined);
    EXPECT_EQ(kernel.claimed_role, &mediation);
    EXPECT_EQ(slots[0].identity.epoch, 1U);
    EXPECT_EQ(nx_user_io_async_reap(&mediation), NX_SUCCESS);
    EXPECT_EQ(nx_user_io_async_join(&mediation), NX_SUCCESS);
}

} /* namespace */
