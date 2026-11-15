/**
 * \file            wait_select_test.cpp
 * \brief           Multi-source predicate selection and latched wait contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/os/wait.h"
#include <array>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <limits>

namespace {
using testing::InSequence;
using testing::Return;
using testing::StrictMock;

/** \brief Only the external port operations are mocked. */
class Port {
  public:
    MOCK_METHOD(uint32_t, arm, ());
    MOCK_METHOD(nx_result_t, wait, (uint32_t, uint64_t));

    nx_wait_port_t view() {
        return {this, arm_callback, wait_callback, nullptr};
    }

    static uint32_t arm_callback(void* context) {
        return static_cast<Port*>(context)->arm();
    }

    static nx_result_t wait_callback(void* context, uint32_t sequence,
                                     uint64_t deadline) {
        return static_cast<Port*>(context)->wait(sequence, deadline);
    }
};

/** \brief A caller-owned authoritative predicate, independent of hints. */
class Source {
  public:
    MOCK_METHOD(bool, ready, ());

    nx_wait_source_t view() {
        return {ready_callback, this};
    }

    static bool ready_callback(void* context) {
        return static_cast<Source*>(context)->ready();
    }
};

TEST(WaitSelect, CursorReadyReturnsWithoutArmingOrScanningOtherSources) {
    StrictMock<Port> model;
    auto port = model.view();
    StrictMock<Source> first, second, third;
    std::array<nx_wait_source_t, 3> sources{first.view(), second.view(),
                                            third.view()};
    EXPECT_CALL(second, ready()).WillOnce(Return(true));
    size_t selected = SIZE_MAX;
    EXPECT_EQ(
        nx_wait_select(&port, sources.data(), sources.size(), 1, 42, &selected),
        NX_SUCCESS);
    EXPECT_EQ(selected, 1U);
}

TEST(WaitSelect, ScanWrapsFromExplicitCursorInBoundedOrder) {
    StrictMock<Port> model;
    auto port = model.view();
    StrictMock<Source> first, second, third;
    std::array<nx_wait_source_t, 3> sources{first.view(), second.view(),
                                            third.view()};
    InSequence order;
    EXPECT_CALL(third, ready()).WillOnce(Return(false));
    EXPECT_CALL(first, ready()).WillOnce(Return(false));
    EXPECT_CALL(second, ready()).WillOnce(Return(true));
    size_t selected = SIZE_MAX;
    EXPECT_EQ(
        nx_wait_select(&port, sources.data(), sources.size(), 2, 42, &selected),
        NX_SUCCESS);
    EXPECT_EQ(selected, 1U);
}

TEST(WaitSelect, ArmRecheckObservesAnotherSourceBeforeSleeping) {
    StrictMock<Port> model;
    auto port = model.view();
    StrictMock<Source> first, second;
    std::array<nx_wait_source_t, 2> sources{first.view(), second.view()};
    InSequence order;
    EXPECT_CALL(first, ready()).WillOnce(Return(false));
    EXPECT_CALL(second, ready()).WillOnce(Return(false));
    EXPECT_CALL(model, arm()).WillOnce(Return(7));
    EXPECT_CALL(first, ready()).WillOnce(Return(false));
    EXPECT_CALL(second, ready()).WillOnce(Return(true));
    size_t selected = SIZE_MAX;
    EXPECT_EQ(
        nx_wait_select(&port, sources.data(), sources.size(), 0, 42, &selected),
        NX_SUCCESS);
    EXPECT_EQ(selected, 1U);
}

TEST(WaitSelect, NotificationOnlyCausesAnotherAuthoritativeScan) {
    StrictMock<Port> model;
    auto port = model.view();
    StrictMock<Source> first, second;
    std::array<nx_wait_source_t, 2> sources{first.view(), second.view()};
    InSequence order;
    EXPECT_CALL(first, ready()).WillOnce(Return(false));
    EXPECT_CALL(second, ready()).WillOnce(Return(false));
    EXPECT_CALL(model, arm()).WillOnce(Return(7));
    EXPECT_CALL(first, ready()).WillOnce(Return(false));
    EXPECT_CALL(second, ready()).WillOnce(Return(false));
    EXPECT_CALL(model, wait(7, 42)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(first, ready()).WillOnce(Return(false));
    EXPECT_CALL(second, ready()).WillOnce(Return(true));
    size_t selected = SIZE_MAX;
    EXPECT_EQ(
        nx_wait_select(&port, sources.data(), sources.size(), 0, 42, &selected),
        NX_SUCCESS);
    EXPECT_EQ(selected, 1U);
}

class WaitSelectFinalObservation : public testing::TestWithParam<nx_result_t> {
};

TEST_P(WaitSelectFinalObservation, FinalReadySourceWinsBackendError) {
    StrictMock<Port> model;
    auto port = model.view();
    StrictMock<Source> first, second;
    std::array<nx_wait_source_t, 2> sources{first.view(), second.view()};
    InSequence order;
    EXPECT_CALL(first, ready()).WillOnce(Return(false));
    EXPECT_CALL(second, ready()).WillOnce(Return(false));
    EXPECT_CALL(model, arm()).WillOnce(Return(8));
    EXPECT_CALL(first, ready()).WillOnce(Return(false));
    EXPECT_CALL(second, ready()).WillOnce(Return(false));
    EXPECT_CALL(model, wait(8, 91)).WillOnce(Return(GetParam()));
    EXPECT_CALL(first, ready()).WillOnce(Return(false));
    EXPECT_CALL(second, ready()).WillOnce(Return(true));
    size_t selected = SIZE_MAX;
    EXPECT_EQ(
        nx_wait_select(&port, sources.data(), sources.size(), 0, 91, &selected),
        NX_SUCCESS);
    EXPECT_EQ(selected, 1U);
}

TEST_P(WaitSelectFinalObservation, NoReadySourcePreservesErrorAndOutput) {
    StrictMock<Port> model;
    auto port = model.view();
    StrictMock<Source> first, second;
    std::array<nx_wait_source_t, 2> sources{first.view(), second.view()};
    InSequence order;
    EXPECT_CALL(second, ready()).WillOnce(Return(false));
    EXPECT_CALL(first, ready()).WillOnce(Return(false));
    EXPECT_CALL(model, arm()).WillOnce(Return(8));
    EXPECT_CALL(second, ready()).WillOnce(Return(false));
    EXPECT_CALL(first, ready()).WillOnce(Return(false));
    EXPECT_CALL(model, wait(8, 91)).WillOnce(Return(GetParam()));
    EXPECT_CALL(second, ready()).WillOnce(Return(false));
    EXPECT_CALL(first, ready()).WillOnce(Return(false));
    size_t selected = 77;
    EXPECT_EQ(
        nx_wait_select(&port, sources.data(), sources.size(), 1, 91, &selected),
        GetParam());
    EXPECT_EQ(selected, 77U);
}

INSTANTIATE_TEST_SUITE_P(BackendErrors, WaitSelectFinalObservation,
                         testing::Values(NX_ERROR_TIMEOUT, NX_ERROR_BUSY,
                                         NX_ERROR_CONTEXT, NX_ERROR_IO));

TEST(WaitSelect, NeverDeadlinePassesUnchangedToSamePort) {
    StrictMock<Port> model;
    auto port = model.view();
    StrictMock<Source> source;
    const auto input = source.view();
    InSequence order;
    EXPECT_CALL(source, ready()).WillOnce(Return(false));
    EXPECT_CALL(model, arm()).WillOnce(Return(1));
    EXPECT_CALL(source, ready()).WillOnce(Return(false));
    EXPECT_CALL(model, wait(1, NX_DEADLINE_NEVER))
        .WillOnce(Return(NX_ERROR_BUSY));
    EXPECT_CALL(source, ready()).WillOnce(Return(false));
    size_t selected = 22;
    EXPECT_EQ(nx_wait_select(&port, &input, 1, 0, NX_DEADLINE_NEVER, &selected),
              NX_ERROR_BUSY);
    EXPECT_EQ(selected, 22U);
}

TEST(WaitSelect, CallerAdvancesCursorWithoutMutatingSourceArray) {
    StrictMock<Port> model;
    auto port = model.view();
    StrictMock<Source> first, second, third;
    const std::array<nx_wait_source_t, 3> sources{first.view(), second.view(),
                                                  third.view()};
    InSequence order;
    EXPECT_CALL(first, ready()).WillOnce(Return(true));
    EXPECT_CALL(second, ready()).WillOnce(Return(true));
    EXPECT_CALL(third, ready()).WillOnce(Return(true));
    EXPECT_CALL(first, ready()).WillOnce(Return(true));
    size_t cursor = 0;
    for (size_t expected : {0U, 1U, 2U, 0U}) {
        size_t selected = SIZE_MAX;
        ASSERT_EQ(nx_wait_select(&port, sources.data(), sources.size(), cursor,
                                 42, &selected),
                  NX_SUCCESS);
        EXPECT_EQ(selected, expected);
        cursor = (selected + 1) % sources.size();
    }
    EXPECT_EQ(sources[0].context, &first);
    EXPECT_EQ(sources[1].context, &second);
    EXPECT_EQ(sources[2].context, &third);
}

TEST(WaitSelect, ValidatesEverySourceBeforeAnyPredicateOrPortCall) {
    StrictMock<Port> model;
    auto port = model.view();
    StrictMock<Source> first;
    std::array<nx_wait_source_t, 2> sources{first.view(), {nullptr, nullptr}};
    size_t selected = 19;
    EXPECT_EQ(
        nx_wait_select(&port, sources.data(), sources.size(), 0, 42, &selected),
        NX_ERROR_INVALID);
    EXPECT_EQ(selected, 19U);
}

TEST(WaitSelect, InvalidInputsPreserveOutputWithoutCallingSources) {
    StrictMock<Port> model;
    auto port = model.view();
    StrictMock<Source> source;
    auto input = source.view();
    size_t selected = 19;
    EXPECT_EQ(nx_wait_select(nullptr, &input, 1, 0, 42, &selected),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_wait_select(&port, nullptr, 1, 0, 42, &selected),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_wait_select(&port, &input, 0, 0, 42, &selected),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_wait_select(&port, &input, 1, 1, 42, &selected),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_wait_select(&port, &input, 1, SIZE_MAX, 42, &selected),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_wait_select(&port, &input, 1, 0, 42, nullptr),
              NX_ERROR_INVALID);
    port.arm = nullptr;
    EXPECT_EQ(nx_wait_select(&port, &input, 1, 0, 42, &selected),
              NX_ERROR_INVALID);
    port = model.view();
    port.wait = nullptr;
    EXPECT_EQ(nx_wait_select(&port, &input, 1, 0, 42, &selected),
              NX_ERROR_INVALID);
    EXPECT_EQ(selected, 19U);
}

TEST(WaitSelect, OverflowRejectsBeforeReadingArrayOrCallingPort) {
    StrictMock<Port> model;
    auto port = model.view();
    StrictMock<Source> source;
    auto input = source.view();
    size_t selected = 19;
    EXPECT_EQ(nx_wait_select(&port, &input,
                             SIZE_MAX / sizeof(nx_wait_source_t) + 1, 0, 42,
                             &selected),
              NX_ERROR_OVERFLOW);
    const auto* overflow = reinterpret_cast<const nx_wait_source_t*>(
        UINTPTR_MAX - sizeof(nx_wait_source_t) + 2);
    EXPECT_EQ(nx_wait_select(&port, overflow, 1, 0, 42, &selected),
              NX_ERROR_OVERFLOW);
    EXPECT_EQ(selected, 19U);
}

TEST(WaitSelect, OutputAndPortArrayAliasesAreRejectedBeforeAnyCalls) {
    StrictMock<Port> model;
    auto port = model.view();
    StrictMock<Source> source;
    auto input = source.view();
    const auto input_before = input;
    const auto port_before = port;
    EXPECT_EQ(nx_wait_select(&port, &input, 1, 0, 42,
                             reinterpret_cast<size_t*>(&input.context)),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_wait_select(&port, &input, 1, 0, 42,
                             reinterpret_cast<size_t*>(&port.context)),
              NX_ERROR_INVALID);
    size_t selected = 19;
    EXPECT_EQ(nx_wait_select(&port,
                             reinterpret_cast<const nx_wait_source_t*>(&port),
                             1, 0, 42, &selected),
              NX_ERROR_INVALID);
    EXPECT_EQ(input.context, input_before.context);
    EXPECT_EQ(input.ready, input_before.ready);
    EXPECT_EQ(port.context, port_before.context);
    EXPECT_EQ(port.arm, port_before.arm);
    EXPECT_EQ(port.wait, port_before.wait);
    EXPECT_EQ(selected, 19U);
}
} /* namespace */
