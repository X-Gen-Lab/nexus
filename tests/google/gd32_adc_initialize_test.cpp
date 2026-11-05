/**
 * \file            gd32_adc_initialize_test.cpp
 * \brief           Real GD32 ADC cold-start failures restore acquired effects
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "gd32f470_control_cold.h"
#include "gd32f470_provider.h"
#include "gd32f4xx.h"
#include "nexus/core/fault.h"
#include "private/system.h"
}
#include <array>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
uint32_t s_host_ordering;
enum class Fault {
    None,
    Stability,
    Reset,
    Calibration,
    Frozen,
    ResetFrozen,
    CalibrationFrozen
};
enum class CleanupFault {
    None,
    AdcDisable,
    CommonRestore,
    PinRestore,
    PinPullRestore,
    AdcGate,
    GpioGate
};
struct ParkSignal {};

/** \brief SDK boundaries are mocked; production MMIO algorithms remain real. */
class ADCBoundary {
  public:
    MOCK_METHOD(void, ClockEnable, (rcu_periph_enum));
    MOCK_METHOD(void, ClockDisable, (rcu_periph_enum));
    MOCK_METHOD(void, ConfigurePin, (uint32_t, uint32_t, uint32_t, uint32_t));
    MOCK_METHOD(void, Reset, ());
    MOCK_METHOD(void, Prescaler, (uint32_t));
    MOCK_METHOD(void, Observe,
                (nx_fault_domain_t, nx_result_t, const char*, unsigned));
};
ADCBoundary* s_boundary;
Fault s_fault;
nx_time_us_t s_now;
unsigned s_powered_ticks;
bool s_isr;
uint32_t s_mask;
bool s_neighbor_write;
bool s_gated_pins;
unsigned s_reset_calibrations;
unsigned s_calibrations;
bool s_conversion_works;
bool s_refuse_prescaler;
bool s_delayed_eoc;
bool s_frozen_eoc;
unsigned s_eoc_hold_reads;
unsigned s_early_data_reads;
unsigned s_data_reads;
nx_time_us_t s_eoc_time;
CleanupFault s_cleanup_fault;
bool s_cleanup_active;
bool s_cleanup_injected;
constexpr uint32_t kNeighborMask = 3U << 30U;
} /* namespace */

/** \brief SDK clock access uses the real encoded RCU register and bit. */
extern "C" void rcu_periph_clock_enable(rcu_periph_enum peripheral) {
    s_boundary->ClockEnable(peripheral);
    REG32(RCU + (static_cast<uint32_t>(peripheral) >> 6U)) |=
        1U << (static_cast<uint32_t>(peripheral) & 31U);
}

/** \brief Release only the peripheral clock actually requested. */
extern "C" void rcu_periph_clock_disable(rcu_periph_enum peripheral) {
    s_boundary->ClockDisable(peripheral);
    if (s_cleanup_active && s_cleanup_fault == CleanupFault::AdcGate &&
        peripheral == RCU_ADC0) {
        s_cleanup_injected = true;
        return;
    }
    REG32(RCU + (static_cast<uint32_t>(peripheral) >> 6U)) &=
        ~(1U << (static_cast<uint32_t>(peripheral) & 31U));
}

/** \brief Match the SDK's two modified fields, checking powered GPIO access. */
extern "C" void gpio_mode_set(uint32_t gpio, uint32_t mode, uint32_t pull,
                              uint32_t pins) {
    s_boundary->ConfigurePin(gpio, mode, pull, pins);
    EXPECT_NE(RCU_AHB1EN & (1U << ((gpio - GPIOA) / 0x400U)), 0U);
    for (unsigned pin = 0U; pin < 16U; ++pin) {
        if ((pins & (1U << pin)) != 0U) {
            uint32_t mask = 3U << (pin * 2U);
            GPIO_CTL(gpio) = (GPIO_CTL(gpio) & ~mask) | (mode << (pin * 2U));
            GPIO_PUD(gpio) = (GPIO_PUD(gpio) & ~mask) | (pull << (pin * 2U));
        }
    }
}

/** \brief Official ADCRST resets all ADC instances and their common state. */
extern "C" void adc_deinit(void) {
    s_boundary->Reset();
    std::memset(reinterpret_cast<void*>(ADC0), 0, 0x50U);
    std::memset(reinterpret_cast<void*>(ADC1), 0, 0x50U);
    std::memset(reinterpret_cast<void*>(ADC2), 0, 0x50U);
    ADC_SYNCCTL = 0U;
}

/** \brief Official divider configuration changes only its common bitfield. */
extern "C" void adc_clock_config(uint32_t prescaler) {
    s_boundary->Prescaler(prescaler);
    if (!s_refuse_prescaler) {
        ADC_SYNCCTL =
            (ADC_SYNCCTL & ~static_cast<uint32_t>(ADC_SYNCCTL_ADCCK)) |
            prescaler;
    }
}

/** \brief Model the erratum's data latency independently of the EOC flag. */
extern "C" uint16_t adc_routine_data_read(uint32_t peripheral) {
    ++s_data_reads;
    if (s_delayed_eoc && s_now - s_eoc_time < 2U) {
        ++s_early_data_reads;
        return UINT16_MAX;
    }
    return static_cast<uint16_t>(ADC_RDATA(peripheral));
}

/** \brief Independently inject each cold-start wait, without a success stub. */
extern "C" nx_time_us_t nx_time_now_us(void) {
    if ((ADC_CTL1(ADC0) & ADC_CTL1_ADCON) != 0U) {
        ++s_powered_ticks;
        if (s_neighbor_write) {
            GPIO_CTL(GPIOA) |= kNeighborMask;
            GPIO_PUD(GPIOA) = (GPIO_PUD(GPIOA) & ~kNeighborMask) | (1U << 30U);
            RCU_AHB1EN |= RCU_AHB1EN_PIEN;
            RCU_APB2EN |= RCU_APB2EN_SYSCFGEN;
            s_neighbor_write = false;
        }
        if (s_fault == Fault::Frozen ||
            (s_fault == Fault::ResetFrozen &&
             (ADC_CTL1(ADC0) & ADC_CTL1_RSTCLB) != 0U) ||
            (s_fault == Fault::CalibrationFrozen &&
             (ADC_CTL1(ADC0) & ADC_CTL1_CLB) != 0U)) {
            return s_now;
        }
        if (s_fault == Fault::Stability && s_powered_ticks == 3U) {
            s_now = 500U;
            s_cleanup_active = true;
        }
        if (s_fault != Fault::Reset && s_fault != Fault::ResetFrozen) {
            if ((ADC_CTL1(ADC0) & ADC_CTL1_RSTCLB) != 0U) {
                ++s_reset_calibrations;
            }
            ADC_CTL1(ADC0) &= ~ADC_CTL1_RSTCLB;
        }
        if (s_fault != Fault::Calibration &&
            s_fault != Fault::CalibrationFrozen) {
            if ((ADC_CTL1(ADC0) & ADC_CTL1_CLB) != 0U) {
                ++s_calibrations;
            }
            ADC_CTL1(ADC0) &= ~ADC_CTL1_CLB;
        }
        if ((ADC_CTL1(ADC0) & ADC_CTL1_SWRCST) != 0U && s_conversion_works) {
            ADC_RDATA(ADC0) = 100U + (ADC_RSQ2(ADC0) & 31U);
            ADC_STAT(ADC0) |= ADC_STAT_EOC;
            ADC_CTL1(ADC0) &= ~ADC_CTL1_SWRCST;
            if (s_delayed_eoc) {
                s_eoc_time = s_now;
                s_eoc_hold_reads = 2U;
                return s_now;
            }
        }
        if (s_delayed_eoc && (ADC_STAT(ADC0) & ADC_STAT_EOC) != 0U &&
            (s_frozen_eoc || s_eoc_hold_reads != 0U)) {
            if (s_eoc_hold_reads != 0U) {
                --s_eoc_hold_reads;
            }
            return s_now;
        }
    }
    return ++s_now;
}

/** \brief Context is external to the initializer's register transaction. */
extern "C" bool nx_arch_in_isr(void) {
    return s_isr;
}

/** \brief Admission must preserve an incoming mask. */
extern "C" bool nx_arch_irq_is_masked(void) {
    return s_mask != 0U;
}

/** \brief All CPU metadata uses the fixture's single incoming mask domain. */
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t previous = {s_mask};
    s_mask = 1U;
    return previous;
}

/** \brief Restore exactly the fixture's incoming interrupt state. */
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    s_mask = previous.value;
}

/** \brief No priority or fault-mask injection is claimed by this fixture. */
extern "C" nx_arch_irq_masks_t nx_arch_irq_masks(void) {
    return {s_mask, 0U, 0U};
}

/** \brief The fixture models privileged Thread or an external exception. */
extern "C" uint32_t nx_arch_exception_number(void) {
    return s_isr ? 16U : 0U;
}

/** \brief CPU privilege faults have a separate Arch contract fixture. */
extern "C" bool nx_arch_is_privileged(void) {
    return true;
}

/** \brief Host ordering does not establish physical peripheral completion. */
extern "C" void nx_arch_dmb(void) {
    (void)__atomic_fetch_add(&s_host_ordering, 0U, __ATOMIC_SEQ_CST);
}

/** \brief Host register fixtures expose no physical DWT counter. */
extern "C" bool nx_arch_cycle_snapshot(uint32_t* cycles) {
    (void)cycles;
    return false;
}

/** \brief Host stores complete synchronously; firmware supplies its barrier. */
extern "C" void nx_arch_dsb(void) {
    if (s_gated_pins && (RCU_AHB1EN & (RCU_AHB1EN_PAEN | RCU_AHB1EN_PCEN)) ==
                            (RCU_AHB1EN_PAEN | RCU_AHB1EN_PCEN)) {
        /* Clock-gated GPIO reads do not expose the retained register shadow.
         * It becomes observable only after the clock enable is ordered.
         */
        GPIO_CTL(GPIOA) = UINT32_C(0x55555555);
        GPIO_PUD(GPIOA) = UINT32_C(0xAAAAAAAA);
        GPIO_CTL(GPIOC) = UINT32_C(0x55555555);
        GPIO_PUD(GPIOC) = UINT32_C(0xAAAAAAAA);
        s_gated_pins = false;
    }
    if (!s_cleanup_active || s_cleanup_injected) {
        return;
    }
    if (s_cleanup_fault == CleanupFault::AdcDisable && ADC_CTL1(ADC0) == 0U) {
        ADC_CTL1(ADC0) = ADC_CTL1_ADCON;
        s_cleanup_injected = true;
    } else if (s_cleanup_fault == CleanupFault::CommonRestore &&
               ADC_SYNCCTL == (ADC_ADCCK_PCLK2_DIV4 | ADC_SYNCCTL_VBATEN)) {
        ADC_SYNCCTL &= ~ADC_SYNCCTL_VBATEN;
        s_cleanup_injected = true;
    } else if ((s_cleanup_fault == CleanupFault::PinRestore ||
                s_cleanup_fault == CleanupFault::PinPullRestore) &&
               (GPIO_CTL(GPIOA) & 15U) == 5U &&
               (GPIO_PUD(GPIOA) & 15U) == 10U) {
        if (s_cleanup_fault == CleanupFault::PinRestore) {
            GPIO_CTL(GPIOA) ^= 1U;
        } else {
            GPIO_PUD(GPIOA) ^= 1U;
        }
        s_cleanup_injected = true;
    } else if (s_cleanup_fault == CleanupFault::GpioGate &&
               (RCU_AHB1EN & RCU_AHB1EN_PAEN) == 0U) {
        RCU_AHB1EN |= RCU_AHB1EN_PAEN;
        s_cleanup_injected = true;
    }
}

/** \brief The test intercepts a terminal observer; firmware cannot unwind. */
extern "C" void nx_fault_observe(nx_fault_domain_t domain, nx_result_t reason,
                                 const char* file, unsigned line) {
    s_boundary->Observe(domain, reason, file, line);
}

/** \brief The register fixture has no physical instruction pipeline. */
extern "C" void nx_arch_isb(void) {
}

class GD32ADCInitialize : public ::testing::Test {
  protected:
    ::testing::NiceMock<ADCBoundary> boundary;
    nx_gd32_adc_state_t state = {};
    const uint8_t channels[5] = {0U, 1U, 8U, 10U, 0U};
    std::array<uint32_t, 3> initial_mode = {};
    std::array<uint32_t, 3> initial_pull = {};
    void* mapping = MAP_FAILED;

    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(UINT32_C(0x40000000)));
        s_boundary = &boundary;
        s_fault = Fault::None;
        s_now = 100U;
        s_powered_ticks = 0U;
        s_isr = false;
        s_mask = 0U;
        s_neighbor_write = false;
        s_gated_pins = false;
        s_reset_calibrations = 0U;
        s_calibrations = 0U;
        s_conversion_works = false;
        s_refuse_prescaler = false;
        s_delayed_eoc = false;
        s_frozen_eoc = false;
        s_eoc_hold_reads = 0U;
        s_early_data_reads = 0U;
        s_data_reads = 0U;
        s_eoc_time = 0U;
        s_cleanup_fault = CleanupFault::None;
        s_cleanup_active = false;
        s_cleanup_injected = false;
        for (unsigned i = 0U; i < 3U; ++i) {
            initial_mode[i] = UINT32_C(0x55555555);
            initial_pull[i] = UINT32_C(0xAAAAAAAA);
            GPIO_CTL(GPIOA + i * 0x400U) = initial_mode[i];
            GPIO_PUD(GPIOA + i * 0x400U) = initial_pull[i];
        }
        RCU_AHB1EN = RCU_AHB1EN_PBEN | RCU_AHB1EN_DMA1EN;
        RCU_APB2EN = RCU_APB2EN_USART0EN;
        ADC_SYNCCTL = ADC_ADCCK_PCLK2_DIV4 | ADC_SYNCCTL_VBATEN;
    }

    void TearDown() override {
        if (state.initialized) {
            s_isr = false;
            s_mask = 0U;
            EXPECT_EQ(nx_gd32_adc_stop(&state), NX_SUCCESS);
        }
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(munmap(mapping, 0x80000), 0);
        }
        s_boundary = nullptr;
    }

    void ExpectRestored() {
        for (unsigned i = 0U; i < 3U; ++i) {
            EXPECT_EQ(GPIO_CTL(GPIOA + i * 0x400U), initial_mode[i]);
            EXPECT_EQ(GPIO_PUD(GPIOA + i * 0x400U), initial_pull[i]);
        }
        EXPECT_EQ(RCU_AHB1EN, RCU_AHB1EN_PBEN | RCU_AHB1EN_DMA1EN);
        EXPECT_EQ(RCU_APB2EN, RCU_APB2EN_USART0EN);
        EXPECT_EQ(ADC_SYNCCTL, ADC_ADCCK_PCLK2_DIV4 | ADC_SYNCCTL_VBATEN);
        EXPECT_EQ(ADC_CTL1(ADC0), 0U);
        EXPECT_FALSE(state.initialized);
    }

    void FailAndRetry(Fault fault, nx_result_t expected) {
        s_fault = fault;
        EXPECT_CALL(boundary, ClockEnable(RCU_ADC0)).Times(1);
        EXPECT_CALL(boundary, ClockDisable(RCU_ADC0)).Times(1);
        EXPECT_CALL(boundary, Reset()).Times(1);
        EXPECT_CALL(boundary, Prescaler(ADC_ADCCK_PCLK2_DIV8)).Times(1);
        nx_gd32_adc_state_t before = state;
        EXPECT_EQ(nx_gd32_adc_initialize(&state, channels, 5U, 3300U, 200U),
                  expected);
        EXPECT_EQ(std::memcmp(&state, &before, sizeof(state)), 0);
        EXPECT_LE(s_powered_ticks, 2000020U);
        ExpectRestored();
        ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(&boundary));
        s_fault = Fault::None;
        s_powered_ticks = 0U;
        ASSERT_EQ(
            nx_gd32_adc_initialize(&state, channels, 5U, 3300U, s_now + 100U),
            NX_SUCCESS);
        EXPECT_TRUE(state.initialized);
    }

    void PowerOffAfterFailedPoll() {
        ASSERT_EQ(nx_gd32_adc_initialize(&state, channels, 5U, 3300U, 200U),
                  NX_SUCCESS);
        uint16_t samples[5] = {};
        size_t count = 99U;
        ASSERT_EQ(nx_gd32_adc_sample(&state, samples, 5U, s_now + 20U, &count),
                  NX_ERROR_TIMEOUT);
        EXPECT_EQ(count, 0U);
        ASSERT_EQ(ADC_CTL1(ADC0) & ADC_CTL1_ADCON, 0U);
        EXPECT_FALSE(state.active);
    }
};

class GD32ADCCleanupFailure
    : public GD32ADCInitialize,
      public ::testing::WithParamInterface<CleanupFault> {};

TEST_P(GD32ADCCleanupFailure, RefusedCleanupParksWithoutReturningAnOwner) {
    s_fault = Fault::Stability;
    s_cleanup_fault = GetParam();
    EXPECT_CALL(boundary, Observe(NX_FAULT_PLATFORM, NX_ERROR_IO, ::testing::_,
                                  ::testing::_))
        .WillOnce([](nx_fault_domain_t, nx_result_t, const char* file,
                     unsigned line) {
            EXPECT_TRUE(s_cleanup_injected);
            EXPECT_EQ(s_mask, 1U);
            EXPECT_NE(file, nullptr);
            EXPECT_GT(line, 0U);
            throw ParkSignal{};
        });
    EXPECT_THROW(nx_gd32_adc_initialize(&state, channels, 5U, 3300U, 200U),
                 ParkSignal);
    EXPECT_TRUE(s_cleanup_injected);
    EXPECT_EQ(s_mask, 1U);
    EXPECT_FALSE(state.initialized);
    if (GetParam() != CleanupFault::GpioGate) {
        EXPECT_NE(RCU_AHB1EN & RCU_AHB1EN_PAEN, 0U);
    }
}

INSTANTIATE_TEST_SUITE_P(
    RetainedEffects, GD32ADCCleanupFailure,
    ::testing::Values(CleanupFault::AdcDisable, CleanupFault::CommonRestore,
                      CleanupFault::PinRestore, CleanupFault::PinPullRestore,
                      CleanupFault::AdcGate, CleanupFault::GpioGate));

class GD32ADCStopFailure : public GD32ADCInitialize,
                           public ::testing::WithParamInterface<CleanupFault> {
};

TEST_P(GD32ADCStopFailure, RefusedStopRetainsTheOwnerUntilExplicitRetry) {
    ASSERT_EQ(nx_gd32_adc_initialize(&state, channels, 5U, 3300U, 200U),
              NX_SUCCESS);
    s_cleanup_active = true;
    s_cleanup_fault = GetParam();
    EXPECT_CALL(boundary,
                Observe(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(0);
    EXPECT_EQ(nx_gd32_adc_stop(&state), NX_ERROR_IO);
    EXPECT_TRUE(s_cleanup_injected);
    EXPECT_TRUE(state.initialized);
    EXPECT_NE(RCU_APB2EN & RCU_APB2EN_ADC0EN, 0U);
    nx_adc_info_t info = {};
    EXPECT_EQ(nx_gd32_adc_info(&state, &info), NX_SUCCESS);
    s_cleanup_fault = CleanupFault::None;
    s_cleanup_active = false;
    EXPECT_EQ(nx_gd32_adc_stop(&state), NX_SUCCESS);
    EXPECT_FALSE(state.initialized);
    EXPECT_EQ(RCU_APB2EN & RCU_APB2EN_ADC0EN, 0U);
}

INSTANTIATE_TEST_SUITE_P(RetryableStop, GD32ADCStopFailure,
                         ::testing::Values(CleanupFault::AdcDisable,
                                           CleanupFault::AdcGate));

TEST_F(GD32ADCInitialize, StabilityDeadlineRollsBackPinsClocksAndCommonState) {
    FailAndRetry(Fault::Stability, NX_ERROR_TIMEOUT);
}

TEST_F(GD32ADCInitialize, ResetCalibrationDeadlineRollsBackWithoutOwner) {
    FailAndRetry(Fault::Reset, NX_ERROR_TIMEOUT);
}

TEST_F(GD32ADCInitialize, CalibrationDeadlineRollsBackWithoutOwner) {
    FailAndRetry(Fault::Calibration, NX_ERROR_TIMEOUT);
}

TEST_F(GD32ADCInitialize, FrozenMonotonicClockFailsBoundedlyAndRollsBack) {
    FailAndRetry(Fault::Frozen, NX_ERROR_IO);
    EXPECT_GE(s_powered_ticks, 1U);
}

TEST_F(GD32ADCInitialize, FrozenResetCalibrationFailsBoundedlyAndRollsBack) {
    FailAndRetry(Fault::ResetFrozen, NX_ERROR_IO);
}

TEST_F(GD32ADCInitialize, FrozenCalibrationFailsBoundedlyAndRollsBack) {
    FailAndRetry(Fault::CalibrationFrozen, NX_ERROR_IO);
}

TEST_F(GD32ADCInitialize, SuccessfulColdStartOnlyConfiguresSelectedPins) {
    EXPECT_CALL(boundary, ClockEnable(RCU_ADC0)).Times(1);
    EXPECT_CALL(boundary, Reset()).Times(1);
    EXPECT_CALL(boundary, Prescaler(ADC_ADCCK_PCLK2_DIV8)).Times(1);
    ASSERT_EQ(nx_gd32_adc_initialize(&state, channels, 5U, 3300U, 200U),
              NX_SUCCESS);
    for (unsigned i = 0U; i < 3U; ++i) {
        uint32_t fields = i == 0U ? 15U : 3U;
        EXPECT_EQ(GPIO_CTL(GPIOA + i * 0x400U),
                  (initial_mode[i] & ~fields) | fields);
        EXPECT_EQ(GPIO_PUD(GPIOA + i * 0x400U), initial_pull[i] & ~fields);
    }
    EXPECT_EQ(RCU_AHB1EN, RCU_AHB1EN_PAEN | RCU_AHB1EN_PBEN | RCU_AHB1EN_PCEN |
                              RCU_AHB1EN_DMA1EN);
    EXPECT_EQ(RCU_APB2EN, RCU_APB2EN_USART0EN | RCU_APB2EN_ADC0EN);
    EXPECT_EQ(ADC_SYNCCTL, ADC_ADCCK_PCLK2_DIV8);
    EXPECT_EQ(ADC_CTL1(ADC0), ADC_CTL1_ADCON);
    EXPECT_EQ(state.channels, channels);
    EXPECT_EQ(state.channel_count, 5U);
}

TEST_F(GD32ADCInitialize, DatasheetReferenceFloorRejectsBeforeHardwareEffects) {
    EXPECT_CALL(boundary, ClockEnable(::testing::_)).Times(0);
    EXPECT_CALL(boundary, Reset()).Times(0);
    EXPECT_EQ(nx_gd32_adc_initialize(&state, channels, 5U, 2599U, 200U),
              NX_ERROR_INVALID);
    EXPECT_FALSE(state.initialized);
    ExpectRestored();
}

TEST_F(GD32ADCInitialize, RefusedDmaClockRollsBackBeforePowerOrCalibration) {
    s_refuse_prescaler = true;
    EXPECT_CALL(boundary, Prescaler(ADC_ADCCK_HCLK_DIV6)).Times(1);
    EXPECT_EQ(nx_gd32_adc_dma_initialize(&state, channels, 5U, 3300U, 200U),
              NX_ERROR_IO);
    EXPECT_EQ(s_powered_ticks, 0U);
    EXPECT_EQ(s_reset_calibrations, 0U);
    EXPECT_EQ(s_calibrations, 0U);
    ExpectRestored();
}

TEST_F(GD32ADCInitialize,
       ColdReferenceFloorRejectsBeforeTimeOrHardwareEffects) {
    const nx_gd32_adc_cold_config_t config{channels, 5U, 2599U, 100U};
    EXPECT_CALL(boundary, ClockEnable(::testing::_)).Times(0);
    EXPECT_CALL(boundary, Reset()).Times(0);
    EXPECT_EQ(nx_gd32_adc_poll_cold_prepare(&state, &config), NX_ERROR_INVALID);
    EXPECT_EQ(s_now, 100U);
    EXPECT_FALSE(state.initialized);
    ExpectRestored();
}

TEST_F(GD32ADCInitialize, EocWaitsForErrataDataVisibility) {
    ASSERT_EQ(nx_gd32_adc_initialize(&state, channels, 5U, 3300U, 200U),
              NX_SUCCESS);
    s_conversion_works = true;
    s_delayed_eoc = true;
    uint16_t samples[5] = {};
    size_t count = 0U;
    ASSERT_EQ(nx_gd32_adc_sample(&state, samples, 5U, s_now + 100U, &count),
              NX_SUCCESS);
    ASSERT_EQ(count, 5U);
    EXPECT_EQ(s_early_data_reads, 0U);
    EXPECT_EQ(s_data_reads, 5U);
    for (size_t i = 0U; i < count; ++i) {
        EXPECT_EQ(samples[i], 100U + channels[i]);
    }
}

TEST_F(GD32ADCInitialize, EocGuardDeadlineDoesNotPublishOrReadAResult) {
    ASSERT_EQ(nx_gd32_adc_initialize(&state, channels, 5U, 3300U, 200U),
              NX_SUCCESS);
    s_conversion_works = true;
    s_delayed_eoc = true;
    uint16_t samples[5] = {};
    size_t count = 99U;
    EXPECT_EQ(nx_gd32_adc_sample(&state, samples, 5U, s_now + 3U, &count),
              NX_ERROR_TIMEOUT);
    EXPECT_EQ(count, 0U);
    EXPECT_EQ(s_data_reads, 0U);
    EXPECT_EQ(ADC_CTL1(ADC0), 0U);
    EXPECT_FALSE(state.active);
}

TEST_F(GD32ADCInitialize, FrozenEocGuardFailsBoundedlyWithoutReadingAResult) {
    ASSERT_EQ(nx_gd32_adc_initialize(&state, channels, 5U, 3300U, 200U),
              NX_SUCCESS);
    s_conversion_works = true;
    s_delayed_eoc = true;
    s_frozen_eoc = true;
    uint16_t samples[5] = {};
    size_t count = 99U;
    EXPECT_EQ(nx_gd32_adc_sample(&state, samples, 5U, s_now + 100U, &count),
              NX_ERROR_IO);
    EXPECT_EQ(count, 0U);
    EXPECT_EQ(s_data_reads, 0U);
    EXPECT_EQ(ADC_CTL1(ADC0), 0U);
    EXPECT_FALSE(state.active);
}

TEST_F(GD32ADCInitialize, GatedPinsAreCapturedOnlyAfterClockObservation) {
    GPIO_CTL(GPIOA) = 0U;
    GPIO_PUD(GPIOA) = 0U;
    GPIO_CTL(GPIOC) = 0U;
    GPIO_PUD(GPIOC) = 0U;
    s_gated_pins = true;
    s_fault = Fault::Calibration;
    ASSERT_EQ(nx_gd32_adc_initialize(&state, channels, 5U, 3300U, 200U),
              NX_ERROR_TIMEOUT);
    EXPECT_FALSE(s_gated_pins);
    ExpectRestored();
}

TEST_F(GD32ADCInitialize, RollbackPreservesNeighborWritesAndUnrelatedClocks) {
    s_fault = Fault::Reset;
    s_neighbor_write = true;
    ASSERT_EQ(nx_gd32_adc_initialize(&state, channels, 5U, 3300U, 200U),
              NX_ERROR_TIMEOUT);
    initial_mode[0] |= kNeighborMask;
    initial_pull[0] = (initial_pull[0] & ~kNeighborMask) | (1U << 30U);
    EXPECT_EQ(GPIO_CTL(GPIOA), initial_mode[0]);
    EXPECT_EQ(GPIO_PUD(GPIOA), initial_pull[0]);
    EXPECT_EQ(RCU_AHB1EN,
              RCU_AHB1EN_PBEN | RCU_AHB1EN_DMA1EN | RCU_AHB1EN_PIEN);
    EXPECT_EQ(RCU_APB2EN, RCU_APB2EN_USART0EN | RCU_APB2EN_SYSCFGEN);
}

TEST_F(GD32ADCInitialize, PreexpiredDeadlineHasNoSDKOrRegisterEffects) {
    EXPECT_CALL(boundary, ConfigurePin(::testing::_, ::testing::_, ::testing::_,
                                       ::testing::_))
        .Times(0);
    EXPECT_CALL(boundary, ClockEnable(::testing::_)).Times(0);
    EXPECT_CALL(boundary, Reset()).Times(0);
    ASSERT_EQ(nx_gd32_adc_initialize(&state, channels, 5U, 3300U, 100U),
              NX_ERROR_TIMEOUT);
    ExpectRestored();
}

TEST_F(GD32ADCInitialize, ExistingADCClockRejectsSharedResetBeforeAnyEffect) {
    for (uint32_t mask :
         {RCU_APB2EN_ADC0EN, RCU_APB2EN_ADC1EN, RCU_APB2EN_ADC2EN}) {
        RCU_APB2EN = RCU_APB2EN_USART0EN | mask;
        ADC_CTL1(ADC1) = ADC_CTL1_ADCON;
        EXPECT_CALL(boundary, ConfigurePin(::testing::_, ::testing::_,
                                           ::testing::_, ::testing::_))
            .Times(0);
        EXPECT_CALL(boundary, ClockEnable(::testing::_)).Times(0);
        EXPECT_CALL(boundary, Reset()).Times(0);
        EXPECT_EQ(nx_gd32_adc_initialize(&state, channels, 5U, 3300U, 200U),
                  NX_ERROR_BUSY);
        EXPECT_EQ(RCU_APB2EN, RCU_APB2EN_USART0EN | mask);
        EXPECT_EQ(ADC_CTL1(ADC1), ADC_CTL1_ADCON);
        EXPECT_EQ(GPIO_CTL(GPIOA), initial_mode[0]);
        EXPECT_EQ(ADC_SYNCCTL, ADC_ADCCK_PCLK2_DIV4 | ADC_SYNCCTL_VBATEN);
        ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(&boundary));
        if (state.initialized) {
            ASSERT_EQ(nx_gd32_adc_stop(&state), NX_SUCCESS);
        }
    }
}

TEST_F(GD32ADCInitialize, ISRAndMaskedAdmissionLeavePinsAndClocksUntouched) {
    EXPECT_CALL(boundary, ClockEnable(::testing::_)).Times(0);
    s_isr = true;
    EXPECT_EQ(nx_gd32_adc_initialize(&state, channels, 5U, 3300U, 200U),
              NX_ERROR_CONTEXT);
    s_isr = false;
    s_mask = 1U;
    EXPECT_EQ(nx_gd32_adc_initialize(&state, channels, 5U, 3300U, 200U),
              NX_ERROR_CONTEXT);
    EXPECT_EQ(s_mask, 1U);
    ExpectRestored();
}

TEST_F(GD32ADCInitialize, FailedPollRecalibratesAfterPowerOffBeforeRetry) {
    PowerOffAfterFailedPoll();
    ASSERT_EQ(s_reset_calibrations, 1U);
    ASSERT_EQ(s_calibrations, 1U);
    uint16_t samples[5] = {};
    size_t count = 99U;
    s_conversion_works = true;
    ASSERT_EQ(nx_gd32_adc_sample(&state, samples, 5U, s_now + 100U, &count),
              NX_SUCCESS);
    EXPECT_EQ(count, 5U);
    EXPECT_EQ(s_reset_calibrations, 2U);
    EXPECT_EQ(s_calibrations, 2U);
    for (size_t i = 0U; i < count; ++i) {
        EXPECT_EQ(samples[i], 100U + channels[i]);
    }
}

TEST_F(GD32ADCInitialize,
       RetryResetCalibrationTimeoutPowersOffAndReleasesOwner) {
    PowerOffAfterFailedPoll();
    s_fault = Fault::Reset;
    s_conversion_works = true;
    uint16_t samples[5] = {};
    size_t count = 99U;
    EXPECT_EQ(nx_gd32_adc_sample(&state, samples, 5U, s_now + 100U, &count),
              NX_ERROR_TIMEOUT);
    EXPECT_EQ(count, 0U);
    EXPECT_EQ(ADC_CTL1(ADC0), 0U);
    EXPECT_FALSE(state.active);
    EXPECT_TRUE(state.initialized);
    s_fault = Fault::None;
    ASSERT_EQ(nx_gd32_adc_sample(&state, samples, 5U, s_now + 100U, &count),
              NX_SUCCESS);
    EXPECT_EQ(count, 5U);
}

TEST_F(GD32ADCInitialize, RetryCalibrationTimeoutPowersOffAndRetainsBinding) {
    PowerOffAfterFailedPoll();
    s_fault = Fault::Calibration;
    s_conversion_works = true;
    uint16_t samples[5] = {};
    size_t count = 99U;
    EXPECT_EQ(nx_gd32_adc_sample(&state, samples, 5U, s_now + 100U, &count),
              NX_ERROR_TIMEOUT);
    EXPECT_EQ(count, 0U);
    EXPECT_EQ(ADC_CTL1(ADC0), 0U);
    EXPECT_FALSE(state.active);
    EXPECT_TRUE(state.initialized);
}

TEST_F(GD32ADCInitialize, RetryFrozenClockFailsBoundedlyBeforeAnyConversion) {
    PowerOffAfterFailedPoll();
    s_fault = Fault::Frozen;
    s_conversion_works = true;
    uint16_t samples[5] = {};
    size_t count = 99U;
    EXPECT_EQ(nx_gd32_adc_sample(&state, samples, 5U, s_now + 100U, &count),
              NX_ERROR_IO);
    EXPECT_EQ(count, 0U);
    EXPECT_EQ(ADC_CTL1(ADC0), 0U);
    EXPECT_FALSE(state.active);
}

TEST_F(GD32ADCInitialize,
       StaticColdConfigRejectsZeroWaitBeforeTimeOrSDKEffects) {
    const nx_gd32_adc_cold_config_t config{channels, 5U, 3300U, 0U};
    EXPECT_CALL(boundary, ClockEnable(testing::_)).Times(0);
    EXPECT_CALL(boundary, Reset()).Times(0);
    EXPECT_EQ(NX_ERROR_INVALID, nx_gd32_adc_poll_cold_prepare(&state, &config));
    EXPECT_EQ(100U, s_now);
    ExpectRestored();
}

TEST_F(GD32ADCInitialize,
       StaticColdConfigKeepsRealCalibrationRollbackAndRetry) {
    const nx_gd32_adc_cold_config_t config{channels, 5U, 3300U, 100U};
    s_fault = Fault::Reset;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_gd32_adc_poll_cold_prepare(&state, &config));
    ExpectRestored();
    s_fault = Fault::None;
    ASSERT_EQ(NX_SUCCESS, nx_gd32_adc_poll_cold_prepare(&state, &config));
    EXPECT_TRUE(state.initialized);
    EXPECT_EQ(channels, state.channels);
}

TEST_F(GD32ADCInitialize, ColdContextRejectsBeforeReadingTheTimeDomain) {
    const nx_gd32_adc_cold_config_t config{channels, 5U, 3300U, 100U};
    s_mask = 1U;
    EXPECT_CALL(boundary, ClockEnable(testing::_)).Times(0);
    EXPECT_CALL(boundary, Reset()).Times(0);
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_gd32_adc_poll_cold_prepare(&state, &config));
    EXPECT_EQ(100U, s_now);
    EXPECT_EQ(1U, s_mask);
    ExpectRestored();
}
