/**
 * \file            user_io_broker_test.cpp
 * \brief           Real typed GPIO gateway and retained kernel memory loans
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/core/request.h"
#include "provider.h"
#include "user_io_broker_impl.h"
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <new>
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
namespace {
bool privileged = true;
uint32_t exception_number;
class Kernel {
  public:
    unsigned pins = 0, unpins = 0;
    uintptr_t cookies = 0;
    MOCK_METHOD(nx_result_t, Identity, (uint32_t*));
    MOCK_METHOD(nx_result_t, Pin,
                (uint32_t, void*, size_t, uint32_t, uintptr_t*));
    MOCK_METHOD(nx_result_t, Unpin, (uintptr_t));
};
struct Proof {
    nx_request_t request;
    unsigned observers;
};
/** \brief Trusted proof requires actual request publication and observer join.
 */
nx_result_t Drain(const void* context) {
    auto* proof = static_cast<const Proof*>(context);
    EXPECT_FALSE(nx_arch_irq_is_masked());
    return nx_request_state(&proof->request) == NX_REQUEST_SETTLED &&
                   proof->observers == 0U
               ? NX_SUCCESS
               : NX_ERROR_BUSY;
}
const nx_user_io_memory_ops_t memory_ops = {
    [](const void* context, uint32_t* principal) {
        EXPECT_FALSE(nx_arch_irq_is_masked());
        return const_cast<Kernel*>(static_cast<const Kernel*>(context))
            ->Identity(principal);
    },
    [](void* context, uint32_t principal, void* address, size_t bytes,
       uint32_t access, uintptr_t* cookie) {
        EXPECT_FALSE(nx_arch_irq_is_masked());
        return static_cast<Kernel*>(context)->Pin(principal, address, bytes,
                                                  access, cookie);
    },
    [](void* context, uintptr_t cookie) {
        EXPECT_FALSE(nx_arch_irq_is_masked());
        return static_cast<Kernel*>(context)->Unpin(cookie);
    }};
class UserIoBroker : public ::testing::Test {
  protected:
    NiceMock<Kernel> kernel;
    alignas(8) unsigned char user[256] = {};
    nx_native_gpio_state_t gpio = {};
    nx_gpio_port_t face = {&nx_native_gpio_ops, &gpio};
    nx_user_io_region_t region = {user, sizeof(user), 7,
                                  NX_USER_MEMORY_READ | NX_USER_MEMORY_WRITE};
    nx_user_io_gpio_grant_t grant = {&face, sizeof(gpio), 7, 3, 0xFF, 0x0F};
    nx_user_io_broker_config_t config = {
        &region, 1, &grant, 1, 2, {&memory_ops, &kernel, sizeof(kernel)}};
    nx_user_io_broker_t broker = {};
    nx_user_io_call_t call = {};
    nx_user_io_loan_t loan = {};
    Proof proof = {};
    nx_user_io_message_t* message() {
        return reinterpret_cast<nx_user_io_message_t*>(user);
    }
    nx_user_io_reply_t* reply() {
        return reinterpret_cast<nx_user_io_reply_t*>(user + 64);
    }
    nx_user_io_drain_port_t drain() {
        return {Drain, &proof, sizeof(proof)};
    }
    void SetUp() override {
        privileged = true;
        exception_number = 0;
        ASSERT_EQ(nx_native_gpio_configure_instance(&gpio, 0xFF, 0, true),
                  NX_SUCCESS);
        nx_request_initialize(&proof.request);
        ASSERT_EQ(nx_request_admit(&proof.request, NX_REQUEST_ACTIVE),
                  NX_SUCCESS);
        proof.observers = 1;
        ON_CALL(kernel, Identity(_)).WillByDefault([](uint32_t* principal) {
            *principal = 7;
            return NX_SUCCESS;
        });
        ON_CALL(kernel, Pin(_, _, _, _, _))
            .WillByDefault(
                [this](uint32_t, void*, size_t, uint32_t, uintptr_t* cookie) {
                    ++kernel.pins;
                    *cookie = ++kernel.cookies;
                    return NX_SUCCESS;
                });
        ON_CALL(kernel, Unpin(_)).WillByDefault([this](uintptr_t) {
            ++kernel.unpins;
            return NX_SUCCESS;
        });
        ASSERT_EQ(nx_user_io_broker_construct(&broker, &config), NX_SUCCESS);
        new (user) nx_user_io_message_t{
            NX_USER_IO_ABI, 3, NX_USER_GPIO_WRITE, 3, 0, 0, {0, 0}};
        new (user + 64) nx_user_io_reply_t{NX_ERROR_IO, 0xFFFFFFFF};
    }
    nx_user_io_result_t Call() {
        return nx_user_io_broker_gpio(&broker, &call, message(), reply());
    }
    nx_user_io_loan_result_t Loan(uint32_t access = NX_USER_MEMORY_READ) {
        return nx_user_io_loan_acquire(&broker, &loan, 7, user + 128, 32,
                                       access, drain());
    }
    void Settled() {
        nx_request_settle(&proof.request, NX_SUCCESS, 32);
        proof.observers = 0;
    }
};
TEST_F(UserIoBroker, AuthorizedScalarWriteUsesRealTypedProvider) {
    auto result = Call();
    EXPECT_EQ(result.result, NX_SUCCESS);
    EXPECT_TRUE(result.accepted);
    EXPECT_FALSE(result.retained);
    EXPECT_TRUE(result.device_effect_possible);
    EXPECT_EQ(nx_native_gpio_output_instance(&gpio), 3U);
    EXPECT_EQ(reply()->result, NX_SUCCESS);
    EXPECT_EQ(reply()->value, 0U);
    EXPECT_EQ(kernel.pins, 2U);
    EXPECT_EQ(kernel.unpins, 2U);
}
TEST_F(UserIoBroker, ReadReturnsOnlyRequestedAuthorizedInputBits) {
    nx_native_gpio_input_instance(&gpio, 0xA5);
    message()->operation = NX_USER_GPIO_READ;
    message()->set_mask = 0;
    message()->read_mask = 3;
    auto result = Call();
    EXPECT_EQ(result.result, NX_SUCCESS);
    EXPECT_FALSE(result.device_effect_possible);
    EXPECT_EQ(reply()->value, 1U);
}
TEST_F(UserIoBroker, AuthenticPrincipalCannotBeSuppliedByUser) {
    EXPECT_CALL(kernel, Identity(_)).WillOnce([](uint32_t* principal) {
        *principal = 99;
        return NX_SUCCESS;
    });
    EXPECT_CALL(kernel, Pin(_, _, _, _, _)).Times(0);
    auto result = Call();
    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.result, NX_ERROR_PERMISSION);
    EXPECT_EQ(nx_native_gpio_output_instance(&gpio), 0U);
}
TEST_F(UserIoBroker, UnauthorizedMasksRejectBeforeAnyDeviceEffect) {
    message()->set_mask = 0x10;
    auto result = Call();
    EXPECT_EQ(result.result, NX_ERROR_PERMISSION);
    EXPECT_FALSE(result.retained);
    EXPECT_FALSE(result.device_effect_possible);
    EXPECT_EQ(nx_native_gpio_output_instance(&gpio), 0U);
    EXPECT_EQ(reply()->value, 0xFFFFFFFFU);
}
TEST_F(UserIoBroker, BadAbiReservedOperationAndOverlappingMasksReject) {
    message()->reserved[0] = 7;
    EXPECT_EQ(Call().result, NX_ERROR_INVALID);
    message()->reserved[0] = 0;
    message()->abi = 2;
    EXPECT_EQ(Call().result, NX_ERROR_INVALID);
    message()->abi = 1;
    message()->operation = 9;
    EXPECT_EQ(Call().result, NX_ERROR_UNSUPPORTED);
    message()->operation = NX_USER_GPIO_WRITE;
    message()->reset_mask = 1;
    EXPECT_EQ(Call().result, NX_ERROR_INVALID);
    EXPECT_EQ(nx_native_gpio_output_instance(&gpio), 0U);
}
TEST_F(UserIoBroker, ContextRejectsBeforeIdentityOrPins) {
    EXPECT_CALL(kernel, Identity(_)).Times(0);
    privileged = false;
    EXPECT_EQ(Call().result, NX_ERROR_CONTEXT);
    privileged = true;
    exception_number = 11;
    EXPECT_EQ(Call().result, NX_ERROR_CONTEXT);
    exception_number = 0;
}
TEST_F(UserIoBroker, WholeSpanBoundsPermissionsAndOverflowAreChecked) {
    auto result = nx_user_io_broker_gpio(
        &broker, &call,
        reinterpret_cast<nx_user_io_message_t*>(user + sizeof(user) - 8),
        reply());
    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.result, NX_ERROR_PERMISSION);
    result = nx_user_io_broker_gpio(
        &broker, &call,
        reinterpret_cast<nx_user_io_message_t*>(UINTPTR_MAX - 7U), reply());
    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.result, NX_ERROR_INVALID);
    auto readonly_region = region;
    readonly_region.access = NX_USER_MEMORY_READ;
    auto readonly_config = config;
    readonly_config.regions = &readonly_region;
    nx_user_io_broker_t readonly_broker = {};
    ASSERT_EQ(nx_user_io_broker_construct(&readonly_broker, &readonly_config),
              NX_SUCCESS);
    result =
        nx_user_io_broker_gpio(&readonly_broker, &call, message(), reply());
    EXPECT_EQ(result.result, NX_ERROR_PERMISSION);
    EXPECT_FALSE(result.device_effect_possible);
    EXPECT_FALSE(result.retained);
}
TEST_F(UserIoBroker, InputReplyAndKernelCallAliasesRejectBeforePins) {
    EXPECT_CALL(kernel, Pin(_, _, _, _, _)).Times(0);
    auto r = nx_user_io_broker_gpio(
        &broker, &call, message(), reinterpret_cast<nx_user_io_reply_t*>(user));
    EXPECT_FALSE(r.accepted);
    EXPECT_EQ(r.result, NX_ERROR_INVALID);
    r = nx_user_io_broker_gpio(&broker,
                               reinterpret_cast<nx_user_io_call_t*>(user + 128),
                               message(), reply());
    EXPECT_FALSE(r.accepted);
    EXPECT_EQ(r.result, NX_ERROR_INVALID);
}
TEST_F(UserIoBroker, FailedUnpinRetainsCallAndFinishNeverReplaysWrite) {
    EXPECT_CALL(kernel, Unpin(_)).WillRepeatedly(Return(NX_ERROR_BUSY));
    auto result = Call();
    EXPECT_TRUE(result.accepted);
    EXPECT_TRUE(result.retained);
    EXPECT_EQ(result.cleanup_result, NX_ERROR_BUSY);
    EXPECT_TRUE(result.device_effect_possible);
    EXPECT_EQ(nx_native_gpio_output_instance(&gpio), 3U);
    EXPECT_EQ(nx_user_io_broker_quiescent(&broker), NX_ERROR_BUSY);
    EXPECT_EQ(Call().result, NX_ERROR_BUSY);
    EXPECT_CALL(kernel, Unpin(_)).WillRepeatedly([this](uintptr_t) {
        ++kernel.unpins;
        return NX_SUCCESS;
    });
    ASSERT_EQ(nx_native_gpio_configure_instance(&gpio, 0xFF, 0, true),
              NX_SUCCESS);
    result = nx_user_io_broker_gpio_finish(&broker, &call);
    EXPECT_FALSE(result.retained);
    EXPECT_EQ(result.cleanup_result, NX_SUCCESS);
    EXPECT_EQ(nx_native_gpio_output_instance(&gpio), 0U);
    EXPECT_EQ(broker.count, 0U);
}
TEST_F(UserIoBroker, SecondPinFailureRetainsFirstIfRollbackCannotUnpin) {
    EXPECT_CALL(kernel, Pin(_, _, _, _, _))
        .WillOnce([this](uint32_t, void*, size_t, uint32_t, uintptr_t* cookie) {
            ++kernel.pins;
            *cookie = ++kernel.cookies;
            return NX_SUCCESS;
        })
        .WillOnce(Return(NX_ERROR_IO));
    EXPECT_CALL(kernel, Unpin(_)).WillRepeatedly(Return(NX_ERROR_BUSY));
    auto result = Call();
    EXPECT_TRUE(result.accepted);
    EXPECT_TRUE(result.retained);
    EXPECT_EQ(result.result, NX_ERROR_IO);
    EXPECT_FALSE(result.device_effect_possible);
    EXPECT_EQ(call.input.owner, &broker);
    EXPECT_EQ(call.output.owner, nullptr);
    EXPECT_EQ(broker.count, 1U);
    EXPECT_EQ(nx_native_gpio_output_instance(&gpio), 0U);
}
TEST_F(UserIoBroker, RevokeDuringPinPreventsAnyDeviceEffectAndKeepsCleanup) {
    EXPECT_CALL(kernel, Pin(_, _, _, _, _))
        .WillOnce([this](uint32_t, void*, size_t, uint32_t, uintptr_t* cookie) {
            *cookie = ++kernel.cookies;
            EXPECT_EQ(nx_user_io_broker_close(&broker), NX_SUCCESS);
            return NX_SUCCESS;
        });
    EXPECT_CALL(kernel, Unpin(_)).WillRepeatedly(Return(NX_ERROR_BUSY));
    auto result = Call();
    EXPECT_TRUE(result.accepted);
    EXPECT_TRUE(result.retained);
    EXPECT_EQ(result.result, NX_ERROR_CANCELLED);
    EXPECT_FALSE(result.device_effect_possible);
    EXPECT_TRUE(broker.closing);
    EXPECT_EQ(nx_native_gpio_output_instance(&gpio), 0U);
}
TEST_F(UserIoBroker, SnapshotDoesNotReReadAChangedUserMessage) {
    EXPECT_CALL(kernel, Pin(_, _, _, _, _))
        .WillOnce([this](uint32_t, void*, size_t, uint32_t, uintptr_t* cookie) {
            *cookie = ++kernel.cookies;
            return NX_SUCCESS;
        })
        .WillOnce([this](uint32_t, void*, size_t, uint32_t, uintptr_t* cookie) {
            message()->set_mask = 0x80;
            *cookie = ++kernel.cookies;
            return NX_SUCCESS;
        });
    EXPECT_EQ(Call().result, NX_SUCCESS);
    EXPECT_EQ(nx_native_gpio_output_instance(&gpio), 3U);
}
TEST_F(UserIoBroker, GenericLoanRequiresActualSettlementAndObserverJoin) {
    auto accepted = Loan();
    ASSERT_TRUE(accepted.accepted);
    EXPECT_EQ(nx_user_io_loan_release(&broker, accepted.ticket), NX_ERROR_BUSY);
    nx_request_settle(&proof.request, NX_SUCCESS, 32);
    EXPECT_EQ(nx_user_io_loan_release(&broker, accepted.ticket), NX_ERROR_BUSY);
    EXPECT_EQ(kernel.unpins, 0U);
    proof.observers = 0;
    EXPECT_EQ(nx_user_io_loan_release(&broker, accepted.ticket), NX_SUCCESS);
    EXPECT_EQ(kernel.unpins, 1U);
    EXPECT_EQ(loan.owner, nullptr);
}
TEST_F(UserIoBroker, CloseCannotRevokePinnedOutstandingLoan) {
    auto accepted = Loan();
    ASSERT_TRUE(accepted.accepted);
    ASSERT_EQ(nx_user_io_broker_close(&broker), NX_SUCCESS);
    EXPECT_EQ(nx_user_io_broker_quiescent(&broker), NX_ERROR_BUSY);
    nx_user_io_loan_t second = {};
    auto reject = nx_user_io_loan_acquire(&broker, &second, 7, user + 160, 16,
                                          NX_USER_MEMORY_READ, drain());
    EXPECT_FALSE(reject.accepted);
    EXPECT_EQ(reject.result, NX_ERROR_BUSY);
    EXPECT_EQ(loan.owner, &broker);
    Settled();
    EXPECT_EQ(nx_user_io_loan_release(&broker, accepted.ticket), NX_SUCCESS);
    EXPECT_EQ(nx_user_io_broker_quiescent(&broker), NX_SUCCESS);
}
TEST_F(UserIoBroker, CopiedAndStaleLoanTicketsCannotUnpinOriginal) {
    auto accepted = Loan();
    ASSERT_TRUE(accepted.accepted);
    auto copy = loan;
    EXPECT_EQ(nx_user_io_loan_release(&broker, {&copy, copy.epoch}),
              NX_ERROR_STATE);
    Settled();
    ASSERT_EQ(nx_user_io_loan_release(&broker, accepted.ticket), NX_SUCCESS);
    auto next = Loan();
    ASSERT_TRUE(next.accepted);
    EXPECT_GT(next.ticket.epoch, accepted.ticket.epoch);
    EXPECT_EQ(nx_user_io_loan_release(&broker, accepted.ticket),
              NX_ERROR_STATE);
    EXPECT_EQ(loan.owner, &broker);
    EXPECT_EQ(nx_user_io_loan_release(&broker, next.ticket), NX_SUCCESS);
}
TEST_F(UserIoBroker, FailedUnpinKeepsPinnedLoanAndExactIdentity) {
    auto accepted = Loan();
    ASSERT_TRUE(accepted.accepted);
    Settled();
    EXPECT_CALL(kernel, Unpin(_)).WillRepeatedly(Return(NX_ERROR_IO));
    EXPECT_EQ(nx_user_io_loan_release(&broker, accepted.ticket), NX_ERROR_IO);
    EXPECT_TRUE(loan.pinned);
    EXPECT_EQ(loan.owner, &broker);
    EXPECT_EQ(broker.count, 1U);
}
TEST_F(UserIoBroker, PinFailureRejectsWithoutBorrowAndEpochCannotWrap) {
    EXPECT_CALL(kernel, Pin(_, _, _, _, _)).WillOnce(Return(NX_ERROR_IO));
    auto reject = Loan();
    EXPECT_FALSE(reject.accepted);
    EXPECT_EQ(reject.result, NX_ERROR_IO);
    EXPECT_EQ(loan.owner, nullptr);
    EXPECT_EQ(broker.count, 0U);
    loan.epoch = UINT64_MAX;
    reject = Loan();
    EXPECT_FALSE(reject.accepted);
    EXPECT_EQ(reject.result, NX_ERROR_EXHAUSTED);
}
TEST_F(UserIoBroker, WritablePeerOverlapRejectsButReadOnlySharingIsLegal) {
    auto first = Loan();
    ASSERT_TRUE(first.accepted);
    nx_user_io_loan_t second = {};
    auto conflict = nx_user_io_loan_acquire(&broker, &second, 7, user + 136, 16,
                                            NX_USER_MEMORY_WRITE, drain());
    EXPECT_FALSE(conflict.accepted);
    EXPECT_EQ(conflict.result, NX_ERROR_INVALID);
    auto readonly = nx_user_io_loan_acquire(&broker, &second, 7, user + 136, 16,
                                            NX_USER_MEMORY_READ, drain());
    EXPECT_TRUE(readonly.accepted);
    Settled();
    EXPECT_EQ(nx_user_io_loan_release(&broker, readonly.ticket), NX_SUCCESS);
    EXPECT_EQ(nx_user_io_loan_release(&broker, first.ticket), NX_SUCCESS);
}
TEST_F(UserIoBroker, UserLoanAndDrainControlsCannotResideInUserWindow) {
    auto reject = nx_user_io_loan_acquire(
        &broker, reinterpret_cast<nx_user_io_loan_t*>(user + 128), 7, user, 32,
        NX_USER_MEMORY_READ, drain());
    EXPECT_FALSE(reject.accepted);
    EXPECT_EQ(reject.result, NX_ERROR_INVALID);
    auto unsafe = drain();
    unsafe.context = user + 224;
    unsafe.context_bytes = 8;
    reject = nx_user_io_loan_acquire(&broker, &loan, 7, user + 128, 16,
                                     NX_USER_MEMORY_READ, unsafe);
    EXPECT_FALSE(reject.accepted);
    EXPECT_EQ(reject.result, NX_ERROR_INVALID);
}
TEST_F(UserIoBroker, ExactCapacityDoesNotCreateHiddenLoans) {
    auto first = Loan();
    ASSERT_TRUE(first.accepted);
    nx_user_io_loan_t second = {}, third = {};
    auto next = nx_user_io_loan_acquire(&broker, &second, 7, user + 160, 16,
                                        NX_USER_MEMORY_READ, drain());
    ASSERT_TRUE(next.accepted);
    auto full = nx_user_io_loan_acquire(&broker, &third, 7, user + 192, 16,
                                        NX_USER_MEMORY_READ, drain());
    EXPECT_FALSE(full.accepted);
    EXPECT_EQ(full.result, NX_ERROR_EXHAUSTED);
    EXPECT_EQ(third.owner, nullptr);
    Settled();
    EXPECT_EQ(nx_user_io_loan_release(&broker, next.ticket), NX_SUCCESS);
    EXPECT_EQ(nx_user_io_loan_release(&broker, first.ticket), NX_SUCCESS);
}
TEST_F(UserIoBroker,
       ConstructionRejectsUserWindowsCoveringKnownKernelControls) {
    nx_user_io_broker_t unused = {};
    auto unsafe_region = region;
    auto unsafe_config = config;
    unsafe_config.regions = &unsafe_region;
    unsafe_region.base = &gpio;
    unsafe_region.bytes = sizeof(gpio);
    EXPECT_EQ(nx_user_io_broker_construct(&unused, &unsafe_config),
              NX_ERROR_INVALID);
    unsafe_region.base = &unused;
    unsafe_region.bytes = sizeof(unused);
    EXPECT_EQ(nx_user_io_broker_construct(&unused, &unsafe_config),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_user_io_broker_construct(&broker, &config), NX_ERROR_STATE);
}
TEST_F(UserIoBroker, KernelCallAndLoanCannotAliasKnownProviderContext) {
    auto result = nx_user_io_broker_gpio(
        &broker, reinterpret_cast<nx_user_io_call_t*>(&gpio), message(),
        reply());
    EXPECT_EQ(result.result, NX_ERROR_INVALID);
    EXPECT_FALSE(result.accepted);
    auto admission = nx_user_io_loan_acquire(
        &broker, reinterpret_cast<nx_user_io_loan_t*>(&gpio), 7, user + 128, 16,
        NX_USER_MEMORY_READ, drain());
    EXPECT_EQ(admission.result, NX_ERROR_INVALID);
    EXPECT_FALSE(admission.accepted);
}
TEST_F(UserIoBroker, LoanCannotOverwriteItsOwnTrustedDrainControl) {
    alignas(nx_user_io_loan_t) unsigned char control[256] = {};
    nx_user_io_drain_port_t trusted = {[](const void*) { return NX_SUCCESS; },
                                       control, sizeof(control)};
    auto result = nx_user_io_loan_acquire(
        &broker, reinterpret_cast<nx_user_io_loan_t*>(control), 7, user + 128,
        16, NX_USER_MEMORY_READ, trusted);
    EXPECT_EQ(result.result, NX_ERROR_INVALID);
    EXPECT_FALSE(result.accepted);
}
TEST_F(UserIoBroker, NewCallCannotOverwriteAnotherRetainedDrainContext) {
    alignas(nx_user_io_call_t) unsigned char
        control[sizeof(nx_user_io_call_t)] = {};
    nx_user_io_drain_port_t trusted = {[](const void*) { return NX_SUCCESS; },
                                       control, sizeof(control)};
    auto accepted = nx_user_io_loan_acquire(&broker, &loan, 7, user + 128, 16,
                                            NX_USER_MEMORY_READ, trusted);
    ASSERT_TRUE(accepted.accepted);
    auto result = nx_user_io_broker_gpio(
        &broker, reinterpret_cast<nx_user_io_call_t*>(control), message(),
        reply());
    EXPECT_EQ(result.result, NX_ERROR_INVALID);
    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(reinterpret_cast<nx_user_io_call_t*>(control)->result,
              NX_SUCCESS);
    EXPECT_EQ(nx_user_io_loan_release(&broker, accepted.ticket), NX_SUCCESS);
}
TEST_F(UserIoBroker, ConstructCannotOverlayKnownMutableProviderStorage) {
    alignas(nx_user_io_broker_t) unsigned char control[128] = {};
    nx_gpio_port_t other_face = {&nx_native_gpio_ops, control};
    auto other_grant = grant;
    other_grant.port = &other_face;
    other_grant.context_bytes = sizeof(control);
    auto other_config = config;
    other_config.grants = &other_grant;
    EXPECT_EQ(
        nx_user_io_broker_construct(
            reinterpret_cast<nx_user_io_broker_t*>(control), &other_config),
        NX_ERROR_INVALID);
}
TEST_F(UserIoBroker, OpenAdmissionIsNotAStableQuiescenceBarrier) {
    EXPECT_EQ(nx_user_io_broker_quiescent(&broker), NX_ERROR_STATE);
    ASSERT_EQ(nx_user_io_broker_close(&broker), NX_SUCCESS);
    EXPECT_EQ(nx_user_io_broker_quiescent(&broker), NX_SUCCESS);
}
TEST_F(UserIoBroker, IncomingMaskedThreadCannotCallMemoryGateway) {
    EXPECT_CALL(kernel, Identity(_)).Times(0);
    EXPECT_CALL(kernel, Pin(_, _, _, _, _)).Times(0);
    auto saved = nx_arch_irq_save();
    EXPECT_EQ(Call().result, NX_ERROR_CONTEXT);
    EXPECT_EQ(Loan().result, NX_ERROR_CONTEXT);
    EXPECT_TRUE(nx_arch_irq_is_masked());
    nx_arch_irq_restore(saved);
}
TEST_F(UserIoBroker, ProviderCancellationStillCopiesInitializedErrorReply) {
    static const nx_gpio_ops_t cancelled_ops = {
        nullptr,
        [](const void*, uint32_t*) {
            EXPECT_TRUE(nx_arch_irq_is_masked());
            return NX_ERROR_CANCELLED;
        },
        nullptr};
    nx_gpio_port_t cancelled_face = {&cancelled_ops, &gpio};
    auto cancelled_grant = grant;
    cancelled_grant.port = &cancelled_face;
    auto cancelled_config = config;
    cancelled_config.grants = &cancelled_grant;
    nx_user_io_broker_t cancelled_broker = {};
    ASSERT_EQ(nx_user_io_broker_construct(&cancelled_broker, &cancelled_config),
              NX_SUCCESS);
    message()->operation = NX_USER_GPIO_READ;
    message()->set_mask = 0;
    message()->read_mask = 1;
    auto result =
        nx_user_io_broker_gpio(&cancelled_broker, &call, message(), reply());
    EXPECT_TRUE(result.accepted);
    EXPECT_FALSE(result.retained);
    EXPECT_EQ(result.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(reply()->result, NX_ERROR_CANCELLED);
    EXPECT_EQ(reply()->value, 0U);
}
} /* namespace */
extern "C" bool nx_user_io_test_privileged(void) {
    return privileged;
}
extern "C" uint32_t nx_user_io_test_exception(void) {
    return exception_number;
}
