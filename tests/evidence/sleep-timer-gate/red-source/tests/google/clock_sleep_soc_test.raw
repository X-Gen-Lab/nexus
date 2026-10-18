/**
 * \file            clock_sleep_soc_test.cpp
 * \brief           Actual fixed RUN MMIO plus real guarded WFI mechanism
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "clock_domain_impl.h"
#include "clock_sleep_soc_model.h"
#include "nexus/arch/sleep.h"
#ifdef NX_CLOCK_GD_MODEL
#include "gd32f470_sleep.h"
#include "gd32f4xx_fmc.h"
#include "gd32f4xx_rcu.h"
#define SOC_SLEEP    nx_gd32f470_sleep
#define PLL          RCU_PLL
#define CONTROL      RCU_CTL
#define CONFIG       RCU_CFG0
#define WAIT_STATE   FMC_WS
#define SOURCE_READY RCU_CTL_HXTALSTB
#define SLEEP_CLOCK  RCU_APB1SPEN
#define TIMER_SLEEP  RCU_APB1SPEN_TIMER1SPEN
#else
#include "stm32f407_sleep.h"
#include "stm32f407xx.h"
#define SOC_SLEEP    nx_stm32f407_sleep
#define PLL          RCC->PLLCFGR
#define CONTROL      RCC->CR
#define CONFIG       RCC->CFGR
#define WAIT_STATE   FLASH->ACR
#define SOURCE_READY RCC_CR_HSERDY
#define SLEEP_CLOCK  RCC->APB1LPENR
#define TIMER_SLEEP  RCC_APB1LPENR_TIM2LPEN
#endif
#include <array>
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>
namespace {
bool privileged;
uint32_t exception_number, basepri, faultmask, sleep_control;
unsigned waits;
void (*on_mask)();
uint32_t* live_sequence;
nx_clock_domain_t* live_domain;
} /* namespace */
extern "C" bool nx_clock_test_privileged(void) {
    return privileged;
}
extern "C" uint32_t nx_clock_test_exception(void) {
    return exception_number;
}
extern "C" nx_arch_irq_masks_t nx_sleep_test_masks(void) {
    auto masks = nx_arch_irq_masks();
    masks.basepri = basepri;
    masks.faultmask = faultmask;
    return masks;
}
extern "C" nx_arch_irq_state_t nx_sleep_test_irq_save(void) {
    auto state = nx_arch_irq_save();
    if (on_mask != nullptr) {
        auto hook = on_mask;
        on_mask = nullptr;
        hook();
    }
    return state;
}
extern "C" uint32_t nx_arch_sleep_model_control(void) {
    return sleep_control;
}
extern "C" void nx_arch_sleep_model_wfi(void) {
    EXPECT_TRUE(nx_arch_irq_is_masked());
    ++waits;
}
class ClockSleep : public ::testing::Test {
  protected:
    nx_clock_domain_t domain = {};
    nx_clock_barrier_t barrier = {};
    nx_clock_lease_t token = {};
    uint32_t sequence = 77;
    bool slept = true;
    void SetUp() override {
        auto* mapped = mmap(reinterpret_cast<void*>(0x40023000U), 8192U,
                            PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        ASSERT_EQ(mapped, reinterpret_cast<void*>(0x40023000U));
        std::memset(mapped, 0, 8192U);
        privileged = true;
        exception_number = basepri = faultmask = sleep_control = 0;
        waits = 0;
        on_mask = nullptr;
        live_sequence = &sequence;
        live_domain = &domain;
#ifdef NX_CLOCK_GD_MODEL
        CONTROL =
            RCU_CTL_HXTALEN | RCU_CTL_HXTALSTB | RCU_CTL_PLLEN | RCU_CTL_PLLSTB;
        PLL = 25U | (400U << 6U) | RCU_PLLSRC_HXTAL | (9U << 24U);
        CONFIG = RCU_CKSYSSRC_PLLP | RCU_SCSS_PLLP | RCU_APB1_CKAHB_DIV4 |
                 RCU_APB2_CKAHB_DIV2;
        WAIT_STATE = WS_WSCNT_7;
#else
        CONTROL = RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY;
        PLL = 8U | (336U << 6U) | RCC_PLLCFGR_PLLSRC_HSE | (7U << 24U);
        CONFIG = RCC_CFGR_SW_PLL | RCC_CFGR_SWS_PLL | RCC_CFGR_PPRE1_DIV4 |
                 RCC_CFGR_PPRE2_DIV2;
        WAIT_STATE = FLASH_ACR_LATENCY_5WS;
#endif
        SLEEP_CLOCK = TIMER_SLEEP;
        ASSERT_EQ(nx_clock_domain_construct(&domain, 2), NX_SUCCESS);
        ASSERT_EQ(nx_clock_domain_close(&domain, &barrier), NX_SUCCESS);
    }
    void TearDown() override {
        EXPECT_FALSE(nx_arch_irq_is_masked());
        EXPECT_EQ(munmap(reinterpret_cast<void*>(0x40023000U), 8192U), 0);
    }
    nx_result_t Sleep(nx_clock_power_mode_t mode = NX_CLOCK_SHALLOW_SLEEP) {
        return SOC_SLEEP(barrier, mode, &sequence, 77, &slept);
    }
    using Registers = std::array<unsigned char, 8192>;
    Registers ClockRegisters() {
        Registers result;
        std::memcpy(result.data(), reinterpret_cast<void*>(0x40023000U),
                    result.size());
        return result;
    }
    void ExpectRejectedWithoutMutation(const Registers& registers) {
        auto saved_domain = domain;
        auto saved_barrier = barrier;
        auto saved_token = token;
        auto saved_sequence = sequence;
        EXPECT_EQ(Sleep(), NX_ERROR_UNSUPPORTED);
        EXPECT_FALSE(slept);
        EXPECT_EQ(waits, 0U);
        EXPECT_FALSE(nx_arch_irq_is_masked());
        EXPECT_EQ(std::memcmp(registers.data(),
                              reinterpret_cast<void*>(0x40023000U),
                              registers.size()),
                  0);
        EXPECT_EQ(std::memcmp(&saved_domain, &domain, sizeof(domain)), 0);
        EXPECT_EQ(std::memcmp(&saved_barrier, &barrier, sizeof(barrier)), 0);
        EXPECT_EQ(std::memcmp(&saved_token, &token, sizeof(token)), 0);
        EXPECT_EQ(sequence, saved_sequence);
    }
};
TEST_F(ClockSleep, FixedRunDrainedWfiPreservesAllClockRegisters) {
    std::array<unsigned char, 8192> before;
    std::memcpy(before.data(), reinterpret_cast<void*>(0x40023000U),
                before.size());
    EXPECT_EQ(Sleep(), NX_SUCCESS);
    EXPECT_TRUE(slept);
    EXPECT_EQ(waits, 1U);
    EXPECT_EQ(std::memcmp(before.data(), reinterpret_cast<void*>(0x40023000U),
                          before.size()),
              0);
    EXPECT_TRUE(domain.closing);
}
TEST_F(ClockSleep, ChangedSequenceDoesNotExecuteWfi) {
    sequence = 78;
    EXPECT_EQ(Sleep(), NX_SUCCESS);
    EXPECT_FALSE(slept);
    EXPECT_EQ(waits, 0U);
}
TEST_F(ClockSleep, PublicationAtMaskBoundaryIsRechecked) {
    on_mask = []() { __atomic_store_n(live_sequence, 78U, __ATOMIC_RELEASE); };
    EXPECT_EQ(Sleep(), NX_SUCCESS);
    EXPECT_FALSE(slept);
    EXPECT_EQ(waits, 0U);
}
TEST_F(ClockSleep, OutstandingLeaseCannotBeRevokedBySleep) {
    ASSERT_EQ(nx_clock_domain_reopen(barrier), NX_SUCCESS);
    ASSERT_EQ(nx_clock_domain_acquire(&domain, &token), NX_SUCCESS);
    ASSERT_EQ(nx_clock_domain_close(&domain, &barrier), NX_SUCCESS);
    EXPECT_EQ(Sleep(), NX_ERROR_BUSY);
    EXPECT_EQ(token.owner, &domain);
    EXPECT_EQ(waits, 0U);
    EXPECT_EQ(nx_clock_domain_release(&domain, &token), NX_SUCCESS);
}
TEST_F(ClockSleep, StaleOrOpenBarrierCannotSleep) {
    ASSERT_EQ(nx_clock_domain_reopen(barrier), NX_SUCCESS);
    EXPECT_EQ(Sleep(), NX_ERROR_STATE);
    EXPECT_EQ(waits, 0U);
}
TEST_F(ClockSleep, ReopenAtFinalMaskBoundaryCannotBypassDrain) {
    on_mask = []() {
        nx_clock_barrier_t control = {live_domain, live_domain->epoch};
        EXPECT_EQ(nx_clock_domain_reopen(control), NX_SUCCESS);
    };
    EXPECT_EQ(Sleep(), NX_ERROR_STATE);
    EXPECT_EQ(waits, 0U);
}
TEST_F(ClockSleep, MissingCrystalOrChangedDividerRejectsReadOnly) {
    CONTROL &= ~static_cast<uint32_t>(SOURCE_READY);
    EXPECT_EQ(Sleep(), NX_ERROR_UNSUPPORTED);
    CONTROL |= SOURCE_READY;
    PLL ^= 1U;
    EXPECT_EQ(Sleep(), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(waits, 0U);
}
TEST_F(ClockSleep, WrongActualMuxBusOrFlashProfileRejects) {
    uint32_t saved = CONFIG;
    CONFIG = 0;
    EXPECT_EQ(Sleep(), NX_ERROR_UNSUPPORTED);
    CONFIG = saved ^ (1U << 10U);
    EXPECT_EQ(Sleep(), NX_ERROR_UNSUPPORTED);
    CONFIG = saved;
    WAIT_STATE = 0;
    EXPECT_EQ(Sleep(), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(waits, 0U);
}
TEST_F(ClockSleep, SourceFailureAtMaskBoundaryRejectsBeforeWfi) {
    on_mask = []() { CONTROL &= ~static_cast<uint32_t>(SOURCE_READY); };
    EXPECT_EQ(Sleep(), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(waits, 0U);
}
TEST_F(ClockSleep, DisabledTimerSleepClockRejectsWithoutRepair) {
    SLEEP_CLOCK &= ~static_cast<uint32_t>(TIMER_SLEEP);
    ExpectRejectedWithoutMutation(ClockRegisters());

    /* Only the external clock owner repairs its profile. Rejection must not
     * consume the closed barrier or change its retained identity. */
    SLEEP_CLOCK |= TIMER_SLEEP;
    EXPECT_EQ(Sleep(), NX_SUCCESS);
    EXPECT_TRUE(slept);
    EXPECT_EQ(waits, 1U);
}
TEST_F(ClockSleep, TimerSleepClockAtFinalMaskBoundaryIsRechecked) {
    SLEEP_CLOCK &= ~static_cast<uint32_t>(TIMER_SLEEP);
    auto expected = ClockRegisters();
    SLEEP_CLOCK |= TIMER_SLEEP;
    on_mask = []() { SLEEP_CLOCK &= ~static_cast<uint32_t>(TIMER_SLEEP); };
    ExpectRejectedWithoutMutation(expected);
}
TEST_F(ClockSleep, StopAndDynamicRunAreExplicitlyUnsupported) {
    EXPECT_EQ(Sleep(NX_CLOCK_DEEP_STOP), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(Sleep(NX_CLOCK_DYNAMIC_RUN), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(Sleep(static_cast<nx_clock_power_mode_t>(3)), NX_ERROR_INVALID);
    EXPECT_EQ(waits, 0U);
}
TEST_F(ClockSleep, DeepScrDoesNotExecuteWfiOrClearCallerControl) {
    sleep_control = 4;
    EXPECT_EQ(Sleep(), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(sleep_control, 4U);
    EXPECT_FALSE(slept);
    EXPECT_EQ(waits, 0U);
}
TEST_F(ClockSleep, PrivilegeAndHandlerRejectBeforeWfi) {
    privileged = false;
    EXPECT_EQ(Sleep(), NX_ERROR_CONTEXT);
    privileged = true;
    exception_number = 16;
    EXPECT_EQ(Sleep(), NX_ERROR_CONTEXT);
    EXPECT_EQ(waits, 0U);
}
TEST_F(ClockSleep, IncomingMaskKindsRejectAndRemainUnchanged) {
    auto mask = nx_arch_irq_save();
    EXPECT_EQ(Sleep(), NX_ERROR_CONTEXT);
    EXPECT_TRUE(nx_arch_irq_is_masked());
    nx_arch_irq_restore(mask);
    basepri = 32;
    EXPECT_EQ(Sleep(), NX_ERROR_CONTEXT);
    EXPECT_EQ(basepri, 32U);
    basepri = 0;
    faultmask = 1;
    EXPECT_EQ(Sleep(), NX_ERROR_CONTEXT);
    EXPECT_EQ(faultmask, 1U);
    faultmask = 0;
    EXPECT_EQ(waits, 0U);
}
TEST_F(ClockSleep, OutputCannotCorruptDomainOrWakeWord) {
    auto before = domain;
    EXPECT_EQ(SOC_SLEEP(barrier, NX_CLOCK_SHALLOW_SLEEP, &sequence, 77,
                        reinterpret_cast<bool*>(&domain)),
              NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&before, &domain, sizeof(domain)), 0);
    EXPECT_EQ(SOC_SLEEP(barrier, NX_CLOCK_SHALLOW_SLEEP, &sequence, 77,
                        reinterpret_cast<bool*>(&sequence)),
              NX_ERROR_INVALID);
    EXPECT_EQ(sequence, 77U);
    EXPECT_EQ(waits, 0U);
}
TEST_F(ClockSleep, NullOrUnalignedWakeRejectsWithoutObservation) {
    EXPECT_EQ(SOC_SLEEP(barrier, NX_CLOCK_SHALLOW_SLEEP, nullptr, 77, &slept),
              NX_ERROR_INVALID);
    alignas(uint32_t) unsigned char word[8] = {};
    EXPECT_EQ(SOC_SLEEP(barrier, NX_CLOCK_SHALLOW_SLEEP,
                        reinterpret_cast<uint32_t*>(word + 1), 77, &slept),
              NX_ERROR_INVALID);
    EXPECT_EQ(
        SOC_SLEEP(barrier, NX_CLOCK_SHALLOW_SLEEP, &sequence, 77, nullptr),
        NX_ERROR_INVALID);
    EXPECT_EQ(waits, 0U);
}

TEST_F(ClockSleep, SpreadSpectrumCannotClaimAFixedDeadlineClock) {
#ifdef NX_CLOCK_GD_MODEL
    RCU_PLLSSCTL = RCU_PLLSSCTL_SSCGON;
#else
    RCC->SSCGR = RCC_SSCGR_SSCGEN;
#endif
    EXPECT_EQ(Sleep(), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(waits, 0U);
}
#ifdef NX_CLOCK_GD_MODEL
TEST_F(ClockSleep, ChangedTimerPrescaleCannotRetainDeadlineDomain) {
    RCU_CFG1 = RCU_CFG1_TIMERSEL;
    EXPECT_EQ(Sleep(), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(waits, 0U);
}
#endif
TEST_F(ClockSleep, ActiveTokenOutputCannotOverwriteAClockBorrow) {
    ASSERT_EQ(nx_clock_domain_reopen(barrier), NX_SUCCESS);
    ASSERT_EQ(nx_clock_domain_acquire(&domain, &token), NX_SUCCESS);
    ASSERT_EQ(nx_clock_domain_close(&domain, &barrier), NX_SUCCESS);
    EXPECT_EQ(SOC_SLEEP(barrier, NX_CLOCK_SHALLOW_SLEEP, &sequence, 77,
                        reinterpret_cast<bool*>(&token)),
              NX_ERROR_INVALID);
    EXPECT_EQ(token.owner, &domain);
    EXPECT_EQ(domain.count, 1U);
    EXPECT_EQ(nx_clock_domain_release(&domain, &token), NX_SUCCESS);
}
