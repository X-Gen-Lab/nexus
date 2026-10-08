/** Observable lifetime/lease tests against the production registry core.
 * Fault ports model hardware ownership, not electrical timing or DMA behavior. */
#include "hal/base/nx_device.h"
#include "hal/nx_factory.h"
#include "arch/nx_arch.h"
#include <gtest/gtest.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

static thread_local bool simulated_isr;
extern "C" bool typed_test_in_isr(void) { return simulated_isr; }
extern "C" void typed_test_set_isr(bool enabled) { simulated_isr = enabled; }

namespace {
struct Port {
    nx_gpio_t gpio{};
    nx_uart_t uart{};
    nx_lifecycle_t life{};
    nx_uart_operations_t operations{};
    nx_device_state_t hardware = NX_DEV_STATE_UNINITIALIZED;
    nx_device_state_t state_after_open = NX_DEV_STATE_RUNNING;
    nx_status_t construction_status = NX_OK, open_status = NX_OK, close_status = NX_OK;
    nx_status_t cancel_status = NX_OK, terminal_status = NX_OK;
    unsigned constructions = 0, opens = 0, closes = 0;
    uint8_t level = 0;
    const uint8_t* borrowed = nullptr;
    bool settled = false, wire_idle = false;
    uint64_t sequence = 0;
    nx_device_ref_t nested_ref{};
    nx_status_t callback_close_status = NX_OK;
    std::mutex mutex;
    std::condition_variable wake;
    bool block_write = false, entered = false, release = false;
};
static Port* current;
static nx_status_t init(nx_lifecycle_t*) {
    EXPECT_FALSE(nx_arch_irq_is_masked());
    ++current->opens;
    current->hardware = current->state_after_open;
    return current->open_status;
}
static nx_status_t deinit(nx_lifecycle_t*) {
    EXPECT_FALSE(nx_arch_irq_is_masked());
    ++current->closes;
    if (current->close_status == NX_OK) current->hardware = NX_DEV_STATE_UNINITIALIZED;
    return current->close_status;
}
static nx_device_state_t state(nx_lifecycle_t*) { return current->hardware; }
static nx_lifecycle_t* gpio_lifecycle(nx_gpio_write_t*) { return &current->life; }
static nx_lifecycle_t* read_lifecycle(nx_gpio_read_t*) { return &current->life; }
static nx_lifecycle_t* uart_lifecycle(nx_uart_t*) { return &current->life; }
static nx_uart_operations_t* operations(nx_uart_t*) { return &current->operations; }
static uint8_t read(nx_gpio_read_t*) { return current->level; }
static void write(nx_gpio_write_t*, uint8_t value) {
    EXPECT_FALSE(nx_arch_irq_is_masked());
    if (current->nested_ref.descriptor) current->callback_close_status = nx_device_close(current->nested_ref);
    std::unique_lock<std::mutex> guard(current->mutex);
    if (current->block_write) {
        current->entered = true;
        current->wake.notify_all();
        current->wake.wait(guard, [] { return current->release; });
    }
    current->level = value;
}
static void toggle(nx_gpio_write_t* self) { write(self, current->level ^ 1); }
static nx_status_t construct(const nx_device_t* dev, void** out) {
    EXPECT_FALSE(nx_arch_irq_is_masked());
    ++current->constructions;
    if (current->construction_status != NX_OK) return current->construction_status;
    *out = dev->device_class == NX_DEVICE_CLASS_UART ? static_cast<void*>(&current->uart) : static_cast<void*>(&current->gpio);
    return NX_OK;
}
static nx_status_t submit(nx_uart_operations_t*, const uint8_t* data, size_t, uint32_t, nx_uart_ticket_t* ticket) {
    EXPECT_FALSE(nx_arch_irq_is_masked());
    if (current->borrowed) return NX_ERR_BUSY;
    current->borrowed = data;
    current->settled = false;
    current->wire_idle = false;
    ticket->sequence = ++current->sequence;
    return NX_OK;
}
static nx_status_t poll(nx_uart_operations_t*, nx_uart_ticket_t ticket, nx_uart_result_t* result) {
    if (ticket.sequence != current->sequence) return NX_ERR_INVALID_STATE;
    *result = {current->settled ? current->terminal_status : NX_ERR_BUSY, 0, current->settled, current->wire_idle};
    if (current->settled) current->borrowed = nullptr;
    return NX_OK;
}
static nx_status_t cancel(nx_uart_operations_t*, nx_uart_ticket_t ticket) {
    if (ticket.sequence != current->sequence) return NX_ERR_INVALID_STATE;
    if (current->cancel_status != NX_OK) return current->cancel_status;
    current->borrowed = nullptr;
    current->settled = true;
    current->terminal_status = NX_ERR_CANCELLED;
    return NX_OK;
}
static nx_status_t receive(nx_uart_operations_t*, nx_uart_rx_event_t*) { return NX_ERR_NO_DATA; }

class TypedDevice : public ::testing::Test {
protected:
    Port port;
    nx_device_config_state_t storage{};
    nx_device_t descriptor{};
    nx_device_ref_t ref{};
    void SetUp() override {
        ASSERT_EQ(nx_device_registry_reset(), NX_OK);
        current = &port;
        port.life.init = init; port.life.deinit = deinit; port.life.get_state = state;
        port.gpio.read.read = read; port.gpio.read.get_lifecycle = read_lifecycle;
        port.gpio.write.write = write; port.gpio.write.toggle = toggle; port.gpio.write.get_lifecycle = gpio_lifecycle;
        port.uart.get_lifecycle = uart_lifecycle; port.uart.get_operations = operations;
        port.operations.submit = submit; port.operations.poll = poll;
        port.operations.cancel = cancel; port.operations.receive_event = receive;
        descriptor.name = "GPIOA0"; descriptor.state = &storage;
        descriptor.device_class = NX_DEVICE_CLASS_GPIO; descriptor.construct = construct;
        ASSERT_EQ(nx_device_register(&descriptor), NX_OK);
    }
    void TearDown() override {
        simulated_isr = false;
        nx_device_shutdown_end();
        port.close_status = NX_OK;
        if (storage.phase == NX_DEVICE_OPEN) {
            if (storage.active_ticket) {
                port.cancel_status = NX_OK;
                EXPECT_EQ(nx_device_uart_cancel(ref, {storage.active_ticket}), NX_OK);
            }
            EXPECT_EQ(nx_device_close(ref), NX_OK);
        } else if (storage.phase == NX_DEVICE_RECOVERY_REQUIRED) {
            EXPECT_EQ(nx_device_recover(descriptor.name, storage.owner), NX_OK);
        }
        EXPECT_EQ(nx_device_registry_reset(), NX_OK);
        current = nullptr;
    }
    void open() { ASSERT_EQ(nx_device_open(descriptor.name, descriptor.device_class, 1, &ref), NX_OK); }
    void uart() { descriptor.name = "UART0"; descriptor.device_class = NX_DEVICE_CLASS_UART; }
};

TEST_F(TypedDevice, DiscoveryProvesClassWithoutConstructionOrHardwareOpen) {
    const nx_device_t* found = nullptr;
    ASSERT_EQ(nx_device_discover("GPIOA0", NX_DEVICE_CLASS_GPIO, &found), NX_OK);
    EXPECT_EQ(found, &descriptor); EXPECT_EQ(port.constructions, 0u); EXPECT_EQ(port.opens, 0u);
    found = &descriptor;
    EXPECT_EQ(nx_device_discover("GPIOA0", NX_DEVICE_CLASS_UART, &found), NX_ERR_TYPE_MISMATCH);
    EXPECT_EQ(found, nullptr);
    EXPECT_EQ(nx_factory_uart(0), nullptr);
    EXPECT_EQ(nx_device_discover("absent", NX_DEVICE_CLASS_GPIO, &found), NX_ERR_NOT_FOUND);
}
TEST_F(TypedDevice, MisnamedDescriptorCannotBecomeFactoryTypeProof) {
    descriptor.name = "UART0";
    EXPECT_EQ(nx_factory_uart(0), nullptr);
    EXPECT_EQ(port.constructions, 0u);
}
TEST_F(TypedDevice, OpensHardwareOnceAndRejectsSecondOwner) {
    open();
    EXPECT_EQ(port.constructions, 1u); EXPECT_EQ(port.opens, 1u);
    nx_device_ref_t other{};
    EXPECT_EQ(nx_device_open(descriptor.name, descriptor.device_class, 2, &other), NX_ERR_BUSY);
    EXPECT_EQ(other.descriptor, nullptr);
    EXPECT_EQ(nx_device_get_checked(descriptor.name, descriptor.device_class), nullptr);
    EXPECT_EQ(nx_device_registry_reset(), NX_ERR_BUSY);
    EXPECT_EQ(nx_device_shutdown_begin(), NX_ERR_BUSY);
}
TEST_F(TypedDevice, PreciseConstructionFailureCanRetryWithInvalidOutput) {
    port.construction_status = NX_ERR_NO_MEMORY;
    EXPECT_EQ(nx_device_open(descriptor.name, descriptor.device_class, 1, &ref), NX_ERR_NO_MEMORY);
    EXPECT_EQ(ref.descriptor, nullptr); EXPECT_EQ(port.opens, 0u);
    port.construction_status = NX_OK; open();
    EXPECT_EQ(port.constructions, 2u);
}
TEST_F(TypedDevice, SimultaneousOwnersHaveExactlyOneHardwareAdmission) {
    constexpr unsigned count = 12;
    std::thread workers[count];
    nx_device_ref_t references[count]{};
    nx_status_t outcomes[count]{};
    std::atomic<bool> start{false};
    for (unsigned i = 0; i < count; ++i) workers[i] = std::thread([&, i] {
        while (!start.load()) std::this_thread::yield();
        outcomes[i] = nx_device_open(descriptor.name, descriptor.device_class, i + 1, &references[i]);
    });
    start = true;
    for (auto& worker : workers) worker.join();
    unsigned admitted = 0;
    for (unsigned i = 0; i < count; ++i) {
        if (outcomes[i] == NX_OK) { ++admitted; ref = references[i]; }
        else { EXPECT_EQ(outcomes[i], NX_ERR_BUSY); EXPECT_EQ(references[i].descriptor, nullptr); }
    }
    EXPECT_EQ(admitted, 1u); EXPECT_EQ(port.opens, 1u);
}
TEST_F(TypedDevice, HardwareOpenFailureIsCleanedBeforeRetry) {
    port.open_status = NX_ERR_HARDWARE;
    EXPECT_EQ(nx_device_open(descriptor.name, descriptor.device_class, 1, &ref), NX_ERR_HARDWARE);
    EXPECT_EQ(ref.descriptor, nullptr); EXPECT_EQ(port.closes, 1u);
    EXPECT_EQ(port.hardware, NX_DEV_STATE_UNINITIALIZED);
    port.open_status = NX_OK; open(); EXPECT_EQ(port.constructions, 1u);
}
TEST_F(TypedDevice, SuccessfulOpenStatusMustActuallyMakeHardwareReady) {
    port.state_after_open = NX_DEV_STATE_SUSPENDED;
    EXPECT_EQ(nx_device_open(descriptor.name, descriptor.device_class, 1, &ref), NX_ERR_NOT_READY);
    EXPECT_EQ(ref.descriptor, nullptr); EXPECT_EQ(port.closes, 1u);
}
TEST_F(TypedDevice, GPIOHardwareStateCannotTurnIgnoredWritesIntoSuccess) {
    open(); port.hardware = NX_DEV_STATE_SUSPENDED;
    EXPECT_EQ(nx_device_gpio_write(ref, 1), NX_ERR_SUSPENDED);
    uint8_t value = 99;
    EXPECT_EQ(nx_device_gpio_read(ref, &value), NX_ERR_SUSPENDED);
    EXPECT_EQ(value, 0); EXPECT_EQ(port.level, 0);
    port.hardware = NX_DEV_STATE_ERROR;
    EXPECT_EQ(nx_device_gpio_toggle(ref), NX_ERR_NOT_READY);
    port.hardware = NX_DEV_STATE_RUNNING;
    EXPECT_EQ(nx_device_gpio_write(ref, 1), NX_OK);
}
TEST_F(TypedDevice, FailedOpenCleanupQuarantinesUntilMatchingOwnerRecovers) {
    port.open_status = NX_ERR_HARDWARE; port.close_status = NX_ERR_IO;
    EXPECT_EQ(nx_device_open(descriptor.name, descriptor.device_class, 7, &ref), NX_ERR_IO);
    EXPECT_EQ(ref.descriptor, nullptr);
    EXPECT_EQ(nx_device_open(descriptor.name, descriptor.device_class, 9, &ref), NX_ERR_BUSY);
    EXPECT_EQ(nx_device_recover(descriptor.name, 9), NX_ERR_INVALID_STATE);
    EXPECT_EQ(nx_device_recover(descriptor.name, 7), NX_ERR_IO);
    port.close_status = NX_OK;
    EXPECT_EQ(nx_device_recover(descriptor.name, 7), NX_OK);
    port.open_status = NX_OK; open();
}
TEST_F(TypedDevice, LegacyHardwareOwnershipIsRejectedAndNeverStopped) {
    port.hardware = NX_DEV_STATE_RUNNING;
    EXPECT_EQ(nx_device_open(descriptor.name, descriptor.device_class, 1, &ref), NX_ERR_RESOURCE_BUSY);
    EXPECT_EQ(port.opens, 0u); EXPECT_EQ(port.closes, 0u);
}
TEST_F(TypedDevice, OldCopiedReferenceCannotActOnReopenedDevice) {
    open(); nx_device_ref_t old = ref;
    ASSERT_EQ(nx_device_gpio_write(ref, 1), NX_OK);
    ASSERT_EQ(nx_device_close(ref), NX_OK); ref = {};
    open(); EXPECT_NE(old.generation, ref.generation);
    EXPECT_EQ(nx_device_gpio_write(old, 1), NX_ERR_INVALID_STATE);
    EXPECT_EQ(nx_device_close(old), NX_ERR_INVALID_STATE);
    EXPECT_EQ(nx_device_gpio_write(ref, 0), NX_OK);
    uint8_t value = 42;
    EXPECT_EQ(nx_device_gpio_read(ref, &value), NX_OK); EXPECT_EQ(value, 0);
    nx_device_ref_t forged = ref; forged.owner = 99;
    EXPECT_EQ(nx_device_gpio_toggle(forged), NX_ERR_INVALID_STATE);
}
TEST_F(TypedDevice, GenerationExhaustionNeverReusesToken) {
    storage.generation = UINT64_MAX;
    EXPECT_EQ(nx_device_open(descriptor.name, descriptor.device_class, 1, &ref), NX_ERR_NO_RESOURCE);
    EXPECT_EQ(ref.descriptor, nullptr); EXPECT_EQ(port.constructions, 0u);
}
TEST_F(TypedDevice, UnregisteredReferenceNeverDereferencesForgedDescriptor) {
    nx_device_ref_t unknown{reinterpret_cast<const nx_device_t*>(uintptr_t{1}), 1, 1, NX_DEVICE_CLASS_GPIO};
    EXPECT_EQ(nx_device_gpio_write(unknown, 1), NX_ERR_NOT_FOUND);
    EXPECT_EQ(nx_device_close(unknown), NX_ERR_NOT_FOUND);
    nx_device_caps_t caps{};
    EXPECT_EQ(nx_device_query(unknown, &caps), NX_ERR_NOT_FOUND);
}
TEST_F(TypedDevice, FailedCloseRetainsReferenceForRetry) {
    open(); port.close_status = NX_ERR_BUSY;
    EXPECT_EQ(nx_device_close(ref), NX_ERR_BUSY);
    EXPECT_EQ(nx_device_gpio_write(ref, 1), NX_OK);
    port.close_status = NX_OK;
    EXPECT_EQ(nx_device_close(ref), NX_OK); ref = {};
}
TEST_F(TypedDevice, ReentrantCloseCannotDestroyRunningDriverCall) {
    open(); port.nested_ref = ref;
    EXPECT_EQ(nx_device_gpio_write(ref, 1), NX_OK);
    EXPECT_EQ(port.callback_close_status, NX_ERR_BUSY);
    EXPECT_EQ(port.closes, 0u);
}
TEST_F(TypedDevice, ConcurrentCloseAndActionRejectUntilDriverCallExits) {
    open(); port.block_write = true;
    std::atomic<int> outcome{NX_ERR_GENERIC};
    std::thread worker([&] { outcome = nx_device_gpio_write(ref, 1); });
    {
        std::unique_lock<std::mutex> guard(port.mutex);
        EXPECT_TRUE(port.wake.wait_for(guard, std::chrono::seconds(2), [&] { return port.entered; }));
    }
    EXPECT_EQ(nx_device_close(ref), NX_ERR_BUSY);
    EXPECT_EQ(nx_device_gpio_toggle(ref), NX_ERR_BUSY);
    {
        std::lock_guard<std::mutex> guard(port.mutex); port.release = true;
    }
    port.wake.notify_all(); worker.join(); EXPECT_EQ(outcome.load(), NX_OK);
    EXPECT_EQ(nx_device_close(ref), NX_OK); ref = {};
}
TEST_F(TypedDevice, InterruptContextNeverStartsOrClosesHardware) {
    const nx_device_t* found = nullptr;
    simulated_isr = true;
    EXPECT_EQ(nx_device_discover(descriptor.name, descriptor.device_class, &found), NX_ERR_CONTEXT);
    EXPECT_EQ(nx_device_open(descriptor.name, descriptor.device_class, 1, &ref), NX_ERR_CONTEXT);
    EXPECT_EQ(nx_device_close(ref), NX_ERR_CONTEXT);
    EXPECT_EQ(port.opens, 0u);
}
TEST_F(TypedDevice, ShutdownFenceRejectsNewOwnerUntilReleased) {
    ASSERT_EQ(nx_device_shutdown_begin(), NX_OK);
    EXPECT_EQ(nx_device_open(descriptor.name, descriptor.device_class, 1, &ref), NX_ERR_BUSY);
    EXPECT_EQ(nx_device_get_checked(descriptor.name, descriptor.device_class), nullptr);
    EXPECT_EQ(nx_device_shutdown_begin(), NX_ERR_BUSY);
    nx_device_shutdown_end(); open();
}
TEST_F(TypedDevice, UartLeaseBlocksCloseEvenWhenMemoryCompletesBeforeWire) {
    uart(); open();
    uint8_t bytes[] = {1, 2, 3}; nx_uart_ticket_t ticket{};
    ASSERT_EQ(nx_device_uart_submit(ref, bytes, sizeof(bytes), 20, &ticket), NX_OK);
    EXPECT_EQ(nx_device_close(ref), NX_ERR_BUSY);
    nx_uart_result_t result{};
    EXPECT_EQ(nx_device_uart_poll(ref, ticket, &result), NX_OK);
    EXPECT_FALSE(result.settled); EXPECT_FALSE(result.wire_idle);
    nx_uart_ticket_t another{};
    EXPECT_EQ(nx_device_uart_submit(ref, bytes, 1, 20, &another), NX_ERR_BUSY);
    EXPECT_EQ(another.sequence, 0u);
    EXPECT_EQ(nx_device_uart_poll(ref, {ticket.sequence + 1}, &result), NX_ERR_INVALID_STATE);
    port.settled = true; port.wire_idle = true;
    EXPECT_EQ(nx_device_uart_poll(ref, ticket, &result), NX_OK);
    EXPECT_TRUE(result.settled); EXPECT_TRUE(result.wire_idle); EXPECT_EQ(result.status, NX_OK);
    EXPECT_EQ(nx_device_close(ref), NX_OK); ref = {};
}
TEST_F(TypedDevice, UartFailedCancellationRetainsBufferUntilSettledError) {
    uart(); open(); uint8_t bytes[] = {7}; nx_uart_ticket_t ticket{};
    ASSERT_EQ(nx_device_uart_submit(ref, bytes, 1, 20, &ticket), NX_OK);
    port.cancel_status = NX_ERR_HARDWARE;
    EXPECT_EQ(nx_device_uart_cancel(ref, ticket), NX_ERR_HARDWARE);
    EXPECT_EQ(port.borrowed, bytes); EXPECT_EQ(nx_device_close(ref), NX_ERR_BUSY);
    port.settled = true; port.terminal_status = NX_ERR_HARDWARE;
    nx_uart_result_t result{};
    EXPECT_EQ(nx_device_uart_poll(ref, ticket, &result), NX_OK);
    EXPECT_TRUE(result.settled); EXPECT_EQ(result.status, NX_ERR_HARDWARE);
    EXPECT_EQ(port.borrowed, nullptr); EXPECT_EQ(nx_device_close(ref), NX_OK); ref = {};
}
TEST_F(TypedDevice, UartSuccessfulCancelSettlesAndOldTicketCannotCancelNewTransfer) {
    uart(); open(); uint8_t bytes[] = {7}; nx_uart_ticket_t old{}, next{};
    ASSERT_EQ(nx_device_uart_submit(ref, bytes, 1, 20, &old), NX_OK);
    EXPECT_EQ(nx_device_uart_cancel(ref, old), NX_OK); EXPECT_EQ(port.borrowed, nullptr);
    ASSERT_EQ(nx_device_uart_submit(ref, bytes, 1, 20, &next), NX_OK);
    EXPECT_EQ(nx_device_uart_cancel(ref, old), NX_ERR_INVALID_STATE);
    EXPECT_EQ(port.borrowed, bytes);
    EXPECT_EQ(nx_device_uart_cancel(ref, next), NX_OK);
}
TEST_F(TypedDevice, UartCapabilitiesReportAbsentOperationsHonestly) {
    uart(); open(); nx_device_caps_t caps{};
    EXPECT_EQ(nx_device_query(ref, &caps), NX_OK);
    EXPECT_EQ(caps.flags, NX_DEVICE_CAP_UART_OPERATIONS | NX_DEVICE_CAP_UART_CANCEL | NX_DEVICE_CAP_UART_RX_EVENTS);
    port.uart.get_operations = nullptr;
    EXPECT_EQ(nx_device_query(ref, &caps), NX_OK); EXPECT_EQ(caps.flags, 0u);
    uint8_t byte = 1; nx_uart_ticket_t ticket{};
    EXPECT_EQ(nx_device_uart_submit(ref, &byte, 1, 20, &ticket), NX_ERR_NOT_SUPPORTED);
    EXPECT_EQ(ticket.sequence, 0u);
}
TEST_F(TypedDevice, NewOwnerCannotQueryPreviousOwnersSettledTicket) {
    uart(); open(); uint8_t byte = 7; nx_uart_ticket_t old{};
    ASSERT_EQ(nx_device_uart_submit(ref, &byte, 1, 20, &old), NX_OK);
    ASSERT_EQ(nx_device_uart_cancel(ref, old), NX_OK);
    ASSERT_EQ(nx_device_close(ref), NX_OK); ref = {};
    open(); nx_uart_result_t result{};
    EXPECT_EQ(nx_device_uart_poll(ref, old, &result), NX_ERR_INVALID_STATE);
    EXPECT_EQ(nx_device_uart_cancel(ref, old), NX_ERR_INVALID_STATE);
}
TEST(RegistryRegion, IntegerBoundsRejectPartialMisalignedOrReversedTables) {
    size_t count = 99;
    EXPECT_EQ(nx_device_validate_registry_region(0, 0, &count), NX_OK); EXPECT_EQ(count, 0u);
    EXPECT_EQ(nx_device_validate_registry_region(0, 0x1000, &count), NX_ERR_INVALID_STATE);
    EXPECT_EQ(nx_device_validate_registry_region(0x1000, 0, &count), NX_ERR_INVALID_STATE);
    EXPECT_EQ(nx_device_validate_registry_region(0x1000, 0x800, &count), NX_ERR_INVALID_STATE);
    EXPECT_EQ(nx_device_validate_registry_region(0x1001, 0x1001 + sizeof(nx_device_t), &count), NX_ERR_INVALID_STATE);
    EXPECT_EQ(nx_device_validate_registry_region(0x1000, 0x1000 + sizeof(nx_device_t) + sizeof(void*), &count), NX_ERR_INVALID_STATE);
    EXPECT_EQ(nx_device_validate_registry_region(0x1000, 0x1000 + 3 * sizeof(nx_device_t), &count), NX_OK);
    EXPECT_EQ(count, 3u);
}
} // namespace
