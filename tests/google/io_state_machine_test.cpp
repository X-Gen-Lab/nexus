/**
 * \file            io_state_machine_test.cpp
 * \brief           Cross-descriptor epochs and explicit recovery sequences
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/request_observer.h"
#include "nexus/io/i2c.h"
#include "nexus/io/spi.h"
#include "nexus/io/stream_budget.h"
#include "nexus/io/uart.h"
#include "provider.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
/** \brief Execute production request helpers on a real public descriptor base.
 */
void verify_sequence(nx_request_t* request) {
    nx_request_slot_t slot{};
    uint64_t epoch{};
    ASSERT_EQ(nx_request_slot_bind(&slot, request, &epoch), NX_SUCCESS);
    nx_request_observer_t observer{};
    ASSERT_EQ(nx_request_observer_bind(&observer, &slot, epoch), NX_SUCCESS);
    ASSERT_EQ(nx_request_admit(request, NX_REQUEST_ACTIVE), NX_SUCCESS);
    ASSERT_EQ(nx_request_transition(request, NX_REQUEST_DRAINING), NX_SUCCESS);
    ASSERT_EQ(nx_request_transition(request, NX_REQUEST_QUARANTINED),
              NX_SUCCESS);
    EXPECT_EQ(nx_request_prepare(request, 1U), NX_ERROR_STATE);
    EXPECT_EQ(nx_request_slot_release(&slot, epoch), NX_ERROR_BUSY);
    nx_request_observation_t out{};
    ASSERT_EQ(nx_request_observer_sample(&observer, 2000U, &out), NX_SUCCESS);
    EXPECT_FALSE(out.settled);
    EXPECT_EQ(out.state, NX_REQUEST_QUARANTINED);
    nx_request_settle(request, NX_ERROR_CANCELLED, 2U);
    nx_request_settle(request, NX_SUCCESS, 99U);
    ASSERT_EQ(nx_request_observer_sample(&observer, 2010U, &out), NX_SUCCESS);
    EXPECT_EQ(out.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(out.transferred, 2U);
    ASSERT_EQ(nx_request_slot_release(&slot, epoch), NX_SUCCESS);
    slot.epoch = UINT64_MAX;
    uint64_t rejected = 77U;
    EXPECT_EQ(nx_request_slot_bind(&slot, request, &rejected),
              NX_ERROR_EXHAUSTED);
    EXPECT_EQ(slot.request, nullptr);
    EXPECT_EQ(rejected, 77U);
    EXPECT_EQ(nx_request_observer_sample(&observer, 2020U, &out),
              NX_ERROR_STATE);
    nx_request_observer_detach(&observer);
}
TEST(IODescriptorSequences, UartCancellationAndEpochDoNotRevokeBorrow) {
    nx_uart_tx_request_t request{};
    nx_request_initialize(&request.base);
    uint8_t bytes[3]{};
    ASSERT_EQ(nx_uart_tx_prepare(&request, bytes, sizeof(bytes), 1000U),
              NX_SUCCESS);
    verify_sequence(&request.base);
}
TEST(IODescriptorSequences, SpiCancellationAndEpochDoNotRevokeBorrow) {
    nx_spi_request_t request{};
    nx_request_initialize(&request.base);
    uint8_t tx[3]{};
    uint8_t rx[3]{};
    ASSERT_EQ(nx_spi_request_prepare(&request, tx, rx, sizeof(tx), 1000U),
              NX_SUCCESS);
    verify_sequence(&request.base);
}
TEST(IODescriptorSequences, I2cCancellationAndEpochDoNotRevokeBorrow) {
    nx_i2c_request_t request{};
    nx_request_initialize(&request.base);
    uint8_t bytes[3]{};
    nx_i2c_message_t messages[] = {{bytes, sizeof(bytes), false}};
    ASSERT_EQ(nx_i2c_request_prepare(&request, messages, 1U, 1000U),
              NX_SUCCESS);
    verify_sequence(&request.base);
}
} /* namespace */

namespace {
TEST(IOProviderSequences, NativeUartDistinctCancelDeadlineDrainHistories) {
    for (unsigned scenario :
         {0U,  1U,  2U,  3U,  4U,  8U,  9U,  10U, 11U, 12U,
          16U, 17U, 18U, 19U, 20U, 24U, 25U, 26U, 27U, 28U}) {
        SCOPED_TRACE(scenario);
        ASSERT_EQ(nx_native_clock_configure(true, 0U), NX_SUCCESS);
        nx_native_uart_state_t state{};
        uint8_t rx[1]{};
        uint8_t txlog[4]{};
        nx_native_uart_config_t config{
            NX_UART_RX_BYTES, rx, 1U, txlog, 4U, false};
        const nx_uart_port_t uart{&nx_native_uart_ops, &state};
        ASSERT_EQ(nx_native_uart_configure_instance(&state, &config),
                  NX_SUCCESS);
        uint8_t bytes[4]{1U, 2U, 3U, 4U};
        nx_uart_tx_request_t request{};
        nx_request_initialize(&request.base);
        ASSERT_EQ(nx_uart_tx_prepare(&request, bytes, 4U, 100U), NX_SUCCESS);
        nx_request_slot_t slot{};
        uint64_t epoch{};
        ASSERT_EQ(nx_request_slot_bind(&slot, &request.base, &epoch),
                  NX_SUCCESS);
        nx_request_observer_t observer{};
        ASSERT_EQ(nx_request_observer_bind(&observer, &slot, epoch),
                  NX_SUCCESS);
        ASSERT_EQ(nx_uart_port_submit(&uart, &request), NX_SUCCESS);
        unsigned steps = scenario & 3U;
        if ((scenario & 4U) != 0U) {
            steps = 5U;
        }
        for (unsigned index = 0U; index < steps; ++index) {
            nx_native_uart_model_irq_step(&uart);
        }
        bool held = (scenario & 8U) != 0U;
        nx_native_uart_model_fault(&uart, false, held);
        if ((scenario & 16U) != 0U) {
            nx_native_clock_advance(100U);
        }
        nx_uart_port_service(&uart);
        ASSERT_EQ(nx_uart_port_cancel(&uart, &request), NX_SUCCESS);
        nx_uart_port_service(&uart);
        if (held) {
            EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
            EXPECT_EQ(nx_uart_port_stop(&uart), NX_ERROR_BUSY);
            EXPECT_EQ(nx_uart_tx_prepare(&request, bytes, 4U, 100U),
                      NX_ERROR_STATE);
            nx_native_uart_model_fault(&uart, false, false);
            nx_uart_port_service(&uart);
        }
        nx_request_observation_t out{};
        ASSERT_EQ(nx_request_observer_sample(&observer, nx_time_now_us(), &out),
                  NX_SUCCESS);
        EXPECT_TRUE(out.settled);
        nx_result_t expected = (scenario & 4U) != 0U    ? NX_SUCCESS
                               : (scenario & 16U) != 0U ? NX_ERROR_TIMEOUT
                                                        : NX_ERROR_CANCELLED;
        EXPECT_EQ(out.result, expected);
        EXPECT_EQ(out.transferred, steps == 5U ? 4U : steps);
        nx_request_settle(&request.base, NX_ERROR_IO, 99U);
        ASSERT_EQ(nx_request_observer_sample(&observer, nx_time_now_us(), &out),
                  NX_SUCCESS);
        EXPECT_EQ(out.result, expected);
        ASSERT_EQ(nx_request_slot_release(&slot, epoch), NX_SUCCESS);
        nx_request_observer_detach(&observer);
        EXPECT_EQ(nx_uart_port_stop(&uart), NX_SUCCESS);
    }
}
TEST(IOProviderSequences, NativeSpiDistinctCancelDeadlineDrainHistories) {
    for (unsigned scenario = 0U; scenario < 8U; ++scenario) {
        SCOPED_TRACE(scenario);
        ASSERT_EQ(nx_native_clock_configure(true, 0U), NX_SUCCESS);
        nx_native_spi_state_t controller{};
        nx_native_spi_endpoint_state_t endpoint{};
        const nx_spi_port_t bus{&nx_native_spi_ops, &controller};
        const nx_spi_endpoint_t device{&nx_native_spi_endpoint_ops, &endpoint};
        ASSERT_EQ(nx_native_spi_port_configure_instance(&controller),
                  NX_SUCCESS);
        ASSERT_EQ(nx_native_spi_configure_instance(&endpoint, &controller,
                                                   nullptr, 0U),
                  NX_SUCCESS);
        nx_native_spi_model_async_configure(&bus, false);
        uint8_t tx[3]{1U, 2U, 3U};
        uint8_t rx[3]{};
        nx_spi_request_t request{};
        nx_request_initialize(&request.base);
        ASSERT_EQ(nx_spi_request_prepare(&request, tx, rx, 3U, 100U),
                  NX_SUCCESS);
        ASSERT_EQ(nx_spi_endpoint_submit(&device, &request), NX_SUCCESS);
        unsigned steps = (scenario & 1U) != 0U ? 4U : 1U;
        for (unsigned index = 0U; index < steps; ++index) {
            nx_native_spi_model_irq_step(&bus);
        }
        bool held = (scenario & 2U) != 0U;
        nx_native_spi_model_drain_hold(&bus, held);
        if ((scenario & 4U) != 0U) {
            nx_native_clock_advance(100U);
        }
        nx_spi_port_service(&bus);
        ASSERT_EQ(nx_spi_port_cancel(&bus, &request), NX_SUCCESS);
        nx_spi_port_service(&bus);
        if (held) {
            EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
            EXPECT_EQ(nx_spi_port_stop(&bus), NX_ERROR_BUSY);
            EXPECT_EQ(nx_spi_request_prepare(&request, tx, rx, 3U, 100U),
                      NX_ERROR_STATE);
            nx_native_spi_model_drain_hold(&bus, false);
            nx_spi_port_service(&bus);
        }
        nx_result_t result{};
        size_t count{};
        ASSERT_EQ(nx_request_result(&request.base, &result, &count),
                  NX_SUCCESS);
        EXPECT_EQ(result, (scenario & 1U) != 0U   ? NX_SUCCESS
                          : (scenario & 4U) != 0U ? NX_ERROR_TIMEOUT
                                                  : NX_ERROR_CANCELLED);
        EXPECT_EQ(count, steps == 4U ? 3U : 1U);
        nx_request_settle(&request.base, NX_ERROR_IO, 99U);
        ASSERT_EQ(nx_request_result(&request.base, &result, &count),
                  NX_SUCCESS);
        EXPECT_EQ(count, steps == 4U ? 3U : 1U);
        EXPECT_FALSE(nx_native_spi_model_cs_active(&device));
        EXPECT_EQ(nx_spi_port_stop(&bus), NX_SUCCESS);
    }
}
TEST(IOProviderSequences,
     NativeAdcConsumerDelayStopAndOutOfOrderReleaseHistories) {
    for (unsigned scenario = 0U; scenario < 2U; ++scenario) {
        SCOPED_TRACE(scenario);
        ASSERT_EQ(nx_native_clock_configure(true, 0U), NX_SUCCESS);
        nx_native_adc_state_t state{};
        const nx_adc_port_t adc{&nx_native_adc_ops, &state};
        uint16_t samples[2]{17U, 29U};
        ASSERT_EQ(
            nx_native_adc_configure_instance(&state, samples, 2U, 12U, 3300U),
            NX_SUCCESS);
        alignas(uint16_t) uint8_t bytes[2][4]{};
        nx_stream_slot_t slots[2]{};
        for (size_t index = 0U; index < 2U; ++index) {
            slots[index].data = bytes[index];
            slots[index].capacity = 4U;
        }
        nx_stream_t stream{};
        ASSERT_EQ(nx_stream_initialize(&stream, slots, 2U), NX_SUCCESS);
        ASSERT_EQ(nx_adc_port_stream_start(&adc, &stream, 1000U), NX_SUCCESS);
        nx_stream_block_t loans[2]{};
        for (size_t index = 0U; index < 2U; ++index) {
            ASSERT_EQ(nx_native_adc_model_trigger(&adc), NX_SUCCESS);
            ASSERT_EQ(nx_stream_acquire(&stream, &loans[index]), NX_SUCCESS);
        }
        EXPECT_EQ(nx_native_adc_model_trigger(&adc), NX_ERROR_OVERFLOW);
        EXPECT_EQ(nx_adc_port_stream_stop(&adc), NX_ERROR_BUSY);
        nx_stream_observation_t out{};
        ASSERT_EQ(nx_stream_observe(&stream, &out), NX_SUCCESS);
        EXPECT_TRUE(out.stopping);
        EXPECT_EQ(out.outstanding, 2U);
        size_t first = (scenario & 1U) != 0U ? 1U : 0U;
        ASSERT_EQ(nx_stream_release(&stream, &loans[first]), NX_SUCCESS);
        EXPECT_EQ(nx_adc_port_stream_stop(&adc), NX_ERROR_BUSY);
        EXPECT_EQ(nx_stream_release(&stream, &loans[first]), NX_ERROR_STATE);
        ASSERT_EQ(nx_stream_release(&stream, &loans[1U - first]), NX_SUCCESS);
        EXPECT_EQ(nx_adc_port_stream_stop(&adc), NX_SUCCESS);
        EXPECT_EQ(nx_native_adc_model_trigger(&adc), NX_ERROR_STATE);
        ASSERT_EQ(nx_stream_observe(&stream, &out), NX_SUCCESS);
        EXPECT_EQ(out.outstanding, 0U);
        EXPECT_EQ(out.free_blocks, 2U);
    }
}
} /* namespace */

namespace {
/** \brief An external provider dependency; production wrappers remain real. */
class I2cDependency {
  public:
    MOCK_METHOD(nx_result_t, Submit, (nx_i2c_request_t*));
    MOCK_METHOD(nx_result_t, Cancel, (nx_i2c_request_t*));
    MOCK_METHOD(void, Service, ());
    MOCK_METHOD(nx_result_t, Stop, ());
};
/** \brief Dispatch exact endpoint context to the external provider dependency.
 */
nx_result_t i2c_submit(void* context, nx_i2c_request_t* request) {
    return static_cast<I2cDependency*>(context)->Submit(request);
}
/** \brief Cancellation is an external source-drain request, not final release.
 */
nx_result_t i2c_cancel(void* context, nx_i2c_request_t* request) {
    return static_cast<I2cDependency*>(context)->Cancel(request);
}
/** \brief Common void service dispatch supplies no invented error result. */
void i2c_service(void* context) {
    static_cast<I2cDependency*>(context)->Service();
}
/** \brief Failed stop retains the external provider's borrower contract. */
nx_result_t i2c_stop(void* context) {
    return static_cast<I2cDependency*>(context)->Stop();
}
TEST(IOProviderSequences,
     I2cPublicDispatchAndObservationRetainAcrossDependencyRetries) {
    testing::StrictMock<I2cDependency> dependency;
    nx_i2c_endpoint_ops_t endpoint_ops{};
    endpoint_ops.submit = i2c_submit;
    nx_i2c_ops_t port_ops{};
    port_ops.cancel = i2c_cancel;
    port_ops.service = i2c_service;
    port_ops.stop = i2c_stop;
    const nx_i2c_port_t port{&port_ops, &dependency};
    const nx_i2c_endpoint_t endpoint{&endpoint_ops, &dependency};
    uint8_t bytes[3]{};
    nx_i2c_message_t message{bytes, 3U, true};
    nx_i2c_request_t request{};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_i2c_request_prepare(&request, &message, 1U, 100U), NX_SUCCESS);
    nx_request_slot_t slot{};
    uint64_t epoch{};
    ASSERT_EQ(nx_request_slot_bind(&slot, &request.base, &epoch), NX_SUCCESS);
    nx_request_observer_t observer{};
    ASSERT_EQ(nx_request_observer_bind(&observer, &slot, epoch), NX_SUCCESS);
    EXPECT_CALL(dependency, Submit(&request))
        .WillOnce(testing::Invoke([](nx_i2c_request_t* r) {
            return nx_request_admit(&r->base, NX_REQUEST_ACTIVE);
        }));
    ASSERT_EQ(nx_i2c_endpoint_submit(&endpoint, &request), NX_SUCCESS);
    EXPECT_CALL(dependency, Cancel(&request))
        .WillOnce(testing::Invoke([](nx_i2c_request_t* r) {
            return nx_request_transition(&r->base, NX_REQUEST_DRAINING);
        }));
    ASSERT_EQ(nx_i2c_port_cancel(&port, &request), NX_SUCCESS);
    EXPECT_CALL(dependency, Service()).WillOnce(testing::Invoke([&] {
        ASSERT_EQ(nx_request_transition(&request.base, NX_REQUEST_QUARANTINED),
                  NX_SUCCESS);
    }));
    nx_i2c_port_service(&port);
    nx_request_observation_t out{};
    ASSERT_EQ(nx_request_observer_sample(&observer, 1000U, &out), NX_SUCCESS);
    EXPECT_EQ(out.state, NX_REQUEST_QUARANTINED);
    EXPECT_FALSE(out.settled);
    EXPECT_EQ(out.result, NX_ERROR_BUSY);
    EXPECT_EQ(nx_i2c_request_prepare(&request, &message, 1U, 2000U),
              NX_ERROR_STATE);
    EXPECT_CALL(dependency, Stop()).WillOnce(testing::Return(NX_ERROR_BUSY));
    EXPECT_EQ(nx_i2c_port_stop(&port), NX_ERROR_BUSY);
    EXPECT_EQ(slot.request, &request.base);
    EXPECT_CALL(dependency, Service()).WillOnce(testing::Invoke([&] {
        nx_request_settle(&request.base, NX_ERROR_CANCELLED, 1U);
    }));
    nx_i2c_port_service(&port);
    ASSERT_EQ(nx_request_observer_sample(&observer, 1001U, &out), NX_SUCCESS);
    EXPECT_TRUE(out.settled);
    EXPECT_EQ(out.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(out.transferred, 1U);
    ASSERT_EQ(nx_request_slot_release(&slot, epoch), NX_SUCCESS);
    nx_request_observer_detach(&observer);
    EXPECT_CALL(dependency, Stop()).WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_EQ(nx_i2c_port_stop(&port), NX_SUCCESS);
}
} /* namespace */
