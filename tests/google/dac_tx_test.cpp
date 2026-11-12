/**
 * \file            dac_tx_test.cpp
 * \brief           Borrowed DAC frames and exact shared-method dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/dac_tx.h"
#include <array>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
class Engine {
  public:
    MOCK_METHOD(nx_result_t, Submit, (nx_dac_tx_request_t*));
    MOCK_METHOD(nx_result_t, Cancel, (nx_dac_tx_request_t*));
    MOCK_METHOD(void, Service, ());
    MOCK_METHOD(nx_result_t, AttachWake, (const nx_irq_wake_t*, uint8_t));
    MOCK_METHOD(nx_time_us_t, NextService, ());
};
nx_result_t submit(void* context, nx_dac_tx_request_t* request) {
    return static_cast<Engine*>(context)->Submit(request);
}
nx_result_t cancel(void* context, nx_dac_tx_request_t* request) {
    return static_cast<Engine*>(context)->Cancel(request);
}
void service(void* context) {
    static_cast<Engine*>(context)->Service();
}
nx_result_t attach(void* context, const nx_irq_wake_t* wake, uint8_t ceiling) {
    return static_cast<Engine*>(context)->AttachWake(wake, ceiling);
}
nx_time_us_t next_service(const void* context) {
    return const_cast<Engine*>(static_cast<const Engine*>(context))
        ->NextService();
}
const nx_dac_tx_ops_t operations{submit, cancel, service, attach, next_service};

class DacTxContract : public testing::Test {
  protected:
    testing::StrictMock<Engine> first;
    testing::StrictMock<Engine> second;
    const nx_dac_tx_port_t one{&operations, &first};
    const nx_dac_tx_port_t two{&operations, &second};
    nx_dac_tx_request_t request{};
    const uint32_t frames[2] = {0x04560123U, 0x06780345U};
    void SetUp() override {
        nx_request_initialize(&request.base);
    }
};

TEST_F(DacTxContract, PreparationKeepsExactPayloadWithoutCopyOrAdmission) {
    ASSERT_EQ(
        nx_dac_tx_prepare(&request, frames, sizeof(frames), 48000U, 123456U),
        NX_SUCCESS);
    EXPECT_EQ(request.data, frames);
    EXPECT_EQ(request.length, sizeof(frames));
    EXPECT_EQ(request.trigger_hz, 48000U);
    EXPECT_EQ(request.base.deadline, 123456U);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(frames[0], 0x04560123U);
}
TEST_F(DacTxContract, InvalidPreparationDoesNotModifyUnborrowedMetadata) {
    request.data = frames;
    request.length = 7U;
    request.trigger_hz = 10U;
    std::array<unsigned char, sizeof(request)> original{};
    std::memcpy(original.data(), &request, sizeof(request));
    EXPECT_EQ(nx_dac_tx_prepare(&request, nullptr, 4U, 1U, 4U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_dac_tx_prepare(&request, frames, 0U, 1U, 4U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_dac_tx_prepare(nullptr, frames, 4U, 1U, 4U), NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&request, original.data(), sizeof(request)), 0);
}
TEST_F(DacTxContract, DescriptorCannotAliasPayloadOrWrapItsExtent) {
    EXPECT_EQ(nx_dac_tx_prepare(&request, &request, sizeof(request), 1U, 4U),
              NX_ERROR_INVALID);
    const auto* wrapping = reinterpret_cast<const void*>(UINTPTR_MAX - 1U);
    EXPECT_EQ(nx_dac_tx_prepare(&request, wrapping, 8U, 1U, 4U),
              NX_ERROR_INVALID);
}
TEST_F(DacTxContract, MisalignedDescriptorRejectsBeforeFieldAccess) {
    alignas(nx_dac_tx_request_t)
        std::array<unsigned char, sizeof(nx_dac_tx_request_t) + 1U>
            bytes{};
    auto* misaligned = reinterpret_cast<nx_dac_tx_request_t*>(bytes.data() + 1);
    EXPECT_EQ(nx_dac_tx_prepare(misaligned, frames, sizeof(frames), 1U, 4U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_dac_tx_port_submit(&one, misaligned), NX_ERROR_INVALID);
    EXPECT_EQ(nx_dac_tx_port_cancel(&one, misaligned), NX_ERROR_INVALID);
}
TEST_F(DacTxContract, PreparationNeverRevokesAnActiveFrameLoan) {
    ASSERT_EQ(nx_request_admit(&request.base, NX_REQUEST_ACTIVE), NX_SUCCESS);
    request.data = frames;
    request.length = sizeof(frames);
    std::array<unsigned char, sizeof(request)> original{};
    std::memcpy(original.data(), &request, sizeof(request));
    EXPECT_EQ(nx_dac_tx_prepare(&request, frames, sizeof(frames), 1U, 4U),
              NX_ERROR_STATE);
    EXPECT_EQ(std::memcmp(&request, original.data(), sizeof(request)), 0);
}
TEST_F(DacTxContract, AConsumedSettlementCanPrepareAnotherImmutableSpan) {
    ASSERT_EQ(nx_request_admit(&request.base, NX_REQUEST_ACTIVE), NX_SUCCESS);
    nx_request_settle(&request.base, NX_SUCCESS, 4U);
    nx_result_t result;
    size_t transferred;
    ASSERT_EQ(nx_request_result(&request.base, &result, &transferred),
              NX_SUCCESS);
    EXPECT_EQ(nx_dac_tx_prepare(&request, frames, sizeof(frames), 1U,
                                NX_DEADLINE_NEVER),
              NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
}
TEST_F(DacTxContract, SharedOperationsUseTheExactSelectedInstance) {
    EXPECT_CALL(first, Submit(&request)).WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_CALL(second, Cancel(&request))
        .WillOnce(testing::Return(NX_ERROR_BUSY));
    EXPECT_CALL(second, Service());
    EXPECT_EQ(nx_dac_tx_port_submit(&one, &request), NX_SUCCESS);
    EXPECT_EQ(nx_dac_tx_port_cancel(&two, &request), NX_ERROR_BUSY);
    nx_dac_tx_port_service(&two);
}
TEST_F(DacTxContract, ARejectedProviderResultNeverBecomesAdmission) {
    EXPECT_CALL(first, Submit(&request))
        .WillOnce(testing::Return(NX_ERROR_BUSY));
    EXPECT_EQ(nx_dac_tx_port_submit(&one, &request), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
}
TEST_F(DacTxContract, MissingOptionalMethodsKeepStorageUnborrowed) {
    const nx_dac_tx_ops_t absent{};
    const nx_dac_tx_port_t face{&absent, &first};
    EXPECT_EQ(nx_dac_tx_port_submit(&face, &request), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_dac_tx_port_cancel(&face, &request), NX_ERROR_UNSUPPORTED);
    nx_dac_tx_port_service(&face);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
}
TEST_F(DacTxContract, InvalidFacesAndMissingDescriptorsNeverEnterProvider) {
    const nx_dac_tx_port_t no_methods{nullptr, &first};
    const nx_dac_tx_port_t no_context{&operations, nullptr};
    const nx_dac_tx_port_t* invalid[] = {nullptr, &no_methods, &no_context};
    for (const nx_dac_tx_port_t* face : invalid) {
        EXPECT_EQ(nx_dac_tx_port_submit(face, &request), NX_ERROR_INVALID);
        EXPECT_EQ(nx_dac_tx_port_cancel(face, &request), NX_ERROR_INVALID);
        nx_dac_tx_port_service(face);
    }
    EXPECT_EQ(nx_dac_tx_port_submit(&one, nullptr), NX_ERROR_INVALID);
    EXPECT_EQ(nx_dac_tx_port_cancel(&one, nullptr), NX_ERROR_INVALID);
}
TEST_F(DacTxContract, DescriptorCannotOverwriteItsOwnFaceOrMethods) {
    auto* face_alias = reinterpret_cast<nx_dac_tx_request_t*>(
        const_cast<nx_dac_tx_port_t*>(&one));
    auto* method_alias = reinterpret_cast<nx_dac_tx_request_t*>(
        const_cast<nx_dac_tx_ops_t*>(&operations));
    EXPECT_EQ(nx_dac_tx_port_submit(&one, face_alias), NX_ERROR_INVALID);
    EXPECT_EQ(nx_dac_tx_port_cancel(&one, method_alias), NX_ERROR_INVALID);
}
TEST_F(DacTxContract, ExplicitWakeBindingAndDetachKeepTheExactInstance) {
    const nx_irq_wake_t wake{&first, nullptr, false};
    EXPECT_CALL(first, AttachWake(&wake, 5U))
        .WillOnce(testing::Return(NX_ERROR_PERMISSION));
    EXPECT_CALL(second, AttachWake(nullptr, 0U))
        .WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_EQ(nx_dac_tx_port_attach_wake(&one, &wake, 5U), NX_ERROR_PERMISSION);
    EXPECT_EQ(nx_dac_tx_port_attach_wake(&two, nullptr, 0U), NX_SUCCESS);
}
TEST_F(DacTxContract, NextServiceExposesTheActualAbsoluteDeadline) {
    EXPECT_CALL(first, NextService()).WillOnce(testing::Return(123456U));
    EXPECT_CALL(second, NextService())
        .WillOnce(testing::Return(NX_DEADLINE_NEVER));
    EXPECT_EQ(nx_dac_tx_port_next_service(&one), 123456U);
    EXPECT_EQ(nx_dac_tx_port_next_service(&two), NX_DEADLINE_NEVER);
}
TEST_F(DacTxContract, InvalidOrAbsentSchedulingMethodsCannotCallAPublisher) {
    const nx_dac_tx_ops_t absent{};
    const nx_dac_tx_port_t no_methods{&absent, &first};
    const nx_dac_tx_port_t no_context{&operations, nullptr};
    const nx_dac_tx_port_t* invalid[] = {nullptr, &no_context};
    for (const auto* face : invalid) {
        EXPECT_EQ(nx_dac_tx_port_attach_wake(face, nullptr, 0U),
                  NX_ERROR_INVALID);
        EXPECT_EQ(nx_dac_tx_port_next_service(face), NX_DEADLINE_NEVER);
    }
    EXPECT_EQ(nx_dac_tx_port_attach_wake(&no_methods, nullptr, 0U),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_dac_tx_port_next_service(&no_methods), NX_DEADLINE_NEVER);
}
TEST_F(DacTxContract, ZeroRatePreservesAnExplicitUnknownExternalFrequency) {
    ASSERT_EQ(nx_dac_tx_prepare(&request, frames, sizeof(frames), 0U, 123456U),
              NX_SUCCESS);
    EXPECT_EQ(request.trigger_hz, 0U);
    EXPECT_EQ(request.data, frames);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
}
} /* namespace */
