/**
 * \file            spi_dma_owner_wait_test.cpp
 * \brief           Real task publishers, latched waits and joined SPI owners
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/components/owner_observer.h"
#include "nexus/components/spi_dma_owner.h"
#include "nexus/io/native/model.h"
#include "nexus/io/native/spi_dma_model.h"
#include "nexus/os/native.h"
#include "spi_dma_provider.h"
#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>

namespace {
class SPIDMAOwnerWait : public testing::Test {
  protected:
    struct Producer {
        nx_request_t request{};
        uint16_t tx[2]{};
        uint16_t rx[2]{};
        nx_spi_dma_segment_t segments[2]{};
        nx_spi_dma_owner_operation_t operation{};
        nx_owner_ticket_t ticket{};
        nx_wait_port_t completion{};
        nx_result_t admitted = NX_ERROR_STATE;
        nx_result_t waited = NX_ERROR_STATE;
        std::atomic<bool> submitted{false};
    } producer[2];
    nx_native_spi_dma_state_t hardware{};
    nx_spi_dma_sequence_state_t sequence{};
    nx_native_spi_dma_endpoint_state_t devices[2]{{&hardware, false, 0U, 0U},
                                                  {&hardware, false, 0U, 0U}};
    nx_spi_dma_sequence_endpoint_t children[2]{{&sequence, &devices[0]},
                                               {&sequence, &devices[1]}};
    const nx_spi_dma_transaction_endpoint_t endpoints[2]{
        {&nx_spi_dma_sequence_endpoint_ops, &children[0]},
        {&nx_spi_dma_sequence_endpoint_ops, &children[1]}};
    const nx_spi_dma_transaction_port_t port{&nx_spi_dma_sequence_port_ops,
                                             &sequence};
    const nx_spi_dma_owner_binding_t binding{
        &port, nx_native_spi_dma_owner_contains,
        nx_native_spi_dma_owner_attach_wake};
    nx_spi_dma_owner_executor_t execution{};
    nx_bus_owner_t owner{};
    nx_owner_slot_t slots[2]{};
    nx_owner_observer_t observer{};
    nx_owner_observation_t observed{};
    nx_native_notify_t owner_event{};
    nx_native_notify_t completion_events[2]{};
    unsigned events_ready = 0U;
    nx_wait_port_t owner_wake{};
    const nx_irq_wake_t hint{&owner_wake,
                             [](void* p) {
                                 auto* event = static_cast<nx_wait_port_t*>(p);
                                 return event->wake(event->context);
                             },
                             true};
    std::thread producer_threads[2];
    std::thread publisher;
    std::thread executor_thread;
    std::atomic<bool> publisher_stop{false};
    std::atomic<bool> overlapping_cs{false};
    std::atomic<bool> callback_entered{false};
    std::atomic<bool> observer_exited{false};
    std::mutex callback_mutex;
    std::condition_variable callback_condition;
    bool callback_release = false;
    bool closed = false;
    static nx_result_t BlockCompletion(void* context) {
        auto* self = static_cast<SPIDMAOwnerWait*>(context);
        self->callback_entered.store(true, std::memory_order_release);
        std::unique_lock<std::mutex> held(self->callback_mutex);
        self->callback_condition.wait(held,
                                      [&] { return self->callback_release; });
        return NX_SUCCESS;
    }
    static bool Settled(void* context) {
        auto* item = static_cast<Producer*>(context);
        return nx_request_state(&item->request) == NX_REQUEST_SETTLED;
    }
    void SetUp() override {
        /* Both IO deadlines and POSIX waits use real CLOCK_MONOTONIC here. */
        ASSERT_EQ(nx_native_clock_configure(false, 0U), NX_SUCCESS);
        ASSERT_EQ(nx_native_spi_dma_initialize(&hardware), NX_SUCCESS);
        ASSERT_EQ(
            nx_spi_dma_sequence_initialize(
                &sequence, &nx_native_spi_dma_sequence_backend, &hardware),
            NX_SUCCESS);
        ASSERT_EQ(nx_native_spi_dma_model_automatic(&port, false), NX_SUCCESS);
        ASSERT_EQ(nx_native_notify_init(&owner_event), NX_SUCCESS);
        ++events_ready;
        owner_wake = nx_native_notify_port(&owner_event);
        for (unsigned i = 0U; i < 2U; ++i) {
            ASSERT_EQ(nx_native_notify_init(&completion_events[i]), NX_SUCCESS);
            ++events_ready;
            producer[i].completion =
                nx_native_notify_port(&completion_events[i]);
            producer[i].tx[0] = static_cast<uint16_t>(0x1234U + i);
            producer[i].tx[1] = static_cast<uint16_t>(0xABCDU + i);
            for (unsigned j = 0U; j < 2U; ++j) {
                producer[i].segments[j] = {producer[i].tx + j,
                                           producer[i].rx + j, 1U, 0U};
            }
            producer[i].operation = {&endpoints[i], producer[i].segments, 2U,
                                     16U};
            nx_request_initialize(&producer[i].request);
            ASSERT_EQ(nx_request_prepare(
                          &producer[i].request,
                          nx_deadline_after(nx_native_now_us(), 2000000U)),
                      NX_SUCCESS);
        }
        const auto executor =
            nx_spi_dma_owner_executor_port(&execution, &binding);
        const nx_owner_guard_port_t guard{
            nullptr,
            [](void*) -> uintptr_t { return nx_arch_irq_save().value; },
            [](void*, uintptr_t token) {
                nx_arch_irq_restore({static_cast<uint32_t>(token)});
            }};
        const nx_clock_t clock{[](void*) { return nx_native_now_us(); },
                               nullptr};
        ASSERT_EQ(nx_bus_owner_init(&owner, slots, 2U, guard, executor, clock,
                                    &owner_wake),
                  NX_SUCCESS);
        ASSERT_EQ(nx_owner_observer_bind(&observer, &owner), NX_SUCCESS);
    }
    void StartPublisher() {
        publisher = std::thread([&] {
            while (!publisher_stop.load(std::memory_order_acquire)) {
                nx_native_spi_dma_model_step(&port);
                auto saved = nx_arch_irq_save();
                if (devices[0].cs && devices[1].cs) {
                    overlapping_cs.store(true, std::memory_order_release);
                }
                nx_arch_irq_restore(saved);
                std::this_thread::yield();
            }
        });
    }
    void StartProducer(unsigned i) {
        producer_threads[i] = std::thread([&, i] {
            auto& item = producer[i];
            const nx_time_us_t deadline = item.request.deadline;
            item.admitted =
                nx_spi_dma_owner_submit(&owner, &item.request, &item.operation,
                                        &item.completion, &item.ticket);
            item.submitted.store(true, std::memory_order_release);
            if (item.admitted == NX_SUCCESS) {
                item.waited =
                    nx_wait_until(&item.completion, Settled, &item, deadline);
            }
        });
    }
    void Close() {
        if (closed) {
            return;
        }
        if (owner.capacity != 0U) {
            nx_bus_owner_stop(&owner);
        }
        {
            std::lock_guard<std::mutex> held(callback_mutex);
            callback_release = true;
        }
        callback_condition.notify_all();
        if (executor_thread.joinable()) {
            executor_thread.join();
        }
        if (owner.capacity != 0U) {
            (void)nx_native_spi_dma_model_hold(&port, false);
            for (unsigned i = 0U; i < 128U && !nx_bus_owner_idle(&owner); ++i) {
                nx_native_spi_dma_model_step(&port);
                (void)nx_bus_owner_service(&owner);
            }
            if (!nx_bus_owner_idle(&owner)) {
                std::abort();
            }
        }
        if (sequence.backend != nullptr &&
            nx_spi_dma_transaction_stop(&port) != NX_SUCCESS) {
            std::abort();
        }
        publisher_stop.store(true, std::memory_order_release);
        if (publisher.joinable()) {
            publisher.join();
        }
        for (auto& task : producer_threads) {
            if (task.joinable()) {
                task.join();
            }
        }
        nx_owner_observer_detach(&observer);
        closed = true;
    }
    void TearDown() override {
        Close();
        for (unsigned i = 1U; i < events_ready; ++i) {
            EXPECT_EQ(nx_native_notify_destroy(&completion_events[i - 1U]),
                      NX_SUCCESS);
        }
        if (events_ready != 0U) {
            EXPECT_EQ(nx_native_notify_destroy(&owner_event), NX_SUCCESS);
        }
        EXPECT_EQ(nx_native_clock_configure(false, 0U), NX_SUCCESS);
    }
};
TEST_F(SPIDMAOwnerWait, RealPublishersWaitersAndObserverJoinAfterWholeChain) {
    ASSERT_EQ(nx_spi_dma_owner_attach_wake(&binding, &hint, 5U), NX_SUCCESS);
    StartPublisher();
    StartProducer(0U);
    StartProducer(1U);
    const nx_time_us_t limit = nx_deadline_after(nx_native_now_us(), 2000000U);
    while ((!Settled(&producer[0]) || !Settled(&producer[1])) &&
           !nx_deadline_expired(limit, nx_native_now_us())) {
        const uint32_t snapshot = owner_wake.arm(owner_wake.context);
        (void)nx_owner_observer_service(&observer, nx_native_now_us(),
                                        &observed);
        if (!Settled(&producer[0]) || !Settled(&producer[1])) {
            nx_time_us_t fallback =
                nx_deadline_after(nx_native_now_us(), 1000U);
            if (fallback > limit) {
                fallback = limit;
            }
            (void)owner_wake.wait(owner_wake.context, snapshot, fallback);
        }
    }
    EXPECT_TRUE(Settled(&producer[0]));
    EXPECT_TRUE(Settled(&producer[1]));
    Close();
    EXPECT_FALSE(overlapping_cs.load(std::memory_order_acquire));
    EXPECT_GT(observed.calls, 0U);
    EXPECT_EQ(observer.owner, nullptr);
    for (unsigned i = 0U; i < 2U; ++i) {
        EXPECT_EQ(producer[i].admitted, NX_SUCCESS);
        EXPECT_EQ(producer[i].waited, NX_SUCCESS);
        EXPECT_EQ(producer[i].request.result, NX_SUCCESS);
        EXPECT_EQ(producer[i].request.transferred, 2U);
        EXPECT_EQ(producer[i].rx[0], producer[i].tx[0]);
        EXPECT_EQ(producer[i].rx[1], producer[i].tx[1]);
        EXPECT_EQ(devices[i].assertions, 1U);
        EXPECT_EQ(devices[i].releases, 1U);
        EXPECT_FALSE(producer_threads[i].joinable());
    }
    EXPECT_FALSE(publisher.joinable());
    EXPECT_FALSE(hardware.opened);
    EXPECT_EQ(hardware.wake, nullptr);
}
TEST_F(SPIDMAOwnerWait, ConcurrentCancelRetainsLoansUntilPublisherDrain) {
    ASSERT_EQ(nx_spi_dma_owner_attach_wake(&binding, &hint, 5U), NX_SUCCESS);
    ASSERT_EQ(nx_native_spi_dma_model_hold(&port, true), NX_SUCCESS);
    StartPublisher();
    StartProducer(0U);
    const nx_time_us_t limit = nx_deadline_after(nx_native_now_us(), 2000000U);
    while ((!producer[0].submitted.load(std::memory_order_acquire) ||
            !execution.pending) &&
           !nx_deadline_expired(limit, nx_native_now_us())) {
        (void)nx_owner_observer_service(&observer, nx_native_now_us(),
                                        &observed);
        std::this_thread::yield();
    }
    if (!producer[0].submitted.load(std::memory_order_acquire) ||
        producer[0].admitted != NX_SUCCESS || !execution.pending) {
        ADD_FAILURE() << "Producer did not establish a live retained chain";
        Close();
        return;
    }
    EXPECT_EQ(nx_bus_owner_cancel(&owner, producer[0].ticket), NX_SUCCESS);
    (void)nx_owner_observer_service(&observer, nx_native_now_us(), &observed);
    EXPECT_NE(nx_request_state(&producer[0].request), NX_REQUEST_SETTLED);
    EXPECT_FALSE(nx_bus_owner_idle(&owner));
    EXPECT_EQ(execution.inner.segments, producer[0].segments);
    EXPECT_TRUE(devices[0].cs);
    EXPECT_EQ(devices[0].releases, 0U);
    EXPECT_EQ(nx_native_spi_dma_model_hold(&port, false), NX_SUCCESS);
    while (!Settled(&producer[0]) &&
           !nx_deadline_expired(limit, nx_native_now_us())) {
        const uint32_t snapshot = owner_wake.arm(owner_wake.context);
        (void)nx_owner_observer_service(&observer, nx_native_now_us(),
                                        &observed);
        if (!Settled(&producer[0])) {
            (void)owner_wake.wait(owner_wake.context, snapshot,
                                  nx_deadline_after(nx_native_now_us(), 1000U));
        }
    }
    EXPECT_TRUE(Settled(&producer[0]));
    Close();
    EXPECT_EQ(producer[0].waited, NX_SUCCESS);
    EXPECT_EQ(producer[0].request.result, NX_ERROR_CANCELLED);
    EXPECT_LE(producer[0].request.transferred, 1U);
    EXPECT_EQ(execution.inner.segments, nullptr);
    EXPECT_EQ(devices[0].assertions, 1U);
    EXPECT_EQ(devices[0].releases, 1U);
    EXPECT_FALSE(devices[0].cs);
    EXPECT_FALSE(publisher.joinable());
    EXPECT_FALSE(producer_threads[0].joinable());
}
TEST_F(SPIDMAOwnerWait, SettledDoesNotJoinCompletionPublisherOrObserverCall) {
    ASSERT_EQ(nx_native_spi_dma_model_automatic(&port, true), NX_SUCCESS);
    const nx_wait_port_t blocked{this, nullptr, nullptr, BlockCompletion};
    ASSERT_EQ(nx_spi_dma_owner_submit(&owner, &producer[0].request,
                                      &producer[0].operation, &blocked,
                                      &producer[0].ticket),
              NX_SUCCESS);
    executor_thread = std::thread([&] {
        for (unsigned i = 0U; i < 128U && !Settled(&producer[0]); ++i) {
            (void)nx_owner_observer_service(&observer, nx_native_now_us(),
                                            &observed);
        }
        observer_exited.store(true, std::memory_order_release);
    });
    const nx_time_us_t limit = nx_deadline_after(nx_native_now_us(), 2000000U);
    while (!callback_entered.load(std::memory_order_acquire) &&
           !nx_deadline_expired(limit, nx_native_now_us())) {
        std::this_thread::yield();
    }
    EXPECT_TRUE(callback_entered.load(std::memory_order_acquire));
    EXPECT_TRUE(Settled(&producer[0]));
    EXPECT_FALSE(nx_bus_owner_idle(&owner));
    EXPECT_FALSE(observer_exited.load(std::memory_order_acquire));
    {
        std::lock_guard<std::mutex> held(callback_mutex);
        callback_release = true;
    }
    callback_condition.notify_all();
    executor_thread.join();
    EXPECT_TRUE(observer_exited.load(std::memory_order_acquire));
    EXPECT_TRUE(nx_bus_owner_idle(&owner));
    EXPECT_EQ(producer[0].request.result, NX_SUCCESS);
    EXPECT_EQ(producer[0].request.transferred, 2U);
    Close();
    EXPECT_EQ(observer.owner, nullptr);
    EXPECT_FALSE(hardware.opened);
}
} /* namespace */
