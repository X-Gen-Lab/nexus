/**
 * \file            atomic_native_test.cpp
 *
 * \brief           Execute real atomic RMW and publication across host tasks.
 *
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/atomic.h"
#include "nexus/components/log.h"
#include <condition_variable>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>

namespace {
TEST(AtomicNative, CompareExchangeFailureUpdatesExpectedWithoutChangingValue) {
    uint32_t value = 9U;
    uint32_t expected = 7U;
    EXPECT_FALSE(nx_atomic_u32_compare_exchange_acq_rel(&value, &expected, 8U));
    EXPECT_EQ(value, 9U);
    EXPECT_EQ(expected, 9U);
    EXPECT_TRUE(nx_atomic_u32_compare_exchange_acq_rel(&value, &expected, 8U));
    EXPECT_EQ(value, 8U);
    EXPECT_EQ(expected, 9U);
}

TEST(AtomicNative, CounterWrapHasDefinedUnsignedSemantics) {
    uint32_t value = UINT32_MAX;
    EXPECT_EQ(nx_atomic_u32_fetch_add_release(&value, 1U), UINT32_MAX);
    EXPECT_EQ(nx_atomic_u32_fetch_sub_release(&value, 1U), 0U);
    EXPECT_EQ(value, UINT32_MAX);
}

TEST(AtomicNative, ConcurrentRmwPreservesEveryUpdateWithoutArchExclusion) {
    constexpr unsigned rounds = 5000U;
    uint32_t value = 0U;
    auto increment = [&] {
        for (unsigned i = 0U; i < rounds; ++i) {
            (void)nx_atomic_u32_fetch_add_acq_rel(&value, 1U);
        }
    };
    std::thread first(increment);
    std::thread second(increment);
    first.join();
    second.join();
    EXPECT_EQ(nx_atomic_u32_load_acquire(&value), rounds * 2U);
    EXPECT_EQ(nx_atomic_u32_fetch_sub_acq_rel(&value, rounds), rounds * 2U);
    EXPECT_EQ(nx_atomic_u32_load_relaxed(&value), rounds);
}

TEST(AtomicNative, ReleaseAcquirePublishesPayloadAndProtectsItsReuse) {
    constexpr uint32_t rounds = 2000U;
    uint32_t ready = 0U;
    uint32_t consumed = 0U;
    uint32_t payload = 0U;
    bool valid = true;
    std::thread producer([&] {
        for (uint32_t i = 1U; i <= rounds; ++i) {
            while (nx_atomic_u32_load_acquire(&consumed) != i - 1U) {
                std::this_thread::yield();
            }
            payload = i ^ UINT32_C(0xabcdef12);
            nx_atomic_u32_store_release(&ready, i);
        }
    });
    std::thread consumer([&] {
        for (uint32_t i = 1U; i <= rounds; ++i) {
            while (nx_atomic_u32_load_acquire(&ready) != i) {
                std::this_thread::yield();
            }
            valid = valid && payload == (i ^ UINT32_C(0xabcdef12));
            nx_atomic_u32_store_release(&consumed, i);
        }
    });
    producer.join();
    consumer.join();
    EXPECT_TRUE(valid);
    EXPECT_EQ(nx_atomic_u32_load_acquire(&consumed), rounds);
}

TEST(AtomicNative, LoggerRejectsConcurrentWriterAndStopRetainsActiveSink) {
    std::mutex mutex;
    std::condition_variable changed;
    bool entered = false;
    bool release = false;
    struct Sink {
        std::mutex* mutex;
        std::condition_variable* changed;
        bool* entered;
        bool* release;
    } sink = {&mutex, &changed, &entered, &release};
    nx_log_sink_port_t port{};
    port.context = &sink;
    port.write = [](void* context, nx_log_level_t, const void*, size_t,
                    nx_time_us_t) {
        auto* state = static_cast<Sink*>(context);
        std::unique_lock<std::mutex> lock(*state->mutex);
        *state->entered = true;
        state->changed->notify_all();
        state->changed->wait(lock, [state] { return *state->release; });
        return NX_SUCCESS;
    };
    nx_log_t logger{};
    ASSERT_EQ(nx_log_init(&logger, port, NX_LOG_INFO), NX_SUCCESS);
    nx_result_t first_result = NX_ERROR_IO;
    std::thread first([&] {
        first_result = nx_log_write(&logger, NX_LOG_INFO, "first", 5U, 100U);
    });
    {
        std::unique_lock<std::mutex> lock(mutex);
        changed.wait(lock, [&] { return entered; });
    }
    const nx_result_t rejected =
        nx_log_write(&logger, NX_LOG_INFO, "second", 6U, 100U);
    const nx_result_t draining = nx_log_stop(&logger);
    {
        std::lock_guard<std::mutex> lock(mutex);
        release = true;
        changed.notify_all();
    }
    first.join();
    EXPECT_EQ(rejected, NX_ERROR_BUSY);
    EXPECT_EQ(draining, NX_ERROR_BUSY);
    EXPECT_EQ(first_result, NX_SUCCESS);
    EXPECT_EQ(nx_log_stop(&logger), NX_SUCCESS);
    EXPECT_EQ(nx_log_write(&logger, NX_LOG_INFO, "third", 5U, 100U),
              NX_ERROR_STATE);
}
} /* namespace */
