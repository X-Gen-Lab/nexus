/**
 * \file            atomic_fallback_test.cpp
 *
 * \brief           Real M0 atomic operations with a mocked CPU mask boundary.
 *
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/atomic.h"
#include "nexus/components/log.h"
#include "nexus/os/baremetal.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
using ::testing::_;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;

/** \brief Observe CPU exclusion without substituting atomic operations. */
class Cpu {
  public:
    MOCK_METHOD(nx_arch_irq_state_t, save, ());
    MOCK_METHOD(void, restore, (nx_arch_irq_state_t));
};
Cpu* cpu;

/** \brief Supply the application-owned synchronous sink boundary. */
class Sink {
  public:
    MOCK_METHOD(nx_result_t, write,
                (nx_log_level_t, const void*, size_t, nx_time_us_t));
};

/** \brief Keep incoming masks explicit and restore them after each RMW. */
class AtomicFallback : public ::testing::Test {
  protected:
    StrictMock<Cpu> masks;
    uint32_t primask = 0U;

    /** \brief Establish a fresh CPU-local hardware boundary. */
    void SetUp() override {
        cpu = &masks;
    }

    /** \brief Reject retained fixture state after a complete operation. */
    void TearDown() override {
        cpu = nullptr;
    }

    /** \brief Require one bounded save/restore pair, retaining incoming mask.
     */
    void expect_guard(uint32_t incoming) {
        primask = incoming;
        InSequence order;
        EXPECT_CALL(masks, save()).WillOnce([this, incoming] {
            EXPECT_EQ(primask, incoming);
            primask = 1U;
            return nx_arch_irq_state_t{incoming};
        });
        EXPECT_CALL(masks,
                    restore(Field(&nx_arch_irq_state_t::value, incoming)))
            .WillOnce([this, incoming](nx_arch_irq_state_t) {
                EXPECT_EQ(primask, 1U);
                primask = incoming;
            });
    }

    /** \brief Bind a nonexpired independent time domain. */
    static uint64_t now(void*) {
        return 0U;
    }
};

TEST_F(AtomicFallback, LoadAndStoreDoNotMaskInterrupts) {
    uint32_t value = 3U;
    EXPECT_EQ(nx_atomic_u32_load_relaxed(&value), 3U);
    nx_atomic_u32_store_relaxed(&value, 7U);
    EXPECT_EQ(nx_atomic_u32_load_acquire(&value), 7U);
    nx_atomic_u32_store_release(&value, 11U);
    EXPECT_EQ(nx_atomic_u32_load_acquire(&value), 11U);
}

TEST_F(AtomicFallback, FetchAddReturnsPreviousAndPreservesMaskedCaller) {
    uint32_t value = UINT32_MAX;
    expect_guard(1U);
    EXPECT_EQ(nx_atomic_u32_fetch_add_release(&value, 1U), UINT32_MAX);
    EXPECT_EQ(value, 0U);
    EXPECT_EQ(primask, 1U);
}

TEST_F(AtomicFallback, FetchSubWrapsAndRestoresUnmaskedCaller) {
    uint32_t value = 0U;
    expect_guard(0U);
    EXPECT_EQ(nx_atomic_u32_fetch_sub_acq_rel(&value, 1U), 0U);
    EXPECT_EQ(value, UINT32_MAX);
    EXPECT_EQ(primask, 0U);
}

TEST_F(AtomicFallback, UpdateOccursBetweenMaskEntryAndRestoredIrqAccess) {
    uint32_t value = 4U;
    InSequence order;
    EXPECT_CALL(masks, save()).WillOnce([this, &value] {
        value = 7U;
        primask = 1U;
        return nx_arch_irq_state_t{0U};
    });
    EXPECT_CALL(masks, restore(Field(&nx_arch_irq_state_t::value, 0U)))
        .WillOnce([this, &value](nx_arch_irq_state_t) {
            EXPECT_EQ(primask, 1U);
            EXPECT_EQ(value, 8U);
            primask = 0U;
            value = 99U;
        });
    EXPECT_EQ(nx_atomic_u32_fetch_add_acq_rel(&value, 1U), 7U);
    EXPECT_EQ(value, 99U);
    EXPECT_EQ(primask, 0U);
}

TEST_F(AtomicFallback, OtherFixedOrdersUseTheSameBoundedGuard) {
    uint32_t value = 9U;
    expect_guard(0U);
    EXPECT_EQ(nx_atomic_u32_fetch_add_acq_rel(&value, 3U), 9U);
    expect_guard(0U);
    EXPECT_EQ(nx_atomic_u32_fetch_sub_release(&value, 2U), 12U);
    EXPECT_EQ(value, 10U);
    EXPECT_EQ(primask, 0U);
}

TEST_F(AtomicFallback, StrongCompareExchangeSucceedsWithoutChangingExpected) {
    uint32_t value = 4U;
    uint32_t expected = 4U;
    expect_guard(1U);
    EXPECT_TRUE(nx_atomic_u32_compare_exchange_acq_rel(&value, &expected, 8U));
    EXPECT_EQ(value, 8U);
    EXPECT_EQ(expected, 4U);
    EXPECT_EQ(primask, 1U);
}

TEST_F(AtomicFallback, FailedCompareExchangeReturnsObservedValueAndNoWrite) {
    uint32_t value = 4U;
    uint32_t expected = 5U;
    expect_guard(0U);
    EXPECT_FALSE(nx_atomic_u32_compare_exchange_acq_rel(&value, &expected, 8U));
    EXPECT_EQ(value, 4U);
    EXPECT_EQ(expected, 4U);
    EXPECT_EQ(primask, 0U);
}

TEST_F(AtomicFallback, BaremetalWakePublishesThroughGuardAndSequenceWrap) {
    nx_baremetal_notify_t notification{};
    ASSERT_EQ(nx_baremetal_notify_init(&notification, now, nullptr),
              NX_SUCCESS);
    const auto port = nx_baremetal_notify_port(&notification);
    notification.sequence = UINT32_MAX;
    EXPECT_EQ(port.arm(port.context), UINT32_MAX);
    expect_guard(1U);
    EXPECT_EQ(port.wake(port.context), NX_SUCCESS);
    EXPECT_EQ(primask, 1U);
    EXPECT_EQ(port.arm(port.context), 0U);
    EXPECT_EQ(port.wait(port.context, UINT32_MAX, 10U), NX_SUCCESS);
}

TEST_F(AtomicFallback, LoggerReleasesCpuMaskBeforeCallingApplicationSink) {
    StrictMock<Sink> sink;
    nx_log_t logger{};
    nx_log_sink_port_t port{};
    port.context = &sink;
    port.write = [](void* context, nx_log_level_t level, const void* bytes,
                    size_t length, nx_time_us_t deadline) {
        return static_cast<Sink*>(context)->write(level, bytes, length,
                                                  deadline);
    };
    ASSERT_EQ(nx_log_init(&logger, port, NX_LOG_INFO), NX_SUCCESS);
    const char bytes[] = "test";
    expect_guard(0U);
    EXPECT_CALL(sink, write(NX_LOG_INFO, bytes, 4U, 100U))
        .WillOnce([this, &logger](nx_log_level_t, const void*, size_t,
                                  nx_time_us_t) {
            EXPECT_EQ(primask, 0U);
            expect_guard(0U);
            EXPECT_EQ(nx_log_write(&logger, NX_LOG_INFO, "recursive", 9U, 100U),
                      NX_ERROR_BUSY);
            EXPECT_EQ(primask, 0U);
            EXPECT_EQ(nx_log_stop(&logger), NX_ERROR_BUSY);
            return NX_ERROR_IO;
        });
    EXPECT_EQ(nx_log_write(&logger, NX_LOG_INFO, bytes, 4U, 100U), NX_ERROR_IO);
    EXPECT_EQ(logger.busy, 0U);
    EXPECT_EQ(nx_log_stop(&logger), NX_SUCCESS);
    EXPECT_EQ(nx_log_write(&logger, NX_LOG_INFO, bytes, 4U, 100U),
              NX_ERROR_STATE);
}
} /* namespace */

/** \brief Save the injected CPU mask, not the operation under test. */
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    return cpu->save();
}

/** \brief Restore the exact incoming token at the external CPU boundary. */
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    cpu->restore(previous);
}
