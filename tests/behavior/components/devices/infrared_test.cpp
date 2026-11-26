/**
 * \file            infrared_test.cpp
 * \brief           Real NEC decoding with an independently mocked EXTI source
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/infrared.h"
#include <array>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>

namespace {
using testing::_;
using testing::Invoke;
using testing::StrictMock;

TEST(InfraredLayout, BoundSourceConfigurationFitsFortyBytes) {
    EXPECT_LE(sizeof(nx_infrared_nec_config_t), 40U);
}

class EdgeSource {
  public:
    MOCK_METHOD(nx_result_t, Read, (nx_exti_event_t*, size_t, size_t*), ());
    static nx_result_t read(void* context, nx_exti_event_t* events,
                            size_t capacity, size_t* count) {
        return static_cast<EdgeSource*>(context)->Read(events, capacity, count);
    }
};
const nx_exti_ops_t edge_ops{EdgeSource::read, nullptr, nullptr, nullptr};

class Infrared : public testing::Test {
  protected:
    nx_infrared_nec_t owner{};
    nx_infrared_nec_config_t config{{8800U, 9200U},
                                    {4300U, 4700U},
                                    {2000U, 2500U},
                                    {400U, 750U},
                                    {400U, 750U},
                                    {1400U, 1900U},
                                    120000U,
                                    200000U,
                                    NX_INFRARED_NEC_STANDARD,
                                    NX_INFRARED_SOURCE_STRICT};
    nx_time_us_t time = 1000U;
    nx_infrared_nec_result_t Edge(nx_exti_edge_t edge, uint32_t after = 0U,
                                  uint32_t flags = 0U) {
        time += after;
        const nx_exti_event_t event{time, flags, edge};
        return nx_infrared_nec_feed(&owner, &event);
    }
    void Init() {
        ASSERT_EQ(NX_SUCCESS, nx_infrared_nec_init(&owner, &config));
    }
    nx_infrared_nec_result_t Word(uint32_t word, bool trailer = true,
                                  unsigned bits = 32U) {
        auto result = Edge(NX_EXTI_FALLING);
        EXPECT_EQ(NX_ERROR_EMPTY, result.result);
        EXPECT_EQ(NX_ERROR_EMPTY, Edge(NX_EXTI_RISING, 9000U).result);
        EXPECT_EQ(NX_ERROR_EMPTY, Edge(NX_EXTI_FALLING, 4500U).result);
        for (unsigned i = 0U; i < bits; ++i) {
            EXPECT_EQ(NX_ERROR_EMPTY, Edge(NX_EXTI_RISING, 560U).result);
            result = Edge(NX_EXTI_FALLING,
                          (word & (UINT32_C(1) << i)) != 0U ? 1690U : 560U);
            EXPECT_EQ(NX_ERROR_EMPTY, result.result);
        }
        return trailer ? Edge(NX_EXTI_RISING, 560U) : result;
    }
    nx_infrared_nec_result_t Repeat(uint32_t gap = 40000U) {
        EXPECT_EQ(NX_ERROR_EMPTY, Edge(NX_EXTI_FALLING, gap).result);
        EXPECT_EQ(NX_ERROR_EMPTY, Edge(NX_EXTI_RISING, 9000U).result);
        auto leader = Edge(NX_EXTI_FALLING, 2250U);
        if (leader.result != NX_ERROR_EMPTY)
            return leader;
        return Edge(NX_EXTI_RISING, 560U);
    }
};

TEST_F(Infrared, FullStandardFrameRequiresEveryBitAndTrailer) {
    Init();
    const auto frame = Word(UINT32_C(0xCB34ED12));
    ASSERT_EQ(NX_SUCCESS, frame.result);
    EXPECT_EQ(0x12U, frame.frame.address);
    EXPECT_EQ(0x34U, frame.frame.command);
    EXPECT_FALSE(frame.frame.repeat);
    EXPECT_EQ(time, frame.frame.timestamp_us);
}

TEST_F(Infrared, ThirtyOneBitsAndTruncationCannotPublishACommand) {
    Init();
    EXPECT_EQ(NX_ERROR_EMPTY, Word(UINT32_C(0xCB34ED12), true, 31U).result);
    EXPECT_EQ(NX_ERROR_TIMEOUT, Edge(NX_EXTI_FALLING, 200000U).result);
    EXPECT_FALSE(owner.last_valid);
}

TEST_F(Infrared, StandardAddressInverseFailureNeverSelectsExtendedSilently) {
    Init();
    EXPECT_EQ(NX_ERROR_IO, Word(UINT32_C(0xCB343412)).result);
    EXPECT_FALSE(owner.last_valid);
    EXPECT_EQ(NX_SUCCESS, nx_infrared_nec_stop(&owner));
    config.protocol = NX_INFRARED_NEC_EXTENDED;
    Init();
    const auto frame = Word(UINT32_C(0xCB343412));
    ASSERT_EQ(NX_SUCCESS, frame.result);
    EXPECT_EQ(0x3412U, frame.frame.address);
}

TEST_F(Infrared, CommandInverseIsRequiredForBothAddressPolicies) {
    for (auto protocol : {NX_INFRARED_NEC_STANDARD, NX_INFRARED_NEC_EXTENDED}) {
        config.protocol = protocol;
        Init();
        EXPECT_EQ(NX_ERROR_IO, Word(UINT32_C(0x3434ED12)).result);
        EXPECT_EQ(NX_SUCCESS, nx_infrared_nec_stop(&owner));
    }
}

TEST_F(Infrared, InvalidMarkOrSpaceClearsPartialFrameAndRepeatHistory) {
    Init();
    ASSERT_EQ(NX_SUCCESS, Word(UINT32_C(0xCB34ED12)).result);
    EXPECT_EQ(NX_ERROR_EMPTY, Edge(NX_EXTI_FALLING, 40000U).result);
    EXPECT_EQ(NX_ERROR_IO, Edge(NX_EXTI_RISING, 3000U).result);
    EXPECT_FALSE(owner.last_valid);
    EXPECT_NE(NX_SUCCESS, Repeat().result);
}

TEST_F(Infrared, RepeatCarriesOnlyThePreviousValidatedAddressAndCommand) {
    Init();
    ASSERT_EQ(NX_SUCCESS, Word(UINT32_C(0xCB34ED12)).result);
    const auto frame = Repeat();
    ASSERT_EQ(NX_SUCCESS, frame.result);
    EXPECT_TRUE(frame.frame.repeat);
    EXPECT_EQ(0x12U, frame.frame.address);
    EXPECT_EQ(0x34U, frame.frame.command);
    EXPECT_EQ(time, frame.frame.timestamp_us);
    EXPECT_EQ(NX_SUCCESS, Repeat().result);
}

TEST_F(Infrared, RepeatWithoutAValidPredecessorOrAfterAgeLimitIsRejected) {
    Init();
    EXPECT_NE(NX_SUCCESS, Repeat().result);
    EXPECT_EQ(NX_SUCCESS, nx_infrared_nec_stop(&owner));
    Init();
    ASSERT_EQ(NX_SUCCESS, Word(UINT32_C(0xCB34ED12)).result);
    EXPECT_EQ(NX_ERROR_TIMEOUT, Repeat(210000U).result);
    EXPECT_FALSE(owner.last_valid);
}

TEST_F(Infrared, LossAndCoalescedEventsInvalidateFramesAndRepeats) {
    for (const auto flag : {NX_EXTI_EVENT_LOSS, NX_EXTI_EVENT_COALESCED}) {
        Init();
        ASSERT_EQ(NX_SUCCESS, Word(UINT32_C(0xCB34ED12)).result);
        const nx_exti_event_t event{time + 900000U, flag,
                                    static_cast<nx_exti_edge_t>(0)};
        EXPECT_EQ(NX_ERROR_OVERFLOW,
                  nx_infrared_nec_feed(&owner, &event).result);
        EXPECT_FALSE(owner.last_valid);
        EXPECT_FALSE(owner.clock_fault);
        EXPECT_EQ(NX_ERROR_EMPTY, Edge(NX_EXTI_FALLING, 1000U).result);
        EXPECT_EQ(NX_SUCCESS, nx_infrared_nec_stop(&owner));
    }
}

TEST_F(Infrared, EqualOrBackwardActualTimestampLatchesUntilExplicitRebind) {
    Init();
    EXPECT_EQ(NX_ERROR_EMPTY, Edge(NX_EXTI_FALLING).result);
    EXPECT_EQ(NX_ERROR_IO, Edge(NX_EXTI_RISING).result);
    EXPECT_TRUE(owner.clock_fault);
    EXPECT_EQ(NX_ERROR_IO, Edge(NX_EXTI_FALLING, 200000U).result);
    EXPECT_EQ(NX_SUCCESS, nx_infrared_nec_stop(&owner));
    Init();
    time = 5U;
    EXPECT_EQ(NX_SUCCESS, Word(UINT32_C(0xCB34ED12)).result);
}

TEST_F(Infrared, UnknownFlagsAndMalformedEdgeRejectUnchanged) {
    Init();
    const auto before = owner;
    for (const nx_exti_event_t event :
         {nx_exti_event_t{time, 8U, NX_EXTI_FALLING},
          nx_exti_event_t{time, 0U, NX_EXTI_BOTH},
          nx_exti_event_t{NX_DEADLINE_NEVER, 0U, NX_EXTI_FALLING}}) {
        EXPECT_EQ(NX_ERROR_INVALID,
                  nx_infrared_nec_feed(&owner, &event).result);
        EXPECT_EQ(0, std::memcmp(&before, &owner, sizeof(owner)));
    }
}

TEST_F(Infrared, ConfigurationRequiresDisjointUnambiguousBoundedWindows) {
    for (unsigned fault = 0U; fault < 5U; ++fault) {
        auto invalid = config;
        if (fault == 0U)
            invalid.zero_space = invalid.one_space;
        if (fault == 1U)
            invalid.bit_mark.minimum_us = 0U;
        if (fault == 2U)
            invalid.repeat_space = invalid.leader_space;
        if (fault == 3U)
            invalid.frame_max_us = 1000U;
        if (fault == 4U)
            invalid.repeat_max_us = 5000001U;
        EXPECT_EQ(NX_ERROR_INVALID, nx_infrared_nec_init(&owner, &invalid));
        EXPECT_FALSE(owner.initialized);
    }
}

TEST_F(Infrared, KnownOwnerAndConfigAliasesRejectBeforeStateMutation) {
    Init();
    const auto before = owner;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_infrared_nec_feed(
                  &owner, reinterpret_cast<const nx_exti_event_t*>(&owner))
                  .result);
    EXPECT_EQ(0, std::memcmp(&before, &owner, sizeof(owner)));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_infrared_nec_feed(
                  &owner, reinterpret_cast<const nx_exti_event_t*>(&config))
                  .result);
    EXPECT_EQ(0, std::memcmp(&before, &owner, sizeof(owner)));
}

TEST_F(Infrared, PollReadsExactlyOneCopiedPublicEventWithoutSourceOwnership) {
    Init();
    StrictMock<EdgeSource> source;
    const nx_exti_port_t port{&edge_ops, &source};
    EXPECT_CALL(source, Read(_, 1U, _))
        .WillOnce(Invoke([&](nx_exti_event_t* events, size_t, size_t* count) {
            *events = {time, 0U, NX_EXTI_FALLING};
            *count = 1U;
            return NX_SUCCESS;
        }));
    EXPECT_EQ(NX_ERROR_EMPTY, nx_infrared_nec_poll(&owner, &port).result);
    EXPECT_EQ(NX_SUCCESS, nx_infrared_nec_stop(&owner));
}

TEST_F(Infrared, ReadCallbackCannotReenterFeedInitStopOrPoll) {
    Init();
    StrictMock<EdgeSource> source;
    const nx_exti_port_t port{&edge_ops, &source};
    EXPECT_CALL(source, Read(_, 1U, _))
        .WillOnce(Invoke([&](nx_exti_event_t*, size_t, size_t* count) {
            const nx_exti_event_t event{time, 0U, NX_EXTI_FALLING};
            EXPECT_EQ(NX_ERROR_BUSY,
                      nx_infrared_nec_feed(&owner, &event).result);
            EXPECT_EQ(NX_ERROR_BUSY, nx_infrared_nec_init(&owner, &config));
            EXPECT_EQ(NX_ERROR_BUSY, nx_infrared_nec_stop(&owner));
            EXPECT_EQ(NX_ERROR_BUSY,
                      nx_infrared_nec_poll(&owner, &port).result);
            *count = 0U;
            return NX_ERROR_EMPTY;
        }));
    EXPECT_EQ(NX_ERROR_EMPTY, nx_infrared_nec_poll(&owner, &port).result);
    EXPECT_FALSE(owner.in_call);
}

TEST_F(Infrared, FailedOrInconsistentPortReadInvalidatesRepeatEligibility) {
    Init();
    ASSERT_EQ(NX_SUCCESS, Word(UINT32_C(0xCB34ED12)).result);
    StrictMock<EdgeSource> source;
    const nx_exti_port_t port{&edge_ops, &source};
    EXPECT_CALL(source, Read(_, 1U, _))
        .WillOnce(Invoke([](nx_exti_event_t*, size_t, size_t* count) {
            *count = 0U;
            return NX_ERROR_IO;
        }));
    EXPECT_EQ(NX_ERROR_IO, nx_infrared_nec_poll(&owner, &port).result);
    EXPECT_FALSE(owner.last_valid);
    EXPECT_NE(NX_SUCCESS, Repeat().result);
}

TEST_F(Infrared, MissingReadMethodAndNullStorageNeverTouchAPort) {
    const nx_exti_ops_t absent{};
    const nx_exti_port_t port{&absent, &owner};
    EXPECT_EQ(NX_ERROR_INVALID, nx_infrared_nec_init(nullptr, &config));
    Init();
    int context = 0;
    const nx_exti_port_t separate{&absent, &context};
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_infrared_nec_poll(&owner, &separate).result);
    EXPECT_EQ(NX_ERROR_INVALID, nx_infrared_nec_poll(&owner, &port).result);
}

TEST_F(Infrared, IndependentInstancesKeepNoSharedProtocolOrRepeatState) {
    Init();
    nx_infrared_nec_t other{};
    ASSERT_EQ(NX_SUCCESS, nx_infrared_nec_init(&other, &config));
    ASSERT_EQ(NX_SUCCESS, Word(UINT32_C(0xCB34ED12)).result);
    EXPECT_FALSE(other.last_valid);
    const nx_exti_event_t event{1U, 0U, NX_EXTI_FALLING};
    EXPECT_EQ(NX_ERROR_EMPTY, nx_infrared_nec_feed(&other, &event).result);
    EXPECT_TRUE(owner.last_valid);
}

TEST_F(Infrared, FiniteTimestampsNearDomainLimitDoNotWrapWidths) {
    Init();
    time = NX_DEADLINE_NEVER - 200000U;
    EXPECT_EQ(NX_SUCCESS, Word(UINT32_C(0xCB34ED12)).result);
    EXPECT_EQ(NX_SUCCESS, Repeat().result);
}

TEST_F(Infrared, AuthoredWindowBoundariesAcceptFullFramesWithoutRounding) {
    Init();
    constexpr uint32_t word = UINT32_C(0xCB34ED12);
    EXPECT_EQ(NX_ERROR_EMPTY, Edge(NX_EXTI_FALLING).result);
    EXPECT_EQ(NX_ERROR_EMPTY,
              Edge(NX_EXTI_RISING, config.leader_mark.minimum_us).result);
    EXPECT_EQ(NX_ERROR_EMPTY,
              Edge(NX_EXTI_FALLING, config.leader_space.maximum_us).result);
    for (unsigned i = 0U; i < 32U; ++i) {
        EXPECT_EQ(NX_ERROR_EMPTY,
                  Edge(NX_EXTI_RISING, config.bit_mark.minimum_us).result);
        EXPECT_EQ(NX_ERROR_EMPTY,
                  Edge(NX_EXTI_FALLING, (word & (UINT32_C(1) << i)) != 0U
                                            ? config.one_space.maximum_us
                                            : config.zero_space.minimum_us)
                      .result);
    }
    EXPECT_EQ(NX_SUCCESS,
              Edge(NX_EXTI_RISING, config.bit_mark.maximum_us).result);
}

TEST_F(Infrared, AGapBetweenZeroAndOneWindowsCannotBecomeEitherBit) {
    Init();
    EXPECT_EQ(NX_ERROR_EMPTY, Edge(NX_EXTI_FALLING).result);
    EXPECT_EQ(NX_ERROR_EMPTY, Edge(NX_EXTI_RISING, 9000U).result);
    EXPECT_EQ(NX_ERROR_EMPTY, Edge(NX_EXTI_FALLING, 4500U).result);
    EXPECT_EQ(NX_ERROR_EMPTY, Edge(NX_EXTI_RISING, 560U).result);
    EXPECT_EQ(NX_ERROR_IO, Edge(NX_EXTI_FALLING, 1000U).result);
    EXPECT_FALSE(owner.last_valid);
}

TEST_F(Infrared, InvalidPortCountCannotPublishOrRetainAnOlderCommand) {
    for (const auto status : {NX_SUCCESS, NX_ERROR_EMPTY}) {
        Init();
        ASSERT_EQ(NX_SUCCESS, Word(UINT32_C(0xCB34ED12)).result);
        StrictMock<EdgeSource> source;
        const nx_exti_port_t port{&edge_ops, &source};
        EXPECT_CALL(source, Read(_, 1U, _))
            .WillOnce(Invoke([&](nx_exti_event_t*, size_t, size_t* count) {
                *count = status == NX_SUCCESS ? 2U : 1U;
                return status;
            }));
        EXPECT_EQ(NX_ERROR_IO, nx_infrared_nec_poll(&owner, &port).result);
        EXPECT_FALSE(owner.last_valid);
        EXPECT_FALSE(owner.in_call);
        EXPECT_EQ(NX_SUCCESS, nx_infrared_nec_stop(&owner));
    }
}

TEST_F(Infrared, FaultedDecoderNeverConsumesAnotherExternalEvent) {
    Init();
    EXPECT_EQ(NX_ERROR_EMPTY, Edge(NX_EXTI_FALLING).result);
    EXPECT_EQ(NX_ERROR_IO, Edge(NX_EXTI_RISING).result);
    StrictMock<EdgeSource> source;
    const nx_exti_port_t port{&edge_ops, &source};
    EXPECT_CALL(source, Read(_, _, _)).Times(0);
    EXPECT_EQ(NX_ERROR_IO, nx_infrared_nec_poll(&owner, &port).result);
}

TEST_F(Infrared, ConsumedInvalidSourceFactCannotPreserveAnOlderRepeatCommand) {
    Init();
    ASSERT_EQ(NX_SUCCESS, Word(UINT32_C(0xCB34ED12)).result);
    StrictMock<EdgeSource> source;
    const nx_exti_port_t port{&edge_ops, &source};
    EXPECT_CALL(source, Read(_, 1U, _))
        .WillOnce(Invoke([&](nx_exti_event_t* event, size_t, size_t* count) {
            *event = {time + 1U, 8U, NX_EXTI_FALLING};
            *count = 1U;
            return NX_SUCCESS;
        }));
    EXPECT_EQ(NX_ERROR_INVALID, nx_infrared_nec_poll(&owner, &port).result);
    EXPECT_FALSE(owner.last_valid);
    EXPECT_FALSE(owner.clock_fault);
    EXPECT_NE(NX_SUCCESS, Repeat().result);
}

TEST_F(Infrared, ObservationPolicyNeverUpgradesAnEntireFlagFreeFrame) {
    config.source_policy = NX_INFRARED_SOURCE_IRQ_OBSERVATION;
    Init();
    const auto result = Word(UINT32_C(0xCB34ED12));
    ASSERT_EQ(NX_SUCCESS, result.result);
    EXPECT_EQ(NX_INFRARED_QUALITY_SAMPLED_UNCERTAIN, result.frame.quality);
    EXPECT_EQ(0U, result.frame.source_flags);
}

} /* namespace */
