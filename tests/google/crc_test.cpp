/**
 * \file            crc_test.cpp
 * \brief           CRC word units, exact dispatch and independent observations
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/io/crc.h"
}
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
struct Backend {
    MOCK_METHOD(nx_result_t, compute,
                (const uint32_t*, size_t, nx_time_us_t, uint32_t*));
};
nx_result_t compute(void* context, const uint32_t* words, size_t count,
                    nx_time_us_t deadline, uint32_t* result) {
    return static_cast<Backend*>(context)->compute(words, count, deadline,
                                                   result);
}
const nx_crc_ops_t operations{compute};
class Crc : public testing::Test {
  protected:
    testing::StrictMock<Backend> backend;
    nx_crc_port_t port{&operations, &backend};
    uint32_t words[2]{0x12345678U, 0x34567890U};
    uint32_t result = 0xabcdef12U;
};
} /* namespace */

TEST_F(Crc, DispatchPreservesWordsCountDeadlineAndOutputStorage) {
    EXPECT_CALL(backend, compute(words, 2U, 123456U, &result))
        .WillOnce(testing::DoAll(testing::SetArgPointee<3>(0x87654321U),
                                 testing::Return(NX_SUCCESS)));
    EXPECT_EQ(nx_crc_port_compute(&port, words, 2U, 123456U, &result),
              NX_SUCCESS);
    EXPECT_EQ(result, 0x87654321U);
}
TEST_F(Crc, EmptySequenceNeedsNoInputStorageOrPadding) {
    EXPECT_CALL(backend, compute(nullptr, 0U, NX_DEADLINE_NEVER, &result))
        .WillOnce(testing::DoAll(testing::SetArgPointee<3>(0xffffffffU),
                                 testing::Return(NX_SUCCESS)));
    EXPECT_EQ(
        nx_crc_port_compute(&port, nullptr, 0U, NX_DEADLINE_NEVER, &result),
        NX_SUCCESS);
    EXPECT_EQ(result, 0xffffffffU);
}
TEST_F(Crc, MalformedFacesDoNotDispatchOrChangeTheObservation) {
    EXPECT_EQ(nx_crc_port_compute(nullptr, words, 2U, 10U, &result),
              NX_ERROR_INVALID);
    port.ops = nullptr;
    EXPECT_EQ(nx_crc_port_compute(&port, words, 2U, 10U, &result),
              NX_ERROR_INVALID);
    port = {&operations, nullptr};
    EXPECT_EQ(nx_crc_port_compute(&port, words, 2U, 10U, &result),
              NX_ERROR_INVALID);
    EXPECT_EQ(result, 0xabcdef12U);
}
TEST_F(Crc, MissingComputationIsExplicitlyUnsupported) {
    const nx_crc_ops_t unsupported{};
    port.ops = &unsupported;
    EXPECT_EQ(nx_crc_port_compute(&port, words, 2U, 10U, &result),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(result, 0xabcdef12U);
}
TEST_F(Crc, InputMustBeAlignedNonwrappingAndPresentWhenNonempty) {
    EXPECT_EQ(nx_crc_port_compute(&port, nullptr, 1U, 10U, &result),
              NX_ERROR_INVALID);
    const auto* unaligned = reinterpret_cast<const uint32_t*>(
        reinterpret_cast<uintptr_t>(words) + 1U);
    EXPECT_EQ(nx_crc_port_compute(&port, unaligned, 1U, 10U, &result),
              NX_ERROR_INVALID);
    EXPECT_EQ(
        nx_crc_port_compute(&port, words, SIZE_MAX / 4U + 1U, 10U, &result),
        NX_ERROR_INVALID);
    EXPECT_EQ(result, 0xabcdef12U);
}
TEST_F(Crc, OutputCannotReplaceTheInputOrInterface) {
    EXPECT_EQ(nx_crc_port_compute(&port, words, 2U, 10U, words + 1U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_crc_port_compute(&port, words, 2U, 10U,
                                  reinterpret_cast<uint32_t*>(&port)),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_crc_port_compute(&port, words, 2U, 10U, nullptr),
              NX_ERROR_INVALID);
    EXPECT_EQ(words[0], 0x12345678U);
    EXPECT_EQ(words[1], 0x34567890U);
}
TEST_F(Crc, ProviderFailuresAreReturnedWithoutInventingAnotherResult) {
    EXPECT_CALL(backend, compute(words, 2U, 10U, &result))
        .WillOnce(testing::Return(NX_ERROR_TIMEOUT));
    EXPECT_EQ(nx_crc_port_compute(&port, words, 2U, 10U, &result),
              NX_ERROR_TIMEOUT);
    EXPECT_EQ(result, 0xabcdef12U);
}
