/**
 * \file            g474_pwm_group_test.cpp
 * \brief           Production group atomicity and latched-break register tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_pwm_group_model.h"
#include "stm32g474_pwm_group.h"
#include <array>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>

extern "C" {
TIM_TypeDef g_group_timer{};
COMP_TypeDef g_group_comparator{};
RCC_TypeDef g_group_clocks{};
nx_stm32g474_system_t g_nx_stm32g474_system{};
bool g_group_irq_quiesced = true;
}
namespace {
std::array<uint32_t, 4> live{};
std::vector<std::array<uint32_t, 4>> updates;
volatile uint32_t* refused = nullptr;
bool refused_once = false;
unsigned writes = 0U;
unsigned software_updates = 0U;
bool escaped_bootstrap = false;
testing::MockFunction<void(bool)>* output_observer = nullptr;

void update() {
    if ((g_group_timer.CR1 & TIM_CR1_UDIS) != 0U) {
        return;
    }
    live = {g_group_timer.CCR1, g_group_timer.CCR2, g_group_timer.CCR3,
            g_group_timer.CCR4};
    updates.push_back(live);
}
} /* namespace */
extern "C" uint32_t nx_group_model_read(volatile uint32_t* address) {
    return *address;
}
extern "C" void nx_group_model_write(volatile uint32_t* address,
                                     uint32_t value) {
    ++writes;
    if (address == refused && !refused_once) {
        refused_once = true;
        return;
    }
    if (address == &g_group_timer.SR) {
        *address &= value;
    } else if (address == &g_group_timer.EGR) {
        if ((value & TIM_EGR_UG) != 0U) {
            ++software_updates;
            if ((g_group_timer.CR2 & TIM_CR2_MMS2) == 0U ||
                (g_group_timer.CR2 & TIM_CR2_MMS2) == TIM_CR2_MMS2_1) {
                escaped_bootstrap = true;
            }
            update();
            g_group_timer.SR |= TIM_SR_UIF;
        }
    } else {
        *address = value;
    }
    if (address == &g_group_timer.BDTR) {
        if ((g_group_comparator.CSR & COMP_CSR_VALUE) != 0U &&
            (g_group_timer.BDTR & TIM_BDTR_BKE) != 0U) {
            g_group_timer.BDTR &= ~static_cast<uint32_t>(TIM_BDTR_MOE);
            g_group_timer.SR |= TIM_SR_BIF;
        }
        if (output_observer != nullptr) {
            output_observer->Call((g_group_timer.BDTR & TIM_BDTR_MOE) != 0U);
        }
    }
    /* A counter rollover may occur after every external MMIO write. UDIS
     * protects the complete batch independently of the local CPU IRQ mask. */
    if ((g_group_timer.CR1 & TIM_CR1_CEN) != 0U) {
        update();
    }
}
namespace {
class G474PwmGroup : public testing::Test {
  protected:
    nx_stm32g474_pwm_group_config_t config{1000000U, 1000U, 64U, COMP_CSR_EN,
                                           true};
    nx_stm32g474_pwm_group_state_t state{};
    nx_pwm_group_t face{&nx_stm32g474_pwm_group_ops, &state};
    void SetUp() override {
        g_group_timer = {};
        g_group_comparator = {};
        g_group_clocks = {};
        g_nx_stm32g474_system = {};
        g_nx_stm32g474_system.started = true;
        g_nx_stm32g474_system.core_hz = 170000000U;
        g_nx_stm32g474_system.rcc = &g_group_clocks;
        g_group_clocks.APB2ENR = RCC_APB2ENR_TIM1EN | RCC_APB2ENR_SYSCFGEN;
        g_group_comparator.CSR = COMP_CSR_EN;
        g_group_irq_quiesced = true;
        live = {};
        updates.clear();
        refused = nullptr;
        refused_once = false;
        writes = 0U;
        software_updates = 0U;
        escaped_bootstrap = false;
        output_observer = nullptr;
    }
    void initialize_and_start() {
        ASSERT_EQ(NX_SUCCESS,
                  nx_stm32g474_pwm_group_initialize(&state, &config));
        ASSERT_EQ(NX_SUCCESS, nx_pwm_group_start(&face));
    }
};

TEST_F(G474PwmGroup,
       InitializesDisabledWithoutExportingSoftwareUpdateTriggers) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32g474_pwm_group_initialize(&state, &config));
    EXPECT_TRUE(state.retained);
    EXPECT_TRUE(state.initialized);
    EXPECT_EQ(169U, g_group_timer.PSC);
    EXPECT_EQ(999U, g_group_timer.ARR);
    EXPECT_EQ(0U, g_group_timer.CCER);
    EXPECT_EQ(0U, g_group_timer.BDTR & (TIM_BDTR_MOE | TIM_BDTR_AOE));
    EXPECT_EQ(TIM1_AF1_BKCMP1E, g_group_timer.AF1);
    EXPECT_FALSE(escaped_bootstrap);
    EXPECT_EQ(1U, live[3]);
}

TEST_F(G474PwmGroup, WholeBatchAndTriggerLatchTogetherUnderAnyCounterRollover) {
    initialize_and_start();
    updates.clear();
    const uint32_t duties[] = {0U, 500U, 1000U};
    const nx_pwm_group_frame_t frame{duties, 3U, 250U};
    const unsigned before = software_updates;
    ASSERT_EQ(NX_SUCCESS, nx_pwm_group_commit(&face, &frame));
    ASSERT_FALSE(updates.empty());
    for (const auto& values : updates) {
        EXPECT_EQ((std::array<uint32_t, 4>{0U, 500U, 1000U, 250U}), values);
    }
    EXPECT_EQ(before, software_updates);
    EXPECT_EQ(0U, g_group_timer.CR1 & TIM_CR1_UDIS);
    EXPECT_EQ(0U, g_group_timer.CCR1);
    EXPECT_GT(g_group_timer.CCR3, g_group_timer.ARR);
    EXPECT_FALSE(escaped_bootstrap);
}

TEST_F(G474PwmGroup, InvalidFinalDutyOrTriggerRejectsBeforeAnyWrite) {
    initialize_and_start();
    uint32_t duties[] = {100U, 200U, 1001U};
    nx_pwm_group_frame_t frame{duties, 3U, 100U};
    const unsigned before = writes;
    EXPECT_EQ(NX_ERROR_INVALID, nx_pwm_group_commit(&face, &frame));
    duties[2] = 300U;
    frame.trigger_ticks = 1000U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_pwm_group_commit(&face, &frame));
    frame.trigger_ticks = 0U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_pwm_group_commit(&face, &frame));
    frame.trigger_ticks = 100U;
    frame.channel_count = 2U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_pwm_group_commit(&face, &frame));
    EXPECT_EQ(before, writes);
}

TEST_F(G474PwmGroup, FailedShadowWriteNeverPublishesAPartialLiveGroup) {
    initialize_and_start();
    const auto original = live;
    updates.clear();
    refused = &g_group_timer.CCR2;
    const uint32_t duties[] = {100U, 200U, 300U};
    const nx_pwm_group_frame_t frame{duties, 3U, 400U};
    EXPECT_EQ(NX_ERROR_IO, nx_pwm_group_commit(&face, &frame));
    for (const auto& values : updates) {
        EXPECT_EQ(original, values);
    }
    EXPECT_EQ(0U, g_group_timer.BDTR & TIM_BDTR_MOE);
    EXPECT_EQ(0U, g_group_timer.CCER);
    EXPECT_EQ(0U, g_group_timer.CR1 & TIM_CR1_CEN);
    EXPECT_TRUE(state.fault);
    EXPECT_EQ(NX_ERROR_STATE, nx_pwm_group_start(&face));
    refused = nullptr;
    ASSERT_EQ(NX_SUCCESS, nx_pwm_group_clear_fault(&face));
    EXPECT_EQ(original, live);
    EXPECT_EQ(0U, g_group_timer.BDTR & TIM_BDTR_MOE);
    EXPECT_EQ(NX_SUCCESS, nx_pwm_group_start(&face));
}

TEST_F(G474PwmGroup, HardwareBreakStaysLatchedUntilExplicitDisabledRecovery) {
    initialize_and_start();
    g_group_comparator.CSR |= COMP_CSR_VALUE;
    g_group_timer.SR |= TIM_SR_BIF;
    g_group_timer.BDTR &= ~static_cast<uint32_t>(TIM_BDTR_MOE);
    testing::StrictMock<testing::MockFunction<void(bool)>> observer;
    output_observer = &observer;
    EXPECT_CALL(observer, Call(false)).Times(testing::AtLeast(1));
    nx_stm32g474_pwm_group_irq(&state);
    EXPECT_TRUE(state.fault);
    EXPECT_EQ(0U, g_group_timer.BDTR & TIM_BDTR_MOE);
    EXPECT_EQ(NX_ERROR_STATE, nx_pwm_group_start(&face));
    EXPECT_EQ(NX_SUCCESS, nx_pwm_group_stop(&face));
    EXPECT_EQ(NX_ERROR_BUSY, nx_pwm_group_clear_fault(&face));
    g_group_comparator.CSR &= ~static_cast<uint32_t>(COMP_CSR_VALUE);
    EXPECT_EQ(NX_SUCCESS, nx_pwm_group_clear_fault(&face));
    EXPECT_FALSE(state.fault);
    EXPECT_EQ(0U, g_group_timer.BDTR & (TIM_BDTR_MOE | TIM_BDTR_AOE));
    output_observer = nullptr;
}

TEST_F(G474PwmGroup, InactivePulseWithPendingBreakCannotBeClearedByStart) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32g474_pwm_group_initialize(&state, &config));
    g_group_timer.SR |= TIM_SR_BIF;
    EXPECT_EQ(NX_ERROR_STATE, nx_pwm_group_start(&face));
    EXPECT_TRUE(state.fault);
    EXPECT_EQ(0U, g_group_timer.BDTR & TIM_BDTR_MOE);
}

TEST_F(G474PwmGroup,
       ForeignOwnerAndComparatorConfigurationRejectWithoutEffects) {
    config.exclusive = false;
    EXPECT_EQ(NX_ERROR_PERMISSION,
              nx_stm32g474_pwm_group_initialize(&state, &config));
    config.exclusive = true;
    g_group_timer.SMCR = TIM_SMCR_SMS_0;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32g474_pwm_group_initialize(&state, &config));
    g_group_timer.SMCR = 0U;
    g_group_comparator.CSR |= COMP_CSR_POLARITY;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32g474_pwm_group_initialize(&state, &config));
    EXPECT_EQ(0U, writes);
    EXPECT_FALSE(state.retained);
}

TEST_F(G474PwmGroup, CloseRetainsLeasesUntilSharedVectorPublisherIsJoined) {
    initialize_and_start();
    ASSERT_EQ(NX_SUCCESS, nx_pwm_group_stop(&face));
    g_group_irq_quiesced = false;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32g474_pwm_group_close(&state));
    EXPECT_TRUE(state.retained);
    g_group_irq_quiesced = true;
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_pwm_group_close(&state));
    EXPECT_FALSE(state.retained);
    EXPECT_EQ(nullptr, state.config);
}

TEST_F(G474PwmGroup, SnapshotsAndBatchesRejectProviderAndRegisterAliases) {
    initialize_and_start();
    const unsigned before = writes;
    nx_pwm_group_frame_t frame{state.duty_ticks, 3U, 100U};
    EXPECT_EQ(NX_ERROR_INVALID, nx_pwm_group_commit(&face, &frame));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_pwm_group_state(
                  &face, reinterpret_cast<nx_pwm_group_state_t*>(&state)));
    EXPECT_EQ(before, writes);
    nx_pwm_group_state_t observation{};
    EXPECT_EQ(NX_SUCCESS, nx_pwm_group_state(&face, &observation));
    EXPECT_EQ(3U, observation.channel_count);
    EXPECT_TRUE(observation.running);
    EXPECT_FALSE(observation.fault);
}

TEST_F(G474PwmGroup, RefusedSoftwareUpdateCannotClaimLoadedInitialShadows) {
    refused = &g_group_timer.EGR;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32g474_pwm_group_initialize(&state, &config));
    EXPECT_TRUE(state.retained);
    EXPECT_FALSE(state.initialized);
    EXPECT_EQ(0U, g_group_timer.BDTR & TIM_BDTR_MOE);
    EXPECT_EQ(0U, g_group_timer.CCER);
    EXPECT_EQ(0U, g_group_timer.CR1 & TIM_CR1_CEN);
}

TEST_F(G474PwmGroup, ColdStateAndConfigurationCannotAliasOwnedHardware) {
    const auto original = g_group_timer;
    EXPECT_EQ(
        NX_ERROR_INVALID,
        nx_stm32g474_pwm_group_initialize(
            reinterpret_cast<nx_stm32g474_pwm_group_state_t*>(&g_group_timer),
            &config));
    EXPECT_EQ(0, std::memcmp(&original, &g_group_timer, sizeof(original)));
    EXPECT_EQ(0U, writes);
    std::memcpy(&g_group_timer, &config, sizeof(config));
    const auto containing_config = g_group_timer;
    EXPECT_EQ(
        NX_ERROR_INVALID,
        nx_stm32g474_pwm_group_initialize(
            &state, reinterpret_cast<const nx_stm32g474_pwm_group_config_t*>(
                        &g_group_timer)));
    EXPECT_EQ(0, std::memcmp(&containing_config, &g_group_timer,
                             sizeof(containing_config)));
    EXPECT_EQ(0U, writes);
    EXPECT_FALSE(state.retained);
}

TEST_F(G474PwmGroup,
       ForeignTriggerIrqOrChannelSourcesRejectBeforeCommitEffects) {
    initialize_and_start();
    const uint32_t duties[] = {100U, 200U, 300U};
    const nx_pwm_group_frame_t frame{duties, 3U, 400U};
    const uint32_t trigger = g_group_timer.CR2;
    const uint32_t interrupt = g_group_timer.DIER;
    const uint32_t channels = g_group_timer.CCER;
    const unsigned before = writes;
    for (unsigned index = 0U; index != 3U; ++index) {
        g_group_timer.CR2 = trigger;
        g_group_timer.DIER = interrupt;
        g_group_timer.CCER = channels;
        if (index == 0U) {
            g_group_timer.CR2 = TIM_CR2_MMS2_1;
        } else if (index == 1U) {
            g_group_timer.DIER |= TIM_DIER_UDE;
        } else {
            g_group_timer.CCER = 0U;
        }
        EXPECT_EQ(NX_ERROR_STATE, nx_pwm_group_commit(&face, &frame)) << index;
        EXPECT_EQ(before, writes) << index;
    }
}

TEST_F(G474PwmGroup, RefusedMasterDisableRetainsQuarantineUntilProvenRetry) {
    initialize_and_start();
    refused = &g_group_timer.BDTR;
    EXPECT_EQ(NX_ERROR_IO, nx_pwm_group_stop(&face));
    EXPECT_TRUE(state.retained);
    EXPECT_TRUE(state.quarantined);
    EXPECT_TRUE(state.fault);
    EXPECT_EQ(NX_ERROR_STATE, nx_pwm_group_start(&face));
    refused = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_pwm_group_stop(&face));
    EXPECT_FALSE(state.quarantined);
    EXPECT_TRUE(state.fault);
    EXPECT_EQ(NX_SUCCESS, nx_pwm_group_clear_fault(&face));
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_pwm_group_close(&state));
    EXPECT_FALSE(state.retained);
}

TEST_F(G474PwmGroup, MissingComparatorControlClockRejectsWithoutAcquisition) {
    /* A retained CSR.EN bit in the MMIO model does not establish the clock
     * lease needed by the actual comparator control interface. */
    ASSERT_NE(0U, g_group_comparator.CSR & COMP_CSR_EN);
    g_group_clocks.APB2ENR = RCC_APB2ENR_TIM1EN;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32g474_pwm_group_initialize(&state, &config));
    EXPECT_EQ(0U, writes);
    EXPECT_FALSE(state.retained);
    EXPECT_FALSE(state.initialized);
}

TEST_F(G474PwmGroup, AssertedComparatorControlResetRejectsWithoutAcquisition) {
    g_group_clocks.APB2RSTR = RCC_APB2RSTR_SYSCFGRST;
    ASSERT_NE(0U, g_group_comparator.CSR & COMP_CSR_EN);
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32g474_pwm_group_initialize(&state, &config));
    EXPECT_EQ(0U, writes);
    EXPECT_FALSE(state.retained);
    EXPECT_FALSE(state.initialized);
}
} /* namespace */
