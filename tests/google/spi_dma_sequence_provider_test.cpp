/**
 * \file            spi_dma_sequence_provider_test.cpp
 * \brief           Production DMA width, CS continuity and retained chain drain
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "spi_dma_sequence.h"
#if defined(NX_SPI_TRANSACTION_STM32)
#include "nexus/io/native/model.h"
#include "stm32f407_dma.h"
extern const nx_spi_dma_sequence_backend_t nx_stm32_spi_dma_sequence_backend;
nx_stm32_system_t g_nx_stm32_system;
GPIO_TypeDef g_nx_stm32_gpioa_model;
#define DMA_STATE    nx_stm32_spi_dma_state_t
#define DEVICE_STATE nx_stm32_spi_dma_endpoint_state_t
#define BACKEND      nx_stm32_spi_dma_sequence_backend
#define IRQ          nx_stm32_spi_dma_irq
#define RXCR         DMA2_Stream0->CR
#define TXCR         DMA2_Stream3->CR
#define RXCOUNT      DMA2_Stream0->NDTR
#define TXCOUNT      DMA2_Stream3->NDTR
#define WIRE_CONTROL SPI1->CR1
#define DMA_REQUESTS SPI1->CR2
#define STATUS       SPI1->SR
#define TXEMPTY      SPI_SR_TXE
#define BUSY         SPI_SR_BSY
#define CS           GPIOC->BSRR
#else
#include "gd32f470_spi_dma.h"
#include "gd32f4xx.h"
extern const nx_spi_dma_sequence_backend_t nx_gd32_spi_dma_sequence_backend;
extern uint64_t g_gd32_model_now;
extern bool g_gd32_model_isr;
extern uint32_t g_gd32_model_mask;
#define DMA_STATE    nx_gd32_spi_dma_state_t
#define DEVICE_STATE nx_gd32_spi_dma_endpoint_state_t
#define BACKEND      nx_gd32_spi_dma_sequence_backend
#define IRQ          nx_gd32_spi_dma_irq
#define RXCR         DMA_CH3CTL(DMA1)
#define TXCR         DMA_CH4CTL(DMA1)
#define RXCOUNT      DMA_CH3CNT(DMA1)
#define TXCOUNT      DMA_CH4CNT(DMA1)
#define WIRE_CONTROL SPI_CTL0(SPI4)
#define DMA_REQUESTS SPI_CTL1(SPI4)
#define STATUS       SPI_STAT(SPI4)
#define TXEMPTY      SPI_STAT_TBE
#define BUSY         SPI_STAT_TRANS
#define CS           GPIO_BOP(GPIOC)
#endif
}
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
volatile uint32_t* held;
bool busy_on_detach;
bool busy_on_final_disable;
class SPIDMAProviderSequence : public testing::Test {
  protected:
    DMA_STATE state{};
    DEVICE_STATE child{};
    nx_spi_dma_sequence_state_t sequence{};
    nx_spi_dma_sequence_endpoint_t binding{&sequence, &child};
    nx_spi_dma_transaction_endpoint_t endpoint{
        &nx_spi_dma_sequence_endpoint_ops, &binding};
    nx_spi_dma_transaction_port_t port{&nx_spi_dma_sequence_port_ops,
                                       &sequence};
    nx_spi_dma_transaction_request_t request{};
    uint16_t tx[2]{0x1234U, 0xABCDU};
    uint16_t rx[2]{};
    nx_dma_memory_region_t regions[2]{};
    nx_spi_dma_segment_t segments[2]{{tx, rx, 1U, 0U},
                                     {tx + 1U, rx + 1U, 1U, 0U}};
#if defined(NX_SPI_TRANSACTION_STM32)
    nx_stm32_gpio_state_t cs{GPIOC, 1U, true, true};
#endif
    void* mapping = MAP_FAILED;
    void SetUp() override {
        mapping = mmap(
            (void*)(uintptr_t)0x40000000U, 0x80000U, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, (void*)(uintptr_t)0x40000000U);
        held = nullptr;
        busy_on_detach = false;
        busy_on_final_disable = false;
        regions[0] = {(uintptr_t)tx, sizeof(tx), NX_DMA_MEMORY_READ};
        regions[1] = {(uintptr_t)rx, sizeof(rx), NX_DMA_MEMORY_WRITE};
        state.regions = regions;
        state.region_count = 2U;
#if defined(NX_SPI_TRANSACTION_STM32)
        g_nx_stm32_gpioa_model = {};
        ASSERT_EQ(nx_native_clock_configure(true, 100U), NX_SUCCESS);
        state.spi.registers = SPI1;
        state.spi.rcc = RCC;
        state.spi.clock_hz = 84000000U;
        state.dma = DMA2;
        state.rx = DMA2_Stream0;
        state.tx = DMA2_Stream3;
        child.endpoint = {&state.spi, &cs, 1U, 1000000U, 0U};
        child.dma = &state;
        ASSERT_EQ(nx_stm32_spi_dma_initialize(&state), NX_SUCCESS);
#else
        g_gd32_model_nvic = {};
        g_gd32_model_now = 1U;
        g_gd32_model_isr = false;
        g_gd32_model_mask = 0U;
        ASSERT_EQ(nx_gd32_spi_dma_initialize(&state, &child, GPIOC, 1U,
                                             1000000U, 0U, 6U),
                  NX_SUCCESS);
#endif
        STATUS = TXEMPTY;
        sequence.backend = &BACKEND;
        sequence.context = &state;
        nx_request_initialize(&request.base);
        ASSERT_EQ(nx_spi_dma_transaction_prepare(&request, segments, 2U, 16U,
                                                 NX_DEADLINE_NEVER),
                  NX_SUCCESS);
    }
    void TearDown() override {
        held = nullptr;
        busy_on_detach = false;
        busy_on_final_disable = false;
        if (sequence.active != nullptr) {
            EXPECT_EQ(nx_spi_dma_transaction_cancel(&port, &request),
                      NX_SUCCESS);
            RXCR &= ~1U;
            TXCR &= ~1U;
            STATUS = TXEMPTY;
            nx_spi_dma_transaction_service(&port);
        }
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(munmap(mapping, 0x80000U), 0);
        }
#if defined(NX_SPI_TRANSACTION_STM32)
        EXPECT_EQ(nx_native_clock_configure(false, 0U), NX_SUCCESS);
#endif
    }
    void Complete() {
        RXCOUNT = 0U;
        TXCOUNT = 0U;
#if defined(NX_SPI_TRANSACTION_STM32)
        DMA2->LISR |= 0x20U | (0x20U << 22U);
#else
        DMA_INTF0(DMA1) |= 0x20U << 22U;
        DMA_INTF1(DMA1) |= 0x20U;
#endif
        IRQ(&state, true);
        IRQ(&state, false);
    }
};
TEST_F(SPIDMAProviderSequence, ColdStopCannotTouchAnUnacquiredController) {
    DMA_STATE cold = state;
    cold.initialized = false;
#if !defined(NX_SPI_TRANSACTION_STM32)
    cold.spi.initialized = false;
#endif
    nx_spi_dma_sequence_state_t prebound{};
    prebound.backend = &BACKEND;
    prebound.context = &cold;
    const nx_spi_dma_transaction_port_t cold_port{&nx_spi_dma_sequence_port_ops,
                                                  &prebound};
    RXCR = 0x1FU;
    TXCR = 0x1FU;
#if defined(NX_SPI_TRANSACTION_STM32)
    WIRE_CONTROL = SPI_CR1_SPE | SPI_CR1_MSTR;
    DMA_REQUESTS = SPI_CR2_TXDMAEN | SPI_CR2_RXDMAEN;
    const nx_spi_port_t peer{&nx_stm32_spi_dma_ops, &state};
#else
    WIRE_CONTROL = SPI_CTL0_SPIEN | SPI_CTL0_MSTMOD;
    DMA_REQUESTS = SPI_CTL1_DMATEN | SPI_CTL1_DMAREN;
    const nx_spi_port_t peer{&nx_gd32_spi_dma_ops, &state};
#endif
    uint32_t control = WIRE_CONTROL;
    uint32_t requests = DMA_REQUESTS;
    EXPECT_EQ(nx_spi_dma_transaction_stop(&cold_port), NX_SUCCESS);
    EXPECT_EQ(RXCR, 0x1FU);
    EXPECT_EQ(TXCR, 0x1FU);
    EXPECT_EQ(WIRE_CONTROL, control);
    EXPECT_EQ(DMA_REQUESTS, requests);
    EXPECT_TRUE(state.initialized);
    EXPECT_EQ(nx_spi_port_stop(&peer), NX_SUCCESS);
}
TEST_F(SPIDMAProviderSequence, HardwareHalfwordWidthsAndCsSpanBothSegments) {
    ASSERT_EQ(nx_spi_dma_transaction_submit(&endpoint, &request), NX_SUCCESS);
    EXPECT_EQ(RXCR & (3U << 11U), 1U << 11U);
    EXPECT_EQ(RXCR & (3U << 13U), 1U << 13U);
    EXPECT_EQ(TXCR & (3U << 11U), 1U << 11U);
    EXPECT_EQ(RXCOUNT, 1U);
    EXPECT_EQ(CS, 1U << 16U);
    Complete();
    STATUS = TXEMPTY | BUSY;
    nx_spi_dma_transaction_service(&port);
    EXPECT_EQ(sequence.phase, 0U);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    STATUS = TXEMPTY;
    nx_spi_dma_transaction_service(&port);
    EXPECT_EQ(sequence.phase, 1U);
    EXPECT_EQ(CS, 1U << 16U);
    EXPECT_EQ(RXCOUNT, 1U);
    Complete();
    nx_spi_dma_transaction_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, 2U);
    EXPECT_EQ(CS, 1U);
    EXPECT_FALSE(state.spi.active);
}
TEST_F(SPIDMAProviderSequence, FutureDmaDomainRejectsBeforeFirstCsOrBorrow) {
    uint16_t foreign = 0U;
    segments[1].rx = &foreign;
    EXPECT_NE(nx_spi_dma_transaction_submit(&endpoint, &request), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(sequence.active, nullptr);
    EXPECT_EQ(state.active, nullptr);
    EXPECT_EQ(CS, 0U);
    EXPECT_EQ((RXCR | TXCR) & 1U, 0U);
}
TEST_F(SPIDMAProviderSequence, DmaDomainCannotAuthorizeProviderStateAsPayload) {
    nx_dma_memory_region_t broad[3]{
        regions[0],
        regions[1],
        {(uintptr_t)&state, sizeof(state), NX_DMA_MEMORY_WRITE}};
    state.regions = broad;
    state.region_count = 3U;
    segments[0].rx = &state;
    ASSERT_EQ(nx_spi_dma_transaction_prepare(&request, segments, 2U, 16U,
                                             NX_DEADLINE_NEVER),
              NX_SUCCESS);
    EXPECT_EQ(nx_spi_dma_transaction_submit(&endpoint, &request),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(CS, 0U);
}
TEST_F(SPIDMAProviderSequence,
       WithdrawalRechecksWireBusyBeforePhaseSettlement) {
    nx_time_us_t deadline = nx_time_now_us() + 1000U;
    ASSERT_EQ(
        nx_spi_dma_transaction_prepare(&request, segments, 2U, 16U, deadline),
        NX_SUCCESS);
    ASSERT_EQ(nx_spi_dma_transaction_submit(&endpoint, &request), NX_SUCCESS);
#if defined(NX_SPI_TRANSACTION_STM32)
    ASSERT_EQ(nx_native_clock_advance(2000U), NX_SUCCESS);
#else
    g_gd32_model_now += 2000U;
#endif
    STATUS = TXEMPTY;
    busy_on_detach = true;
    nx_spi_dma_transaction_service(&port);
    EXPECT_NE(nx_request_state(&sequence.phase_request.base),
              NX_REQUEST_SETTLED);
    EXPECT_EQ(state.active, &sequence.phase_request);
    EXPECT_EQ(CS, 1U << 16U);
    busy_on_detach = false;
    STATUS = TXEMPTY;
    nx_spi_dma_transaction_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_TIMEOUT);
    EXPECT_EQ(CS, 1U);
}
TEST_F(SPIDMAProviderSequence, FinalEngineDisablePrecedesWireIdleObservation) {
    ASSERT_EQ(nx_spi_dma_transaction_submit(&endpoint, &request), NX_SUCCESS);
    held = &RXCR;
    ASSERT_EQ(nx_spi_dma_transaction_cancel(&port, &request), NX_SUCCESS);
    ASSERT_NE(RXCR & 1U, 0U);
    held = nullptr;
    STATUS = TXEMPTY;
    busy_on_final_disable = true;
    nx_spi_dma_transaction_service(&port);
    EXPECT_NE(nx_request_state(&sequence.phase_request.base),
              NX_REQUEST_SETTLED);
    EXPECT_EQ(state.active, &sequence.phase_request);
    EXPECT_EQ(CS, 1U << 16U);
    EXPECT_FALSE(busy_on_final_disable);
    STATUS = TXEMPTY;
    nx_spi_dma_transaction_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(CS, 1U);
}
TEST_F(SPIDMAProviderSequence, CancelAfterFirstPhaseRetainsCurrentDmaAndCs) {
    ASSERT_EQ(nx_spi_dma_transaction_submit(&endpoint, &request), NX_SUCCESS);
    Complete();
    nx_spi_dma_transaction_service(&port);
    ASSERT_EQ(sequence.phase, 1U);
    held = &RXCR;
    ASSERT_EQ(nx_spi_dma_transaction_cancel(&port, &request), NX_SUCCESS);
    nx_spi_dma_transaction_service(&port);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(CS, 1U << 16U);
    EXPECT_EQ(sequence.active, &request);
    held = nullptr;
    RXCR &= ~1U;
    nx_spi_dma_transaction_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(request.base.transferred, 1U);
    EXPECT_EQ(CS, 1U);
}
} /* namespace */
/** \brief W1C applies only the selected actual flag bank. */
extern "C" void nx_spi_multi_dma_ack(volatile uint32_t* clear, uint32_t mask) {
    *clear = mask;
    *(clear - 2) &= ~mask;
}
/** \brief One explicit injected engine refuses source withdrawal. */
extern "C" void nx_spi_multi_dma_disable(volatile uint32_t* control,
                                         uint32_t mask) {
    *control &= ~(control == held ? mask & ~1U : mask);
    if (busy_on_detach) {
        STATUS |= BUSY;
    }
}
/** \brief Apply exact selected reset effects for legacy recovery code. */
extern "C" void nx_spi_multi_reset(volatile uint32_t* reset, uint32_t mask,
                                   bool active) {
    if (active) {
        *reset |= mask;
    } else {
        *reset &= ~mask;
    }
}
#if !defined(NX_SPI_TRANSACTION_STM32)
/** \brief SPI-only fixture selects no UART publisher. */
extern "C" void USART0_IRQHandler(void) {
}
#endif

/** \brief A final bus beat can make SPI busy as DMA EN finally clears. */
extern "C" uint32_t nx_spi_dma_sequence_model_read(volatile uint32_t* control) {
    if (busy_on_final_disable && control == &RXCR) {
        *control &= ~1U;
        STATUS |= BUSY;
        busy_on_final_disable = false;
    }
    return *control;
}
