/**
 * \file            common_cmsis_nn_test.cpp
 * \brief           Selected upstream dense inference and rejection contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/algorithms/cmsis_nn.h"
#include <array>
#include <gtest/gtest.h>
#include <limits>

namespace {
class Dense : public ::testing::Test {
  protected:
    std::array<int8_t, 6> weights{1, 2, -1, -2, 0, 1};
    std::array<int32_t, 2> bias{2, 1};
    std::array<int8_t, 3> input{3, -2, 5};
    std::array<int8_t, 2> output{99, 98};
    nx_cmsis_nn_dense_s8_t layer{3U,
                                 2U,
                                 weights.data(),
                                 bias.data(),
                                 0,
                                 0,
                                 std::numeric_limits<int32_t>::max(),
                                 0,
                                 -128,
                                 127};

    nx_result_t run() {
        return nx_cmsis_nn_dense_s8(&layer, input.data(), input.size(),
                                    output.data(), output.size());
    }
};

TEST_F(Dense, ExecutesTheLockedUpstreamQuantizedLayer) {
    ASSERT_EQ(run(), NX_SUCCESS);
    EXPECT_EQ(output[0], -4);
    EXPECT_EQ(output[1], 0);
    layer.activation_min = 0;
    ASSERT_EQ(run(), NX_SUCCESS);
    EXPECT_EQ(output[0], 0);
    EXPECT_EQ(output[1], 0);
}

TEST_F(Dense, IndependentCallsRetainNoMutableHistory) {
    ASSERT_EQ(run(), NX_SUCCESS);
    const auto first = output;
    input.fill(0);
    ASSERT_EQ(run(), NX_SUCCESS);
    EXPECT_EQ(output[0], 2);
    EXPECT_EQ(output[1], 1);
    input = {3, -2, 5};
    ASSERT_EQ(run(), NX_SUCCESS);
    EXPECT_EQ(output, first);
}

TEST_F(Dense, RejectsCapacityAndUnsupportedQuantizationBeforeOutput) {
    const auto before = output;
    EXPECT_EQ(nx_cmsis_nn_dense_s8(&layer, input.data(), input.size(),
                                   output.data(), 1U),
              NX_ERROR_INVALID);
    layer.shift = 1;
    EXPECT_EQ(run(), NX_ERROR_INVALID);
    layer.shift = -31;
    EXPECT_EQ(run(), NX_ERROR_INVALID);
    layer.shift = 0;
    layer.multiplier = 0;
    EXPECT_EQ(run(), NX_ERROR_INVALID);
    EXPECT_EQ(output, before);
}

TEST_F(Dense, RejectsAccumulatorOverflowAndOverlappingStorage) {
    const auto before = output;
    bias[0] = std::numeric_limits<int32_t>::max();
    EXPECT_EQ(run(), NX_ERROR_INVALID);
    bias[0] = 2;
    EXPECT_EQ(nx_cmsis_nn_dense_s8(&layer, input.data(), input.size(),
                                   input.data(), output.size()),
              NX_ERROR_INVALID);
    EXPECT_EQ(output, before);
}
} /* namespace */
