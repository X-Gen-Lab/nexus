/**
 * \file            common_debounce_test.cpp
 * \brief           Real explicit-time input qualification contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/debounce.h"
#include <gtest/gtest.h>

namespace {
void expect_same_state(const nx_debounce_t& actual,
                       const nx_debounce_t& expected) {
    EXPECT_EQ(actual.rise_us, expected.rise_us);
    EXPECT_EQ(actual.fall_us, expected.fall_us);
    EXPECT_EQ(actual.candidate_since, expected.candidate_since);
    EXPECT_EQ(actual.last_observed, expected.last_observed);
    EXPECT_EQ(actual.stable, expected.stable);
    EXPECT_EQ(actual.candidate, expected.candidate);
    EXPECT_EQ(actual.initialized, expected.initialized);
}

class CommonDebounce : public ::testing::Test {
  protected:
    nx_debounce_t debounce = {};
    nx_debounce_observation_t observation = {};
    void SetUp() override {
        ASSERT_EQ(nx_debounce_initialize(&debounce, false, 10, 20, 100),
                  NX_SUCCESS);
    }
};

TEST_F(CommonDebounce, RiseQualifiesAtExactBoundaryAndEmitsOneChange) {
    ASSERT_EQ(nx_debounce_observe(&debounce, true, 101, &observation),
              NX_SUCCESS);
    EXPECT_FALSE(observation.stable);
    EXPECT_FALSE(observation.changed);
    ASSERT_EQ(nx_debounce_observe(&debounce, true, 110, &observation),
              NX_SUCCESS);
    EXPECT_FALSE(observation.stable);
    EXPECT_FALSE(observation.changed);
    ASSERT_EQ(nx_debounce_observe(&debounce, true, 111, &observation),
              NX_SUCCESS);
    EXPECT_TRUE(observation.stable);
    EXPECT_TRUE(observation.changed);
    ASSERT_EQ(nx_debounce_observe(&debounce, true, 111, &observation),
              NX_SUCCESS);
    EXPECT_TRUE(observation.stable);
    EXPECT_FALSE(observation.changed);
}

TEST_F(CommonDebounce, FallingEdgeUsesItsIndependentHoldDuration) {
    ASSERT_EQ(nx_debounce_initialize(&debounce, true, 10, 20, 100), NX_SUCCESS);
    ASSERT_EQ(nx_debounce_observe(&debounce, false, 105, &observation),
              NX_SUCCESS);
    ASSERT_EQ(nx_debounce_observe(&debounce, false, 124, &observation),
              NX_SUCCESS);
    EXPECT_TRUE(observation.stable);
    ASSERT_EQ(nx_debounce_observe(&debounce, false, 125, &observation),
              NX_SUCCESS);
    EXPECT_FALSE(observation.stable);
    EXPECT_TRUE(observation.changed);
}

TEST_F(CommonDebounce, ReturningToStableLevelRestartsCandidateTime) {
    ASSERT_EQ(nx_debounce_observe(&debounce, true, 101, &observation),
              NX_SUCCESS);
    ASSERT_EQ(nx_debounce_observe(&debounce, false, 109, &observation),
              NX_SUCCESS);
    EXPECT_FALSE(observation.changed);
    ASSERT_EQ(nx_debounce_observe(&debounce, true, 110, &observation),
              NX_SUCCESS);
    ASSERT_EQ(nx_debounce_observe(&debounce, true, 119, &observation),
              NX_SUCCESS);
    EXPECT_FALSE(observation.stable);
    ASSERT_EQ(nx_debounce_observe(&debounce, true, 120, &observation),
              NX_SUCCESS);
    EXPECT_TRUE(observation.stable);
    EXPECT_TRUE(observation.changed);
}

TEST_F(CommonDebounce, ZeroHoldChangesImmediatelyAtFirstObservation) {
    ASSERT_EQ(nx_debounce_initialize(&debounce, false, 0, 0, 100), NX_SUCCESS);
    ASSERT_EQ(nx_debounce_observe(&debounce, true, 100, &observation),
              NX_SUCCESS);
    EXPECT_TRUE(observation.stable);
    EXPECT_TRUE(observation.changed);
    ASSERT_EQ(nx_debounce_observe(&debounce, false, 100, &observation),
              NX_SUCCESS);
    EXPECT_FALSE(observation.stable);
    EXPECT_TRUE(observation.changed);
}

TEST_F(CommonDebounce, InitialLevelIsTrustedWithoutAnInferredEdge) {
    ASSERT_EQ(nx_debounce_observe(&debounce, false, 200, &observation),
              NX_SUCCESS);
    EXPECT_FALSE(observation.stable);
    EXPECT_FALSE(observation.changed);
    ASSERT_EQ(nx_debounce_initialize(&debounce, true, 10, 20, 1), NX_SUCCESS);
    ASSERT_EQ(nx_debounce_observe(&debounce, true, 1, &observation),
              NX_SUCCESS);
    EXPECT_TRUE(observation.stable);
    EXPECT_FALSE(observation.changed);
}

TEST_F(CommonDebounce, LongGapStartsAtObservedChangeRatherThanPreviousSample) {
    ASSERT_EQ(nx_debounce_observe(&debounce, true, 10000, &observation),
              NX_SUCCESS);
    EXPECT_FALSE(observation.stable);
    EXPECT_FALSE(observation.changed);
    ASSERT_EQ(nx_debounce_observe(&debounce, true, 10010, &observation),
              NX_SUCCESS);
    EXPECT_TRUE(observation.stable);
    EXPECT_TRUE(observation.changed);
}

TEST_F(CommonDebounce, ClockReversalPreservesCandidateAndOutput) {
    ASSERT_EQ(nx_debounce_observe(&debounce, true, 110, &observation),
              NX_SUCCESS);
    const auto saved = debounce;
    observation = {true, true};
    EXPECT_EQ(nx_debounce_observe(&debounce, false, 109, &observation),
              NX_ERROR_STATE);
    expect_same_state(debounce, saved);
    EXPECT_TRUE(observation.stable);
    EXPECT_TRUE(observation.changed);
    ASSERT_EQ(nx_debounce_observe(&debounce, true, 120, &observation),
              NX_SUCCESS);
    EXPECT_TRUE(observation.changed);
}

TEST_F(CommonDebounce, FiniteDomainNearExhaustionDoesNotNeedFutureAddition) {
    const nx_time_us_t end = NX_DEADLINE_NEVER - 1;
    ASSERT_EQ(nx_debounce_initialize(&debounce, false, 2, 2, end - 3),
              NX_SUCCESS);
    ASSERT_EQ(nx_debounce_observe(&debounce, true, end - 2, &observation),
              NX_SUCCESS);
    EXPECT_FALSE(observation.stable);
    ASSERT_EQ(nx_debounce_observe(&debounce, true, end, &observation),
              NX_SUCCESS);
    EXPECT_TRUE(observation.stable);
    EXPECT_TRUE(observation.changed);
}

TEST_F(CommonDebounce, InvalidObservationArgumentsLeaveStateAndOutputAlone) {
    const auto saved = debounce;
    observation = {true, true};
    EXPECT_EQ(nx_debounce_observe(nullptr, true, 100, &observation),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_debounce_observe(&debounce, true, 100, nullptr),
              NX_ERROR_INVALID);
    EXPECT_EQ(
        nx_debounce_observe(&debounce, true, NX_DEADLINE_NEVER, &observation),
        NX_ERROR_INVALID);
    EXPECT_EQ(nx_debounce_observe(
                  &debounce, true, 100,
                  reinterpret_cast<nx_debounce_observation_t*>(&debounce)),
              NX_ERROR_INVALID);
    expect_same_state(debounce, saved);
    EXPECT_TRUE(observation.stable);
    EXPECT_TRUE(observation.changed);
}

TEST_F(CommonDebounce, InvalidInitializeDoesNotReplaceState) {
    const auto saved = debounce;
    EXPECT_EQ(nx_debounce_initialize(nullptr, true, 1, 1, 0), NX_ERROR_INVALID);
    EXPECT_EQ(nx_debounce_initialize(&debounce, true, NX_DEADLINE_NEVER, 1, 0),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_debounce_initialize(&debounce, true, 1, NX_DEADLINE_NEVER, 0),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_debounce_initialize(&debounce, true, 1, 1, NX_DEADLINE_NEVER),
              NX_ERROR_INVALID);
    expect_same_state(debounce, saved);
}

TEST(CommonDebounceConstruction, ObserveBeforeInitializePreservesOutput) {
    nx_debounce_t debounce = {};
    nx_debounce_observation_t observation = {true, true};
    EXPECT_EQ(nx_debounce_observe(&debounce, false, 0, &observation),
              NX_ERROR_STATE);
    EXPECT_TRUE(observation.stable);
    EXPECT_TRUE(observation.changed);
    EXPECT_FALSE(debounce.initialized);
}

TEST(CommonDebounceConstruction, InstancesHaveIndependentCandidateWindows) {
    nx_debounce_t first = {}, second = {};
    nx_debounce_observation_t observation = {};
    ASSERT_EQ(nx_debounce_initialize(&first, false, 10, 10, 0), NX_SUCCESS);
    ASSERT_EQ(nx_debounce_initialize(&second, false, 10, 10, 0), NX_SUCCESS);
    ASSERT_EQ(nx_debounce_observe(&first, true, 1, &observation), NX_SUCCESS);
    ASSERT_EQ(nx_debounce_observe(&second, true, 5, &observation), NX_SUCCESS);
    ASSERT_EQ(nx_debounce_observe(&first, true, 11, &observation), NX_SUCCESS);
    EXPECT_TRUE(observation.stable);
    ASSERT_EQ(nx_debounce_observe(&second, true, 11, &observation), NX_SUCCESS);
    EXPECT_FALSE(observation.stable);
    ASSERT_EQ(nx_debounce_observe(&second, true, 15, &observation), NX_SUCCESS);
    EXPECT_TRUE(observation.stable);
}
} /* namespace */
