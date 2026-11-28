/**
 * \file            pma_test.cpp
 *
 * \brief           Verify STM32F1 packet memory addressing and ownership.
 *
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "pma.h"
#include <array>
#include <gtest/gtest.h>

namespace {

using Pma = std::array<uint16_t, NX_STM32F1_USB_PMA_BYTES>;
constexpr uint16_t sentinel = 0xcafeU;

Pma make_pma() {
    Pma pma;
    pma.fill(sentinel);
    return pma;
}

TEST(Stm32F1UsbPma, PopulatesSparseTableWithExactReceiveBlockEncoding) {
    Pma pma = make_pma();
    constexpr nx_stm32f1_usb_pma_endpoint_t endpoints[] = {{64U, 8U, 72U, 8U},
                                                           {80U, 8U, 88U, 64U},
                                                           {0U, 0U, 152U, 63U},
                                                           {0U, 0U, 216U, 62U},
                                                           {0U, 0U, 278U, 1U}};
    ASSERT_EQ(nx_stm32f1_usb_pma_prepare(pma.data(), endpoints, 5U),
              NX_SUCCESS);
    EXPECT_EQ(pma[0], 64U);
    EXPECT_EQ(pma[2], 0U);
    EXPECT_EQ(pma[4], 72U);
    EXPECT_EQ(pma[6], 0x1000U);
    EXPECT_EQ(pma[8], 80U);
    EXPECT_EQ(pma[12], 88U);
    EXPECT_EQ(pma[14], 0x8400U);
    EXPECT_EQ(pma[16], 0U);
    EXPECT_EQ(pma[20], 152U);
    EXPECT_EQ(pma[22], 0x8400U);
    EXPECT_EQ(pma[30], 0x7c00U);
    EXPECT_EQ(pma[38], 0x0400U);
    for (size_t index = 1U; index < 40U; index += 2U) {
        EXPECT_EQ(pma[index], sentinel);
    }
    EXPECT_EQ(pma[40], sentinel);
    EXPECT_EQ(pma[64], sentinel);
}

TEST(Stm32F1UsbPma, RejectsAnInvalidPlanBeforeWritingAnySlot) {
    Pma pma = make_pma();
    const Pma initial = pma;
    const nx_stm32f1_usb_pma_endpoint_t ep0{64U, 8U, 72U, 8U};
    const nx_stm32f1_usb_pma_endpoint_t failures[] = {
        {76U, 8U, 88U, 8U},   {80U, 8U, 84U, 8U},  {81U, 8U, 88U, 8U},
        {32U, 8U, 88U, 8U},   {0U, 0U, 450U, 63U}, {512U, 1U, 0U, 0U},
        {80U, 65U, 160U, 8U}, {80U, 0U, 88U, 8U}};
    for (const auto& failure : failures) {
        const nx_stm32f1_usb_pma_endpoint_t endpoints[] = {ep0, failure};
        EXPECT_EQ(nx_stm32f1_usb_pma_prepare(pma.data(), endpoints, 2U),
                  NX_ERROR_INVALID);
        EXPECT_EQ(pma, initial);
    }
    EXPECT_EQ(nx_stm32f1_usb_pma_prepare(pma.data(), &ep0, 9U),
              NX_ERROR_INVALID);
    EXPECT_EQ(pma, initial);
}

TEST(Stm32F1UsbPma, CopiesOddTailAndPreservesTheUnaddressableHalfwords) {
    Pma pma = make_pma();
    constexpr nx_stm32f1_usb_pma_endpoint_t layout{508U, 3U, 0U, 0U};
    ASSERT_EQ(nx_stm32f1_usb_pma_prepare(pma.data(), &layout, 1U), NX_SUCCESS);
    const uint8_t data[] = {0x13U, 0x57U, 0x9bU};
    ASSERT_EQ(
        nx_stm32f1_usb_pma_transmit(pma.data(), 0, &layout, data, sizeof(data)),
        NX_SUCCESS);
    EXPECT_EQ(pma[2], sizeof(data));
    EXPECT_EQ(pma[508], 0x5713U);
    EXPECT_EQ(pma[510], 0x009bU);
    EXPECT_EQ(pma[509], sentinel);
    EXPECT_EQ(pma[511], sentinel);
}

TEST(Stm32F1UsbPma, ZeroLengthInChangesCountWithoutTouchingPayload) {
    Pma pma = make_pma();
    constexpr nx_stm32f1_usb_pma_endpoint_t layout{64U, 8U, 72U, 8U};
    ASSERT_EQ(nx_stm32f1_usb_pma_prepare(pma.data(), &layout, 1U), NX_SUCCESS);
    pma[2] = 3U;
    ASSERT_EQ(nx_stm32f1_usb_pma_transmit(pma.data(), 0, &layout, nullptr, 0U),
              NX_SUCCESS);
    EXPECT_EQ(pma[2], 0U);
    EXPECT_EQ(pma[64], sentinel);
    EXPECT_EQ(pma[66], sentinel);
}

TEST(Stm32F1UsbPma, ReceivesOnlyTheObservedShortPacketOrZeroLength) {
    Pma pma = make_pma();
    constexpr nx_stm32f1_usb_pma_endpoint_t layout{64U, 8U, 72U, 64U};
    ASSERT_EQ(nx_stm32f1_usb_pma_prepare(pma.data(), &layout, 1U), NX_SUCCESS);
    pma[6] = 0x8403U;
    pma[72] = 0x3412U;
    pma[74] = 0xee56U;
    uint8_t bytes[4] = {0U, 0U, 0U, 0xadU};
    size_t count = 19U;
    ASSERT_EQ(nx_stm32f1_usb_pma_receive(pma.data(), 0, &layout, bytes,
                                         sizeof(bytes), &count),
              NX_SUCCESS);
    EXPECT_EQ(count, 3U);
    EXPECT_EQ(bytes[0], 0x12U);
    EXPECT_EQ(bytes[1], 0x34U);
    EXPECT_EQ(bytes[2], 0x56U);
    EXPECT_EQ(bytes[3], 0xadU);
    pma[6] = 0x8400U;
    ASSERT_EQ(
        nx_stm32f1_usb_pma_receive(pma.data(), 0, &layout, nullptr, 0U, &count),
        NX_SUCCESS);
    EXPECT_EQ(count, 0U);
}

TEST(Stm32F1UsbPma, RejectsOversizedOutWithoutBorrowingOrTouchingOutputs) {
    Pma pma = make_pma();
    constexpr nx_stm32f1_usb_pma_endpoint_t layout{0U, 0U, 64U, 8U};
    ASSERT_EQ(nx_stm32f1_usb_pma_prepare(pma.data(), &layout, 1U), NX_SUCCESS);
    const std::array<uint8_t, 4> original{3U, 4U, 5U, 6U};
    auto bytes = original;
    size_t count = 19U;
    for (uint16_t actual : std::array<uint16_t, 2U>{5U, 10U}) {
        pma[6] = 0x1000U | actual;
        EXPECT_EQ(nx_stm32f1_usb_pma_receive(pma.data(), 0, &layout,
                                             bytes.data(), bytes.size(),
                                             &count),
                  NX_ERROR_OVERFLOW);
        EXPECT_EQ(bytes, original);
        EXPECT_EQ(count, 19U);
    }
    EXPECT_EQ(nx_stm32f1_usb_pma_transmit(pma.data(), 0, &layout, nullptr, 0U),
              NX_ERROR_INVALID);
}

} /* namespace */
