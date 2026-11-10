/**
 * \file            stm32_system_transaction_test.cpp
 * \brief           Owned F407 clock transaction and failed readback contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "clock_domain_impl.h"
#include "nexus/core/fault.h"
#include "nexus/soc/stm32f407_power.h"
#include "nexus/soc/stm32f407_supervision.h"
#include "stm32f407_clock.h"
#include "stm32f407_system.h"
nx_result_t nx_stm32f407_power_enter(nx_clock_barrier_t,
                                     const nx_stm32_power_options_t*,
                                     nx_stm32_stop_receipt_t*)
    __attribute__((weak));
nx_result_t nx_stm32f407_standby_enter(nx_clock_barrier_t,
                                       const nx_stm32_power_options_t*,
                                       const nx_rtc_port_t*, nx_rtc_standby_t*,
                                       nx_stm32_stop_receipt_t*)
    __attribute__((weak));
nx_result_t nx_stm32f407_deadline_bridge(const nx_stm32_stop_receipt_t*,
                                         uint64_t, uint64_t, uint64_t, uint64_t,
                                         nx_stm32_deadline_bridge_t*)
    __attribute__((weak));
/* A missing CMSIS producer is an observable RED, not a fake clock provider. */
extern uint32_t SystemCoreClock __attribute__((weak));
uint32_t HAL_GetTick(void) __attribute__((weak));
uint64_t nx_stm32_system_clock_read(void* context) __attribute__((weak));
void nx_stm32_system_css_irq(nx_stm32_system_t*) __attribute__((weak));
bool nx_stm32_system_cpu_boot(SCB_Type*, volatile const uint32_t*, uintptr_t)
    __attribute__((weak));
nx_result_t nx_stm32f407_fault_capture(nx_stm32_fault_record_t*, uint32_t,
                                       uint32_t,
                                       const nx_stm32_exception_frame_t*)
    __attribute__((weak));
nx_result_t nx_stm32f407_fault_read(const nx_stm32_fault_record_t*,
                                    nx_stm32_fault_record_t*)
    __attribute__((weak));
nx_result_t nx_stm32f407_reset_flags_clear(void) __attribute__((weak));
void nx_stm32f407_reset_request(void) __attribute__((weak, noreturn));
nx_result_t nx_stm32f407_cycle_trace(bool, bool, nx_stm32_cycle_trace_t*)
    __attribute__((weak));
}
#include "terminal_watchdog.h"
#include <cstring>
#include <functional>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <unistd.h>
#include <vector>

nx_stm32_system_t g_nx_stm32_system{};
extern "C" {
uint32_t nx_power_scr;
uint32_t nx_power_icsr;
uint32_t nx_power_tick;
uint32_t nx_power_emr;
uint32_t nx_power_rtsr;
uint32_t nx_power_imr;
uint32_t nx_power_ftsr;
uint32_t nx_power_dma1[64];
uint32_t nx_power_dma2[64];
uint32_t nx_power_enabled[3];
uint32_t nx_power_active[3];
uint32_t nx_boot_cpacr;
uint32_t nx_boot_mpu;
uint32_t nx_boot_vtor;
uint32_t nx_system_rng_control;
SCB_Type nx_supervision_scb{};
DWT_Type nx_supervision_dwt{};
DCB_Type nx_supervision_debug{};
}
#if defined(NEXUS_SYSTEM_CLOCK_TEST_84)
#define NX_TEST_CORE_HZ   84000000U
#define NX_TEST_TIMER_PSC 41U
#else
#define NX_TEST_CORE_HZ   168000000U
#define NX_TEST_TIMER_PSC 83U
#endif
#if defined(NEXUS_SYSTEM_CLOCK_TEST_HSI84)
#define NX_TEST_HOLD_SOURCE hold_pll
#else
#define NX_TEST_HOLD_SOURCE hold_hse
#endif

namespace {
class HardwarePort {
  public:
    MOCK_METHOD(void, Write, (volatile uint32_t * reg, uint32_t value));
    MOCK_METHOD(void, Fault,
                (nx_fault_domain_t, nx_result_t, const char*, unsigned));
};
struct ParkSignal {};
HardwarePort* hardware_port;
uint32_t mask;
bool privileged = true;
bool handler;
bool irq_enabled;
bool irq_pending;
bool irq_active;
bool refuse_irq_disable;
bool source_prevents_vector_proof;
bool hold_hse;
bool hold_hsi;
bool hold_pll;
bool hold_switch;
bool update_observed;
bool update_trigger_observed;
void (*before_save)();
std::function<void(volatile uint32_t*, uint32_t)> before_write;
std::function<void()> before_power_wait;
testing::MockFunction<nx_result_t(unsigned)>* rtc_calls;
const nx_rtc_port_t* expected_rtc;
nx_rtc_standby_t* expected_rtc_receipt;
unsigned deep_waits;
unsigned event_waits;
bool fail_deep_restore;

class Stm32System : public testing::Test {
  protected:
    RCC_TypeDef rcc{};
    FLASH_TypeDef flash{};
    PWR_TypeDef power{};
    TIM_TypeDef timer{};
    testing::NiceMock<HardwarePort> hardware_mock;

    void SetUp() override {
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = &rcc;
        g_nx_stm32_system.flash = &flash;
        g_nx_stm32_system.power = &power;
        g_nx_stm32_system.timer = &timer;
        rcc.CR = RCC_CR_HSION | RCC_CR_HSIRDY | (16U << RCC_CR_HSITRIM_Pos);
        rcc.PLLCFGR = UINT32_C(0x24003010);
        flash.ACR = FLASH_ACR_ICEN;
        power.CR = PWR_CR_DBP;
        mask = 0U;
        privileged = true;
        handler = false;
        irq_enabled = irq_pending = irq_active = false;
        refuse_irq_disable = false;
        source_prevents_vector_proof = false;
        hold_hse = hold_hsi = hold_pll = hold_switch = false;
        update_observed = false;
        update_trigger_observed = false;
        before_save = nullptr;
        before_write = nullptr;
        before_power_wait = nullptr;
        rtc_calls = nullptr;
        expected_rtc = nullptr;
        expected_rtc_receipt = nullptr;
        nx_power_scr = nx_power_icsr = nx_power_tick = 0U;
        nx_power_emr = nx_power_ftsr = 0U;
        nx_power_imr = nx_power_rtsr = 1U;
        for (unsigned i = 0; i < 64; ++i)
            nx_power_dma1[i] = nx_power_dma2[i] = 0U;
        for (unsigned i = 0; i < 3; ++i)
            nx_power_enabled[i] = nx_power_active[i] = 0U;
        deep_waits = 0U;
        event_waits = 0U;
        fail_deep_restore = false;
        nx_boot_cpacr = 0x00f00000U;
        nx_boot_mpu = 0U;
        nx_boot_vtor = 0x08000000U;
        nx_system_rng_control = 0U;
        std::memset(static_cast<void*>(&nx_supervision_scb), 0,
                    sizeof(nx_supervision_scb));
        std::memset(static_cast<void*>(&nx_supervision_dwt), 0,
                    sizeof(nx_supervision_dwt));
        std::memset(static_cast<void*>(&nx_supervision_debug), 0,
                    sizeof(nx_supervision_debug));
        hardware_port = &hardware_mock;
        EXPECT_CALL(hardware_mock, Write(testing::_, testing::_))
            .Times(testing::AnyNumber());
        ON_CALL(hardware_mock, Write(testing::_, testing::_))
            .WillByDefault([](volatile uint32_t* reg, uint32_t value) {
                auto* hardware_timer = g_nx_stm32_system.timer;
                if (reg == &hardware_timer->SR) {
                    *reg &= value;
                } else if (reg == &g_nx_stm32_system.rcc->CSR &&
                           (value & RCC_CSR_RMVF) != 0U) {
                    *reg = value &
                           ~(uint32_t)(RCC_CSR_RMVF | RCC_CSR_BORRSTF |
                                       RCC_CSR_PINRSTF | RCC_CSR_PORRSTF |
                                       RCC_CSR_SFTRSTF | RCC_CSR_IWDGRSTF |
                                       RCC_CSR_WWDGRSTF | RCC_CSR_LPWRRSTF);
                } else if (reg == &g_nx_stm32_system.power->CR &&
                           (value & PWR_CR_CWUF) != 0U) {
                    g_nx_stm32_system.power->CSR &= ~(uint32_t)PWR_CSR_WUF;
                    *reg = value & ~(uint32_t)PWR_CR_CWUF;
                } else {
                    *reg = value;
                }
                if (reg == &hardware_timer->EGR && (value & TIM_EGR_UG) != 0U) {
                    update_observed = true;
                    if ((hardware_timer->CR2 & TIM_CR2_MMS) == 0U)
                        update_trigger_observed = true;
                    hardware_timer->SR |= TIM_SR_UIF;
                    hardware_timer->CNT = 0U;
                }
            });
    }
    void TearDown() override {
        /* Host fixture reset after observer interception is not recovery from
         * the production non-returning terminal path. */
        testing::Mock::VerifyAndClearExpectations(&hardware_mock);
        hold_hse = hold_hsi = hold_pll = hold_switch = false;
        refuse_irq_disable = false;
        source_prevents_vector_proof = false;
        before_save = nullptr;
        before_write = nullptr;
        before_power_wait = nullptr;
        rtc_calls = nullptr;
        irq_active = false;
        mask = 0U;
        handler = false;
        privileged = true;
        EXPECT_EQ(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
        hardware_port = nullptr;
    }
    nx_stm32_start_result_t Start() {
        return nx_stm32_system_start(&g_nx_stm32_system, 8U);
    }
    void RtcWake() {
        rcc.BDCR = RCC_BDCR_RTCEN | RCC_BDCR_RTCSEL_0 | RCC_BDCR_LSEON |
                   RCC_BDCR_LSERDY;
        nx_power_emr = nx_power_rtsr = NX_STM32_WAKE_RTC_TIMER;
        power.CSR = PWR_CSR_EWUP;
    }
};
} /* namespace */

TEST_F(Stm32System, ExactClockAndIndependentTimerHaveOneLiveEpoch) {
    ASSERT_EQ(0, Start().primary);
    EXPECT_EQ(NX_TEST_CORE_HZ, g_nx_stm32_system.core_hz);
    EXPECT_EQ(NX_TEST_TIMER_PSC, timer.PSC);
    EXPECT_EQ(UINT32_MAX, timer.ARR);
#if defined(NEXUS_SYSTEM_CLOCK_TEST_HSI84)
    EXPECT_EQ(0U, rcc.CR & RCC_CR_HSEON);
    EXPECT_EQ(0U, rcc.PLLCFGR & RCC_PLLCFGR_PLLSRC_HSE);
#else
    EXPECT_NE(0U, rcc.CR & RCC_CR_HSEON);
#endif
    timer.CNT = 5U;
    EXPECT_EQ(5U, nx_stm32_system_now(&g_nx_stm32_system));
    ASSERT_EQ(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    EXPECT_EQ(0U, nx_stm32_system_now(&g_nx_stm32_system));
}

TEST_F(Stm32System, LiveRngClockLeaseBlocksSystemWithdrawalWithoutAnyWrite) {
    ASSERT_EQ(0, Start().primary);
    rcc.AHB2ENR |= RCC_AHB2ENR_RNGEN;
    nx_system_rng_control = RNG_CR_RNGEN;
    const uint32_t remaining = g_nx_stm32_system.remaining_effects;
    const uint64_t epoch = g_nx_stm32_system.time_epoch;
    testing::Mock::VerifyAndClearExpectations(&hardware_mock);
    EXPECT_CALL(hardware_mock, Write(testing::_, testing::_)).Times(0);
    EXPECT_NE(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    EXPECT_TRUE(g_nx_stm32_system.started);
    EXPECT_EQ(remaining, g_nx_stm32_system.remaining_effects);
    EXPECT_EQ(epoch, g_nx_stm32_system.time_epoch);
    testing::Mock::VerifyAndClearExpectations(&hardware_mock);
    EXPECT_CALL(hardware_mock, Write(testing::_, testing::_))
        .Times(testing::AnyNumber());
    nx_system_rng_control = 0U;
    rcc.AHB2ENR &= ~(uint32_t)RCC_AHB2ENR_RNGEN;
    EXPECT_EQ(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
}

TEST_F(Stm32System, CssLossEndsTimeBeforeObservationAndRetainsEveryEffect) {
    ASSERT_NE(nullptr, &nx_stm32_system_css_irq);
    ASSERT_EQ(0, Start().primary);
    uint32_t effects = g_nx_stm32_system.remaining_effects;
    rcc.CIR = RCC_CIR_CSSF | RCC_CIR_LSIRDYIE;
    EXPECT_CALL(hardware_mock,
                Fault(NX_FAULT_PLATFORM, NX_ERROR_IO, testing::_, testing::_))
        .WillOnce([](nx_fault_domain_t, nx_result_t, const char*, unsigned) {
            EXPECT_FALSE(g_nx_stm32_system.started);
            EXPECT_EQ(NX_ERROR_IO, g_nx_stm32_system.timer_error);
            EXPECT_EQ(1U, mask);
            throw ParkSignal{};
        });
    EXPECT_THROW(nx_stm32_system_css_irq(&g_nx_stm32_system), ParkSignal);
    EXPECT_EQ(effects, g_nx_stm32_system.remaining_effects);
    EXPECT_EQ(RCC_CIR_LSIRDYIE, rcc.CIR & RCC_CIR_LSIRDYIE);
    EXPECT_NE(0U, rcc.CIR & RCC_CIR_CSSC);
}

TEST_F(Stm32System, ColdStartupRejectsMissingFpuForeignMpuAndWrongVector) {
    EXPECT_CALL(hardware_mock, Write(testing::_, testing::_)).Times(0);
    nx_boot_cpacr = 0U;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32_system_preflight(&g_nx_stm32_system));
    nx_boot_cpacr = 0x00f00000U;
    nx_boot_mpu = MPU_CTRL_ENABLE_Msk;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32_system_preflight(&g_nx_stm32_system));
    nx_boot_mpu = 0U;
    nx_boot_vtor += 512U;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32_system_preflight(&g_nx_stm32_system));
}

TEST_F(Stm32System, CpuBootChecksMpuAndVectorBeforeEnablingHardFloat) {
    ASSERT_NE(nullptr, &nx_stm32_system_cpu_boot);
    MPU_Type mpu{};
    mpu.CTRL = MPU_CTRL_ENABLE_Msk;
    EXPECT_FALSE(
        nx_stm32_system_cpu_boot(&nx_supervision_scb, &mpu.CTRL, 0x08000000U));
    EXPECT_EQ(0U, nx_supervision_scb.CPACR);
    mpu.CTRL = 0U;
    EXPECT_FALSE(
        nx_stm32_system_cpu_boot(&nx_supervision_scb, &mpu.CTRL, 0x08000004U));
    EXPECT_TRUE(
        nx_stm32_system_cpu_boot(&nx_supervision_scb, &mpu.CTRL, 0x08000200U));
    EXPECT_EQ(0x00f00000U, nx_supervision_scb.CPACR);
    EXPECT_EQ(0x08000200U, nx_supervision_scb.VTOR);
}

TEST_F(Stm32System, FirstFaultSnapshotCannotBeReplacedOrReadWhileIncomplete) {
    ASSERT_NE(nullptr, &nx_stm32f407_fault_capture);
    ASSERT_NE(nullptr, &nx_stm32f407_fault_read);
    nx_stm32_fault_record_t record{};
    nx_stm32_fault_record_t output{};
    output.reason = 99U;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32f407_fault_read(&record, &output));
    EXPECT_EQ(99U, output.reason);
    nx_stm32_exception_frame_t frame{};
    frame.pc = 0x08001234U;
    frame.xpsr = 0x01000000U;
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32f407_fault_capture(&record, 7U, 0xfffffff9U, &frame));
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32f407_fault_capture(&record, 8U, 0xfffffff9U, nullptr));
    ASSERT_EQ(NX_SUCCESS, nx_stm32f407_fault_read(&record, &output));
    EXPECT_EQ(7U, output.reason);
    EXPECT_EQ(frame.pc, output.frame.pc);
    record.frame.pc ^= 1U;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f407_fault_read(&record, &output));
    EXPECT_EQ(frame.pc, output.frame.pc);
}

TEST_F(Stm32System, ExplicitResetFlagAcknowledgePreservesCapturedCauseAndLsi) {
    ASSERT_NE(nullptr, &nx_stm32f407_reset_flags_clear);
    rcc.CSR = RCC_CSR_IWDGRSTF | RCC_CSR_LSION;
    ASSERT_EQ(0, Start().primary);
    ASSERT_EQ(NX_SUCCESS, nx_stm32f407_reset_flags_clear());
    EXPECT_EQ(RCC_CSR_LSION, rcc.CSR);
    EXPECT_EQ(RCC_CSR_IWDGRSTF | RCC_CSR_LSION, g_nx_stm32_system.reset_cause);
}

TEST_F(Stm32System, ResetRequestUsesKeyAndKeepsPriorityGroupingAndMasks) {
    ASSERT_NE(nullptr, &nx_stm32f407_reset_request);
    nx_supervision_scb.AIRCR = 3U << SCB_AIRCR_PRIGROUP_Pos;
    EXPECT_CALL(hardware_mock, Write(&nx_supervision_scb.AIRCR,
                                     (0x5faU << SCB_AIRCR_VECTKEY_Pos) |
                                         (3U << SCB_AIRCR_PRIGROUP_Pos) |
                                         SCB_AIRCR_SYSRESETREQ_Msk))
        .WillOnce([](volatile uint32_t*, uint32_t) {
            EXPECT_EQ(1U, mask);
            throw ParkSignal{};
        });
    EXPECT_THROW(nx_stm32f407_reset_request(), ParkSignal);
    EXPECT_EQ(1U, mask);
}

TEST_F(Stm32System, CycleTraceIsExplicitAndDoesNotBecomeMonotonicTime) {
    ASSERT_NE(nullptr, &nx_stm32f407_cycle_trace);
    ASSERT_EQ(0, Start().primary);
    nx_stm32_cycle_trace_t trace{};
    ASSERT_EQ(NX_SUCCESS, nx_stm32f407_cycle_trace(true, true, &trace));
    EXPECT_TRUE(trace.enabled);
    EXPECT_EQ(NX_TEST_CORE_HZ, trace.core_hz);
    EXPECT_NE(0U, nx_supervision_debug.DEMCR & DCB_DEMCR_TRCENA_Msk);
    nx_supervision_dwt.CYCCNT = 77U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32f407_cycle_trace(false, false, &trace));
    EXPECT_FALSE(trace.enabled);
    EXPECT_EQ(77U, trace.cycles);
    EXPECT_NE(0U, nx_supervision_debug.DEMCR & DCB_DEMCR_TRCENA_Msk);
}

TEST_F(Stm32System, RefusedCycleResetCannotBeReportedAsSuccessfulReset) {
    ASSERT_EQ(0, Start().primary);
    nx_supervision_dwt.CYCCNT = 77U;
    EXPECT_CALL(hardware_mock, Write(&nx_supervision_dwt.CYCCNT, 0U))
        .WillOnce([](volatile uint32_t*, uint32_t) {});
    nx_stm32_cycle_trace_t trace{};
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f407_cycle_trace(true, true, &trace));
    EXPECT_EQ(0U, nx_supervision_dwt.CTRL & DWT_CTRL_CYCCNTENA_Msk);
}

TEST_F(Stm32System, SelectedHsiProfileRejectsForeignTrimBeforeReuse) {
    ASSERT_EQ(0, Start().primary);
    ASSERT_TRUE(nx_stm32_clock_profile_matches(&rcc, &flash));
#if defined(NEXUS_SYSTEM_CLOCK_TEST_HSI84)
    rcc.CR &= ~(uint32_t)RCC_CR_HSITRIM;
    EXPECT_FALSE(nx_stm32_clock_profile_matches(&rcc, &flash));
    rcc.CR |= 16U << RCC_CR_HSITRIM_Pos;
#endif
}

TEST_F(Stm32System, ColdAdmissionRejectsForeignRccInterruptAndReadySource) {
    rcc.CIR = RCC_CIR_LSIRDYIE;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_system_preflight(&g_nx_stm32_system));
    rcc.CIR = 0U;
    rcc.CR |= RCC_CR_HSERDY;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_system_preflight(&g_nx_stm32_system));
}

TEST_F(Stm32System, DisabledExtiRouteCannotBecomeASuccessfulWakePublisher) {
    ASSERT_EQ(0, Start().primary);
    nx_clock_domain_t domain{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_construct(&domain, 1U));
    nx_clock_barrier_t barrier{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_close(&domain, &barrier));
    nx_stm32_power_options_t options = {
        {1U << 6U, 0U, 0U}, 8U, true, false, 0U, false, false};
    nx_power_enabled[0] = 1U << 6U;
    nx_power_imr = 0U;
    nx_stm32_stop_receipt_t receipt{};
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32f407_power_enter(barrier, &options, &receipt));
    EXPECT_EQ(0U, deep_waits);
}

TEST_F(Stm32System, CapturedResetCauseSurvivesExplicitClearAndStopRestore) {
    rcc.CSR = RCC_CSR_PINRSTF;
    ASSERT_EQ(0, Start().primary);
    ASSERT_EQ(NX_SUCCESS, nx_stm32f407_reset_flags_clear());
    nx_clock_domain_t domain{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_construct(&domain, 1U));
    nx_clock_barrier_t barrier{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_close(&domain, &barrier));
    nx_stm32_power_options_t options = {
        {1U << 6U, 0U, 0U}, 8U, true, false, 0U, false, false};
    nx_power_enabled[0] = 1U << 6U;
    nx_stm32_stop_receipt_t receipt{};
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32f407_power_enter(barrier, &options, &receipt));
    EXPECT_EQ(RCC_CSR_PINRSTF, g_nx_stm32_system.reset_cause);
}

TEST_F(Stm32System, StandbyRequiresADeclaredIndependentWakeSource) {
    ASSERT_EQ(0, Start().primary);
    nx_clock_domain_t domain{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_construct(&domain, 1U));
    nx_clock_barrier_t barrier{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_close(&domain, &barrier));
    nx_stm32_power_options_t options = {
        {1U << 6U, 0U, 0U}, 8U, true, true, 0U, false, false};
    nx_power_enabled[0] = 1U << 6U;
    rcc.BDCR =
        RCC_BDCR_RTCEN | RCC_BDCR_RTCSEL_0 | RCC_BDCR_LSEON | RCC_BDCR_LSERDY;
    nx_stm32_stop_receipt_t receipt{};
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_stm32f407_power_enter(barrier, &options, &receipt));
    EXPECT_EQ(0U, deep_waits);
}

TEST_F(Stm32System, StandbyRearmsWakePinOnlyAfterClearingItsWakeFlag) {
    ASSERT_EQ(0, Start().primary);
    nx_clock_domain_t domain{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_construct(&domain, 1U));
    nx_clock_barrier_t barrier{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_close(&domain, &barrier));
    nx_stm32_power_options_t options{};
    options.poll_limit = 8U;
    options.standby = true;
    options.wake_irqs[0] = 1U << 6U;
    nx_power_enabled[0] = 1U << 6U;
    power.CSR = PWR_CSR_EWUP;
    std::vector<unsigned> sequence;
    before_write = [&](volatile uint32_t* reg, uint32_t value) {
        if (reg == &power.CSR && (value & PWR_CSR_EWUP) == 0U)
            sequence.push_back(1U);
        if (reg == &power.CR && (value & PWR_CR_CWUF) != 0U) {
            EXPECT_EQ(0U, power.CSR & PWR_CSR_EWUP);
            sequence.push_back(2U);
        }
        if (reg == &power.CSR && (value & PWR_CSR_EWUP) != 0U)
            sequence.push_back(3U);
    };
    before_power_wait = [&]() {
        EXPECT_EQ((std::vector<unsigned>{1U, 2U, 3U}), sequence);
        EXPECT_EQ(PWR_CSR_EWUP, power.CSR & (PWR_CSR_EWUP | PWR_CSR_WUF));
    };
    nx_stm32_stop_receipt_t receipt{};
    EXPECT_EQ(NX_ERROR_IO,
              nx_stm32f407_power_enter(barrier, &options, &receipt));
    EXPECT_EQ(1U, deep_waits);
    EXPECT_TRUE(g_nx_stm32_system.started);
    EXPECT_NE(receipt.old_epoch, receipt.new_epoch);
}

TEST_F(Stm32System, StandbyCannotDiscardAnUnconsumedWakeFlag) {
    ASSERT_EQ(0, Start().primary);
    nx_clock_domain_t domain{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_construct(&domain, 1U));
    nx_clock_barrier_t barrier{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_close(&domain, &barrier));
    nx_stm32_power_options_t options{};
    options.poll_limit = 8U;
    options.standby = true;
    options.wake_irqs[0] = 1U << 6U;
    nx_power_enabled[0] = 1U << 6U;
    power.CSR = PWR_CSR_EWUP | PWR_CSR_WUF;
    testing::Mock::VerifyAndClearExpectations(&hardware_mock);
    EXPECT_CALL(hardware_mock, Write(testing::_, testing::_)).Times(0);
    nx_stm32_stop_receipt_t receipt{};
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32f407_power_enter(barrier, &options, &receipt));
    EXPECT_EQ(0U, deep_waits);
    EXPECT_NE(0U, power.CSR & PWR_CSR_WUF);
}

TEST_F(Stm32System, StandbyWakePinDoesNotRequireAnUnrelatedNvicInterrupt) {
    ASSERT_EQ(0, Start().primary);
    nx_clock_domain_t domain{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_construct(&domain, 1U));
    nx_clock_barrier_t barrier{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_close(&domain, &barrier));
    nx_stm32_power_options_t options{};
    options.poll_limit = 8U;
    options.standby = true;
    power.CSR = PWR_CSR_EWUP;
    nx_stm32_stop_receipt_t receipt{};
    EXPECT_EQ(NX_ERROR_IO,
              nx_stm32f407_power_enter(barrier, &options, &receipt));
    EXPECT_EQ(1U, deep_waits);
    EXPECT_TRUE(receipt.waited);
    EXPECT_TRUE(g_nx_stm32_system.started);
}

TEST_F(Stm32System, ExplicitWakePinSelectionNeedsNoPrivateRegisterSetup) {
    ASSERT_EQ(0, Start().primary);
    nx_clock_domain_t domain{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_construct(&domain, 1U));
    nx_clock_barrier_t barrier{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_close(&domain, &barrier));
    nx_stm32_power_options_t options{};
    options.poll_limit = 8U;
    options.standby = options.wake_pin = true;
    EXPECT_EQ(0U, power.CSR & PWR_CSR_EWUP);
    before_power_wait = [&]() {
        EXPECT_NE(0U, power.CSR & PWR_CSR_EWUP);
        EXPECT_EQ(0U, power.CSR & PWR_CSR_WUF);
    };
    nx_stm32_stop_receipt_t receipt{};
    EXPECT_EQ(NX_ERROR_IO,
              nx_stm32f407_power_enter(barrier, &options, &receipt));
    EXPECT_TRUE(receipt.waited);
    EXPECT_EQ(1U, deep_waits);
    EXPECT_EQ(0U, power.CSR & PWR_CSR_EWUP);
}

TEST_F(Stm32System, WakePinSelectionCannotPretendToBeAStopWakeInterrupt) {
    ASSERT_EQ(0, Start().primary);
    nx_clock_domain_t domain{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_construct(&domain, 1U));
    nx_clock_barrier_t barrier{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_close(&domain, &barrier));
    nx_stm32_power_options_t options{};
    options.poll_limit = 8U;
    options.wake_pin = true;
    nx_stm32_stop_receipt_t receipt{};
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32f407_power_enter(barrier, &options, &receipt));
    EXPECT_EQ(0U, deep_waits);
}

TEST_F(Stm32System, RtcStandbyRequiresItsPreparedOwnerReceiptBeforeAnyEffect) {
    ASSERT_EQ(0, Start().primary);
    RtcWake();
    nx_clock_domain_t domain{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_construct(&domain, 1U));
    nx_clock_barrier_t barrier{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_close(&domain, &barrier));
    nx_stm32_power_options_t options{};
    options.poll_limit = 8U;
    options.standby = true;
    options.wake_events = NX_STM32_WAKE_RTC_TIMER;
    testing::Mock::VerifyAndClearExpectations(&hardware_mock);
    EXPECT_CALL(hardware_mock, Write(testing::_, testing::_)).Times(0);
    nx_stm32_stop_receipt_t receipt{};
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32f407_power_enter(barrier, &options, &receipt));
    EXPECT_EQ(0U, deep_waits);
}

TEST_F(Stm32System,
       RtcStandbyCommitsAfterCwufAndRestoresWithoutClearingEvents) {
    ASSERT_NE(nullptr, &nx_stm32f407_standby_enter);
    ASSERT_EQ(0, Start().primary);
    RtcWake();
    power.CSR |= PWR_CSR_WUF;
    nx_clock_domain_t domain{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_construct(&domain, 1U));
    nx_clock_barrier_t barrier{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_close(&domain, &barrier));
    nx_stm32_power_options_t options{};
    options.poll_limit = 8U;
    options.standby = options.discard_wakeup = true;
    options.wake_events = NX_STM32_WAKE_RTC_TIMER;
    nx_rtc_port_t rtc{};
    nx_rtc_standby_t standby{&domain, 1U};
    expected_rtc = &rtc;
    expected_rtc_receipt = &standby;
    testing::StrictMock<testing::MockFunction<nx_result_t(unsigned)>> calls;
    rtc_calls = &calls;
    std::vector<char> sequence;
    const uint64_t old_epoch = g_nx_stm32_system.time_epoch;
    testing::InSequence order;
    EXPECT_CALL(calls, Call(1U)).WillOnce([&](unsigned) {
        sequence.push_back('v');
        EXPECT_EQ(1U, mask);
        EXPECT_TRUE(g_nx_stm32_system.started);
        return NX_SUCCESS;
    });
    EXPECT_CALL(calls, Call(2U)).WillOnce([&](unsigned) {
        EXPECT_FALSE(g_nx_stm32_system.started);
        EXPECT_EQ(0U, power.CSR & (PWR_CSR_WUF | PWR_CSR_EWUP));
        EXPECT_NE(0U, power.CR & PWR_CR_PDDS);
        sequence.push_back('a');
        return NX_SUCCESS;
    });
    EXPECT_CALL(calls, Call(3U)).WillOnce([&](unsigned) {
        EXPECT_TRUE(g_nx_stm32_system.started);
        EXPECT_GT(g_nx_stm32_system.time_epoch, old_epoch);
        EXPECT_EQ(1U, mask);
        sequence.push_back('r');
        standby = {};
        return NX_SUCCESS;
    });
    before_write = [&](volatile uint32_t* reg, uint32_t value) {
        if (reg == &power.CSR)
            sequence.push_back((value & PWR_CSR_EWUP) == 0U ? 'd' : 'e');
        if (reg == &power.CR && (value & PWR_CR_CWUF) != 0U)
            sequence.push_back('c');
    };
    before_power_wait = [&]() {
        EXPECT_EQ((std::vector<char>{'v', 'd', 'c', 'a', 'e'}), sequence);
        sequence.push_back('w');
    };
    nx_stm32_stop_receipt_t receipt{};
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f407_standby_enter(barrier, &options, &rtc,
                                                      &standby, &receipt));
    EXPECT_EQ((std::vector<char>{'v', 'd', 'c', 'a', 'e', 'w', 'r'}), sequence);
    EXPECT_EQ(1U, deep_waits);
    EXPECT_EQ(0U, event_waits);
    EXPECT_EQ(nullptr, standby.owner);
    EXPECT_TRUE(receipt.waited);
    EXPECT_EQ(0U, mask);
}

TEST_F(Stm32System, RtcStandbyValidationFailureKeepsTheExactPreparedReceipt) {
    ASSERT_NE(nullptr, &nx_stm32f407_standby_enter);
    ASSERT_EQ(0, Start().primary);
    RtcWake();
    nx_clock_domain_t domain{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_construct(&domain, 1U));
    nx_clock_barrier_t barrier{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_close(&domain, &barrier));
    nx_stm32_power_options_t options{};
    options.poll_limit = 8U;
    options.standby = true;
    options.wake_events = NX_STM32_WAKE_RTC_TIMER;
    nx_rtc_port_t rtc{};
    nx_rtc_standby_t standby{&domain, 1U};
    expected_rtc = &rtc;
    expected_rtc_receipt = &standby;
    testing::StrictMock<testing::MockFunction<nx_result_t(unsigned)>> calls;
    rtc_calls = &calls;
    EXPECT_CALL(calls, Call(1U)).WillOnce(testing::Return(NX_ERROR_STATE));
    testing::Mock::VerifyAndClearExpectations(&hardware_mock);
    EXPECT_CALL(hardware_mock, Write(testing::_, testing::_)).Times(0);
    nx_stm32_stop_receipt_t receipt{91U, 92U, 93U, 94U, true};
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32f407_standby_enter(
                                  barrier, &options, &rtc, &standby, &receipt));
    EXPECT_EQ(91U, receipt.old_epoch);
    EXPECT_EQ(&domain, standby.owner);
    EXPECT_EQ(1U, standby.epoch);
    EXPECT_EQ(0U, deep_waits);
}

TEST_F(Stm32System, FailedRtcCommitRestoresRunBeforeReturningTheRtcReceipt) {
    ASSERT_NE(nullptr, &nx_stm32f407_standby_enter);
    ASSERT_EQ(0, Start().primary);
    RtcWake();
    nx_clock_domain_t domain{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_construct(&domain, 1U));
    nx_clock_barrier_t barrier{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_close(&domain, &barrier));
    nx_stm32_power_options_t options{};
    options.poll_limit = 8U;
    options.standby = true;
    options.wake_events = NX_STM32_WAKE_RTC_TIMER;
    nx_rtc_port_t rtc{};
    nx_rtc_standby_t standby{&domain, 1U};
    expected_rtc = &rtc;
    expected_rtc_receipt = &standby;
    testing::StrictMock<testing::MockFunction<nx_result_t(unsigned)>> calls;
    rtc_calls = &calls;
    const uint64_t old_epoch = g_nx_stm32_system.time_epoch;
    testing::InSequence order;
    EXPECT_CALL(calls, Call(1U)).WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_CALL(calls, Call(2U)).WillOnce(testing::Return(NX_ERROR_IO));
    EXPECT_CALL(calls, Call(3U)).WillOnce([&](unsigned) {
        EXPECT_TRUE(g_nx_stm32_system.started);
        EXPECT_GT(g_nx_stm32_system.time_epoch, old_epoch);
        standby = {};
        return NX_SUCCESS;
    });
    nx_stm32_stop_receipt_t receipt{};
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f407_standby_enter(barrier, &options, &rtc,
                                                      &standby, &receipt));
    EXPECT_EQ(0U, deep_waits);
    EXPECT_FALSE(receipt.waited);
    EXPECT_NE(receipt.old_epoch, receipt.new_epoch);
    EXPECT_EQ(0U, nx_power_scr & SCB_SCR_SLEEPDEEP_Msk);
    EXPECT_EQ(PWR_CSR_EWUP, power.CSR & PWR_CSR_EWUP);
    EXPECT_EQ(nullptr, standby.owner);
}

TEST_F(Stm32System, FailedRtcRestoreRetainsItsReceiptAfterANewRunEpoch) {
    ASSERT_NE(nullptr, &nx_stm32f407_standby_enter);
    ASSERT_EQ(0, Start().primary);
    RtcWake();
    nx_clock_domain_t domain{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_construct(&domain, 1U));
    nx_clock_barrier_t barrier{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_close(&domain, &barrier));
    nx_stm32_power_options_t options{};
    options.poll_limit = 8U;
    options.standby = true;
    options.wake_events = NX_STM32_WAKE_RTC_TIMER;
    nx_rtc_port_t rtc{};
    nx_rtc_standby_t standby{&domain, 1U};
    expected_rtc = &rtc;
    expected_rtc_receipt = &standby;
    testing::StrictMock<testing::MockFunction<nx_result_t(unsigned)>> calls;
    rtc_calls = &calls;
    testing::InSequence order;
    EXPECT_CALL(calls, Call(1U)).WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_CALL(calls, Call(2U)).WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_CALL(calls, Call(3U)).WillOnce(testing::Return(NX_ERROR_IO));
    EXPECT_CALL(calls, Call(3U)).WillOnce([&](unsigned) {
        standby = {};
        return NX_SUCCESS;
    });
    nx_stm32_stop_receipt_t receipt{};
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f407_standby_enter(barrier, &options, &rtc,
                                                      &standby, &receipt));
    EXPECT_EQ(&domain, standby.owner);
    EXPECT_TRUE(g_nx_stm32_system.started);
    EXPECT_TRUE(receipt.waited);
    EXPECT_NE(receipt.old_epoch, receipt.new_epoch);
    EXPECT_EQ(0U, mask);
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_standby_restore(&rtc, &standby));
    EXPECT_EQ(nullptr, standby.owner);
}

TEST_F(Stm32System, IndependentRtcEnvelopeBridgesOnlyTheRecordedEpoch) {
    ASSERT_NE(nullptr, &nx_stm32f407_deadline_bridge);
    nx_stm32_stop_receipt_t receipt = {1U, 2U, 100U, 3U, true};
    nx_stm32_deadline_bridge_t output = {9U, 9U, 9U};
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32f407_deadline_bridge(&receipt, 2U, 150U,
                                                           10U, 12U, &output));
    EXPECT_EQ(9U, output.epoch);
    ASSERT_EQ(NX_SUCCESS, nx_stm32f407_deadline_bridge(&receipt, 1U, 150U, 10U,
                                                       12U, &output));
    EXPECT_EQ(2U, output.epoch);
    EXPECT_EQ(41U, output.earliest_us);
    EXPECT_EQ(43U, output.latest_us);
    ASSERT_EQ(NX_SUCCESS, nx_stm32f407_deadline_bridge(&receipt, 1U, 105U, 10U,
                                                       12U, &output));
    EXPECT_EQ(3U, output.earliest_us);
    EXPECT_EQ(3U, output.latest_us);
}

TEST_F(Stm32System, StopRejectsDmaAndPeriodicTickBeforeEffects) {
    ASSERT_NE(nullptr, &nx_stm32f407_power_enter);
    ASSERT_EQ(0, Start().primary);
    nx_clock_domain_t domain{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_construct(&domain, 1U));
    nx_clock_barrier_t barrier{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_close(&domain, &barrier));
    nx_stm32_power_options_t options = {
        {1U << 6U, 0U, 0U}, 8U, true, false, 0U, false, false};
    nx_power_enabled[0] = 1U << 6U;
    nx_stm32_stop_receipt_t receipt{};
    nx_power_tick = SysTick_CTRL_ENABLE_Msk;
    EXPECT_CALL(hardware_mock, Write(testing::_, testing::_)).Times(0);
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32f407_power_enter(barrier, &options, &receipt));
    nx_power_tick = 0U;
    rcc.AHB1ENR |= RCC_AHB1ENR_DMA2EN;
    nx_power_dma2[4U + 7U * 6U] = DMA_SxCR_EN;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32f407_power_enter(barrier, &options, &receipt));
    EXPECT_EQ(0U, deep_waits);
    EXPECT_TRUE(g_nx_stm32_system.started);
}

TEST_F(Stm32System, StopRestoresRunInANewEpochWithUnknownElapsedTime) {
    ASSERT_NE(nullptr, &nx_stm32f407_power_enter);
    ASSERT_EQ(0, Start().primary);
    timer.CNT = 123U;
    nx_clock_domain_t domain{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_construct(&domain, 1U));
    nx_clock_barrier_t barrier{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_close(&domain, &barrier));
    nx_stm32_power_options_t options = {
        {1U << 6U, 0U, 0U}, 8U, true, false, 0U, false, false};
    nx_power_enabled[0] = 1U << 6U;
    nx_stm32_stop_receipt_t receipt{};
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32f407_power_enter(barrier, &options, &receipt));
    EXPECT_TRUE(receipt.waited);
    EXPECT_EQ(1U, deep_waits);
    EXPECT_EQ(123U, receipt.before_us);
    EXPECT_EQ(0U, receipt.after_us);
    EXPECT_LT(receipt.old_epoch, receipt.new_epoch);
    EXPECT_EQ(0U, nx_power_scr);
    EXPECT_EQ(0U, nx_power_tick);
    EXPECT_EQ(0U, mask);
    EXPECT_TRUE(g_nx_stm32_system.started);
}

TEST_F(Stm32System, StopClockRestoreFailureRetainsMasksAndEndsOldEpoch) {
    ASSERT_NE(nullptr, &nx_stm32f407_power_enter);
    ASSERT_EQ(0, Start().primary);
    nx_clock_domain_t domain{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_construct(&domain, 1U));
    nx_clock_barrier_t barrier{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_close(&domain, &barrier));
    nx_stm32_power_options_t options = {
        {1U << 6U, 0U, 0U}, 8U, false, false, 0U, false, false};
    nx_power_enabled[0] = 1U << 6U;
    nx_stm32_stop_receipt_t receipt{};
    fail_deep_restore = true;
    EXPECT_CALL(hardware_mock,
                Fault(NX_FAULT_PLATFORM, NX_ERROR_IO, testing::_, testing::_))
        .WillOnce([](nx_fault_domain_t, nx_result_t, const char*, unsigned) {
            EXPECT_EQ(1U, mask);
            throw ParkSignal{};
        });
    EXPECT_THROW(nx_stm32f407_power_enter(barrier, &options, &receipt),
                 ParkSignal);
    EXPECT_EQ(1U, mask);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_EQ(UINT64_MAX, nx_stm32_system_clock_read(&g_nx_stm32_system));
}

TEST_F(Stm32System, RtcEventOnlyWakeUsesOneWfeWithoutConsumingPendingSource) {
    ASSERT_NE(nullptr, &nx_stm32f407_power_enter);
    ASSERT_EQ(0, Start().primary);
    nx_clock_domain_t domain{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_construct(&domain, 1U));
    nx_clock_barrier_t barrier{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_close(&domain, &barrier));
    nx_stm32_power_options_t options = {
        {0U, 0U, 0U}, 8U, true, false, NX_STM32_WAKE_RTC_TIMER, false, false};
    nx_power_emr = nx_power_rtsr = NX_STM32_WAKE_RTC_TIMER;
    rcc.BDCR =
        RCC_BDCR_RTCEN | RCC_BDCR_RTCSEL_0 | RCC_BDCR_LSEON | RCC_BDCR_LSERDY;
    nx_stm32_stop_receipt_t receipt{};
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32f407_power_enter(barrier, &options, &receipt));
    EXPECT_EQ(1U, event_waits);
    EXPECT_EQ(1U, deep_waits);
    EXPECT_EQ(NX_STM32_WAKE_RTC_TIMER, nx_power_emr);
    EXPECT_EQ(NX_STM32_WAKE_RTC_TIMER, nx_power_rtsr);
}

/** \brief Observe the real deep sleep boundary after the timer epoch ended.
 */
extern "C" void nx_stm32_power_model_wait(void) {
    EXPECT_EQ(1U, mask);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_EQ(UINT64_MAX, nx_stm32_system_clock_read(&g_nx_stm32_system));
    EXPECT_NE(0U, nx_power_scr & SCB_SCR_SLEEPDEEP_Msk);
    ++deep_waits;
    if (before_power_wait)
        before_power_wait();
    if (fail_deep_restore)
        hold_pll = true;
}

/** \brief Observe one WFE; the production path never clears its event latch.
 */
extern "C" void nx_stm32_power_model_event_wait(void) {
    ++event_waits;
    nx_stm32_power_model_wait();
}

/** \brief The model exposes all three incoming Cortex-M interrupt masks. */
extern "C" nx_arch_irq_masks_t nx_arch_irq_masks(void) {
    return {mask, 0U, 0U};
}

/** \brief Bound the external exception number for real clock-domain logic. */
extern "C" uint32_t nx_arch_exception_number(void) {
    return handler ? 16U : 0U;
}

TEST_F(Stm32System, SharedClockAdapterRejectsUnboundAndStoppedEpochs) {
    ASSERT_NE(nullptr, &nx_stm32_system_clock_read);
    EXPECT_EQ(UINT64_MAX, nx_stm32_system_clock_read(nullptr));
    EXPECT_EQ(UINT64_MAX,
              nx_stm32_system_clock_read(reinterpret_cast<void*>(1U)));
    EXPECT_EQ(UINT64_MAX, nx_stm32_system_clock_read(&g_nx_stm32_system));
}

TEST_F(Stm32System, SharedClockAdapterPreservesValidZeroAndMicroseconds) {
    ASSERT_NE(nullptr, &nx_stm32_system_clock_read);
    ASSERT_EQ(0, Start().primary);
    EXPECT_EQ(0U, nx_stm32_system_clock_read(&g_nx_stm32_system));
    timer.CNT = 37U;
    EXPECT_EQ(37U, nx_stm32_system_clock_read(&g_nx_stm32_system));
    g_nx_stm32_system.timer_error = NX_ERROR_IO;
    EXPECT_EQ(UINT64_MAX, nx_stm32_system_clock_read(&g_nx_stm32_system));
    g_nx_stm32_system.timer_error = NX_SUCCESS;
}

TEST_F(Stm32System, SharedClockAdapterChecksEpochAgainAfterSnapshot) {
    ASSERT_NE(nullptr, &nx_stm32_system_clock_read);
    ASSERT_EQ(0, Start().primary);
    before_save = []() { g_nx_stm32_system.timer_error = NX_ERROR_IO; };
    EXPECT_EQ(UINT64_MAX, nx_stm32_system_clock_read(&g_nx_stm32_system));
    before_save = nullptr;
    g_nx_stm32_system.timer_error = NX_SUCCESS;
}

TEST_F(Stm32System, CmsisClockTracksConfirmedStartAndHsiReturn) {
    ASSERT_NE(nullptr, &SystemCoreClock);
    EXPECT_EQ(16000000U, SystemCoreClock);
    ASSERT_EQ(0, Start().primary);
    EXPECT_EQ(NX_TEST_CORE_HZ, SystemCoreClock);
    EXPECT_EQ(g_nx_stm32_system.core_hz, SystemCoreClock);
    ASSERT_EQ(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    EXPECT_EQ(16000000U, SystemCoreClock);
    EXPECT_EQ(g_nx_stm32_system.core_hz, SystemCoreClock);
}

TEST_F(Stm32System, CmsisClockKeepsPllAfterTimerFailureAndRefusedHsiReturn) {
    ASSERT_NE(nullptr, &SystemCoreClock);
    EXPECT_CALL(hardware_mock, Write(&timer.PSC, NX_TEST_TIMER_PSC))
        .WillOnce([](volatile uint32_t*, uint32_t) { hold_hsi = true; });
    auto result = Start();
    EXPECT_NE(0, result.primary);
    EXPECT_NE(0, result.cleanup);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_EQ(RCC_CFGR_SWS_PLL, rcc.CFGR & RCC_CFGR_SWS);
    EXPECT_EQ(NX_TEST_CORE_HZ, SystemCoreClock);
    EXPECT_EQ(g_nx_stm32_system.core_hz, SystemCoreClock);
}

TEST_F(Stm32System, CmsisClockDoesNotClaimPllAfterEarlySourceFailure) {
    ASSERT_NE(nullptr, &SystemCoreClock);
    NX_TEST_HOLD_SOURCE = true;
    auto result = Start();
    EXPECT_NE(0, result.primary);
    EXPECT_EQ(0, result.cleanup);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_EQ(16000000U, SystemCoreClock);
    EXPECT_EQ(g_nx_stm32_system.core_hz, SystemCoreClock);
}

TEST_F(Stm32System, ConfirmedHsiMetadataSurvivesLaterFlashRestoreRefusal) {
    ASSERT_NE(nullptr, &SystemCoreClock);
    const uint32_t saved_flash = flash.ACR;
    ASSERT_EQ(0, Start().primary);
    EXPECT_CALL(hardware_mock, Write(&flash.ACR, saved_flash))
        .WillOnce([](volatile uint32_t*, uint32_t) {})
        .RetiresOnSaturation();
    EXPECT_NE(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    EXPECT_EQ(0U, rcc.CFGR & (RCC_CFGR_SWS | RCC_CFGR_HPRE));
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_NE(0U, g_nx_stm32_system.remaining_effects);
    EXPECT_EQ(16000000U, SystemCoreClock);
    EXPECT_EQ(16000000U, g_nx_stm32_system.core_hz);
}

TEST_F(Stm32System, ForeignClockRefusalPreservesObservedClockMetadata) {
    ASSERT_NE(nullptr, &SystemCoreClock);
    const uint32_t previous_clock = SystemCoreClock;
    SystemCoreClock = 84000000U;
    g_nx_stm32_system.core_hz = 84000000U;
    rcc.CR |= RCC_CR_PLLON | RCC_CR_PLLRDY;
    rcc.CFGR = RCC_CFGR_SW_PLL | RCC_CFGR_SWS_PLL;
    EXPECT_CALL(hardware_mock, Write(testing::_, testing::_)).Times(0);
    EXPECT_NE(0, Start().primary);
    EXPECT_EQ(0U, g_nx_stm32_system.remaining_effects);
    EXPECT_EQ(RCC_CFGR_SWS_PLL, rcc.CFGR & RCC_CFGR_SWS);
    EXPECT_EQ(84000000U, SystemCoreClock);
    EXPECT_EQ(84000000U, g_nx_stm32_system.core_hz);
    SystemCoreClock = previous_clock;
}

TEST_F(Stm32System, HalMillisecondsUseTheOwnedEpochAndModuloWrap) {
    auto* volatile get_tick = &HAL_GetTick;
    ASSERT_NE(nullptr, get_tick);
    EXPECT_EQ(0U, HAL_GetTick());
    ASSERT_EQ(0, Start().primary);
    timer.CNT = 1999U;
    EXPECT_EQ(1U, HAL_GetTick());
    timer.CNT = 2000U;
    EXPECT_EQ(2U, HAL_GetTick());
    g_nx_stm32_system.overflow_us =
        (static_cast<uint64_t>(UINT32_MAX) + 1U) * 1000U;
    timer.CNT = 1234U;
    EXPECT_EQ(1U, HAL_GetTick());
    ASSERT_EQ(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    EXPECT_EQ(0U, HAL_GetTick());
}

TEST_F(Stm32System, PendingAtSourceWithdrawalIsClearedBeforeOwnerRelease) {
    ASSERT_EQ(0, Start().primary);
    EXPECT_CALL(hardware_mock, Write(&timer.DIER, 0U))
        .WillOnce([](volatile uint32_t* reg, uint32_t value) {
            irq_pending = true;
            *reg = value;
        })
        .RetiresOnSaturation();
    EXPECT_EQ(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    EXPECT_FALSE(irq_pending);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_system_preflight(&g_nx_stm32_system));
}

TEST_F(Stm32System, RefusedStopFlagClearRetainsTimerAndClockForRetry) {
    ASSERT_EQ(0, Start().primary);
    timer.SR = TIM_SR_UIF;
    EXPECT_CALL(hardware_mock, Write(&timer.SR, 0U))
        .WillOnce([](volatile uint32_t*, uint32_t) {})
        .RetiresOnSaturation();
    EXPECT_NE(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    EXPECT_NE(0U, g_nx_stm32_system.remaining_effects);
    EXPECT_NE(0U, rcc.APB1ENR & RCC_APB1ENR_TIM2EN);
    EXPECT_EQ(0U, nx_stm32_system_now(&g_nx_stm32_system));
    EXPECT_EQ(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    EXPECT_EQ(0U, g_nx_stm32_system.remaining_effects);
}

TEST_F(Stm32System, AssertedSourceCannotPreventItsOwnWithdrawal) {
    ASSERT_EQ(0, Start().primary);
    timer.SR = TIM_SR_UIF;
    irq_pending = true;
    source_prevents_vector_proof = true;
    EXPECT_EQ(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    EXPECT_EQ(0U, timer.DIER);
    EXPECT_EQ(0U, timer.SR);
    EXPECT_FALSE(irq_pending);
    EXPECT_EQ(0U, g_nx_stm32_system.remaining_effects);
}

TEST_F(Stm32System, SoftwareUpdateIsClearedBeforePublishingAZeroEpoch) {
    ASSERT_EQ(0, Start().primary);
    EXPECT_TRUE(update_observed);
    EXPECT_EQ(0U, timer.SR & TIM_SR_UIF);
    EXPECT_EQ(0U, timer.CNT);
    EXPECT_EQ(0U, nx_stm32_system_now(&g_nx_stm32_system));
    EXPECT_TRUE(g_nx_stm32_system.started);
}

TEST_F(Stm32System, PrescalerShadowUpdateDoesNotEmitAResetModeTrigger) {
    ASSERT_EQ(0, Start().primary);
    EXPECT_TRUE(update_observed);
    EXPECT_FALSE(update_trigger_observed);
    EXPECT_EQ(0U, timer.CR2 & TIM_CR2_MMS);
}

TEST_F(Stm32System, RefusedInitialUpdateClearCannotPublishLiveTime) {
    EXPECT_CALL(hardware_mock, Write(&timer.SR, 0U))
        .WillRepeatedly([](volatile uint32_t*, uint32_t) {});
    auto result = Start();
    EXPECT_NE(0, result.primary);
    EXPECT_NE(0, result.cleanup);
    EXPECT_NE(0U, result.remaining_effects);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_FALSE(irq_enabled);
    EXPECT_EQ(0U, nx_stm32_system_now(&g_nx_stm32_system));
    EXPECT_EQ(0U, timer.CR1 & TIM_CR1_CEN);
    EXPECT_EQ(0U, timer.DIER);
    EXPECT_NE(0U, rcc.APB1ENR & RCC_APB1ENR_TIM2EN);
}

TEST_F(Stm32System, RefusedSoftwareUpdateCannotReuseAnOldUpdateFlag) {
    timer.SR = TIM_SR_UIF;
    EXPECT_CALL(hardware_mock, Write(&timer.EGR, TIM_EGR_UG))
        .WillRepeatedly([](volatile uint32_t*, uint32_t) {});
    auto result = Start();
    EXPECT_NE(0, result.primary);
    EXPECT_EQ(0, result.cleanup);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_FALSE(irq_enabled);
    EXPECT_EQ(0U, nx_stm32_system_now(&g_nx_stm32_system));
}

TEST_F(Stm32System, RefusedInitialCounterZeroCannotPublishLiveTime) {
    EXPECT_CALL(hardware_mock, Write(&timer.CNT, 0U))
        .WillRepeatedly([](volatile uint32_t* reg, uint32_t) { *reg = 37U; });
    auto result = Start();
    EXPECT_NE(0, result.primary);
    EXPECT_EQ(0, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_FALSE(irq_enabled);
    EXPECT_EQ(0U, nx_stm32_system_now(&g_nx_stm32_system));
    EXPECT_EQ(0U, timer.CR1 & TIM_CR1_CEN);
}

TEST_F(Stm32System, CounterFailureRetainsObservedCleanupResponsibility) {
    EXPECT_CALL(hardware_mock, Write(&timer.CNT, 0U))
        .WillRepeatedly([](volatile uint32_t* reg, uint32_t) { *reg = 37U; });
    EXPECT_CALL(hardware_mock, Write(&rcc.APB1ENR, testing::_))
        .WillRepeatedly([](volatile uint32_t* reg, uint32_t value) {
            if ((*reg & RCC_APB1ENR_TIM2EN) != 0U)
                value |= RCC_APB1ENR_TIM2EN;
            *reg = value;
        });
    auto result = Start();
    EXPECT_NE(0, result.primary);
    EXPECT_NE(0, result.cleanup);
    EXPECT_NE(0U, result.remaining_effects);
    EXPECT_NE(0U, rcc.APB1ENR & RCC_APB1ENR_TIM2EN);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_EQ(0U, nx_stm32_system_now(&g_nx_stm32_system));
}

TEST_F(Stm32System, SuccessfulStopRestoresOnlyAcquiredPowerAndClockFields) {
    const uint32_t old_pll = rcc.PLLCFGR;
    const uint32_t old_flash = flash.ACR;
    const uint32_t old_power = power.CR;
    ASSERT_EQ(0, Start().primary);
    power.CR |= PWR_CR_PVDE;
    ASSERT_EQ(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    EXPECT_EQ(old_pll, rcc.PLLCFGR);
    EXPECT_EQ(old_flash, flash.ACR);
    EXPECT_EQ(old_power | PWR_CR_PVDE, power.CR);
    EXPECT_EQ(0U, rcc.APB1ENR & RCC_APB1ENR_PWREN);
}

TEST_F(Stm32System, ExistingPowerGateSurvivesOwnedTransaction) {
    rcc.APB1ENR |= RCC_APB1ENR_PWREN;
    ASSERT_EQ(0, Start().primary);
    ASSERT_EQ(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    EXPECT_NE(0U, rcc.APB1ENR & RCC_APB1ENR_PWREN);
}

TEST_F(Stm32System, HseFailureRestoresSavedFieldsWithoutPublishingTime) {
    const uint32_t old_pll = rcc.PLLCFGR;
    const uint32_t old_flash = flash.ACR;
    const uint32_t old_power = power.CR;
    NX_TEST_HOLD_SOURCE = true;
    auto result = Start();
    EXPECT_NE(0, result.primary);
    EXPECT_EQ(0, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(old_pll, rcc.PLLCFGR);
    EXPECT_EQ(old_flash, flash.ACR);
    EXPECT_EQ(old_power, power.CR);
    EXPECT_EQ(0U, nx_stm32_system_now(&g_nx_stm32_system));
}

class Stm32AdjustedHsiTrim : public Stm32System,
                             public testing::WithParamInterface<uint32_t> {};

TEST_P(Stm32AdjustedHsiTrim, RejectsBeforeAnyWriteOrOwnership) {
    rcc.CR = (rcc.CR & ~(uint32_t)RCC_CR_HSITRIM) |
             (GetParam() << RCC_CR_HSITRIM_Pos) | (0xA5U << RCC_CR_HSICAL_Pos);
    const uint32_t saved_clock = rcc.CR;
    EXPECT_CALL(hardware_mock, Write(testing::_, testing::_)).Times(0);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_system_preflight(&g_nx_stm32_system));
    const auto result = Start();
    EXPECT_NE(0, result.primary);
    EXPECT_EQ(0, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_EQ(0U, g_nx_stm32_system.remaining_effects);
    EXPECT_EQ(0U, g_nx_stm32_system.core_hz);
    EXPECT_FALSE(g_nx_stm32_system.owned_power_clock);
    EXPECT_FALSE(g_nx_stm32_system.power_snapshotted);
    EXPECT_EQ(saved_clock, rcc.CR);
    EXPECT_FALSE(irq_enabled);
    EXPECT_FALSE(irq_pending);
    EXPECT_EQ(0U, mask);
    testing::Mock::VerifyAndClearExpectations(&hardware_mock);
}

INSTANTIATE_TEST_SUITE_P(Adjusted, Stm32AdjustedHsiTrim,
                         testing::Values(0U, 15U, 17U, 31U));

class Stm32FactoryHsiCalibration
    : public Stm32System,
      public testing::WithParamInterface<uint32_t> {};

TEST_P(Stm32FactoryHsiCalibration, DefaultTrimPreservesFactoryCalibration) {
    rcc.CR |= GetParam() << RCC_CR_HSICAL_Pos;
    const uint32_t calibration = rcc.CR & (RCC_CR_HSITRIM | RCC_CR_HSICAL);
    EXPECT_CALL(hardware_mock, Write(testing::_, testing::_)).Times(0);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_system_preflight(&g_nx_stm32_system));
    testing::Mock::VerifyAndClearExpectations(&hardware_mock);
    ASSERT_EQ(0, Start().primary);
    EXPECT_EQ(NX_TEST_CORE_HZ, g_nx_stm32_system.core_hz);
    EXPECT_EQ(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    EXPECT_EQ(16000000U, g_nx_stm32_system.core_hz);
    EXPECT_EQ(calibration, rcc.CR & (RCC_CR_HSITRIM | RCC_CR_HSICAL));
}

INSTANTIATE_TEST_SUITE_P(Factory, Stm32FactoryHsiCalibration,
                         testing::Values(0U, 0x55U, 0xAAU, 0xFFU));

TEST_F(Stm32System, ForeignClockIsRejectedBeforeAnyOwnedWrite) {
    rcc.CR |= RCC_CR_PLLON;
    EXPECT_CALL(hardware_mock, Write(testing::_, testing::_)).Times(0);
    EXPECT_NE(0, Start().primary);
    EXPECT_EQ(0U, g_nx_stm32_system.remaining_effects);
    EXPECT_NE(0U, rcc.CR & RCC_CR_PLLON);
}

TEST_F(Stm32System, ForeignTimerIsRejectedBeforeClockChanges) {
    timer.CR1 = TIM_CR1_CEN;
    EXPECT_CALL(hardware_mock, Write(testing::_, testing::_)).Times(0);
    EXPECT_NE(0, Start().primary);
    EXPECT_EQ(TIM_CR1_CEN, timer.CR1);
    EXPECT_EQ(0U, rcc.CR & RCC_CR_HSEON);
    timer.CR1 = 0U;
}

TEST_F(Stm32System, PendingReservedIrqIsAColdAdmissionConflict) {
    irq_pending = true;
    EXPECT_CALL(hardware_mock, Write(testing::_, testing::_)).Times(0);
    EXPECT_NE(0, Start().primary);
    EXPECT_TRUE(irq_pending);
    irq_pending = false;
}

TEST_F(Stm32System, RetainedTimerModesRejectBeforeClockOrUpdateEffects) {
    EXPECT_CALL(hardware_mock, Write(testing::_, testing::_)).Times(0);
    for (volatile uint32_t* field : {&timer.CR2, &timer.SMCR, &timer.CCMR1,
                                     &timer.CCMR2, &timer.CCER, &timer.DCR}) {
        *field = 1U;
        EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_system_preflight(&g_nx_stm32_system));
        EXPECT_EQ(1U, *field);
        EXPECT_FALSE(update_observed);
        *field = 0U;
    }
}

TEST_F(Stm32System, PoweredForeignTimerKeepsItsRegistersDuringCleanupRetry) {
    bool refuse_gate_drop = true;
    EXPECT_CALL(hardware_mock, Write(&timer.CR1, testing::_)).Times(0);
    EXPECT_CALL(hardware_mock, Write(&timer.DIER, testing::_)).Times(0);
    EXPECT_CALL(hardware_mock, Write(&timer.EGR, testing::_)).Times(0);
    EXPECT_CALL(hardware_mock, Write(&rcc.APB1ENR, testing::_))
        .WillRepeatedly([&](volatile uint32_t* reg, uint32_t value) {
            if ((value & RCC_APB1ENR_TIM2EN) != 0U) {
                *reg = value;
                timer.SMCR = 1U;
            } else if ((*reg & RCC_APB1ENR_TIM2EN) == 0U || !refuse_gate_drop) {
                *reg = value;
            }
        });
    auto result = Start();
    EXPECT_NE(0, result.primary);
    EXPECT_NE(0, result.cleanup);
    EXPECT_EQ(1U, timer.SMCR);
    EXPECT_NE(0U, result.remaining_effects);
    refuse_gate_drop = false;
    EXPECT_EQ(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    EXPECT_EQ(1U, timer.SMCR);
    EXPECT_EQ(0U, g_nx_stm32_system.remaining_effects);
}

TEST_F(Stm32System, RefusedTimerResetCannotPublishAnUpdateOrLiveEpoch) {
    EXPECT_CALL(hardware_mock, Write(&rcc.APB1RSTR, testing::_))
        .WillRepeatedly([](volatile uint32_t*, uint32_t) {});
    EXPECT_CALL(hardware_mock, Write(&timer.EGR, testing::_)).Times(0);
    auto result = Start();
    EXPECT_NE(0, result.primary);
    EXPECT_EQ(0, result.cleanup);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_FALSE(update_observed);
}

TEST_F(Stm32System, ForeignAssertedTimerResetIsNotAnAvailableResource) {
    rcc.APB1RSTR = RCC_APB1RSTR_TIM2RST;
    EXPECT_CALL(hardware_mock, Write(testing::_, testing::_)).Times(0);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_system_preflight(&g_nx_stm32_system));
    EXPECT_EQ(RCC_APB1RSTR_TIM2RST, rcc.APB1RSTR);
}

TEST_F(Stm32System, RefusedOwnedResetReleaseRetainsCleanupUntilRetry) {
    bool refuse_reset_release = true;
    EXPECT_CALL(hardware_mock, Write(&rcc.APB1RSTR, testing::_))
        .WillRepeatedly([&](volatile uint32_t* reg, uint32_t value) {
            if ((value & RCC_APB1RSTR_TIM2RST) != 0U || !refuse_reset_release)
                *reg = value;
        });
    auto result = Start();
    EXPECT_NE(0, result.primary);
    EXPECT_NE(0, result.cleanup);
    EXPECT_NE(0U, result.remaining_effects);
    EXPECT_NE(0U, rcc.APB1RSTR & RCC_APB1RSTR_TIM2RST);
    refuse_reset_release = false;
    EXPECT_EQ(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    EXPECT_EQ(0U, rcc.APB1RSTR & RCC_APB1RSTR_TIM2RST);
    EXPECT_EQ(0U, g_nx_stm32_system.remaining_effects);
}

TEST_F(Stm32System, MaskedOrUnprivilegedStartHasNoEffects) {
    EXPECT_CALL(hardware_mock, Write(testing::_, testing::_)).Times(0);
    mask = 1U;
    EXPECT_NE(0, Start().primary);
    mask = 0U;
    privileged = false;
    EXPECT_NE(0, Start().primary);
    privileged = true;
    EXPECT_EQ(0U, g_nx_stm32_system.remaining_effects);
}

TEST_F(Stm32System, RefusedFlashLatencyCannotPublishAHighSpeedClock) {
    EXPECT_CALL(hardware_mock, Write(&flash.ACR, testing::_))
        .WillRepeatedly([](volatile uint32_t*, uint32_t) {});
    auto result = Start();
    EXPECT_NE(0, result.primary);
    EXPECT_EQ(0U, rcc.CFGR & RCC_CFGR_SWS);
    EXPECT_FALSE(g_nx_stm32_system.started);
}

TEST_F(Stm32System, RefusedTimerGateRetainsOnlyFailedCleanupResponsibility) {
    EXPECT_CALL(hardware_mock, Write(&rcc.APB1ENR, testing::_))
        .WillRepeatedly([&](volatile uint32_t* reg, uint32_t value) {
            *reg = value & ~(uint32_t)RCC_APB1ENR_TIM2EN;
        });
    auto result = Start();
    EXPECT_NE(0, result.primary);
    EXPECT_EQ(0, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(0U, nx_stm32_system_now(&g_nx_stm32_system));
}

TEST_F(Stm32System, RefusedIrqWithdrawalKeepsOwnershipButEndsLiveTime) {
    ASSERT_EQ(0, Start().primary);
    timer.CNT = 13U;
    refuse_irq_disable = true;
    EXPECT_NE(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    EXPECT_NE(0U, g_nx_stm32_system.remaining_effects);
    EXPECT_EQ(0U, nx_stm32_system_now(&g_nx_stm32_system));
    EXPECT_TRUE(irq_enabled);
}

TEST_F(Stm32System, RetainedTimerStillAcknowledgesOverflowWithoutLiveEpoch) {
    ASSERT_EQ(0, Start().primary);
    refuse_irq_disable = true;
    ASSERT_NE(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    timer.SR = TIM_SR_UIF;
    nx_stm32_system_timer_irq(&g_nx_stm32_system);
    EXPECT_EQ(0U, timer.SR & TIM_SR_UIF);
    EXPECT_EQ(0U, nx_stm32_system_now(&g_nx_stm32_system));
    EXPECT_NE(0U, g_nx_stm32_system.remaining_effects);
}

TEST_F(Stm32System, IsolatedAckFaultStillObservesTerminalOwnership) {
    ASSERT_EQ(0, Start().primary);
    timer.SR = TIM_SR_UIF;
    timer.CNT = 7U;
    irq_active = true;
    EXPECT_CALL(hardware_mock, Write(&timer.SR, ~(uint32_t)TIM_SR_UIF))
        .WillRepeatedly([](volatile uint32_t*, uint32_t) {});
    EXPECT_CALL(hardware_mock,
                Fault(NX_FAULT_PLATFORM, NX_ERROR_IO, testing::_, testing::_))
        .WillOnce([](nx_fault_domain_t, nx_result_t, const char* file,
                     unsigned line) {
            EXPECT_NE(nullptr, file);
            EXPECT_NE(0U, line);
            EXPECT_TRUE(nx_arch_irq_is_masked());
            throw ParkSignal{};
        });
    EXPECT_THROW(nx_stm32_system_timer_irq(&g_nx_stm32_system), ParkSignal);
    EXPECT_EQ(0U, g_nx_stm32_system.overflow_us);
    EXPECT_EQ(NX_ERROR_IO, g_nx_stm32_system.timer_error);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_NE(0U, g_nx_stm32_system.remaining_effects & 2U);
    EXPECT_EQ(0U, timer.DIER);
    EXPECT_EQ(0U, timer.CR1 & TIM_CR1_CEN);
    EXPECT_FALSE(irq_enabled);
    EXPECT_EQ(0U, nx_stm32_system_now(&g_nx_stm32_system));
    EXPECT_TRUE(nx_arch_irq_is_masked());
}

TEST_F(Stm32System, ReturningObserverCannotResumeIsolatedTimerProgress) {
    EXPECT_TRUE(nexus_test::ObserveTerminal([&] {
        if (Start().primary != 0)
            _exit(90);
        timer.SR = TIM_SR_UIF;
        EXPECT_CALL(hardware_mock, Write(&timer.SR, ~(uint32_t)TIM_SR_UIF))
            .WillOnce([](volatile uint32_t*, uint32_t) {});
        EXPECT_CALL(hardware_mock, Fault(NX_FAULT_PLATFORM, NX_ERROR_IO,
                                         testing::_, testing::_))
            .WillOnce([](nx_fault_domain_t, nx_result_t, const char* file,
                         unsigned line) {
                const bool valid =
                    file != nullptr && line != 0U && mask == 1U &&
                    !irq_enabled &&
                    g_nx_stm32_system.timer_error == NX_ERROR_IO &&
                    !g_nx_stm32_system.started &&
                    g_nx_stm32_system.remaining_effects != 0U &&
                    g_nx_stm32_system.overflow_us == 0U &&
                    g_nx_stm32_system.timer->DIER == 0U &&
                    (g_nx_stm32_system.timer->CR1 & TIM_CR1_CEN) == 0U;
                nexus_test::RecordTerminalObservation(valid);
            });
        nx_stm32_system_timer_irq(&g_nx_stm32_system);
        _exit(99);
    }));
}

TEST_F(Stm32System, UnisolatableAckFaultRetainsMaskBeforeTerminalObservation) {
    ASSERT_EQ(0, Start().primary);
    timer.SR = TIM_SR_UIF;
    EXPECT_CALL(hardware_mock, Write(&timer.SR, ~(uint32_t)TIM_SR_UIF))
        .WillRepeatedly([](volatile uint32_t*, uint32_t) {});
    EXPECT_CALL(hardware_mock, Write(&timer.DIER, 0U))
        .WillRepeatedly([](volatile uint32_t*, uint32_t) {});
    EXPECT_CALL(hardware_mock,
                Fault(NX_FAULT_PLATFORM, NX_ERROR_IO, testing::_, testing::_))
        .WillOnce([](nx_fault_domain_t, nx_result_t, const char*, unsigned) {
            EXPECT_EQ(1U, mask);
            throw ParkSignal{};
        });
    EXPECT_THROW(nx_stm32_system_timer_irq(&g_nx_stm32_system), ParkSignal);
    EXPECT_EQ(1U, mask);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_NE(0U, g_nx_stm32_system.remaining_effects & 2U);
    EXPECT_EQ(0U, g_nx_stm32_system.overflow_us);
}

TEST_F(Stm32System, RefusedCounterIsolationRetainsTerminalOwnership) {
    ASSERT_EQ(0, Start().primary);
    timer.SR = TIM_SR_UIF;
    irq_active = true;
    EXPECT_CALL(hardware_mock, Write(&timer.SR, ~(uint32_t)TIM_SR_UIF))
        .WillOnce([](volatile uint32_t*, uint32_t) {});
    EXPECT_CALL(hardware_mock, Write(&timer.DIER, 0U)).Times(1);
    EXPECT_CALL(hardware_mock, Write(&timer.CR1, 0U))
        .WillOnce([](volatile uint32_t*, uint32_t) {});
    EXPECT_CALL(hardware_mock,
                Fault(NX_FAULT_PLATFORM, NX_ERROR_IO, testing::_, testing::_))
        .WillOnce([](nx_fault_domain_t, nx_result_t, const char*, unsigned) {
            EXPECT_EQ(1U, mask);
            throw ParkSignal{};
        });
    EXPECT_THROW(nx_stm32_system_timer_irq(&g_nx_stm32_system), ParkSignal);
    EXPECT_EQ(1U, mask);
    EXPECT_EQ(0U, timer.DIER);
    EXPECT_NE(0U, timer.CR1 & TIM_CR1_CEN);
    EXPECT_FALSE(irq_enabled);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_EQ(NX_ERROR_IO, g_nx_stm32_system.timer_error);
    EXPECT_NE(0U, g_nx_stm32_system.remaining_effects & 2U);
    EXPECT_EQ(0U, g_nx_stm32_system.overflow_us);
}

TEST_F(Stm32System, RefusedVectorIsolationRetainsTerminalOwnership) {
    ASSERT_EQ(0, Start().primary);
    timer.SR = TIM_SR_UIF;
    irq_active = true;
    refuse_irq_disable = true;
    EXPECT_CALL(hardware_mock, Write(&timer.SR, ~(uint32_t)TIM_SR_UIF))
        .WillOnce([](volatile uint32_t*, uint32_t) {});
    EXPECT_CALL(hardware_mock, Write(&timer.DIER, 0U)).Times(1);
    EXPECT_CALL(hardware_mock, Write(&timer.CR1, 0U)).Times(1);
    EXPECT_CALL(hardware_mock,
                Fault(NX_FAULT_PLATFORM, NX_ERROR_IO, testing::_, testing::_))
        .WillOnce([](nx_fault_domain_t, nx_result_t, const char*, unsigned) {
            EXPECT_EQ(1U, mask);
            throw ParkSignal{};
        });
    EXPECT_THROW(nx_stm32_system_timer_irq(&g_nx_stm32_system), ParkSignal);
    EXPECT_EQ(1U, mask);
    EXPECT_EQ(0U, timer.DIER);
    EXPECT_EQ(0U, timer.CR1 & TIM_CR1_CEN);
    EXPECT_TRUE(irq_enabled);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_EQ(NX_ERROR_IO, g_nx_stm32_system.timer_error);
    EXPECT_NE(0U, g_nx_stm32_system.remaining_effects & 2U);
    EXPECT_EQ(0U, g_nx_stm32_system.overflow_us);
}

TEST_F(Stm32System, FaultBeforeIrqSaveCannotEscapeAsAStaleTimeSnapshot) {
    ASSERT_EQ(0, Start().primary);
    timer.SR = TIM_SR_UIF;
    timer.CNT = 7U;
    EXPECT_CALL(hardware_mock, Write(&timer.SR, ~(uint32_t)TIM_SR_UIF))
        .WillRepeatedly([](volatile uint32_t*, uint32_t) {});
    before_save = []() {
        irq_active = true;
        nx_stm32_system_timer_irq(&g_nx_stm32_system);
        irq_active = false;
    };
    EXPECT_CALL(hardware_mock,
                Fault(NX_FAULT_PLATFORM, NX_ERROR_IO, testing::_, testing::_))
        .WillOnce([](nx_fault_domain_t, nx_result_t, const char* file,
                     unsigned line) {
            EXPECT_NE(nullptr, file);
            EXPECT_NE(0U, line);
            EXPECT_TRUE(nx_arch_irq_is_masked());
            throw ParkSignal{};
        });
    EXPECT_THROW(nx_stm32_system_now(&g_nx_stm32_system), ParkSignal);
    EXPECT_EQ(1U, mask);
    EXPECT_EQ(NX_ERROR_IO, g_nx_stm32_system.timer_error);
    EXPECT_FALSE(g_nx_stm32_system.started);
}

/** \brief Advance only external hardware readiness and source acknowledgement.
 */
extern "C" void nx_stm32_system_model_poll(void* raw) {
    auto* system = static_cast<nx_stm32_system_t*>(raw);
    auto* rcc = system->rcc;
    if (hold_hsi)
        rcc->CR &= ~(uint32_t)RCC_CR_HSIRDY;
    else if ((rcc->CR & RCC_CR_HSION) != 0U)
        rcc->CR |= RCC_CR_HSIRDY;
    if ((rcc->CR & RCC_CR_HSEON) != 0U && !hold_hse)
        rcc->CR |= RCC_CR_HSERDY;
    else
        rcc->CR &= ~(uint32_t)RCC_CR_HSERDY;
    if ((rcc->CR & RCC_CR_PLLON) != 0U && !hold_pll)
        rcc->CR |= RCC_CR_PLLRDY;
    else
        rcc->CR &= ~(uint32_t)RCC_CR_PLLRDY;
    if (!hold_switch)
        rcc->CFGR = (rcc->CFGR & ~(uint32_t)RCC_CFGR_SWS) |
                    ((rcc->CFGR & (uint32_t)RCC_CFGR_SW) << 2U);
}

/** \brief GoogleMock observes writes at the hardware port, not inside logic. */
extern "C" void nx_stm32_system_model_write(volatile uint32_t* reg,
                                            uint32_t value) {
    if (before_write)
        before_write(reg, value);
    hardware_port->Write(reg, value);
}

/** \brief The RTC owner is an external typed dependency of the power owner.
 */
extern "C" nx_result_t
nx_rtc_port_standby_validate(const nx_rtc_port_t* port,
                             const nx_rtc_standby_t* receipt) {
    EXPECT_EQ(expected_rtc, port);
    EXPECT_EQ(expected_rtc_receipt, receipt);
    return rtc_calls == nullptr ? NX_ERROR_STATE : rtc_calls->Call(1U);
}
/** \brief Observe the actual commit call after Power has cleared only WUF.
 */
extern "C" nx_result_t nx_rtc_port_standby_commit(const nx_rtc_port_t* port,
                                                  nx_rtc_standby_t* receipt) {
    EXPECT_EQ(expected_rtc, port);
    EXPECT_EQ(expected_rtc_receipt, receipt);
    return rtc_calls == nullptr ? NX_ERROR_STATE : rtc_calls->Call(2U);
}
/** \brief Refuse or release the external RTC receipt independently of Power.
 */
extern "C" nx_result_t nx_rtc_port_standby_restore(const nx_rtc_port_t* port,
                                                   nx_rtc_standby_t* receipt) {
    EXPECT_EQ(expected_rtc, port);
    EXPECT_EQ(expected_rtc_receipt, receipt);
    return rtc_calls == nullptr ? NX_ERROR_STATE : rtc_calls->Call(3U);
}

/** \brief Model existing enabled, pending and active vector owners. */
extern "C" bool nx_stm32_system_model_irq_available(int) {
    return !irq_enabled && !irq_pending && !irq_active;
}

/** \brief Refuse an actual modeled withdrawal without pretending it succeeded.
 */
extern "C" int nx_stm32_system_model_irq_control(int, bool enabled) {
    if (!enabled && refuse_irq_disable)
        return -1;
    if (!enabled && source_prevents_vector_proof &&
        (g_nx_stm32_system.timer->DIER & TIM_DIER_UIE) != 0U &&
        (g_nx_stm32_system.timer->SR & TIM_SR_UIF) != 0U) {
        irq_pending = true;
        return -1;
    }
    irq_enabled = enabled;
    irq_pending = false;
    return 0;
}

/** \brief Save the injected incoming CPU mask for one short metadata span. */
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    if (before_save != nullptr && mask == 0U) {
        auto callback = before_save;
        before_save = nullptr;
        callback();
    }
    nx_arch_irq_state_t previous = {mask};
    mask = 1U;
    return previous;
}
/** \brief Restore the exact injected CPU mask. */
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    mask = previous.value;
}
/** \brief Query injected privilege at the external CPU boundary. */
extern "C" bool nx_arch_is_privileged(void) {
    return privileged;
}
/** \brief Query injected exception context. */
extern "C" bool nx_arch_in_isr(void) {
    return handler;
}
/** \brief Query the incoming mask without changing it. */
extern "C" bool nx_arch_irq_is_masked(void) {
    return mask != 0U;
}
/** \brief Synchronous host register writes do not establish physical timing. */
extern "C" void nx_arch_dsb(void) {
}
/** \brief The host model has no Cortex-M instruction pipeline. */
extern "C" void nx_arch_isb(void) {
}

/** \brief The host-only observer stops before the retained terminal loop. */
extern "C" void nx_fault_observe(nx_fault_domain_t domain, nx_result_t reason,
                                 const char* file, unsigned line) {
    hardware_port->Fault(domain, reason, file, line);
}
