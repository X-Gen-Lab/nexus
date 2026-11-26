/**
 * \file            os_diagnostic_native_test.cpp
 * \brief           Real Native exclusion with concurrent diagnostic producers
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/sleep.h"
#include "nexus/os/diagnostic.h"
#include <atomic>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

TEST(DiagnosticNative, TwoProducersAndReaderShareExplicitRing) {
    std::vector<nx_diagnostic_event_t> storage(512);
    nx_diagnostic_ring_t ring = {};
    ASSERT_EQ(nx_diagnostic_ring_init(&ring, storage.data(), storage.size()),
              NX_SUCCESS);
    std::atomic<bool> start{false};
    std::atomic<unsigned> errors{0};
    std::vector<unsigned> observed(400, 0);
    auto ready = [&start]() {
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
    };
    auto producer = [&](unsigned offset) {
        ready();
        for (unsigned value = 0; value < 200; ++value) {
            nx_diagnostic_event_t event = {value, offset, value + offset, 1};
            if (nx_diagnostic_ring_write(&ring, &event) != NX_SUCCESS) {
                errors.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };
    std::thread first(producer, 0);
    std::thread second(producer, 200);
    std::thread reader([&]() {
        ready();
        unsigned count = 0;
        while (count < 400) {
            nx_diagnostic_event_t event = {};
            nx_result_t result = nx_diagnostic_ring_read(&ring, &event);
            if (result == NX_ERROR_BUSY) {
                std::this_thread::yield();
                continue;
            }
            if (result != NX_SUCCESS || event.value >= observed.size() ||
                event.timestamp + event.identity != event.value) {
                errors.fetch_add(1, std::memory_order_relaxed);
            } else {
                ++observed[event.value];
            }
            ++count;
        }
    });
    start.store(true, std::memory_order_release);
    first.join();
    second.join();
    reader.join();
    EXPECT_EQ(errors.load(), 0U);
    for (unsigned count : observed) {
        EXPECT_EQ(count, 1U);
    }
    uint32_t dropped = 1;
    ASSERT_EQ(nx_diagnostic_ring_dropped(&ring, &dropped), NX_SUCCESS);
    EXPECT_EQ(dropped, 0U);
}

TEST(ArchSleepNative, RejectsHardwareSleepWithoutInventingHostTiming) {
    uint32_t sequence = 0;
    bool slept = true;
    EXPECT_EQ(nx_arch_wait_for_interrupt(), NX_ARCH_UNSUPPORTED);
    EXPECT_EQ(nx_arch_idle_if_unchanged(&sequence, 0, &slept),
              NX_ARCH_UNSUPPORTED);
    EXPECT_FALSE(slept);
}
