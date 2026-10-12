/**
 * \file            stm32_exti_test.cpp
 * \brief           F407 EXTI retained cleanup and shared-vector isolation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32_exti_model.h"
extern "C" {
#include "stm32f407_provider.h"
nx_time_us_t nx_time_now_us(void) {
    return 0U;
}
}
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
extern "C" {
nx_stm32_exti_nvic_model_t g_stm32_exti_nvic;
}
static EXTI_TypeDef* s_registers;
static volatile uint32_t* s_ignored_write;
static uint32_t s_refused_mask;
static uint32_t s_refused_pending;
static uint32_t s_refused_trigger;
static bool s_refused_irq_disable;
static bool s_refused_irq_pending;
static unsigned s_clear_calls;
static unsigned s_disable_calls;
static unsigned s_irq_clear_calls;
uint32_t nx_stm32_exti_model_read(volatile uint32_t* reg) {
    return *reg;
}
void nx_stm32_exti_model_write(volatile uint32_t* reg, uint32_t value) {
    if (reg != s_ignored_write)
        *reg = value;
}
void nx_stm32_exti_model_clear(volatile uint32_t* reg, uint32_t mask) {
    ++s_clear_calls;
    *reg |= mask & s_refused_pending;
    *reg &= ~(mask & ~s_refused_pending);
    s_registers->IMR |= mask & s_refused_mask;
    s_registers->RTSR |= mask & s_refused_trigger;
}
void nx_stm32_exti_model_disable(int irq) {
    ++s_disable_calls;
    if (!s_refused_irq_disable)
        g_stm32_exti_nvic.ISER[static_cast<unsigned>(irq) / 32U] &=
            ~(1U << (static_cast<unsigned>(irq) % 32U));
}
void nx_stm32_exti_model_clear_irq(int irq) {
    ++s_irq_clear_calls;
    if (!s_refused_irq_pending)
        g_stm32_exti_nvic.ISPR[static_cast<unsigned>(irq) / 32U] &=
            ~(1U << (static_cast<unsigned>(irq) % 32U));
}
class ExtiWakeBoundary {
  public:
    MOCK_METHOD(nx_result_t, Signal, ());
};
class Stm32Exti : public testing::Test {
  protected:
    EXTI_TypeDef registers{};
    GPIO_TypeDef gpio{};
    SYSCFG_TypeDef mux{};
    nx_exti_event_t events[2]{};
    nx_exti_event_t other_events[2]{};
    nx_stm32_exti_state_t first{};
    nx_stm32_exti_state_t second{};
    const nx_exti_port_t face{&nx_stm32_exti_ops, &first};
    const nx_exti_port_t other_face{&nx_stm32_exti_ops, &second};
    unsigned irq = static_cast<unsigned>(EXTI9_5_IRQn);
    unsigned word = irq / 32U;
    uint32_t irq_bit = 1U << (irq % 32U);
    void SetUp() override {
        std::memset(&g_stm32_exti_nvic, 0, sizeof(g_stm32_exti_nvic));
        s_registers = &registers;
        s_ignored_write = nullptr;
        s_refused_mask = s_refused_pending = s_refused_trigger = 0U;
        s_refused_irq_disable = s_refused_irq_pending = false;
        s_clear_calls = s_disable_calls = s_irq_clear_calls = 0U;
        first.registers = second.registers = &registers;
        first.gpio = second.gpio = &gpio;
        first.storage = events;
        second.storage = other_events;
        first.capacity = second.capacity = 2U;
        first.line = 5U;
        second.line = 6U;
        first.edge = second.edge = NX_EXTI_BOTH;
    }
    void Start() {
        ASSERT_EQ(NX_SUCCESS, nx_stm32_exti_initialize(&first, &mux, 1U));
        g_stm32_exti_nvic.ISER[word] |= irq_bit;
    }
};
TEST_F(Stm32Exti, NormalStopObservesSourceAndLastVectorBeforeReleasingWake) {
    Start();
    testing::StrictMock<ExtiWakeBoundary> notification;
    const nx_irq_wake_t wake{
        &notification,
        [](void* p) { return static_cast<ExtiWakeBoundary*>(p)->Signal(); },
        false};
    ASSERT_EQ(NX_SUCCESS, nx_exti_port_attach_wake(&face, &wake, 5U));
    registers.PR = 1U << 5U;
    g_stm32_exti_nvic.ISPR[word] |= irq_bit;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
    EXPECT_FALSE(first.initialized);
    EXPECT_EQ(nullptr, first.wake);
    EXPECT_EQ(0U,
              (registers.IMR | registers.RTSR | registers.FTSR | registers.PR) &
                  (1U << 5U));
    EXPECT_EQ(0U,
              (g_stm32_exti_nvic.ISER[word] | g_stm32_exti_nvic.ISPR[word]) &
                  irq_bit);
}
TEST_F(Stm32Exti, RefusedLineMaskRetainsStorageWakeAndRetryResponsibility) {
    Start();
    const nx_irq_wake_t wake{nullptr, [](void*) { return NX_SUCCESS; }, false};
    ASSERT_EQ(NX_SUCCESS, nx_exti_port_attach_wake(&face, &wake, 5U));
    s_refused_mask = 1U << 5U;
    EXPECT_EQ(NX_ERROR_IO, nx_exti_port_stop(&face));
    EXPECT_TRUE(first.initialized);
    EXPECT_EQ(&wake, first.wake);
    EXPECT_EQ(events, first.storage);
    EXPECT_EQ(0U, s_disable_calls);
    s_refused_mask = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
}
TEST_F(Stm32Exti, RefusedLinePendingClearRetainsBorrowUntilObservedRetry) {
    Start();
    s_refused_pending = 1U << 5U;
    EXPECT_EQ(NX_ERROR_IO, nx_exti_port_stop(&face));
    EXPECT_TRUE(first.initialized);
    EXPECT_EQ(0U, s_disable_calls);
    s_refused_pending = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
}
TEST_F(Stm32Exti, RefusedTriggerWithdrawalKeepsInitializedCleanupState) {
    Start();
    s_refused_trigger = 1U << 5U;
    EXPECT_EQ(NX_ERROR_IO, nx_exti_port_stop(&face));
    EXPECT_TRUE(first.initialized);
    s_refused_trigger = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
}
TEST_F(Stm32Exti, RefusedNvicDisableDoesNotDropOwnedState) {
    Start();
    s_refused_irq_disable = true;
    EXPECT_EQ(NX_ERROR_IO, nx_exti_port_stop(&face));
    EXPECT_TRUE(first.initialized);
    s_refused_irq_disable = false;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
}
TEST_F(Stm32Exti, RefusedNvicPendingClearDoesNotReleaseStorage) {
    Start();
    g_stm32_exti_nvic.ISPR[word] |= irq_bit;
    s_refused_irq_pending = true;
    EXPECT_EQ(NX_ERROR_IO, nx_exti_port_stop(&face));
    EXPECT_TRUE(first.initialized);
    s_refused_irq_pending = false;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
}
TEST_F(Stm32Exti, ActiveVectorRejectsStopBeforeSourceEffects) {
    Start();
    g_stm32_exti_nvic.IABR[word] |= irq_bit;
    unsigned clears = s_clear_calls;
    EXPECT_EQ(NX_ERROR_BUSY, nx_exti_port_stop(&face));
    EXPECT_TRUE(first.initialized);
    EXPECT_EQ(1U << 5U, registers.IMR);
    EXPECT_EQ(clears, s_clear_calls);
}
TEST_F(Stm32Exti, SharedPeerSourceAndPendingArePreserved) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_stm32_exti_initialize(&second, &mux, 1U));
    registers.PR = (1U << 5U) | (1U << 6U);
    g_stm32_exti_nvic.ISPR[word] |= irq_bit;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
    EXPECT_EQ(1U << 6U, registers.IMR);
    EXPECT_EQ(1U << 6U, registers.PR);
    EXPECT_EQ(irq_bit, g_stm32_exti_nvic.ISER[word] & irq_bit);
    EXPECT_EQ(irq_bit, g_stm32_exti_nvic.ISPR[word] & irq_bit);
    EXPECT_EQ(0U, s_disable_calls);
    EXPECT_EQ(0U, s_irq_clear_calls);
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&other_face));
    EXPECT_EQ(0U, g_stm32_exti_nvic.ISER[word] & irq_bit);
}
TEST_F(Stm32Exti, FailedColdAdmissionStopHasNoForeignHardwareEffects) {
    registers.IMR = 1U << 5U;
    g_stm32_exti_nvic.ISER[word] = irq_bit;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_exti_initialize(&first, &mux, 1U));
    EXPECT_FALSE(first.initialized);
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
    EXPECT_EQ(1U << 5U, registers.IMR);
    EXPECT_EQ(irq_bit, g_stm32_exti_nvic.ISER[word]);
    EXPECT_EQ(0U, s_clear_calls);
    EXPECT_EQ(0U, s_disable_calls);
}
TEST_F(Stm32Exti, TriggerReadbackFailureRetainsPartialOwnerForStop) {
    s_ignored_write = &registers.RTSR;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_exti_initialize(&first, &mux, 1U));
    EXPECT_TRUE(first.initialized);
    EXPECT_EQ(events, first.storage);
    s_ignored_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
}
TEST_F(Stm32Exti, MuxReadbackFailureRetainsPartialOwnerWithoutReadySuccess) {
    s_ignored_write = &mux.EXTICR[1];
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_exti_initialize(&first, &mux, 1U));
    EXPECT_TRUE(first.initialized);
    EXPECT_EQ(0U, registers.IMR);
    s_ignored_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
}
TEST_F(Stm32Exti, InitPendingAcknowledgementFailureRetainsCleanupState) {
    s_refused_pending = 1U << 5U;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_exti_initialize(&first, &mux, 1U));
    EXPECT_TRUE(first.initialized);
    s_refused_pending = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
}
TEST_F(Stm32Exti, EventRingAndLossStayInLineOrderWithSharedDispatch) {
    Start();
    nx_stm32_exti_state_t* const ports[]{&first, &second};
    for (unsigned index = 0U; index < 3U; ++index) {
        registers.PR |= 1U << 5U;
        gpio.IDR = 1U << 5U;
        nx_stm32_exti_dispatch(ports, 2U, 0x3e0U);
    }
    nx_exti_event_t output[3]{};
    size_t count = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_exti_port_read(&face, output, 3U, &count));
    ASSERT_EQ(3U, count);
    EXPECT_EQ(NX_EXTI_EVENT_COALESCED, output[0].flags);
    EXPECT_EQ(NX_EXTI_EVENT_COALESCED, output[1].flags);
    EXPECT_EQ(NX_EXTI_EVENT_LOSS, output[2].flags);
    EXPECT_EQ(0U, second.count);
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
}
