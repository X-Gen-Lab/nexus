/**
 * \file            uart_dma_test.cpp
 * \brief           F103 DMA request borrowing and physical USART drain.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "system/platform.h"
#include "uart_dma.h"
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
constexpr uintptr_t mmio = 0x40000000U;
constexpr uintptr_t sram = 0x20000000U;
constexpr size_t mmio_bytes = 0x24000U;
constexpr size_t sram_bytes = 0x2000U;
struct Vector {
    bool enabled;
    bool pending;
    unsigned priority;
};
Vector dma_irq;
Vector uart_irq;
bool block_lane_disable;
bool block_tc_disable;
bool late_transfer_error;
nx_time_us_t now;

Vector& vector(IRQn_Type irq) {
    EXPECT_TRUE(irq == DMA1_Channel7_IRQn || irq == USART2_IRQn);
    return irq == DMA1_Channel7_IRQn ? dma_irq : uart_irq;
}

void* map_at(uintptr_t address, size_t bytes) {
    return mmap(reinterpret_cast<void*>(address), bytes, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
}

class F1UartDma : public testing::Test {
  protected:
    void* bus = MAP_FAILED;
    void* ram = MAP_FAILED;
    const nx_dma_memory_region_t region{sram, sram_bytes, NX_DMA_MEMORY_READ};
    nx_stm32f1_uart_dma_t owner{};
    nx_stm32f1_uart_dma_config_t config{};
    const nx_uart_port_t face{&nx_stm32f1_uart_dma_ops, &owner};
    nx_uart_tx_request_t request{};
    uint8_t receive_buffer[4]{};
    const uint8_t* packet{reinterpret_cast<const uint8_t*>(sram)};

    void SetUp() override {
        bus = map_at(mmio, mmio_bytes);
        ASSERT_EQ(bus, reinterpret_cast<void*>(mmio));
        ram = map_at(sram, sram_bytes);
        ASSERT_EQ(ram, reinterpret_cast<void*>(sram));
        dma_irq = {false, false, 7U};
        uart_irq = {false, false, 11U};
        block_lane_disable = block_tc_disable = late_transfer_error = false;
        now = 10U;
        GPIOA->CRL = GPIOA->CRH = UINT32_C(0x44444444);
        USART2->SR = USART_SR_TC | USART_SR_TXE;
        config.uart = {USART2,
                       GPIOA,
                       GPIOA,
                       USART2_IRQn,
                       8000000U,
                       115200U,
                       receive_buffer,
                       sizeof(receive_buffer),
                       NX_UART_RX_BYTES,
                       2U,
                       3U,
                       5U};
        config.dma = DMA1;
        config.channel = DMA1_Channel7;
        config.irq = DMA1_Channel7_IRQn;
        config.regions = &region;
        config.region_count = 1U;
        nx_request_initialize(&request.base);
    }

    void TearDown() override {
        if (bus != MAP_FAILED) {
            EXPECT_EQ(munmap(bus, mmio_bytes), 0);
        }
        if (ram != MAP_FAILED) {
            EXPECT_EQ(munmap(ram, sram_bytes), 0);
        }
    }

    void prepare() {
        ASSERT_EQ(nx_stm32f1_uart_dma_prepare(&owner, &config), NX_SUCCESS);
    }

    void submit() {
        ASSERT_EQ(nx_uart_tx_prepare(&request, packet, 4U, 1000U), NX_SUCCESS);
        ASSERT_EQ(nx_uart_port_submit(&face, &request), NX_SUCCESS);
    }

    void signal_dma(uint32_t flags, uint32_t remaining) {
        DMA1_Channel7->CNDTR = remaining;
        *const_cast<volatile uint32_t*>(&DMA1->ISR) |= flags | DMA_ISR_GIF7;
        nx_stm32f1_uart_dma_irq(&owner);
    }

    void signal_wire() {
        USART2->SR |= USART_SR_TC;
        nx_stm32f1_uart_dma_usart_irq(&owner);
    }
};

TEST_F(F1UartDma, DmaCompletionAndWireTcAreIndependent) {
    prepare();
    submit();
    EXPECT_EQ(DMA1_Channel7->CPAR,
              static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&USART2->DR)));
    EXPECT_EQ(DMA1_Channel7->CMAR, sram);
    EXPECT_EQ(DMA1_Channel7->CNDTR, 4U);
    EXPECT_NE(DMA1_Channel7->CCR & DMA_CCR_EN, 0U);
    EXPECT_NE(USART2->CR3 & USART_CR3_DMAT, 0U);
    EXPECT_EQ(USART2->SR & USART_SR_TC, 0U);
    signal_dma(DMA_ISR_TCIF7, 0U);
    EXPECT_EQ(DMA1_Channel7->CCR & DMA_CCR_EN, 0U);
    EXPECT_EQ(USART2->CR3 & USART_CR3_DMAT, 0U);
    nx_uart_port_service(&face);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    signal_wire();
    now = 2000U;
    nx_uart_port_service(&face);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, 4U);
    ASSERT_EQ(nx_stm32f1_uart_dma_close(&owner), NX_SUCCESS);
    EXPECT_EQ(dma_irq.priority, 7U);
    EXPECT_EQ(RCC->AHBENR & RCC_AHBENR_DMA1EN, 0U);
}

TEST_F(F1UartDma, CancellationKeepsLoanWhileChannelCannotStop) {
    prepare();
    submit();
    DMA1_Channel7->CNDTR = 3U;
    block_lane_disable = true;
    ASSERT_EQ(nx_uart_port_cancel(&face, &request), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    EXPECT_EQ(USART2->CR3 & USART_CR3_DMAT, 0U);
    EXPECT_FALSE(dma_irq.enabled);
    signal_wire();
    now = 2000U;
    nx_uart_port_service(&face);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    block_lane_disable = false;
    nx_uart_port_service(&face);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(request.base.transferred, 0U);
}

TEST_F(F1UartDma, LateTransferErrorSurvivesCompletionAck) {
    prepare();
    submit();
    late_transfer_error = true;
    *const_cast<volatile uint32_t*>(&DMA1->ISR) |= DMA_ISR_TCIF6 | DMA_ISR_GIF6;
    signal_dma(DMA_ISR_TCIF7, 0U);
    signal_wire();
    nx_uart_port_service(&face);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(DMA1->ISR & DMA_ISR_TCIF6, DMA_ISR_TCIF6);
}

TEST_F(F1UartDma, FailedTcSourceIsolationMasksIrqAndKeepsTheLoan) {
    prepare();
    submit();
    signal_dma(DMA_ISR_TCIF7, 0U);
    block_tc_disable = true;
    signal_wire();
    EXPECT_FALSE(uart_irq.enabled);
    nx_uart_port_service(&face);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    block_tc_disable = false;
    nx_uart_port_service(&face);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
}

TEST_F(F1UartDma, OldUnqualifiedIrqCannotCompleteNextLoan) {
    prepare();
    submit();
    ASSERT_EQ(nx_uart_port_cancel(&face, &request), NX_SUCCESS);
    nx_uart_port_service(&face);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    dma_irq.pending = true;
    *const_cast<volatile uint32_t*>(&DMA1->ISR) |= DMA_ISR_TCIF7 | DMA_ISR_GIF7;
    submit();
    EXPECT_FALSE(dma_irq.pending);
    EXPECT_EQ(DMA1->ISR & DMA_ISR_TCIF7, 0U);
    nx_stm32f1_uart_dma_irq(&owner);
    nx_uart_port_service(&face);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    signal_dma(DMA_ISR_TCIF7, 0U);
    signal_wire();
    nx_uart_port_service(&face);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
}

TEST_F(F1UartDma, DeadlineRemainsTimeoutWhileWaitingForWireThenQuarantined) {
    prepare();
    submit();
    signal_dma(DMA_ISR_TCIF7, 0U);
    now = 2000U;
    nx_uart_port_service(&face);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    signal_wire();
    nx_uart_port_service(&face);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_TIMEOUT);
}

TEST_F(F1UartDma, IrqAfterColdCloseIsHarmless) {
    prepare();
    ASSERT_EQ(nx_stm32f1_uart_dma_close(&owner), NX_SUCCESS);
    nx_stm32f1_uart_dma_irq(&owner);
    nx_stm32f1_uart_dma_usart_irq(&owner);
}

TEST_F(F1UartDma, ActiveDmaSharesTheIrqEventRingWithoutAliasingPayload) {
    nx_uart_rx_event_t storage[2]{};
    config.uart.profile = NX_UART_RX_EVENTS;
    config.uart.rx_storage = storage;
    config.uart.rx_capacity = 2U;
    prepare();
    submit();
    USART2->DR = 0x8AU;
    USART2->SR = USART_SR_RXNE | USART_SR_NE;
    nx_stm32f1_uart_dma_usart_irq(&owner);
    nx_uart_rx_event_t event{};
    size_t count = 0U;
    EXPECT_EQ(
        nx_uart_port_read_events(
            &face, reinterpret_cast<nx_uart_rx_event_t*>(sram), 1U, &count),
        NX_ERROR_INVALID);
    EXPECT_EQ(nx_uart_port_read_events(&face, &event, 1U, &count), NX_SUCCESS);
    EXPECT_EQ(count, 1U);
    EXPECT_EQ(event.byte, 0x8AU);
    EXPECT_EQ(event.flags, NX_UART_EVENT_NOISE);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
}

TEST_F(F1UartDma, RejectsUnreachablePayloadAndKeepsIrqRx) {
    prepare();
    const uint8_t data = 0x2AU;
    ASSERT_EQ(nx_uart_tx_prepare(&request, &data, 1U, 1000U), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_submit(&face, &request), NX_ERROR_PERMISSION);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(DMA1_Channel7->CCR, 0U);
    USART2->DR = data;
    USART2->SR = USART_SR_RXNE;
    nx_stm32f1_uart_dma_usart_irq(&owner);
    uint8_t byte = 0U;
    size_t count = 0U;
    EXPECT_EQ(nx_uart_port_read_bytes(&face, &byte, 1U, &count), NX_SUCCESS);
    EXPECT_EQ(byte, data);
    EXPECT_EQ(count, 1U);
    ASSERT_EQ(nx_uart_port_stop(&face), NX_SUCCESS);
    EXPECT_NE(RCC->AHBENR & RCC_AHBENR_DMA1EN, 0U);
}
} /* namespace */

extern "C" {
bool nx_stm32f1_system_live(void) {
    return true;
}
nx_time_us_t nx_time_now_us(void) {
    return now;
}
void nx_dma_model_write(volatile uint32_t* reg, uint32_t value) {
    if (reg == &DMA1->IFCR) {
        EXPECT_EQ(value & DMA_IFCR_CGIF7, 0U);
        *const_cast<volatile uint32_t*>(&DMA1->ISR) &= ~value;
        if ((DMA1->ISR & (DMA_ISR_TCIF7 | DMA_ISR_TEIF7 | DMA_ISR_HTIF7)) ==
            0U) {
            *const_cast<volatile uint32_t*>(&DMA1->ISR) &=
                ~static_cast<uint32_t>(DMA_ISR_GIF7);
        }
    }
    if (reg == &DMA1_Channel7->CCR && (DMA1_Channel7->CCR & DMA_CCR_EN) != 0U &&
        (value & DMA_CCR_EN) == 0U) {
        if (block_lane_disable) {
            return;
        }
        if (late_transfer_error) {
            late_transfer_error = false;
            *const_cast<volatile uint32_t*>(&DMA1->ISR) |=
                DMA_ISR_TEIF7 | DMA_ISR_GIF7;
        }
    }
    if (reg != &USART2->CR1 || !block_tc_disable ||
        (value & USART_CR1_TCIE) != 0U) {
        *reg = value;
    }
}
void nx_dma_model_bsrr(GPIO_TypeDef* reg, uint32_t value) {
    reg->ODR = (reg->ODR & ~(value >> 16U)) | (value & 0xFFFFU);
}
uint8_t nx_dma_model_rx(USART_TypeDef* reg) {
    reg->SR &= ~static_cast<uint32_t>(USART_SR_RXNE | USART_SR_ORE |
                                      USART_SR_NE | USART_SR_FE | USART_SR_PE);
    return static_cast<uint8_t>(reg->DR);
}
void nx_dma_model_tc_clear(USART_TypeDef* reg) {
    reg->SR &= ~static_cast<uint32_t>(USART_SR_TC);
}
bool nx_dma_model_irq_enabled(IRQn_Type irq) {
    return vector(irq).enabled;
}
bool nx_dma_model_irq_pending(IRQn_Type irq) {
    return vector(irq).pending;
}
bool nx_dma_model_irq_active(IRQn_Type irq) {
    (void)irq;
    return false;
}
uint32_t nx_dma_model_irq_priority(IRQn_Type irq) {
    return vector(irq).priority;
}
void nx_dma_model_irq_priority_set(IRQn_Type irq, uint32_t value) {
    vector(irq).priority = value;
}
void nx_dma_model_irq_enable(IRQn_Type irq) {
    vector(irq).enabled = true;
}
void nx_dma_model_irq_disable(IRQn_Type irq) {
    vector(irq).enabled = false;
}
void nx_dma_model_irq_clear(IRQn_Type irq) {
    vector(irq).pending = false;
}
}
