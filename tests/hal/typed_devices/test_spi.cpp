/** User-visible SPI ownership and settlement outcomes against production HAL. */
#include "hal/base/nx_device.h"
#include "arch/nx_arch.h"
#include <gtest/gtest.h>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>
#include <cstring>
extern "C" void typed_test_set_isr(bool enabled);

namespace {
struct Slave {
    nx_spi_device_config_t config{};
    nx_spi_transaction_t queued{};
    uint64_t token = 0;
    bool allocated = false, pending = false, cancelled = false;
};
struct Bus {
    nx_spi_bus_t api{};
    nx_lifecycle_t life{};
    nx_device_state_t hardware = NX_DEV_STATE_UNINITIALIZED;
    Slave slaves[NX_DEVICE_SPI_MAX_CHILDREN + 1]{};
    uint64_t token = 0;
    unsigned opens = 0, closes = 0, transfers = 0, cancels = 0;
    nx_status_t open_status = NX_OK, close_status = NX_OK, cancel_status = NX_OK, submit_status = NX_OK;
    bool malformed_handle = false, queue_supported = true, cancel_supported = true;
    uint32_t last_budget = 0;
    std::mutex mutex;
    std::condition_variable wake;
    bool block = false, entered = false, release = false;
};
static Bus* bus;
static Slave* resolve(nx_spi_device_t* value) {
    for (auto& slave : bus->slaves) if (slave.allocated && slave.token == value->token) return &slave;
    return nullptr;
}
static nx_status_t init(nx_lifecycle_t*) { bus->hardware = NX_DEV_STATE_RUNNING; return NX_OK; }
static nx_status_t deinit(nx_lifecycle_t*) { bus->hardware = NX_DEV_STATE_UNINITIALIZED; return NX_OK; }
static nx_device_state_t state(nx_lifecycle_t*) { return bus->hardware; }
static nx_lifecycle_t* lifecycle(void*) { return &bus->life; }
static nx_status_t construct(const nx_device_t*, void** api) { *api = &bus->api; return NX_OK; }
static nx_status_t execute(nx_spi_device_t* value, const nx_spi_transaction_t* transaction) {
    EXPECT_FALSE(nx_arch_irq_is_masked());
    std::unique_lock<std::mutex> lock(bus->mutex);
    Slave* slave = resolve(value);
    if (!slave) return NX_ERR_INVALID_STATE;
    ++bus->transfers; bus->last_budget = transaction->timeout_ms;
    if (bus->block) {
        bus->entered = true; bus->wake.notify_all();
        bus->wake.wait(lock, [&] { return bus->release || slave->cancelled; });
    }
    nx_status_t status = slave->cancelled ? NX_ERR_CANCELLED : NX_OK;
    if (status == NX_OK && transaction->rx_data)
        std::memcpy(transaction->rx_data, transaction->tx_data, transaction->length);
    lock.unlock();
    if (transaction->callback) transaction->callback(transaction->user_data, status);
    return status;
}
static nx_status_t submit(nx_spi_device_t* value, const nx_spi_transaction_t* transaction) {
    EXPECT_FALSE(nx_arch_irq_is_masked());
    std::lock_guard<std::mutex> lock(bus->mutex);
    Slave* slave = resolve(value);
    if (!slave) return NX_ERR_INVALID_STATE;
    if (bus->submit_status != NX_OK) return bus->submit_status;
    if (slave->pending) return NX_ERR_BUSY;
    slave->queued = *transaction; slave->pending = true; slave->cancelled = false;
    return NX_OK;
}
static nx_status_t cancel(nx_spi_device_t* value) {
    EXPECT_FALSE(nx_arch_irq_is_masked());
    std::lock_guard<std::mutex> lock(bus->mutex);
    ++bus->cancels;
    Slave* slave = resolve(value);
    if (!slave) return NX_ERR_INVALID_STATE;
    if (bus->cancel_status != NX_OK) return bus->cancel_status;
    slave->cancelled = true; bus->wake.notify_all(); return NX_OK;
}
static nx_status_t open(nx_spi_bus_t* api, const nx_spi_device_config_t* config, nx_spi_device_t* out) {
    EXPECT_FALSE(nx_arch_irq_is_masked());
    ++bus->opens;
    if (bus->open_status != NX_OK) return bus->open_status;
    for (auto& slave : bus->slaves) if (!slave.allocated) {
        slave = {}; slave.allocated = true; slave.token = ++bus->token; slave.config = *config;
        *out = {api, slave.token, bus->malformed_handle ? nullptr : execute,
                bus->queue_supported ? submit : nullptr, bus->cancel_supported ? cancel : nullptr};
        return NX_OK;
    }
    return NX_ERR_NO_RESOURCE;
}
static nx_status_t close(nx_spi_bus_t*, nx_spi_device_t* value) {
    EXPECT_FALSE(nx_arch_irq_is_masked());
    ++bus->closes;
    if (bus->close_status != NX_OK) return bus->close_status;
    Slave* slave = resolve(value);
    if (!slave) return NX_ERR_INVALID_STATE;
    if (slave->pending) return NX_ERR_BUSY;
    slave->allocated = false; return NX_OK;
}
static nx_status_t service(nx_spi_bus_t*) {
    nx_spi_device_t value{};
    nx_spi_transaction_t transaction{};
    {
        std::lock_guard<std::mutex> lock(bus->mutex);
        Slave* selected = nullptr;
        for (auto& slave : bus->slaves) if (slave.pending) { selected = &slave; break; }
        if (!selected) return NX_ERR_NO_DATA;
        value = {&bus->api, selected->token, execute, submit, cancel};
        transaction = selected->queued; selected->pending = false;
    }
    return execute(&value, &transaction);
}
class TypedSPI : public ::testing::Test {
protected:
    Bus model;
    nx_device_config_state_t storage{};
    nx_device_t descriptor{};
    nx_device_ref_t controller{};
    std::vector<nx_device_spi_ref_t> opened;
    uint8_t tx[3]{1, 2, 3}, rx[3]{};
    nx_spi_device_config_t config{4, 1000000, NX_SPI_MODE_1, NX_SPI_BIT_ORDER_MSB};
    nx_spi_transaction_t transaction{tx, rx, sizeof(tx), 100, nullptr, nullptr};
    void SetUp() override {
        ASSERT_EQ(nx_device_registry_reset(), NX_OK);
        bus = &model;
        model.life.init = init; model.life.deinit = deinit; model.life.get_state = state;
        model.api.open_device = open; model.api.close_device = close; model.api.service = service;
        descriptor.name = "SPI7"; descriptor.state = &storage;
        descriptor.device_class = NX_DEVICE_CLASS_SPI; descriptor.construct = construct;
        descriptor.get_lifecycle = lifecycle;
        ASSERT_EQ(nx_device_register(&descriptor), NX_OK);
        ASSERT_EQ(nx_device_open("SPI7", NX_DEVICE_CLASS_SPI, 41, &controller), NX_OK);
    }
    void TearDown() override {
        model.block = false; model.release = true; model.close_status = NX_OK; model.wake.notify_all();
        while (nx_device_spi_service(controller) != NX_ERR_NO_DATA) {
            if (storage.phase != NX_DEVICE_OPEN) break;
        }
        for (auto ref : opened) {
            nx_status_t status = nx_device_spi_close(ref);
            EXPECT_TRUE(status == NX_OK || status == NX_ERR_INVALID_STATE);
        }
        if (storage.phase == NX_DEVICE_OPEN) {
            nx_status_t recovered = nx_device_spi_recover(controller);
            EXPECT_TRUE(recovered == NX_OK || recovered == NX_ERR_NO_DATA);
            EXPECT_EQ(nx_device_close(controller), NX_OK);
        }
        EXPECT_EQ(nx_device_registry_reset(), NX_OK);
        bus = nullptr;
    }
    nx_device_spi_ref_t child() {
        nx_device_spi_ref_t ref{};
        EXPECT_EQ(nx_device_spi_open(controller, &config, &ref), NX_OK);
        opened.push_back(ref); return ref;
    }
};

TEST_F(TypedSPI, ChildRetainsParentOwnershipAndCopiedConfig) {
    auto ref = child(); config.speed = 42;
    EXPECT_EQ(model.slaves[0].config.speed, 1000000u);
    EXPECT_EQ(nx_device_close(controller), NX_ERR_BUSY);
    EXPECT_EQ(nx_device_registry_reset(), NX_ERR_BUSY);
    EXPECT_EQ(nx_device_shutdown_begin(), NX_ERR_BUSY);
    ASSERT_EQ(nx_device_spi_close(ref), NX_OK);
    ASSERT_EQ(nx_device_close(controller), NX_OK);
}
TEST_F(TypedSPI, StaleChildCannotTouchReusedProviderSlotOrNewControllerGeneration) {
    auto old = child(); ASSERT_EQ(nx_device_spi_close(old), NX_OK);
    auto next = child(); EXPECT_EQ(old.slot, next.slot); EXPECT_NE(old.generation, next.generation);
    EXPECT_EQ(nx_device_spi_transfer(old, &transaction), NX_ERR_INVALID_STATE);
    EXPECT_EQ(nx_device_spi_close(old), NX_ERR_INVALID_STATE);
    EXPECT_EQ(model.transfers, 0u);
    ASSERT_EQ(nx_device_spi_close(next), NX_OK); ASSERT_EQ(nx_device_close(controller), NX_OK);
    ASSERT_EQ(nx_device_open("SPI7", NX_DEVICE_CLASS_SPI, 41, &controller), NX_OK);
    auto reopened = child();
    EXPECT_EQ(nx_device_spi_transfer(next, &transaction), NX_ERR_INVALID_STATE);
    EXPECT_EQ(nx_device_spi_transfer(reopened, &transaction), NX_OK);
}
TEST_F(TypedSPI, BoundedPoolRejectsAdditionalChildrenWithoutOpeningProvider) {
    for (unsigned i = 0; i < NX_DEVICE_SPI_MAX_CHILDREN; ++i) child();
    nx_device_spi_ref_t rejected{};
    EXPECT_EQ(nx_device_spi_open(controller, &config, &rejected), NX_ERR_NO_RESOURCE);
    EXPECT_EQ(rejected.slot, 0u); EXPECT_EQ(model.opens, NX_DEVICE_SPI_MAX_CHILDREN);
    EXPECT_EQ(nx_device_spi_transfer(opened[0], &transaction), NX_OK);
}
TEST_F(TypedSPI, OpenAndCloseErrorsPreserveRetryableOwnership) {
    model.open_status = NX_ERR_HARDWARE;
    nx_device_spi_ref_t failed{};
    EXPECT_EQ(nx_device_spi_open(controller, &config, &failed), NX_ERR_HARDWARE);
    EXPECT_EQ(failed.slot, 0u); EXPECT_EQ(storage.child_refs, 0u);
    model.open_status = NX_OK; auto ref = child(); model.close_status = NX_ERR_IO;
    EXPECT_EQ(nx_device_spi_close(ref), NX_ERR_IO);
    EXPECT_EQ(nx_device_close(controller), NX_ERR_BUSY);
    EXPECT_EQ(nx_device_spi_transfer(ref, &transaction), NX_OK);
    model.close_status = NX_OK; EXPECT_EQ(nx_device_spi_close(ref), NX_OK);
}
TEST_F(TypedSPI, IncompleteProviderHandleCleanupFailureQuarantinesParentUntilRecovery) {
    model.malformed_handle = true; model.close_status = NX_ERR_IO;
    nx_device_spi_ref_t failed{};
    EXPECT_EQ(nx_device_spi_open(controller, &config, &failed), NX_ERR_IO);
    EXPECT_EQ(failed.slot, 0u); EXPECT_EQ(nx_device_close(controller), NX_ERR_BUSY);
    EXPECT_EQ(nx_device_spi_recover(controller), NX_ERR_IO);
    model.close_status = NX_OK;
    EXPECT_EQ(nx_device_spi_recover(controller), NX_OK);
    EXPECT_EQ(storage.child_refs, 0u);
}
TEST_F(TypedSPI, TransferKeepsTotalBudgetAndZeroBudgetCannotStartHardware) {
    auto ref = child(); transaction.timeout_ms = 0;
    EXPECT_EQ(nx_device_spi_transfer(ref, &transaction), NX_ERR_TIMEOUT);
    EXPECT_EQ(model.transfers, 0u);
    transaction.timeout_ms = 27;
    ASSERT_EQ(nx_device_spi_transfer(ref, &transaction), NX_OK);
    EXPECT_EQ(model.last_budget, 27u); EXPECT_EQ(std::memcmp(tx, rx, sizeof(tx)), 0);
}
TEST_F(TypedSPI, BlockingActionsRejectSavedInterruptMaskWithoutCallingProvider) {
    auto ref = child(); nx_device_spi_ticket_t ticket{};
    ASSERT_EQ(nx_device_spi_submit(ref, &transaction, &ticket), NX_OK);
    auto saved = nx_arch_irq_save();
    EXPECT_EQ(nx_device_spi_transfer(ref, &transaction), NX_ERR_INVALID_STATE);
    EXPECT_EQ(nx_device_spi_service(controller), NX_ERR_INVALID_STATE);
    EXPECT_TRUE(nx_arch_irq_is_masked()); EXPECT_EQ(model.transfers, 0u);
    nx_arch_irq_restore(saved);
    EXPECT_EQ(nx_device_spi_service(controller), NX_OK);
}
TEST_F(TypedSPI, BlockingTransferPreventsChildAndParentCloseFromAnotherTask) {
    auto ref = child(); model.block = true; nx_status_t outcome = NX_OK;
    std::thread worker([&] { outcome = nx_device_spi_transfer(ref, &transaction); });
    { std::unique_lock<std::mutex> lock(model.mutex); model.wake.wait(lock, [&] { return model.entered; }); }
    EXPECT_EQ(nx_device_spi_close(ref), NX_ERR_BUSY);
    EXPECT_EQ(nx_device_close(controller), NX_ERR_BUSY);
    EXPECT_EQ(nx_device_spi_transfer(ref, &transaction), NX_ERR_BUSY);
    { std::lock_guard<std::mutex> lock(model.mutex); model.release = true; model.wake.notify_all(); }
    worker.join(); EXPECT_EQ(outcome, NX_OK); EXPECT_EQ(nx_device_spi_close(ref), NX_OK);
}
TEST_F(TypedSPI, CancelSuccessAndErrorBothRetainLeaseUntilTerminalService) {
    auto ref = child(); nx_device_spi_ticket_t ticket{};
    ASSERT_EQ(nx_device_spi_submit(ref, &transaction, &ticket), NX_OK);
    model.cancel_status = NX_ERR_IO;
    EXPECT_EQ(nx_device_spi_cancel(ref, ticket), NX_ERR_IO);
    EXPECT_EQ(nx_device_spi_close(ref), NX_ERR_BUSY);
    nx_device_spi_result_t result{};
    ASSERT_EQ(nx_device_spi_poll(ref, ticket, &result), NX_OK); EXPECT_FALSE(result.settled);
    model.cancel_status = NX_OK;
    EXPECT_EQ(nx_device_spi_cancel(ref, ticket), NX_OK);
    ASSERT_EQ(nx_device_spi_poll(ref, ticket, &result), NX_OK); EXPECT_FALSE(result.settled);
    EXPECT_EQ(nx_device_spi_close(ref), NX_ERR_BUSY);
    EXPECT_EQ(nx_device_spi_service(controller), NX_ERR_CANCELLED);
    ASSERT_EQ(nx_device_spi_poll(ref, ticket, &result), NX_OK);
    EXPECT_TRUE(result.settled); EXPECT_EQ(result.status, NX_ERR_CANCELLED);
}
TEST_F(TypedSPI, QueuedCancellationCanRunConcurrentlyWithBlockedService) {
    auto ref = child(); nx_device_spi_ticket_t ticket{};
    ASSERT_EQ(nx_device_spi_submit(ref, &transaction, &ticket), NX_OK);
    model.block = true; nx_status_t outcome = NX_OK;
    std::thread worker([&] { outcome = nx_device_spi_service(controller); });
    { std::unique_lock<std::mutex> lock(model.mutex); model.wake.wait(lock, [&] { return model.entered; }); }
    EXPECT_EQ(nx_device_spi_cancel(ref, ticket), NX_OK);
    worker.join(); EXPECT_EQ(outcome, NX_ERR_CANCELLED);
    nx_device_spi_result_t result{};
    ASSERT_EQ(nx_device_spi_poll(ref, ticket, &result), NX_OK); EXPECT_TRUE(result.settled);
}
TEST_F(TypedSPI, BlockingCancelRequestDoesNotReleaseFailedCancellationStorage) {
    auto ref = child(); model.block = true; model.cancel_status = NX_ERR_IO;
    nx_status_t outcome = NX_OK;
    std::thread worker([&] { outcome = nx_device_spi_transfer(ref, &transaction); });
    { std::unique_lock<std::mutex> lock(model.mutex); model.wake.wait(lock, [&] { return model.entered; }); }
    EXPECT_EQ(nx_device_spi_cancel_transfer(ref), NX_ERR_IO);
    EXPECT_EQ(nx_device_spi_close(ref), NX_ERR_BUSY);
    EXPECT_EQ(nx_device_close(controller), NX_ERR_BUSY);
    model.cancel_status = NX_OK;
    EXPECT_EQ(nx_device_spi_cancel_transfer(ref), NX_OK);
    worker.join(); EXPECT_EQ(outcome, NX_ERR_CANCELLED);
    EXPECT_EQ(rx[0], 0);
    EXPECT_EQ(nx_device_spi_cancel_transfer(ref), NX_ERR_NO_DATA);
    EXPECT_EQ(nx_device_spi_close(ref), NX_OK);
}
struct CallbackContext {
    nx_device_ref_t controller{};
    nx_device_spi_ref_t child{};
    nx_device_spi_ticket_t ticket{};
    nx_spi_transaction_t* transaction = nullptr;
    unsigned calls = 0;
};
static void reentry(void* user, nx_status_t status) {
    auto& context = *static_cast<CallbackContext*>(user);
    EXPECT_FALSE(nx_arch_irq_is_masked()); EXPECT_EQ(status, NX_OK); ++context.calls;
    EXPECT_EQ(nx_device_close(context.controller), NX_ERR_BUSY);
    EXPECT_EQ(nx_device_spi_close(context.child), NX_ERR_BUSY);
    EXPECT_EQ(nx_device_spi_transfer(context.child, context.transaction), NX_ERR_BUSY);
    EXPECT_EQ(nx_device_spi_service(context.controller), NX_ERR_BUSY);
    if (context.ticket.sequence) {
        nx_device_spi_result_t result{};
        ASSERT_EQ(nx_device_spi_poll(context.child, context.ticket, &result), NX_OK);
        EXPECT_FALSE(result.settled);
        nx_device_spi_ticket_t next{};
        EXPECT_EQ(nx_device_spi_submit(context.child, context.transaction, &next), NX_ERR_BUSY);
    }
}
TEST_F(TypedSPI, AsyncCallbackReturnsBeforeCompletionLeaseIsReleased) {
    auto ref = child(); CallbackContext context{controller, ref, {}, &transaction, 0};
    transaction.callback = reentry; transaction.user_data = &context;
    ASSERT_EQ(nx_device_spi_submit(ref, &transaction, &context.ticket), NX_OK);
    EXPECT_EQ(nx_device_spi_service(controller), NX_OK);
    EXPECT_EQ(context.calls, 1u);
    nx_device_spi_result_t result{};
    ASSERT_EQ(nx_device_spi_poll(ref, context.ticket, &result), NX_OK); EXPECT_TRUE(result.settled);
    EXPECT_EQ(nx_device_spi_close(ref), NX_OK);
}
TEST_F(TypedSPI, SynchronousCallbackAlsoKeepsBothReferencesPinned) {
    auto ref = child(); CallbackContext context{controller, ref, {}, &transaction, 0};
    transaction.callback = reentry; transaction.user_data = &context;
    EXPECT_EQ(nx_device_spi_transfer(ref, &transaction), NX_OK); EXPECT_EQ(context.calls, 1u);
}
TEST_F(TypedSPI, OldAndForeignTicketsCannotCancelLaterBorrowedStorage) {
    auto ref = child(); nx_device_spi_ticket_t old{}, next{};
    ASSERT_EQ(nx_device_spi_submit(ref, &transaction, &old), NX_OK);
    ASSERT_EQ(nx_device_spi_service(controller), NX_OK);
    ASSERT_EQ(nx_device_spi_submit(ref, &transaction, &next), NX_OK);
    EXPECT_NE(old.sequence, next.sequence);
    EXPECT_EQ(nx_device_spi_cancel(ref, old), NX_ERR_INVALID_STATE);
    nx_device_spi_result_t result{};
    EXPECT_EQ(nx_device_spi_poll(ref, old, &result), NX_ERR_INVALID_STATE);
    auto other = child();
    EXPECT_EQ(nx_device_spi_cancel(other, next), NX_ERR_INVALID_STATE);
    EXPECT_EQ(model.cancels, 0u);
    EXPECT_EQ(nx_device_spi_close(ref), NX_ERR_BUSY);
}
TEST_F(TypedSPI, CapabilityAbsenceIsExplicitRatherThanAWorkingQueueClaim) {
    model.queue_supported = false; model.cancel_supported = false;
    auto ref = child(); nx_device_caps_t caps{};
    ASSERT_EQ(nx_device_spi_query(ref, &caps), NX_OK);
    EXPECT_EQ(caps.flags, NX_DEVICE_CAP_SPI_DEVICES);
    nx_device_spi_ticket_t ticket{};
    EXPECT_EQ(nx_device_spi_submit(ref, &transaction, &ticket), NX_ERR_NOT_SUPPORTED);
    EXPECT_EQ(ticket.sequence, 0u);
    EXPECT_EQ(nx_device_spi_transfer(ref, &transaction), NX_OK);
}
TEST_F(TypedSPI, FailedAdmissionReturnsInvalidTicketAndPreservesPreviousTerminalResult) {
    auto ref = child(); nx_device_spi_ticket_t previous{}, rejected{}, next{};
    ASSERT_EQ(nx_device_spi_submit(ref, &transaction, &previous), NX_OK);
    ASSERT_EQ(nx_device_spi_service(controller), NX_OK);
    model.submit_status = NX_ERR_FULL;
    EXPECT_EQ(nx_device_spi_submit(ref, &transaction, &rejected), NX_ERR_FULL);
    EXPECT_EQ(rejected.sequence, 0u);
    nx_device_spi_result_t result{};
    ASSERT_EQ(nx_device_spi_poll(ref, previous, &result), NX_OK);
    EXPECT_TRUE(result.settled); EXPECT_EQ(result.status, NX_OK);
    model.submit_status = NX_OK;
    ASSERT_EQ(nx_device_spi_submit(ref, &transaction, &next), NX_OK);
    EXPECT_GT(next.sequence, previous.sequence);
}
TEST_F(TypedSPI, ForgedChildAndClassMismatchAreRejectedBeforeProviderAccess) {
    auto ref = child(); ref.slot = UINT32_MAX;
    EXPECT_EQ(nx_device_spi_transfer(ref, &transaction), NX_ERR_INVALID_STATE);
    EXPECT_EQ(nx_device_spi_close(ref), NX_ERR_INVALID_STATE);
    ref.controller.device_class = NX_DEVICE_CLASS_UART;
    EXPECT_EQ(nx_device_spi_transfer(ref, &transaction), NX_ERR_TYPE_MISMATCH);
    EXPECT_EQ(model.transfers, 0u);
}
TEST_F(TypedSPI, InterruptContextRejectsChildActionsWithoutChangingOwnership) {
    auto ref = child(); nx_device_spi_ticket_t ticket{};
    ASSERT_EQ(nx_device_spi_submit(ref, &transaction, &ticket), NX_OK);
    typed_test_set_isr(true);
    EXPECT_EQ(nx_device_spi_close(ref), NX_ERR_CONTEXT);
    EXPECT_EQ(nx_device_spi_transfer(ref, &transaction), NX_ERR_CONTEXT);
    EXPECT_EQ(nx_device_spi_cancel(ref, ticket), NX_ERR_CONTEXT);
    EXPECT_EQ(nx_device_spi_cancel_transfer(ref), NX_ERR_CONTEXT);
    EXPECT_EQ(nx_device_spi_service(controller), NX_ERR_CONTEXT);
    nx_device_spi_result_t result{};
    EXPECT_EQ(nx_device_spi_poll(ref, ticket, &result), NX_ERR_CONTEXT);
    typed_test_set_isr(false);
    EXPECT_EQ(model.transfers, 0u); EXPECT_EQ(model.cancels, 0u);
    EXPECT_EQ(nx_device_spi_close(ref), NX_ERR_BUSY);
    EXPECT_EQ(nx_device_spi_service(controller), NX_OK);
}
} // namespace
