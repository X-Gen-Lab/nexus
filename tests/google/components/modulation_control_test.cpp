/**
 * \file            modulation_control_test.cpp
 * \brief           Real carrier-window and quantized-voltage planning contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/control/modulation.h"
#include <cmath>
#include <cstring>
#include <gtest/gtest.h>

namespace {
nx_modulation_config_t profile() {
    nx_modulation_config_t c = {};
    c.topology = NX_MODULATION_THREE_SHUNT;
    c.alignment = NX_MODULATION_CENTER_ALIGNED;
    c.period_ticks = 1000;
    c.minimum_duty_ticks = 50;
    c.maximum_duty_ticks = 950;
    c.deadtime_ticks = 2;
    c.settling_ticks = 3;
    c.aperture_ticks = 4;
    c.conversion_ticks = 8;
    c.irq_ticks = 2;
    c.compute_ticks = 10;
    c.rearm_ticks = 2;
    c.commit_before_ticks = 980;
    c.sensor_mask = 7;
    c.samples_per_trigger = 2;
    return c;
}
unsigned vector_at(const nx_modulation_output_t& plan, uint32_t period,
                   uint32_t tick) {
    unsigned vector = 0;
    for (unsigned i = 0; i != 3; ++i) {
        const auto relative =
            (static_cast<uint64_t>(tick) + period - plan.start_ticks[i]) %
            period;
        if (relative < plan.duty_ticks[i])
            vector |= 1U << (2U - i);
    }
    return vector;
}
TEST(ModulationControl, CenteredZeroVoltageHasOneValidLowSideObservation) {
    const auto c = profile();
    nx_modulation_output_t out = {};
    ASSERT_EQ(nx_modulation_plan(&c, {0, 0}, 24, &out), NX_SUCCESS);
    for (unsigned i = 0; i != 3; ++i) {
        EXPECT_EQ(out.duty_ticks[i], 500U);
        EXPECT_EQ(out.start_ticks[i], 750U);
    }
    EXPECT_EQ(out.sample_count, 1);
    EXPECT_EQ(out.samples[0].switching_vector, 0);
    EXPECT_EQ(out.samples[0].phase_mask, 3);
    EXPECT_FLOAT_EQ(out.voltage.alpha, 0);
    EXPECT_FLOAT_EQ(out.voltage.beta, 0);
    EXPECT_FALSE(out.phase_shifted);
}
TEST(ModulationControl, FinalVoltageComesFromQuantizedPulseWidths) {
    const auto c = profile();
    nx_modulation_output_t out = {};
    ASSERT_EQ(nx_modulation_plan(&c, {8.123F, 4.567F}, 24, &out), NX_SUCCESS);
    const float a = 24 * static_cast<float>(out.duty_ticks[0]) / 1000;
    const float b = 24 * static_cast<float>(out.duty_ticks[1]) / 1000;
    const float d = 24 * static_cast<float>(out.duty_ticks[2]) / 1000;
    EXPECT_NEAR(out.voltage.alpha, (2 * a - b - d) / 3, 1e-5F);
    EXPECT_NEAR(out.voltage.beta, (b - d) / std::sqrt(3.0F), 1e-5F);
    for (const auto width : out.duty_ticks) {
        EXPECT_EQ(width % 2, 0U);
        EXPECT_GE(width, c.minimum_duty_ticks);
        EXPECT_LE(width, c.maximum_duty_ticks);
    }
}
TEST(ModulationControl, RequestedVoltageScalesCoherentlyAtPulseLimit) {
    const auto c = profile();
    nx_modulation_output_t out = {};
    ASSERT_EQ(nx_modulation_plan(&c, {100, 30}, 24, &out), NX_SUCCESS);
    EXPECT_TRUE(out.limited);
    EXPECT_NEAR(out.voltage.beta / out.voltage.alpha, 0.3F, 0.01F);
}
TEST(ModulationControl, ThreeShuntChoosesObservableTwoPhasesAtHighDuty) {
    auto c = profile();
    c.aperture_ticks = 60;
    c.conversion_ticks = 65;
    nx_modulation_output_t out = {};
    ASSERT_EQ(nx_modulation_plan(&c, {100, 0}, 24, &out), NX_SUCCESS);
    EXPECT_EQ(out.samples[0].phase_mask, 6);
    EXPECT_EQ(out.samples[0].switching_vector, 4);
    c.topology = NX_MODULATION_TWO_SHUNT;
    c.sensor_mask = 3;
    EXPECT_EQ(nx_modulation_plan(&c, {100, 0}, 24, &out), NX_ERROR_UNSUPPORTED);
}
TEST(ModulationControl, ThreeSimultaneousValuesRequireAllThreeLowSides) {
    auto c = profile();
    c.samples_per_trigger = 3;
    nx_modulation_output_t out = {};
    ASSERT_EQ(nx_modulation_plan(&c, {8, 3}, 24, &out), NX_SUCCESS);
    EXPECT_EQ(out.samples[0].phase_mask, 7);
    EXPECT_EQ(out.samples[0].switching_vector, 0);
}
TEST(ModulationControl, EdgeAlignedPlanUsesSameLogicalFullCarrierContract) {
    auto c = profile();
    c.alignment = NX_MODULATION_EDGE_ALIGNED;
    nx_modulation_output_t out = {};
    ASSERT_EQ(nx_modulation_plan(&c, {4, 3}, 24, &out), NX_SUCCESS);
    for (const auto start : out.start_ticks)
        EXPECT_EQ(start, 0U);
    EXPECT_GT(out.samples[0].tick, 0U);
}
TEST(ModulationControl, SingleShuntNeedsTwoIndependentActiveVectors) {
    auto c = profile();
    c.topology = NX_MODULATION_SINGLE_SHUNT;
    c.sensor_mask = 0;
    c.samples_per_trigger = 1;
    nx_modulation_output_t out = {};
    ASSERT_EQ(nx_modulation_plan(&c, {8, 5}, 24, &out), NX_SUCCESS);
    ASSERT_EQ(out.sample_count, 2);
    EXPECT_NE(out.samples[0].phase_mask, out.samples[1].phase_mask);
    EXPECT_LT(out.samples[0].tick, out.samples[1].tick);
    EXPECT_GE(out.samples[1].tick - out.samples[0].tick,
              c.conversion_ticks + c.irq_ticks + c.rearm_ticks);
    for (const auto& sample : out.samples) {
        EXPECT_GT(sample.switching_vector, 0);
        EXPECT_LT(sample.switching_vector, 7);
        EXPECT_EQ(sample.switching_vector,
                  vector_at(out, c.period_ticks, sample.tick));
    }
}
TEST(ModulationControl, UnobservableSingleShuntDoesNotPublishCandidate) {
    auto c = profile();
    c.topology = NX_MODULATION_SINGLE_SHUNT;
    c.sensor_mask = 0;
    c.samples_per_trigger = 1;
    nx_modulation_output_t out = {};
    out.duty_ticks[0] = 99;
    const auto before = out;
    EXPECT_EQ(nx_modulation_plan(&c, {0, 0}, 24, &out), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(std::memcmp(&out, &before, sizeof(out)), 0);
}
TEST(ModulationControl, SingleShuntCanMoveFirstApertureToAvoidAdcCollision) {
    auto c = profile();
    c.alignment = NX_MODULATION_EDGE_ALIGNED;
    c.topology = NX_MODULATION_SINGLE_SHUNT;
    c.sensor_mask = 0;
    c.samples_per_trigger = 1;
    c.conversion_ticks = 160;
    nx_modulation_output_t out = {};
    ASSERT_EQ(nx_modulation_plan(&c, {1.68F, 2.632717F}, 24, &out), NX_SUCCESS);
    ASSERT_EQ(out.sample_count, 2);
    EXPECT_GE(out.samples[1].tick - out.samples[0].tick,
              c.conversion_ticks + c.irq_ticks + c.rearm_ticks);
}
TEST(ModulationControl, ExplicitFixedPhaseShiftPreservesAverageVoltage) {
    auto c = profile();
    c.topology = NX_MODULATION_SINGLE_SHUNT;
    c.sensor_mask = 0;
    c.samples_per_trigger = 1;
    c.single_shunt_shift_ticks = 20;
    nx_modulation_output_t out = {};
    ASSERT_EQ(nx_modulation_plan(&c, {0, 0}, 24, &out), NX_SUCCESS);
    EXPECT_TRUE(out.phase_shifted);
    EXPECT_FLOAT_EQ(out.voltage.alpha, 0);
    EXPECT_FLOAT_EQ(out.voltage.beta, 0);
    for (const auto width : out.duty_ticks)
        EXPECT_EQ(width, 500U);
    for (const auto& sample : out.samples)
        EXPECT_EQ(sample.switching_vector,
                  vector_at(out, c.period_ticks, sample.tick));
}
TEST(ModulationControl, EverySectorKeepsAperturesAndCommitInsideBudget) {
    const auto c = profile();
    for (unsigned sector = 0; sector != 12; ++sector) {
        const float angle = static_cast<float>(sector) * 0.5235987756F;
        nx_modulation_output_t out = {};
        ASSERT_EQ(nx_modulation_plan(
                      &c, {9 * std::cos(angle), 9 * std::sin(angle)}, 24, &out),
                  NX_SUCCESS);
        const auto& sample = out.samples[0];
        EXPECT_GE(sample.tick, sample.window_begin);
        EXPECT_LE(sample.tick + c.aperture_ticks, sample.window_end);
        EXPECT_LT(sample.tick + c.conversion_ticks + c.irq_ticks +
                      c.compute_ticks,
                  c.commit_before_ticks);
        EXPECT_EQ(sample.switching_vector,
                  vector_at(out, c.period_ticks, sample.tick));
        unsigned lows = 0;
        for (unsigned phase = 0; phase != 3; ++phase)
            if ((sample.switching_vector & (1U << (2U - phase))) == 0)
                lows |= 1U << phase;
        EXPECT_EQ(sample.phase_mask & lows, sample.phase_mask);
    }
}
TEST(ModulationControl, WideBudgetArithmeticRejectsWithoutWrap) {
    auto c = profile();
    c.deadtime_ticks = UINT32_MAX;
    c.settling_ticks = 100;
    EXPECT_EQ(nx_modulation_check(&c), NX_ERROR_INVALID);
    c = profile();
    c.compute_ticks = UINT32_MAX;
    EXPECT_EQ(nx_modulation_check(&c), NX_ERROR_INVALID);
    c = profile();
    c.period_ticks = (1U << 24) + 1U;
    EXPECT_EQ(nx_modulation_check(&c), NX_ERROR_INVALID);
    c = profile();
    c.minimum_duty_ticks = 51;
    EXPECT_EQ(nx_modulation_check(&c), NX_ERROR_INVALID);
}
TEST(ModulationControl, InputAndOutputAliasesRejectWithoutMutation) {
    auto c = profile();
    const auto before = c;
    EXPECT_EQ(nx_modulation_plan(&c, {0, 0}, 24,
                                 reinterpret_cast<nx_modulation_output_t*>(&c)),
              NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&c, &before, sizeof(c)), 0);
    nx_modulation_output_t out = {};
    EXPECT_EQ(nx_modulation_plan(&c, {NAN, 0}, 24, &out), NX_ERROR_INVALID);
    EXPECT_EQ(nx_modulation_plan(&c, {0, 0}, 0, &out), NX_ERROR_INVALID);
}
TEST(ModulationControl, DirectProfilesHaveExplicitTopologyAdmission) {
    auto c = profile();
    nx_modulation_output_t out = {};
    ASSERT_EQ(nx_modulation_phase_plan(&c, {0, 0}, 24, &out), NX_SUCCESS);
    EXPECT_EQ(out.sample_count, 1);
    EXPECT_EQ(nx_modulation_bus_plan(&c, {0, 0}, 24, &out), NX_ERROR_INVALID);
    c.topology = NX_MODULATION_SINGLE_SHUNT;
    c.sensor_mask = 0;
    c.samples_per_trigger = 1;
    ASSERT_EQ(nx_modulation_bus_plan(&c, {8, 5}, 24, &out), NX_SUCCESS);
    EXPECT_EQ(out.sample_count, 2);
    EXPECT_EQ(nx_modulation_phase_plan(&c, {8, 5}, 24, &out), NX_ERROR_INVALID);
}
TEST(ModulationControl, ProviderCaptureSlotRestrictsCenterCarrierHalf) {
    auto c = profile();
    c.capture_begin_ticks = 600;
    c.capture_end_ticks = 900;
    nx_modulation_output_t out = {};
    ASSERT_EQ(nx_modulation_phase_plan(&c, {100, 0}, 24, &out), NX_SUCCESS);
    EXPECT_GE(out.samples[0].tick, c.capture_begin_ticks);
    EXPECT_LE(out.samples[0].tick + c.conversion_ticks + c.irq_ticks +
                  c.rearm_ticks,
              c.capture_end_ticks);
}
TEST(ModulationControl, BothSingleShuntCapturesBelongToProviderSlot) {
    auto c = profile();
    c.topology = NX_MODULATION_SINGLE_SHUNT;
    c.sensor_mask = 0;
    c.samples_per_trigger = 1;
    c.capture_begin_ticks = 600;
    c.capture_end_ticks = 900;
    nx_modulation_output_t out = {};
    ASSERT_EQ(nx_modulation_bus_plan(&c, {8, 5}, 24, &out), NX_SUCCESS);
    for (const auto& sample : out.samples) {
        EXPECT_GE(sample.tick, c.capture_begin_ticks);
        EXPECT_LE(sample.tick + c.conversion_ticks + c.irq_ticks +
                      c.rearm_ticks,
                  c.capture_end_ticks);
    }
}
TEST(ModulationControl, InvalidOrInsufficientCaptureSlotsReject) {
    auto c = profile();
    c.capture_begin_ticks = 500;
    c.capture_end_ticks = 510;
    EXPECT_EQ(nx_modulation_check(&c), NX_ERROR_INVALID);
    c.capture_end_ticks = 1001;
    EXPECT_EQ(nx_modulation_check(&c), NX_ERROR_INVALID);
    c.capture_end_ticks = 499;
    EXPECT_EQ(nx_modulation_check(&c), NX_ERROR_INVALID);
    c.capture_begin_ticks = UINT32_MAX;
    c.capture_end_ticks = 0;
    EXPECT_EQ(nx_modulation_check(&c), NX_ERROR_INVALID);
}
} /* namespace */
