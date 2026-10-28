/**
 * \file            gd32f303_exti_test.cpp
 * \brief           Actual GD303 EXTI independent source and borrow tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f303_provider.h"
#include "model.h"
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>
class GD303Exti : public testing::Test {
  protected:
    nx_exti_event_t ring[2]{};
    nx_exti_event_t other_ring[2]{};
    nx_gd32f303_exti_state_t state{}, other{};
    const nx_exti_port_t face{&nx_gd32f303_exti_ops, &state};
    const nx_exti_port_t other_face{&nx_gd32f303_exti_ops, &other};
    void* mapping = MAP_FAILED;
    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(0x40000000U), 0x30000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(0x40000000U));
        std::memset(g_gd303_irq, 0, sizeof(g_gd303_irq));
        std::memset(g_gd303_pending, 0, sizeof(g_gd303_pending));
        g_gd303_hold_irq_off = false;
        g_gd303_ignore = nullptr;
        g_gd303_exti_hold_pending = g_gd303_exti_reassert_pending = false;
        g_gd303_writes = 0U;
        g_nx_gd32f303_system = {};
        g_nx_gd32f303_system.started = true;
        g_nx_gd32f303_system.core_hz = 8000000U;
        RCU_APB2EN = RCU_APB2EN_PAEN | RCU_APB2EN_AFEN;
        GPIO_CTL0(GPIOA) = 0x44444444U;
        state.registers = other.registers = NX_GD303_EXTI;
        state.mux = other.mux = NX_GD303_EXTI_MUX;
        state.gpio = other.gpio = NX_GD303_EXTI_GPIO;
        state.storage = ring;
        other.storage = other_ring;
        state.capacity = other.capacity = 2U;
        state.edge = other.edge = NX_EXTI_BOTH;
        state.priority = other.priority = 5U;
        other.line = 1U;
        state.irq_policy = other.irq_policy = {NX_IRQ_KERNEL_NONE, 60U, 4U, 0U};
    }
    void TearDown() override {
        if (mapping != MAP_FAILED)
            munmap(mapping, 0x30000U);
    }
    void Start() {
        ASSERT_EQ(NX_SUCCESS, nx_gd32f303_exti_initialize(&state));
    }
    void Edge() {
        EXTI_PD |= 1U;
        GPIO_ISTAT(GPIOA) |= 1U;
        nx_gd32f303_exti_irq(&state);
    }
};
TEST_F(GD303Exti, ColdPrepareKeepsRetainedStorageAndInitializesExactRoute) {
    const nx_gd32f303_exti_config_t config{
        other_ring,   2U, {NX_IRQ_KERNEL_NONE, 60U, 4U, 0U},
        NX_EXTI_BOTH, 0U, 5U};
    state.retained = true;
    EXPECT_EQ(nx_gd32f303_exti_prepare(&state, &config), NX_ERROR_BUSY);
    EXPECT_EQ(state.storage, ring);
    EXPECT_EQ(g_gd303_writes, 0U);
    state.retained = false;
    ASSERT_EQ(nx_gd32f303_exti_prepare(&state, &config), NX_SUCCESS);
    EXPECT_EQ(state.storage, other_ring);
    EXPECT_TRUE(state.initialized);
    ASSERT_EQ(nx_exti_port_stop(&face), NX_SUCCESS);
}
TEST_F(GD303Exti, ExactRouteUsesIndependentVectorsAndPreservesOtherFields) {
    AFIO_EXTISS0 = 0x1234U;
    Start();
    EXPECT_EQ(0x1230U, AFIO_EXTISS0);
    EXPECT_TRUE(g_gd303_irq[EXTI0_IRQn]);
    EXPECT_FALSE(g_gd303_irq[EXTI1_IRQn]);
    EXPECT_EQ(1U, EXTI_INTEN);
    ASSERT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
    EXPECT_EQ(0x1234U, AFIO_EXTISS0);
}
TEST_F(GD303Exti, ForeignLineAndInvalidRingRejectBeforeWrites) {
    EXTI_INTEN = 1U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_gd32f303_exti_initialize(&state));
    EXPECT_FALSE(state.retained);
    EXPECT_EQ(0U, g_gd303_writes);
    EXTI_INTEN = 0U;
    state.storage = reinterpret_cast<nx_exti_event_t*>(&state);
    EXPECT_EQ(NX_ERROR_INVALID, nx_gd32f303_exti_initialize(&state));
}
TEST_F(GD303Exti, OverflowPreservesQueuedFactsThenLossBoundary) {
    Start();
    Edge();
    Edge();
    Edge();
    EXPECT_TRUE(state.loss);
    nx_exti_event_t events[3]{};
    size_t count = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_exti_port_read(&face, events, 3U, &count));
    ASSERT_EQ(3U, count);
    EXPECT_EQ(NX_EXTI_EVENT_COALESCED, events[0].flags);
    EXPECT_EQ(NX_EXTI_EVENT_LOSS, events[2].flags);
}
TEST_F(GD303Exti, NewPendingAfterW1cIsNotAnErrorAndOtherLineSurvivesStop) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_gd32f303_exti_initialize(&other));
    g_gd303_exti_reassert_pending = true;
    Edge();
    EXPECT_EQ(1U, state.count);
    EXPECT_EQ(1U, EXTI_PD & 1U);
    g_gd303_exti_reassert_pending = false;
    ASSERT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
    EXPECT_EQ(2U, EXTI_INTEN);
    EXPECT_TRUE(g_gd303_irq[EXTI1_IRQn]);
}
TEST_F(GD303Exti, FailedSourceAndPendingWithdrawalRetainRingUntilRetry) {
    Start();
    g_gd303_ignore = &EXTI_INTEN;
    EXPECT_EQ(NX_ERROR_IO, nx_exti_port_stop(&face));
    EXPECT_TRUE(state.retained);
    EXPECT_FALSE(g_gd303_irq[EXTI0_IRQn]);
    g_gd303_ignore = nullptr;
    EXTI_PD = 1U;
    g_gd303_exti_hold_pending = true;
    EXPECT_EQ(NX_ERROR_IO, nx_exti_port_stop(&face));
    EXPECT_TRUE(state.retained);
    g_gd303_exti_hold_pending = false;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
    EXPECT_FALSE(state.retained);
}
TEST_F(GD303Exti, CapturedWakeSeesFactAndCannotReleaseItsOwnBorrow) {
    Start();
    testing::MockFunction<nx_result_t()> notification;
    nx_irq_wake_t wake{
        &notification,
        [](void* p) {
            return static_cast<testing::MockFunction<nx_result_t()>*>(p)
                ->Call();
        },
        false};
    ASSERT_EQ(NX_SUCCESS, nx_exti_port_attach_wake(&face, &wake, 0U));
    EXPECT_CALL(notification, Call()).WillOnce(testing::Invoke([&]() {
        EXPECT_EQ(1U, state.count);
        EXPECT_EQ(NX_ERROR_BUSY, nx_exti_port_stop(&face));
        EXPECT_EQ(&wake, state.wake);
        return NX_SUCCESS;
    }));
    Edge();
    EXPECT_EQ(0U, state.wake_publishers);
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
}
