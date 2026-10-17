/**
 * \file            spi_transaction_test.cpp
 * \brief           Enhanced whole-CS words, segments and rejection contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/io/spi_transaction.h"
#include "provider.h"
extern const nx_spi_transaction_ops_t nx_native_spi_transaction_ops;
}
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <limits>

namespace {
class SPITransaction : public testing::Test {
  protected:
    nx_native_spi_state_t state{};
    nx_native_spi_endpoint_state_t device{};
    nx_spi_transaction_endpoint_t endpoint{&nx_native_spi_transaction_ops,
                                           &device};
    size_t transferred = 99U;
    void SetUp() override {
        ASSERT_EQ(nx_native_spi_port_configure_instance(&state), NX_SUCCESS);
        ASSERT_EQ(
            nx_native_spi_configure_instance(&device, &state, nullptr, 0U),
            NX_SUCCESS);
    }
    nx_result_t Transfer(const nx_spi_segment_t* segments, size_t count,
                         uint8_t bits = 8U) {
        return nx_spi_transaction_transfer(&endpoint, segments, count, bits,
                                           nx_time_now_us() + 1000U,
                                           &transferred);
    }
};
TEST_F(SPITransaction, SegmentsShareRegisterCommandAndOneCsInterval) {
    uint8_t registers[4]{0x11U, 0x22U, 0x33U, 0x44U};
    ASSERT_EQ(nx_native_spi_configure_instance(&device, &state, registers, 4U),
              NX_SUCCESS);
    const uint8_t command = 0x81U;
    uint8_t data[2]{};
    const nx_spi_segment_t segments[]{{&command, nullptr, 1U, 0U},
                                      {nullptr, nullptr, 1U, 0xFFU},
                                      {nullptr, data, 2U, 0xFFU}};
    ASSERT_EQ(Transfer(segments, 3U), NX_SUCCESS);
    EXPECT_EQ(transferred, 4U);
    EXPECT_THAT(data, testing::ElementsAre(0x33U, 0x44U));
    EXPECT_FALSE(device.cs);
    EXPECT_FALSE(state.active);
}
TEST_F(SPITransaction, HalfwordsRemainWordsAcrossMultipleSpans) {
    const uint16_t first[]{0x12ABU, 0xFF01U};
    uint16_t first_rx[2]{};
    uint16_t second_rx[2]{};
    const nx_spi_segment_t segments[]{{first, first_rx, 2U, 0U},
                                      {nullptr, second_rx, 2U, 0xA55AU}};
    ASSERT_EQ(Transfer(segments, 2U, 16U), NX_SUCCESS);
    EXPECT_EQ(transferred, 4U);
    EXPECT_THAT(first_rx, testing::ElementsAre(0x12ABU, 0xFF01U));
    EXPECT_THAT(second_rx, testing::ElementsAre(0xA55AU, 0xA55AU));
}
TEST_F(SPITransaction, LateMalformedSegmentRejectsBeforeEarlierWrite) {
    uint8_t rx = 0xA5U;
    const nx_spi_segment_t segments[]{{nullptr, &rx, 1U, 0x11U},
                                      {nullptr, nullptr, 0U, 0U}};
    EXPECT_EQ(Transfer(segments, 2U), NX_ERROR_INVALID);
    EXPECT_EQ(transferred, 0U);
    EXPECT_EQ(rx, 0xA5U);
    EXPECT_FALSE(device.cs);
    EXPECT_FALSE(state.active);
}
TEST_F(SPITransaction, ReceiveCannotOverwriteFutureTransmitOrDescriptor) {
    uint8_t tx[2]{0x11U, 0x22U};
    const nx_spi_segment_t segments[]{{nullptr, &tx[1], 1U, 0x99U},
                                      {&tx[1], nullptr, 1U, 0U}};
    EXPECT_EQ(Transfer(segments, 2U), NX_ERROR_INVALID);
    EXPECT_THAT(tx, testing::ElementsAre(0x11U, 0x22U));
    nx_spi_segment_t descriptor{nullptr, nullptr, 1U, 0U};
    descriptor.rx = &descriptor;
    EXPECT_EQ(Transfer(&descriptor, 1U), NX_ERROR_INVALID);
    EXPECT_FALSE(state.active);
}
TEST_F(SPITransaction, OverflowAndMisalignedHalfwordRejectBeforeCs) {
    uint8_t storage[4]{};
    const nx_spi_segment_t overflow{storage, nullptr,
                                    std::numeric_limits<size_t>::max(), 0U};
    EXPECT_EQ(Transfer(&overflow, 1U, 16U), NX_ERROR_INVALID);
    alignas(uint16_t) uint8_t misaligned[4]{};
    const nx_spi_segment_t odd{misaligned + 1U, nullptr, 1U, 0U};
    EXPECT_EQ(Transfer(&odd, 1U, 16U), NX_ERROR_INVALID);
    EXPECT_FALSE(device.cs);
}
TEST_F(SPITransaction, FaultCountsWholeWordsAndReleasesCsWithoutReplay) {
    uint8_t rx[3]{0xAAU, 0xAAU, 0xAAU};
    const nx_spi_segment_t segments[]{{nullptr, rx, 1U, 0x11U},
                                      {nullptr, rx + 1U, 2U, 0x22U}};
    state.fail_after = 2U;
    EXPECT_EQ(Transfer(segments, 2U), NX_ERROR_IO);
    EXPECT_EQ(transferred, 2U);
    EXPECT_THAT(rx, testing::ElementsAre(0x11U, 0x22U, 0xAAU));
    EXPECT_FALSE(device.cs);
    EXPECT_FALSE(state.active);
}
TEST_F(SPITransaction, LegacyBusyControllerBlocksEnhancedEndpoint) {
    const nx_spi_segment_t segment{nullptr, nullptr, 1U, 0x11U};
    state.active = true;
    EXPECT_EQ(Transfer(&segment, 1U), NX_ERROR_BUSY);
    EXPECT_EQ(transferred, 0U);
    EXPECT_FALSE(device.cs);
    state.active = false;
}
TEST_F(SPITransaction, RejectionNeverDispatchesProvider) {
    testing::StrictMock<testing::MockFunction<nx_result_t()>> observer;
    nx_spi_transaction_ops_t ops{[](void* p, const nx_spi_segment_t*, size_t,
                                    uint8_t, nx_time_us_t, size_t*) {
        return static_cast<testing::MockFunction<nx_result_t()>*>(p)->Call();
    }};
    endpoint = {&ops, &observer};
    const nx_spi_segment_t empty{nullptr, nullptr, 0U, 0U};
    EXPECT_EQ(Transfer(&empty, 1U), NX_ERROR_INVALID);
}
} /* namespace */
