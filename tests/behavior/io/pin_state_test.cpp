/**
 * \file            pin_state_test.cpp
 * \brief           Public pin-state polymorphic dispatch and refusal boundary
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-18
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/pin_state.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
using testing::_;
class PinStateBoundary {
  public:
    MOCK_METHOD(nx_result_t, Select, (nx_pin_state_selection_t));
};
static nx_result_t Dispatch(void* context, nx_pin_state_selection_t state) {
    return static_cast<PinStateBoundary*>(context)->Select(state);
}
class PinStatePublic : public testing::Test {
  protected:
    testing::StrictMock<PinStateBoundary> boundary;
    const nx_pin_state_ops_t ops{Dispatch};
    const nx_pin_state_port_t port{&ops, &boundary};
};
TEST_F(PinStatePublic, NullFaceIsInvalid) {
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_pin_state_select(nullptr, NX_PIN_STATE_DEFAULT));
}
TEST_F(PinStatePublic, NullMethodsAreInvalid) {
    const nx_pin_state_port_t broken{nullptr, &boundary};
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_pin_state_select(&broken, NX_PIN_STATE_DEFAULT));
}
TEST_F(PinStatePublic, MissingOperationIsUnsupported) {
    const nx_pin_state_ops_t absent{nullptr};
    const nx_pin_state_port_t broken{&absent, &boundary};
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_pin_state_select(&broken, NX_PIN_STATE_DEFAULT));
}
TEST_F(PinStatePublic, UnknownStateDoesNotInvokeProvider) {
    EXPECT_EQ(
        NX_ERROR_INVALID,
        nx_pin_state_select(&port, static_cast<nx_pin_state_selection_t>(2)));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_pin_state_select(&port, NX_PIN_STATE_INVALID));
}
TEST_F(PinStatePublic, DefaultDispatchesExactlyOnce) {
    EXPECT_CALL(boundary, Select(NX_PIN_STATE_DEFAULT))
        .WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_EQ(NX_SUCCESS, nx_pin_state_select(&port, NX_PIN_STATE_DEFAULT));
}
TEST_F(PinStatePublic, SleepDispatchesExactlyOnce) {
    EXPECT_CALL(boundary, Select(NX_PIN_STATE_SLEEP))
        .WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_EQ(NX_SUCCESS, nx_pin_state_select(&port, NX_PIN_STATE_SLEEP));
}
TEST_F(PinStatePublic, ProviderFailureIsPreserved) {
    EXPECT_CALL(boundary, Select(_)).WillOnce(testing::Return(NX_ERROR_IO));
    EXPECT_EQ(NX_ERROR_IO, nx_pin_state_select(&port, NX_PIN_STATE_SLEEP));
}

TEST_F(PinStatePublic, NullContextDoesNotInvokeProvider) {
    const nx_pin_state_port_t broken{&ops, nullptr};
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_pin_state_select(&broken, NX_PIN_STATE_DEFAULT));
}
