/**
 * \file            stm32_window_watchdog_test.cpp
 * \brief           WWDG activation, feed windows and cold rollback
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-11-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/core/platform_lifecycle.h"
#include "nexus/soc/stm32f407_wwdg_ewi.h"
#include "stm32f407_window_watchdog.h"
extern "C" nx_result_t nx_stm32f407_wwdg_ewi_arm(nx_stm32f407_wwdg_ewi_t*,
                                                 const nx_watchdog_port_t*,
                                                 uint8_t) __attribute__((weak));
extern "C" nx_result_t nx_stm32f407_wwdg_ewi_irq(nx_stm32f407_wwdg_ewi_t*)
    __attribute__((weak));
extern "C" nx_result_t
nx_stm32f407_wwdg_ewi_read(const nx_stm32f407_wwdg_ewi_t*, uint32_t*, bool*)
    __attribute__((weak));
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <vector>

extern "C" {
DBGMCU_TypeDef nx_wwdg_model_debug;
NVIC_Type nx_wwdg_model_nvic;
}

namespace {
uint32_t mask;
bool privileged;
bool isr;
volatile uint32_t* refused_write;
volatile uint32_t* corrupt_write;
uint32_t corrupt_mask;
std::vector<volatile uint32_t*> writes;
testing::MockFunction<void(uint32_t)>* activation;
nx_stm32_window_watchdog_state_t* platform_owner;
unsigned system_releases;

class WindowWatchdog : public testing::Test {
  protected:
    void* registers = nullptr;
    alignas(nx_watchdog_state_t) nx_stm32_window_watchdog_state_t owner = {};
    const nx_watchdog_port_t face = {&nx_stm32_window_watchdog_ops, &owner};
    nx_watchdog_state_t observed = {};

    void SetUp() override {
        registers =
            mmap(reinterpret_cast<void*>(0x40000000U), 0x80000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(registers, reinterpret_cast<void*>(0x40000000U));
        std::memset(&nx_wwdg_model_debug, 0, sizeof(nx_wwdg_model_debug));
        std::memset(&nx_wwdg_model_nvic, 0, sizeof(nx_wwdg_model_nvic));
        mask = 0U;
        privileged = true;
        isr = false;
        refused_write = nullptr;
        corrupt_write = nullptr;
        corrupt_mask = 0U;
        activation = nullptr;
        writes.clear();
        RCC->CR = RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY;
        RCC->PLLCFGR = RCC_PLLCFGR_PLLSRC_HSE | 8U | (336U << 6U) | (7U << 24U);
        RCC->CFGR = RCC_CFGR_SW_PLL | RCC_CFGR_SWS_PLL | RCC_CFGR_PPRE1_DIV4 |
                    RCC_CFGR_PPRE2_DIV2;
        RCC->APB1LPENR = RCC_APB1LPENR_TIM2LPEN;
        DBGMCU->APB1FZ = DBGMCU_APB1_FZ_DBG_TIM2_STOP;
        WWDG->CFR = 0x7FU;
        WWDG->CR = 0x7FU;
    }
    void TearDown() override {
        activation = nullptr;
        refused_write = nullptr;
        corrupt_write = nullptr;
        if (registers != nullptr && registers != MAP_FAILED) {
            EXPECT_EQ(0, munmap(registers, 0x80000U));
        }
    }
    void Prepare(uint8_t ticks = 8U) {
        ASSERT_EQ(NX_SUCCESS, nx_stm32_window_watchdog_prepare(&owner, ticks));
    }
};
} /* namespace */

TEST_F(WindowWatchdog, EarlyWarningRetainsSourceAndNeverReloadsWatchdog) {
    ASSERT_NE(nullptr, &nx_stm32f407_wwdg_ewi_arm);
    ASSERT_NE(nullptr, &nx_stm32f407_wwdg_ewi_irq);
    ASSERT_NE(nullptr, &nx_stm32f407_wwdg_ewi_read);
    nx_stm32f407_wwdg_ewi_t warning{};
    Prepare();
    ASSERT_EQ(NX_SUCCESS,
              face.ops->enable(face.context, 2000U, false, &observed));
    ASSERT_EQ(NX_SUCCESS, nx_stm32f407_wwdg_ewi_arm(&warning, &face, 4U));
    EXPECT_TRUE(warning.retained);
    EXPECT_NE(0U, WWDG->CFR & WWDG_CFR_EWI);
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32_window_watchdog_stop(&owner));
    WWDG->CR = WWDG_CR_WDGA | 0x40U;
    WWDG->SR = WWDG_SR_EWIF;
    writes.clear();
    isr = true;
    EXPECT_EQ(NX_SUCCESS, nx_stm32f407_wwdg_ewi_irq(&warning));
    EXPECT_EQ(0U, WWDG->SR & WWDG_SR_EWIF);
    EXPECT_EQ(WWDG_CR_WDGA | 0x40U, WWDG->CR);
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32f407_wwdg_ewi_irq(&warning));
    isr = false;
    uint32_t count = 0U;
    bool overflow = true;
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32f407_wwdg_ewi_read(&warning, &count, &overflow));
    EXPECT_EQ(1U, count);
    EXPECT_FALSE(overflow);
    EXPECT_EQ(1U, writes.size());
}

TEST_F(WindowWatchdog, RefusedWarningAcknowledgeRetainsBorrowAndCount) {
    ASSERT_NE(nullptr, &nx_stm32f407_wwdg_ewi_arm);
    nx_stm32f407_wwdg_ewi_t warning{};
    Prepare();
    ASSERT_EQ(NX_SUCCESS,
              face.ops->enable(face.context, 2000U, false, &observed));
    ASSERT_EQ(NX_SUCCESS, nx_stm32f407_wwdg_ewi_arm(&warning, &face, 4U));
    WWDG->SR = WWDG_SR_EWIF;
    refused_write = &WWDG->SR;
    isr = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f407_wwdg_ewi_irq(&warning));
    EXPECT_EQ(0U, warning.occurrences);
    EXPECT_TRUE(warning.retained);
    isr = false;
    refused_write = nullptr;
    WWDG->CR = WWDG_CR_WDGA | (owner.reload - owner.window_ticks);
    EXPECT_EQ(NX_SUCCESS, face.ops->feed(face.context));
    EXPECT_NE(0U, WWDG->CFR & WWDG_CFR_EWI);
}

TEST_F(WindowWatchdog, WarningSnapshotCannotOverwriteRetainedWatchdogOwner) {
    nx_stm32f407_wwdg_ewi_t warning{};
    Prepare();
    ASSERT_EQ(NX_SUCCESS,
              face.ops->enable(face.context, 2000U, false, &observed));
    ASSERT_EQ(NX_SUCCESS, nx_stm32f407_wwdg_ewi_arm(&warning, &face, 4U));
    auto saved = owner;
    bool overflow = false;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32f407_wwdg_ewi_read(
                  &warning, reinterpret_cast<uint32_t*>(&owner), &overflow));
    EXPECT_EQ(saved.effects, owner.effects);
    owner = saved;
}

/** \brief Observe actual writes, with a selectable refused register. */
extern "C" void nx_stm32_window_watchdog_model_write(volatile uint32_t* address,
                                                     uint32_t value) {
    writes.push_back(address);
    if (address == &WWDG->CR && activation != nullptr)
        activation->Call(value);
    if (address != refused_write) {
        *address = value ^ (address == corrupt_write ? corrupt_mask : 0U);
    }
}
/** \brief Save exactly the incoming CPU mask for the feed window. */
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t incoming = {mask};
    mask = 1U;
    return incoming;
}
/** \brief Preserve the incoming mask even after failed feed admission. */
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t incoming) {
    mask = incoming.value;
}
/** \brief Observe an injected privilege domain. */
extern "C" bool nx_arch_is_privileged(void) {
    return privileged;
}
/** \brief Observe an injected exception context. */
extern "C" bool nx_arch_in_isr(void) {
    return isr;
}
/** \brief Observe an injected incoming IRQ mask. */
extern "C" bool nx_arch_irq_is_masked(void) {
    return mask != 0U;
}
/** \brief Host stores model ordering without physical APB timing claims. */
extern "C" void nx_arch_dsb(void) {
}

TEST_F(WindowWatchdog, PrepareNeverActivatesAndStopRestoresOwnedBits) {
    const uint32_t sleep = RCC->APB1LPENR;
    ASSERT_NO_FATAL_FAILURE(Prepare());
    EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_WWDGEN);
    EXPECT_NE(0U, RCC->APB1LPENR & RCC_APB1LPENR_WWDGLPEN);
    EXPECT_EQ(0U, WWDG->CR & WWDG_CR_WDGA);
    EXPECT_EQ(NX_SUCCESS, nx_watchdog_port_state(&face, &observed));
    EXPECT_FALSE(observed.enabled);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_window_watchdog_stop(&owner));
    EXPECT_EQ(0U, RCC->APB1ENR & RCC_APB1ENR_WWDGEN);
    EXPECT_EQ(sleep, RCC->APB1LPENR);
    EXPECT_EQ(DBGMCU_APB1_FZ_DBG_TIM2_STOP, DBGMCU->APB1FZ);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_window_watchdog_stop(&owner));
}

TEST_F(WindowWatchdog,
       AdmissionRejectsForeignClockResetAndVectorWithoutWrites) {
    for (unsigned case_id = 0U; case_id < 5U; ++case_id) {
        SCOPED_TRACE(case_id);
        RCC->APB1ENR = case_id == 0U ? RCC_APB1ENR_WWDGEN : 0U;
        RCC->APB1RSTR = case_id == 1U ? RCC_APB1RSTR_WWDGRST : 0U;
        NVIC->ISER[0] = case_id == 2U ? 1U : 0U;
        NVIC->ISPR[0] = case_id == 3U ? 1U : 0U;
        const_cast<volatile uint32_t&>(NVIC->IABR[0]) = case_id == 4U ? 1U : 0U;
        EXPECT_NE(NX_SUCCESS, nx_stm32_window_watchdog_admit(&owner, 8U));
        EXPECT_NE(NX_SUCCESS, nx_stm32_window_watchdog_prepare(&owner, 8U));
        EXPECT_TRUE(writes.empty());
        EXPECT_EQ(0U, owner.effects);
    }
}

TEST_F(WindowWatchdog, ReadOnlyAdmissionPrecedesTheSystemClockBoundary) {
    RCC->CR = RCC_CR_HSION;
    RCC->CFGR = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_window_watchdog_admit(&owner, 8U));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_stm32_window_watchdog_prepare(&owner, 8U));
    EXPECT_TRUE(writes.empty());
    EXPECT_EQ(0U, owner.effects);
}

TEST_F(WindowWatchdog, RealPlatformStopsBeforeSystemWithdrawalAfterActivation) {
    platform_owner = &owner;
    system_releases = 0U;
    RCC->CR = RCC_CR_HSION;
    RCC->CFGR = 0U;
    const size_t ordinals[] = {0U};
    const nx_platform_lifecycle_plan_t plan = {
        1U,
        ordinals,
        [] { return NX_SUCCESS; },
        [](size_t) {
            return nx_stm32_window_watchdog_admit(platform_owner, 8U);
        },
        [] {
            RCC->CR =
                RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY;
            RCC->CFGR = RCC_CFGR_SW_PLL | RCC_CFGR_SWS_PLL |
                        RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV2;
            return nx_platform_start_result_t{NX_SUCCESS, NX_SUCCESS, 0U};
        },
        [] { return NX_SUCCESS; },
        [] { return NX_SUCCESS; },
        [](bool) {
            ++system_releases;
            return NX_SUCCESS;
        },
        [](size_t) {
            return nx_lifecycle_acquisition_t{
                nx_stm32_window_watchdog_prepare(platform_owner, 8U), true};
        },
        [](size_t) { return nx_stm32_window_watchdog_stop(platform_owner); },
        false,
        nullptr,
        nullptr,
        nullptr};
    nx_platform_lifecycle_state_t lifecycle = {};
    ASSERT_EQ(NX_SUCCESS,
              nx_platform_lifecycle_start(&plan, &lifecycle, nullptr).primary);
    ASSERT_EQ(NX_SUCCESS,
              nx_watchdog_port_enable(&face, 10000U, false, &observed));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_platform_lifecycle_stop(&plan, &lifecycle, nullptr));
    EXPECT_EQ(0U, system_releases);
    EXPECT_TRUE(lifecycle.started);
    EXPECT_EQ(1U, lifecycle.initialized);
    EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_WWDGEN);
}

TEST_F(WindowWatchdog, ConfigureAndDebugReadbackPrecedeSingleActivationWrite) {
    ASSERT_NO_FATAL_FAILURE(Prepare());
    testing::StrictMock<testing::MockFunction<void(uint32_t)>> observer;
    activation = &observer;
    EXPECT_CALL(observer, Call(WWDG_CR_WDGA | 115U)).WillOnce([](uint32_t) {
        EXPECT_EQ((1U << 7U) | 108U, WWDG->CFR);
        EXPECT_NE(0U, RCC->APB1LPENR & RCC_APB1LPENR_WWDGLPEN);
        EXPECT_NE(0U, DBGMCU->APB1FZ & DBGMCU_APB1_FZ_DBG_WWDG_STOP);
    });
    EXPECT_EQ(NX_SUCCESS,
              nx_watchdog_port_enable(&face, 10000U, true, &observed));
    activation = nullptr;
    EXPECT_TRUE(observed.enabled);
    EXPECT_TRUE(observed.irreversible);
    EXPECT_TRUE(observed.debug_freeze);
    EXPECT_EQ(0U, observed.minimum_timeout_us);
    EXPECT_EQ(0U, observed.maximum_timeout_us);
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32_window_watchdog_stop(&owner));
    EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_WWDGEN);
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_watchdog_port_state(&face, &observed));
}

TEST_F(WindowWatchdog, ExactWindowBoundaryAndLateCounterNeverWriteReload) {
    ASSERT_NO_FATAL_FAILURE(Prepare());
    ASSERT_EQ(NX_SUCCESS,
              nx_watchdog_port_enable(&face, 10000U, false, &observed));
    for (uint32_t counter : {115U, 108U, 64U, 63U}) {
        WWDG->CR = WWDG_CR_WDGA | counter;
        writes.clear();
        mask = 1U;
        EXPECT_EQ(counter > 64U ? NX_ERROR_BUSY : NX_ERROR_STATE,
                  nx_watchdog_port_feed(&face));
        EXPECT_TRUE(writes.empty());
        EXPECT_EQ(1U, mask);
    }
    isr = true;
    for (uint32_t counter : {107U, 65U}) {
        WWDG->CR = WWDG_CR_WDGA | counter;
        writes.clear();
        mask = 0U;
        EXPECT_EQ(NX_SUCCESS, nx_watchdog_port_feed(&face));
        EXPECT_EQ(WWDG_CR_WDGA | 115U, WWDG->CR);
        EXPECT_EQ(0U, mask);
        ASSERT_EQ(1U, writes.size());
        EXPECT_EQ(&WWDG->CR, writes[0]);
    }
}

TEST_F(WindowWatchdog, AllDividerBoundariesRetainAnActualFeedWindow) {
    const uint32_t timeouts[] = {1000U,  6241U,  6242U,  12483U,
                                 12484U, 24966U, 24967U, 49932U};
    for (uint32_t timeout : timeouts) {
        SCOPED_TRACE(timeout);
        ASSERT_NO_FATAL_FAILURE(Prepare(1U));
        ASSERT_EQ(NX_SUCCESS,
                  nx_watchdog_port_enable(&face, timeout, false, &observed));
        const uint32_t divider =
            static_cast<uint32_t>((WWDG->CFR & WWDG_CFR_WDGTB) >> 7U);
        const uint32_t cycles = 4096U << divider;
        const uint32_t steps = (WWDG->CR & 0x7FU) - 63U;
        EXPECT_GE(steps * cycles, timeout * 42U);
        EXPECT_LT((steps - 1U) * cycles, timeout * 42U);
        EXPECT_GE(WWDG->CFR & 0x7FU, 66U);
        /* Each iteration models a distinct MCU reset, never a driver stop. */
        owner = {};
        RCC->APB1ENR = 0U;
        WWDG->CR = 0x7FU;
        WWDG->CFR = 0x7FU;
    }
}

TEST_F(WindowWatchdog, MaximumClosedWindowStillAllowsCounterSixtyFive) {
    ASSERT_NO_FATAL_FAILURE(Prepare(62U));
    ASSERT_EQ(NX_SUCCESS,
              nx_watchdog_port_enable(&face, 49932U, false, &observed));
    EXPECT_EQ(66U, WWDG->CFR & 0x7FU);
    WWDG->CR = WWDG_CR_WDGA | 65U;
    EXPECT_EQ(NX_SUCCESS, nx_watchdog_port_feed(&face));
}

TEST_F(WindowWatchdog, RefusedActivationCannotBeRetriedOrRolledBackAsDisabled) {
    ASSERT_NO_FATAL_FAILURE(Prepare());
    refused_write = &WWDG->CR;
    EXPECT_EQ(NX_ERROR_IO,
              nx_watchdog_port_enable(&face, 10000U, false, &observed));
    EXPECT_TRUE(observed.enabled);
    EXPECT_TRUE(observed.irreversible);
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32_window_watchdog_stop(&owner));
    EXPECT_EQ(NX_ERROR_STATE, nx_watchdog_port_feed(&face));
    EXPECT_EQ(NX_ERROR_STATE,
              nx_watchdog_port_enable(&face, 10000U, false, &observed));
    EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_WWDGEN);
}

TEST_F(WindowWatchdog,
       RefusedColdClockAndSleepRestoreKeepCleanupResponsibility) {
    refused_write = &RCC->APB1LPENR;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_window_watchdog_prepare(&owner, 8U));
    EXPECT_NE(0U, owner.effects);
    refused_write = &RCC->APB1ENR;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_window_watchdog_stop(&owner));
    EXPECT_NE(0U, owner.effects);
    refused_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_window_watchdog_stop(&owner));
    EXPECT_EQ(0U, owner.effects);
}

TEST_F(WindowWatchdog,
       EveryReversibleWriteFailureBlocksActivationAndKeepsReceipt) {
    volatile uint32_t* const targets[] = {&RCC->APB1ENR, &RCC->APB1LPENR,
                                          &WWDG->CFR, &DBGMCU->APB1FZ};
    const uint32_t bits[] = {RCC_APB1ENR_WWDGEN, RCC_APB1LPENR_WWDGLPEN, 1U,
                             DBGMCU_APB1_FZ_DBG_WWDG_STOP};
    for (size_t i = 0U; i < 4U; ++i) {
        SCOPED_TRACE(i);
        if (i >= 2U) {
            ASSERT_NO_FATAL_FAILURE(Prepare());
        }
        corrupt_write = targets[i];
        corrupt_mask = bits[i];
        if (i < 2U) {
            ASSERT_EQ(NX_ERROR_IO,
                      nx_stm32_window_watchdog_prepare(&owner, 8U));
        } else {
            ASSERT_EQ(NX_ERROR_IO,
                      nx_watchdog_port_enable(&face, 10000U, true, &observed));
        }
        EXPECT_EQ(0U, WWDG->CR & WWDG_CR_WDGA);
        ASSERT_EQ(NX_ERROR_IO, nx_stm32_window_watchdog_stop(&owner));
        EXPECT_NE(0U, owner.effects);
        const size_t before = writes.size();
        EXPECT_EQ(NX_ERROR_STATE,
                  nx_watchdog_port_enable(&face, 10000U, true, &observed));
        EXPECT_EQ(before, writes.size());
        corrupt_write = nullptr;
        EXPECT_EQ(NX_SUCCESS, nx_stm32_window_watchdog_stop(&owner));
        EXPECT_EQ(0U, owner.effects);
        EXPECT_EQ(0U, RCC->APB1ENR & RCC_APB1ENR_WWDGEN);
        EXPECT_EQ(0x7FU, WWDG->CFR);
    }
}

TEST_F(WindowWatchdog, ColdContextsAndOutputAliasesRejectWithoutEffects) {
    for (unsigned context = 0U; context < 3U; ++context) {
        privileged = context != 0U;
        isr = context == 1U;
        mask = context == 2U ? 1U : 0U;
        EXPECT_EQ(NX_ERROR_CONTEXT, nx_stm32_window_watchdog_admit(&owner, 8U));
        EXPECT_EQ(NX_ERROR_CONTEXT,
                  nx_stm32_window_watchdog_prepare(&owner, 8U));
        EXPECT_EQ(NX_ERROR_CONTEXT, nx_stm32_window_watchdog_stop(&owner));
        EXPECT_EQ(NX_ERROR_CONTEXT,
                  nx_watchdog_port_enable(&face, 10000U, false, &observed));
        if (context == 0U) {
            EXPECT_EQ(NX_ERROR_CONTEXT, nx_watchdog_port_feed(&face));
            EXPECT_EQ(NX_ERROR_CONTEXT,
                      nx_watchdog_port_state(&face, &observed));
        }
        EXPECT_TRUE(writes.empty());
    }
    privileged = true;
    isr = false;
    mask = 0U;
    const uint8_t invalid[] = {0U, 63U};
    for (uint8_t ticks : invalid) {
        EXPECT_EQ(NX_ERROR_INVALID,
                  nx_stm32_window_watchdog_admit(&owner, ticks));
        EXPECT_EQ(NX_ERROR_INVALID,
                  nx_stm32_window_watchdog_prepare(&owner, ticks));
    }
    EXPECT_TRUE(writes.empty());
    ASSERT_NO_FATAL_FAILURE(Prepare());
    writes.clear();
    const auto before = owner;
    nx_watchdog_state_t* const outputs[] = {
        reinterpret_cast<nx_watchdog_state_t*>(&owner),
        reinterpret_cast<nx_watchdog_state_t*>(WWDG_BASE),
        reinterpret_cast<nx_watchdog_state_t*>(RCC_BASE),
        reinterpret_cast<nx_watchdog_state_t*>(DBGMCU_BASE),
        reinterpret_cast<nx_watchdog_state_t*>(NVIC_BASE),
        nullptr};
    for (auto* output : outputs) {
        EXPECT_EQ(NX_ERROR_INVALID,
                  nx_watchdog_port_enable(&face, 10000U, false, output));
        EXPECT_EQ(NX_ERROR_INVALID, nx_watchdog_port_state(&face, output));
    }
    EXPECT_TRUE(writes.empty());
    EXPECT_EQ(0, std::memcmp(&before, &owner, sizeof(owner)));
}

TEST_F(WindowWatchdog, DepartingOrDisabledClockRejectsPrepareBeforeWrites) {
    const uint32_t cr = RCC->CR, cfgr = RCC->CFGR, pll = RCC->PLLCFGR;
    for (unsigned profile = 0U; profile < 5U; ++profile) {
        SCOPED_TRACE(profile);
        RCC->CR = cr;
        RCC->CFGR = cfgr;
        RCC->PLLCFGR = pll;
        if (profile == 0U)
            RCC->CFGR &= ~static_cast<uint32_t>(RCC_CFGR_SW);
        if (profile == 1U)
            RCC->CR &= ~static_cast<uint32_t>(RCC_CR_PLLON);
        if (profile == 2U)
            RCC->CR &= ~static_cast<uint32_t>(RCC_CR_HSEON);
        if (profile == 3U)
            RCC->CFGR ^= RCC_CFGR_PPRE1_0;
        if (profile == 4U)
            RCC->PLLCFGR ^= 1U;
        writes.clear();
        EXPECT_EQ(NX_SUCCESS, nx_stm32_window_watchdog_admit(&owner, 8U));
        nx_result_t result = nx_stm32_window_watchdog_prepare(&owner, 8U);
        EXPECT_EQ(NX_ERROR_UNSUPPORTED, result);
        EXPECT_TRUE(writes.empty());
        if (result == NX_SUCCESS) {
            EXPECT_EQ(NX_SUCCESS, nx_stm32_window_watchdog_stop(&owner));
        }
    }
}

TEST_F(WindowWatchdog, UnsupportedTimeoutsNeverClampOrActivate) {
    ASSERT_NO_FATAL_FAILURE(Prepare());
    writes.clear();
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_watchdog_port_enable(&face, 0U, false, &observed));
    for (uint32_t timeout : {1U, 200U, 49933U, UINT32_MAX}) {
        EXPECT_EQ(NX_ERROR_UNSUPPORTED,
                  nx_watchdog_port_enable(&face, timeout, false, &observed));
        EXPECT_FALSE(observed.enabled);
    }
    EXPECT_TRUE(writes.empty());
    EXPECT_EQ(0U, WWDG->CR & WWDG_CR_WDGA);
}

TEST_F(WindowWatchdog, ForeignActivationAndEwiAreNeverAdoptedOrAcknowledged) {
    WWDG->CR = WWDG_CR_WDGA | 127U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_window_watchdog_prepare(&owner, 8U));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_watchdog_port_state(&face, &observed));
    EXPECT_TRUE(observed.enabled);
    EXPECT_TRUE(observed.irreversible);
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32_window_watchdog_stop(&owner));
    EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_WWDGEN);
    /* A separate model reset supplies an inactive foreign EWI configuration. */
    owner = {};
    RCC->APB1ENR = 0U;
    WWDG->CR = 127U;
    WWDG->CFR = WWDG_CFR_EWI | 127U;
    WWDG->SR = 1U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_window_watchdog_prepare(&owner, 8U));
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_window_watchdog_stop(&owner));
    EXPECT_EQ(1U, WWDG->SR);
    EXPECT_NE(0U, WWDG->CFR & WWDG_CFR_EWI);
    /* EWI is sticky until hardware reset; ordinary close cannot withdraw it. */
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_window_watchdog_stop(&owner));
    EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_WWDGEN);
    EXPECT_EQ(1U, WWDG->SR);
}
