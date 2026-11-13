/**
 * \file            stm32_dac_test.cpp
 * \brief           Actual F407 DAC owner against low-address register events
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-08
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f407_dac.h"
}
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

struct DacWrite {
    volatile uint32_t* reg;
    uint32_t value;
};
static std::vector<DacWrite> writes;
static bool privileged;
static bool in_isr;
static bool masked;
static bool refuse_gate;
static bool refuse_disable;
static bool refuse_gate_clear;
static bool refuse_wave_clear;
static bool advance_on_write;
static nx_time_us_t model_time;
static nx_time_us_t enabled_at;
static nx_time_us_t expire_on_timer_enable;
static uint32_t refused_control_bits;
static bool refuse_trigger_clear;
static bool preempt_on_first_time;
static bool lose_time_on_output_enable;
static bool model_time_lost;
static nx_time_us_t external_enabled_at;

/** \brief Model the peripheral's DHR encoding and automatic DOR transfer. */
static void holding(unsigned channel, uint32_t code) {
    volatile uint32_t* data = channel == 1U ? &DAC->DHR12R1 : &DAC->DHR12R2;
    volatile uint32_t* output = channel == 1U ? &DAC->DOR1 : &DAC->DOR2;
    *data = code;
    if ((DAC->CR & (DAC_CR_TEN1 << (16U * (channel - 1U)))) == 0U) {
        *output = code;
    }
}

extern "C" void nx_dac_model_write(volatile uint32_t* reg, uint32_t value) {
    writes.push_back({reg, value});
    if (reg == &EXTI->PR) {
        *reg &= ~value;
        return;
    }
    if (advance_on_write) {
        ++model_time;
    }
    if (reg == &RCC->APB1ENR && refuse_gate) {
        value &= ~static_cast<uint32_t>(RCC_APB1ENR_DACEN);
    }
    if (reg == &DAC->CR && refuse_disable) {
        value |= DAC->CR & static_cast<uint32_t>(DAC_CR_EN1 | DAC_CR_EN2);
    }
    if (reg == &DAC->CR) {
        value =
            (value & ~refused_control_bits) | (DAC->CR & refused_control_bits);
        if (refuse_trigger_clear) {
            value |= DAC->CR & static_cast<uint32_t>(DAC_CR_TEN1 | DAC_CR_TEN2);
        }
    }
    if ((reg == &TIM6->CR1 || reg == &TIM7->CR1) &&
        (value & TIM_CR1_CEN) != 0U && expire_on_timer_enable != 0U) {
        model_time = expire_on_timer_enable;
    }
    if (reg == &RCC->APB1ENR && refuse_gate_clear) {
        value |= RCC->APB1ENR;
    }
    if (reg == &DAC->CR && refuse_wave_clear) {
        value |= DAC->CR & static_cast<uint32_t>(DAC_CR_WAVE1 | DAC_CR_WAVE2);
    }
    if (reg == &DAC->CR && (value & DAC_CR_EN2) != 0U &&
        (DAC->CR & DAC_CR_EN2) == 0U) {
        enabled_at = model_time;
        if (lose_time_on_output_enable)
            model_time_lost = true;
    }
    if (reg == &DAC->CR && (DAC->CR & DAC_CR_TEN1) == 0U &&
        (value & DAC_CR_TEN1) != 0U && (value & DAC_CR_EN1) != 0U &&
        (value & DAC_CR_TSEL1) == (DAC_CR_TSEL1_2 | DAC_CR_TSEL1_1))
        external_enabled_at = model_time;
    if (reg == &TIM6->SR || reg == &TIM7->SR) {
        *reg &= value;
        return;
    }
    if (reg == &TIM6->EGR || reg == &TIM7->EGR) {
        TIM_TypeDef* selected = reg == &TIM6->EGR ? TIM6 : TIM7;
        selected->SR |= TIM_SR_UIF;
    }
    *reg = value;
    if (reg == &DAC->DHR12R1 || reg == &DAC->DHR12L1 || reg == &DAC->DHR8R1) {
        holding(1U, reg == &DAC->DHR12L1  ? value >> 4U
                    : reg == &DAC->DHR8R1 ? value << 4U
                                          : value);
    } else if (reg == &DAC->DHR12R2 || reg == &DAC->DHR12L2 ||
               reg == &DAC->DHR8R2) {
        holding(2U, reg == &DAC->DHR12L2  ? value >> 4U
                    : reg == &DAC->DHR8R2 ? value << 4U
                                          : value);
    } else if (reg == &DAC->DHR12RD || reg == &DAC->DHR12LD) {
        const unsigned shift = reg == &DAC->DHR12LD ? 4U : 0U;
        holding(1U, (value >> shift) & 0xfffU);
        holding(2U, (value >> (16U + shift)) & 0xfffU);
    } else if (reg == &DAC->DHR8RD) {
        holding(1U, (value & 0xffU) << 4U);
        holding(2U, ((value >> 8U) & 0xffU) << 4U);
    } else if (reg == &DAC->SWTRIGR) {
        if ((value & DAC_SWTRIGR_SWTRIG1) != 0U) {
            DAC->DOR1 = DAC->DHR12R1;
        }
        if ((value & DAC_SWTRIGR_SWTRIG2) != 0U) {
            DAC->DOR2 = DAC->DHR12R2;
        }
        DAC->SWTRIGR = 0U;
    }
}
extern "C" bool nx_arch_is_privileged(void) {
    return privileged;
}
extern "C" bool nx_arch_in_isr(void) {
    return in_isr;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return masked;
}
extern "C" void nx_arch_dsb(void) {
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    if (model_time_lost)
        return NX_DEADLINE_NEVER;
    nx_time_us_t observed = model_time++;
    if (preempt_on_first_time) {
        preempt_on_first_time = false;
        model_time += 5000U;
    }
    return observed;
}

class DacOwner : public testing::Test {
  protected:
    void* mapping = MAP_FAILED;
    nx_stm32_dac_state_t state{};
    nx_stm32_dac_config_t config{};
    nx_dac_port_t face{&nx_stm32_dac_ops, &state};
    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(0x40000000U), 0x80000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(0x40000000U));
        writes.clear();
        privileged = true;
        in_isr = masked = refuse_gate = refuse_disable = false;
        refuse_gate_clear = refuse_wave_clear = advance_on_write = false;
        enabled_at = 0U;
        expire_on_timer_enable = 0U;
        refused_control_bits = 0U;
        refuse_trigger_clear = false;
        preempt_on_first_time = lose_time_on_output_enable = false;
        model_time_lost = false;
        external_enabled_at = 0U;
        model_time = 100U;
        RCC->AHB1ENR = RCC_AHB1ENR_GPIOAEN;
        config.registers = DAC;
        config.channels = NX_DAC_CHANNEL_1 | NX_DAC_CHANNEL_2;
        config.format = NX_DAC_FORMAT_U12_RIGHT;
        config.mode = NX_STM32_DAC_DIRECT;
        config.trigger = NX_STM32_DAC_TRIGGER_NONE;
        config.output_buffer = true;
    }
    void TearDown() override {
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(0, munmap(mapping, 0x80000U));
        }
    }
};

TEST_F(DacOwner, RejectsInvalidSelectionBeforeHardwareEffects) {
    config.channels = 0U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_dac_prepare(&state, &config));
    config.channels = 4U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_dac_prepare(&state, &config));
    config.channels = 3U;
    config.idle_codes[0] = 4096U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_dac_prepare(&state, &config));
    EXPECT_TRUE(writes.empty());
    EXPECT_EQ(nullptr, state.config);
}
TEST_F(DacOwner, PreparationKeepsBothOutputsDisabledAndMapsOnlyAnalogPads) {
    GPIOA->MODER = 0x55555555U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_dac_prepare(&state, &config));
    EXPECT_EQ(0U, DAC->CR & (DAC_CR_EN1 | DAC_CR_EN2));
    EXPECT_EQ(0x55555f55U, GPIOA->MODER);
    nx_dac_info_t info{};
    ASSERT_EQ(NX_SUCCESS, nx_dac_port_info(&face, &info));
    EXPECT_EQ(3U, info.channel_mask);
    EXPECT_EQ(4095U, info.maximum_code);
    EXPECT_EQ(12U, info.code_bits);
    EXPECT_TRUE(info.output_buffer);
}
TEST_F(DacOwner, SingleWritesKeepOtherChannelAndUnselectedFields) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32_dac_prepare(&state, &config));
    ASSERT_EQ(NX_SUCCESS, nx_dac_port_write(&face, 2U, 0x123U));
    ASSERT_EQ(NX_SUCCESS, nx_dac_port_write(&face, 1U, 0xabcU));
    EXPECT_EQ(0xabcU, DAC->DOR1);
    EXPECT_EQ(0x123U, DAC->DOR2);
}
TEST_F(DacOwner, LeftAlignedStorageDoesNotChangeThePublicRawCodeUnit) {
    config.format = NX_DAC_FORMAT_U12_LEFT;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_dac_prepare(&state, &config));
    ASSERT_EQ(NX_SUCCESS, nx_dac_port_write(&face, 1U, 0xabcU));
    EXPECT_EQ(0xabc0U, DAC->DHR12L1);
    EXPECT_EQ(0xabcU, DAC->DOR1);
}
TEST_F(DacOwner, EightBitReadbackUsesTheSelectedPublicCodeUnit) {
    config.format = NX_DAC_FORMAT_U8;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_dac_prepare(&state, &config));
    ASSERT_EQ(NX_SUCCESS, nx_dac_port_write(&face, 1U, 0xabU));
    nx_dac_state_t sample{};
    ASSERT_EQ(NX_SUCCESS, nx_dac_port_state(&face, &sample));
    EXPECT_EQ(0xabU, sample.output_codes[0]);
    EXPECT_EQ(8U, sample.code_bits);
    nx_dac_info_t info{};
    ASSERT_EQ(NX_SUCCESS, nx_dac_port_info(&face, &info));
    EXPECT_EQ(8U, info.code_bits);
    EXPECT_EQ(0xab0U, DAC->DOR1);
    const size_t count = writes.size();
    EXPECT_EQ(NX_ERROR_INVALID, nx_dac_port_write(&face, 1U, 256U));
    EXPECT_EQ(count, writes.size());
}
TEST_F(DacOwner, PairUsesOneOfficialDualRegisterWrite) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32_dac_prepare(&state, &config));
    writes.clear();
    ASSERT_EQ(NX_SUCCESS, nx_dac_port_write_pair(&face, 0x123U, 0xabcU));
    ASSERT_EQ(1U, writes.size());
    EXPECT_EQ(&DAC->DHR12RD, writes[0].reg);
    EXPECT_EQ(0x0abc0123U, writes[0].value);
    EXPECT_EQ(0x123U, DAC->DOR1);
    EXPECT_EQ(0xabcU, DAC->DOR2);
}
TEST_F(DacOwner, SoftwareStartEnablesBothChannelsAndTriggerCommitsTogether) {
    config.mode = NX_STM32_DAC_TRIGGERED;
    config.trigger = NX_STM32_DAC_TRIGGER_SOFTWARE;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_dac_prepare(&state, &config));
    ASSERT_EQ(NX_SUCCESS, nx_dac_port_write_pair(&face, 0x123U, 0x456U));
    EXPECT_EQ(0U, DAC->DOR1);
    EXPECT_EQ(NX_ERROR_STATE, nx_dac_port_trigger(&face, 3U));
    ASSERT_EQ(NX_SUCCESS, nx_dac_port_start(&face, 0U, 200U));
    EXPECT_EQ(DAC_CR_EN1 | DAC_CR_EN2, DAC->CR & (DAC_CR_EN1 | DAC_CR_EN2));
    writes.clear();
    ASSERT_EQ(NX_SUCCESS, nx_dac_port_trigger(&face, 3U));
    ASSERT_EQ(1U, writes.size());
    EXPECT_EQ(3U, writes[0].value);
    EXPECT_EQ(0x123U, DAC->DOR1);
    EXPECT_EQ(0x456U, DAC->DOR2);
}
TEST_F(DacOwner, UnauthorizedChannelAndExpiredStartHaveNoSideEffects) {
    config.channels = NX_DAC_CHANNEL_1;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_dac_prepare(&state, &config));
    writes.clear();
    EXPECT_EQ(NX_ERROR_PERMISSION, nx_dac_port_write(&face, 2U, 1U));
    EXPECT_EQ(NX_ERROR_PERMISSION, nx_dac_port_write_pair(&face, 1U, 2U));
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_dac_port_start(&face, 0U, 99U));
    EXPECT_TRUE(writes.empty());
}
TEST_F(DacOwner, HardwareWaveUsesExactAmplitudeAndRejectsOffsetOverflow) {
    config.mode = NX_STM32_DAC_TRIANGLE;
    config.trigger = NX_STM32_DAC_TRIGGER_SOFTWARE;
    config.amplitude = 255U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_dac_prepare(&state, &config));
    EXPECT_EQ(7U, (DAC->CR & DAC_CR_MAMP1) >> DAC_CR_MAMP1_Pos);
    EXPECT_EQ(7U, (DAC->CR & DAC_CR_MAMP2) >> DAC_CR_MAMP2_Pos);
    EXPECT_EQ(DAC_CR_WAVE1_1 | DAC_CR_WAVE2_1,
              DAC->CR & (DAC_CR_WAVE1 | DAC_CR_WAVE2));
    const size_t count = writes.size();
    EXPECT_EQ(NX_ERROR_INVALID, nx_dac_port_write(&face, 1U, 4095U));
    EXPECT_EQ(count, writes.size());
}

TEST_F(DacOwner, NoiseUsesAllReviewedLfsrMasksWithoutChangingOtherChannels) {
    config.mode = NX_STM32_DAC_NOISE;
    config.trigger = NX_STM32_DAC_TRIGGER_SOFTWARE;
    config.channels = NX_DAC_CHANNEL_2;
    for (unsigned bits = 1U; bits <= 12U; ++bits) {
        DAC->CR = DAC_CR_BOFF1;
        config.amplitude = static_cast<uint16_t>((1U << bits) - 1U);
        ASSERT_EQ(NX_SUCCESS, nx_stm32_dac_prepare(&state, &config));
        EXPECT_EQ(static_cast<uint32_t>(DAC_CR_BOFF1), DAC->CR & 0xffffU);
        EXPECT_EQ(static_cast<uint32_t>(DAC_CR_WAVE2_0),
                  DAC->CR & DAC_CR_WAVE2);
        EXPECT_EQ(bits - 1U, (DAC->CR & DAC_CR_MAMP2) >> DAC_CR_MAMP2_Pos);
        nx_dac_info_t info{};
        ASSERT_EQ(NX_SUCCESS, nx_dac_port_info(&face, &info));
        EXPECT_EQ(NX_DAC_WAVE_NOISE, info.wave);
        EXPECT_EQ(config.amplitude, info.amplitude);
        ASSERT_EQ(NX_SUCCESS, nx_stm32_dac_close(&state));
        EXPECT_EQ(static_cast<uint32_t>(DAC_CR_BOFF1), DAC->CR & 0xffffU);
        EXPECT_EQ(0U, DAC->CR & (DAC_CR_EN2 | DAC_CR_TEN2 | DAC_CR_WAVE2 |
                                 DAC_CR_DMAEN2));
    }
}
TEST_F(DacOwner, RefusedClockNeverPublishesAnInitializedDevice) {
    refuse_gate = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_dac_prepare(&state, &config));
    EXPECT_FALSE(state.initialized);
    EXPECT_EQ(0U, DAC->CR);
    refuse_gate = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_dac_close(&state));
}
TEST_F(DacOwner, RefusedDisableRetainsTheClockAndAllowsCloseRetry) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32_dac_prepare(&state, &config));
    ASSERT_EQ(NX_SUCCESS, nx_dac_port_start(&face, 0U, 200U));
    refuse_disable = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_dac_close(&state));
    EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_DACEN);
    EXPECT_NE(nullptr, state.config);
    refuse_disable = false;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_dac_close(&state));
    EXPECT_EQ(0U, RCC->APB1ENR & RCC_APB1ENR_DACEN);
    EXPECT_EQ(nullptr, state.config);
}
TEST_F(DacOwner, ContextRejectionPrecedesClockOrPadWrites) {
    in_isr = true;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_stm32_dac_prepare(&state, &config));
    in_isr = false;
    masked = true;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_stm32_dac_prepare(&state, &config));
    masked = false;
    privileged = false;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_stm32_dac_prepare(&state, &config));
    EXPECT_TRUE(writes.empty());
}

TEST_F(DacOwner, StartupDelayBeginsAfterTheLastOutputEnable) {
    config.mode = NX_STM32_DAC_TRIGGERED;
    config.trigger = NX_STM32_DAC_TRIGGER_TIM6;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_dac_prepare(&state, &config));
    advance_on_write = true;
    ASSERT_EQ(NX_SUCCESS, nx_dac_port_start(&face, 1000U, 1000U));
    EXPECT_GE(model_time - enabled_at, 15U);
}

TEST_F(DacOwner, ClosingWithRefusedGateWithdrawalRejectsFurtherIo) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32_dac_prepare(&state, &config));
    refuse_gate_clear = true;
    ASSERT_EQ(NX_ERROR_BUSY, nx_stm32_dac_close(&state));
    ASSERT_NE(nullptr, state.config);
    writes.clear();
    EXPECT_EQ(NX_ERROR_STATE, nx_dac_port_write(&face, 1U, 123U));
    EXPECT_TRUE(writes.empty());
    refuse_gate_clear = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_dac_close(&state));
}

TEST_F(DacOwner, RefusedWaveWithdrawalCannotReportIdleOrReleaseTheClock) {
    config.mode = NX_STM32_DAC_TRIANGLE;
    config.trigger = NX_STM32_DAC_TRIGGER_SOFTWARE;
    config.amplitude = 255U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_dac_prepare(&state, &config));
    refuse_wave_clear = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_dac_port_stop(&face, 1000U));
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_dac_close(&state));
    EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_DACEN);
    refuse_wave_clear = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_dac_close(&state));
}

TEST_F(DacOwner, TimerDividerSetupHasNoEnabledDacTriggerAndClearsOnlyUif) {
    for (const auto source :
         {NX_STM32_DAC_TRIGGER_TIM6, NX_STM32_DAC_TRIGGER_TIM7}) {
        config.mode = NX_STM32_DAC_TRIGGERED;
        config.trigger = source;
        ASSERT_EQ(NX_SUCCESS, nx_stm32_dac_prepare(&state, &config));
        TIM_TypeDef* selected =
            source == NX_STM32_DAC_TRIGGER_TIM6 ? TIM6 : TIM7;
        selected->SR = TIM_SR_UIF | TIM_SR_CC1IF;
        writes.clear();
        ASSERT_EQ(NX_SUCCESS, nx_dac_port_start(&face, 1U, 1000U));
        uint32_t control = DAC_CR_TEN1 | DAC_CR_TEN2;
        bool update = false;
        for (const auto& event : writes) {
            if (event.reg == &DAC->CR) {
                control = event.value;
            } else if (event.reg == &selected->EGR) {
                EXPECT_EQ(0U, control & (DAC_CR_TEN1 | DAC_CR_TEN2));
                update = true;
            }
        }
        EXPECT_TRUE(update);
        EXPECT_EQ(static_cast<uint32_t>(TIM_SR_CC1IF), selected->SR);
        nx_dac_state_t observed{};
        ASSERT_EQ(NX_SUCCESS, nx_dac_port_state(&face, &observed));
        EXPECT_GE(observed.trigger_divisor, 84000000U);
        EXPECT_EQ(84000000U, observed.timer_clock_hz);
        ASSERT_EQ(NX_SUCCESS, nx_dac_port_stop(&face, 1000U));
        EXPECT_EQ(0U, selected->CR1 & TIM_CR1_CEN);
        ASSERT_EQ(NX_SUCCESS, nx_stm32_dac_close(&state));
    }
}

TEST_F(DacOwner, ForeignDacOrTimerCannotBeStoppedByRejectedPreparation) {
    config.mode = NX_STM32_DAC_TRIGGERED;
    config.trigger = NX_STM32_DAC_TRIGGER_TIM6;
    RCC->APB1ENR = RCC_APB1ENR_DACEN | RCC_APB1ENR_TIM6EN;
    for (unsigned busy = 0U; busy < 2U; ++busy) {
        DAC->CR = busy == 0U ? DAC_CR_EN2 : 0U;
        TIM6->DIER = busy == 1U ? TIM_DIER_UIE : 0U;
        writes.clear();
        ASSERT_EQ(NX_ERROR_BUSY, nx_stm32_dac_prepare(&state, &config));
        EXPECT_EQ(busy == 0U ? DAC_CR_EN2 : 0U, DAC->CR);
        EXPECT_EQ(busy == 1U ? TIM_DIER_UIE : 0U, TIM6->DIER);
        EXPECT_EQ(RCC_APB1ENR_DACEN | RCC_APB1ENR_TIM6EN, RCC->APB1ENR);
        for (const auto& event : writes) {
            EXPECT_EQ(&RCC->APB1ENR, event.reg);
        }
    }
}

TEST_F(DacOwner, ClosePreservesPreexistingGatesAndOtherPinFields) {
    config.channels = NX_DAC_CHANNEL_1;
    RCC->APB1ENR = RCC_APB1ENR_DACEN | RCC_APB1ENR_TIM7EN;
    GPIOA->MODER = 0x55555555U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_dac_prepare(&state, &config));
    ASSERT_EQ(NX_SUCCESS, nx_stm32_dac_close(&state));
    EXPECT_EQ(RCC_APB1ENR_DACEN | RCC_APB1ENR_TIM7EN, RCC->APB1ENR);
    EXPECT_EQ(0x55555755U, GPIOA->MODER);
}

TEST_F(DacOwner, ObservationCannotOverwriteLiveStateOrConstConfiguration) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32_dac_prepare(&state, &config));
    const auto original = state;
    EXPECT_EQ(
        NX_ERROR_INVALID,
        nx_dac_port_info(&face, reinterpret_cast<nx_dac_info_t*>(&state)));
    EXPECT_EQ(
        NX_ERROR_INVALID,
        nx_dac_port_state(&face, reinterpret_cast<nx_dac_state_t*>(&config)));
    EXPECT_EQ(0, std::memcmp(&state, &original, sizeof(state)));
}

TEST_F(DacOwner, BasicTimerCannotInheritOnePulseOrDisabledUpdateEvents) {
    config.mode = NX_STM32_DAC_TRIGGERED;
    config.trigger = NX_STM32_DAC_TRIGGER_TIM7;
    TIM7->CR1 = TIM_CR1_OPM | TIM_CR1_UDIS | TIM_CR1_URS | TIM_CR1_ARPE;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_dac_prepare(&state, &config));
    ASSERT_EQ(NX_SUCCESS, nx_dac_port_start(&face, 1000U, 1000U));
    EXPECT_EQ(0U, TIM7->CR1 & (TIM_CR1_OPM | TIM_CR1_UDIS | TIM_CR1_URS |
                               TIM_CR1_ARPE));
    EXPECT_NE(0U, TIM7->CR1 & TIM_CR1_CEN);
}

TEST_F(DacOwner, StartupCannotSucceedWhenTheFinalDelayObservationExpires) {
    config.mode = NX_STM32_DAC_TRIGGERED;
    config.trigger = NX_STM32_DAC_TRIGGER_TIM6;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_dac_prepare(&state, &config));
    const nx_time_us_t deadline = model_time + 17U;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_dac_port_start(&face, 1000U, deadline));
    EXPECT_EQ(0U, TIM6->CR1 & TIM_CR1_CEN);
    EXPECT_TRUE(state.faulted);
    EXPECT_EQ(NX_SUCCESS, nx_dac_port_stop(&face, NX_DEADLINE_NEVER));
}

TEST_F(DacOwner,
       ExpiryDuringTimerEnableRetainsResponsibilityAndReportsTimeout) {
    config.mode = NX_STM32_DAC_TRIGGERED;
    config.trigger = NX_STM32_DAC_TRIGGER_TIM6;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_dac_prepare(&state, &config));
    expire_on_timer_enable = 1000U;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_dac_port_start(&face, 1000U, 900U));
    EXPECT_TRUE(state.faulted);
    EXPECT_TRUE(state.owned);
    EXPECT_EQ(NX_SUCCESS, nx_dac_port_stop(&face, NX_DEADLINE_NEVER));
}

TEST_F(DacOwner, RefusedControlFieldsPreventInitializedPublication) {
    config.mode = NX_STM32_DAC_TRIANGLE;
    config.trigger = NX_STM32_DAC_TRIGGER_SOFTWARE;
    config.amplitude = 255U;
    config.output_buffer = false;
    for (const uint32_t field :
         {static_cast<uint32_t>(DAC_CR_TSEL1 | DAC_CR_TSEL2),
          static_cast<uint32_t>(DAC_CR_WAVE1 | DAC_CR_WAVE2),
          static_cast<uint32_t>(DAC_CR_MAMP1 | DAC_CR_MAMP2),
          static_cast<uint32_t>(DAC_CR_BOFF1 | DAC_CR_BOFF2)}) {
        DAC->CR = 0U;
        refused_control_bits = field;
        EXPECT_EQ(NX_ERROR_IO, nx_stm32_dac_prepare(&state, &config));
        EXPECT_FALSE(state.initialized);
        EXPECT_TRUE(state.owned);
        refused_control_bits = 0U;
        ASSERT_EQ(NX_SUCCESS, nx_stm32_dac_close(&state));
    }
}

TEST_F(DacOwner, RefusedTriggerDisablePreventsTimerSetupEvent) {
    config.mode = NX_STM32_DAC_TRIGGERED;
    config.trigger = NX_STM32_DAC_TRIGGER_TIM6;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_dac_prepare(&state, &config));
    refuse_trigger_clear = true;
    writes.clear();
    EXPECT_EQ(NX_ERROR_IO, nx_dac_port_start(&face, 1000U, 1000U));
    for (const auto& event : writes) {
        EXPECT_NE(&TIM6->EGR, event.reg);
    }
    EXPECT_TRUE(state.faulted);
    refuse_trigger_clear = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_dac_close(&state));
}

TEST_F(DacOwner, ForeignUnderrunInterruptIsRejectedWithoutAcknowledgement) {
    DAC->CR = DAC_CR_DMAUDRIE1 | DAC_CR_DMAUDRIE2;
    DAC->SR = DAC_SR_DMAUDR1;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_dac_prepare(&state, &config));
    EXPECT_EQ(DAC_CR_DMAUDRIE1 | DAC_CR_DMAUDRIE2, DAC->CR);
    EXPECT_EQ(static_cast<uint32_t>(DAC_SR_DMAUDR1), DAC->SR);
    EXPECT_EQ(nullptr, state.config);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_dac_close(&state));
    EXPECT_EQ(DAC_CR_DMAUDRIE1 | DAC_CR_DMAUDRIE2, DAC->CR);
}

static testing::MockFunction<nx_result_t(void*, uint8_t, uint16_t)>* dispatched;
/** \brief External provider boundary observes the exact supplied context. */
static nx_result_t dispatch_write(void* context, uint8_t channel,
                                  uint16_t code) {
    return dispatched->Call(context, channel, code);
}
TEST(DacDispatch, SharedMethodsReceiveDistinctInstanceContexts) {
    testing::MockFunction<nx_result_t(void*, uint8_t, uint16_t)> mock;
    struct Reset {
        ~Reset() {
            dispatched = nullptr;
        }
    } reset;
    dispatched = &mock;
    nx_dac_ops_t selected{};
    selected.write = dispatch_write;
    const nx_dac_ops_t ops = selected;
    unsigned first = 1U;
    unsigned second = 2U;
    const nx_dac_port_t left{&ops, &first};
    const nx_dac_port_t right{&ops, &second};
    EXPECT_CALL(mock, Call(&first, 1U, 123U))
        .WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_CALL(mock, Call(&second, 2U, 456U))
        .WillOnce(testing::Return(NX_ERROR_BUSY));
    EXPECT_EQ(NX_SUCCESS, nx_dac_port_write(&left, 1U, 123U));
    EXPECT_EQ(NX_ERROR_BUSY, nx_dac_port_write(&right, 2U, 456U));
}
TEST(DacDispatch, InvalidFaceAndOutputAliasesAreRejectedBeforeDispatch) {
    const nx_dac_ops_t ops{};
    unsigned state = 0U;
    nx_dac_port_t face{&ops, &state};
    EXPECT_EQ(NX_ERROR_INVALID, nx_dac_port_start(nullptr, 0U, 100U));
    EXPECT_EQ(NX_ERROR_INVALID, nx_dac_port_write(&face, 3U, 0U));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_dac_port_info(&face, reinterpret_cast<nx_dac_info_t*>(&face)));
    EXPECT_EQ(&ops, face.ops);
    EXPECT_EQ(&state, face.context);
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_dac_port_write(&face, 1U, 1U));
}

class DacExternal : public DacOwner {
  protected:
    nx_stm32_exti_hardware_config_t event_config{};
    nx_stm32_exti_hardware_state_t event{};
    void SelectSource(uint8_t line = 9U) {
        RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
        RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
        event_config = {GPIOB, NX_EXTI_RISING, 1U, line, 0U};
        ASSERT_EQ(nx_stm32_exti_hardware_prepare(&event, &event_config),
                  NX_SUCCESS);
        config.mode = NX_STM32_DAC_TRIGGERED;
        config.trigger = NX_STM32_DAC_TRIGGER_EXTI9;
        config.external = &event;
    }
    void Edge() {
        /* Physical input/event routing, followed by the documented TSEL=110
         * digital transfer. There is no provider call or synthetic timer. */
        constexpr uint32_t bit = 1U << 9U;
        if ((EXTI->EMR & EXTI->RTSR & bit) == 0U ||
            ((SYSCFG->EXTICR[2] >> 4U) & 15U) != 1U ||
            (RCC->APB1ENR & RCC_APB1ENR_DACEN) == 0U)
            return;
        for (unsigned index = 0U; index < 2U; ++index) {
            uint32_t shift = 16U * index;
            if ((DAC->CR & (DAC_CR_EN1 << shift)) != 0U &&
                (DAC->CR & (DAC_CR_TEN1 << shift)) != 0U &&
                ((DAC->CR >> shift) & DAC_CR_TSEL1) ==
                    (DAC_CR_TSEL1_2 | DAC_CR_TSEL1_1)) {
                if (index == 0U)
                    DAC->DOR1 = DAC->DHR12R1;
                else
                    DAC->DOR2 = DAC->DHR12R2;
            }
        }
    }
    void TearDown() override {
        refuse_trigger_clear = refuse_disable = refuse_gate_clear = false;
        refuse_wave_clear = false;
        refused_control_bits = 0U;
        (void)nx_stm32_dac_close(&state);
        (void)nx_stm32_exti_hardware_close(&event);
        DacOwner::TearDown();
    }
};

TEST_F(DacExternal, MissingOrWrongEventOwnerRejectsBeforeDacEffects) {
    config.mode = NX_STM32_DAC_TRIGGERED;
    config.trigger = NX_STM32_DAC_TRIGGER_EXTI9;
    EXPECT_EQ(nx_stm32_dac_prepare(&state, &config), NX_ERROR_STATE);
    EXPECT_TRUE(writes.empty());
    ASSERT_NO_FATAL_FAILURE(SelectSource(8U));
    const size_t before = writes.size();
    EXPECT_EQ(nx_stm32_dac_prepare(&state, &config), NX_ERROR_INVALID);
    EXPECT_EQ(writes.size(), before);
    EXPECT_EQ(state.config, nullptr);
}

TEST_F(DacExternal, DualStagingUsesOnlyTheOwnedExternalElectricalEdge) {
    ASSERT_NO_FATAL_FAILURE(SelectSource());
    ASSERT_EQ(nx_stm32_dac_prepare(&state, &config), NX_SUCCESS);
    EXPECT_EQ(DAC->CR & (DAC_CR_TEN1 | DAC_CR_TEN2), 0U);
    EXPECT_EQ(nx_dac_port_write_pair(&face, 0x123U, 0x456U), NX_ERROR_STATE);
    preempt_on_first_time = true;
    ASSERT_EQ(nx_dac_port_start(&face, 0U, 10000U), NX_SUCCESS);
    EXPECT_GE(external_enabled_at - enabled_at, 15U);
    ASSERT_EQ(nx_dac_port_write_pair(&face, 0x123U, 0x456U), NX_SUCCESS);
    EXPECT_EQ(DAC->DOR1 | DAC->DOR2, 0U);
    Edge();
    EXPECT_EQ(DAC->DOR1, 0x123U);
    EXPECT_EQ(DAC->DOR2, 0x456U);
    nx_dac_info_t info{};
    ASSERT_EQ(nx_dac_port_info(&face, &info), NX_SUCCESS);
    EXPECT_TRUE(info.external_trigger);
    EXPECT_FALSE(info.timer_trigger || info.software_trigger);
    nx_dac_state_t observed{};
    ASSERT_EQ(nx_dac_port_state(&face, &observed), NX_SUCCESS);
    EXPECT_EQ(observed.timer_clock_hz | observed.trigger_divisor, 0U);
    EXPECT_EQ(TIM6->CR1 | TIM7->CR1, 0U);
    EXPECT_EQ(RCC->APB1ENR & (RCC_APB1ENR_TIM6EN | RCC_APB1ENR_TIM7EN), 0U);
    ASSERT_EQ(nx_dac_port_stop(&face, 10000U), NX_SUCCESS);
    Edge();
    EXPECT_EQ(DAC->DOR1 | DAC->DOR2, 0U);
    EXPECT_TRUE(event.initialized);
}

TEST_F(DacExternal, NoiseAndTriangleUseTheSameOwnedEventWithoutATimer) {
    ASSERT_NO_FATAL_FAILURE(SelectSource());
    for (nx_stm32_dac_mode_t mode :
         {NX_STM32_DAC_NOISE, NX_STM32_DAC_TRIANGLE}) {
        config.mode = mode;
        config.amplitude = 255U;
        ASSERT_EQ(nx_stm32_dac_prepare(&state, &config), NX_SUCCESS);
        ASSERT_EQ(nx_dac_port_start(&face, 0U, 10000U), NX_SUCCESS);
        EXPECT_EQ(DAC->CR & DAC_CR_WAVE1,
                  mode == NX_STM32_DAC_NOISE ? DAC_CR_WAVE1_0 : DAC_CR_WAVE1_1);
        EXPECT_EQ(DAC->CR & DAC_CR_TSEL1, DAC_CR_TSEL1_2 | DAC_CR_TSEL1_1);
        EXPECT_EQ(TIM6->CR1 | TIM7->CR1, 0U);
        ASSERT_EQ(nx_stm32_dac_close(&state), NX_SUCCESS);
    }
}

TEST_F(DacExternal, InventedFrequencyRejectsWithoutStartingAnyTrigger) {
    ASSERT_NO_FATAL_FAILURE(SelectSource());
    ASSERT_EQ(nx_stm32_dac_prepare(&state, &config), NX_SUCCESS);
    const size_t before = writes.size();
    EXPECT_EQ(nx_dac_port_start(&face, 1000U, 10000U), NX_ERROR_INVALID);
    EXPECT_EQ(writes.size(), before);
    EXPECT_EQ(DAC->CR & (DAC_CR_EN1 | DAC_CR_EN2 | DAC_CR_TEN1 | DAC_CR_TEN2),
              0U);
}

TEST_F(DacExternal, AWithdrawnEventOwnerCannotAuthorizeFurtherWrites) {
    ASSERT_NO_FATAL_FAILURE(SelectSource());
    ASSERT_EQ(nx_stm32_dac_prepare(&state, &config), NX_SUCCESS);
    ASSERT_EQ(nx_dac_port_start(&face, 0U, 10000U), NX_SUCCESS);
    ASSERT_EQ(nx_stm32_exti_hardware_close(&event), NX_SUCCESS);
    const size_t before = writes.size();
    EXPECT_EQ(nx_dac_port_write(&face, 1U, 0x123U), NX_ERROR_STATE);
    EXPECT_EQ(writes.size(), before);
    EXPECT_EQ(nx_dac_port_stop(&face, 10000U), NX_SUCCESS);
}

TEST_F(DacExternal, ChangedMuxCannotArmTheStaticExternalSelection) {
    ASSERT_NO_FATAL_FAILURE(SelectSource());
    ASSERT_EQ(nx_stm32_dac_prepare(&state, &config), NX_SUCCESS);
    SYSCFG->EXTICR[2] ^= 1U << 4U;
    const size_t before = writes.size();
    EXPECT_EQ(nx_dac_port_start(&face, 0U, 10000U), NX_ERROR_STATE);
    EXPECT_EQ(writes.size(), before);
    EXPECT_EQ(DAC->CR & (DAC_CR_EN1 | DAC_CR_EN2), 0U);
}

TEST_F(DacExternal, RefusedDacGateRetainsBothIndependentColdOwners) {
    ASSERT_NO_FATAL_FAILURE(SelectSource());
    ASSERT_EQ(nx_stm32_dac_prepare(&state, &config), NX_SUCCESS);
    ASSERT_EQ(nx_dac_port_start(&face, 0U, 10000U), NX_SUCCESS);
    refuse_trigger_clear = true;
    EXPECT_NE(nx_dac_port_stop(&face, 10000U), NX_SUCCESS);
    EXPECT_EQ(state.config, &config);
    EXPECT_TRUE(event.initialized);
    EXPECT_NE(RCC->APB1ENR & RCC_APB1ENR_DACEN, 0U);
    refuse_trigger_clear = false;
    EXPECT_EQ(nx_dac_port_stop(&face, 10000U), NX_SUCCESS);
}

TEST_F(DacExternal, InvalidTimeAfterOutputEnableCannotBecomeInfiniteWarmup) {
    ASSERT_NO_FATAL_FAILURE(SelectSource());
    ASSERT_EQ(nx_stm32_dac_prepare(&state, &config), NX_SUCCESS);
    lose_time_on_output_enable = true;
    EXPECT_EXIT(
        {
            alarm(2U);
            nx_result_t result =
                nx_dac_port_start(&face, 0U, NX_DEADLINE_NEVER);
            alarm(0U);
            _exit(result == NX_ERROR_STATE &&
                          (DAC->CR & (DAC_CR_TEN1 | DAC_CR_TEN2)) == 0U
                      ? 0
                      : 1);
        },
        testing::ExitedWithCode(0), "");
}
