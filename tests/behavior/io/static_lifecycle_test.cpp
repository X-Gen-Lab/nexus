/**
 * \file            static_lifecycle_test.cpp
 * \brief           Cold transactions and retained cleanup responsibility.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/lifecycle.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
using testing::_;
using testing::InSequence;
using testing::Return;
using testing::StrictMock;

/** \brief Explicit external operations, with no implicit hardware admission. */
class Ports {
  public:
    MOCK_METHOD(nx_result_t, context, (nx_lifecycle_operation_t));
    MOCK_METHOD(nx_result_t, admit, (int));
    MOCK_METHOD(nx_lifecycle_acquisition_t, acquire, (int));
    MOCK_METHOD(nx_result_t, release, (int));
};

/** \brief Independent per-resource mutable storage passed by typed bindings. */
struct Slot {
    Ports* ports;
    unsigned receipt = 0U;
};

/** \brief Effect-free admission forwards the exact state/config binding. */
nx_result_t admit(void* state, const void* config) {
    return static_cast<Slot*>(state)->ports->admit(
        *static_cast<const int*>(config));
}

/** \brief Acquisition exposes the current step's actual failure receipt. */
nx_lifecycle_acquisition_t acquire(void* state, const void* config) {
    return static_cast<Slot*>(state)->ports->acquire(
        *static_cast<const int*>(config));
}

/** \brief Release reports whether this exact resource is fully relinquished. */
nx_result_t release(void* state, const void* config) {
    return static_cast<Slot*>(state)->ports->release(
        *static_cast<const int*>(config));
}

/** \brief Caller context admission is explicit for each external operation. */
nx_result_t context(const void* opaque, nx_lifecycle_operation_t operation) {
    return static_cast<Ports*>(const_cast<void*>(opaque))->context(operation);
}

/** \brief Three ordered resources in readonly plan storage. */
class StaticLifecycle : public testing::Test {
  protected:
    StrictMock<Ports> ports;
    Slot slots[3]{{&ports}, {&ports}, {&ports}};
    const int ids[3]{0, 1, 2};
    const nx_lifecycle_ops_t ops{admit, acquire, release};
    const nx_lifecycle_entry_t entries[3]{{&ops, &slots[0], &ids[0]},
                                          {&ops, &slots[1], &ids[1]},
                                          {&ops, &slots[2], &ids[2]}};
    const nx_lifecycle_plan_t plan{entries, 3U, context, &ports};
    nx_lifecycle_state_t state{};

    /** \brief Set exact complete startup expectations without hiding order. */
    void start_expectations() {
        EXPECT_CALL(ports, context(NX_LIFECYCLE_START))
            .WillOnce(Return(NX_SUCCESS));
        for (int id : ids) {
            EXPECT_CALL(ports, admit(id)).WillOnce(Return(NX_SUCCESS));
        }
        for (int id : ids) {
            EXPECT_CALL(ports, acquire(id))
                .WillOnce(Return(nx_lifecycle_acquisition_t{NX_SUCCESS, true}));
        }
    }
};

TEST_F(StaticLifecycle, EveryAdmissionFinishesBeforeFirstAcquisition) {
    InSequence order;
    start_expectations();
    ASSERT_EQ(nx_lifecycle_start(&plan, &state), NX_SUCCESS);
    EXPECT_EQ(state.phase, NX_LIFECYCLE_ACTIVE);
    EXPECT_EQ(state.cursor, 3U);
    EXPECT_EQ(state.bound, &plan);
    EXPECT_EQ(state.primary, NX_SUCCESS);
    EXPECT_EQ(state.cleanup, NX_SUCCESS);
}

TEST_F(StaticLifecycle, RefusedContextHasNoResourceEffects) {
    EXPECT_CALL(ports, context(NX_LIFECYCLE_START))
        .WillOnce(Return(NX_ERROR_CONTEXT));
    EXPECT_EQ(nx_lifecycle_start(&plan, &state), NX_ERROR_CONTEXT);
    EXPECT_EQ(state.phase, NX_LIFECYCLE_IDLE);
    EXPECT_EQ(state.cursor, 0U);
    EXPECT_EQ(state.bound, nullptr);
    EXPECT_EQ(state.primary, NX_ERROR_CONTEXT);
    EXPECT_EQ(state.cleanup, NX_SUCCESS);
}

TEST_F(StaticLifecycle, RefusedAdmissionHasNoAcquisitionOrRelease) {
    InSequence order;
    EXPECT_CALL(ports, context(NX_LIFECYCLE_START))
        .WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, admit(0)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, admit(1)).WillOnce(Return(NX_ERROR_BUSY));
    EXPECT_EQ(nx_lifecycle_start(&plan, &state), NX_ERROR_BUSY);
    EXPECT_EQ(state.phase, NX_LIFECYCLE_IDLE);
    EXPECT_EQ(state.bound, nullptr);
}

TEST_F(StaticLifecycle, ReleaseRunsInReverseAcquisitionOrder) {
    InSequence order;
    start_expectations();
    ASSERT_EQ(nx_lifecycle_start(&plan, &state), NX_SUCCESS);
    EXPECT_CALL(ports, context(NX_LIFECYCLE_STOP)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, release(2)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, release(1)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, release(0)).WillOnce(Return(NX_SUCCESS));
    ASSERT_EQ(nx_lifecycle_stop(&plan, &state), NX_SUCCESS);
    EXPECT_EQ(state.phase, NX_LIFECYCLE_IDLE);
    EXPECT_EQ(state.cursor, 0U);
    EXPECT_EQ(state.bound, nullptr);
}

TEST_F(StaticLifecycle,
       FailedCurrentStepIsReleasedOnlyIfItRetainsResponsibility) {
    for (bool retained : {false, true}) {
        SCOPED_TRACE(retained);
        InSequence order;
        EXPECT_CALL(ports, context(NX_LIFECYCLE_START))
            .WillOnce(Return(NX_SUCCESS));
        for (int id : ids) {
            EXPECT_CALL(ports, admit(id)).WillOnce(Return(NX_SUCCESS));
        }
        EXPECT_CALL(ports, acquire(0))
            .WillOnce(Return(nx_lifecycle_acquisition_t{NX_SUCCESS, false}));
        EXPECT_CALL(ports, acquire(1))
            .WillOnce(
                Return(nx_lifecycle_acquisition_t{NX_ERROR_IO, retained}));
        if (retained) {
            EXPECT_CALL(ports, release(1)).WillOnce(Return(NX_SUCCESS));
        }
        EXPECT_CALL(ports, release(0)).WillOnce(Return(NX_SUCCESS));
        EXPECT_EQ(nx_lifecycle_start(&plan, &state), NX_ERROR_IO);
        EXPECT_EQ(state.phase, NX_LIFECYCLE_IDLE);
        EXPECT_EQ(state.primary, NX_ERROR_IO);
        EXPECT_EQ(state.cleanup, NX_SUCCESS);
    }
}

TEST_F(StaticLifecycle, FailedRollbackKeepsOriginalFailureAndExactRetryCursor) {
    InSequence order;
    EXPECT_CALL(ports, context(NX_LIFECYCLE_START))
        .WillOnce(Return(NX_SUCCESS));
    for (int id : ids) {
        EXPECT_CALL(ports, admit(id)).WillOnce(Return(NX_SUCCESS));
    }
    EXPECT_CALL(ports, acquire(0))
        .WillOnce(Return(nx_lifecycle_acquisition_t{NX_SUCCESS, true}));
    EXPECT_CALL(ports, acquire(1))
        .WillOnce(Return(nx_lifecycle_acquisition_t{NX_ERROR_IO, true}));
    EXPECT_CALL(ports, release(1)).WillOnce(Return(NX_ERROR_TIMEOUT));
    EXPECT_EQ(nx_lifecycle_start(&plan, &state), NX_ERROR_IO);
    EXPECT_EQ(state.primary, NX_ERROR_IO);
    EXPECT_EQ(state.cleanup, NX_ERROR_TIMEOUT);
    EXPECT_EQ(state.phase, NX_LIFECYCLE_DRAINING);
    EXPECT_EQ(state.cursor, 2U);
    EXPECT_EQ(state.bound, &plan);
    EXPECT_EQ(nx_lifecycle_start(&plan, &state), NX_ERROR_STATE);
    EXPECT_CALL(ports, context(NX_LIFECYCLE_STOP)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, release(1)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, release(0)).WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(nx_lifecycle_stop(&plan, &state), NX_SUCCESS);
    EXPECT_EQ(state.primary, NX_ERROR_IO);
    EXPECT_EQ(state.cleanup, NX_SUCCESS);
    EXPECT_EQ(state.phase, NX_LIFECYCLE_IDLE);
}

TEST_F(StaticLifecycle, StopRetryNeverRepeatsAnAlreadyCompletedRelease) {
    InSequence order;
    start_expectations();
    ASSERT_EQ(nx_lifecycle_start(&plan, &state), NX_SUCCESS);
    EXPECT_CALL(ports, context(NX_LIFECYCLE_STOP)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, release(2)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, release(1)).WillOnce(Return(NX_ERROR_BUSY));
    EXPECT_EQ(nx_lifecycle_stop(&plan, &state), NX_ERROR_BUSY);
    EXPECT_EQ(state.cursor, 2U);
    EXPECT_EQ(state.primary, NX_SUCCESS);
    EXPECT_EQ(state.cleanup, NX_ERROR_BUSY);
    EXPECT_CALL(ports, context(NX_LIFECYCLE_STOP)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, release(1)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, release(0)).WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(nx_lifecycle_stop(&plan, &state), NX_SUCCESS);
    EXPECT_EQ(nx_lifecycle_stop(&plan, &state), NX_SUCCESS);
}

TEST_F(StaticLifecycle, ConcreteReceiptSurvivesUntilItsSuccessfulRelease) {
    InSequence order;
    EXPECT_CALL(ports, context(NX_LIFECYCLE_START))
        .WillOnce(Return(NX_SUCCESS));
    for (int id : ids) {
        EXPECT_CALL(ports, admit(id)).WillOnce(Return(NX_SUCCESS));
    }
    EXPECT_CALL(ports, acquire(0)).WillOnce([&](int) {
        slots[0].receipt = 42U;
        return nx_lifecycle_acquisition_t{NX_ERROR_IO, true};
    });
    EXPECT_CALL(ports, release(0)).WillOnce([&](int) {
        EXPECT_EQ(slots[0].receipt, 42U);
        return NX_ERROR_TIMEOUT;
    });
    ASSERT_EQ(nx_lifecycle_start(&plan, &state), NX_ERROR_IO);
    EXPECT_EQ(state.cursor, 1U);
    EXPECT_EQ(slots[0].receipt, 42U);
    EXPECT_CALL(ports, context(NX_LIFECYCLE_STOP)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, release(0)).WillOnce([&](int) {
        EXPECT_EQ(slots[0].receipt, 42U);
        slots[0].receipt = 0U;
        return NX_SUCCESS;
    });
    EXPECT_EQ(nx_lifecycle_stop(&plan, &state), NX_SUCCESS);
    EXPECT_EQ(slots[0].receipt, 0U);
    EXPECT_EQ(state.primary, NX_ERROR_IO);
}

TEST_F(StaticLifecycle, RefusedStopContextPreservesAllPendingResponsibility) {
    InSequence order;
    start_expectations();
    ASSERT_EQ(nx_lifecycle_start(&plan, &state), NX_SUCCESS);
    EXPECT_CALL(ports, context(NX_LIFECYCLE_STOP))
        .WillOnce(Return(NX_ERROR_CONTEXT));
    EXPECT_EQ(nx_lifecycle_stop(&plan, &state), NX_ERROR_CONTEXT);
    EXPECT_EQ(state.cursor, 3U);
    EXPECT_EQ(state.phase, NX_LIFECYCLE_ACTIVE);
    EXPECT_EQ(state.bound, &plan);
    EXPECT_EQ(state.cleanup, NX_ERROR_CONTEXT);
}

TEST_F(StaticLifecycle, DuplicateStartAndForeignStopCannotRebindLiveResources) {
    InSequence order;
    start_expectations();
    ASSERT_EQ(nx_lifecycle_start(&plan, &state), NX_SUCCESS);
    EXPECT_EQ(nx_lifecycle_start(&plan, &state), NX_ERROR_STATE);
    const nx_lifecycle_plan_t other{entries, 3U, context, &ports};
    EXPECT_EQ(nx_lifecycle_stop(&other, &state), NX_ERROR_STATE);
    EXPECT_EQ(state.bound, &plan);
    EXPECT_EQ(state.cursor, 3U);
}

TEST_F(StaticLifecycle, IndependentInstancesDoNotShareEngineState) {
    StrictMock<Ports> other_ports;
    Slot other_slot{&other_ports};
    const int id = 9;
    const nx_lifecycle_entry_t other_entry{&ops, &other_slot, &id};
    const nx_lifecycle_plan_t other{&other_entry, 1U, context, &other_ports};
    nx_lifecycle_state_t other_state{};
    EXPECT_CALL(other_ports, context(NX_LIFECYCLE_START))
        .WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(other_ports, admit(9)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(other_ports, acquire(9))
        .WillOnce(Return(nx_lifecycle_acquisition_t{NX_SUCCESS, true}));
    ASSERT_EQ(nx_lifecycle_start(&other, &other_state), NX_SUCCESS);
    EXPECT_EQ(state.phase, NX_LIFECYCLE_IDLE);
    EXPECT_EQ(state.bound, nullptr);
    EXPECT_EQ(other_state.bound, &other);
    EXPECT_CALL(other_ports, context(NX_LIFECYCLE_STOP))
        .WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(other_ports, release(9)).WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(nx_lifecycle_stop(&other, &other_state), NX_SUCCESS);
}

TEST_F(StaticLifecycle, ReentryDuringCallbacksIsBusyAndCannotRevokeOwnership) {
    InSequence order;
    EXPECT_CALL(ports, context(NX_LIFECYCLE_START))
        .WillOnce([&](nx_lifecycle_operation_t) {
            EXPECT_EQ(nx_lifecycle_start(&plan, &state), NX_ERROR_BUSY);
            EXPECT_EQ(nx_lifecycle_stop(&plan, &state), NX_ERROR_BUSY);
            return NX_SUCCESS;
        });
    for (int id : ids) {
        EXPECT_CALL(ports, admit(id)).WillOnce(Return(NX_SUCCESS));
    }
    for (int id : ids) {
        EXPECT_CALL(ports, acquire(id))
            .WillOnce(Return(nx_lifecycle_acquisition_t{NX_SUCCESS, true}));
    }
    EXPECT_EQ(nx_lifecycle_start(&plan, &state), NX_SUCCESS);
    EXPECT_CALL(ports, context(NX_LIFECYCLE_STOP)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, release(2)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, release(1)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, release(0)).WillOnce([&](int) {
        EXPECT_EQ(nx_lifecycle_stop(&plan, &state), NX_ERROR_BUSY);
        return NX_SUCCESS;
    });
    EXPECT_EQ(nx_lifecycle_stop(&plan, &state), NX_SUCCESS);
}

TEST_F(StaticLifecycle, InvalidTableRejectsBeforeAnyCallback) {
    EXPECT_EQ(nx_lifecycle_start(nullptr, &state), NX_ERROR_INVALID);
    EXPECT_EQ(nx_lifecycle_start(&plan, nullptr), NX_ERROR_INVALID);
    EXPECT_EQ(nx_lifecycle_stop(nullptr, &state), NX_ERROR_INVALID);
    EXPECT_EQ(nx_lifecycle_stop(&plan, nullptr), NX_ERROR_INVALID);
    nx_lifecycle_plan_t invalid = plan;
    invalid.context_check = nullptr;
    EXPECT_EQ(nx_lifecycle_start(&invalid, &state), NX_ERROR_INVALID);
    invalid = plan;
    invalid.entries = nullptr;
    EXPECT_EQ(nx_lifecycle_start(&invalid, &state), NX_ERROR_INVALID);
    invalid = plan;
    invalid.count = SIZE_MAX;
    EXPECT_EQ(nx_lifecycle_start(&invalid, &state), NX_ERROR_INVALID);
    nx_lifecycle_ops_t bad_ops = ops;
    nx_lifecycle_entry_t bad_entries[2]{entries[0],
                                        {&bad_ops, &slots[1], &ids[1]}};
    invalid = {bad_entries, 2U, context, &ports};
    bad_ops.admit = nullptr;
    EXPECT_EQ(nx_lifecycle_start(&invalid, &state), NX_ERROR_INVALID);
    bad_ops = ops;
    bad_ops.acquire = nullptr;
    EXPECT_EQ(nx_lifecycle_start(&invalid, &state), NX_ERROR_INVALID);
    bad_ops = ops;
    bad_ops.release = nullptr;
    EXPECT_EQ(nx_lifecycle_start(&invalid, &state), NX_ERROR_INVALID);
    bad_entries[1].ops = nullptr;
    EXPECT_EQ(nx_lifecycle_start(&invalid, &state), NX_ERROR_INVALID);
    EXPECT_EQ(state.phase, NX_LIFECYCLE_IDLE);
    EXPECT_EQ(state.bound, nullptr);
}

TEST_F(StaticLifecycle, EmptyPlanHasExplicitContextAndIdempotentIdleStop) {
    const nx_lifecycle_plan_t empty{nullptr, 0U, context, &ports};
    EXPECT_EQ(nx_lifecycle_stop(&empty, &state), NX_SUCCESS);
    EXPECT_CALL(ports, context(NX_LIFECYCLE_START))
        .WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(nx_lifecycle_start(&empty, &state), NX_SUCCESS);
    EXPECT_EQ(state.phase, NX_LIFECYCLE_ACTIVE);
    EXPECT_EQ(state.cursor, 0U);
    EXPECT_CALL(ports, context(NX_LIFECYCLE_STOP)).WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(nx_lifecycle_stop(&empty, &state), NX_SUCCESS);
    EXPECT_EQ(nx_lifecycle_stop(&empty, &state), NX_SUCCESS);
}

TEST_F(StaticLifecycle,
       FullyReleasedEngineCanStartAgainWithClearedDiagnostics) {
    EXPECT_CALL(ports, context(NX_LIFECYCLE_START))
        .WillOnce(Return(NX_ERROR_CONTEXT));
    ASSERT_EQ(nx_lifecycle_start(&plan, &state), NX_ERROR_CONTEXT);
    InSequence order;
    start_expectations();
    EXPECT_EQ(nx_lifecycle_start(&plan, &state), NX_SUCCESS);
    EXPECT_EQ(state.primary, NX_SUCCESS);
    EXPECT_EQ(state.cleanup, NX_SUCCESS);
}
} /* namespace */
