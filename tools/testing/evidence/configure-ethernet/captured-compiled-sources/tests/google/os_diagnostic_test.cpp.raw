/**
 * \file            os_diagnostic_test.cpp
 * \brief           Caller-owned diagnostic ring overflow and context contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/os/diagnostic.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <limits>
using ::testing::Return;
namespace {
bool privileged = true;
uint32_t exception = 0;
uint32_t mask = 0;
uint32_t saves = 0;
class Diagnostic : public ::testing::Test {
  protected:
    nx_diagnostic_ring_t ring = {};
    nx_diagnostic_event_t entries[3] = {};
    nx_diagnostic_event_t event = {100, 200, 300, 400};
    void SetUp() override {
        privileged = true;
        exception = 0;
        mask = 0;
        saves = 0;
    }
    void Init(size_t count = 3) {
        ASSERT_EQ(nx_diagnostic_ring_init(&ring, entries, count), NX_SUCCESS);
    }
};
TEST_F(Diagnostic, RejectsMissingStorageAndZeroCapacity) {
    EXPECT_EQ(nx_diagnostic_ring_init(nullptr, entries, 3), NX_ERROR_INVALID);
    EXPECT_EQ(nx_diagnostic_ring_init(&ring, nullptr, 3), NX_ERROR_INVALID);
    EXPECT_EQ(nx_diagnostic_ring_init(&ring, entries, 0), NX_ERROR_INVALID);
    EXPECT_EQ(ring.events, nullptr);
}
TEST_F(Diagnostic, InitializesWithoutMaskingOrChangingEntries) {
    entries[0] = event;
    Init();
    EXPECT_EQ(saves, 0U);
    EXPECT_EQ(entries[0].value, 300U);
    EXPECT_EQ(ring.capacity, 3U);
    EXPECT_EQ(ring.count, 0U);
}
TEST_F(Diagnostic, CopiesRecordByValue) {
    Init();
    ASSERT_EQ(nx_diagnostic_ring_write(&ring, &event), NX_SUCCESS);
    event.value = 900;
    nx_diagnostic_event_t out = {};
    ASSERT_EQ(nx_diagnostic_ring_read(&ring, &out), NX_SUCCESS);
    EXPECT_EQ(out.timestamp, 100U);
    EXPECT_EQ(out.identity, 200U);
    EXPECT_EQ(out.value, 300U);
    EXPECT_EQ(out.event, 400U);
}
TEST_F(Diagnostic, FullDoesNotOverwriteAndCountsDrop) {
    Init(1);
    ASSERT_EQ(nx_diagnostic_ring_write(&ring, &event), NX_SUCCESS);
    event.value = 500;
    EXPECT_EQ(nx_diagnostic_ring_write(&ring, &event), NX_ERROR_BUSY);
    nx_diagnostic_event_t out = {};
    ASSERT_EQ(nx_diagnostic_ring_read(&ring, &out), NX_SUCCESS);
    EXPECT_EQ(out.value, 300U);
    uint32_t dropped = 0;
    ASSERT_EQ(nx_diagnostic_ring_dropped(&ring, &dropped), NX_SUCCESS);
    EXPECT_EQ(dropped, 1U);
}
TEST_F(Diagnostic, SaturatesDropCounter) {
    Init(1);
    ASSERT_EQ(nx_diagnostic_ring_write(&ring, &event), NX_SUCCESS);
    ring.dropped = std::numeric_limits<uint32_t>::max();
    EXPECT_EQ(nx_diagnostic_ring_write(&ring, &event), NX_ERROR_BUSY);
    EXPECT_EQ(ring.dropped, std::numeric_limits<uint32_t>::max());
}
TEST_F(Diagnostic, EmptyLeavesDestinationUnchanged) {
    Init();
    EXPECT_EQ(nx_diagnostic_ring_read(&ring, &event), NX_ERROR_BUSY);
    EXPECT_EQ(event.value, 300U);
}
TEST_F(Diagnostic, NonPowerOfTwoCapacityWrapsAndPreservesOrder) {
    Init();
    for (uint32_t round = 0; round < 20; round++) {
        for (uint32_t i = 0; i < 3; i++) {
            event.value = round * 3 + i;
            ASSERT_EQ(nx_diagnostic_ring_write(&ring, &event), NX_SUCCESS);
        }
        for (uint32_t i = 0; i < 3; i++) {
            nx_diagnostic_event_t out = {};
            ASSERT_EQ(nx_diagnostic_ring_read(&ring, &out), NX_SUCCESS);
            EXPECT_EQ(out.value, round * 3 + i);
        }
    }
}
TEST_F(Diagnostic, RejectsUnprivilegedBeforeMask) {
    Init();
    privileged = false;
    EXPECT_EQ(nx_diagnostic_ring_write(&ring, &event), NX_ERROR_CONTEXT);
    EXPECT_EQ(saves, 0U);
}
TEST_F(Diagnostic, RejectsNmiAndHardFaultBeforeMask) {
    Init();
    for (exception = 1; exception <= 3; exception++)
        EXPECT_EQ(nx_diagnostic_ring_write(&ring, &event), NX_ERROR_CONTEXT);
    EXPECT_EQ(saves, 0U);
}
TEST_F(Diagnostic, ConfigurableIrqCanPublish) {
    Init();
    exception = 16;
    EXPECT_EQ(nx_diagnostic_ring_write(&ring, &event), NX_SUCCESS);
}
TEST_F(Diagnostic, PreservesIncomingMask) {
    Init();
    mask = 1;
    EXPECT_EQ(nx_diagnostic_ring_write(&ring, &event), NX_SUCCESS);
    EXPECT_EQ(mask, 1U);
    EXPECT_EQ(saves, 1U);
}
TEST_F(Diagnostic, MissingObjectsRejectWithoutEffects) {
    uint32_t dropped = 0;
    EXPECT_EQ(nx_diagnostic_ring_write(nullptr, &event), NX_ERROR_INVALID);
    EXPECT_EQ(nx_diagnostic_ring_read(&ring, &event), NX_ERROR_INVALID);
    EXPECT_EQ(nx_diagnostic_ring_dropped(&ring, &dropped), NX_ERROR_INVALID);
    Init();
    EXPECT_EQ(nx_diagnostic_ring_write(&ring, nullptr), NX_ERROR_INVALID);
    EXPECT_EQ(nx_diagnostic_ring_read(&ring, nullptr), NX_ERROR_INVALID);
    EXPECT_EQ(nx_diagnostic_ring_dropped(&ring, nullptr), NX_ERROR_INVALID);
}
TEST_F(Diagnostic, CapacityOverflowRejectsBeforeStateMutation) {
    if (sizeof(size_t) > sizeof(uint32_t)) {
        EXPECT_EQ(nx_diagnostic_ring_init(&ring, entries,
                                          static_cast<size_t>(UINT32_MAX) + 1),
                  NX_ERROR_INVALID);
        EXPECT_EQ(ring.events, nullptr);
    }
}
TEST_F(Diagnostic, UnalignedStorageRejectsBeforeStateMutation) {
    alignas(nx_diagnostic_event_t) unsigned char bytes[64] = {};
    EXPECT_EQ(
        nx_diagnostic_ring_init(
            &ring, reinterpret_cast<nx_diagnostic_event_t*>(bytes + 1), 1),
        NX_ERROR_INVALID);
    EXPECT_EQ(ring.events, nullptr);
}
TEST_F(Diagnostic, ReadersAndCountersHonorTheSamePrivilegeDomain) {
    Init();
    privileged = false;
    uint32_t dropped = 50;
    EXPECT_EQ(nx_diagnostic_ring_read(&ring, &event), NX_ERROR_CONTEXT);
    EXPECT_EQ(nx_diagnostic_ring_dropped(&ring, &dropped), NX_ERROR_CONTEXT);
    EXPECT_EQ(dropped, 50U);
    EXPECT_EQ(saves, 0U);
}
} /* namespace */
extern "C" bool nx_arch_is_privileged(void) {
    return privileged;
}
extern "C" uint32_t nx_arch_exception_number(void) {
    return exception;
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t state = {mask};
    mask = 1;
    saves++;
    return state;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    mask = previous.value;
}
