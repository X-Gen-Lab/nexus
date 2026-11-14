/**
 * \file            stm32_spi_dma_test.cpp
 *
 * \brief           Full-duplex DMA preserves CS and buffer loans through bus
 *                  drain
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-10
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f407_dma.h"
nx_stm32_system_t g_nx_stm32_system;
GPIO_TypeDef g_nx_stm32_gpioa_model;
}
#include "nexus/io/native/model.h"
#include <cstring>
#include <gtest/gtest.h>
#include <new>
#include <sys/mman.h>
#include <unistd.h>

static bool s_hold_dma_enable;

/** \brief Model a memory engine whose disable write is not yet acknowledged. */
extern "C" void nx_stm32_dma_model_disable(void* raw, uint32_t mask) {
    auto* stream = static_cast<DMA_Stream_TypeDef*>(raw);
    if (s_hold_dma_enable) {
        mask &= ~static_cast<uint32_t>(DMA_SxCR_EN);
    }
    stream->CR &= ~mask;
}

class Stm32SpiDma : public testing::Test {
  protected:
    SPI_TypeDef spi = {};
    RCC_TypeDef rcc = {};
    GPIO_TypeDef gpio = {};
    DMA_TypeDef dma = {};
    DMA_Stream_TypeDef tx = {};
    DMA_Stream_TypeDef rx = {};
    uint8_t source[4] = {1U, 2U, 3U, 4U};
    uint8_t destination[4] = {};
    nx_dma_memory_region_t regions[2] = {};
    nx_stm32_gpio_state_t cs = {};
    nx_stm32_spi_dma_resources_t dma_resources = {};
    nx_stm32_spi_dma_state_t state = {};
    nx_stm32_spi_dma_endpoint_state_t device = {};
    nx_spi_port_t bus = {&nx_stm32_spi_dma_ops, &state};
    nx_spi_endpoint_t endpoint = {&nx_stm32_spi_dma_endpoint_ops, &device};
    nx_spi_request_t request = {};

    void SetUp() override {
        nx_stm32_spi_cs_model_reset();
        s_hold_dma_enable = false;
        ASSERT_EQ(nx_native_clock_configure(true, 100U), NX_SUCCESS);
        regions[0] = {reinterpret_cast<uintptr_t>(source), sizeof(source),
                      NX_DMA_MEMORY_READ};
        regions[1] = {reinterpret_cast<uintptr_t>(destination),
                      sizeof(destination), NX_DMA_MEMORY_WRITE};
        state.spi.registers = &spi;
        state.spi.rcc = &rcc;
        state.spi.clock_hz = 84000000U;
        dma_resources = {{&dma, &tx, DMA2_Stream3_IRQn, 3U, 3U},
                         {&dma, &rx, DMA2_Stream0_IRQn, 0U, 3U},
                         regions,
                         2U};
        state.resources = &dma_resources;
        cs.registers = &gpio;
        cs.mask = 1U;
        cs.output = true;
        cs.initialized = true;
        gpio.ODR = 1U;
        device.endpoint.port = &state.spi;
        device.endpoint.cs = &cs;
        device.endpoint.cs_mask = 1U;
        device.endpoint.frequency_hz = 1000000U;
        device.dma = &state;
        spi.SR = SPI_SR_TXE;
        ASSERT_EQ(nx_stm32_spi_dma_initialize(&state), NX_SUCCESS);
        nx_request_initialize(&request.base);
        ASSERT_EQ(
            nx_spi_request_prepare(&request, source, destination, 4U, 1000U),
            NX_SUCCESS);
    }

    void CompleteMemory() {
        tx.NDTR = 0U;
        rx.NDTR = 0U;
        dma.LISR = (1U << 27U) | (1U << 5U);
        nx_stm32_spi_dma_irq(&state, false);
        nx_stm32_spi_dma_irq(&state, true);
    }

    void TearDown() override {
        nx_stm32_spi_cs_model_reset();
    }
};

TEST_F(Stm32SpiDma, HardwareBitOrderDoesNotReverseMemoryOrChangeDmaWidth) {
    for (bool lsb : {true, false}) {
        device.endpoint.lsb_first = lsb;
        ASSERT_EQ(NX_SUCCESS, nx_spi_endpoint_submit(&endpoint, &request));
        EXPECT_EQ(lsb, (spi.CR1 & SPI_CR1_LSBFIRST) != 0U);
        EXPECT_EQ(0U, tx.CR & (DMA_SxCR_MSIZE | DMA_SxCR_PSIZE));
        EXPECT_EQ(source[0], 1U);
        EXPECT_EQ(tx.NDTR, sizeof(source));
        CompleteMemory();
        nx_spi_port_service(&bus);
        ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
        ASSERT_EQ(NX_SUCCESS, nx_spi_request_prepare(&request, source,
                                                     destination, 4U, 1000U));
    }
}

TEST_F(Stm32SpiDma, PollingProgressCannotOverwriteTheDmaOwnerExtension) {
    spi.SR = SPI_SR_TXE | SPI_SR_RXNE;
    state.drain_deadline = 77U;
    const auto before = state;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_spi_endpoint_transfer(
                  &endpoint, source, nullptr, 1U, 1000U,
                  reinterpret_cast<size_t*>(&state.drain_deadline)));
    EXPECT_EQ(0, std::memcmp(&before, &state, sizeof(state)));
    EXPECT_EQ(0U, spi.CR1);
    EXPECT_EQ(0U, gpio.BSRR);
}

TEST_F(Stm32SpiDma, PollingReceiveCannotOverwriteTheDmaOwnerExtension) {
    spi.SR = SPI_SR_TXE | SPI_SR_RXNE;
    state.drain_deadline = 77U;
    const auto before = state;
    size_t count = 99U;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_spi_endpoint_transfer(
                  &endpoint, source,
                  reinterpret_cast<uint8_t*>(&state.drain_deadline), 1U, 1000U,
                  &count));
    EXPECT_EQ(0, std::memcmp(&before, &state, sizeof(state)));
    EXPECT_EQ(99U, count);
    EXPECT_EQ(0U, spi.CR1);
    EXPECT_EQ(0U, gpio.BSRR);
}

TEST_F(Stm32SpiDma, RefusedAssertIsAcceptedWithoutStartingEitherEngine) {
    tx.NDTR = 60000U;
    rx.NDTR = 60000U;
    g_nx_spi_cs_fault_odr = &gpio.ODR;
    g_nx_spi_cs_hold_high = 1U;
    ASSERT_EQ(nx_spi_endpoint_submit(&endpoint, &request), NX_SUCCESS);
    EXPECT_EQ((tx.CR | rx.CR) & DMA_SxCR_EN, 0U);
    EXPECT_EQ(spi.CR2 & (SPI_CR2_TXDMAEN | SPI_CR2_RXDMAEN), 0U);
    EXPECT_EQ(spi.CR1 & SPI_CR1_SPE, 0U);
    EXPECT_EQ(state.active, &request);
    nx_spi_port_service(&bus);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
    EXPECT_EQ(state.active, nullptr);
    EXPECT_TRUE(state.spi.fault);
    EXPECT_EQ(nx_spi_port_recover(&bus), NX_SUCCESS);
}

TEST_F(Stm32SpiDma, RefusedReleaseQuarantinesAndRetriesWithoutLosingBorrow) {
    ASSERT_EQ(nx_spi_endpoint_submit(&endpoint, &request), NX_SUCCESS);
    g_nx_spi_cs_fault_odr = &gpio.ODR;
    g_nx_spi_cs_hold_low = 1U;
    CompleteMemory();
    nx_spi_port_service(&bus);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(state.active, &request);
    EXPECT_EQ(state.spi.held_endpoint, &device.endpoint);
    EXPECT_TRUE(state.spi.active);
    EXPECT_EQ((tx.CR | rx.CR) & DMA_SxCR_EN, 0U);
    nx_time_us_t cutoff = state.drain_deadline;
    ASSERT_EQ(nx_native_clock_advance(201U), NX_SUCCESS);
    nx_spi_port_service(&bus);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(state.drain_deadline, cutoff);
    dma.LISR = 1U << 25U;
    nx_stm32_spi_dma_irq(&state, false);
    EXPECT_EQ(state.drain_deadline, cutoff);
    g_nx_spi_cs_hold_low = 0U;
    nx_spi_port_service(&bus);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
    EXPECT_EQ(state.active, nullptr);
    EXPECT_EQ(state.spi.held_endpoint, nullptr);
    EXPECT_FALSE(state.spi.active);
    EXPECT_EQ(gpio.ODR & 1U, 1U);
    gpio.BSRR = 0xA5U;
    nx_spi_port_service(&bus);
    EXPECT_EQ(gpio.BSRR, 0xA5U);
}

TEST_F(Stm32SpiDma, BothDmaCompletionsKeepCsAndBorrowUntilSpiIsIdle) {
    ASSERT_EQ(nx_spi_endpoint_submit(&endpoint, &request), NX_SUCCESS);
    EXPECT_EQ(gpio.BSRR, 1U << 16U);
    EXPECT_NE(tx.CR & DMA_SxCR_EN, 0U);
    EXPECT_NE(rx.CR & DMA_SxCR_EN, 0U);
    spi.SR = SPI_SR_BSY;
    CompleteMemory();
    nx_spi_port_service(&bus);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(gpio.BSRR, 1U << 16U);
    spi.SR = SPI_SR_TXE;
    nx_spi_port_service(&bus);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, 4U);
    EXPECT_EQ(gpio.BSRR, 1U);
    EXPECT_EQ(state.active, nullptr);
    EXPECT_FALSE(state.spi.active);
}

TEST_F(Stm32SpiDma, ExplicitDiscardBorrowsOneCellAndDisablesRxIncrement) {
    regions[1].size = 1U;
    ASSERT_EQ(nx_spi_endpoint_submit_memory(&endpoint, &request,
                                            NX_SPI_MEMORY_RX_FIXED),
              NX_SUCCESS);
    EXPECT_EQ(rx.CR & DMA_SxCR_MINC, 0U);
    EXPECT_NE(tx.CR & DMA_SxCR_MINC, 0U);
    spi.SR = SPI_SR_BSY;
    CompleteMemory();
    nx_spi_port_service(&bus);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(state.active, &request);
    spi.SR = SPI_SR_TXE;
    nx_spi_port_service(&bus);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
}

TEST_F(Stm32SpiDma, ExplicitDummyBorrowsOneCellAndDisablesTxIncrement) {
    regions[0].size = 1U;
    ASSERT_EQ(nx_spi_endpoint_submit_memory(&endpoint, &request,
                                            NX_SPI_MEMORY_TX_FIXED),
              NX_SUCCESS);
    EXPECT_EQ(tx.CR & DMA_SxCR_MINC, 0U);
    EXPECT_NE(rx.CR & DMA_SxCR_MINC, 0U);
}

TEST_F(Stm32SpiDma, FixedMemoryRejectsUnknownOverlapAndMissingDomains) {
    EXPECT_EQ(nx_spi_endpoint_submit_memory(&endpoint, &request, 4U),
              NX_ERROR_INVALID);
    regions[1].size = 1U;
    EXPECT_EQ(nx_spi_endpoint_submit_memory(&endpoint, &request, 0U),
              NX_ERROR_PERMISSION);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    request.rx = source;
    regions[0].permissions |= NX_DMA_MEMORY_WRITE;
    EXPECT_EQ(nx_spi_endpoint_submit_memory(&endpoint, &request,
                                            NX_SPI_MEMORY_RX_FIXED),
              NX_ERROR_INVALID);
    EXPECT_EQ(state.active, nullptr);
    EXPECT_EQ(gpio.BSRR, 0U);
    EXPECT_EQ(tx.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(rx.CR & DMA_SxCR_EN, 0U);
}

TEST_F(Stm32SpiDma, FollowingOrdinaryAdmissionRestoresBothMemoryIncrements) {
    ASSERT_EQ(nx_spi_endpoint_submit_memory(&endpoint, &request,
                                            NX_SPI_MEMORY_RX_FIXED),
              NX_SUCCESS);
    CompleteMemory();
    nx_spi_port_service(&bus);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    ASSERT_EQ(nx_spi_request_prepare(&request, source, destination, 4U, 1000U),
              NX_SUCCESS);
    ASSERT_EQ(nx_spi_endpoint_submit(&endpoint, &request), NX_SUCCESS);
    EXPECT_NE(tx.CR & DMA_SxCR_MINC, 0U);
    EXPECT_NE(rx.CR & DMA_SxCR_MINC, 0U);
    EXPECT_EQ(state.memory_flags, 0U);
}

TEST_F(Stm32SpiDma, CancelledPartialDmaRetainsCsAndNeverInventsWireByteCount) {
    ASSERT_EQ(nx_spi_endpoint_submit(&endpoint, &request), NX_SUCCESS);
    tx.NDTR = 1U;
    rx.NDTR = 2U;
    spi.SR = SPI_SR_BSY;
    ASSERT_EQ(nx_spi_port_cancel(&bus, &request), NX_SUCCESS);
    nx_spi_port_service(&bus);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    EXPECT_EQ(gpio.BSRR, 1U << 16U);
    ASSERT_EQ(nx_native_clock_advance(899U), NX_SUCCESS);
    nx_spi_port_service(&bus);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(nx_spi_port_stop(&bus), NX_ERROR_BUSY);
    spi.SR = SPI_SR_TXE;
    nx_spi_port_service(&bus);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(request.base.transferred, 0U);
    EXPECT_EQ(gpio.BSRR, 1U);
}

TEST_F(Stm32SpiDma, RefusedEngineDisableRetainsBorrowEvenWhenWireIsIdle) {
    ASSERT_EQ(nx_spi_endpoint_submit(&endpoint, &request), NX_SUCCESS);
    tx.NDTR = 1U;
    s_hold_dma_enable = true;
    ASSERT_EQ(nx_spi_port_cancel(&bus, &request), NX_SUCCESS);
    nx_spi_port_service(&bus);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    EXPECT_EQ(gpio.BSRR, 1U << 16U);
    ASSERT_EQ(nx_native_clock_advance(899U), NX_SUCCESS);
    nx_spi_port_service(&bus);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    s_hold_dma_enable = false;
    tx.CR &= ~static_cast<uint32_t>(DMA_SxCR_EN);
    rx.CR &= ~static_cast<uint32_t>(DMA_SxCR_EN);
    nx_spi_port_service(&bus);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
}

TEST_F(Stm32SpiDma, WrongBusIdentityAndInvalidDomainsFailBeforeAdmission) {
    nx_stm32_spi_dma_state_t other = {};
    nx_spi_port_t wrong = {&nx_stm32_spi_dma_ops, &other};
    EXPECT_TRUE(nx_spi_endpoint_on_port(&endpoint, &bus));
    EXPECT_FALSE(nx_spi_endpoint_on_port(&endpoint, &wrong));
    request.rx = source;
    EXPECT_EQ(nx_spi_endpoint_submit(&endpoint, &request), NX_ERROR_PERMISSION);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(state.active, nullptr);
    EXPECT_EQ(gpio.BSRR, 0U);
    EXPECT_EQ(tx.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(rx.CR & DMA_SxCR_EN, 0U);
}

TEST_F(Stm32SpiDma, DmaDomainCannotAuthorizeReadonlyRouteStorageAsReceive) {
    auto* metadata = reinterpret_cast<uint8_t*>(&dma_resources);
    regions[1] = {reinterpret_cast<uintptr_t>(metadata), sizeof(dma_resources),
                  NX_DMA_MEMORY_WRITE};
    request.rx = metadata;
    EXPECT_EQ(NX_ERROR_INVALID, nx_spi_endpoint_submit(&endpoint, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(nullptr, state.active);
    EXPECT_EQ(0U, tx.CR & DMA_SxCR_EN);
    EXPECT_EQ(0U, rx.CR & DMA_SxCR_EN);
}

TEST_F(Stm32SpiDma, StoppedOwnerRejectsBeforeReadingRetiredConfiguration) {
    ASSERT_EXIT(
        {
            if (nx_spi_port_stop(&bus) != NX_SUCCESS)
                _exit(2);
            const long page = sysconf(_SC_PAGESIZE);
            if (page <= 0)
                _exit(3);
            void* mapping =
                mmap(nullptr, static_cast<size_t>(page), PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (mapping == MAP_FAILED)
                _exit(4);
            auto* retained =
                new (mapping) nx_stm32_spi_dma_resources_t(dma_resources);
            state.resources = retained;
            if (nx_stm32_spi_dma_initialize(&state) != NX_SUCCESS)
                _exit(5);
            if (nx_spi_port_stop(&bus) != NX_SUCCESS)
                _exit(6);
            if (mprotect(mapping, static_cast<size_t>(page), PROT_NONE) != 0)
                _exit(7);
            const nx_result_t result =
                nx_spi_endpoint_submit(&endpoint, &request);
            const nx_result_t stopped = nx_spi_port_stop(&bus);
            munmap(mapping, static_cast<size_t>(page));
            _exit(result == NX_ERROR_STATE && stopped == NX_SUCCESS ? 0 : 8);
        },
        testing::ExitedWithCode(0), "");
}

TEST_F(Stm32SpiDma, PollingEndpointsProveSharedControllerWithoutDmaAlias) {
    nx_stm32_spi_state_t other = {};
    const nx_spi_port_t polling_bus = {&nx_stm32_spi_ops, &state.spi};
    const nx_spi_port_t foreign_bus = {&nx_stm32_spi_ops, &other};
    const nx_spi_port_t incompatible = {&nx_stm32_spi_dma_ops, &state.spi};
    const nx_spi_endpoint_t first = {&nx_stm32_spi_endpoint_ops,
                                     &device.endpoint};
    nx_stm32_spi_endpoint_state_t second_state = device.endpoint;
    second_state.cs_mask = 2U;
    const nx_spi_endpoint_t second = {&nx_stm32_spi_endpoint_ops,
                                      &second_state};
    EXPECT_TRUE(nx_spi_endpoint_on_port(&first, &polling_bus));
    EXPECT_TRUE(nx_spi_endpoint_on_port(&second, &polling_bus));
    EXPECT_FALSE(nx_spi_endpoint_on_port(&first, &foreign_bus));
    EXPECT_FALSE(nx_spi_endpoint_on_port(&first, &incompatible));
}

TEST_F(Stm32SpiDma, CompletedDmaHasBoundedDrainEvenWithoutRequestDeadline) {
    request.base.deadline = NX_DEADLINE_NEVER;
    ASSERT_EQ(nx_spi_endpoint_submit(&endpoint, &request), NX_SUCCESS);
    s_hold_dma_enable = true;
    CompleteMemory();
    ASSERT_EQ(nx_native_clock_advance(201U), NX_SUCCESS);
    nx_spi_port_service(&bus);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(state.active, &request);
    EXPECT_EQ(gpio.BSRR, 1U << 16U);
    s_hold_dma_enable = false;
    tx.CR &= ~static_cast<uint32_t>(DMA_SxCR_EN);
    rx.CR &= ~static_cast<uint32_t>(DMA_SxCR_EN);
    nx_spi_port_service(&bus);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
}

/** \brief Cancellation and memory completion construct independent budgets. */
class Stm32SpiDrainBoundary : public Stm32SpiDma,
                              public testing::WithParamInterface<bool> {};

TEST_P(Stm32SpiDrainBoundary, ExhaustedBudgetRetainsBothEnginesAndChipSelect) {
    request.base.deadline = NX_DEADLINE_NEVER;
    ASSERT_EQ(nx_spi_endpoint_submit(&endpoint, &request), NX_SUCCESS);
    ASSERT_EQ(nx_native_clock_advance(UINT64_MAX - 101U), NX_SUCCESS);
    s_hold_dma_enable = true;
    if (GetParam()) {
        CompleteMemory();
    } else {
        ASSERT_EQ(nx_spi_port_cancel(&bus, &request), NX_SUCCESS);
    }
    nx_spi_port_service(&bus);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(state.active, &request);
    EXPECT_EQ(gpio.BSRR, 1U << 16U);
    s_hold_dma_enable = false;
    tx.CR &= ~static_cast<uint32_t>(DMA_SxCR_EN);
    rx.CR &= ~static_cast<uint32_t>(DMA_SxCR_EN);
    nx_spi_port_service(&bus);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result,
              GetParam() ? NX_ERROR_IO : NX_ERROR_CANCELLED);
    EXPECT_EQ(state.active, nullptr);
}

INSTANTIATE_TEST_SUITE_P(DrainOrigins, Stm32SpiDrainBoundary, testing::Bool());

TEST_F(Stm32SpiDma, LateServiceWithRealCompletionProofPreservesSuccess) {
    request.base.deadline = NX_DEADLINE_NEVER;
    ASSERT_EQ(nx_spi_endpoint_submit(&endpoint, &request), NX_SUCCESS);
    CompleteMemory();
    ASSERT_EQ(nx_native_clock_advance(201U), NX_SUCCESS);
    nx_spi_port_service(&bus);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, sizeof(source));
}
