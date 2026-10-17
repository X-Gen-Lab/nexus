/**
 * \file            g474_adc_group_admission_factory_test.cpp
 * \brief           Actual generated ADC group cold observed-cut admission
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
/* Each target compiles its actual normal generated assembly. The bridge maps
 * FLASHSIZE_BASE to g_ag.flash_kib. The existing coarse
 * identity hooks also read that same full IDCODE/Flash-size storage. There is
 * no mirrored identity state or replacement source/comparison callback. */
#include "g474_adc_group_factory_model.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32g474_system.h"
#include <array>
#include <cstdlib>
#include <gtest/gtest.h>

namespace {
class G474ADCGroupAdmissionFactory : public testing::Test {
  protected:
    static void Observed(uint16_t revision, uint16_t device = 0x469U,
                         uint16_t kib = 512U) {
        g_ag.debug.IDCODE = (static_cast<uint32_t>(revision) << 16U) | device;
        g_ag.flash_kib = kib;
    }
    void SetUp() override {
        nx_ag_reset(0x2003U);
        nx_g474_model_reset();
        Observed(0x2003U);
        g_ag_factory_refuse_priority = false;
    }
    void TearDown() override {
        g_ag.refused = nullptr;
        g_g474_model.ignore_write = nullptr;
        g_ag_factory_refuse_priority = false;
        if (nx_platform_stop() != NX_SUCCESS) {
            std::abort();
        }
        EXPECT_EQ(g_ag.forbidden_writes, 0U);
    }
    static void ExpectRejectedWithoutEffects(nx_result_t expected) {
        const auto result = nx_platform_start();
        EXPECT_EQ(expected, result.primary);
        EXPECT_EQ(NX_SUCCESS, result.cleanup);
        EXPECT_EQ(0U, result.remaining_effects);
        EXPECT_FALSE(g_nx_stm32g474_system.started);
        EXPECT_EQ(0U, g_g474_model.writes);
        EXPECT_EQ(0U, g_ag.writes);
    }
};

TEST_F(G474ADCGroupAdmissionFactory, ColdFactoryDoesNotAdmitOrInitialize) {
    Observed(0x7fffU);
    const auto* port = nx_factory_adc_group(NX_ADC_GROUP_ID_SAMPLES);
    EXPECT_NE(nullptr, port);
    EXPECT_EQ(port, nx_factory_adc_group(NX_ADC_GROUP_ID_SAMPLES));
    EXPECT_FALSE(g_nx_stm32g474_system.started);
    EXPECT_EQ(0U, g_g474_model.writes);
    EXPECT_EQ(0U, g_ag.writes);
    ExpectRejectedWithoutEffects(NX_ERROR_UNSUPPORTED);
}

TEST_F(G474ADCGroupAdmissionFactory, ActualXStartsTheSelectedImmutableLayout) {
    const auto result = nx_platform_start();
    ASSERT_EQ(NX_SUCCESS, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    nx_adc_group_info_t info{};
    ASSERT_EQ(NX_SUCCESS,
              nx_adc_group_info(nx_factory_adc_group(NX_ADC_GROUP_ID_SAMPLES),
                                &info));
    EXPECT_EQ(2U, info.format.sample_stride);
    EXPECT_EQ(4U, info.format.scan_stride);
    EXPECT_EQ(21250000U, info.adc_clock_hz);
}

TEST_F(G474ADCGroupAdmissionFactory, NamedXRejectsKnownOtherCutsBeforeEffects) {
    for (uint16_t revision : std::array<uint16_t, 2U>{0x2001U, 0x2002U}) {
        Observed(revision);
        ExpectRejectedWithoutEffects(NX_ERROR_UNSUPPORTED);
    }
}

TEST_F(G474ADCGroupAdmissionFactory, ZeroAndUnknownAreObservedButUnreviewed) {
    for (uint16_t revision : std::array<uint16_t, 2U>{0U, 0x7fffU}) {
        Observed(revision);
        ExpectRejectedWithoutEffects(NX_ERROR_UNSUPPORTED);
    }
}

TEST_F(G474ADCGroupAdmissionFactory, WrongGroupCannotBorrowAReviewedCutNumber) {
    Observed(0x2003U, 0x450U);
    ExpectRejectedWithoutEffects(NX_ERROR_IO);
}

TEST_F(G474ADCGroupAdmissionFactory, WrongDensityRejectsWithoutRepair) {
    Observed(0x2003U, 0x469U, 256U);
    ExpectRejectedWithoutEffects(NX_ERROR_IO);
}

TEST_F(G474ADCGroupAdmissionFactory, MissingDensityHasNoSdkFallback) {
    for (uint16_t kib : std::array<uint16_t, 2U>{0U, UINT16_MAX}) {
        Observed(0x2003U, 0x469U, kib);
        ExpectRejectedWithoutEffects(NX_ERROR_IO);
    }
}

TEST_F(G474ADCGroupAdmissionFactory, RejectionDoesNotCacheAdmissionOrFailure) {
    Observed(0x2002U);
    ExpectRejectedWithoutEffects(NX_ERROR_UNSUPPORTED);
    Observed(0x2003U);
    ASSERT_EQ(NX_SUCCESS, nx_platform_start().primary);
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
    g_g474_model.writes = 0U;
    g_ag.writes = 0U;
    Observed(0x2002U);
    ExpectRejectedWithoutEffects(NX_ERROR_UNSUPPORTED);
}
} /* namespace */
