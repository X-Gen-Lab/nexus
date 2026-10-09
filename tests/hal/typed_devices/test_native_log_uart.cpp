/** Production logger -> UART adapter -> actual Native provider. The TX
 * observation buffer is a host model; this fixture establishes no serial
 * device, physical wire-idle, DMA, or board qualification. */
#include "hal/nx_hal.h"
#include "hal/provider/nx_device_provider.h"
#include "log/log.h"
#include "log/log_uart.h"
#include "runtime/nx_runtime.h"
#include <cstring>
#include <gtest/gtest.h>
extern "C" {
#include "../../../soc/native/controllers/uart/nx_uart_types.h"
}

class TypedNativeLogUART : public ::testing::Test {
  protected:
    log_uart_backend_t sink{};
    nx_uart_impl_t* provider = nullptr;

    void SetUp() override {
        ASSERT_EQ(nx_runtime_bootstrap(nullptr), NX_OK);
        ASSERT_EQ(log_backend_uart_init(&sink, "UART0", 1000, 1000), LOG_OK);
        provider =
            static_cast<nx_uart_impl_t*>(sink.device.descriptor->state->api);
        ASSERT_NE(provider, nullptr);
        ASSERT_EQ(log_init(nullptr), LOG_OK);
        ASSERT_EQ(log_backend_register(&sink.backend), LOG_OK);
    }
    void TearDown() override {
        if (log_backend_get("uart") == &sink.backend) {
            EXPECT_EQ(log_backend_unregister("uart"), LOG_OK);
        }
        EXPECT_EQ(log_backend_uart_shutdown(&sink, true), LOG_OK);
        if (log_is_initialized()) {
            EXPECT_EQ(log_deinit(), LOG_OK);
        }
        EXPECT_EQ(nx_runtime_shutdown(nullptr), NX_OK);
    }
};

TEST_F(TypedNativeLogUART,
       BoundedCopyExplicitPumpAndSettledInfrastructureClose) {
    constexpr char expected[] = "native-owned";
    char source[sizeof(expected)];
    std::memcpy(source, expected, sizeof(source));
    constexpr size_t length = sizeof(expected) - 1;
    ASSERT_GE(provider->state->tx_buf.size, LOG_UART_QUEUE_DEPTH * length);
    nx_device_ref_t former_owner = sink.device;

    for (size_t i = 0; i < LOG_UART_QUEUE_DEPTH; ++i)
        ASSERT_EQ(log_write_raw(source, length), LOG_OK);
    EXPECT_EQ(log_write_raw(source, length), LOG_ERROR_FULL);
    std::memset(source, '!', length);

    // Enqueueing takes a caller-independent copy and starts no hidden worker.
    log_uart_stats_t stats{};
    ASSERT_EQ(log_backend_uart_get_stats(&sink, &stats), LOG_OK);
    EXPECT_EQ(stats.accepted, LOG_UART_QUEUE_DEPTH);
    EXPECT_EQ(stats.completed, 0u);
    EXPECT_EQ(stats.pending, LOG_UART_QUEUE_DEPTH);
    EXPECT_EQ(stats.dropped, 1u);
    EXPECT_FALSE(stats.buffer_leased);
    EXPECT_EQ(provider->state->tx_buf.count, 0u);
    EXPECT_EQ(provider->state->stats.tx_count, 0u);

    // The registered sink and the infrastructure each retain their ownership.
    EXPECT_EQ(log_backend_uart_shutdown(&sink, true), LOG_ERROR_BUSY);
    EXPECT_EQ(nx_runtime_shutdown(nullptr), NX_ERR_BUSY);
    EXPECT_EQ(nx_runtime_get_state(), NX_RUNTIME_READY);
    EXPECT_TRUE(sink.opened);

    ASSERT_EQ(log_backend_uart_service(&sink), LOG_OK);
    ASSERT_EQ(log_backend_uart_get_stats(&sink, &stats), LOG_OK);
    EXPECT_EQ(stats.completed, 1u);
    EXPECT_EQ(stats.pending, LOG_UART_QUEUE_DEPTH - 1u);
    EXPECT_FALSE(stats.buffer_leased);
    EXPECT_EQ(sink.device.descriptor->state->active_ticket, 0u);
    ASSERT_EQ(provider->state->tx_buf.count, length);
    EXPECT_EQ(std::memcmp(provider->state->tx_buf.data, expected, length), 0);

    ASSERT_EQ(log_backend_uart_flush(&sink), LOG_OK);
    ASSERT_EQ(log_backend_uart_get_stats(&sink, &stats), LOG_OK);
    EXPECT_EQ(stats.completed, LOG_UART_QUEUE_DEPTH);
    EXPECT_EQ(stats.pending, 0u);
    EXPECT_EQ(stats.failed, 0u);
    EXPECT_FALSE(stats.buffer_leased);
    ASSERT_EQ(provider->state->tx_buf.count, LOG_UART_QUEUE_DEPTH * length);
    for (size_t i = 0; i < LOG_UART_QUEUE_DEPTH; ++i)
        EXPECT_EQ(std::memcmp(provider->state->tx_buf.data + i * length,
                              expected, length),
                  0);

    // Unregistration runs the actual provider lifecycle and invalidates the
    // previous owner only after every accepted message and ticket settles.
    ASSERT_EQ(log_backend_unregister("uart"), LOG_OK);
    EXPECT_FALSE(sink.opened);
    nx_device_caps_t caps{};
    EXPECT_EQ(nx_device_query(former_owner, &caps), NX_ERR_INVALID_STATE);
    EXPECT_EQ(provider->lifecycle.get_state(&provider->lifecycle),
              NX_DEV_STATE_UNINITIALIZED);
    EXPECT_EQ(log_deinit(), LOG_OK);
    EXPECT_EQ(log_backend_uart_shutdown(&sink, true), LOG_OK);
    EXPECT_EQ(nx_runtime_shutdown(nullptr), NX_OK);
    EXPECT_EQ(nx_runtime_get_state(), NX_RUNTIME_OFFLINE);
}
