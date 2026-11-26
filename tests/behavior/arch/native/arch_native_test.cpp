/**
 * \file            arch_native_test.cpp
 *
 * \brief           Execute the real Native CPU exclusion and context contract.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-11
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include <atomic>
#include <condition_variable>
#include <gtest/gtest.h>
#include <initializer_list>
#include <mutex>
#include <thread>

namespace {
/** \brief Release both host workers before any metadata section begins. */
class StartGate {
  public:
    /** \brief Wait outside Arch exclusion until both workers are ready. */
    void arrive() {
        std::unique_lock<std::mutex> lock(mutex);
        ++ready;
        if (ready == 2U) {
            changed.notify_all();
        } else {
            changed.wait(lock, [this] { return ready == 2U; });
        }
    }

  private:
    std::mutex mutex;
    std::condition_variable changed;
    unsigned ready = 0U;
};

TEST(ArchNative, NestedRestorePreservesIncomingMask) {
    ASSERT_FALSE(nx_arch_irq_is_masked());
    const nx_arch_irq_masks_t unmasked = nx_arch_irq_masks();
    const nx_arch_irq_state_t outer = nx_arch_irq_save();
    const bool outer_masked = nx_arch_irq_is_masked();
    const nx_arch_irq_masks_t outer_masks = nx_arch_irq_masks();
    const nx_arch_irq_state_t inner = nx_arch_irq_save();
    const bool inner_masked = nx_arch_irq_is_masked();
    const nx_arch_irq_masks_t inner_masks = nx_arch_irq_masks();
    nx_arch_irq_restore(inner);
    const bool restored_outer_mask = nx_arch_irq_is_masked();
    const nx_arch_irq_masks_t restored_outer_masks = nx_arch_irq_masks();
    nx_arch_irq_restore(outer);
    const nx_arch_irq_masks_t restored_masks = nx_arch_irq_masks();
    EXPECT_TRUE(outer_masked);
    EXPECT_TRUE(inner_masked);
    EXPECT_TRUE(restored_outer_mask);
    EXPECT_FALSE(nx_arch_irq_is_masked());
    EXPECT_EQ(unmasked.primask, 0U);
    EXPECT_EQ(unmasked.basepri, 0U);
    EXPECT_EQ(unmasked.faultmask, 0U);
    for (const auto& masks : {outer_masks, inner_masks, restored_outer_masks}) {
        EXPECT_EQ(masks.primask, 1U);
        EXPECT_EQ(masks.basepri, 0U);
        EXPECT_EQ(masks.faultmask, 0U);
    }
    EXPECT_EQ(restored_masks.primask, 0U);
    EXPECT_EQ(restored_masks.basepri, 0U);
    EXPECT_EQ(restored_masks.faultmask, 0U);
}

TEST(ArchNative, OutOfOrderRestoreAborts) {
    EXPECT_DEATH(
        {
            const nx_arch_irq_state_t outer = nx_arch_irq_save();
            (void)nx_arch_irq_save();
            nx_arch_irq_restore(outer);
        },
        "");
    EXPECT_FALSE(nx_arch_irq_is_masked());
}

TEST(ArchNative, UnbalancedRestoreAborts) {
    EXPECT_DEATH({ nx_arch_irq_restore(nx_arch_irq_state_t{0U}); }, "");
    EXPECT_FALSE(nx_arch_irq_is_masked());
}

TEST(ArchNative, DuplicateRestoreAborts) {
    EXPECT_DEATH(
        {
            const nx_arch_irq_state_t saved = nx_arch_irq_save();
            nx_arch_irq_restore(saved);
            nx_arch_irq_restore(saved);
        },
        "");
    EXPECT_FALSE(nx_arch_irq_is_masked());
}

TEST(ArchNative, RestoreWithoutSaveOnCallingThreadAborts) {
    EXPECT_DEATH(
        {
            const nx_arch_irq_state_t saved = nx_arch_irq_save();
            std::thread other([saved] { nx_arch_irq_restore(saved); });
            other.join();
            nx_arch_irq_restore(saved);
        },
        "");
    EXPECT_FALSE(nx_arch_irq_is_masked());
}

TEST(ArchNative, ConcurrentMetadataSectionsExcludeOtherThreads) {
    constexpr unsigned rounds = 4000U;
    StartGate start;
    std::atomic<unsigned> active{0U};
    std::atomic<unsigned> violations{0U};
    std::atomic<unsigned> metadata{0U};
    bool contexts[2] = {true, true};
    auto work = [&](unsigned index) {
        contexts[index] = !nx_arch_irq_is_masked();
        start.arrive();
        for (unsigned i = 0U; i < rounds; ++i) {
            const nx_arch_irq_state_t outer = nx_arch_irq_save();
            const nx_arch_irq_state_t inner = nx_arch_irq_save();
            if (active.fetch_add(1U, std::memory_order_relaxed) != 0U) {
                violations.fetch_add(1U, std::memory_order_relaxed);
            }
            contexts[index] = contexts[index] && nx_arch_irq_is_masked();
            /* Atomics keep a broken exclusion implementation free of C++ data
             * races; load/store still require the real Arch section to retain
             * every metadata update. No wait or assertion runs while masked. */
            const unsigned previous = metadata.load(std::memory_order_relaxed);
            metadata.store(previous + 1U, std::memory_order_relaxed);
            if (active.fetch_sub(1U, std::memory_order_relaxed) != 1U) {
                violations.fetch_add(1U, std::memory_order_relaxed);
            }
            nx_arch_irq_restore(inner);
            contexts[index] = contexts[index] && nx_arch_irq_is_masked();
            nx_arch_irq_restore(outer);
            contexts[index] = contexts[index] && !nx_arch_irq_is_masked();
        }
    };
    std::thread first(work, 0U);
    std::thread second(work, 1U);
    first.join();
    second.join();
    EXPECT_EQ(violations.load(), 0U);
    EXPECT_EQ(active.load(), 0U);
    EXPECT_EQ(metadata.load(), rounds * 2U);
    EXPECT_TRUE(contexts[0]);
    EXPECT_TRUE(contexts[1]);
    EXPECT_FALSE(nx_arch_irq_is_masked());
}

TEST(ArchNative, UnavailableCycleSnapshotPreservesCallerStorage) {
    uint32_t cycles = UINT32_C(0xabcdef12);
    EXPECT_FALSE(nx_arch_cycle_snapshot(&cycles));
    EXPECT_EQ(cycles, UINT32_C(0xabcdef12));
    EXPECT_FALSE(nx_arch_cycle_snapshot(nullptr));
}

TEST(ArchNative, ContextQueriesDescribePrivilegedThreadMode) {
    EXPECT_EQ(nx_arch_exception_number(), 0U);
    EXPECT_TRUE(nx_arch_is_privileged());
    EXPECT_FALSE(nx_arch_in_isr());
    const nx_arch_irq_state_t saved = nx_arch_irq_save();
    const uint32_t exception = nx_arch_exception_number();
    const bool privileged = nx_arch_is_privileged();
    const bool in_isr = nx_arch_in_isr();
    nx_arch_irq_restore(saved);
    EXPECT_EQ(exception, 0U);
    EXPECT_TRUE(privileged);
    EXPECT_FALSE(in_isr);
}
} /* namespace */
