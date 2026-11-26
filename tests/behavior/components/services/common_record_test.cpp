/**
 * \file            common_record_test.cpp
 * \brief           Wire record identity, checksum, schema and bounded encoding
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/persistent_record.h"
#include <array>
#include <cstring>
#include <gtest/gtest.h>
namespace {
TEST(CommonRecord, RoundTripPreservesSchemaIdentityAndExactPayload) {
    std::array<uint8_t, 64> image{};
    const std::array<uint8_t, 4> payload{1U, 2U, 3U, 4U};
    size_t bytes = image.size();
    ASSERT_EQ(nx_persistent_record_encode(7U, UINT64_C(0x123456789abcdef0),
                                          payload.data(), payload.size(),
                                          image.data(), &bytes),
              NX_SUCCESS);
    EXPECT_EQ(bytes, 36U);
    nx_persistent_record_view_t view{};
    ASSERT_EQ(nx_persistent_record_decode(7U, image.data(), bytes, &view),
              NX_SUCCESS);
    EXPECT_EQ(view.identifier, UINT64_C(0x123456789abcdef0));
    EXPECT_EQ(view.payload_bytes, payload.size());
    EXPECT_EQ(std::memcmp(view.payload, payload.data(), payload.size()), 0);
}
TEST(CommonRecord, CapacityFailureDoesNotWriteAPrefix) {
    std::array<uint8_t, 4> image{0xa5U, 0xa5U, 0xa5U, 0xa5U};
    size_t bytes = image.size();
    EXPECT_EQ(
        nx_persistent_record_encode(7U, 1U, "x", 1U, image.data(), &bytes),
        NX_ERROR_EXHAUSTED);
    EXPECT_EQ(bytes, 33U);
    EXPECT_EQ(image[0], 0xa5U);
}
TEST(CommonRecord, DamageAndUnknownSchemaNeverPublishAView) {
    std::array<uint8_t, 64> image{};
    size_t bytes = image.size();
    ASSERT_EQ(
        nx_persistent_record_encode(7U, 1U, "data", 4U, image.data(), &bytes),
        NX_SUCCESS);
    nx_persistent_record_view_t view{99U, nullptr, 99U};
    EXPECT_EQ(nx_persistent_record_decode(8U, image.data(), bytes, &view),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(view.identifier, 99U);
    image[32] ^= 1U;
    EXPECT_EQ(nx_persistent_record_decode(7U, image.data(), bytes, &view),
              NX_ERROR_IO);
    EXPECT_EQ(view.identifier, 99U);
}
} /* namespace */
