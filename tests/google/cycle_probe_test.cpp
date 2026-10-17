/**
 * \file            cycle_probe_test.cpp
 * \brief           Optional raw cycle probes against real CPU leaf models.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "arch_cortex_model.h"
#include "nexus/io/cycle_probe.h"
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;

namespace {
class Hardware {
  public:
    MOCK_METHOD(uint32_t, Read, (nx_arch_model_register_t));
    MOCK_METHOD(void, Write, (nx_arch_model_register_t, uint32_t));
    MOCK_METHOD(void, Instruction, (nx_arch_model_instruction_t));
    MOCK_METHOD(void, Service, ());
};
Hardware* hardware;
void service(void* context) {
    static_cast<Hardware*>(context)->Service();
}
class CycleProbe : public ::testing::Test {
  protected:
    StrictMock<Hardware> cpu;
    nx_io_cycle_probe_t probe{};
    nx_io_cycle_token_t token{};
    void SetUp() override {
        hardware = &cpu;
    }
    void TearDown() override {
        hardware = nullptr;
    }
    void Counter(uint32_t value) {
        EXPECT_CALL(cpu, Read(NX_ARCH_MODEL_IPSR)).WillOnce(Return(16U));
        EXPECT_CALL(cpu, Read(NX_ARCH_MODEL_DEMCR))
            .WillOnce(Return(UINT32_C(1) << 24U));
        EXPECT_CALL(cpu, Read(NX_ARCH_MODEL_DWT_CONTROL)).WillOnce(Return(1U));
        EXPECT_CALL(cpu, Read(NX_ARCH_MODEL_CYCCNT)).WillOnce(Return(value));
    }
    void Unavailable() {
        EXPECT_CALL(cpu, Read(NX_ARCH_MODEL_IPSR)).WillOnce(Return(16U));
        EXPECT_CALL(cpu, Read(NX_ARCH_MODEL_DEMCR)).WillOnce(Return(0U));
    }
    nx_io_cycle_observation_t
    Snapshot(const nx_io_cycle_probe_t* source = nullptr) {
        nx_io_cycle_observation_t out{};
        InSequence order;
        EXPECT_CALL(cpu, Read(NX_ARCH_MODEL_IPSR)).WillOnce(Return(0U));
        EXPECT_CALL(cpu, Read(NX_ARCH_MODEL_IPSR)).WillOnce(Return(0U));
        EXPECT_CALL(cpu, Read(NX_ARCH_MODEL_CONTROL)).WillOnce(Return(0U));
        EXPECT_CALL(cpu, Read(NX_ARCH_MODEL_PRIMASK)).WillOnce(Return(1U));
        EXPECT_CALL(cpu, Instruction(NX_ARCH_MODEL_CPSID_I));
        EXPECT_CALL(cpu, Instruction(NX_ARCH_MODEL_DSB));
        EXPECT_CALL(cpu, Instruction(NX_ARCH_MODEL_ISB));
        EXPECT_CALL(cpu, Instruction(NX_ARCH_MODEL_DSB));
        EXPECT_CALL(cpu, Write(NX_ARCH_MODEL_PRIMASK, 1U));
        EXPECT_CALL(cpu, Instruction(NX_ARCH_MODEL_ISB));
        EXPECT_EQ(nx_io_cycle_probe_snapshot(
                      source != nullptr ? source : &probe, &out),
                  NX_SUCCESS);
        return out;
    }
};

TEST_F(CycleProbe, RawModuloPairNeverInventsElapsedEpochOrBudgetValidity) {
    InSequence order;
    Counter(100U);
    ASSERT_EQ(nx_io_cycle_probe_begin(&probe, &token), NX_SUCCESS);
    Counter(127U);
    ASSERT_EQ(nx_io_cycle_probe_end(&probe, &token), NX_SUCCESS);
    const auto out = Snapshot();
    EXPECT_EQ(out.attempts, 1U);
    EXPECT_EQ(out.raw_pairs, 1U);
    EXPECT_EQ(out.last_begin, 100U);
    EXPECT_EQ(out.last_end, 127U);
    EXPECT_EQ(out.last_modulo_cycles, 27U);
    EXPECT_TRUE(out.last_available);
    EXPECT_FALSE(out.elapsed_known);
    EXPECT_FALSE(out.active);
}

TEST_F(CycleProbe, NumericalWrapProducesOnlyTheUnsignedResidue) {
    InSequence order;
    Counter(UINT32_C(0xfffffff0));
    ASSERT_EQ(nx_io_cycle_probe_begin(&probe, &token), NX_SUCCESS);
    Counter(16U);
    ASSERT_EQ(nx_io_cycle_probe_end(&probe, &token), NX_SUCCESS);
    const auto out = Snapshot();
    EXPECT_EQ(out.last_modulo_cycles, 32U);
    EXPECT_EQ(out.maximum_modulo_cycles, 32U);
    EXPECT_FALSE(out.elapsed_known);
}

TEST_F(CycleProbe, IdenticalResidueCannotDistinguishOneOrMultipleWraps) {
    InSequence order;
    Counter(100U);
    ASSERT_EQ(nx_io_cycle_probe_begin(&probe, &token), NX_SUCCESS);
    Counter(101U);
    ASSERT_EQ(nx_io_cycle_probe_end(&probe, &token), NX_SUCCESS);
    const auto out = Snapshot();
    EXPECT_EQ(out.last_modulo_cycles, 1U);
    EXPECT_FALSE(out.elapsed_known);
}

TEST_F(CycleProbe, CounterAvailabilityNeverFabricatesContinuityAfterReset) {
    InSequence order;
    Counter(1000U);
    ASSERT_EQ(nx_io_cycle_probe_begin(&probe, &token), NX_SUCCESS);
    Counter(0U);
    ASSERT_EQ(nx_io_cycle_probe_end(&probe, &token), NX_SUCCESS);
    EXPECT_EQ(Snapshot().last_modulo_cycles, UINT32_MAX - 999U);
    EXPECT_FALSE(Snapshot().elapsed_known);
}

TEST_F(CycleProbe, UnavailableCounterIsCountedWithoutAZeroMeasurement) {
    InSequence order;
    Unavailable();
    ASSERT_EQ(nx_io_cycle_probe_begin(&probe, &token), NX_SUCCESS);
    Unavailable();
    ASSERT_EQ(nx_io_cycle_probe_end(&probe, &token), NX_SUCCESS);
    const auto out = Snapshot();
    EXPECT_EQ(out.attempts, 1U);
    EXPECT_EQ(out.raw_pairs, 0U);
    EXPECT_EQ(out.unavailable_pairs, 1U);
    EXPECT_FALSE(out.last_available);
    EXPECT_FALSE(out.elapsed_known);
}

TEST_F(CycleProbe, ACounterLostBetweenEntryAndExitInvalidatesOnlyThePair) {
    InSequence order;
    Counter(9U);
    ASSERT_EQ(nx_io_cycle_probe_begin(&probe, &token), NX_SUCCESS);
    Unavailable();
    ASSERT_EQ(nx_io_cycle_probe_end(&probe, &token), NX_SUCCESS);
    EXPECT_EQ(Snapshot().unavailable_pairs, 1U);
    EXPECT_FALSE(Snapshot().last_available);
}

TEST_F(CycleProbe, NestedDifferentVectorsKeepIndependentTokens) {
    nx_io_cycle_probe_t inner{};
    nx_io_cycle_token_t nested{};
    InSequence order;
    Counter(100U);
    ASSERT_EQ(nx_io_cycle_probe_begin(&probe, &token), NX_SUCCESS);
    Counter(110U);
    ASSERT_EQ(nx_io_cycle_probe_begin(&inner, &nested), NX_SUCCESS);
    Counter(120U);
    ASSERT_EQ(nx_io_cycle_probe_end(&inner, &nested), NX_SUCCESS);
    Counter(150U);
    ASSERT_EQ(nx_io_cycle_probe_end(&probe, &token), NX_SUCCESS);
    EXPECT_EQ(Snapshot().last_modulo_cycles, 50U);
    const auto out = Snapshot(&inner);
    EXPECT_EQ(out.last_modulo_cycles, 10U);
    EXPECT_FALSE(out.elapsed_known);
}

TEST_F(CycleProbe, SameVectorReentryCannotOverwriteTheOuterToken) {
    nx_io_cycle_token_t nested{};
    InSequence order;
    Counter(50U);
    ASSERT_EQ(nx_io_cycle_probe_begin(&probe, &token), NX_SUCCESS);
    EXPECT_EQ(nx_io_cycle_probe_begin(&probe, &nested), NX_ERROR_BUSY);
    EXPECT_EQ(nested.owner, nullptr);
    EXPECT_TRUE(Snapshot().active);
    Counter(80U);
    ASSERT_EQ(nx_io_cycle_probe_end(&probe, &token), NX_SUCCESS);
    EXPECT_EQ(Snapshot().reentries, 1U);
    EXPECT_EQ(Snapshot().last_modulo_cycles, 30U);
}

TEST_F(CycleProbe, AnActiveTokenCannotBeReboundToAnotherProbe) {
    nx_io_cycle_probe_t other{};
    InSequence order;
    Counter(1U);
    ASSERT_EQ(nx_io_cycle_probe_begin(&probe, &token), NX_SUCCESS);
    EXPECT_EQ(nx_io_cycle_probe_begin(&other, &token), NX_ERROR_STATE);
    EXPECT_EQ(nx_io_cycle_probe_end(&other, &token), NX_ERROR_STATE);
    Counter(2U);
    ASSERT_EQ(nx_io_cycle_probe_end(&probe, &token), NX_SUCCESS);
    EXPECT_EQ(nx_io_cycle_probe_end(&probe, &token), NX_ERROR_STATE);
}

TEST_F(CycleProbe, InvalidAndAliasedStorageRejectsBeforeHardwareReads) {
    const auto before = probe;
    EXPECT_EQ(nx_io_cycle_probe_begin(nullptr, &token), NX_ERROR_INVALID);
    EXPECT_EQ(nx_io_cycle_probe_begin(&probe, nullptr), NX_ERROR_INVALID);
    EXPECT_EQ(nx_io_cycle_probe_begin(
                  &probe, reinterpret_cast<nx_io_cycle_token_t*>(&probe)),
              NX_ERROR_INVALID);
    EXPECT_EQ(
        nx_io_cycle_probe_begin(
            reinterpret_cast<nx_io_cycle_probe_t*>(UINTPTR_MAX - 3U), &token),
        NX_ERROR_INVALID);
    EXPECT_EQ(nx_io_cycle_probe_snapshot(&probe, &probe.observation),
              NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&before, &probe, sizeof(probe)), 0);
}

TEST_F(CycleProbe, ObservationCountersSaturateWithoutWrapping) {
    probe.observation.attempts = UINT32_MAX;
    probe.observation.raw_pairs = UINT32_MAX;
    InSequence order;
    Counter(10U);
    ASSERT_EQ(nx_io_cycle_probe_begin(&probe, &token), NX_SUCCESS);
    Counter(12U);
    ASSERT_EQ(nx_io_cycle_probe_end(&probe, &token), NX_SUCCESS);
    EXPECT_EQ(Snapshot().attempts, UINT32_MAX);
    EXPECT_EQ(Snapshot().raw_pairs, UINT32_MAX);
}

TEST_F(CycleProbe, WordServiceStillDispatchesOnceWhenTheCounterIsUnavailable) {
    nx_uart_word_ops_t ops{};
    ops.service = service;
    const nx_uart_word_port_t port{&ops, &cpu};
    InSequence order;
    Unavailable();
    EXPECT_CALL(cpu, Service());
    Unavailable();
    EXPECT_EQ(nx_uart_word_port_service_observed(&port, &probe), NX_SUCCESS);
    EXPECT_EQ(Snapshot().unavailable_pairs, 1U);
}

TEST_F(CycleProbe, SequenceServiceKeepsTheRealQuarantinedBorrow) {
    nx_spi_dma_transaction_port_ops_t ops{};
    ops.service = service;
    const nx_spi_dma_transaction_port_t port{&ops, &cpu};
    nx_request_t request{};
    nx_request_initialize(&request);
    ASSERT_EQ(nx_request_admit(&request, NX_REQUEST_ACTIVE), NX_SUCCESS);
    ASSERT_EQ(nx_request_transition(&request, NX_REQUEST_DRAINING), NX_SUCCESS);
    InSequence order;
    Unavailable();
    EXPECT_CALL(cpu, Service()).WillOnce([&] {
        EXPECT_EQ(nx_request_transition(&request, NX_REQUEST_QUARANTINED),
                  NX_SUCCESS);
    });
    Unavailable();
    EXPECT_EQ(nx_spi_dma_transaction_service_observed(&port, &probe),
              NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request), NX_REQUEST_QUARANTINED);
    EXPECT_FALSE(Snapshot().elapsed_known);
}

TEST_F(CycleProbe, AProbeCannotOverwriteTheImmutableServiceFaceOrMethods) {
    nx_uart_word_ops_t ops{};
    ops.service = service;
    const nx_uart_word_port_t port{&ops, &cpu};
    const auto before = ops;
    EXPECT_CALL(cpu, Service()).Times(2);
    EXPECT_EQ(nx_uart_word_port_service_observed(
                  &port, reinterpret_cast<nx_io_cycle_probe_t*>(&ops)),
              NX_SUCCESS);
    EXPECT_EQ(nx_uart_word_port_service_observed(
                  &port, reinterpret_cast<nx_io_cycle_probe_t*>(
                             const_cast<nx_uart_word_port_t*>(&port))),
              NX_SUCCESS);
    EXPECT_EQ(std::memcmp(&ops, &before, sizeof(ops)), 0);
}

TEST_F(CycleProbe, OptionalNullProbeDoesNotCreateAnotherServiceLayer) {
    nx_uart_word_ops_t word_ops{};
    word_ops.service = service;
    const nx_uart_word_port_t word{&word_ops, &cpu};
    nx_spi_dma_transaction_port_ops_t spi_ops{};
    spi_ops.service = service;
    const nx_spi_dma_transaction_port_t spi{&spi_ops, &cpu};
    EXPECT_CALL(cpu, Service()).Times(2);
    EXPECT_EQ(nx_uart_word_port_service_observed(&word, nullptr), NX_SUCCESS);
    EXPECT_EQ(nx_spi_dma_transaction_service_observed(&spi, nullptr),
              NX_SUCCESS);
}

TEST_F(CycleProbe, MissingMethodsRejectBeforeCounterOrServiceDispatch) {
    nx_uart_word_ops_t ops{};
    const nx_uart_word_port_t port{&ops, &cpu};
    EXPECT_EQ(nx_uart_word_port_service_observed(nullptr, &probe),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_uart_word_port_service_observed(&port, &probe),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(Snapshot().attempts, 0U);
}
} /* namespace */

extern "C" uint32_t nx_arch_model_read(nx_arch_model_register_t reg) {
    return hardware->Read(reg);
}
extern "C" void nx_arch_model_write(nx_arch_model_register_t reg,
                                    uint32_t value) {
    hardware->Write(reg, value);
}
extern "C" void
nx_arch_model_instruction(nx_arch_model_instruction_t instruction) {
    hardware->Instruction(instruction);
}
