/** Runtime -> typed HAL -> real Native SPI. Host timing is a model, not HIL. */
#include "runtime/nx_runtime.h"
#include "hal/base/nx_device.h"
#include "hal/system/nx_mem.h"
#include "devices/native_spi_helpers.h"
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

class TypedNativeSPI : public ::testing::Test {
protected:
    nx_device_ref_t controller{};
    std::vector<nx_device_spi_ref_t> opened;
    uint8_t tx[3]{0x12, 0x34, 0x56}, rx[3]{};
    nx_spi_device_config_t config{3, 1000000, NX_SPI_MODE_0, NX_SPI_BIT_ORDER_MSB};
    nx_spi_transaction_t transaction{tx, rx, sizeof(tx), 500, nullptr, nullptr};
    virtual uint32_t delay() const { return 0; }
    void SetUp() override {
        ASSERT_EQ(nx_runtime_bootstrap(nullptr), NX_OK);
        nx_mem_stats_t before{}, after{};
        ASSERT_EQ(nx_mem_get_stats(&before), NX_OK);
        // First binding allocates neither the bus, slave pool nor capture bytes.
        ASSERT_EQ(native_spi_reset(0), NX_OK);
        ASSERT_EQ(nx_mem_get_stats(&after), NX_OK);
        EXPECT_EQ(after.alloc_count, before.alloc_count);
        ASSERT_EQ(native_spi_set_transfer_delay(0, delay()), NX_OK);
        ASSERT_EQ(nx_device_open("SPI0", NX_DEVICE_CLASS_SPI, 0x701, &controller), NX_OK);
    }
    void TearDown() override {
        if (controller.descriptor) {
            while (nx_device_spi_service(controller) != NX_ERR_NO_DATA) {}
            for (auto ref : opened) {
                nx_status_t status = nx_device_spi_close(ref);
                EXPECT_TRUE(status == NX_OK || status == NX_ERR_INVALID_STATE);
            }
            EXPECT_EQ(nx_device_close(controller), NX_OK);
        }
        EXPECT_EQ(nx_runtime_shutdown(nullptr), NX_OK);
    }
    nx_device_spi_ref_t child() {
        nx_device_spi_ref_t ref{};
        EXPECT_EQ(nx_device_spi_open(controller, &config, &ref), NX_OK);
        if (ref.slot) opened.push_back(ref);
        return ref;
    }
    void close_all() {
        for (auto ref : opened) EXPECT_EQ(nx_device_spi_close(ref), NX_OK);
        opened.clear(); EXPECT_EQ(nx_device_close(controller), NX_OK); controller = {};
    }
};
TEST_F(TypedNativeSPI, RuntimeShutdownRejectsControllerAndChildrenUntilClosed) {
    auto ref = child();
    EXPECT_EQ(nx_runtime_shutdown(nullptr), NX_ERR_BUSY);
    EXPECT_TRUE((nx_runtime_get_state() == NX_RUNTIME_READY));
    EXPECT_EQ(nx_device_close(controller), NX_ERR_BUSY);
    ASSERT_EQ(nx_device_spi_transfer(ref, &transaction), NX_OK);
    EXPECT_EQ(std::memcmp(rx, tx, sizeof(tx)), 0);
    close_all(); EXPECT_EQ(nx_runtime_shutdown(nullptr), NX_OK);
}
TEST_F(TypedNativeSPI, DistinctChildrenKeepTheirOwnPhysicalConfiguration) {
    auto first = child(); config.cs_pin = 5; config.speed = 500000; config.mode = NX_SPI_MODE_3;
    auto second = child();
    ASSERT_EQ(nx_device_spi_transfer(first, &transaction), NX_OK);
    ASSERT_EQ(nx_device_spi_transfer(second, &transaction), NX_OK);
    close_all();
    native_spi_operation_t trace[2]{}; size_t count = 2;
    ASSERT_EQ(native_spi_get_trace(0, trace, &count), NX_OK); ASSERT_EQ(count, 2u);
    EXPECT_EQ(trace[0].config.cs_pin, 3); EXPECT_EQ(trace[0].config.speed, 1000000u);
    EXPECT_EQ(trace[1].config.cs_pin, 5); EXPECT_EQ(trace[1].config.speed, 500000u);
    EXPECT_NE(trace[0].token, trace[1].token);
}
TEST_F(TypedNativeSPI, QueueDelayConsumesOriginalDeadlineAndLeavesRXUntouched) {
    auto ref = child(); transaction.timeout_ms = 5; nx_device_spi_ticket_t ticket{};
    ASSERT_EQ(nx_device_spi_submit(ref, &transaction, &ticket), NX_OK);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    nx_device_spi_result_t result{};
    ASSERT_EQ(nx_device_spi_poll(ref, ticket, &result), NX_OK); EXPECT_FALSE(result.settled);
    EXPECT_EQ(nx_device_spi_service(controller), NX_ERR_TIMEOUT);
    ASSERT_EQ(nx_device_spi_poll(ref, ticket, &result), NX_OK);
    EXPECT_TRUE(result.settled); EXPECT_EQ(result.status, NX_ERR_TIMEOUT);
    EXPECT_EQ(rx[0], 0); EXPECT_EQ(rx[1], 0); EXPECT_EQ(rx[2], 0);
    close_all();
    native_spi_operation_t trace[1]{}; size_t count = 1;
    ASSERT_EQ(native_spi_get_trace(0, trace, &count), NX_OK); EXPECT_EQ(count, 0u);
}
TEST_F(TypedNativeSPI, QueuedCancelRequiresServiceAndStaleTicketCannotCancelNext) {
    auto ref = child(); nx_device_spi_ticket_t old{}, next{};
    ASSERT_EQ(nx_device_spi_submit(ref, &transaction, &old), NX_OK);
    ASSERT_EQ(nx_device_spi_cancel(ref, old), NX_OK);
    EXPECT_EQ(nx_device_spi_close(ref), NX_ERR_BUSY);
    EXPECT_EQ(nx_device_spi_service(controller), NX_ERR_CANCELLED);
    nx_device_spi_result_t result{};
    ASSERT_EQ(nx_device_spi_poll(ref, old, &result), NX_OK);
    EXPECT_TRUE(result.settled); EXPECT_EQ(result.status, NX_ERR_CANCELLED);
    ASSERT_EQ(nx_device_spi_submit(ref, &transaction, &next), NX_OK);
    EXPECT_EQ(nx_device_spi_cancel(ref, old), NX_ERR_INVALID_STATE);
    EXPECT_EQ(nx_device_spi_service(controller), NX_OK);
    ASSERT_EQ(nx_device_spi_poll(ref, next, &result), NX_OK);
    EXPECT_TRUE(result.settled); EXPECT_EQ(result.status, NX_OK);
    EXPECT_EQ(std::memcmp(rx, tx, sizeof(tx)), 0);
}
class TypedNativeDelayedSPI : public TypedNativeSPI {
    uint32_t delay() const override { return 100; }
};
TEST_F(TypedNativeDelayedSPI, SynchronousCancelFromAnotherTaskSettlesBeforeReturn) {
    auto ref = child(); std::atomic<bool> done{false}; nx_status_t outcome = NX_OK;
    std::thread worker([&] { outcome = nx_device_spi_transfer(ref, &transaction); done = true; });
    nx_status_t request = NX_ERR_NO_DATA;
    auto end = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (!done.load() && std::chrono::steady_clock::now() < end) {
        request = nx_device_spi_cancel_transfer(ref);
        if (request == NX_OK) break;
        // The core may pin just before the provider admits the transfer.
        EXPECT_TRUE(request == NX_ERR_NO_DATA || request == NX_ERR_BUSY || request == NX_ERR_NOT_FOUND);
        std::this_thread::yield();
    }
    worker.join(); EXPECT_EQ(request, NX_OK); EXPECT_EQ(outcome, NX_ERR_CANCELLED);
    EXPECT_EQ(rx[0], 0); EXPECT_EQ(nx_device_spi_close(ref), NX_OK);
}
TEST_F(TypedNativeDelayedSPI, ServiceAndCancellationUseIndependentTaskPins) {
    auto ref = child(); nx_device_spi_ticket_t ticket{};
    ASSERT_EQ(nx_device_spi_submit(ref, &transaction, &ticket), NX_OK);
    std::atomic<bool> entered{false}; nx_status_t outcome = NX_OK;
    std::thread worker([&] {
        entered = true;
        do {
            outcome = nx_device_spi_service(controller);
            if (outcome == NX_ERR_BUSY) std::this_thread::yield();
        } while (outcome == NX_ERR_BUSY);
    });
    while (!entered.load()) std::this_thread::yield();
    EXPECT_EQ(nx_device_spi_cancel(ref, ticket), NX_OK);
    worker.join(); EXPECT_EQ(outcome, NX_ERR_CANCELLED);
    nx_device_spi_result_t result{};
    ASSERT_EQ(nx_device_spi_poll(ref, ticket, &result), NX_OK);
    EXPECT_TRUE(result.settled); EXPECT_EQ(result.status, NX_ERR_CANCELLED);
    EXPECT_EQ(rx[0], 0);
}
