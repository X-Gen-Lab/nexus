/**
 * \file            g474_exti_test.cpp
 * \brief           G474 EXTI production and generated lifecycle contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_exti_model.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32g474_provider.h"
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
extern "C" {
EXTI_TypeDef g_g474_exti;
SYSCFG_TypeDef g_g474_exti_mux;
USART_TypeDef g_g474_factory_uart;
void EXTI0_IRQHandler(void);
void EXTI1_IRQHandler(void);
}
static bool s_refuse_pending;
static bool s_reassert_pending;
static uint32_t s_refused_falling;
static unsigned s_irq_enables[102];
nx_result_t nx_g474_exti_model_irq_control(int irq, bool enabled,
                                           uint8_t priority) {
    if (enabled && irq >= 0 && irq < 102)
        ++s_irq_enables[irq];
    return nx_g474_model_irq_control(irq, enabled, priority);
}
uint32_t nx_g474_exti_model_read(volatile uint32_t* reg) {
    return nx_g474_model_read(reg);
}
void nx_g474_exti_model_write(volatile uint32_t* reg, uint32_t value) {
    if (reg == &EXTI->PR1) {
        ++g_g474_model.writes;
        if (!s_refuse_pending) {
            *reg &= ~value;
            if (s_reassert_pending)
                *reg |= value;
        }
    } else {
        if (reg == &EXTI->FTSR1)
            value = (value & ~s_refused_falling) | (*reg & s_refused_falling);
        nx_g474_model_write(reg, value);
    }
}
class ExtiWake {
  public:
    MOCK_METHOD(nx_result_t, Notify, ());
};
class G474Exti : public testing::Test {
  protected:
    nx_exti_event_t storage[3]{};
    nx_exti_event_t other_storage[2]{};
    nx_stm32g474_exti_state_t state{};
    nx_stm32g474_exti_state_t other{};
    const nx_exti_port_t face{&nx_stm32g474_exti_ops, &state};
    const nx_exti_port_t other_face{&nx_stm32g474_exti_ops, &other};
    void SetUp() override {
        nx_g474_model_reset();
        std::memset(EXTI, 0, sizeof(*EXTI));
        std::memset(SYSCFG, 0, sizeof(*SYSCFG));
        s_refuse_pending = s_reassert_pending = false;
        s_refused_falling = 0U;
        std::memset(s_irq_enables, 0, sizeof(s_irq_enables));
        g_nx_stm32g474_system.started = true;
        g_nx_stm32g474_system.core_hz = 170000000U;
        RCC->AHB2ENR = RCC_AHB2ENR_GPIOAEN;
        RCC->APB2ENR = RCC_APB2ENR_SYSCFGEN;
        GPIOA->MODER = 15U;
        state.registers = other.registers = EXTI;
        state.mux = other.mux = SYSCFG;
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
        state.irq_policy =
            other.irq_policy = {NX_IRQ_KERNEL_NONE, 102U, 4U, 0U};
    }
    void Start() {
        ASSERT_EQ(NX_SUCCESS, nx_stm32g474_exti_initialize(&state));
    }
    void Edge(uint32_t level = 1U) {
        GPIOA->IDR = level;
        EXTI->PR1 |= 1U;
        nx_stm32g474_exti_irq(&state);
    }
};
TEST_F(G474Exti, InitializeConfiguresExactInputVectorAndPreservesOtherLines) {
    EXTI->IMR1 = EXTI->RTSR1 = EXTI->FTSR1 = 1U << 9U;
    SYSCFG->EXTICR[0] = 0x5674U;
    Start();
    EXPECT_TRUE(state.initialized);
    EXPECT_TRUE(state.retained);
    EXPECT_EQ(0U, GPIOA->MODER & 3U);
    EXPECT_EQ(0x5670U, SYSCFG->EXTICR[0]);
    EXPECT_EQ((1U << 9U) | 1U, EXTI->IMR1);
    EXPECT_EQ((1U << 9U) | 1U, EXTI->RTSR1);
    EXPECT_EQ((1U << 9U) | 1U, EXTI->FTSR1);
    EXPECT_TRUE(g_g474_model.irq_enabled[EXTI0_IRQn]);
    EXPECT_FALSE(g_g474_model.irq_enabled[EXTI1_IRQn]);
    EXPECT_EQ(5U, g_g474_model.priority[EXTI0_IRQn]);
}
TEST_F(G474Exti, UnsupportedLineRejectsWithoutEffects) {
    state.line = 5U;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32g474_exti_initialize(&state));
    EXPECT_EQ(0U, g_g474_model.writes);
    EXPECT_FALSE(state.retained);
}
TEST_F(G474Exti, ExistingLineOrVectorRejectsWithoutRetainedStorage) {
    for (auto* reg :
         {&EXTI->IMR1, &EXTI->EMR1, &EXTI->RTSR1, &EXTI->FTSR1, &EXTI->PR1}) {
        *reg = 1U;
        EXPECT_EQ(NX_ERROR_BUSY, nx_stm32g474_exti_initialize(&state));
        EXPECT_FALSE(state.retained);
        EXPECT_EQ(0U, g_g474_model.writes);
        *reg = 0U;
    }
    g_g474_model.irq_active[EXTI0_IRQn] = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32g474_exti_initialize(&state));
    EXPECT_EQ(0U, g_g474_model.writes);
}
TEST_F(G474Exti, DisabledClockAndInvalidCapacityRejectBeforeWrites) {
    RCC->APB2ENR = 0U;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32g474_exti_initialize(&state));
    RCC->APB2ENR = RCC_APB2ENR_SYSCFGEN;
    state.capacity = 0U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32g474_exti_initialize(&state));
    state.capacity = 4097U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32g474_exti_initialize(&state));
    EXPECT_EQ(0U, g_g474_model.writes);
}
TEST_F(G474Exti, StorageOverlappingProviderIsRejected) {
    state.storage = reinterpret_cast<nx_exti_event_t*>(&state);
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32g474_exti_initialize(&state));
    EXPECT_EQ(0U, g_g474_model.writes);
}
TEST_F(G474Exti, PrivilegedUnmaskedTaskRequiredForColdMutation) {
    g_g474_model.privileged = false;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_stm32g474_exti_initialize(&state));
    g_g474_model.privileged = true;
    auto saved = nx_arch_irq_save();
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_stm32g474_exti_initialize(&state));
    nx_arch_irq_restore(saved);
    EXPECT_EQ(0U, g_g474_model.writes);
}
TEST_F(G474Exti, FailedMuxWriteRetainsCleanupUntilStopRestoresOriginal) {
    SYSCFG->EXTICR[0] = 2U;
    g_g474_model.ignore_write = &SYSCFG->EXTICR[0];
    EXPECT_EQ(NX_ERROR_IO, nx_stm32g474_exti_initialize(&state));
    EXPECT_TRUE(state.retained);
    EXPECT_FALSE(state.initialized);
    g_g474_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
    EXPECT_EQ(2U, SYSCFG->EXTICR[0]);
    EXPECT_FALSE(state.retained);
}
TEST_F(G474Exti, FailedNvicEnableRetainsExactOwnedLine) {
    g_g474_model.hold_irq_on = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32g474_exti_initialize(&state));
    EXPECT_TRUE(state.retained);
    g_g474_model.hold_irq_on = false;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
    EXPECT_EQ(0U, EXTI->IMR1 | EXTI->RTSR1 | EXTI->FTSR1);
}
TEST_F(G474Exti, OneIrqPublishesSampledCoalescedFactAndTimestamp) {
    Start();
    g_g474_model.timer.CNT = 123U;
    Edge();
    nx_exti_event_t event{};
    size_t count = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_exti_port_read(&face, &event, 1U, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(123U, event.timestamp_us);
    EXPECT_EQ(NX_EXTI_RISING, event.edge);
    EXPECT_EQ(0U, event.line);
    EXPECT_EQ(NX_EXTI_EVENT_COALESCED, event.flags);
    EXPECT_EQ(0U, EXTI->PR1);
}
TEST_F(G474Exti, PendingReassertionIsNotMistakenForAnAcknowledgementFault) {
    Start();
    s_reassert_pending = true;
    Edge();
    EXPECT_EQ(1U, state.count);
    EXPECT_EQ(1U, EXTI->PR1);
    EXPECT_TRUE(state.initialized);
    EXPECT_FALSE(state.closing);
}
TEST_F(G474Exti, ArbitraryCapacityWrapPreservesLossBoundaryBeforeLaterFacts) {
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
TEST_F(G474Exti, DestinationAndCountMayNotOverlapBorrowedRingOrEachOther) {
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
TEST_F(G474Exti, MisalignedOutputOrCountIsRejectedWithoutConsumingFact) {
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
TEST_F(G474Exti, ReadAndStopPreserveIncomingMaskWhenRejectingContext) {
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
TEST_F(G474Exti, IndependentLineStopPreservesOtherLineEventsAndIrq) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_stm32g474_exti_initialize(&other));
    EXTI->PR1 = 3U;
    nx_stm32g474_exti_irq(&other);
    ASSERT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
    EXPECT_EQ(2U, EXTI->IMR1);
    EXPECT_TRUE(g_g474_model.irq_enabled[EXTI1_IRQn]);
    EXPECT_FALSE(g_g474_model.irq_enabled[EXTI0_IRQn]);
    EXPECT_EQ(1U, other.count);
    EXPECT_TRUE(other.initialized);
}
TEST_F(G474Exti, StopRefusedMaskKeepsBorrowButAlsoWithdrawsItsVector) {
    Start();
    const nx_irq_wake_t wake{nullptr, [](void*) { return NX_SUCCESS; }, false};
    ASSERT_EQ(NX_SUCCESS, nx_exti_port_attach_wake(&face, &wake, 0U));
    g_g474_model.ignore_write = &EXTI->IMR1;
    EXPECT_EQ(NX_ERROR_IO, nx_exti_port_stop(&face));
    EXPECT_TRUE(state.retained);
    EXPECT_EQ(&wake, state.wake);
    EXPECT_FALSE(g_g474_model.irq_enabled[EXTI0_IRQn]);
    g_g474_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
    EXPECT_FALSE(state.retained);
    EXPECT_EQ(nullptr, state.wake);
}
TEST_F(G474Exti, StopRefusedPendingOrVectorRequiresObservableRetry) {
    Start();
    EXTI->PR1 = 1U;
    s_refuse_pending = true;
    EXPECT_EQ(NX_ERROR_IO, nx_exti_port_stop(&face));
    EXPECT_TRUE(state.retained);
    s_refuse_pending = false;
    g_g474_model.irq_enabled[EXTI0_IRQn] = true;
    g_g474_model.hold_irq_off = true;
    EXPECT_EQ(NX_ERROR_IO, nx_exti_port_stop(&face));
    EXPECT_TRUE(state.retained);
    g_g474_model.hold_irq_off = false;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
}
TEST_F(G474Exti, StopRestoresOnlyOwnedMuxAndRetainsFailedRestore) {
    SYSCFG->EXTICR[0] = 0x1234U;
    Start();
    SYSCFG->EXTICR[0] = 0x5670U;
    g_g474_model.ignore_write = &SYSCFG->EXTICR[0];
    EXPECT_EQ(NX_ERROR_IO, nx_exti_port_stop(&face));
    EXPECT_TRUE(state.retained);
    g_g474_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
    EXPECT_EQ(0x5674U, SYSCFG->EXTICR[0]);
}
TEST_F(G474Exti, WakeSeesLatchedFactAndCapturedPublisherCannotBeReclaimed) {
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
TEST_F(G474Exti, KernelWakeRequiresActualPriorityAndSelectedPolicy) {
    Start();
    const nx_irq_wake_t wake{nullptr, [](void*) { return NX_SUCCESS; }, true};
    EXPECT_EQ(NX_ERROR_PERMISSION, nx_exti_port_attach_wake(&face, &wake, 0U));
    state.irq_policy = {NX_IRQ_KERNEL_BASEPRI, 102U, 4U, 5U};
    g_g474_model.priority[EXTI0_IRQn] = 4U;
    EXPECT_EQ(NX_ERROR_PERMISSION, nx_exti_port_attach_wake(&face, &wake, 5U));
    g_g474_model.priority[EXTI0_IRQn] = 5U;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_attach_wake(&face, &wake, 5U));
}
class G474ExtiFactory : public testing::Test {
  protected:
    void SetUp() override {
        nx_g474_model_reset();
        std::memset(EXTI, 0, sizeof(*EXTI));
        std::memset(SYSCFG, 0, sizeof(*SYSCFG));
        s_refuse_pending = s_reassert_pending = false;
        s_refused_falling = 0U;
        std::memset(s_irq_enables, 0, sizeof(s_irq_enables));
        GPIOA->MODER = 15U;
    }
    void TearDown() override {
        g_g474_model.ignore_write = nullptr;
        g_g474_model.hold_irq_off = false;
        s_refuse_pending = false;
        s_refused_falling = 0U;
        EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    }
};
TEST_F(G474ExtiFactory, LookupHasNoSideEffectsAndUsesSharedReadonlyOperations) {
    const auto* first = nx_factory_exti(NX_EXTI_ID_EDGE0);
    const auto* second = nx_factory_exti(NX_EXTI_ID_EDGE1);
    ASSERT_NE(nullptr, first);
    ASSERT_NE(nullptr, second);
    EXPECT_EQ(first->ops, second->ops);
    EXPECT_NE(first->context, second->context);
    EXPECT_EQ(0U, g_g474_model.writes);
    EXPECT_EQ(0U, g_g474_model.reads);
}
TEST_F(G474ExtiFactory, StartDispatchStopClosesPinsMuxClocksAndBothVectors) {
    SYSCFG->EXTICR[0] = 0x34U;
    const auto started = nx_platform_start();
    ASSERT_EQ(NX_SUCCESS, started.primary);
    EXTI->PR1 = 3U;
    EXTI0_IRQHandler();
    EXTI1_IRQHandler();
    nx_exti_event_t events[2]{};
    size_t count = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_read(nx_factory_exti(NX_EXTI_ID_EDGE0),
                                            events, 2U, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(15U, GPIOA->MODER & 15U);
    EXPECT_EQ(0x34U, SYSCFG->EXTICR[0]);
    EXPECT_EQ(0U, RCC->APB2ENR & RCC_APB2ENR_SYSCFGEN);
    EXPECT_EQ(0U, RCC->AHB2ENR & RCC_AHB2ENR_GPIOAEN);
    EXPECT_FALSE(g_g474_model.irq_enabled[EXTI0_IRQn]);
    EXPECT_FALSE(g_g474_model.irq_enabled[EXTI1_IRQn]);
}
TEST_F(G474ExtiFactory, ExistingSourceIsRejectedBeforeGlobalClockMutation) {
    for (auto* reg :
         {&EXTI->IMR1, &EXTI->EMR1, &EXTI->RTSR1, &EXTI->FTSR1, &EXTI->PR1}) {
        *reg = 2U;
        const auto result = nx_platform_start();
        EXPECT_EQ(NX_ERROR_BUSY, result.primary);
        EXPECT_EQ(0U, g_g474_model.writes);
        EXPECT_EQ(0U, result.remaining_effects);
        *reg = 0U;
    }
}
TEST_F(G474ExtiFactory, FailedSecondLineRollsBackFirstAndOriginalPinLeases) {
    s_refused_falling = 2U;
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(15U, GPIOA->MODER & 15U);
    EXPECT_EQ(0U, RCC->APB2ENR & RCC_APB2ENR_SYSCFGEN);
    EXPECT_EQ(1U, s_irq_enables[EXTI0_IRQn]);
    EXPECT_EQ(0U, s_irq_enables[EXTI1_IRQn]);
}
TEST_F(G474ExtiFactory, PreexistingSyscfgGateIsNotReleasedByPlatform) {
    RCC->APB2ENR = RCC_APB2ENR_SYSCFGEN;
    ASSERT_EQ(NX_SUCCESS, nx_platform_start().primary);
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(RCC_APB2ENR_SYSCFGEN, RCC->APB2ENR & RCC_APB2ENR_SYSCFGEN);
}
