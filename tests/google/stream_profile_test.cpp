/**
 * \file            stream_profile_test.cpp
 * \brief           Actual cold budget checks before an external I/O boundary
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/io/stream_profile.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

class StreamStartBoundary {
  public:
    MOCK_METHOD(nx_result_t, UartStart, (nx_stream_t*));
    MOCK_METHOD(nx_result_t, AdcStart, (nx_stream_t*, uint32_t));
    MOCK_METHOD(nx_result_t, AdcInfo, (nx_adc_info_t*));
};
static nx_result_t UartStart(void* context, nx_stream_t* stream) {
    return static_cast<StreamStartBoundary*>(context)->UartStart(stream);
}
static nx_result_t AdcStart(void* context, nx_stream_t* stream, uint32_t hz) {
    return static_cast<StreamStartBoundary*>(context)->AdcStart(stream, hz);
}
static nx_result_t AdcInfo(const void* context, nx_adc_info_t* info) {
    return const_cast<StreamStartBoundary*>(
               static_cast<const StreamStartBoundary*>(context))
        ->AdcInfo(info);
}
class StreamProfile : public testing::Test {
  protected:
    testing::StrictMock<StreamStartBoundary> boundary;
    nx_uart_ops_t uart_ops{};
    nx_adc_ops_t adc_ops{};
    const nx_uart_port_t uart{&uart_ops, &boundary};
    const nx_adc_port_t adc{&adc_ops, &boundary};
    uint8_t bytes[8][400]{};
    nx_stream_slot_t slots[8]{};
    nx_stream_t stream{};
    nx_uart_stream_profile_t uart_profile{
        &uart, {1000U, 100U, 1U, 500000U, 99999U, 2U, 1U, 8U}};
    nx_adc_stream_profile_t adc_profile{
        &adc, {1000U, 100U, 4U, 500000U, 99999U, 2U, 1U, 8U}};
    void SetUp() override {
        uart_ops.rx_start = UartStart;
        adc_ops.stream_start = AdcStart;
        adc_ops.info = AdcInfo;
        Initialize(100U);
    }
    void Initialize(size_t capacity) {
        for (size_t i = 0U; i < 8U; ++i) {
            slots[i] = {};
            slots[i].data = bytes[i];
            slots[i].capacity = capacity;
        }
        ASSERT_EQ(NX_SUCCESS, nx_stream_initialize(&stream, slots, 8U));
    }
};
TEST_F(StreamProfile, GoodColdCheckCallsBoundUartExactlyOnceAndReturnsResult) {
    EXPECT_CALL(boundary, UartStart(&stream))
        .WillOnce(testing::Return(NX_ERROR_BUSY));
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_uart_stream_profile_start(&uart_profile, &stream));
    EXPECT_EQ(0U, stream.outstanding);
    EXPECT_EQ(0U, stream.epoch);
}
TEST_F(StreamProfile, SmallerActualPoolRejectsWithoutCallingProvider) {
    stream.count = 3U;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_uart_stream_profile_start(&uart_profile, &stream));
    EXPECT_EQ(0U, stream.outstanding);
    EXPECT_EQ(0U, stream.epoch);
}
TEST_F(StreamProfile, EveryActualSlotMustMatchDeclaredByteExtent) {
    for (size_t i = 0U; i < 8U; ++i) {
        slots[i].capacity = 99U;
        EXPECT_EQ(NX_ERROR_INVALID,
                  nx_uart_stream_profile_start(&uart_profile, &stream));
        slots[i].capacity = 100U;
    }
}
TEST_F(StreamProfile, ActualTargetsMustRemainDisjointAfterPoolInitialization) {
    for (size_t offset : {0U, 1U, 99U}) {
        slots[1].data = slots[0].data + offset;
        EXPECT_EQ(NX_ERROR_INVALID,
                  nx_uart_stream_profile_start(&uart_profile, &stream));
        EXPECT_EQ(0U, stream.outstanding);
        EXPECT_EQ(0U, stream.epoch);
    }
}
TEST_F(StreamProfile, InsufficientDeclaredCapacityAndLatencyFailClosed) {
    uart_profile.budget.configured_blocks = 7U;
    EXPECT_EQ(NX_ERROR_EXHAUSTED,
              nx_uart_stream_profile_start(&uart_profile, &stream));
    uart_profile.budget.configured_blocks = 8U;
    uart_profile.budget.target_delay_us = 100000U;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_uart_stream_profile_start(&uart_profile, &stream));
}
TEST_F(StreamProfile, LiveOrStoppingPoolIsNeverResetToFitBudget) {
    stream.stopping = true;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_uart_stream_profile_start(&uart_profile, &stream));
    stream.stopping = false;
    nx_stream_fill_t filling{};
    ASSERT_EQ(NX_SUCCESS, nx_stream_reserve(&stream, &filling));
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_uart_stream_profile_start(&uart_profile, &stream));
    EXPECT_EQ(1U, stream.outstanding);
}
TEST_F(StreamProfile, WrongGeometryCannotForgeSingleTargetContinuousMode) {
    uart_profile.budget.hardware_targets = 1U;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_uart_stream_profile_start(&uart_profile, &stream));
    uart_profile.budget.hardware_targets = 2U;
    uart_profile.budget.unit_bytes = 2U;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_uart_stream_profile_start(&uart_profile, &stream));
}
TEST_F(StreamProfile, PayloadCannotOverwriteReadonlyProfileDuringDma) {
    slots[0].data = reinterpret_cast<uint8_t*>(&uart_profile);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_uart_stream_profile_start(&uart_profile, &stream));
}
TEST_F(StreamProfile, MaskedTaskRejectsBeforeCallingExternalPort) {
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    EXPECT_EQ(NX_ERROR_CONTEXT,
              nx_uart_stream_profile_start(&uart_profile, &stream));
    nx_arch_irq_restore(saved);
}
TEST_F(StreamProfile, AdcRateExceedingEnvelopeRejectsBeforeInfoOrStart) {
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_adc_stream_profile_start(&adc_profile, &stream, 1001U));
}
TEST_F(StreamProfile, AdcUsesActualWholeScanStorageBeforeStarting) {
    Initialize(400U);
    EXPECT_CALL(boundary, AdcInfo(testing::_))
        .WillOnce(testing::Invoke([](nx_adc_info_t* info) {
            *info = {12U, 3300U, 2U};
            return NX_SUCCESS;
        }));
    EXPECT_CALL(boundary, AdcStart(&stream, 1000U))
        .WillOnce(testing::Return(NX_ERROR_IO));
    EXPECT_EQ(NX_ERROR_IO,
              nx_adc_stream_profile_start(&adc_profile, &stream, 1000U));
}
TEST_F(StreamProfile, AdcProfileCannotUnderstateSelectedScanByteWidth) {
    adc_profile.budget.unit_bytes = 2U;
    EXPECT_CALL(boundary, AdcInfo(testing::_))
        .WillOnce(testing::Invoke([](nx_adc_info_t* info) {
            *info = {12U, 3300U, 2U};
            return NX_SUCCESS;
        }));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_adc_stream_profile_start(&adc_profile, &stream, 1000U));
}
