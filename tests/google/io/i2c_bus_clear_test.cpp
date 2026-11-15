/**
 * \file            i2c_bus_clear_test.cpp
 * \brief           Actual bus-clear logic through explicit electrical ports
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/i2c_bus_clear.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <utility>
#include <vector>

namespace {
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoDefault;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

/** \brief Mock only the caller's electrical/resource operations. */
class Pins {
  public:
    MOCK_METHOD(nx_result_t, acquire, ());
    MOCK_METHOD(nx_result_t, drive, (bool, bool));
    MOCK_METHOD(nx_result_t, sample, (bool*, bool*));
    MOCK_METHOD(nx_result_t, wait, (nx_time_us_t));
    MOCK_METHOD(nx_result_t, release, ());
};

/** \brief Supply wired-AND levels and a deterministic external clock. */
class BusClear : public ::testing::Test {
  protected:
    NiceMock<Pins> pins;
    nx_i2c_bus_clear_t clear{};
    nx_i2c_bus_clear_result_t result{};
    nx_time_us_t now = 10;
    bool scl_low = false;
    bool sda_low = false;
    bool leased = false;
    bool stuck_sda = false;
    nx_time_us_t scl_stretch_until = 0;
    unsigned release_after = 0;
    unsigned pulses = 0;
    std::vector<std::pair<bool, bool>> drives;
    std::vector<nx_time_us_t> waits;

    /** \brief A failed handoff cannot acquire either external resource. */
    static nx_result_t acquire(void* context) {
        return static_cast<BusClear*>(context)->pins.acquire();
    }

    /** \brief Both pin values express low sinks or high-impedance release. */
    static nx_result_t drive(void* context, bool scl, bool sda) {
        return static_cast<BusClear*>(context)->pins.drive(scl, sda);
    }

    /** \brief GPIO snapshots are external observations, not output latches. */
    static nx_result_t sample(void* context, bool* scl, bool* sda) {
        return static_cast<BusClear*>(context)->pins.sample(scl, sda);
    }

    /** \brief Waiting changes only the fixture's explicit clock domain. */
    static nx_result_t wait(void* context, nx_time_us_t target) {
        return static_cast<BusClear*>(context)->pins.wait(target);
    }

    /** \brief The provider restores ownership only on successful release. */
    static nx_result_t release(void* context) {
        return static_cast<BusClear*>(context)->pins.release();
    }

    /** \brief Preserve a clock independent of the tested state machine. */
    static nx_time_us_t read(void* context) {
        return static_cast<BusClear*>(context)->now;
    }

    /** \brief Install external electrical behavior without running recovery. */
    void SetUp() override {
        ON_CALL(pins, acquire()).WillByDefault(Invoke([this]() {
            leased = true;
            scl_low = false;
            sda_low = false;
            return NX_SUCCESS;
        }));
        ON_CALL(pins, drive(_, _))
            .WillByDefault(Invoke([this](bool scl, bool sda) {
                EXPECT_TRUE(leased);
                if (scl_low && !scl && !sda) {
                    ++pulses;
                }
                scl_low = scl;
                sda_low = sda;
                drives.emplace_back(scl, sda);
                return NX_SUCCESS;
            }));
        EXPECT_CALL(pins, drive(_, _))
            .Times(AnyNumber())
            .WillRepeatedly(DoDefault());
        ON_CALL(pins, sample(_, _))
            .WillByDefault(Invoke([this](bool* scl, bool* sda) {
                EXPECT_TRUE(leased);
                *scl = !scl_low && now >= scl_stretch_until;
                *sda = !sda_low && !stuck_sda && pulses >= release_after;
                return NX_SUCCESS;
            }));
        ON_CALL(pins, wait(_))
            .WillByDefault(Invoke([this](nx_time_us_t target) {
                EXPECT_GT(target, now);
                waits.push_back(target);
                now = target;
                return NX_SUCCESS;
            }));
        ON_CALL(pins, release()).WillByDefault(Invoke([this]() {
            EXPECT_TRUE(leased);
            leased = false;
            return NX_SUCCESS;
        }));
    }

    /** \brief Bind reviewed timings and a separate caller-owned session. */
    nx_result_t initialize(nx_i2c_bus_clear_t* session = nullptr) {
        static const nx_i2c_bus_clear_ops_t ops = {acquire, drive, sample, wait,
                                                   release};
        return nx_i2c_bus_clear_init(session == nullptr ? &clear : session,
                                     {&ops, this}, {read, this}, {5, 5, 5, 2});
    }
};

TEST_F(BusClear, BindingHasNoGPIOClockOrLeaseEffects) {
    EXPECT_CALL(pins, acquire()).Times(0);
    EXPECT_CALL(pins, drive(_, _)).Times(0);
    EXPECT_CALL(pins, sample(_, _)).Times(0);
    EXPECT_CALL(pins, wait(_)).Times(0);
    EXPECT_CALL(pins, release()).Times(0);
    EXPECT_EQ(initialize(), NX_SUCCESS);
    EXPECT_FALSE(clear.result.lease_held);
    EXPECT_FALSE(clear.clock_observed);
}

TEST_F(BusClear, IncompletePortsOrZeroTimingRejectWithoutChangingStorage) {
    static const nx_i2c_bus_clear_ops_t ops = {acquire, drive, sample, wait,
                                               release};
    auto broken = ops;
    broken.sample = nullptr;
    EXPECT_EQ(nx_i2c_bus_clear_init(&clear, {&broken, this}, {read, this},
                                    {5, 5, 5, 2}),
              NX_ERROR_INVALID);
    EXPECT_FALSE(clear.initialized);
    EXPECT_EQ(
        nx_i2c_bus_clear_init(&clear, {&ops, this}, {read, this}, {0, 5, 5, 2}),
        NX_ERROR_INVALID);
    EXPECT_EQ(nx_i2c_bus_clear_init(&clear, {&ops, this}, {nullptr, this},
                                    {5, 5, 5, 2}),
              NX_ERROR_INVALID);
    EXPECT_FALSE(clear.initialized);
}

TEST_F(BusClear, InvalidArgumentsAndUnboundedDeadlinesHaveNoEffects) {
    ASSERT_EQ(initialize(), NX_SUCCESS);
    EXPECT_CALL(pins, acquire()).Times(0);
    result.primary = NX_ERROR_NACK;
    EXPECT_EQ(nx_i2c_bus_clear_run(&clear, NX_DEADLINE_NEVER, 200, &result),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_i2c_bus_clear_run(&clear, 100, NX_DEADLINE_NEVER, &result),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_i2c_bus_clear_run(&clear, 100, 99, &result), NX_ERROR_INVALID);
    EXPECT_EQ(nx_i2c_bus_clear_run(&clear, 100, 200, nullptr),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_i2c_bus_clear_run(nullptr, 100, 200, &result),
              NX_ERROR_INVALID);
    EXPECT_EQ(result.primary, NX_ERROR_NACK);
}

TEST_F(BusClear, ExpiredDeadlineRejectsBeforeAcquiringOrSamplingPins) {
    ASSERT_EQ(initialize(), NX_SUCCESS);
    now = 100;
    EXPECT_CALL(pins, acquire()).Times(0);
    EXPECT_CALL(pins, sample(_, _)).Times(0);
    EXPECT_EQ(nx_i2c_bus_clear_run(&clear, 100, 200, &result),
              NX_ERROR_TIMEOUT);
    EXPECT_EQ(result.primary, NX_ERROR_TIMEOUT);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_FALSE(result.lease_held);
}

TEST_F(BusClear, FailedLeaseHasNoPinOrReleaseEffects) {
    ASSERT_EQ(initialize(), NX_SUCCESS);
    EXPECT_CALL(pins, acquire()).WillOnce(Return(NX_ERROR_BUSY));
    EXPECT_CALL(pins, drive(_, _)).Times(0);
    EXPECT_CALL(pins, sample(_, _)).Times(0);
    EXPECT_CALL(pins, release()).Times(0);
    EXPECT_EQ(nx_i2c_bus_clear_run(&clear, 100, 200, &result), NX_ERROR_BUSY);
    EXPECT_EQ(result.primary, NX_ERROR_BUSY);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_FALSE(result.lease_held);
}

TEST_F(BusClear, IdleBusNeedsNoRecoveryPulsesAndProducesPhysicalSTOP) {
    ASSERT_EQ(initialize(), NX_SUCCESS);
    EXPECT_CALL(pins, release()).Times(1);
    ASSERT_EQ(nx_i2c_bus_clear_run(&clear, 100, 200, &result), NX_SUCCESS);
    EXPECT_EQ(result.primary, NX_SUCCESS);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.pulses, 0U);
    EXPECT_EQ(pulses, 0U);
    EXPECT_TRUE(result.stop_observed);
    EXPECT_FALSE(result.lease_held);
    EXPECT_FALSE(leased);
    const std::vector<std::pair<bool, bool>> expected = {
        {true, false}, {true, true}, {false, true}, {false, false}};
    EXPECT_EQ(drives, expected);
}

TEST_F(BusClear, SDAReleaseStopsRecoveryAfterExactlyThreeCompletedPulses) {
    ASSERT_EQ(initialize(), NX_SUCCESS);
    release_after = 3;
    ASSERT_EQ(nx_i2c_bus_clear_run(&clear, 100, 200, &result), NX_SUCCESS);
    EXPECT_EQ(result.pulses, 3U);
    EXPECT_EQ(pulses, 3U);
    EXPECT_TRUE(result.stop_observed);
    EXPECT_FALSE(result.lease_held);
}

TEST_F(BusClear, StuckSDAStopsAtNineAndFailedSTOPRetainsExclusiveLease) {
    ASSERT_EQ(initialize(), NX_SUCCESS);
    stuck_sda = true;
    EXPECT_CALL(pins, release()).Times(0);
    EXPECT_EQ(nx_i2c_bus_clear_run(&clear, 1000, 1100, &result), NX_ERROR_IO);
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_EQ(result.cleanup, NX_ERROR_IO);
    EXPECT_EQ(result.pulses, NX_I2C_BUS_CLEAR_MAX_PULSES);
    EXPECT_EQ(pulses, NX_I2C_BUS_CLEAR_MAX_PULSES);
    EXPECT_TRUE(result.lease_held);
    EXPECT_TRUE(leased);
    EXPECT_FALSE(result.stop_observed);
}

TEST_F(BusClear, ClockStretchUsesPhysicalSCLAndFiniteSeparateBudgets) {
    ASSERT_EQ(initialize(), NX_SUCCESS);
    scl_stretch_until = 1000;
    EXPECT_CALL(pins, release()).Times(0);
    EXPECT_EQ(nx_i2c_bus_clear_run(&clear, 30, 60, &result), NX_ERROR_TIMEOUT);
    EXPECT_EQ(result.primary, NX_ERROR_TIMEOUT);
    EXPECT_EQ(result.cleanup, NX_ERROR_TIMEOUT);
    EXPECT_EQ(now, 60U);
    EXPECT_EQ(result.pulses, 0U);
    EXPECT_TRUE(result.lease_held);
    EXPECT_LT(waits.size(), 30U);
}

TEST_F(BusClear, FiniteInitialStretchCompletesWithoutRecoveryPulses) {
    ASSERT_EQ(initialize(), NX_SUCCESS);
    scl_stretch_until = 16;
    ASSERT_EQ(nx_i2c_bus_clear_run(&clear, 100, 200, &result), NX_SUCCESS);
    ASSERT_GE(waits.size(), 3U);
    EXPECT_EQ(waits[0], 12U);
    EXPECT_EQ(waits[1], 14U);
    EXPECT_EQ(waits[2], 16U);
    EXPECT_EQ(result.pulses, 0U);
}

TEST_F(BusClear, DeadlineDuringPulsePreservesTimeoutAndIndependentSTOPSuccess) {
    ASSERT_EQ(initialize(), NX_SUCCESS);
    EXPECT_CALL(pins, sample(_, _))
        .WillOnce(Invoke([](bool* scl, bool* sda) {
            *scl = true;
            *sda = false;
            return NX_SUCCESS;
        }))
        .WillRepeatedly(Invoke([this](bool* scl, bool* sda) {
            *scl = !scl_low;
            *sda = !sda_low;
            return NX_SUCCESS;
        }));
    EXPECT_EQ(nx_i2c_bus_clear_run(&clear, 15, 100, &result), NX_ERROR_TIMEOUT);
    EXPECT_EQ(result.primary, NX_ERROR_TIMEOUT);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.pulses, 0U);
    EXPECT_TRUE(result.stop_observed);
    EXPECT_FALSE(result.lease_held);
}

TEST_F(BusClear, PrimaryPinErrorIsPreservedAfterSuccessfulSTOPAndRelease) {
    ASSERT_EQ(initialize(), NX_SUCCESS);
    EXPECT_CALL(pins, drive(true, false))
        .WillOnce(Return(NX_ERROR_PERMISSION))
        .WillRepeatedly(DoDefault());
    EXPECT_CALL(pins, sample(_, _))
        .WillOnce(Invoke([](bool* scl, bool* sda) {
            *scl = true;
            *sda = false;
            return NX_SUCCESS;
        }))
        .WillRepeatedly(Invoke([this](bool* scl, bool* sda) {
            *scl = !scl_low;
            *sda = !sda_low;
            return NX_SUCCESS;
        }));
    EXPECT_EQ(nx_i2c_bus_clear_run(&clear, 100, 200, &result),
              NX_ERROR_PERMISSION);
    EXPECT_EQ(result.primary, NX_ERROR_PERMISSION);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_FALSE(result.lease_held);
}

TEST_F(BusClear, STOPErrorRetainsLeaseAndRetryNeverReplaysRecoveryPulses) {
    ASSERT_EQ(initialize(), NX_SUCCESS);
    EXPECT_CALL(pins, drive(false, true)).WillOnce(Return(NX_ERROR_IO));
    EXPECT_CALL(pins, release()).Times(0);
    EXPECT_EQ(nx_i2c_bus_clear_run(&clear, 100, 200, &result), NX_ERROR_IO);
    EXPECT_EQ(result.primary, NX_SUCCESS);
    EXPECT_EQ(result.cleanup, NX_ERROR_IO);
    EXPECT_TRUE(result.lease_held);
    EXPECT_FALSE(result.stop_observed);
    ::testing::Mock::VerifyAndClearExpectations(&pins);
    EXPECT_CALL(pins, acquire()).Times(0);
    EXPECT_CALL(pins, release()).Times(1);
    ASSERT_EQ(nx_i2c_bus_clear_finish(&clear, 300, &result), NX_SUCCESS);
    EXPECT_EQ(result.primary, NX_SUCCESS);
    EXPECT_EQ(result.pulses, 0U);
    EXPECT_EQ(pulses, 0U);
    EXPECT_TRUE(result.stop_observed);
    EXPECT_FALSE(result.lease_held);
}

TEST_F(BusClear, FailedReleaseRetriesOnlyOwnershipReturnAfterProvedSTOP) {
    ASSERT_EQ(initialize(), NX_SUCCESS);
    EXPECT_CALL(pins, release()).WillOnce(Return(NX_ERROR_BUSY));
    EXPECT_EQ(nx_i2c_bus_clear_run(&clear, 100, 200, &result), NX_ERROR_BUSY);
    EXPECT_TRUE(result.stop_observed);
    EXPECT_TRUE(result.lease_held);
    ::testing::Mock::VerifyAndClearExpectations(&pins);
    EXPECT_CALL(pins, drive(_, _)).Times(0);
    EXPECT_CALL(pins, sample(_, _)).Times(0);
    EXPECT_CALL(pins, acquire()).Times(0);
    EXPECT_CALL(pins, release()).Times(1);
    ASSERT_EQ(nx_i2c_bus_clear_finish(&clear, 300, &result), NX_SUCCESS);
    EXPECT_FALSE(result.lease_held);
}

TEST_F(BusClear, RetryPreservesOriginalPrimaryFailureAfterBusRecovers) {
    ASSERT_EQ(initialize(), NX_SUCCESS);
    stuck_sda = true;
    ASSERT_EQ(nx_i2c_bus_clear_run(&clear, 1000, 1100, &result), NX_ERROR_IO);
    stuck_sda = false;
    ASSERT_EQ(nx_i2c_bus_clear_finish(&clear, 1200, &result), NX_SUCCESS);
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.pulses, 9U);
    EXPECT_EQ(pulses, 9U);
    EXPECT_FALSE(result.lease_held);
}

TEST_F(BusClear, RetainedLeaseRejectsNewClearAndRebindingWithoutEffects) {
    ASSERT_EQ(initialize(), NX_SUCCESS);
    stuck_sda = true;
    ASSERT_EQ(nx_i2c_bus_clear_run(&clear, 1000, 1100, &result), NX_ERROR_IO);
    EXPECT_CALL(pins, acquire()).Times(0);
    EXPECT_CALL(pins, drive(_, _)).Times(0);
    EXPECT_CALL(pins, sample(_, _)).Times(0);
    EXPECT_CALL(pins, release()).Times(0);
    result.primary = NX_ERROR_NACK;
    EXPECT_EQ(nx_i2c_bus_clear_run(&clear, 2000, 2100, &result),
              NX_ERROR_STATE);
    EXPECT_EQ(initialize(), NX_ERROR_BUSY);
    EXPECT_EQ(result.primary, NX_ERROR_NACK);
    EXPECT_TRUE(clear.result.lease_held);
}

TEST_F(BusClear, FinishingWithoutLeaseOrWithInfiniteBudgetHasNoEffects) {
    ASSERT_EQ(initialize(), NX_SUCCESS);
    EXPECT_CALL(pins, release()).Times(0);
    EXPECT_EQ(nx_i2c_bus_clear_finish(&clear, 100, &result), NX_ERROR_STATE);
    EXPECT_EQ(nx_i2c_bus_clear_finish(&clear, NX_DEADLINE_NEVER, &result),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_i2c_bus_clear_finish(nullptr, 100, &result), NX_ERROR_INVALID);
}

TEST_F(BusClear, FrozenClockExitsRatherThanRepeatingStretchSamplesForever) {
    ASSERT_EQ(initialize(), NX_SUCCESS);
    scl_stretch_until = 1000;
    EXPECT_CALL(pins, wait(_)).WillRepeatedly(Return(NX_SUCCESS));
    EXPECT_CALL(pins, release()).Times(0);
    EXPECT_EQ(nx_i2c_bus_clear_run(&clear, 100, 200, &result), NX_ERROR_IO);
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_EQ(result.cleanup, NX_ERROR_IO);
    EXPECT_EQ(now, 10U);
    EXPECT_TRUE(result.lease_held);
}

TEST_F(BusClear, ReversedClockIsRejectedBeforeASecondPinHandoff) {
    ASSERT_EQ(initialize(), NX_SUCCESS);
    ASSERT_EQ(nx_i2c_bus_clear_run(&clear, 100, 200, &result), NX_SUCCESS);
    now = 1;
    EXPECT_CALL(pins, acquire()).Times(0);
    EXPECT_EQ(nx_i2c_bus_clear_run(&clear, 100, 200, &result), NX_ERROR_IO);
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_FALSE(result.lease_held);
}

TEST_F(BusClear, ExhaustedClockDomainCannotAcquireOrDrivePins) {
    ASSERT_EQ(initialize(), NX_SUCCESS);
    now = NX_DEADLINE_NEVER;
    EXPECT_CALL(pins, acquire()).Times(0);
    EXPECT_EQ(nx_i2c_bus_clear_run(&clear, 100, 200, &result), NX_ERROR_IO);
    EXPECT_FALSE(result.lease_held);
}

TEST_F(BusClear, WaitFailurePreservesItsPrimaryAndAllowsBoundedSTOPCleanup) {
    ASSERT_EQ(initialize(), NX_SUCCESS);
    scl_stretch_until = 12;
    EXPECT_CALL(pins, wait(_))
        .WillOnce(Invoke([this](nx_time_us_t target) {
            now = target;
            return NX_ERROR_CANCELLED;
        }))
        .WillRepeatedly(Invoke([this](nx_time_us_t target) {
            now = target;
            return NX_SUCCESS;
        }));
    EXPECT_EQ(nx_i2c_bus_clear_run(&clear, 100, 200, &result),
              NX_ERROR_CANCELLED);
    EXPECT_EQ(result.primary, NX_ERROR_CANCELLED);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_FALSE(result.lease_held);
}

TEST_F(BusClear, SuccessfulLateLeaseReturnReportsTimeoutWithoutInventingLease) {
    ASSERT_EQ(initialize(), NX_SUCCESS);
    EXPECT_CALL(pins, release()).WillOnce(Invoke([this]() {
        leased = false;
        now = 201;
        return NX_SUCCESS;
    }));
    EXPECT_EQ(nx_i2c_bus_clear_run(&clear, 100, 200, &result),
              NX_ERROR_TIMEOUT);
    EXPECT_EQ(result.primary, NX_SUCCESS);
    EXPECT_EQ(result.cleanup, NX_ERROR_TIMEOUT);
    EXPECT_TRUE(result.stop_observed);
    EXPECT_FALSE(result.lease_held);
    EXPECT_FALSE(leased);
}

TEST_F(BusClear, ExpiredFinishRetainsLeaseWithoutPinOrReturnEffects) {
    ASSERT_EQ(initialize(), NX_SUCCESS);
    stuck_sda = true;
    ASSERT_EQ(nx_i2c_bus_clear_run(&clear, 1000, 1100, &result), NX_ERROR_IO);
    EXPECT_CALL(pins, drive(_, _)).Times(0);
    EXPECT_CALL(pins, release()).Times(0);
    EXPECT_EQ(nx_i2c_bus_clear_finish(&clear, now, &result), NX_ERROR_TIMEOUT);
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_TRUE(result.lease_held);
}

TEST_F(BusClear, DistinctSessionsDoNotAcquireOrModifyAnotherInstancesLease) {
    nx_i2c_bus_clear_t other{};
    ASSERT_EQ(initialize(), NX_SUCCESS);
    ASSERT_EQ(initialize(&other), NX_SUCCESS);
    stuck_sda = true;
    ASSERT_EQ(nx_i2c_bus_clear_run(&clear, 1000, 1100, &result), NX_ERROR_IO);
    EXPECT_FALSE(other.result.lease_held);
    EXPECT_EQ(other.result.pulses, 0U);
    EXPECT_FALSE(other.clock_observed);
}
TEST_F(BusClear, FailedPhysicalSCLLowCannotCountAPulseOrReturnTheLease) {
    ASSERT_EQ(initialize(), NX_SUCCESS);
    release_after = 1;
    EXPECT_CALL(pins, sample(_, _))
        .WillRepeatedly(Invoke([this](bool* scl, bool* sda) {
            *scl = true;
            *sda = !sda_low && pulses >= release_after;
            return NX_SUCCESS;
        }));
    EXPECT_CALL(pins, release()).Times(0);
    EXPECT_EQ(nx_i2c_bus_clear_run(&clear, 100, 200, &result), NX_ERROR_IO);
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_EQ(result.cleanup, NX_ERROR_IO);
    EXPECT_EQ(result.pulses, 0U);
    EXPECT_TRUE(result.lease_held);
}

TEST_F(BusClear, STOPRequiresObservedSDALowBeforeReleasingItsClock) {
    ASSERT_EQ(initialize(), NX_SUCCESS);
    EXPECT_CALL(pins, sample(_, _))
        .WillRepeatedly(Invoke([this](bool* scl, bool* sda) {
            *scl = !scl_low;
            *sda = true;
            return NX_SUCCESS;
        }));
    EXPECT_CALL(pins, drive(false, true)).Times(0);
    EXPECT_CALL(pins, release()).Times(0);
    EXPECT_EQ(nx_i2c_bus_clear_run(&clear, 100, 200, &result), NX_ERROR_IO);
    EXPECT_EQ(result.primary, NX_SUCCESS);
    EXPECT_EQ(result.cleanup, NX_ERROR_IO);
    EXPECT_FALSE(result.stop_observed);
    EXPECT_TRUE(result.lease_held);
}

TEST_F(BusClear, SCLFallingDuringSTOPSetupDoesNotReleaseSDAOrInventSTOP) {
    ASSERT_EQ(initialize(), NX_SUCCESS);
    EXPECT_CALL(pins, wait(_))
        .WillRepeatedly(Invoke([this](nx_time_us_t target) {
            now = target;
            if (!scl_low && sda_low) {
                scl_stretch_until = now + 1000;
            }
            return NX_SUCCESS;
        }));
    EXPECT_CALL(pins, drive(false, false)).Times(0);
    EXPECT_CALL(pins, release()).Times(0);
    EXPECT_EQ(nx_i2c_bus_clear_run(&clear, 100, 200, &result), NX_ERROR_IO);
    EXPECT_EQ(result.primary, NX_SUCCESS);
    EXPECT_FALSE(result.stop_observed);
    EXPECT_TRUE(result.lease_held);
}

TEST_F(BusClear, ShortenedPhysicalHighPeriodDoesNotCountACompletedPulse) {
    ASSERT_EQ(initialize(), NX_SUCCESS);
    release_after = 1;
    EXPECT_CALL(pins, wait(_))
        .WillRepeatedly(Invoke([this](nx_time_us_t target) {
            now = target;
            if (!scl_low && !sda_low) {
                scl_stretch_until = now + 1000;
            }
            return NX_SUCCESS;
        }));
    EXPECT_CALL(pins, release()).Times(0);
    EXPECT_EQ(nx_i2c_bus_clear_run(&clear, 100, 200, &result), NX_ERROR_IO);
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_EQ(result.pulses, 0U);
    EXPECT_TRUE(result.lease_held);
}
} /* namespace */
