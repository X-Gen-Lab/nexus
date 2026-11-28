/**
 * \file            watchdog_test.cpp
 * \brief           F103 IWDG ownership, timeout and silicon ordering
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/watchdog/instance.h"
#include "nexus/arch/arch.h"
#include "nexus/core/time.h"
#include "stm32f1xx_ll_iwdg.h"
#include <gtest/gtest.h>

RCC_TypeDef f103_watchdog_rcc{};
FLASH_TypeDef f103_watchdog_flash{};
IWDG_TypeDef f103_watchdog_iwdg{};

static struct {
    nx_time_us_t now;
    nx_time_us_t ready_at;
    nx_time_us_t apply_at;
    nx_time_us_t started_at;
    uint32_t starts;
    uint32_t writes;
    uint32_t feeds;
    bool live;
    bool privileged;
    bool interrupt;
    bool freeze_time;
    bool lose_lsi;
    bool refuse_lsi_release;
    bool hold_pvu;
    bool hold_rvu;
    bool ignore_prescaler;
    bool unlocked;
} bus;

extern "C" void f103_watchdog_write(volatile uint32_t* reg, uint32_t value) {
    ++bus.writes;
    if (reg == &RCC->CSR) {
        EXPECT_EQ(0U, value & RCC_CSR_RMVF);
        if (bus.refuse_lsi_release && (value & RCC_CSR_LSION) == 0U) {
            return;
        }
        if ((value & RCC_CSR_LSION) != 0U && (RCC->CSR & RCC_CSR_LSION) == 0U) {
            bus.ready_at = bus.now + 20U;
        }
    } else if (reg == &IWDG->KR) {
        if (value == LL_IWDG_KEY_ENABLE) {
            EXPECT_TRUE((RCC->CSR & RCC_CSR_LSIRDY) != 0U);
            ++bus.starts;
            bus.started_at = bus.now;
        } else if (value == LL_IWDG_KEY_RELOAD) {
            ++bus.feeds;
        }
        bus.unlocked = value == LL_IWDG_KEY_WR_ACCESS_ENABLE;
        return;
    } else if (reg == &IWDG->PR || reg == &IWDG->RLR) {
        EXPECT_TRUE(bus.unlocked);
        EXPECT_EQ(1U, bus.starts);
        const uint32_t pending = reg == &IWDG->PR ? IWDG_SR_PVU : IWDG_SR_RVU;
        EXPECT_EQ(0U, IWDG->SR & pending);
        IWDG->SR |= pending;
        bus.apply_at = bus.now + 55U;
        if (reg == &IWDG->PR && bus.ignore_prescaler) {
            return;
        }
    }
    *reg = value;
}

extern "C" nx_time_us_t nx_time_now_us(void) {
    if (!bus.freeze_time) {
        ++bus.now;
    }
    if (!bus.lose_lsi && (RCC->CSR & RCC_CSR_LSION) != 0U &&
        bus.now >= bus.ready_at) {
        RCC->CSR |= RCC_CSR_LSIRDY;
    }
    if (bus.now >= bus.apply_at) {
        const uint32_t clear = (bus.hold_pvu ? 0U : IWDG_SR_PVU) |
                               (bus.hold_rvu ? 0U : IWDG_SR_RVU);
        IWDG->SR &= ~clear;
    }
    return bus.now;
}

extern "C" bool nx_stm32f1_system_live(void) {
    return bus.live;
}

extern "C" bool nx_arch_is_privileged(void) {
    return bus.privileged;
}

extern "C" bool nx_arch_in_isr(void) {
    return bus.interrupt;
}

extern "C" bool nx_arch_irq_is_masked(void) {
    return false;
}

extern "C" void nx_arch_dsb(void) {
}

class F103Watchdog : public testing::Test {
  protected:
    nx_stm32f1_watchdog_state_t state{};
    nx_watchdog_state_t status{};
    const nx_watchdog_port_t face{&nx_stm32f1_watchdog_ops, &state};

    void SetUp() override {
        bus = {};
        bus.live = true;
        bus.privileged = true;
        RCC->CSR = 0U;
        RCC->CFGR = 0U;
        RCC->CR = RCC_CR_HSION | RCC_CR_HSIRDY;
        FLASH->OBR = FLASH_OBR_IWDG_SW;
        IWDG->SR = 0U;
        IWDG->PR = 0U;
        IWDG->RLR = 0x0fffU;
        ASSERT_EQ(NX_SUCCESS, nx_stm32f1_watchdog_prepare(&state));
        ASSERT_EQ(0U, bus.writes);
    }
};

TEST_F(F103Watchdog, RejectsUnsupportedDebugConfigurationAndInvalidTiming) {
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_watchdog_port_enable(&face, 1000U, true, &status));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_watchdog_port_enable(&face, 99U, false, &status));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_watchdog_port_enable(&face, 26214401U, false, &status));
    EXPECT_EQ(NX_ERROR_STATE, nx_watchdog_port_feed(&face));
    EXPECT_EQ(0U, bus.writes);
    EXPECT_FALSE(status.enabled);
}

TEST_F(F103Watchdog, RequiresLiveReviewedClockAndQuietShadowBeforeWrites) {
    bus.interrupt = true;
    EXPECT_EQ(NX_ERROR_CONTEXT,
              nx_watchdog_port_enable(&face, 1000U, false, &status));
    bus.interrupt = false;
    bus.live = false;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_watchdog_port_enable(&face, 1000U, false, &status));
    bus.live = true;
    RCC->CFGR = RCC_CFGR_PPRE1_DIV16;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_watchdog_port_enable(&face, 1000U, false, &status));
    RCC->CFGR = 0U;
    IWDG->SR = IWDG_SR_PVU;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_watchdog_port_enable(&face, 1000U, false, &status));
    EXPECT_EQ(0U, bus.writes);
}

TEST_F(F103Watchdog, WaitsAfterLsirdyBeforeStartingAndWaitsForBothShadowFlags) {
    ASSERT_EQ(NX_SUCCESS,
              nx_watchdog_port_enable(&face, 409601U, false, &status));
    EXPECT_GE(bus.started_at - bus.ready_at, 103U);
    EXPECT_GE(bus.now, bus.apply_at);
    EXPECT_EQ(1U, IWDG->PR);
    EXPECT_EQ(2048U, IWDG->RLR);
    EXPECT_EQ(1U, bus.starts);
    EXPECT_EQ(1U, bus.feeds);
    EXPECT_TRUE(status.enabled);
    EXPECT_TRUE(status.irreversible);
    EXPECT_FALSE(status.debug_freeze);
    EXPECT_EQ(0U, status.minimum_timeout_us);
    EXPECT_EQ(0U, status.maximum_timeout_us);
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_watchdog_port_state(&face, &status));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32f1_watchdog_close(&state));
    bus.interrupt = true;
    EXPECT_EQ(NX_SUCCESS, nx_watchdog_port_feed(&face));
    EXPECT_EQ(2U, bus.feeds);
}

TEST_F(F103Watchdog, RetainsFailedLsiReleaseUntilCloseRetriesWithoutStarting) {
    bus.lose_lsi = true;
    bus.refuse_lsi_release = true;
    EXPECT_EQ(NX_ERROR_IO,
              nx_watchdog_port_enable(&face, 1000U, false, &status));
    EXPECT_FALSE(status.enabled);
    EXPECT_EQ(0U, bus.starts);
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_watchdog_port_enable(&face, 1000U, false, &status));
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f1_watchdog_close(&state));
    bus.refuse_lsi_release = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_watchdog_close(&state));
    EXPECT_EQ(0U, RCC->CSR & RCC_CSR_LSION);
}

TEST_F(F103Watchdog, BoundedWaitReturnsIfTimebaseStops) {
    bus.freeze_time = true;
    ASSERT_EQ(NX_ERROR_IO,
              nx_watchdog_port_enable(&face, 1000U, false, &status));
    EXPECT_EQ(0U, bus.starts);
    EXPECT_FALSE(status.enabled);
    EXPECT_EQ(0U, RCC->CSR & RCC_CSR_LSION);
}

TEST_F(F103Watchdog, NeverReplaysIrreversibleStartAfterPendingShadowFailure) {
    bus.hold_rvu = true;
    ASSERT_EQ(NX_ERROR_IO,
              nx_watchdog_port_enable(&face, 26214400U, false, &status));
    EXPECT_TRUE(status.enabled);
    EXPECT_TRUE(status.irreversible);
    EXPECT_EQ(0U, status.maximum_timeout_us);
    EXPECT_EQ(1U, bus.starts);
    EXPECT_EQ(0U, bus.feeds);
    EXPECT_FALSE(bus.unlocked);
    EXPECT_EQ(NX_ERROR_STATE,
              nx_watchdog_port_enable(&face, 1000U, false, &status));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32f1_watchdog_close(&state));
    EXPECT_EQ(1U, bus.starts);
    ASSERT_EQ(NX_SUCCESS, nx_watchdog_port_feed(&face));
    EXPECT_EQ(1U, bus.feeds);
}

TEST_F(F103Watchdog, AlreadyStartedOptionCanBeFedWithoutChangingItsSetup) {
    FLASH->OBR = 0U;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_watchdog_port_state(&face, &status));
    EXPECT_TRUE(status.enabled);
    EXPECT_TRUE(status.irreversible);
    EXPECT_EQ(NX_ERROR_STATE,
              nx_watchdog_port_enable(&face, 1000U, false, &status));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32f1_watchdog_close(&state));
    EXPECT_EQ(0U, bus.writes);
    EXPECT_EQ(NX_SUCCESS, nx_watchdog_port_feed(&face));
    EXPECT_EQ(0U, bus.starts);
    EXPECT_EQ(1U, bus.feeds);
}

TEST_F(F103Watchdog, LeavesAnotherOwnersLsiRequestUntouchedOnFailure) {
    RCC->CSR = RCC_CSR_LSION | RCC_CSR_PINRSTF;
    bus.lose_lsi = true;
    ASSERT_EQ(NX_ERROR_IO,
              nx_watchdog_port_enable(&face, 1000U, false, &status));
    EXPECT_FALSE(status.enabled);
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_watchdog_close(&state));
    EXPECT_EQ(0U, bus.writes);
    EXPECT_EQ(static_cast<uint32_t>(RCC_CSR_LSION | RCC_CSR_PINRSTF), RCC->CSR);
}

TEST_F(F103Watchdog, SettledShadowMismatchRetainsStartWithoutFeeding) {
    bus.ignore_prescaler = true;
    ASSERT_EQ(NX_ERROR_IO,
              nx_watchdog_port_enable(&face, 26214400U, false, &status));
    EXPECT_EQ(0U, IWDG->SR);
    EXPECT_TRUE(status.enabled);
    EXPECT_EQ(1U, bus.starts);
    EXPECT_EQ(0U, bus.feeds);
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32f1_watchdog_close(&state));
}
