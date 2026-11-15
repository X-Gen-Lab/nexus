/**
 * \file            adc_format_test.cpp
 * \brief           Integer ADC format, complete scan and loan boundary tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/adc_format.h"
#include <array>
#include <cstring>
#include <gtest/gtest.h>

namespace {
nx_adc_format_t regular() {
    return {2U,
            4U,
            0U,
            2U,
            2U,
            12U,
            0U,
            NX_ADC_UNSIGNED,
            NX_ADC_LITTLE_ENDIAN,
            NX_ADC_REGULAR,
            NX_ADC_TRIGGER_SOFTWARE};
}

TEST(ADCFormat, DecodesExistingUnsignedHalfwordScansWithoutScaling) {
    const auto format = regular();
    const uint8_t samples[] = {0x23U, 0xf1U, 0xffU, 0xffU,
                               0x00U, 0x00U, 0xbcU, 0xfaU};
    int64_t value = -1;
    ASSERT_EQ(NX_SUCCESS, nx_adc_format_validate(&format));
    ASSERT_EQ(NX_SUCCESS,
              nx_adc_format_decode(&format, samples, sizeof(samples), 0U, 0U,
                                   &value));
    EXPECT_EQ(0x123, value);
    ASSERT_EQ(NX_SUCCESS,
              nx_adc_format_decode(&format, samples, sizeof(samples), 1U, 1U,
                                   &value));
    EXPECT_EQ(0xabc, value);
}

TEST(ADCFormat, DecodesSignedLeftAlignedBigEndianWithScanPadding) {
    auto format = regular();
    format.sample_stride = 4U;
    format.scan_stride = 12U;
    format.significant_bits = 12U;
    format.lsb_bits = 4U;
    format.encoding = NX_ADC_TWOS_COMPLEMENT;
    format.byte_order = NX_ADC_BIG_ENDIAN;
    format.group = NX_ADC_INJECTED;
    format.trigger = NX_ADC_TRIGGER_TIMER;
    format.trigger_identity = 7U;
    const uint8_t samples[] = {0x80U, 0x0fU, 0xaaU, 0xbbU, 0x7fU, 0xf0U,
                               0xccU, 0xddU, 0xeeU, 0xeeU, 0xeeU, 0xeeU};
    int64_t value = 99;
    ASSERT_EQ(NX_SUCCESS,
              nx_adc_format_decode(&format, samples, sizeof(samples), 0U, 0U,
                                   &value));
    EXPECT_EQ(-2048, value);
    ASSERT_EQ(NX_SUCCESS,
              nx_adc_format_decode(&format, samples, sizeof(samples), 0U, 1U,
                                   &value));
    EXPECT_EQ(2047, value);
}

TEST(ADCFormat, PreservesAllUnsignedAndSigned32BitRawCounts) {
    auto format = regular();
    format.sample_stride = 4U;
    format.scan_stride = 4U;
    format.storage_bytes = 4U;
    format.significant_bits = 32U;
    format.channel_count = 1U;
    const uint8_t samples[] = {0xffU, 0xffU, 0xffU, 0xffU};
    int64_t value = 0;
    ASSERT_EQ(NX_SUCCESS,
              nx_adc_format_decode(&format, samples, sizeof(samples), 0U, 0U,
                                   &value));
    EXPECT_EQ(INT64_C(4294967295), value);
    format.encoding = NX_ADC_TWOS_COMPLEMENT;
    ASSERT_EQ(NX_SUCCESS,
              nx_adc_format_decode(&format, samples, sizeof(samples), 0U, 0U,
                                   &value));
    EXPECT_EQ(-1, value);
}

TEST(ADCFormat, RejectsIncoherentAndUnknownColdFacts) {
    for (unsigned index = 0U; index != 13U; ++index) {
        auto format = regular();
        switch (index) {
            case 0U:
                format.storage_bytes = 0U;
                break;
            case 1U:
                format.storage_bytes = 5U;
                break;
            case 2U:
                format.significant_bits = 0U;
                break;
            case 3U:
                format.significant_bits = 17U;
                break;
            case 4U:
                format.lsb_bits = 5U;
                break;
            case 5U:
                format.sample_stride = 1U;
                break;
            case 6U:
                format.scan_stride = 3U;
                break;
            case 7U:
                format.channel_count = 0U;
                break;
            case 8U:
                format.encoding = static_cast<nx_adc_encoding_t>(3);
                break;
            case 9U:
                format.byte_order = static_cast<nx_adc_byte_order_t>(3);
                break;
            case 10U:
                format.group = static_cast<nx_adc_group_t>(3);
                break;
            case 11U:
                format.trigger = NX_ADC_TRIGGER_UNKNOWN;
                break;
            default:
                format.trigger_identity = 1U;
                break;
        }
        EXPECT_EQ(NX_ERROR_INVALID, nx_adc_format_validate(&format)) << index;
    }
    auto format = regular();
    format.trigger = NX_ADC_TRIGGER_EVENT;
    EXPECT_EQ(NX_ERROR_INVALID, nx_adc_format_validate(&format));
    format.trigger_identity = 1U;
    EXPECT_EQ(NX_SUCCESS, nx_adc_format_validate(&format));
    for (unsigned index = 0U; index != 3U; ++index) {
        format = regular();
        if (index == 0U) {
            format.encoding = NX_ADC_ENCODING_UNKNOWN;
        } else if (index == 1U) {
            format.byte_order = NX_ADC_BYTE_ORDER_UNKNOWN;
        } else {
            format.group = NX_ADC_GROUP_UNKNOWN;
        }
        EXPECT_EQ(NX_ERROR_INVALID, nx_adc_format_validate(&format)) << index;
    }
}

TEST(ADCFormat, RejectsIncompleteScansAndOutOfRangeIndicesWithoutOutputWrites) {
    const auto format = regular();
    const uint8_t samples[8]{};
    int64_t value = 777;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_adc_format_decode(&format, samples, 7U, 0U, 0U, &value));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_adc_format_decode(&format, samples, 8U, 2U, 0U, &value));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_adc_format_decode(&format, samples, 8U, 0U, 2U, &value));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_adc_format_decode(&format, samples, 0U, 0U, 0U, &value));
    EXPECT_EQ(777, value);
}

TEST(ADCFormat, RejectsAliasesAndWrappingSpansBeforeAnySampleAccess) {
    auto format = regular();
    alignas(int64_t) std::array<uint8_t, 16> samples{};
    const auto original = samples;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_adc_format_decode(&format, samples.data(), samples.size(), 0U,
                                   0U,
                                   reinterpret_cast<int64_t*>(samples.data())));
    EXPECT_EQ(original, samples);
    int64_t value = 777;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_adc_format_decode(
                  &format, reinterpret_cast<const void*>(UINTPTR_MAX - 1U), 4U,
                  0U, 0U, &value));
    EXPECT_EQ(
        NX_ERROR_INVALID,
        nx_adc_format_decode(&format, &format, sizeof(format), 0U, 0U, &value));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_adc_format_decode(&format, samples.data(), samples.size(), 0U,
                                   0U, reinterpret_cast<int64_t*>(&format)));
    EXPECT_EQ(777, value);
    format.sample_stride = SIZE_MAX;
    format.scan_stride = SIZE_MAX;
    EXPECT_EQ(NX_ERROR_INVALID, nx_adc_format_validate(&format));
}

TEST(ADCFormat, RejectsNullAndMisalignedDescriptorsAndOutputs) {
    const auto format = regular();
    const uint8_t samples[4]{};
    int64_t value = 777;
    EXPECT_EQ(NX_ERROR_INVALID, nx_adc_format_validate(nullptr));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_adc_format_decode(&format, nullptr, 4U, 0U, 0U, &value));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_adc_format_decode(&format, samples, 4U, 0U, 0U, nullptr));
    alignas(nx_adc_format_t) uint8_t raw[sizeof(nx_adc_format_t) + 1U]{};
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_adc_format_validate(
                  reinterpret_cast<const nx_adc_format_t*>(raw + 1U)));
    alignas(int64_t) uint8_t output[sizeof(int64_t) + 1U]{};
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_adc_format_decode(&format, samples, 4U, 0U, 0U,
                                   reinterpret_cast<int64_t*>(output + 1U)));
    EXPECT_EQ(777, value);
}
} /* namespace */
