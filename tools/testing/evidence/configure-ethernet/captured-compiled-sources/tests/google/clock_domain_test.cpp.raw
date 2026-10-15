/**
 * \file            clock_domain_test.cpp
 * \brief           Real clock admission, retained identity and drain contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "clock_domain_impl.h"
#include "nexus/arch/arch.h"
#include <atomic>
#include <gtest/gtest.h>
#include <thread>
namespace {
bool privileged = true;
uint32_t exception_number;
} /* namespace */
extern "C" bool nx_clock_test_privileged(void) {
    return privileged;
}
extern "C" uint32_t nx_clock_test_exception(void) {
    return exception_number;
}
namespace {
class ClockDomain : public ::testing::Test {
  protected:
    nx_clock_domain_t domain = {};
    nx_clock_lease_t first = {}, second = {};
    nx_clock_barrier_t barrier = {};
    void SetUp() override {
        privileged = true;
        exception_number = 0;
        ASSERT_EQ(nx_clock_domain_construct(&domain, 2), NX_SUCCESS);
    }
};
TEST_F(ClockDomain, CloseRetainsConsumerUntilExactRelease) {
    ASSERT_EQ(nx_clock_domain_acquire(&domain, &first), NX_SUCCESS);
    ASSERT_EQ(nx_clock_domain_close(&domain, &barrier), NX_SUCCESS);
    EXPECT_EQ(nx_clock_domain_quiescent(barrier), NX_ERROR_BUSY);
    EXPECT_EQ(nx_clock_domain_acquire(&domain, &second), NX_ERROR_BUSY);
    EXPECT_EQ(second.owner, nullptr);
    ASSERT_EQ(nx_clock_domain_release(&domain, &first), NX_SUCCESS);
    EXPECT_EQ(first.owner, nullptr);
    EXPECT_EQ(nx_clock_domain_quiescent(barrier), NX_SUCCESS);
}
TEST_F(ClockDomain, CopiedTokenCannotRemoveOriginalBorrow) {
    ASSERT_EQ(nx_clock_domain_acquire(&domain, &first), NX_SUCCESS);
    auto copy = first;
    EXPECT_EQ(nx_clock_domain_release(&domain, &copy), NX_ERROR_STATE);
    EXPECT_EQ(domain.count, 1U);
    EXPECT_EQ(domain.head, &first);
    EXPECT_EQ(nx_clock_domain_release(&domain, &first), NX_SUCCESS);
}
TEST_F(ClockDomain, ReopenInvalidatesOldControlWithoutEpochReuse) {
    ASSERT_EQ(nx_clock_domain_close(&domain, &barrier), NX_SUCCESS);
    ASSERT_EQ(nx_clock_domain_reopen(barrier), NX_SUCCESS);
    EXPECT_EQ(nx_clock_domain_quiescent(barrier), NX_ERROR_STATE);
    EXPECT_EQ(nx_clock_domain_reopen(barrier), NX_ERROR_STATE);
    nx_clock_barrier_t next = {};
    ASSERT_EQ(nx_clock_domain_close(&domain, &next), NX_SUCCESS);
    EXPECT_GT(next.epoch, barrier.epoch);
}
TEST_F(ClockDomain, ExhaustedEpochCannotReopen) {
    domain.epoch = UINT64_MAX;
    ASSERT_EQ(nx_clock_domain_close(&domain, &barrier), NX_SUCCESS);
    EXPECT_EQ(nx_clock_domain_reopen(barrier), NX_ERROR_EXHAUSTED);
    EXPECT_TRUE(domain.closing);
    EXPECT_EQ(domain.epoch, UINT64_MAX);
}
TEST_F(ClockDomain, ExactCapacityAndDuplicateAdmissionReject) {
    nx_clock_lease_t third = {};
    ASSERT_EQ(nx_clock_domain_acquire(&domain, &first), NX_SUCCESS);
    EXPECT_EQ(nx_clock_domain_acquire(&domain, &first), NX_ERROR_STATE);
    ASSERT_EQ(nx_clock_domain_acquire(&domain, &second), NX_SUCCESS);
    EXPECT_EQ(nx_clock_domain_acquire(&domain, &third), NX_ERROR_EXHAUSTED);
    EXPECT_EQ(third.owner, nullptr);
    EXPECT_EQ(nx_clock_domain_release(&domain, &second), NX_SUCCESS);
    EXPECT_EQ(nx_clock_domain_release(&domain, &first), NX_SUCCESS);
}
TEST_F(ClockDomain, ForeignDomainAndDoubleReleaseKeepOwnership) {
    nx_clock_domain_t foreign = {};
    ASSERT_EQ(nx_clock_domain_construct(&foreign, 1), NX_SUCCESS);
    ASSERT_EQ(nx_clock_domain_acquire(&domain, &first), NX_SUCCESS);
    EXPECT_EQ(nx_clock_domain_release(&foreign, &first), NX_ERROR_STATE);
    EXPECT_EQ(nx_clock_domain_acquire(&foreign, &first), NX_ERROR_STATE);
    ASSERT_EQ(nx_clock_domain_release(&domain, &first), NX_SUCCESS);
    EXPECT_EQ(nx_clock_domain_release(&domain, &first), NX_ERROR_STATE);
}
TEST_F(ClockDomain, CloseOutputCannotOverwriteDomainOrLease) {
    ASSERT_EQ(nx_clock_domain_acquire(&domain, &first), NX_SUCCESS);
    EXPECT_EQ(nx_clock_domain_close(
                  &domain, reinterpret_cast<nx_clock_barrier_t*>(&domain)),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_clock_domain_close(
                  &domain, reinterpret_cast<nx_clock_barrier_t*>(&first)),
              NX_ERROR_INVALID);
    EXPECT_FALSE(domain.closing);
    EXPECT_EQ(first.owner, &domain);
    ASSERT_EQ(nx_clock_domain_release(&domain, &first), NX_SUCCESS);
}
TEST_F(ClockDomain, UnalignedAndWrappingTokenRejectBeforeAccess) {
    alignas(nx_clock_lease_t) unsigned char bytes[64] = {};
    EXPECT_EQ(nx_clock_domain_acquire(
                  &domain, reinterpret_cast<nx_clock_lease_t*>(bytes + 1)),
              NX_ERROR_INVALID);
    auto* wrapping = reinterpret_cast<nx_clock_lease_t*>(
        UINTPTR_MAX - (alignof(nx_clock_lease_t) - 1U));
    EXPECT_EQ(nx_clock_domain_acquire(&domain, wrapping), NX_ERROR_INVALID);
    EXPECT_EQ(domain.count, 0U);
}
TEST_F(ClockDomain, LiveConstructAndZeroCapacityRejectUnchanged) {
    EXPECT_EQ(nx_clock_domain_construct(&domain, 9), NX_ERROR_STATE);
    EXPECT_EQ(domain.capacity, 2U);
    nx_clock_domain_t unused = {};
    EXPECT_EQ(nx_clock_domain_construct(&unused, 0), NX_ERROR_INVALID);
    EXPECT_FALSE(unused.initialized);
}
TEST_F(ClockDomain, CloseIsIdempotentButOpenControlIsNotQuiescent) {
    EXPECT_EQ(nx_clock_domain_quiescent({&domain, domain.epoch}),
              NX_ERROR_STATE);
    ASSERT_EQ(nx_clock_domain_close(&domain, &barrier), NX_SUCCESS);
    nx_clock_barrier_t copy = {};
    EXPECT_EQ(nx_clock_domain_close(&domain, &copy), NX_SUCCESS);
    EXPECT_EQ(copy.epoch, barrier.epoch);
    EXPECT_EQ(nx_clock_domain_quiescent(copy), NX_SUCCESS);
}
TEST_F(ClockDomain, ReopenWithConsumerIsBusyAndRetainsClosedState) {
    ASSERT_EQ(nx_clock_domain_acquire(&domain, &first), NX_SUCCESS);
    ASSERT_EQ(nx_clock_domain_close(&domain, &barrier), NX_SUCCESS);
    EXPECT_EQ(nx_clock_domain_reopen(barrier), NX_ERROR_BUSY);
    EXPECT_TRUE(domain.closing);
    EXPECT_EQ(domain.count, 1U);
    EXPECT_EQ(nx_clock_domain_release(&domain, &first), NX_SUCCESS);
}
TEST_F(ClockDomain, GuardNestingRestoresIncomingMask) {
    auto saved = nx_arch_irq_save();
    ASSERT_TRUE(nx_arch_irq_is_masked());
    EXPECT_EQ(nx_clock_domain_acquire(&domain, &first), NX_SUCCESS);
    EXPECT_TRUE(nx_arch_irq_is_masked());
    EXPECT_EQ(nx_clock_domain_release(&domain, &first), NX_SUCCESS);
    EXPECT_TRUE(nx_arch_irq_is_masked());
    nx_arch_irq_restore(saved);
    EXPECT_FALSE(nx_arch_irq_is_masked());
}
TEST_F(ClockDomain, ConcurrentConsumersCannotCrossClosedAdmission) {
    constexpr unsigned workers = 8;
    std::atomic<unsigned> ready{0};
    std::atomic<bool> finish{false};
    std::atomic<unsigned> accepted{0};
    std::thread tasks[workers];
    for (auto& task : tasks) {
        task = std::thread([&]() {
            nx_clock_lease_t token = {};
            auto result = nx_clock_domain_acquire(&domain, &token);
            if (result == NX_SUCCESS) {
                ++accepted;
            }
            ++ready;
            while (!finish.load()) {
                std::this_thread::yield();
            }
            if (result == NX_SUCCESS) {
                EXPECT_EQ(nx_clock_domain_release(&domain, &token), NX_SUCCESS);
            }
        });
    }
    while (ready.load() != workers) {
        std::this_thread::yield();
    }
    ASSERT_EQ(accepted.load(), 2U);
    ASSERT_EQ(nx_clock_domain_close(&domain, &barrier), NX_SUCCESS);
    EXPECT_EQ(nx_clock_domain_quiescent(barrier), NX_ERROR_BUSY);
    EXPECT_EQ(nx_clock_domain_acquire(&domain, &first), NX_ERROR_BUSY);
    finish.store(true);
    for (auto& task : tasks) {
        task.join();
    }
    EXPECT_EQ(nx_clock_domain_quiescent(barrier), NX_SUCCESS);
}
TEST_F(ClockDomain, DuplicateNonHeadTokenIsAStateRejection) {
    ASSERT_EQ(nx_clock_domain_acquire(&domain, &first), NX_SUCCESS);
    ASSERT_EQ(nx_clock_domain_acquire(&domain, &second), NX_SUCCESS);
    EXPECT_EQ(nx_clock_domain_acquire(&domain, &first), NX_ERROR_STATE);
    EXPECT_EQ(domain.count, 2U);
    EXPECT_EQ(nx_clock_domain_release(&domain, &first), NX_SUCCESS);
    EXPECT_EQ(nx_clock_domain_release(&domain, &second), NX_SUCCESS);
}
TEST_F(ClockDomain, UnprivilegedOrFaultCannotAcquireOrClose) {
    privileged = false;
    EXPECT_EQ(nx_clock_domain_acquire(&domain, &first), NX_ERROR_CONTEXT);
    EXPECT_EQ(nx_clock_domain_close(&domain, &barrier), NX_ERROR_CONTEXT);
    privileged = true;
    for (uint32_t exception : {2U, 3U, 4U, 11U}) {
        exception_number = exception;
        EXPECT_EQ(nx_clock_domain_acquire(&domain, &first), NX_ERROR_CONTEXT);
    }
    exception_number = 0;
    EXPECT_EQ(domain.count, 0U);
    EXPECT_FALSE(domain.closing);
}
TEST_F(ClockDomain, ConfigurableIrqAcquiresButCannotCloseLifecycle) {
    exception_number = 16;
    EXPECT_EQ(nx_clock_domain_acquire(&domain, &first), NX_SUCCESS);
    EXPECT_EQ(nx_clock_domain_close(&domain, &barrier), NX_ERROR_CONTEXT);
    EXPECT_EQ(nx_clock_domain_release(&domain, &first), NX_SUCCESS);
    exception_number = 0;
    EXPECT_FALSE(domain.closing);
}
TEST_F(ClockDomain, StaleControlAndNullInputsRejectWithoutWrites) {
    EXPECT_EQ(nx_clock_domain_construct(nullptr, 1), NX_ERROR_INVALID);
    EXPECT_EQ(nx_clock_domain_acquire(nullptr, &first), NX_ERROR_INVALID);
    EXPECT_EQ(nx_clock_domain_acquire(&domain, nullptr), NX_ERROR_INVALID);
    EXPECT_EQ(nx_clock_domain_release(&domain, nullptr), NX_ERROR_INVALID);
    EXPECT_EQ(nx_clock_domain_close(&domain, nullptr), NX_ERROR_INVALID);
    EXPECT_EQ(nx_clock_domain_quiescent({nullptr, 0}), NX_ERROR_INVALID);
    EXPECT_EQ(nx_clock_domain_reopen({nullptr, 0}), NX_ERROR_INVALID);
    ASSERT_EQ(nx_clock_domain_close(&domain, &barrier), NX_SUCCESS);
    EXPECT_EQ(nx_clock_domain_quiescent({&domain, barrier.epoch + 1U}),
              NX_ERROR_STATE);
    EXPECT_EQ(nx_clock_domain_reopen({&domain, barrier.epoch + 1U}),
              NX_ERROR_STATE);
}
} /* namespace */
