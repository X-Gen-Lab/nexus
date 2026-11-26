/**
 * \file            system_test.cpp
 * \brief           Actual GD32 System startup, timebase and resource boundaries
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-14
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "gd32_reference.h"
#include "gd32f4xx_timer.h"
#include "nexus/arch/arch.h"
#include "nexus/core/time.h"
void __real_timer_interrupt_flag_clear(uint32_t timer, uint32_t interrupt);
void __wrap_timer_interrupt_flag_clear(uint32_t timer, uint32_t interrupt);
}
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
uint32_t masks;
uint32_t enabled[91];
uint32_t priority[91];
bool route_fault;
uint32_t route_value;
class Gd32System : public testing::Test {
  protected:
    void* mapping{MAP_FAILED};
    void SetUp() override {
        mapping = mmap(reinterpret_cast<void*>(0x40000000U), 0x30000U,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        ASSERT_NE(mapping, MAP_FAILED);
        std::memset(mapping, 0, 0x30000U);
        std::memset(enabled, 0, sizeof enabled);
        std::memset(priority, 0, sizeof priority);
        masks = 0U;
        route_fault = false;
        route_value = 0U;
        RCU_CTL = RCU_CTL_IRC16MEN | RCU_CTL_IRC16MSTB;
    }
    void TearDown() override {
        if (mapping != MAP_FAILED) {
            route_fault = false;
            USART_CTL0(USART0) = 0U;
            EXPECT_EQ(nx_gd32_resources_release(), NX_SUCCESS);
            EXPECT_EQ(nx_gd32_system_release(true), NX_SUCCESS);
            EXPECT_EQ(munmap(mapping, 0x30000U), 0);
        }
    }
};
} /* namespace */

TEST_F(Gd32System, ForeignOscillatorRejectsBeforeTimerEffects) {
    RCU_CTL |= RCU_CTL_PLLEN;
    EXPECT_EQ(nx_gd32_system_start().primary, NX_ERROR_BUSY);
    EXPECT_EQ(RCU_APB1EN, 0U);
    EXPECT_EQ(enabled[TIMER1_IRQn], 0U);
}

TEST_F(Gd32System, GatedForeignTimerModeIsNotResetOrAdopted) {
    TIMER_CTL0(TIMER1) = TIMER_CTL0_CEN;
    TIMER_PSC(TIMER1) = 123U;
    EXPECT_EQ(nx_gd32_system_start().primary, NX_ERROR_BUSY);
    EXPECT_EQ(TIMER_PSC(TIMER1), 123U);
    EXPECT_EQ(RCU_APB1EN & RCU_APB1EN_TIMER1EN, 0U);
}

TEST_F(Gd32System, PendingWrapAndAcknowledgedWrapHaveOneMonotonicValue) {
    ASSERT_EQ(nx_gd32_system_start().primary, NX_SUCCESS);
    ASSERT_TRUE(nx_gd32_system_ready());
    TIMER_CNT(TIMER1) = 100U;
    EXPECT_EQ(nx_time_now_us(), 100U);
    TIMER_INTF(TIMER1) |= TIMER_INTF_UPIF;
    TIMER_CNT(TIMER1) = 3U;
    EXPECT_EQ(nx_time_now_us(), (UINT64_C(1) << 32U) + 3U);
    TIMER1_IRQHandler();
    EXPECT_EQ(nx_time_now_us(), (UINT64_C(1) << 32U) + 3U);
    EXPECT_EQ(masks, 0U);
}

TEST_F(Gd32System, ResourceLeaseRestoresOnlySelectedPadsAndOwnedClocks) {
    ASSERT_EQ(nx_gd32_system_start().primary, NX_SUCCESS);
    GPIO_AFSEL1(GPIOA) = 0x456789abU;
    GPIO_CTL(GPIOA) = 0x12345678U;
    const uint32_t af = GPIO_AFSEL1(GPIOA);
    const uint32_t mode = GPIO_CTL(GPIOA);
    ASSERT_EQ(nx_gd32_resources_acquire(), NX_SUCCESS);
    EXPECT_EQ((GPIO_AFSEL1(GPIOA) >> 4U) & 0xffU, 0x77U);
    EXPECT_EQ(nx_gd32_system_release(true), NX_ERROR_BUSY);
    EXPECT_TRUE(nx_gd32_system_ready());
    EXPECT_EQ(nx_gd32_resources_release(), NX_SUCCESS);
    EXPECT_EQ(GPIO_AFSEL1(GPIOA), af);
    EXPECT_EQ(GPIO_CTL(GPIOA), mode);
    EXPECT_EQ(RCU_AHB1EN & RCU_AHB1EN_PAEN, 0U);
}

TEST_F(Gd32System, ForeignUartClockRefusesBeforeSharedPadChanges) {
    ASSERT_EQ(nx_gd32_system_start().primary, NX_SUCCESS);
    RCU_APB2EN |= RCU_APB2EN_USART0EN;
    EXPECT_EQ(nx_gd32_resources_acquire(), NX_ERROR_BUSY);
    EXPECT_EQ(RCU_AHB1EN, 0U);
    EXPECT_EQ(GPIO_AFSEL1(GPIOA), 0U);
}

TEST_F(Gd32System, LockedUartPadRejectsAfterGateWithoutRouteMutation) {
    ASSERT_EQ(nx_gd32_system_start().primary, NX_SUCCESS);
    GPIO_LOCK(GPIOA) = GPIO_LOCK_LKK | GPIO_PIN_9;
    EXPECT_EQ(nx_gd32_resources_acquire(), NX_ERROR_BUSY);
    EXPECT_EQ(GPIO_AFSEL1(GPIOA), 0U);
    EXPECT_EQ(GPIO_CTL(GPIOA), 0U);
    EXPECT_EQ(RCU_AHB1EN & RCU_AHB1EN_PAEN, 0U);
    EXPECT_EQ(RCU_APB2EN & RCU_APB2EN_USART0EN, 0U);
}

TEST_F(Gd32System, RouteReadbackFailuresKeepReceiptAndClocksUntilRetry) {
    ASSERT_EQ(nx_gd32_system_start().primary, NX_SUCCESS);
    route_fault = true;
    route_value = 0U;
    EXPECT_EQ(nx_gd32_resources_acquire(), NX_ERROR_IO);
    EXPECT_NE(RCU_AHB1EN & RCU_AHB1EN_PAEN, 0U);
    EXPECT_EQ(nx_gd32_system_release(true), NX_ERROR_BUSY);
    route_value = 0xff0U;
    EXPECT_EQ(nx_gd32_resources_release(), NX_ERROR_IO);
    EXPECT_NE(RCU_AHB1EN & RCU_AHB1EN_PAEN, 0U);
    EXPECT_EQ(nx_gd32_system_release(true), NX_ERROR_BUSY);
    route_fault = false;
    EXPECT_EQ(nx_gd32_resources_release(), NX_SUCCESS);
    EXPECT_EQ(GPIO_AFSEL1(GPIOA) & 0xff0U, 0U);
    EXPECT_EQ(RCU_AHB1EN & RCU_AHB1EN_PAEN, 0U);
}

extern "C" uint32_t nx_gd32_system_irq_enabled(IRQn_Type irq) {
    return enabled[static_cast<unsigned>(irq)];
}
extern "C" uint32_t nx_gd32_system_irq_pending(IRQn_Type) {
    return 0U;
}
extern "C" uint32_t nx_gd32_system_irq_active(IRQn_Type) {
    return 0U;
}
extern "C" void nx_gd32_system_irq_priority(IRQn_Type irq, uint32_t value) {
    priority[static_cast<unsigned>(irq)] = value;
}
extern "C" void nx_gd32_system_irq_enable(IRQn_Type irq) {
    enabled[static_cast<unsigned>(irq)] = 1U;
}
extern "C" void nx_gd32_system_irq_disable(IRQn_Type irq) {
    enabled[static_cast<unsigned>(irq)] = 0U;
}
extern "C" void nx_gd32_system_irq_clear(IRQn_Type) {
}
extern "C" bool nx_arch_is_privileged(void) {
    return true;
}
extern "C" bool nx_arch_in_isr(void) {
    return false;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return masks != 0U;
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t state{masks};
    masks = 1U;
    return state;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t state) {
    masks = state.value;
}
extern "C" void nx_arch_dsb(void) {
    if (route_fault)
        GPIO_AFSEL1(GPIOA) = (GPIO_AFSEL1(GPIOA) & ~0xff0U) | route_value;
}
extern "C" void nx_arch_isb(void) {
}
extern "C" void __wrap_timer_interrupt_flag_clear(uint32_t timer,
                                                  uint32_t interrupt) {
    const uint32_t before = TIMER_INTF(timer);
    __real_timer_interrupt_flag_clear(timer, interrupt);
    TIMER_INTF(timer) &= before;
}
