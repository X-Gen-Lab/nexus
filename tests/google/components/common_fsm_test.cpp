/**
 * \file            common_fsm_test.cpp
 * \brief           Real finite-state transition and callback contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/fsm.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;

class CallbackPort {
  public:
    MOCK_METHOD(void, Action, (int, const nx_fsm_event_t*), ());
    MOCK_METHOD(bool, Guard, (const nx_fsm_event_t*), ());
};

void enter_zero(void* context, const nx_fsm_event_t* event) {
    static_cast<CallbackPort*>(context)->Action(0, event);
}
void exit_zero(void* context, const nx_fsm_event_t* event) {
    static_cast<CallbackPort*>(context)->Action(1, event);
}
void enter_one(void* context, const nx_fsm_event_t* event) {
    static_cast<CallbackPort*>(context)->Action(2, event);
}
void transition_action(void* context, const nx_fsm_event_t* event) {
    static_cast<CallbackPort*>(context)->Action(3, event);
}
bool transition_guard(void* context, const nx_fsm_event_t* event) {
    return static_cast<CallbackPort*>(context)->Guard(event);
}

class CommonFsm : public ::testing::Test {
  protected:
    StrictMock<CallbackPort> port;
    nx_fsm_t machine = {};
    nx_fsm_state_t states[2] = {{enter_zero, exit_zero}, {enter_one, nullptr}};
    nx_fsm_transition_t transitions[2] = {
        {0, 1, 7, NX_FSM_EXTERNAL, transition_guard, transition_action},
        {0, 0, 7, NX_FSM_INTERNAL, nullptr, transition_action}};
    nx_fsm_config_t config = {states, 2, transitions, 1};
    nx_fsm_event_t event = {7, nullptr, 0};

    void SetUp() override {
        EXPECT_CALL(port, Action(0, nullptr));
        ASSERT_EQ(nx_fsm_initialize(&machine, &config, &port, 0), NX_SUCCESS);
        ASSERT_EQ(machine.current, 0U);
    }
};

TEST_F(CommonFsm, ExternalTransitionOrdersExitActionAndTargetEntry) {
    InSequence sequence;
    EXPECT_CALL(port, Guard(&event)).WillOnce(Return(true));
    EXPECT_CALL(port, Action(1, &event)).WillOnce([&](int, const auto*) {
        EXPECT_EQ(machine.current, 0U);
    });
    EXPECT_CALL(port, Action(3, &event)).WillOnce([&](int, const auto*) {
        EXPECT_EQ(machine.current, 0U);
    });
    EXPECT_CALL(port, Action(2, &event)).WillOnce([&](int, const auto*) {
        EXPECT_EQ(machine.current, 1U);
    });
    EXPECT_EQ(nx_fsm_dispatch(&machine, &event), NX_SUCCESS);
    EXPECT_EQ(machine.current, 1U);
    EXPECT_FALSE(machine.busy);
}

TEST_F(CommonFsm, DisabledGuardLeavesStateAndRunsNoActions) {
    EXPECT_CALL(port, Guard(&event)).WillOnce(Return(false));
    EXPECT_EQ(nx_fsm_dispatch(&machine, &event), NX_ERROR_EMPTY);
    EXPECT_EQ(machine.current, 0U);
    EXPECT_FALSE(machine.busy);
}

TEST_F(CommonFsm, FirstEnabledRowWinsInTableOrder) {
    nx_fsm_config_t expanded = config;
    expanded.transition_count = 2;
    EXPECT_CALL(port, Action(0, nullptr));
    ASSERT_EQ(nx_fsm_initialize(&machine, &expanded, &port, 0), NX_SUCCESS);
    InSequence sequence;
    EXPECT_CALL(port, Guard(&event)).WillOnce(Return(false));
    EXPECT_CALL(port, Action(3, &event));
    EXPECT_EQ(nx_fsm_dispatch(&machine, &event), NX_SUCCESS);
    EXPECT_EQ(machine.current, 0U);
}

TEST_F(CommonFsm, AcceptedFirstRowDoesNotRunLaterMatchingRow) {
    nx_fsm_config_t expanded = config;
    expanded.transition_count = 2;
    EXPECT_CALL(port, Action(0, nullptr));
    ASSERT_EQ(nx_fsm_initialize(&machine, &expanded, &port, 0), NX_SUCCESS);
    EXPECT_CALL(port, Guard(&event)).WillOnce(Return(true));
    EXPECT_CALL(port, Action(1, &event));
    EXPECT_CALL(port, Action(3, &event)).Times(1);
    EXPECT_CALL(port, Action(2, &event));
    EXPECT_EQ(nx_fsm_dispatch(&machine, &event), NX_SUCCESS);
    EXPECT_EQ(machine.current, 1U);
}

TEST_F(CommonFsm, InternalTransitionRunsActionWithoutExitOrEntry) {
    nx_fsm_config_t internal = config;
    internal.transitions = &transitions[1];
    EXPECT_CALL(port, Action(0, nullptr));
    ASSERT_EQ(nx_fsm_initialize(&machine, &internal, &port, 0), NX_SUCCESS);
    EXPECT_CALL(port, Action(3, &event));
    EXPECT_EQ(nx_fsm_dispatch(&machine, &event), NX_SUCCESS);
    EXPECT_EQ(machine.current, 0U);
}

TEST_F(CommonFsm, ExternalSelfTransitionRunsExitActionAndEntry) {
    const nx_fsm_transition_t row = {
        0, 0, 7, NX_FSM_EXTERNAL, nullptr, transition_action};
    nx_fsm_config_t self = config;
    self.transitions = &row;
    EXPECT_CALL(port, Action(0, nullptr));
    ASSERT_EQ(nx_fsm_initialize(&machine, &self, &port, 0), NX_SUCCESS);
    InSequence sequence;
    EXPECT_CALL(port, Action(1, &event));
    EXPECT_CALL(port, Action(3, &event));
    EXPECT_CALL(port, Action(0, &event));
    EXPECT_EQ(nx_fsm_dispatch(&machine, &event), NX_SUCCESS);
    EXPECT_EQ(machine.current, 0U);
}

TEST_F(CommonFsm, NonmatchingEventDoesNotEvaluateGuard) {
    event.id = 8;
    EXPECT_EQ(nx_fsm_dispatch(&machine, &event), NX_ERROR_EMPTY);
    EXPECT_EQ(machine.current, 0U);
}

TEST_F(CommonFsm, DispatchReentryIsBusyAndOuterTransitionCompletes) {
    EXPECT_CALL(port, Guard(&event)).WillOnce([&](const auto*) {
        EXPECT_EQ(nx_fsm_dispatch(&machine, &event), NX_ERROR_BUSY);
        return true;
    });
    EXPECT_CALL(port, Action(1, &event));
    EXPECT_CALL(port, Action(3, &event)).WillOnce([&](int, const auto*) {
        EXPECT_EQ(nx_fsm_dispatch(&machine, &event), NX_ERROR_BUSY);
        EXPECT_EQ(nx_fsm_initialize(&machine, &config, &port, 0),
                  NX_ERROR_BUSY);
    });
    EXPECT_CALL(port, Action(2, &event));
    EXPECT_EQ(nx_fsm_dispatch(&machine, &event), NX_SUCCESS);
    EXPECT_FALSE(machine.busy);
}

TEST_F(CommonFsm, EventPayloadIsBorrowedWithoutCopyOrRetention) {
    const uint8_t payload[] = {4, 5, 6};
    event.payload = payload;
    event.size = sizeof(payload);
    EXPECT_CALL(port, Guard(&event)).WillOnce([&](const auto* seen) {
        EXPECT_EQ(seen->payload, payload);
        EXPECT_EQ(seen->size, sizeof(payload));
        return false;
    });
    EXPECT_EQ(nx_fsm_dispatch(&machine, &event), NX_ERROR_EMPTY);
}

TEST_F(CommonFsm, InvalidEventRejectsBeforeCallbacks) {
    EXPECT_EQ(nx_fsm_dispatch(nullptr, &event), NX_ERROR_INVALID);
    EXPECT_EQ(nx_fsm_dispatch(&machine, nullptr), NX_ERROR_INVALID);
    event.size = 1;
    EXPECT_EQ(nx_fsm_dispatch(&machine, &event), NX_ERROR_INVALID);
    EXPECT_EQ(machine.current, 0U);
    EXPECT_FALSE(machine.busy);
}

TEST_F(CommonFsm, InvalidTablesRejectBeforeReplacingExistingBinding) {
    nx_fsm_config_t bad = config;
    bad.states = nullptr;
    EXPECT_EQ(nx_fsm_initialize(&machine, &bad, &port, 0), NX_ERROR_INVALID);
    bad = config;
    bad.state_count = 0;
    EXPECT_EQ(nx_fsm_initialize(&machine, &bad, &port, 0), NX_ERROR_INVALID);
    bad = config;
    bad.transitions = nullptr;
    EXPECT_EQ(nx_fsm_initialize(&machine, &bad, &port, 0), NX_ERROR_INVALID);
    EXPECT_EQ(nx_fsm_initialize(&machine, &config, &port, 2), NX_ERROR_INVALID);
    EXPECT_EQ(machine.config, &config);
    EXPECT_EQ(machine.context, &port);
    EXPECT_EQ(machine.current, 0U);
}

TEST_F(CommonFsm, InvalidTransitionIndicesAndInternalTargetsRejectAtomically) {
    nx_fsm_transition_t row = transitions[0];
    nx_fsm_config_t bad = config;
    bad.transitions = &row;
    row.source = 2;
    EXPECT_EQ(nx_fsm_initialize(&machine, &bad, &port, 0), NX_ERROR_INVALID);
    row.source = 0;
    row.target = 2;
    EXPECT_EQ(nx_fsm_initialize(&machine, &bad, &port, 0), NX_ERROR_INVALID);
    row.target = 1;
    row.kind = NX_FSM_INTERNAL;
    EXPECT_EQ(nx_fsm_initialize(&machine, &bad, &port, 0), NX_ERROR_INVALID);
    EXPECT_EQ(machine.current, 0U);
}

TEST_F(CommonFsm, IdleResetEntersSelectedStateWithoutExitingOldState) {
    EXPECT_CALL(port, Action(2, nullptr));
    EXPECT_EQ(nx_fsm_initialize(&machine, &config, &port, 1), NX_SUCCESS);
    EXPECT_EQ(machine.current, 1U);
    EXPECT_EQ(nx_fsm_dispatch(&machine, &event), NX_ERROR_EMPTY);
}

TEST(CommonFsmConstruction, InitialEntryCannotDispatchRecursively) {
    StrictMock<CallbackPort> port;
    nx_fsm_t machine = {};
    nx_fsm_state_t state = {enter_zero, nullptr};
    nx_fsm_config_t config = {&state, 1, nullptr, 0};
    nx_fsm_event_t event = {1, nullptr, 0};
    EXPECT_CALL(port, Action(0, nullptr)).WillOnce([&](int, const auto*) {
        EXPECT_EQ(machine.current, 0U);
        EXPECT_EQ(nx_fsm_dispatch(&machine, &event), NX_ERROR_BUSY);
    });
    EXPECT_EQ(nx_fsm_initialize(&machine, &config, &port, 0), NX_SUCCESS);
    EXPECT_EQ(nx_fsm_dispatch(&machine, &event), NX_ERROR_EMPTY);
}

TEST(CommonFsmConstruction, NullAndUninitializedInputsReject) {
    nx_fsm_t machine = {};
    nx_fsm_event_t event = {1, nullptr, 0};
    nx_fsm_state_t state = {};
    nx_fsm_config_t config = {&state, 1, nullptr, 0};
    EXPECT_EQ(nx_fsm_initialize(nullptr, &config, nullptr, 0),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_fsm_initialize(&machine, nullptr, nullptr, 0),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_fsm_dispatch(&machine, &event), NX_ERROR_STATE);
    EXPECT_EQ(machine.config, nullptr);
}

TEST(CommonFsmConstruction, InstancesShareImmutableTablesButKeepSeparateState) {
    const nx_fsm_state_t states[2] = {};
    const nx_fsm_transition_t row = {0,       1,      4, NX_FSM_EXTERNAL,
                                     nullptr, nullptr};
    const nx_fsm_config_t config = {states, 2, &row, 1};
    const nx_fsm_event_t event = {4, nullptr, 0};
    nx_fsm_t first = {}, second = {};
    ASSERT_EQ(nx_fsm_initialize(&first, &config, nullptr, 0), NX_SUCCESS);
    ASSERT_EQ(nx_fsm_initialize(&second, &config, nullptr, 0), NX_SUCCESS);
    EXPECT_EQ(nx_fsm_dispatch(&first, &event), NX_SUCCESS);
    EXPECT_EQ(first.current, 1U);
    EXPECT_EQ(second.current, 0U);
    EXPECT_EQ(nx_fsm_dispatch(&second, &event), NX_SUCCESS);
}

TEST(CommonFsmConstruction, CorruptCurrentStateRejectsBeforeTableAccess) {
    nx_fsm_t machine = {};
    const nx_fsm_state_t state = {};
    const nx_fsm_config_t config = {&state, 1, nullptr, 0};
    const nx_fsm_event_t event = {};
    ASSERT_EQ(nx_fsm_initialize(&machine, &config, nullptr, 0), NX_SUCCESS);
    machine.current = SIZE_MAX;
    EXPECT_EQ(nx_fsm_dispatch(&machine, &event), NX_ERROR_STATE);
    EXPECT_EQ(machine.current, SIZE_MAX);
    EXPECT_FALSE(machine.busy);
}
} /* namespace */
