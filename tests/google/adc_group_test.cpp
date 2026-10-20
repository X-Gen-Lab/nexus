/**
 * \file            adc_group_test.cpp
 * \brief           Finite ADC group caller storage and dispatch contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/adc_group.h"
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
namespace {
TEST(ADCGroupAPI, PreparationCopiesOnlyUnborrowedRequestMetadata) {
    nx_adc_group_request_t request{};
    uint16_t data[6]{11U, 22U, 33U, 44U, 55U, 66U};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_adc_group_prepare(&request, data, sizeof(data), 3U, 1000U),
              NX_SUCCESS);
    EXPECT_EQ(request.frames, data);
    EXPECT_EQ(request.bytes, sizeof(data));
    EXPECT_EQ(request.frame_count, 3U);
    EXPECT_EQ(request.base.deadline, 1000U);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(data[0], 11U);
    EXPECT_EQ(data[5], 66U);
}
TEST(ADCGroupAPI, BorrowedRequestCannotBeRepreparedToRevokeLoan) {
    nx_adc_group_request_t request{};
    uint16_t data[6]{};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_adc_group_prepare(&request, data, sizeof(data), 3U, 1000U),
              NX_SUCCESS);
    ASSERT_EQ(nx_request_admit(&request.base, NX_REQUEST_ACTIVE), NX_SUCCESS);
    const nx_adc_group_request_t before = request;
    EXPECT_EQ(nx_adc_group_prepare(&request, data, sizeof(data), 3U, 2000U),
              NX_ERROR_STATE);
    EXPECT_EQ(std::memcmp(&request, &before, sizeof(before)), 0);
    nx_request_settle(&request.base, NX_ERROR_CANCELLED, 0U);
}
TEST(ADCGroupAPI, ZeroOrUnboundedPreparationDoesNotMutateReadyStorage) {
    nx_adc_group_request_t request{};
    uint16_t data[2]{};
    nx_request_initialize(&request.base);
    const nx_adc_group_request_t before = request;
    EXPECT_EQ(nx_adc_group_prepare(&request, data, sizeof(data), 0U, 1000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_adc_group_prepare(&request, data, sizeof(data), 1U,
                                   NX_DEADLINE_NEVER),
              NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&request, &before, sizeof(before)), 0);
}
TEST(ADCGroupAPI, WrappingPayloadCannotBeAdmittedByPreparation) {
    nx_adc_group_request_t request{};
    nx_request_initialize(&request.base);
    void* data = reinterpret_cast<void*>(UINTPTR_MAX - 1U);
    EXPECT_EQ(nx_adc_group_prepare(&request, data, 4U, 1U, 1000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(request.frames, nullptr);
}
TEST(ADCGroupAPI, RequestCannotBeItsOwnWritablePayload) {
    nx_adc_group_request_t request{};
    nx_request_initialize(&request.base);
    EXPECT_EQ(
        nx_adc_group_prepare(&request, &request, sizeof(request), 1U, 1000U),
        NX_ERROR_INVALID);
    EXPECT_EQ(request.frames, nullptr);
}
TEST(ADCGroupAPI, MissingSelectedMethodsHaveExplicitUnsupportedResult) {
    const nx_adc_group_ops_t operations{};
    unsigned context = 0U;
    const nx_adc_group_port_t port{&operations, &context};
    nx_adc_group_info_t info{};
    nx_adc_group_request_t request{};
    nx_request_initialize(&request.base);
    EXPECT_EQ(nx_adc_group_submit(&port, &request), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_adc_group_cancel(&port, &request), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_adc_group_service(&port), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_adc_group_stop(&port), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_adc_group_info(&port, &info), NX_ERROR_UNSUPPORTED);
}
TEST(ADCGroupAPI, OutputOverlappingImmutableFaceRejectsBeforeDispatch) {
    testing::StrictMock<
        testing::MockFunction<nx_result_t(nx_adc_group_info_t*)>>
        dependency;
    nx_adc_group_ops_t operations{};
    operations.info = [](const void* context, nx_adc_group_info_t* output) {
        auto* callback = static_cast<
            testing::MockFunction<nx_result_t(nx_adc_group_info_t*)>*>(
            const_cast<void*>(context));
        return callback->Call(output);
    };
    nx_adc_group_port_t port{&operations, &dependency};
    EXPECT_CALL(dependency, Call(testing::_)).Times(0);
    EXPECT_EQ(
        nx_adc_group_info(&port, reinterpret_cast<nx_adc_group_info_t*>(&port)),
        NX_ERROR_INVALID);
}
TEST(ADCGroupAPI, SameReadOnlyOperationsPassEachInstancesExactContext) {
    testing::StrictMock<testing::MockFunction<nx_result_t()>> first;
    testing::StrictMock<testing::MockFunction<nx_result_t()>> second;
    nx_adc_group_ops_t operations{};
    operations.service = [](void* context) {
        return static_cast<testing::MockFunction<nx_result_t()>*>(context)
            ->Call();
    };
    const nx_adc_group_port_t a{&operations, &first};
    const nx_adc_group_port_t b{&operations, &second};
    EXPECT_CALL(first, Call()).WillOnce(testing::Return(NX_ERROR_BUSY));
    EXPECT_CALL(second, Call()).WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_EQ(nx_adc_group_service(&a), NX_ERROR_BUSY);
    EXPECT_EQ(nx_adc_group_service(&b), NX_SUCCESS);
}
} /* namespace */
