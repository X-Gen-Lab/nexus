/**
 * \file            spi_dma_transaction_test.cpp
 * \brief           Real segmented request owner and backend boundary contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/io/native/model.h"
#include "nexus/io/spi_dma_transaction.h"
#include "spi_dma_sequence.h"
}
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
class SPIDMASequence : public testing::Test {
  protected:
    struct Hardware {
        testing::StrictMock<testing::MockFunction<nx_result_t()>> validate;
        testing::StrictMock<testing::MockFunction<void(bool, uint8_t, uint8_t)>>
            begin;
        nx_spi_request_t* active = nullptr;
        bool complete = false;
        bool quarantined = false;
        bool held = false;
        bool release_fails = false;
        unsigned releases = 0U;
        unsigned begins = 0U;
        unsigned stops = 0U;
        bool successor_owner = false;
        nx_result_t stop_result = NX_SUCCESS;
        bool cancelled = false;
    } hardware;
    const nx_spi_dma_sequence_backend_t backend{
        [](void* p, void*, const nx_spi_dma_transaction_request_t*) {
            return static_cast<Hardware*>(p)->validate.Call();
        },
        [](void* p, void*, nx_spi_request_t* phase, uint8_t bits, uint8_t flags,
           bool retained_cs) {
            auto* h = static_cast<Hardware*>(p);
            h->begin.Call(retained_cs, bits, flags);
            if (retained_cs) {
                EXPECT_TRUE(h->held);
            } else {
                EXPECT_FALSE(h->held);
            }
            h->held = true;
            h->active = phase;
            h->complete = false;
            h->cancelled = false;
            ++h->begins;
            return nx_request_admit(&phase->base, NX_REQUEST_ACTIVE);
        },
        [](void* p) {
            auto* h = static_cast<Hardware*>(p);
            if (h->active == nullptr) {
                return;
            }
            if (h->quarantined) {
                (void)nx_request_transition(&h->active->base,
                                            NX_REQUEST_QUARANTINED);
            } else if (h->complete) {
                auto* phase = h->active;
                h->active = nullptr;
                nx_request_settle(&phase->base,
                                  h->cancelled ? NX_ERROR_CANCELLED
                                               : NX_SUCCESS,
                                  h->cancelled ? 0U : phase->length);
            }
        },
        [](void* p, nx_spi_request_t* phase) {
            auto* h = static_cast<Hardware*>(p);
            EXPECT_EQ(h->active, phase);
            h->cancelled = true;
            return NX_SUCCESS;
        },
        [](void* p, void*) {
            auto* h = static_cast<Hardware*>(p);
            EXPECT_EQ(h->active, nullptr);
            if (h->release_fails) {
                return NX_ERROR_IO;
            }
            h->held = false;
            ++h->releases;
            return NX_SUCCESS;
        },
        [](void* p) {
            auto* h = static_cast<Hardware*>(p);
            ++h->stops;
            if (h->stop_result == NX_SUCCESS) {
                h->successor_owner = false;
            }
            return h->stop_result;
        }};
    nx_spi_dma_sequence_state_t sequence{};
    nx_spi_dma_sequence_endpoint_t binding{&sequence, &hardware};
    nx_spi_dma_transaction_endpoint_t endpoint{
        &nx_spi_dma_sequence_endpoint_ops, &binding};
    nx_spi_dma_transaction_port_t port{&nx_spi_dma_sequence_port_ops,
                                       &sequence};
    nx_spi_dma_transaction_request_t request{};
    uint16_t tx[2]{0x1111U, 0x2222U};
    uint16_t rx[2]{};
    nx_spi_dma_segment_t segments[2]{{tx, rx, 1U, 0U},
                                     {tx + 1U, rx + 1U, 1U, 0U}};
    void SetUp() override {
        ASSERT_EQ(nx_native_clock_configure(true, 100U), NX_SUCCESS);
        sequence.backend = &backend;
        sequence.context = &hardware;
        nx_request_initialize(&request.base);
        ASSERT_EQ(nx_spi_dma_transaction_prepare(&request, segments, 2U, 16U,
                                                 NX_DEADLINE_NEVER),
                  NX_SUCCESS);
    }
    void TearDown() override {
        EXPECT_EQ(nx_native_clock_configure(false, 0U), NX_SUCCESS);
    }
    void Accept() {
        EXPECT_CALL(hardware.validate, Call())
            .WillOnce(testing::Return(NX_SUCCESS));
        EXPECT_CALL(hardware.begin, Call(false, 16U, 0U));
        ASSERT_EQ(nx_spi_dma_transaction_submit(&endpoint, &request),
                  NX_SUCCESS);
    }
};
TEST_F(SPIDMASequence, SecondSegmentKeepsOuterLoanAndCsUntilFinalDrain) {
    ASSERT_NO_FATAL_FAILURE(Accept());
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    hardware.complete = true;
    EXPECT_CALL(hardware.begin, Call(true, 16U, 0U));
    nx_spi_dma_transaction_service(&port);
    EXPECT_EQ(hardware.begins, 2U);
    EXPECT_TRUE(hardware.held);
    EXPECT_EQ(hardware.releases, 0U);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    hardware.complete = true;
    nx_spi_dma_transaction_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, 2U);
    EXPECT_EQ(hardware.releases, 1U);
    EXPECT_EQ(sequence.active, nullptr);
}
TEST_F(SPIDMASequence, FutureRegionFailureRejectsBeforeAnySegmentStarts) {
    EXPECT_CALL(hardware.validate, Call())
        .WillOnce(testing::Return(NX_ERROR_PERMISSION));
    EXPECT_EQ(nx_spi_dma_transaction_submit(&endpoint, &request),
              NX_ERROR_PERMISSION);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(sequence.active, nullptr);
    EXPECT_FALSE(hardware.held);
}
TEST_F(SPIDMASequence, CancelRetainsAllLoansWhileBackendIsQuarantined) {
    ASSERT_NO_FATAL_FAILURE(Accept());
    ASSERT_EQ(nx_spi_dma_transaction_cancel(&port, &request), NX_SUCCESS);
    hardware.quarantined = true;
    nx_spi_dma_transaction_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(sequence.active, &request);
    EXPECT_TRUE(hardware.held);
    EXPECT_EQ(nx_spi_dma_transaction_prepare(&request, segments, 2U, 16U,
                                             NX_DEADLINE_NEVER),
              NX_ERROR_STATE);
    hardware.quarantined = false;
    hardware.complete = true;
    nx_spi_dma_transaction_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(hardware.begins, 1U);
}
TEST_F(SPIDMASequence, FailedCsReleaseRetainsOuterRequestForRetry) {
    request.count = 1U;
    ASSERT_NO_FATAL_FAILURE(Accept());
    hardware.complete = true;
    hardware.release_fails = true;
    nx_spi_dma_transaction_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(sequence.active, &request);
    EXPECT_EQ(hardware.releases, 0U);
    hardware.release_fails = false;
    nx_spi_dma_transaction_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(hardware.releases, 1U);
}
TEST_F(SPIDMASequence, PriorReceiveCannotOverwriteFutureTransmit) {
    segments[0].rx = tx + 1U;
    EXPECT_EQ(nx_spi_dma_transaction_prepare(&request, segments, 2U, 16U,
                                             NX_DEADLINE_NEVER),
              NX_ERROR_INVALID);
}
TEST_F(SPIDMASequence, ReceiveCannotOverwriteSequenceControllerMetadata) {
    segments[0].rx = &sequence;
    ASSERT_EQ(nx_spi_dma_transaction_prepare(&request, segments, 1U, 16U,
                                             NX_DEADLINE_NEVER),
              NX_SUCCESS);
    EXPECT_CALL(hardware.validate, Call()).Times(0);
    EXPECT_CALL(hardware.begin, Call(testing::_, testing::_, testing::_))
        .Times(0);
    nx_result_t result = nx_spi_dma_transaction_submit(&endpoint, &request);
    EXPECT_EQ(result, NX_ERROR_INVALID);
    if (result == NX_SUCCESS) {
        hardware.complete = true;
        nx_spi_dma_transaction_service(&port);
    }
}
TEST_F(SPIDMASequence, TransmitCannotBorrowChangingEmbeddedPhaseMetadata) {
    segments[0].tx = &sequence.phase_request.base.result;
    ASSERT_EQ(nx_spi_dma_transaction_prepare(&request, segments, 1U, 16U,
                                             NX_DEADLINE_NEVER),
              NX_SUCCESS);
    EXPECT_CALL(hardware.validate, Call()).Times(0);
    EXPECT_CALL(hardware.begin, Call(testing::_, testing::_, testing::_))
        .Times(0);
    nx_result_t result = nx_spi_dma_transaction_submit(&endpoint, &request);
    EXPECT_EQ(result, NX_ERROR_INVALID);
    if (result == NX_SUCCESS) {
        hardware.complete = true;
        nx_spi_dma_transaction_service(&port);
    }
}
TEST_F(SPIDMASequence, RepeatedStopCannotWithdrawSuccessorHardwareOwner) {
    ASSERT_EQ(nx_spi_dma_transaction_stop(&port), NX_SUCCESS);
    ASSERT_EQ(hardware.stops, 1U);
    hardware.successor_owner = true;
    EXPECT_EQ(nx_spi_dma_transaction_stop(&port), NX_SUCCESS);
    EXPECT_TRUE(hardware.successor_owner);
    EXPECT_EQ(hardware.stops, 1U);
}
TEST_F(SPIDMASequence, InitializeCannotEraseFailedHardwareStopResponsibility) {
    hardware.stop_result = NX_ERROR_BUSY;
    ASSERT_EQ(nx_spi_dma_transaction_stop(&port), NX_ERROR_BUSY);
    EXPECT_EQ(nx_spi_dma_sequence_initialize(&sequence, &backend, &hardware),
              NX_ERROR_BUSY);
    hardware.stop_result = NX_SUCCESS;
    ASSERT_EQ(nx_spi_dma_transaction_stop(&port), NX_SUCCESS);
    EXPECT_EQ(nx_spi_dma_sequence_initialize(&sequence, &backend, &hardware),
              NX_SUCCESS);
}
TEST_F(SPIDMASequence, IdleInitializeCannotChangeHardwareBinding) {
    Hardware other;
    EXPECT_EQ(nx_spi_dma_sequence_initialize(&sequence, &backend, &other),
              NX_ERROR_INVALID);
    EXPECT_EQ(sequence.backend, &backend);
    EXPECT_EQ(sequence.context, &hardware);
}
TEST_F(SPIDMASequence, ZeroConstructorBindsWithoutHardwareEffects) {
    nx_spi_dma_sequence_state_t cold{};
    ASSERT_EQ(nx_spi_dma_sequence_initialize(&cold, &backend, &hardware),
              NX_SUCCESS);
    EXPECT_EQ(cold.backend, &backend);
    EXPECT_EQ(cold.context, &hardware);
    EXPECT_EQ(cold.active, nullptr);
    EXPECT_FALSE(cold.stopping);
    EXPECT_EQ(nx_request_state(&cold.phase_request.base), NX_REQUEST_READY);
    EXPECT_EQ(hardware.stops, 0U);
    EXPECT_EQ(hardware.begins, 0U);
}
TEST_F(SPIDMASequence, InitializeQuiescentStateReopensAfterStop) {
    ASSERT_EQ(nx_spi_dma_transaction_stop(&port), NX_SUCCESS);
    ASSERT_TRUE(sequence.stopping);
    ASSERT_EQ(nx_spi_dma_sequence_initialize(&sequence, &backend, &hardware),
              NX_SUCCESS);
    EXPECT_FALSE(sequence.stopping);
    EXPECT_EQ(sequence.active, nullptr);
    request.count = 1U;
    ASSERT_NO_FATAL_FAILURE(Accept());
    hardware.complete = true;
    nx_spi_dma_transaction_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
}
TEST_F(SPIDMASequence, InitializeCannotRevokeLiveOrQuarantinedLoans) {
    ASSERT_NO_FATAL_FAILURE(Accept());
    EXPECT_EQ(nx_spi_dma_sequence_initialize(&sequence, &backend, &hardware),
              NX_ERROR_BUSY);
    EXPECT_EQ(sequence.active, &request);
    hardware.quarantined = true;
    nx_spi_dma_transaction_service(&port);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(nx_spi_dma_sequence_initialize(&sequence, &backend, &hardware),
              NX_ERROR_BUSY);
    EXPECT_EQ(sequence.active, &request);
    hardware.quarantined = false;
    ASSERT_EQ(nx_spi_dma_transaction_cancel(&port, &request), NX_SUCCESS);
    hardware.complete = true;
    nx_spi_dma_transaction_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
}
TEST_F(SPIDMASequence, InitializeInvalidBackendPreservesExistingState) {
    nx_spi_dma_sequence_backend_t invalid = backend;
    invalid.begin = nullptr;
    sequence.stopping = true;
    sequence.phase = 99U;
    EXPECT_EQ(nx_spi_dma_sequence_initialize(&sequence, &invalid, &hardware),
              NX_ERROR_INVALID);
    EXPECT_EQ(sequence.backend, &backend);
    EXPECT_EQ(sequence.context, &hardware);
    EXPECT_EQ(sequence.phase, 99U);
    EXPECT_TRUE(sequence.stopping);
}
TEST_F(SPIDMASequence, FixedDummyAndDiscardAreExplicitHalfwordCells) {
    const uint16_t dummy = 0xFFFFU;
    uint16_t discard = 0U;
    nx_spi_dma_segment_t fixed{&dummy, &discard, 4U,
                               NX_SPI_MEMORY_TX_FIXED | NX_SPI_MEMORY_RX_FIXED};
    ASSERT_EQ(nx_spi_dma_transaction_prepare(&request, &fixed, 1U, 16U,
                                             NX_DEADLINE_NEVER),
              NX_SUCCESS);
    EXPECT_CALL(hardware.validate, Call())
        .WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_CALL(hardware.begin, Call(false, 16U, 3U));
    ASSERT_EQ(nx_spi_dma_transaction_submit(&endpoint, &request), NX_SUCCESS);
    hardware.complete = true;
    nx_spi_dma_transaction_service(&port);
    EXPECT_EQ(request.base.transferred, 4U);
}
} /* namespace */
