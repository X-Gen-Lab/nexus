/**
 * \file            platform_lifecycle_test.cpp
 * \brief           Default platform ordering and retained cleanup contracts.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/platform_lifecycle.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>

namespace {
using testing::InSequence;
using testing::Return;
using testing::StrictMock;

/** \brief External platform owners; the production coordinator remains real. */
class Ports {
  public:
    MOCK_METHOD(nx_result_t, context, ());
    MOCK_METHOD(nx_result_t, admit, (size_t));
    MOCK_METHOD(nx_platform_start_result_t, system_start, ());
    MOCK_METHOD(nx_result_t, resources_acquire, ());
    MOCK_METHOD(nx_result_t, resources_release, ());
    MOCK_METHOD(nx_result_t, system_release, (bool));
    MOCK_METHOD(nx_lifecycle_acquisition_t, acquire, (size_t));
    MOCK_METHOD(nx_result_t, release, (size_t));
};

Ports* current;

/** \brief Model only the externally established CPU execution context. */
nx_result_t context() {
    return current->context();
}

/** \brief Dispatch an effect-free controller admission. */
nx_result_t admit(size_t index) {
    return current->admit(index);
}

/** \brief Preserve the dedicated system owner's complete startup result. */
nx_platform_start_result_t system_start() {
    return current->system_start();
}

/** \brief Acquire selected shared resources through their concrete owner. */
nx_result_t resources_acquire() {
    return current->resources_acquire();
}

/** \brief Release shared resources, including any permanent-owner guard. */
nx_result_t resources_release() {
    return current->resources_release();
}

/** \brief Keep system withdrawal after every dependent owner's release. */
nx_result_t system_release(bool started) {
    return current->system_release(started);
}

/** \brief Return actual acquisition responsibility from one typed owner. */
nx_lifecycle_acquisition_t acquire(size_t index) {
    return current->acquire(index);
}

/** \brief Retry only the selected controller's outstanding receipt. */
nx_result_t release(size_t index) {
    return current->release(index);
}

/** \brief One platform cursor; no mutable per-controller dispatch storage. */
class PlatformLifecycle : public testing::Test {
  protected:
    StrictMock<Ports> ports;
    const size_t ordinals[3]{0U, 2U, 3U};
    nx_platform_lifecycle_plan_t plan{3U,
                                      ordinals,
                                      context,
                                      admit,
                                      system_start,
                                      resources_acquire,
                                      resources_release,
                                      system_release,
                                      acquire,
                                      release,
                                      false};
    nx_platform_lifecycle_state_t state{};
    nx_platform_diagnostic_t diagnostic{};

    void SetUp() override {
        current = &ports;
    }

    void TearDown() override {
        current = nullptr;
    }

    /** \brief Expect every preflight before the system acquires resources. */
    void admission() {
        EXPECT_CALL(ports, context()).WillOnce(Return(NX_SUCCESS));
        for (size_t index = 0U; index < 3U; ++index) {
            EXPECT_CALL(ports, admit(index)).WillOnce(Return(NX_SUCCESS));
        }
    }

    /** \brief Establish a live platform with the actual C coordinator. */
    void start() {
        admission();
        EXPECT_CALL(ports, system_start())
            .WillOnce(
                Return(nx_platform_start_result_t{NX_SUCCESS, NX_SUCCESS, 0U}));
        EXPECT_CALL(ports, resources_acquire()).WillOnce(Return(NX_SUCCESS));
        for (size_t index = 0U; index < 3U; ++index) {
            EXPECT_CALL(ports, acquire(index))
                .WillOnce(Return(nx_lifecycle_acquisition_t{NX_SUCCESS, true}));
        }
        ASSERT_EQ(
            nx_platform_lifecycle_start(&plan, &state, &diagnostic).primary,
            NX_SUCCESS);
    }
};

TEST_F(PlatformLifecycle,
       AdmissionAndReleaseUsePhysicalOrderAndResolvedOrdinals) {
    InSequence order;
    start();
    EXPECT_EQ(state.initialized, 3U);
    EXPECT_TRUE(state.started);
    EXPECT_EQ(diagnostic.phase, NX_PLATFORM_PHASE_NONE);
    EXPECT_EQ(diagnostic.controller_index, SIZE_MAX);
    EXPECT_CALL(ports, context()).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, release(2U)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, release(1U)).WillOnce(Return(NX_ERROR_BUSY));
    EXPECT_EQ(nx_platform_lifecycle_stop(&plan, &state, &diagnostic),
              NX_ERROR_BUSY);
    EXPECT_EQ(state.initialized, 2U);
    EXPECT_EQ(diagnostic.phase, NX_PLATFORM_PHASE_CONTROLLER);
    EXPECT_EQ(diagnostic.controller_index, 2U);
    EXPECT_CALL(ports, context()).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, release(1U)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, release(0U)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, resources_release()).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, system_release(true)).WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(nx_platform_lifecycle_stop(&plan, &state, &diagnostic),
              NX_SUCCESS);
    EXPECT_FALSE(state.started);
    EXPECT_EQ(state.bound, nullptr);
    EXPECT_EQ(diagnostic.phase, NX_PLATFORM_PHASE_NONE);
}

TEST_F(PlatformLifecycle, RefusedAdmissionHasNoAcquisitionOrRollback) {
    InSequence order;
    EXPECT_CALL(ports, context()).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, admit(0U)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, admit(1U)).WillOnce(Return(NX_ERROR_INVALID));
    const auto result = nx_platform_lifecycle_start(&plan, &state, &diagnostic);
    EXPECT_EQ(result.primary, NX_ERROR_INVALID);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.remaining_effects, 0U);
    EXPECT_EQ(diagnostic.phase, NX_PLATFORM_PHASE_CONTROLLER);
    EXPECT_EQ(diagnostic.controller_index, 2U);
    EXPECT_EQ(diagnostic.cleanup_phase, NX_PLATFORM_PHASE_NONE);
    EXPECT_EQ(state.bound, nullptr);
}

TEST_F(PlatformLifecycle, FailedRetainedAcquisitionPreservesPrimaryAndCleanup) {
    InSequence order;
    admission();
    EXPECT_CALL(ports, system_start())
        .WillOnce(
            Return(nx_platform_start_result_t{NX_SUCCESS, NX_SUCCESS, 0U}));
    EXPECT_CALL(ports, resources_acquire()).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, acquire(0U))
        .WillOnce(Return(nx_lifecycle_acquisition_t{NX_SUCCESS, true}));
    EXPECT_CALL(ports, acquire(1U))
        .WillOnce(Return(nx_lifecycle_acquisition_t{NX_ERROR_IO, true}));
    EXPECT_CALL(ports, context()).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, release(1U)).WillOnce(Return(NX_ERROR_BUSY));
    const auto result = nx_platform_lifecycle_start(&plan, &state, &diagnostic);
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_EQ(result.cleanup, NX_ERROR_BUSY);
    EXPECT_EQ(result.remaining_effects, 1U);
    EXPECT_EQ(diagnostic.controller_index, 2U);
    EXPECT_EQ(diagnostic.cleanup_phase, NX_PLATFORM_PHASE_CONTROLLER);
    EXPECT_EQ(diagnostic.cleanup_controller_index, 2U);
    EXPECT_EQ(state.initialized, 2U);
    EXPECT_CALL(ports, context()).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, release(1U)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, release(0U)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, resources_release()).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, system_release(true)).WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(nx_platform_lifecycle_stop(&plan, &state, nullptr), NX_SUCCESS);
}

TEST_F(PlatformLifecycle, FailedUnretainedAcquisitionSkipsThatController) {
    InSequence order;
    admission();
    EXPECT_CALL(ports, system_start())
        .WillOnce(
            Return(nx_platform_start_result_t{NX_SUCCESS, NX_SUCCESS, 0U}));
    EXPECT_CALL(ports, resources_acquire()).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, acquire(0U))
        .WillOnce(Return(nx_lifecycle_acquisition_t{NX_ERROR_IO, false}));
    EXPECT_CALL(ports, context()).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, resources_release()).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, system_release(true)).WillOnce(Return(NX_SUCCESS));
    const auto result = nx_platform_lifecycle_start(&plan, &state, nullptr);
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.remaining_effects, 0U);
    EXPECT_FALSE(state.started);
}

TEST_F(PlatformLifecycle, SystemFailureKeepsItsIndependentReceiptAndErrors) {
    InSequence order;
    admission();
    EXPECT_CALL(ports, system_start())
        .WillOnce(Return(
            nx_platform_start_result_t{NX_ERROR_IO, NX_ERROR_TIMEOUT, 7U}));
    const auto result = nx_platform_lifecycle_start(&plan, &state, &diagnostic);
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_EQ(result.cleanup, NX_ERROR_TIMEOUT);
    EXPECT_EQ(result.remaining_effects, 7U);
    EXPECT_TRUE(state.started);
    EXPECT_EQ(state.bound, &plan);
    EXPECT_EQ(diagnostic.phase, NX_PLATFORM_PHASE_SYSTEM);
    EXPECT_EQ(diagnostic.cleanup_phase, NX_PLATFORM_PHASE_SYSTEM);
}

TEST_F(PlatformLifecycle, ResourceFailureRollsBackWithSeparatePhase) {
    InSequence order;
    admission();
    EXPECT_CALL(ports, system_start())
        .WillOnce(
            Return(nx_platform_start_result_t{NX_SUCCESS, NX_SUCCESS, 0U}));
    EXPECT_CALL(ports, resources_acquire()).WillOnce(Return(NX_ERROR_IO));
    EXPECT_CALL(ports, context()).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, resources_release()).WillOnce(Return(NX_ERROR_BUSY));
    const auto result = nx_platform_lifecycle_start(&plan, &state, &diagnostic);
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_EQ(result.cleanup, NX_ERROR_BUSY);
    EXPECT_EQ(diagnostic.phase, NX_PLATFORM_PHASE_RESOURCES);
    EXPECT_EQ(diagnostic.cleanup_phase, NX_PLATFORM_PHASE_RESOURCES);
}

TEST_F(PlatformLifecycle,
       PermanentOwnerGuardRunsWithoutAnInitializedController) {
    plan.permanent_effects = true;
    EXPECT_CALL(ports, context()).WillOnce(Return(NX_ERROR_CONTEXT));
    const auto result = nx_platform_lifecycle_start(&plan, &state, nullptr);
    EXPECT_EQ(result.primary, NX_ERROR_CONTEXT);
    EXPECT_EQ(result.remaining_effects, 1U);
    EXPECT_CALL(ports, context()).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, resources_release()).WillOnce(Return(NX_ERROR_BUSY));
    EXPECT_EQ(nx_platform_lifecycle_stop(&plan, &state, &diagnostic),
              NX_ERROR_BUSY);
    EXPECT_EQ(diagnostic.phase, NX_PLATFORM_PHASE_RESOURCES);
}

TEST_F(PlatformLifecycle, StartAndStopReentryReturnBusyWithoutCallingOwners) {
    InSequence order;
    EXPECT_CALL(ports, context()).WillOnce([&] {
        EXPECT_EQ(nx_platform_lifecycle_start(&plan, &state, nullptr).primary,
                  NX_ERROR_BUSY);
        EXPECT_EQ(nx_platform_lifecycle_stop(&plan, &state, nullptr),
                  NX_ERROR_BUSY);
        return NX_ERROR_CONTEXT;
    });
    EXPECT_EQ(nx_platform_lifecycle_start(&plan, &state, nullptr).primary,
              NX_ERROR_CONTEXT);
    EXPECT_FALSE(state.operating);
}

TEST_F(PlatformLifecycle, ReentryCannotEraseTheOuterFailureDiagnostic) {
    InSequence order;
    EXPECT_CALL(ports, context()).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, admit(0U)).WillOnce([&](size_t) {
        EXPECT_EQ(
            nx_platform_lifecycle_start(&plan, &state, &diagnostic).primary,
            NX_ERROR_BUSY);
        EXPECT_EQ(nx_platform_lifecycle_stop(&plan, &state, &diagnostic),
                  NX_ERROR_BUSY);
        return NX_ERROR_INVALID;
    });
    const auto result = nx_platform_lifecycle_start(&plan, &state, &diagnostic);
    EXPECT_EQ(result.primary, NX_ERROR_INVALID);
    EXPECT_EQ(diagnostic.phase, NX_PLATFORM_PHASE_CONTROLLER);
    EXPECT_EQ(diagnostic.controller_index, 0U);
}

TEST_F(PlatformLifecycle, LivePlatformRejectsASecondStartAndDifferentPlan) {
    InSequence order;
    start();
    EXPECT_CALL(ports, context()).WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(nx_platform_lifecycle_start(&plan, &state, nullptr).primary,
              NX_ERROR_STATE);
    const auto other = plan;
    EXPECT_EQ(nx_platform_lifecycle_stop(&other, &state, nullptr),
              NX_ERROR_STATE);
    EXPECT_EQ(state.initialized, 3U);
}

TEST_F(PlatformLifecycle, InvalidPlansRejectBeforeAnyCallback) {
    EXPECT_EQ(nx_platform_lifecycle_start(nullptr, &state, nullptr).primary,
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_platform_lifecycle_stop(&plan, nullptr, nullptr),
              NX_ERROR_INVALID);
    plan.ordinals = nullptr;
    EXPECT_EQ(nx_platform_lifecycle_start(&plan, &state, nullptr).primary,
              NX_ERROR_INVALID);
}

TEST_F(PlatformLifecycle, CoordinationStateFitsTwoNativePointers) {
    EXPECT_LE(sizeof(nx_platform_lifecycle_state_t), 2U * sizeof(void*));
}

TEST_F(PlatformLifecycle, UnrepresentableControllerCountRejectsBeforeContext) {
    plan.controller_count = static_cast<size_t>(UINT16_MAX) + 1U;
    ON_CALL(ports, context()).WillByDefault(Return(NX_ERROR_CONTEXT));
    EXPECT_CALL(ports, context()).Times(0);
    EXPECT_EQ(nx_platform_lifecycle_start(&plan, &state, nullptr).primary,
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_platform_lifecycle_stop(&plan, &state, nullptr),
              NX_ERROR_INVALID);
    EXPECT_EQ(state.bound, nullptr);
    EXPECT_EQ(state.initialized, 0U);
}

TEST_F(PlatformLifecycle, MaximumControllerPrefixDoesNotWrapOrLoseRelease) {
    std::vector<size_t> positions(UINT16_MAX);
    for (size_t index = 0U; index < positions.size(); ++index) {
        positions[index] = index;
    }
    plan.controller_count = positions.size();
    plan.ordinals = positions.data();
    plan.context_check = [] { return NX_SUCCESS; };
    plan.admit = [](size_t) { return NX_SUCCESS; };
    plan.system_start = [] {
        return nx_platform_start_result_t{NX_SUCCESS, NX_SUCCESS, 0U};
    };
    plan.resources_acquire = [] { return NX_SUCCESS; };
    plan.resources_release = [] { return NX_SUCCESS; };
    plan.system_release = [](bool started) {
        return started ? NX_SUCCESS : NX_ERROR_STATE;
    };
    plan.acquire = [](size_t) {
        return nx_lifecycle_acquisition_t{NX_SUCCESS, true};
    };
    plan.release = [](size_t) { return NX_SUCCESS; };
    EXPECT_EQ(nx_platform_lifecycle_start(&plan, &state, nullptr).primary,
              NX_SUCCESS);
    EXPECT_EQ(state.initialized, UINT16_MAX);
    EXPECT_EQ(nx_platform_lifecycle_stop(&plan, &state, nullptr), NX_SUCCESS);
    EXPECT_EQ(state.initialized, 0U);
    EXPECT_EQ(state.bound, nullptr);
}

TEST_F(PlatformLifecycle, InvalidStateCannotHideThePermanentPlatformEffects) {
    plan.permanent_effects = true;
    const auto result =
        nx_platform_lifecycle_start(&plan, nullptr, &diagnostic);
    EXPECT_EQ(result.primary, NX_ERROR_INVALID);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.remaining_effects, 1U);
}

TEST_F(PlatformLifecycle, RefusedRestartReportsTheStillLivePlatformEffects) {
    InSequence order;
    start();
    EXPECT_CALL(ports, context()).WillOnce(Return(NX_SUCCESS));
    const auto duplicate = nx_platform_lifecycle_start(&plan, &state, nullptr);
    EXPECT_EQ(duplicate.primary, NX_ERROR_STATE);
    EXPECT_EQ(duplicate.remaining_effects, 1U);
    EXPECT_CALL(ports, context()).WillOnce(Return(NX_ERROR_CONTEXT));
    const auto refused = nx_platform_lifecycle_start(&plan, &state, nullptr);
    EXPECT_EQ(refused.primary, NX_ERROR_CONTEXT);
    EXPECT_EQ(refused.remaining_effects, 1U);
    EXPECT_EQ(state.initialized, 3U);
    EXPECT_TRUE(state.started);
    EXPECT_EQ(state.bound, &plan);
}

TEST_F(PlatformLifecycle,
       EmptyNativePlatformStillOwnsItsExplicitSystemBoundary) {
    plan.controller_count = 0U;
    plan.ordinals = nullptr;
    InSequence order;
    EXPECT_CALL(ports, context()).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, system_start())
        .WillOnce(
            Return(nx_platform_start_result_t{NX_SUCCESS, NX_SUCCESS, 0U}));
    EXPECT_CALL(ports, resources_acquire()).WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(nx_platform_lifecycle_start(&plan, &state, &diagnostic).primary,
              NX_SUCCESS);
    EXPECT_CALL(ports, context()).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, resources_release()).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, system_release(true)).WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(nx_platform_lifecycle_stop(&plan, &state, &diagnostic),
              NX_SUCCESS);
}
} /* namespace */
