/**
 * \file            g474_exti_test.cpp
 * \brief           F103 EXTI production and generated lifecycle contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "model.h"
#include "stm32f103_provider.h"
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
class ExtiWake {
  public:
    MOCK_METHOD(nx_result_t, Notify, ());
};
class F103Exti : public testing::Test {
  protected:
    nx_exti_event_t storage[3]{};
    nx_exti_event_t other_storage[2]{};
    nx_stm32f103_exti_state_t state{};
    nx_stm32f103_exti_state_t other{};
    const nx_exti_port_t face{&nx_stm32f103_exti_ops, &state};
    const nx_exti_port_t other_face{&nx_stm32f103_exti_ops, &other};
    void SetUp() override {
        nx_f103_model_reset();
        std::memset(EXTI, 0, sizeof(*EXTI));
        std::memset(AFIO, 0, sizeof(*AFIO));
        g_f103_model.refuse_exti_pending = g_f103_model.reassert_exti_pending =
            false;
        g_f103_model.refused_falling = 0U;
        std::memset(g_f103_model.irq_enables, 0,
                    sizeof(g_f103_model.irq_enables));
        g_nx_stm32f103_system.started = true;
        g_nx_stm32f103_system.core_hz = 8000000U;

        RCC->APB2ENR = RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPAEN;
        GPIOA->CRL = UINT32_C(0x44444444);
        state.registers = other.registers = EXTI;
        state.mux = other.mux = AFIO;
        state.gpio = other.gpio = GPIOA;
        state.storage = storage;
        other.storage = other_storage;
        state.capacity = 3U;
        other.capacity = 2U;
        state.edge = NX_EXTI_BOTH;
        other.edge = NX_EXTI_FALLING;
        state.line = 0U;
        other.line = 1U;
        state.priority = 5U;
        other.priority = 6U;
        state.irq_policy = other.irq_policy = {NX_IRQ_KERNEL_NONE, 43U, 4U, 0U};
    }
    void Start() {
        ASSERT_EQ(NX_SUCCESS, nx_stm32f103_exti_initialize(&state));
    }
    void Edge(uint32_t level = 1U) {
        GPIOA->IDR = level;
        EXTI->PR |= 1U;
        nx_stm32f103_exti_irq(&state);
    }
};
TEST_F(F103Exti, ColdPrepareKeepsRetainedStorageAndInitializesExactRoute) {
    const nx_stm32f103_exti_config_t config{
        other_storage, 2U, {NX_IRQ_KERNEL_NONE, 43U, 4U, 0U},
        NX_EXTI_BOTH,  0U, 5U};
    state.retained = true;
    EXPECT_EQ(nx_stm32f103_exti_prepare(&state, &config), NX_ERROR_BUSY);
    EXPECT_EQ(state.storage, storage);
    EXPECT_EQ(g_f103_model.writes, 0U);
    state.retained = false;
    ASSERT_EQ(nx_stm32f103_exti_prepare(&state, &config), NX_SUCCESS);
    EXPECT_EQ(state.storage, other_storage);
    EXPECT_TRUE(state.initialized);
    ASSERT_EQ(nx_exti_port_stop(&face), NX_SUCCESS);
}
TEST_F(F103Exti, InitializeConfiguresExactInputVectorAndPreservesOtherLines) {
    EXTI->IMR = EXTI->RTSR = EXTI->FTSR = 1U << 9U;
    AFIO->EXTICR[0] = 0x5674U;
    Start();
    EXPECT_TRUE(state.initialized);
    EXPECT_TRUE(state.retained);
    EXPECT_EQ(4U, GPIOA->CRL & 15U);
    EXPECT_EQ(0x5670U, AFIO->EXTICR[0]);
    EXPECT_EQ((1U << 9U) | 1U, EXTI->IMR);
    EXPECT_EQ((1U << 9U) | 1U, EXTI->RTSR);
    EXPECT_EQ((1U << 9U) | 1U, EXTI->FTSR);
    EXPECT_TRUE(g_f103_model.irq_enabled[EXTI0_IRQn]);
    EXPECT_FALSE(g_f103_model.irq_enabled[EXTI1_IRQn]);
    EXPECT_EQ(5U, g_f103_model.priority[EXTI0_IRQn]);
}
TEST_F(F103Exti, UnsupportedLineRejectsWithoutEffects) {
    state.line = 5U;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32f103_exti_initialize(&state));
    EXPECT_EQ(0U, g_f103_model.writes);
    EXPECT_FALSE(state.retained);
}
TEST_F(F103Exti, ExistingLineOrVectorRejectsWithoutRetainedStorage) {
    for (auto* reg :
         {&EXTI->IMR, &EXTI->EMR, &EXTI->RTSR, &EXTI->FTSR, &EXTI->PR}) {
        *reg = 1U;
        EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f103_exti_initialize(&state));
        EXPECT_FALSE(state.retained);
        EXPECT_EQ(0U, g_f103_model.writes);
        *reg = 0U;
    }
    g_f103_model.irq_active[EXTI0_IRQn] = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f103_exti_initialize(&state));
    EXPECT_EQ(0U, g_f103_model.writes);
}
TEST_F(F103Exti, DisabledClockAndInvalidCapacityRejectBeforeWrites) {
    RCC->APB2ENR = 0U;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32f103_exti_initialize(&state));
    RCC->APB2ENR = RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPAEN;
    state.capacity = 0U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32f103_exti_initialize(&state));
    state.capacity = 4097U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32f103_exti_initialize(&state));
    EXPECT_EQ(0U, g_f103_model.writes);
}
TEST_F(F103Exti, StorageOverlappingProviderIsRejected) {
    state.storage = reinterpret_cast<nx_exti_event_t*>(&state);
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32f103_exti_initialize(&state));
    EXPECT_EQ(0U, g_f103_model.writes);
}
TEST_F(F103Exti, PrivilegedUnmaskedTaskRequiredForColdMutation) {
    g_f103_model.privileged = false;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_stm32f103_exti_initialize(&state));
    g_f103_model.privileged = true;
    auto saved = nx_arch_irq_save();
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_stm32f103_exti_initialize(&state));
    nx_arch_irq_restore(saved);
    EXPECT_EQ(0U, g_f103_model.writes);
}
TEST_F(F103Exti, FailedMuxWriteRetainsCleanupUntilStopRestoresOriginal) {
    AFIO->EXTICR[0] = 2U;
    g_f103_model.ignore_write = &AFIO->EXTICR[0];
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f103_exti_initialize(&state));
    EXPECT_TRUE(state.retained);
    EXPECT_FALSE(state.initialized);
    g_f103_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
    EXPECT_EQ(2U, AFIO->EXTICR[0]);
    EXPECT_FALSE(state.retained);
}
TEST_F(F103Exti, FailedNvicEnableRetainsExactOwnedLine) {
    g_f103_model.hold_irq_on = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f103_exti_initialize(&state));
    EXPECT_TRUE(state.retained);
    g_f103_model.hold_irq_on = false;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
    EXPECT_EQ(0U, EXTI->IMR | EXTI->RTSR | EXTI->FTSR);
}
TEST_F(F103Exti, OneIrqPublishesSampledCoalescedFactAndTimestamp) {
    Start();
    g_f103_model.timer.CNT = 123U;
    Edge();
    nx_exti_event_t event{};
    size_t count = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_exti_port_read(&face, &event, 1U, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(123U, event.timestamp_us);
    EXPECT_EQ(NX_EXTI_RISING, event.edge);
    EXPECT_EQ(0U, event.line);
    EXPECT_EQ(NX_EXTI_EVENT_COALESCED, event.flags);
    EXPECT_EQ(0U, EXTI->PR);
}
TEST_F(F103Exti, PendingReassertionIsNotMistakenForAnAcknowledgementFault) {
    Start();
    g_f103_model.reassert_exti_pending = true;
    Edge();
    EXPECT_EQ(1U, state.count);
    EXPECT_EQ(1U, EXTI->PR);
    EXPECT_TRUE(state.initialized);
    EXPECT_FALSE(state.closing);
}
TEST_F(F103Exti, ArbitraryCapacityWrapPreservesLossBoundaryBeforeLaterFacts) {
    Start();
    Edge();
    Edge(0U);
    Edge();
    Edge(0U);
    EXPECT_TRUE(state.loss);
    nx_exti_event_t events[3]{};
    size_t count = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_exti_port_read(&face, events, 2U, &count));
    EXPECT_EQ(2U, count);
    Edge();
    ASSERT_EQ(NX_SUCCESS, nx_exti_port_read(&face, events, 3U, &count));
    ASSERT_EQ(2U, count);
    EXPECT_EQ(NX_EXTI_EVENT_COALESCED, events[0].flags);
    EXPECT_EQ(NX_EXTI_EVENT_LOSS, events[1].flags);
    EXPECT_FALSE(state.loss);
    Edge(0U);
    ASSERT_EQ(NX_SUCCESS, nx_exti_port_read(&face, events, 3U, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(NX_EXTI_FALLING, events[0].edge);
}
TEST_F(F103Exti, DestinationAndCountMayNotOverlapBorrowedRingOrEachOther) {
    Start();
    Edge();
    size_t count = 99U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_exti_port_read(&face, storage, 1U, &count));
    EXPECT_EQ(1U, state.count);
    nx_exti_event_t event{};
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_exti_port_read(&face, &event, 1U,
                                reinterpret_cast<size_t*>(&event)));
    EXPECT_EQ(1U, state.count);
}
TEST_F(F103Exti, MisalignedOutputOrCountIsRejectedWithoutConsumingFact) {
    Start();
    Edge();
    alignas(nx_exti_event_t) uint8_t bytes[2U * sizeof(nx_exti_event_t)]{};
    size_t count = 0U;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_exti_port_read(&face,
                                reinterpret_cast<nx_exti_event_t*>(bytes + 1U),
                                1U, &count));
    nx_exti_event_t event{};
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_exti_port_read(&face, &event, 1U,
                                reinterpret_cast<size_t*>(bytes + 1U)));
    EXPECT_EQ(1U, state.count);
}
TEST_F(F103Exti, ReadAndStopPreserveIncomingMaskWhenRejectingContext) {
    Start();
    Edge();
    nx_exti_event_t event{};
    size_t count = 99U;
    auto saved = nx_arch_irq_save();
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_exti_port_read(&face, &event, 1U, &count));
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_exti_port_stop(&face));
    EXPECT_TRUE(nx_arch_irq_is_masked());
    nx_arch_irq_restore(saved);
    EXPECT_EQ(99U, count);
    EXPECT_EQ(1U, state.count);
}
TEST_F(F103Exti, IndependentLineStopPreservesOtherLineEventsAndIrq) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_stm32f103_exti_initialize(&other));
    EXTI->PR = 3U;
    nx_stm32f103_exti_irq(&other);
    ASSERT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
    EXPECT_EQ(2U, EXTI->IMR);
    EXPECT_TRUE(g_f103_model.irq_enabled[EXTI1_IRQn]);
    EXPECT_FALSE(g_f103_model.irq_enabled[EXTI0_IRQn]);
    EXPECT_EQ(1U, other.count);
    EXPECT_TRUE(other.initialized);
}
TEST_F(F103Exti, StopRefusedMaskKeepsBorrowButAlsoWithdrawsItsVector) {
    Start();
    const nx_irq_wake_t wake{nullptr, [](void*) { return NX_SUCCESS; }, false};
    ASSERT_EQ(NX_SUCCESS, nx_exti_port_attach_wake(&face, &wake, 0U));
    g_f103_model.ignore_write = &EXTI->IMR;
    EXPECT_EQ(NX_ERROR_IO, nx_exti_port_stop(&face));
    EXPECT_TRUE(state.retained);
    EXPECT_EQ(&wake, state.wake);
    EXPECT_FALSE(g_f103_model.irq_enabled[EXTI0_IRQn]);
    g_f103_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
    EXPECT_FALSE(state.retained);
    EXPECT_EQ(nullptr, state.wake);
}
TEST_F(F103Exti, StopRefusedPendingOrVectorRequiresObservableRetry) {
    Start();
    EXTI->PR = 1U;
    g_f103_model.refuse_exti_pending = true;
    EXPECT_EQ(NX_ERROR_IO, nx_exti_port_stop(&face));
    EXPECT_TRUE(state.retained);
    g_f103_model.refuse_exti_pending = false;
    g_f103_model.irq_enabled[EXTI0_IRQn] = true;
    g_f103_model.hold_irq_off = true;
    EXPECT_EQ(NX_ERROR_IO, nx_exti_port_stop(&face));
    EXPECT_TRUE(state.retained);
    g_f103_model.hold_irq_off = false;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
}
TEST_F(F103Exti, StopRestoresOnlyOwnedMuxAndRetainsFailedRestore) {
    AFIO->EXTICR[0] = 0x1234U;
    Start();
    AFIO->EXTICR[0] = 0x5670U;
    g_f103_model.ignore_write = &AFIO->EXTICR[0];
    EXPECT_EQ(NX_ERROR_IO, nx_exti_port_stop(&face));
    EXPECT_TRUE(state.retained);
    g_f103_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
    EXPECT_EQ(0x5674U, AFIO->EXTICR[0]);
}
TEST_F(F103Exti, WakeSeesLatchedFactAndCapturedPublisherCannotBeReclaimed) {
    Start();
    testing::StrictMock<ExtiWake> boundary;
    const nx_irq_wake_t wake{
        &boundary,
        [](void* context) { return static_cast<ExtiWake*>(context)->Notify(); },
        false};
    ASSERT_EQ(NX_SUCCESS, nx_exti_port_attach_wake(&face, &wake, 0U));
    EXPECT_CALL(boundary, Notify()).WillOnce(testing::Invoke([&]() {
        EXPECT_EQ(1U, state.count);
        EXPECT_EQ(NX_ERROR_BUSY, nx_exti_port_stop(&face));
        EXPECT_EQ(NX_ERROR_BUSY, nx_exti_port_attach_wake(&face, nullptr, 0U));
        EXPECT_EQ(&wake, state.wake);
        return NX_ERROR_IO;
    }));
    Edge();
    EXPECT_EQ(0U, state.wake_publishers);
    EXPECT_EQ(1U, state.count);
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
}
TEST_F(F103Exti, KernelWakeRequiresActualPriorityAndSelectedPolicy) {
    Start();
    const nx_irq_wake_t wake{nullptr, [](void*) { return NX_SUCCESS; }, true};
    EXPECT_EQ(NX_ERROR_PERMISSION, nx_exti_port_attach_wake(&face, &wake, 0U));
    state.irq_policy = {NX_IRQ_KERNEL_BASEPRI, 43U, 4U, 5U};
    g_f103_model.priority[EXTI0_IRQn] = 4U;
    EXPECT_EQ(NX_ERROR_PERMISSION, nx_exti_port_attach_wake(&face, &wake, 5U));
    g_f103_model.priority[EXTI0_IRQn] = 5U;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_attach_wake(&face, &wake, 5U));
}

TEST_F(F103Exti, ForeignAlternateOutputNibbleIsNotModernAnalogMode) {
    GPIOA->CRL = (GPIOA->CRL & ~15U) | 11U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f103_exti_initialize(&state));
    EXPECT_EQ(0U, g_f103_model.writes);
    EXPECT_FALSE(state.retained);
}
