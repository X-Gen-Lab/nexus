/**
 * \file            io_owner_diagnostic_pipeline_test.cpp
 * \brief           Real UART/SPI progress feeds copied read-only diagnostics
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/owner_observer.h"
#include "nexus/components/spi_async_owner.h"
#include "nexus/components/uart_owner.h"
#include "nexus/io/diagnostic.h"
#include "provider.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
enum class Transport { Uart, Spi };

/** \brief A monitor receives value copies, never owner or payload pointers. */
nx_io_diagnostic_t consume(const nx_owner_observation_t& owner,
                           const nx_request_observation_t* request) {
    nx_io_diagnostic_facts_t facts{};
    facts.request = request;
    if (owner.diagnostic_result == NX_SUCCESS) {
        facts.known = NX_IO_FACT_OWNER;
        facts.owner_accepting = owner.accepting;
        facts.owner_queued = owner.queued;
        facts.owner_slot_available = owner.slot_available;
    }
    nx_io_diagnostic_t result{};
    EXPECT_EQ(nx_io_diagnose(&facts, &result), NX_SUCCESS);
    return result;
}

/** \brief Caller-owned integration storage; all providers and bridges are C. */
class Pipeline {
  public:
    nx_native_uart_state_t uart_state{};
    nx_native_spi_state_t spi_state{};
    nx_native_spi_endpoint_state_t endpoint_state{};
    const nx_uart_port_t uart{&nx_native_uart_ops, &uart_state};
    const nx_spi_port_t spi{&nx_native_spi_ops, &spi_state};
    const nx_spi_endpoint_t endpoint{&nx_native_spi_endpoint_ops,
                                     &endpoint_state};
    uint8_t payload[3]{1U, 2U, 3U};
    uint8_t received[3]{};
    uint8_t uart_rx[1]{};
    uint8_t uart_log[4]{};
    nx_uart_owner_operation_t uart_operation{payload, sizeof(payload)};
    nx_spi_owner_operation_t spi_operation{&endpoint, payload, received,
                                           sizeof(payload)};
    nx_uart_owner_executor_t uart_executor{};
    nx_spi_async_owner_executor_t spi_executor{};
    nx_bus_owner_t owner{};
    nx_owner_slot_t owner_slots[1]{};
    nx_owner_observer_t progress{};
    nx_owner_observation_t copy{};
    nx_request_t request{};
    nx_request_slot_t application_slot{};
    nx_request_observer_t application_observer{};
    nx_owner_ticket_t ticket{};
    uint64_t application_epoch{};
    unsigned guard_entries{};
    unsigned clock_reads{};
    uintptr_t guard_depth{};
    Transport transport{};

    nx_result_t initialize(Transport selected) {
        transport = selected;
        nx_owner_executor_port_t executor{};
        nx_result_t result;
        if (transport == Transport::Uart) {
            const nx_native_uart_config_t config{NX_UART_RX_BYTES, uart_rx,
                                                 sizeof(uart_rx),  uart_log,
                                                 sizeof(uart_log), false};
            result = nx_native_uart_configure_instance(&uart_state, &config);
            executor = nx_uart_owner_executor_port(&uart_executor, &uart);
        } else {
            result = nx_native_spi_port_configure_instance(&spi_state);
            if (result == NX_SUCCESS) {
                result = nx_native_spi_configure_instance(
                    &endpoint_state, &spi_state, nullptr, 0U);
            }
            nx_native_spi_model_async_configure(&spi, false);
            executor = nx_spi_async_owner_executor_port(&spi_executor, &spi);
        }
        if (result != NX_SUCCESS) {
            return result;
        }
        result =
            nx_bus_owner_init(&owner, owner_slots, 1U, {this, enter, leave},
                              executor, {clock, this}, nullptr);
        if (result != NX_SUCCESS) {
            return result;
        }
        nx_request_initialize(&request);
        result = nx_request_prepare(&request, 1000U);
        if (result == NX_SUCCESS) {
            result = nx_request_slot_bind(&application_slot, &request,
                                          &application_epoch);
        }
        if (result == NX_SUCCESS) {
            result = nx_request_observer_bind(
                &application_observer, &application_slot, application_epoch);
        }
        return result == NX_SUCCESS ? nx_owner_observer_bind(&progress, &owner)
                                    : result;
    }

    nx_result_t submit() {
        void* operation = transport == Transport::Uart
                              ? static_cast<void*>(&uart_operation)
                              : static_cast<void*>(&spi_operation);
        return nx_bus_owner_submit(&owner, &request, operation, nullptr,
                                   &ticket);
    }

    /* This is the unique executor's normal progress point, never a query. */
    nx_result_t step(nx_time_us_t timestamp) {
        return nx_owner_observer_service(&progress, timestamp, &copy);
    }

    nx_request_observation_t request_copy() {
        nx_request_observation_t output{};
        EXPECT_EQ(nx_request_observer_sample(&application_observer,
                                             nx_time_now_us(), &output),
                  NX_SUCCESS);
        return output;
    }

    void hold_drain(bool held) {
        if (transport == Transport::Uart) {
            nx_native_uart_model_fault(&uart, false, held);
        } else {
            nx_native_spi_model_drain_hold(&spi, held);
        }
    }

    bool hardware_opened() const {
        return transport == Transport::Uart ? uart_state.opened
                                            : spi_state.opened;
    }

    size_t wire_progress() const {
        return transport == Transport::Uart ? uart_state.tx_index
                                            : spi_state.transferred;
    }

    nx_result_t join_observers_and_stop_provider() {
        nx_request_observer_detach(&application_observer);
        nx_result_t result =
            nx_request_slot_release(&application_slot, application_epoch);
        if (result != NX_SUCCESS) {
            return result;
        }
        nx_owner_observer_detach(&progress);
        return transport == Transport::Uart ? nx_uart_port_stop(&uart)
                                            : nx_spi_port_stop(&spi);
    }

  private:
    static uintptr_t enter(void* context) {
        auto* pipeline = static_cast<Pipeline*>(context);
        EXPECT_EQ(pipeline->guard_depth, 0U);
        ++pipeline->guard_entries;
        pipeline->guard_depth = 1U;
        return 0U;
    }
    static void leave(void* context, uintptr_t saved) {
        auto* pipeline = static_cast<Pipeline*>(context);
        EXPECT_EQ(pipeline->guard_depth, 1U);
        pipeline->guard_depth = saved;
    }
    static nx_time_us_t clock(void* context) {
        auto* pipeline = static_cast<Pipeline*>(context);
        ++pipeline->clock_reads;
        return nx_time_now_us();
    }
};

TEST(IOOwnerDiagnosticPipeline,
     RealProvidersKeepActiveQuarantineAndStopDistinct) {
    for (Transport transport : {Transport::Uart, Transport::Spi}) {
        SCOPED_TRACE(static_cast<int>(transport));
        ASSERT_EQ(nx_native_clock_configure(true, 0U), NX_SUCCESS);
        Pipeline pipeline;
        ASSERT_EQ(pipeline.initialize(transport), NX_SUCCESS);
        ASSERT_EQ(pipeline.submit(), NX_SUCCESS);
        ASSERT_EQ(pipeline.step(0U), NX_SUCCESS);
        ASSERT_TRUE(pipeline.copy.has_active);
        EXPECT_EQ(pipeline.copy.active_state, NX_REQUEST_ACTIVE);
        const nx_owner_observation_t active_owner = pipeline.copy;
        const nx_request_observation_t active_request = pipeline.request_copy();
        const auto active = consume(active_owner, &active_request);
        EXPECT_NE(active.findings & NX_IO_DIAG_ACTIVE, 0U);
        EXPECT_NE(active.unknown_facts & NX_IO_FACT_LIFECYCLE, 0U);
        const auto guards = pipeline.guard_entries;
        const auto clocks = pipeline.clock_reads;
        const auto calls = pipeline.progress.observation.calls;
        const auto wire = pipeline.wire_progress();
        for (unsigned index = 0U; index < 16U; ++index) {
            EXPECT_THAT(consume(active_owner, &active_request).findings,
                        testing::Eq(active.findings));
        }
        EXPECT_EQ(pipeline.guard_entries, guards);
        EXPECT_EQ(pipeline.clock_reads, clocks);
        EXPECT_EQ(pipeline.progress.observation.calls, calls);
        EXPECT_EQ(pipeline.wire_progress(), wire);

        pipeline.hold_drain(true);
        ASSERT_EQ(nx_bus_owner_cancel(&pipeline.owner, pipeline.ticket),
                  NX_SUCCESS);
        ASSERT_EQ(pipeline.step(1U), NX_ERROR_IO);
        const auto quarantined_request = pipeline.request_copy();
        EXPECT_EQ(pipeline.copy.active_state, NX_REQUEST_QUARANTINED);
        EXPECT_NE(consume(pipeline.copy, &quarantined_request).findings &
                      NX_IO_DIAG_QUARANTINED,
                  0U);
        EXPECT_EQ(nx_request_prepare(&pipeline.request, 2000U), NX_ERROR_STATE);
        nx_bus_owner_stop(&pipeline.owner);
        ASSERT_EQ(pipeline.step(2U), NX_ERROR_IO);
        EXPECT_FALSE(pipeline.copy.accepting);
        EXPECT_FALSE(pipeline.copy.idle);
        EXPECT_TRUE(pipeline.hardware_opened());
        pipeline.hold_drain(false);
        ASSERT_EQ(pipeline.step(3U), NX_SUCCESS);
        EXPECT_TRUE(pipeline.copy.idle);
        EXPECT_FALSE(pipeline.copy.accepting);
        EXPECT_TRUE(pipeline.copy.slot_available);
        const auto settled_request = pipeline.request_copy();
        EXPECT_TRUE(settled_request.settled);
        EXPECT_EQ(settled_request.result, NX_ERROR_CANCELLED);
        const auto stopped_owner = consume(pipeline.copy, &settled_request);
        EXPECT_NE(stopped_owner.findings & NX_IO_DIAG_OWNER_CLOSED, 0U);
        EXPECT_NE(stopped_owner.findings & NX_IO_DIAG_SETTLED, 0U);
        EXPECT_NE(stopped_owner.unknown_facts & NX_IO_FACT_LIFECYCLE, 0U);
        EXPECT_TRUE(pipeline.hardware_opened());
        ASSERT_EQ(pipeline.join_observers_and_stop_provider(), NX_SUCCESS);
        EXPECT_FALSE(pipeline.hardware_opened());
        /* Old copies remain historical facts, never current permissions. */
        EXPECT_EQ(consume(active_owner, &active_request).findings,
                  active.findings);
    }
}

TEST(IOOwnerDiagnosticPipeline,
     TwoRealControllersKeepCopiesAndAdmissionSeparate) {
    for (Transport transport : {Transport::Uart, Transport::Spi}) {
        SCOPED_TRACE(static_cast<int>(transport));
        ASSERT_EQ(nx_native_clock_configure(true, 0U), NX_SUCCESS);
        Pipeline first;
        Pipeline second;
        ASSERT_EQ(first.initialize(transport), NX_SUCCESS);
        ASSERT_EQ(second.initialize(transport), NX_SUCCESS);
        ASSERT_EQ(first.submit(), NX_SUCCESS);
        ASSERT_EQ(first.step(0U), NX_SUCCESS);
        nx_bus_owner_stop(&second.owner);
        ASSERT_EQ(second.step(0U), NX_SUCCESS);
        EXPECT_TRUE(second.copy.idle);
        EXPECT_FALSE(second.copy.accepting);
        EXPECT_FALSE(second.copy.has_active);
        EXPECT_TRUE(first.copy.accepting);
        EXPECT_TRUE(first.copy.has_active);
        EXPECT_FALSE(first.copy.idle);
        const auto closed = consume(second.copy, nullptr);
        EXPECT_NE(closed.findings & NX_IO_DIAG_OWNER_CLOSED, 0U);
        EXPECT_NE(closed.unknown_facts & NX_IO_FACT_REQUEST, 0U);
        const auto first_calls = first.progress.observation.calls;
        const auto first_wire = first.wire_progress();
        EXPECT_EQ(consume(second.copy, nullptr).findings, closed.findings);
        EXPECT_EQ(first.progress.observation.calls, first_calls);
        EXPECT_EQ(first.wire_progress(), first_wire);
        ASSERT_EQ(second.join_observers_and_stop_provider(), NX_SUCCESS);
        EXPECT_TRUE(first.hardware_opened());
        nx_bus_owner_stop(&first.owner);
        ASSERT_EQ(first.step(1U), NX_SUCCESS);
        ASSERT_EQ(first.join_observers_and_stop_provider(), NX_SUCCESS);
    }
}

TEST(IOOwnerDiagnosticPipeline,
     FailedDiagnosticClockCannotInventKnownOwnerFacts) {
    ASSERT_EQ(nx_native_clock_configure(true, 0U), NX_SUCCESS);
    Pipeline pipeline;
    ASSERT_EQ(pipeline.initialize(Transport::Uart), NX_SUCCESS);
    ASSERT_EQ(pipeline.step(10U), NX_SUCCESS);
    ASSERT_EQ(pipeline.step(9U), NX_SUCCESS);
    ASSERT_EQ(pipeline.copy.diagnostic_result, NX_ERROR_IO);
    const auto diagnostic = consume(pipeline.copy, nullptr);
    EXPECT_EQ(diagnostic.findings, 0U);
    EXPECT_EQ(diagnostic.unknown_facts, NX_IO_FACT_ALL);
    nx_bus_owner_stop(&pipeline.owner);
    ASSERT_EQ(pipeline.step(11U), NX_SUCCESS);
    ASSERT_EQ(pipeline.join_observers_and_stop_provider(), NX_SUCCESS);
}

TEST(IOOwnerDiagnosticPipeline, JoinedCopiesOutliveTheirPayloadAndExecutor) {
    nx_owner_observation_t owner_copy{};
    nx_request_observation_t request_copy{};
    {
        ASSERT_EQ(nx_native_clock_configure(true, 0U), NX_SUCCESS);
        Pipeline pipeline;
        ASSERT_EQ(pipeline.initialize(Transport::Spi), NX_SUCCESS);
        ASSERT_EQ(pipeline.submit(), NX_SUCCESS);
        ASSERT_EQ(pipeline.step(0U), NX_SUCCESS);
        nx_bus_owner_stop(&pipeline.owner);
        ASSERT_EQ(pipeline.step(1U), NX_SUCCESS);
        owner_copy = pipeline.copy;
        request_copy = pipeline.request_copy();
        ASSERT_TRUE(request_copy.settled);
        ASSERT_EQ(pipeline.join_observers_and_stop_provider(), NX_SUCCESS);
        for (auto& byte : pipeline.payload) {
            byte = 0xffU;
        }
    }
    const auto diagnostic = consume(owner_copy, &request_copy);
    EXPECT_NE(diagnostic.findings & NX_IO_DIAG_SETTLED, 0U);
    EXPECT_NE(diagnostic.findings & NX_IO_DIAG_OWNER_CLOSED, 0U);
    EXPECT_NE(diagnostic.unknown_facts & NX_IO_FACT_OBSERVERS, 0U);
    EXPECT_NE(diagnostic.unknown_facts & NX_IO_FACT_LIFECYCLE, 0U);
}
} /* namespace */
