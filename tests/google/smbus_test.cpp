/**
 * \file            smbus_test.cpp
 * \brief           Packet PEC over borrowed I2C, without a hardware host claim
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-13
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/smbus.h"
#include <array>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
nx_time_us_t now;
bool in_isr;
bool masked;
class I2CPorts {
  public:
    MOCK_METHOD(nx_result_t, Submit, (nx_i2c_request_t*));
    MOCK_METHOD(nx_result_t, Cancel, (nx_i2c_request_t*));
    MOCK_METHOD(void, Service, ());
    MOCK_METHOD(bool, Matches, (const nx_i2c_port_t*), (const));
};
nx_result_t submit(void* context, nx_i2c_request_t* request) {
    return static_cast<I2CPorts*>(context)->Submit(request);
}
nx_result_t cancel(void* context, nx_i2c_request_t* request) {
    return static_cast<I2CPorts*>(context)->Cancel(request);
}
void service(void* context) {
    static_cast<I2CPorts*>(context)->Service();
}
bool matches(const void* context, const nx_i2c_port_t* port) {
    return static_cast<const I2CPorts*>(context)->Matches(port);
}
class SMBus : public testing::Test {
  protected:
    testing::NiceMock<I2CPorts> hardware;
    nx_i2c_endpoint_ops_t endpoint_ops{};
    nx_i2c_ops_t controller_ops{};
    const nx_i2c_endpoint_t endpoint{&endpoint_ops, &hardware};
    const nx_i2c_port_t controller{&controller_ops, &hardware};
    const nx_smbus_binding_t binding{&endpoint, &controller, 0x2AU};
    nx_smbus_t owner{};
    nx_smbus_request_t request{};
    nx_i2c_request_t* executing{};
    std::array<uint8_t, 34> tx{{0x10U, 0x20U, 0x30U, 0xEEU}};
    std::array<uint8_t, 34> rx{};
    void SetUp() override {
        now = 100U;
        in_isr = masked = false;
        endpoint_ops.submit = submit;
        endpoint_ops.on_port = matches;
        controller_ops.cancel = cancel;
        controller_ops.service = service;
        ON_CALL(hardware, Matches(&controller))
            .WillByDefault(testing::Return(true));
        ON_CALL(hardware, Submit(testing::_))
            .WillByDefault(testing::Invoke([this](nx_i2c_request_t* inner) {
                executing = inner;
                return nx_request_admit(&inner->base, NX_REQUEST_ACTIVE);
            }));
        ON_CALL(hardware, Cancel(testing::_))
            .WillByDefault(testing::Return(NX_SUCCESS));
        ASSERT_EQ(NX_SUCCESS, nx_smbus_initialize(&owner, &binding));
        nx_request_initialize(&request.base);
    }
    void Write() {
        ASSERT_EQ(NX_SUCCESS,
                  nx_smbus_prepare(&owner, &request,
                                   nx_smbus_tx_t{tx.data(), 3U, &tx[3]},
                                   nx_smbus_rx_t{}, 1000U));
    }
    void Read(bool combined = false) {
        tx[0] = 0x07U;
        ASSERT_EQ(
            NX_SUCCESS,
            nx_smbus_prepare(&owner, &request,
                             combined ? nx_smbus_tx_t{tx.data(), 1U, nullptr}
                                      : nx_smbus_tx_t{},
                             nx_smbus_rx_t{rx.data(), 2U, rx.size()}, 1000U));
    }
    nx_result_t Complete(nx_result_t result, size_t bytes) {
        auto* inner = executing;
        executing = nullptr;
        EXPECT_NE(nullptr, inner);
        if (inner != nullptr)
            nx_request_settle(&inner->base, result, bytes);
        return nx_smbus_service(&owner);
    }
};

TEST_F(SMBus, InitializationChecksOneBindingWithoutOpeningTheBus) {
    nx_smbus_t fresh{};
    auto invalid = binding;
    invalid.address = 0x80U;
    EXPECT_CALL(hardware, Submit(testing::_)).Times(0);
    EXPECT_CALL(hardware, Service()).Times(0);
    EXPECT_EQ(NX_ERROR_INVALID, nx_smbus_initialize(&fresh, &invalid));
    invalid = binding;
    EXPECT_CALL(hardware, Matches(&controller))
        .WillOnce(testing::Return(false));
    EXPECT_EQ(NX_ERROR_INVALID, nx_smbus_initialize(&fresh, &invalid));
    EXPECT_EQ(nullptr, fresh.binding);
}

TEST_F(SMBus, ConstPayloadAndExplicitWritablePecTailHaveSeparateRights) {
    Write();
    EXPECT_EQ(0xEEU, tx[3]);
    EXPECT_CALL(hardware, Submit(testing::_))
        .WillOnce(testing::Invoke([this](nx_i2c_request_t* inner) {
            EXPECT_NE(NX_REQUEST_READY, nx_request_state(&request.base));
            EXPECT_EQ(1U, inner->count);
            EXPECT_FALSE(inner->messages[0].read);
            EXPECT_EQ(tx.data(), inner->messages[0].buffer.tx);
            EXPECT_EQ(4U, inner->messages[0].length);
            EXPECT_EQ(0x38U, tx[3]);
            executing = inner;
            return nx_request_admit(&inner->base, NX_REQUEST_ACTIVE);
        }));
    ASSERT_EQ(NX_SUCCESS, nx_smbus_submit(&owner, &request));
    EXPECT_THAT((std::array<uint8_t, 3>{{tx[0], tx[1], tx[2]}}),
                testing::ElementsAre(0x10U, 0x20U, 0x30U));
    ASSERT_EQ(NX_SUCCESS, Complete(NX_SUCCESS, 4U));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(3U, request.base.transferred);
    EXPECT_EQ(NX_SMBUS_PEC_NOT_CHECKED, request.pec);
    EXPECT_EQ(nullptr, owner.active);
}

TEST_F(SMBus, ReceiveChecksTheAddressAndExcludesPecFromPayloadCount) {
    Read();
    ASSERT_EQ(NX_SUCCESS, nx_smbus_submit(&owner, &request));
    ASSERT_NE(nullptr, executing);
    EXPECT_EQ(1U, executing->count);
    EXPECT_TRUE(executing->messages[0].read);
    EXPECT_EQ(3U, executing->messages[0].length);
    rx[0] = 1U;
    rx[1] = 2U;
    rx[2] = 0xFFU;
    ASSERT_EQ(NX_SUCCESS, Complete(NX_SUCCESS, 3U));
    EXPECT_EQ(NX_SMBUS_PEC_VALID, request.pec);
    EXPECT_EQ(2U, request.base.transferred);
}

TEST_F(SMBus, WriteReadPreservesOneRepeatedStartAndOneTerminalPec) {
    Read(true);
    ASSERT_EQ(NX_SUCCESS, nx_smbus_submit(&owner, &request));
    ASSERT_NE(nullptr, executing);
    ASSERT_EQ(2U, executing->count);
    EXPECT_FALSE(executing->messages[0].read);
    EXPECT_EQ(1U, executing->messages[0].length);
    EXPECT_TRUE(executing->messages[1].read);
    EXPECT_EQ(3U, executing->messages[1].length);
    rx[0] = 0x34U;
    rx[1] = 0x12U;
    rx[2] = 0x20U;
    ASSERT_EQ(NX_SUCCESS, Complete(NX_SUCCESS, 4U));
    EXPECT_EQ(NX_SMBUS_PEC_VALID, request.pec);
    EXPECT_EQ(3U, request.base.transferred);
}

TEST_F(SMBus, PecMismatchNeverBecomesASuccessfulPayload) {
    Read();
    ASSERT_EQ(NX_SUCCESS, nx_smbus_submit(&owner, &request));
    rx[0] = 1U;
    rx[1] = 2U;
    rx[2] = 0xFEU;
    EXPECT_EQ(NX_ERROR_IO, Complete(NX_SUCCESS, 3U));
    EXPECT_EQ(NX_ERROR_IO, request.base.result);
    EXPECT_EQ(NX_SMBUS_PEC_ERROR, request.pec);
    EXPECT_EQ(0U, request.base.transferred);
}

TEST_F(SMBus, TransportErrorsAndShortSuccessCannotValidatePec) {
    for (auto result : {NX_ERROR_NACK, NX_ERROR_ARBITRATION, NX_SUCCESS}) {
        Read();
        ASSERT_EQ(NX_SUCCESS, nx_smbus_submit(&owner, &request));
        rx[0] = 1U;
        rx[1] = 2U;
        rx[2] = 0xFFU;
        EXPECT_EQ(result == NX_SUCCESS ? NX_ERROR_IO : result,
                  Complete(result, 2U));
        EXPECT_EQ(NX_SMBUS_PEC_NOT_CHECKED, request.pec);
        EXPECT_EQ(0U, request.base.transferred);
    }
}

TEST_F(SMBus, RejectedArgumentsAndExpiredAdmissionKeepPecAndPortsUnchanged) {
    Write();
    EXPECT_CALL(hardware, Submit(testing::_)).Times(0);
    now = 1000U;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_smbus_submit(&owner, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(0xEEU, tx[3]);
    EXPECT_EQ(nullptr, owner.active);
    now = 100U;
    for (auto invalid : {nx_smbus_tx_t{tx.data(), 33U, &tx[33]},
                         nx_smbus_tx_t{tx.data(), 3U, &tx[4]},
                         nx_smbus_tx_t{tx.data(), 3U, nullptr}})
        EXPECT_EQ(NX_ERROR_INVALID, nx_smbus_prepare(&owner, &request, invalid,
                                                     nx_smbus_rx_t{}, 1000U));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_smbus_prepare(&owner, &request, nx_smbus_tx_t{},
                               nx_smbus_rx_t{rx.data(), 2U, 2U}, 1000U));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_smbus_prepare(&owner, &request, nx_smbus_tx_t{},
                               nx_smbus_rx_t{reinterpret_cast<uint8_t*>(&owner),
                                             2U, sizeof(owner)},
                               1000U));
}

TEST_F(SMBus, InnerRejectionAfterAcceptanceSettlesWithoutLeakingPointers) {
    Write();
    EXPECT_CALL(hardware, Submit(testing::_))
        .WillOnce(testing::Return(NX_ERROR_BUSY));
    EXPECT_EQ(NX_SUCCESS, nx_smbus_submit(&owner, &request));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_BUSY, request.base.result);
    EXPECT_EQ(nullptr, owner.active);
    EXPECT_EQ(nullptr, owner.transfer.messages);
}

TEST_F(SMBus, CancelAndFailedDrainRetainAllWireStorageUntilActualSettlement) {
    Write();
    ASSERT_EQ(NX_SUCCESS, nx_smbus_submit(&owner, &request));
    ASSERT_NE(nullptr, executing);
    EXPECT_EQ(NX_SUCCESS, nx_smbus_cancel(&owner, &request));
    ASSERT_EQ(NX_SUCCESS,
              nx_request_transition(&executing->base, NX_REQUEST_DRAINING));
    ASSERT_EQ(NX_SUCCESS,
              nx_request_transition(&executing->base, NX_REQUEST_QUARANTINED));
    EXPECT_EQ(NX_ERROR_BUSY, nx_smbus_service(&owner));
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(&request, owner.active);
    EXPECT_EQ(tx.data(), owner.transfer.messages[0].buffer.tx);
    EXPECT_EQ(NX_ERROR_BUSY, nx_smbus_stop(&owner));
    EXPECT_EQ(NX_ERROR_CANCELLED, Complete(NX_ERROR_CANCELLED, 1U));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(nullptr, owner.transfer.messages);
    EXPECT_EQ(NX_SUCCESS, nx_smbus_stop(&owner));
}

TEST_F(SMBus, ASettledTransportWinsCancelAndStoppingDoesNotStopTheSharedBus) {
    Write();
    ASSERT_EQ(NX_SUCCESS, nx_smbus_submit(&owner, &request));
    nx_request_settle(&executing->base, NX_SUCCESS, 4U);
    executing = nullptr;
    EXPECT_CALL(hardware, Cancel(testing::_)).Times(0);
    EXPECT_EQ(NX_SUCCESS, nx_smbus_cancel(&owner, &request));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(NX_SUCCESS, nx_smbus_stop(&owner));
}

TEST_F(SMBus, AColdRestartInvalidatesPreparedRequestsWithoutTouchingThePec) {
    Write();
    ASSERT_EQ(NX_SUCCESS, nx_smbus_stop(&owner));
    ASSERT_EQ(NX_SUCCESS, nx_smbus_initialize(&owner, &binding));
    EXPECT_CALL(hardware, Submit(testing::_)).Times(0);
    EXPECT_EQ(NX_ERROR_STATE, nx_smbus_submit(&owner, &request));
    EXPECT_EQ(0xEEU, tx[3]);
}

TEST_F(SMBus, RepeatedSuccessfulStopDoesNotCallOrReopenTheTransport) {
    EXPECT_CALL(hardware, Cancel(testing::_)).Times(0);
    EXPECT_CALL(hardware, Service()).Times(0);
    EXPECT_EQ(NX_SUCCESS, nx_smbus_stop(&owner));
    const uint64_t generation = owner.generation;
    EXPECT_EQ(NX_SUCCESS, nx_smbus_stop(&owner));
    EXPECT_EQ(generation, owner.generation);
    EXPECT_EQ(nullptr, owner.binding);
}

TEST_F(SMBus, TransportCallbackReentryCannotCloseOrResubmitThePacket) {
    Write();
    EXPECT_CALL(hardware, Submit(testing::_))
        .WillOnce(testing::Invoke([this](nx_i2c_request_t* inner) {
            EXPECT_EQ(NX_ERROR_BUSY, nx_smbus_stop(&owner));
            EXPECT_EQ(NX_ERROR_BUSY, nx_smbus_submit(&owner, &request));
            EXPECT_EQ(NX_ERROR_BUSY, nx_smbus_service(&owner));
            executing = inner;
            return nx_request_admit(&inner->base, NX_REQUEST_ACTIVE);
        }));
    ASSERT_EQ(NX_SUCCESS, nx_smbus_submit(&owner, &request));
    EXPECT_FALSE(owner.closing);
    EXPECT_EQ(NX_SUCCESS, Complete(NX_SUCCESS, 4U));
}

TEST_F(SMBus, WrongContextAndASecondOwnerCannotStealAnAcceptedRequest) {
    Write();
    in_isr = true;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_smbus_submit(&owner, &request));
    in_isr = false;
    ASSERT_EQ(NX_SUCCESS, nx_smbus_submit(&owner, &request));
    EXPECT_EQ(NX_ERROR_BUSY, nx_smbus_submit(&owner, &request));
    nx_smbus_t second{};
    ASSERT_EQ(NX_SUCCESS, nx_smbus_initialize(&second, &binding));
    EXPECT_EQ(NX_ERROR_STATE, nx_smbus_submit(&second, &request));
    EXPECT_EQ(&request, owner.active);
    EXPECT_EQ(nullptr, second.active);
    EXPECT_EQ(NX_SUCCESS, Complete(NX_SUCCESS, 4U));
}
} /* namespace */

/** \brief The mocked external time source belongs to the test transport. */
extern "C" nx_time_us_t nx_time_now_us(void) {
    return now;
}
/** \brief CPU exception identity is independent of the adapter under test. */
extern "C" bool nx_arch_in_isr(void) {
    return in_isr;
}
/** \brief Observe the external CPU mask without manufacturing adapter state. */
extern "C" bool nx_arch_irq_is_masked(void) {
    return masked;
}
