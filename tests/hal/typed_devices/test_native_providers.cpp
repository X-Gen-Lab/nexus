/** Actual selected Native providers: binding, ownership and resource fences.
 * No vendor/physical timing is inferred from this host resource model. */
#include "arch/nx_arch.h"
#include "hal/nx_hal.h"
#include "hal/provider/nx_device_provider.h"
#include "runtime/nx_runtime.h"
#include <array>
#include <gtest/gtest.h>
extern "C" {
#include "../../../soc/native/private/native_platform.h"
void nx_isr_simulate(uint32_t irq);
}

class NativeProviders : public ::testing::Test {
  protected:
    void SetUp() override {
        ASSERT_EQ(nx_runtime_bootstrap(nullptr), NX_OK);
    }
    void TearDown() override {
        nx_device_shutdown_end();
        EXPECT_EQ(nx_runtime_shutdown(nullptr), NX_OK);
    }
};

TEST_F(NativeProviders,
       MaintainedDescriptorsUsePreciseConstructionAndStableBinding) {
    const std::array<std::pair<const char*, nx_device_class_t>, 5> providers{
        {{"GPIOA0", NX_DEVICE_CLASS_GPIO},
         {"UART0", NX_DEVICE_CLASS_UART},
         {"SPI0", NX_DEVICE_CLASS_SPI},
         {"I2C0", NX_DEVICE_CLASS_I2C},
         {"FLASH0", NX_DEVICE_CLASS_FLASH}}};
    for (const auto& provider : providers) {
        const nx_device_t* descriptor = nullptr;
        ASSERT_EQ(
            nx_device_discover(provider.first, provider.second, &descriptor),
            NX_OK);
        ASSERT_NE(descriptor->construct, nullptr);
        EXPECT_EQ(descriptor->device_init, nullptr);
        void* rejected = reinterpret_cast<void*>(1);
        EXPECT_EQ(descriptor->construct(nullptr, &rejected),
                  NX_ERR_INVALID_PARAM);
        EXPECT_EQ(rejected, nullptr);
        void* binding = nx_device_get_checked(provider.first, provider.second);
        ASSERT_NE(binding, nullptr);
        nx_lifecycle_t* life =
            descriptor->get_lifecycle ? descriptor->get_lifecycle(binding)
            : provider.second == NX_DEVICE_CLASS_GPIO
                ? static_cast<nx_gpio_t*>(binding)->write.get_lifecycle(
                      &static_cast<nx_gpio_t*>(binding)->write)
            : provider.second == NX_DEVICE_CLASS_UART
                ? static_cast<nx_uart_t*>(binding)->get_lifecycle(
                      static_cast<nx_uart_t*>(binding))
            : provider.second == NX_DEVICE_CLASS_SPI
                ? static_cast<nx_spi_bus_t*>(binding)->get_lifecycle(
                      static_cast<nx_spi_bus_t*>(binding))
            : provider.second == NX_DEVICE_CLASS_I2C
                ? static_cast<nx_i2c_bus_t*>(binding)->get_lifecycle(
                      static_cast<nx_i2c_bus_t*>(binding))
                : static_cast<nx_internal_flash_t*>(binding)->get_lifecycle(
                      static_cast<nx_internal_flash_t*>(binding));
        ASSERT_NE(life, nullptr);
        EXPECT_EQ(life->get_state(life), NX_DEV_STATE_UNINITIALIZED);
        EXPECT_EQ(nx_device_get_checked(provider.first, provider.second),
                  binding);
    }
}
TEST_F(NativeProviders, GpioCloseReopenKeepsProviderBindingButRejectsOldOwner) {
    const nx_device_t* descriptor = nullptr;
    ASSERT_EQ(nx_device_discover("GPIOA0", NX_DEVICE_CLASS_GPIO, &descriptor),
              NX_OK);
    void* binding = nx_device_get_checked("GPIOA0", NX_DEVICE_CLASS_GPIO);
    nx_device_ref_t ref{};
    ASSERT_EQ(nx_device_open("GPIOA0", NX_DEVICE_CLASS_GPIO, 1, &ref), NX_OK);
    EXPECT_EQ(nx_device_gpio_write(ref, 1), NX_OK);
    EXPECT_EQ(nx_device_close(ref), NX_OK);
    nx_device_ref_t next{};
    ASSERT_EQ(nx_device_open("GPIOA0", NX_DEVICE_CLASS_GPIO, 2, &next), NX_OK);
    EXPECT_EQ(descriptor->state->api, binding);
    EXPECT_NE(next.generation, ref.generation);
    EXPECT_EQ(nx_device_gpio_write(ref, 0), NX_ERR_INVALID_STATE);
    EXPECT_EQ(nx_device_close(next), NX_OK);
}
TEST_F(NativeProviders, ShutdownFenceRejectsNewDmaAndInterruptOwners) {
    nx_dma_channel_t* dma = nx_dma_allocate_channel(0, 0);
    ASSERT_NE(dma, nullptr);
    EXPECT_EQ(nx_native_resources_idle(), NX_ERR_BUSY);
    EXPECT_EQ(nx_dma_release_channel(dma), NX_OK);
    auto callback = [](void*) {};
    auto* manager = nx_isr_manager_get();
    ASSERT_EQ(manager->connect(manager, 3, callback, nullptr, 6), NX_OK);
    EXPECT_EQ(nx_native_resources_idle(), NX_ERR_BUSY);
    ASSERT_EQ(nx_device_shutdown_begin(), NX_OK);
    EXPECT_EQ(nx_dma_allocate_channel(0, 0), nullptr);
    EXPECT_EQ(manager->connect(manager, 4, callback, nullptr, 6), NX_ERR_BUSY);
    EXPECT_EQ(manager->disconnect(manager, 3), NX_OK);
    EXPECT_EQ(nx_native_resources_idle(), NX_OK);
    EXPECT_EQ(nx_device_provider_quiescence_check(), NX_OK);
}
TEST_F(NativeProviders, DmaCallbackPinsContextAndRunsOutsideMetadataMask) {
    struct Context {
        nx_dma_channel_t* channel;
        bool called = false;
        nx_status_t release = NX_OK;
    };
    nx_dma_channel_t* dma = nx_dma_allocate_channel(0, 0);
    ASSERT_NE(dma, nullptr);
    Context context{dma};
    nx_dma_config_t cfg{};
    cfg.size = 4;
    cfg.data_width = 1;
    ASSERT_EQ(dma->configure(dma, &cfg), NX_OK);
    auto callback = [](void* opaque) {
        auto* ctx = static_cast<Context*>(opaque);
        EXPECT_FALSE(nx_arch_irq_is_masked());
        ctx->called = true;
        ctx->release = nx_dma_release_channel(ctx->channel);
        EXPECT_EQ(nx_native_resources_idle(), NX_ERR_BUSY);
    };
    ASSERT_EQ(dma->set_callback(dma, callback, &context), NX_OK);
    EXPECT_EQ(dma->start(dma), NX_OK);
    EXPECT_TRUE(context.called);
    EXPECT_EQ(context.release, NX_ERR_BUSY);
    EXPECT_EQ(nx_dma_release_channel(dma), NX_OK);
    EXPECT_EQ(nx_native_resources_idle(), NX_OK);
}

extern "C" {
#include "../../../soc/native/controllers/flash/nx_flash_helpers.h"
#include "../../../soc/native/controllers/uart/nx_uart_types.h"
}
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

class TypedNativeUART : public ::testing::Test {
  protected:
    nx_device_ref_t ref{};
    nx_uart_impl_t* impl = nullptr;
    void SetUp() override {
        ASSERT_EQ(nx_runtime_bootstrap(nullptr), NX_OK);
        ASSERT_EQ(nx_device_open("UART0", NX_DEVICE_CLASS_UART, 1, &ref),
                  NX_OK);
        impl = static_cast<nx_uart_impl_t*>(ref.descriptor->state->api);
    }
    void TearDown() override {
        if (ref.descriptor) {
            if (ref.descriptor->state->active_ticket) {
                nx_uart_result_t result{};
                EXPECT_EQ(
                    nx_device_uart_poll(
                        ref, {ref.descriptor->state->active_ticket}, &result),
                    NX_OK);
            }
            EXPECT_EQ(nx_device_close(ref), NX_OK);
        }
        EXPECT_EQ(nx_runtime_shutdown(nullptr), NX_OK);
    }
    void inject(const std::vector<uint8_t>& bytes, uint64_t timestamp) {
        // Explicit test producer, independent of production RX drain time.
        nx_arch_irq_state_t saved = nx_arch_irq_save();
        for (uint8_t byte : bytes) {
            if (impl->rx_event_count == impl->rx_event_capacity ||
                impl->rx_event_dropped) {
                if (!impl->rx_event_dropped)
                    impl->rx_drop_timestamp = timestamp;
                if (impl->rx_event_dropped != UINT32_MAX)
                    ++impl->rx_event_dropped;
            } else {
                size_t tail = (impl->rx_event_head + impl->rx_event_count) %
                              impl->rx_event_capacity;
                impl->rx_events[tail] = {timestamp, 1000, 0, byte, true, NX_OK};
                ++impl->rx_event_count;
            }
        }
        nx_arch_irq_restore(saved);
    }
};
TEST_F(TypedNativeUART, TypedCopySettlesOnlyThroughActualTicketQuery) {
    nx_device_caps_t caps{};
    ASSERT_EQ(nx_device_query(ref, &caps), NX_OK);
    const uint32_t required = NX_DEVICE_CAP_UART_OPERATIONS |
                              NX_DEVICE_CAP_UART_CANCEL |
                              NX_DEVICE_CAP_UART_RX_EVENTS;
    EXPECT_EQ(caps.flags & required, required);
    uint8_t data[] = {1, 2, 3};
    nx_uart_ticket_t ticket{}, other{};
    ASSERT_EQ(nx_device_uart_submit(ref, data, sizeof(data), 50, &ticket),
              NX_OK);
    EXPECT_NE(ticket.sequence, 0u);
    EXPECT_EQ(nx_device_close(ref), NX_ERR_BUSY);
    EXPECT_EQ(nx_device_uart_submit(ref, data, 1, 50, &other), NX_ERR_BUSY);
    EXPECT_EQ(other.sequence, 0u);
    nx_uart_result_t result{};
    ASSERT_EQ(nx_device_uart_poll(ref, ticket, &result), NX_OK);
    EXPECT_EQ(result.status, NX_OK);
    EXPECT_TRUE(result.settled);
    EXPECT_TRUE(result.wire_idle);
    EXPECT_EQ(result.transferred, sizeof(data));
    data[0] = 99;
    EXPECT_EQ(impl->state->tx_buf.data[0], 1u);
    EXPECT_EQ(impl->state->tx_buf.count, sizeof(data));
}
TEST_F(TypedNativeUART, FullAndZeroBudgetNeverPartiallyAdmitStorage) {
    std::vector<uint8_t> data(impl->state->tx_buf.size + 1, 0x42);
    nx_uart_ticket_t ticket{};
    EXPECT_EQ(nx_device_uart_submit(ref, data.data(), data.size(), 50, &ticket),
              NX_ERR_FULL);
    EXPECT_EQ(ticket.sequence, 0u);
    EXPECT_EQ(impl->state->tx_buf.count, 0u);
    EXPECT_EQ(nx_device_uart_submit(ref, data.data(), 1, 0, &ticket),
              NX_ERR_TIMEOUT);
    EXPECT_EQ(ticket.sequence, 0u);
    EXPECT_EQ(impl->state->tx_buf.count, 0u);
    ASSERT_EQ(
        nx_device_uart_submit(ref, data.data(), data.size() - 1, 50, &ticket),
        NX_OK);
    nx_uart_result_t result{};
    ASSERT_EQ(nx_device_uart_poll(ref, ticket, &result), NX_OK);
    EXPECT_EQ(result.transferred, data.size() - 1);
    size_t head = impl->state->tx_buf.head;
    EXPECT_EQ(nx_device_uart_submit(ref, data.data(), 1, 50, &ticket),
              NX_ERR_FULL);
    EXPECT_EQ(impl->state->tx_buf.head, head);
    EXPECT_EQ(impl->state->tx_buf.count, data.size() - 1);
}
TEST_F(TypedNativeUART, OldOwnerAndOldTicketCannotAddressReopenedModel) {
    uint8_t byte = 3;
    nx_uart_ticket_t previous{}, ticket{};
    nx_uart_result_t result{};
    ASSERT_EQ(nx_device_uart_submit(ref, &byte, 1, 50, &previous), NX_OK);
    ASSERT_EQ(nx_device_uart_poll(ref, previous, &result), NX_OK);
    nx_device_ref_t old = ref;
    ASSERT_EQ(nx_device_close(ref), NX_OK);
    ref = {};
    ASSERT_EQ(nx_device_open("UART0", NX_DEVICE_CLASS_UART, 2, &ref), NX_OK);
    ASSERT_EQ(nx_device_uart_submit(ref, &byte, 1, 50, &ticket), NX_OK);
    EXPECT_GT(ticket.sequence, previous.sequence);
    EXPECT_EQ(nx_device_uart_poll(old, previous, &result),
              NX_ERR_INVALID_STATE);
    EXPECT_EQ(nx_device_uart_cancel(ref, previous), NX_ERR_INVALID_STATE);
    EXPECT_EQ(nx_device_uart_cancel(ref, ticket), NX_OK);
    ASSERT_EQ(nx_device_uart_poll(ref, ticket, &result), NX_OK);
    // Cancellation cannot undo a model copy that is already terminal.
    EXPECT_EQ(result.status, NX_OK);
    EXPECT_TRUE(result.settled);
}
TEST_F(TypedNativeUART, SequenceExhaustionRejectsInsteadOfReturningZeroTicket) {
    impl->sequence = UINT64_MAX;
    uint8_t byte = 3;
    nx_uart_ticket_t ticket{};
    EXPECT_EQ(nx_device_uart_submit(ref, &byte, 1, 50, &ticket),
              NX_ERR_NO_RESOURCE);
    EXPECT_EQ(ticket.sequence, 0u);
    EXPECT_EQ(impl->state->tx_buf.count, 0u);
    EXPECT_EQ(ref.descriptor->state->phase, NX_DEVICE_OPEN);
}
TEST_F(TypedNativeUART, ReceivePreservesProducerTimestampAndOrderedOverflow) {
    std::vector<uint8_t> bytes(impl->rx_event_capacity + 2, 0x31);
    inject(bytes, 123000);
    nx_uart_rx_event_t event{};
    for (size_t i = 0; i < impl->rx_event_capacity; ++i) {
        ASSERT_EQ(nx_device_uart_receive_event(ref, &event), NX_OK);
        EXPECT_TRUE(event.has_data);
        EXPECT_EQ(event.data, 0x31u);
        EXPECT_EQ(event.timestamp_us, 123000u);
        EXPECT_EQ(event.resolution_us, 1000u);
    }
    inject({0x55}, 124000);  // A pending loss record precedes newer data.
    ASSERT_EQ(nx_device_uart_receive_event(ref, &event), NX_OK);
    EXPECT_EQ(event.status, NX_ERR_FULL);
    EXPECT_FALSE(event.has_data);
    EXPECT_EQ(event.raw_error, 3u);
    EXPECT_EQ(event.timestamp_us, 123000u);
    EXPECT_EQ(nx_device_uart_receive_event(ref, &event), NX_ERR_NO_DATA);
    inject({0x56}, 125000);
    ASSERT_EQ(nx_device_uart_receive_event(ref, &event), NX_OK);
    EXPECT_EQ(event.data, 0x56u);
    EXPECT_EQ(event.timestamp_us, 125000u);
}

class TypedNativeFlash : public ::testing::Test {
  protected:
    nx_device_ref_t ref{};
    nx_flash_impl_t* impl = nullptr;
    std::filesystem::path directory;
    std::string original_path;
    void SetUp() override {
        ASSERT_EQ(nx_runtime_bootstrap(nullptr), NX_OK);
        auto* api = static_cast<nx_internal_flash_t*>(
            nx_device_get_checked("FLASH0", NX_DEVICE_CLASS_FLASH));
        ASSERT_NE(api, nullptr);
        impl = NX_CONTAINER_OF(api, nx_flash_impl_t, base);
        original_path = impl->state->backing_file;
        auto nonce =
            std::chrono::steady_clock::now().time_since_epoch().count();
        directory =
            std::filesystem::temp_directory_path() /
            (std::string("nexus-native-flash-") +
             ::testing::UnitTest::GetInstance()->current_test_info()->name() +
             "-" + std::to_string(nonce));
        ASSERT_TRUE(std::filesystem::create_directory(directory));
        set_path(directory / "image.bin");
    }
    void TearDown() override {
        // Restore a writable target before retrying an intentionally failed
        // close.
        set_path(directory / "image.bin");
        if (ref.descriptor) {
            EXPECT_EQ(nx_device_close(ref), NX_OK);
        }
        std::strncpy(impl->state->backing_file, original_path.c_str(),
                     sizeof(impl->state->backing_file) - 1);
        EXPECT_EQ(nx_runtime_shutdown(nullptr), NX_OK);
        std::filesystem::remove_all(directory);
    }
    void set_path(const std::filesystem::path& path) {
        auto value = path.string();
        ASSERT_LT(value.size(), sizeof(impl->state->backing_file));
        std::strcpy(impl->state->backing_file, value.c_str());
    }
    void open() {
        ASSERT_EQ(nx_device_open("FLASH0", NX_DEVICE_CLASS_FLASH, 1, &ref),
                  NX_OK);
    }
};
TEST_F(TypedNativeFlash,
       PhysicalModelGeometryAndExactEraseDoNotRoundOrOverflow) {
    open();
    nx_flash_geometry_t geometry{};
    nx_flash_block_t block{};
    ASSERT_EQ(nx_device_flash_geometry(ref, &geometry), NX_OK);
    EXPECT_EQ(geometry.size_bytes, NX_FLASH_TOTAL_SIZE);
    EXPECT_EQ(geometry.block_count, NX_FLASH_NUM_SECTORS);
    EXPECT_EQ(geometry.program_alignment, NX_FLASH_WRITE_UNIT);
    EXPECT_EQ(geometry.erased_value, 0xFFu);
    EXPECT_EQ(geometry.flags, 0u);
    ASSERT_EQ(nx_device_flash_block(ref, NX_FLASH_TOTAL_SIZE - 1, &block),
              NX_OK);
    EXPECT_EQ(block.index, NX_FLASH_NUM_SECTORS - 1);
    EXPECT_EQ(nx_device_flash_block(ref, NX_FLASH_TOTAL_SIZE, &block),
              NX_ERR_INVALID_PARAM);
    ASSERT_EQ(nx_device_flash_set_write_enabled(ref, true), NX_OK);
    // Exercise the private compatibility entry against the same strict path.
    EXPECT_EQ(impl->base.erase(&impl->base, 0, 1), NX_ERR_INVALID_PARAM);
    EXPECT_EQ(impl->base.erase(&impl->base, 1, NX_FLASH_SECTOR_SIZE),
              NX_ERR_INVALID_PARAM);
    if (sizeof(size_t) > 4) {
        EXPECT_EQ(impl->base.erase(&impl->base, 0,
                                   static_cast<size_t>(UINT32_MAX) + 1),
                  NX_ERR_INVALID_PARAM);
    }
    EXPECT_EQ(impl->base.erase(&impl->base,
                               NX_FLASH_TOTAL_SIZE - NX_FLASH_SECTOR_SIZE,
                               NX_FLASH_SECTOR_SIZE * 2),
              NX_ERR_INVALID_PARAM);
}
TEST_F(TypedNativeFlash, RegionUnlockProgramSyncReadbackAndStaleLifetime) {
    open();
    nx_device_flash_region_t region{};
    ASSERT_EQ(nx_device_flash_region_open(ref, 0, NX_FLASH_SECTOR_SIZE,
                                          NX_FLASH_REGION_READ |
                                              NX_FLASH_REGION_PROGRAM |
                                              NX_FLASH_REGION_ERASE,
                                          &region),
              NX_OK);
    uint8_t data[] = {1, 2, 3, 4}, read[4] = {};
    EXPECT_EQ(nx_device_flash_erase(region, 0, NX_FLASH_SECTOR_SIZE, 50),
              NX_ERR_PERMISSION);
    ASSERT_EQ(nx_device_flash_set_write_enabled(ref, true), NX_OK);
    ASSERT_EQ(nx_device_flash_erase(region, 0, NX_FLASH_SECTOR_SIZE, 50),
              NX_OK);
    EXPECT_EQ(nx_device_flash_program(region, 0, data, sizeof(data), 0),
              NX_ERR_TIMEOUT);
    ASSERT_EQ(nx_device_flash_program(region, 0, data, sizeof(data), 50),
              NX_OK);
    EXPECT_EQ(nx_device_close(ref), NX_ERR_BUSY);
    ASSERT_EQ(nx_device_flash_sync(ref, 10000), NX_OK);
    ASSERT_EQ(nx_device_flash_read(region, 0, read, sizeof(read)), NX_OK);
    EXPECT_EQ(std::memcmp(read, data, sizeof(data)), 0);
    ASSERT_EQ(nx_device_flash_region_close(region), NX_OK);
    nx_device_ref_t previous = ref;
    ASSERT_EQ(nx_device_close(ref), NX_OK);
    ref = {};
    open();
    EXPECT_NE(ref.generation, previous.generation);
    EXPECT_EQ(nx_device_flash_read(region, 0, read, sizeof(read)),
              NX_ERR_INVALID_STATE);
    ASSERT_EQ(nx_device_flash_region_open(ref, 0, NX_FLASH_SECTOR_SIZE,
                                          NX_FLASH_REGION_READ, &region),
              NX_OK);
    ASSERT_EQ(nx_device_flash_read(region, 0, read, sizeof(read)), NX_OK);
    EXPECT_EQ(std::memcmp(read, data, sizeof(data)), 0);
    ASSERT_EQ(nx_device_flash_region_close(region), NX_OK);
}
TEST_F(TypedNativeFlash,
       FileSyncAndCloseFailurePreserveOwnerUntilSuccessfulRetry) {
    open();
    set_path(directory / "missing" / "image.bin");
    EXPECT_EQ(nx_device_flash_sync(ref, 10000), NX_ERR_IO);
    EXPECT_EQ(nx_device_close(ref), NX_ERR_IO);
    EXPECT_EQ(ref.descriptor->state->phase, NX_DEVICE_OPEN);
    EXPECT_EQ(impl->state->initialized, true);
    set_path(directory / "image.bin");
    EXPECT_EQ(nx_device_close(ref), NX_OK);
    ref = {};
}
TEST_F(TypedNativeFlash,
       MalformedBackingImageCannotLookLikeAValidCompleteImage) {
    {
        std::ofstream image(directory / "image.bin", std::ios::binary);
        std::vector<char> content(NX_FLASH_TOTAL_SIZE + 1,
                                  static_cast<char>(0xFF));
        image.write(content.data(),
                    static_cast<std::streamsize>(content.size()));
    }
    EXPECT_EQ(nx_device_open("FLASH0", NX_DEVICE_CLASS_FLASH, 1, &ref),
              NX_ERR_IO);
    EXPECT_EQ(ref.descriptor, nullptr);
    EXPECT_FALSE(impl->state->initialized);
    std::filesystem::resize_file(directory / "image.bin",
                                 NX_FLASH_TOTAL_SIZE - 1);
    EXPECT_EQ(nx_device_open("FLASH0", NX_DEVICE_CLASS_FLASH, 1, &ref),
              NX_ERR_IO);
    EXPECT_EQ(ref.descriptor, nullptr);
    std::filesystem::resize_file(directory / "image.bin", NX_FLASH_TOTAL_SIZE);
    open();
}
