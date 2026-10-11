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
#include <gtest/gtest.h>

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
    nx_stm32_spi_dma_state_t state = {};
    nx_stm32_spi_dma_endpoint_state_t device = {};
    nx_spi_port_t bus = {&nx_stm32_spi_dma_ops, &state};
    nx_spi_endpoint_t endpoint = {&nx_stm32_spi_dma_endpoint_ops, &device};
    nx_spi_request_t request = {};

    void SetUp() override {
        s_hold_dma_enable = false;
        ASSERT_EQ(nx_native_clock_configure(true, 100U), NX_SUCCESS);
        regions[0] = {reinterpret_cast<uintptr_t>(source), sizeof(source),
                      NX_DMA_MEMORY_READ};
        regions[1] = {reinterpret_cast<uintptr_t>(destination),
                      sizeof(destination), NX_DMA_MEMORY_WRITE};
        state.spi.registers = &spi;
        state.spi.rcc = &rcc;
        state.spi.clock_hz = 84000000U;
        state.dma = &dma;
        state.tx = &tx;
        state.rx = &rx;
        state.regions = regions;
        state.region_count = 2U;
        cs.registers = &gpio;
        cs.mask = 1U;
        cs.output = true;
        cs.initialized = true;
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
};

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
