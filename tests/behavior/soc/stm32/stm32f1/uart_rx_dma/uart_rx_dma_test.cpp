/**
 * \file            uart_rx_dma_test.cpp
 * \brief           F103 finite RX DMA detach, faults and immutable blocks
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/uart/uart_rx_dma.h"
#include "system/platform.h"
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
    bool active;
    unsigned priority;
};
Vector dma_irq;
Vector uart_irq;
bool refuse_dma_disable;
bool refuse_dma_clear;
bool refuse_dma_address_clear;
bool late_uart_error;
bool sr_sampled;
unsigned dr_reads;

Vector& vector(IRQn_Type irq) {
    EXPECT_TRUE(irq == DMA1_Channel6_IRQn || irq == USART2_IRQn);
    return irq == DMA1_Channel6_IRQn ? dma_irq : uart_irq;
}

void* map_at(uintptr_t address, size_t bytes) {
    return mmap(reinterpret_cast<void*>(address), bytes, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
}

class F103ReceiveDma : public testing::Test {
  protected:
    void* bus = MAP_FAILED;
    void* ram = MAP_FAILED;
    nx_dma_memory_region_t region{sram, sram_bytes, NX_DMA_MEMORY_WRITE};
    nx_stm32f1_uart_rx_dma_t owner{};
    nx_stm32f1_uart_rx_dma_config_t config{};
    const nx_uart_port_t face{&nx_stm32f1_uart_rx_dma_ops, &owner};
    nx_stream_t stream{};
    nx_stream_slot_t slots[2]{};

    void SetUp() override {
        bus = map_at(mmio, mmio_bytes);
        ASSERT_EQ(bus, reinterpret_cast<void*>(mmio));
        ram = map_at(sram, sram_bytes);
        ASSERT_EQ(ram, reinterpret_cast<void*>(sram));

        dma_irq = {false, false, false, 7U};
        uart_irq = {false, false, false, 11U};
        refuse_dma_disable = refuse_dma_clear = late_uart_error = false;
        sr_sampled = refuse_dma_address_clear = false;
        dr_reads = 0U;
        GPIOA->CRL = GPIOA->CRH = 0x44444444U;
        USART2->SR = USART_SR_TC | USART_SR_TXE;
        config.uart = {USART2,      nullptr,  GPIOA,
                       USART2_IRQn, 8000000U, 115200U,
                       nullptr,     0U,       NX_UART_RX_BLOCKS,
                       0U,          3U,       5U,
                       0U,          0U,       0U};
        config.dma = DMA1;
        config.channel = DMA1_Channel6;
        config.irq = DMA1_Channel6_IRQn;
        config.regions = &region;
        config.region_count = 1U;
        slots[0].data = reinterpret_cast<uint8_t*>(sram);
        slots[0].capacity = 4U;
        slots[1].data = reinterpret_cast<uint8_t*>(sram + 16U);
        slots[1].capacity = 8U;
        ASSERT_EQ(NX_SUCCESS, nx_stream_initialize(&stream, slots, 2U));
    }

    void TearDown() override {
        if (bus != MAP_FAILED) {
            EXPECT_EQ(0, munmap(bus, mmio_bytes));
        }
        if (ram != MAP_FAILED) {
            EXPECT_EQ(0, munmap(ram, sram_bytes));
        }
    }

    void Prepare() {
        ASSERT_EQ(NX_SUCCESS, nx_stm32f1_uart_rx_dma_prepare(&owner, &config));
    }

    void Start() {
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&face, &stream));
        EXPECT_EQ(4U, DMA1_Channel6->CNDTR);
        EXPECT_EQ(sram, DMA1_Channel6->CMAR);
        EXPECT_EQ(0U, DMA1_Channel6->CCR & (DMA_CCR_DIR | DMA_CCR_CIRC));
        EXPECT_NE(0U, USART2->CR3 & USART_CR3_DMAR);
        EXPECT_NE(0U, DMA1_Channel6->CCR & DMA_CCR_EN);
        EXPECT_EQ(0U, USART2->CR1 & (USART_CR1_TE | USART_CR1_RXNEIE));
        EXPECT_EQ(0x4U, (GPIOA->CRL >> 8U) & 0xfU);
    }

    void Signal(uint32_t dma, uint32_t uart, unsigned remaining) {
        DMA1_Channel6->CNDTR = remaining;
        auto* const isr = const_cast<volatile uint32_t*>(&DMA1->ISR);
        *isr |= dma == 0U ? 0U : dma | DMA_ISR_GIF6;
        USART2->SR |= uart;
        auto& irq = dma != 0U ? dma_irq : uart_irq;
        irq.active = true;
        nx_stm32f1_uart_rx_dma_irq(&owner);
        irq.active = false;
    }
};
} /* namespace */

extern "C" void nx_dma_model_write(volatile uint32_t* reg, uint32_t value) {
    if (reg == &DMA1->IFCR) {
        constexpr uint32_t lane6 =
            DMA_IFCR_CTCIF6 | DMA_IFCR_CHTIF6 | DMA_IFCR_CTEIF6;
        EXPECT_EQ(0U, value & ~lane6);
        auto* const isr = const_cast<volatile uint32_t*>(&DMA1->ISR);
        if (!refuse_dma_clear)
            *isr &= ~(value | static_cast<uint32_t>(DMA_ISR_GIF6));
        return;
    }
    if (reg == &DMA1_Channel6->CCR && (value & DMA_CCR_EN) == 0U &&
        (DMA1_Channel6->CCR & DMA_CCR_EN) != 0U) {
        EXPECT_EQ(0U, USART2->CR3 & USART_CR3_DMAR);
        if (refuse_dma_disable)
            return;
        if (late_uart_error)
            USART2->SR |= USART_SR_ORE;
    }
    if (reg == &DMA1_Channel6->CMAR && value == 0U && refuse_dma_address_clear)
        return;
    *reg = value;
}

extern "C" void nx_dma_model_bsrr(GPIO_TypeDef* reg, uint32_t value) {
    reg->ODR = (reg->ODR | (value & 0xffffU)) & ~(value >> 16U);
}

extern "C" uint32_t nx_dma_model_status(USART_TypeDef* reg) {
    sr_sampled = true;
    return reg->SR;
}

extern "C" uint8_t nx_dma_model_rx(USART_TypeDef* reg) {
    EXPECT_TRUE(sr_sampled);
    EXPECT_EQ(0U, reg->CR3 & USART_CR3_DMAR);
    EXPECT_EQ(0U, DMA1_Channel6->CCR & DMA_CCR_EN);
    sr_sampled = false;
    ++dr_reads;
    constexpr uint32_t read_to_clear = USART_SR_PE | USART_SR_FE | USART_SR_NE |
                                       USART_SR_ORE | USART_SR_RXNE |
                                       USART_SR_IDLE;
    reg->SR &= ~read_to_clear;
    return static_cast<uint8_t>(reg->DR);
}

extern "C" bool nx_dma_model_irq_enabled(IRQn_Type irq) {
    return vector(irq).enabled;
}
extern "C" bool nx_dma_model_irq_pending(IRQn_Type irq) {
    return vector(irq).pending;
}
extern "C" bool nx_dma_model_irq_active(IRQn_Type irq) {
    return vector(irq).active;
}
extern "C" uint32_t nx_dma_model_irq_priority(IRQn_Type irq) {
    return vector(irq).priority;
}
extern "C" void nx_dma_model_irq_priority_set(IRQn_Type irq, uint32_t value) {
    vector(irq).priority = value;
}
extern "C" void nx_dma_model_irq_enable(IRQn_Type irq) {
    vector(irq).enabled = true;
}
extern "C" void nx_dma_model_irq_disable(IRQn_Type irq) {
    vector(irq).enabled = false;
}
extern "C" void nx_dma_model_irq_clear(IRQn_Type irq) {
    vector(irq).pending = false;
}
extern "C" bool nx_stm32f1_system_live(void) {
    return true;
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    return 30U;
}

TEST_F(F103ReceiveDma, AdmitsOnlyExactWriteableBuffersAndFixedLane) {
    config.channel = DMA1_Channel7;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32f1_uart_rx_dma_prepare(&owner, &config));
    config.channel = DMA1_Channel6;
    dma_irq.pending = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_uart_rx_dma_prepare(&owner, &config));
    dma_irq.pending = false;
    Prepare();
    region.permissions = NX_DMA_MEMORY_READ;
    EXPECT_EQ(NX_ERROR_PERMISSION, nx_uart_port_rx_start(&face, &stream));
    EXPECT_EQ(0U, stream.outstanding);
    EXPECT_EQ(0U, DMA1_Channel6->CCR);
    EXPECT_EQ(0U, USART2->CR3);
}

TEST_F(F103ReceiveDma, FullCountNeverPublishesWhileChannelOrVectorIsLive) {
    Prepare();
    Start();
    refuse_dma_disable = true;
    Signal(DMA_ISR_TCIF6, 0U, 0U);
    EXPECT_FALSE(dma_irq.enabled);
    EXPECT_FALSE(uart_irq.enabled);
    const unsigned reads = dr_reads;
    nx_uart_port_service(&face);
    nx_stream_block_t block{};
    EXPECT_EQ(NX_ERROR_EMPTY, nx_stream_acquire(&stream, &block));
    EXPECT_EQ(reads, dr_reads);
    EXPECT_EQ(NX_STREAM_SLOT_FILLING, slots[0].state);

    refuse_dma_disable = false;
    RCC->APB1ENR &= ~static_cast<uint32_t>(RCC_APB1ENR_USART2EN);
    nx_uart_port_service(&face);
    EXPECT_EQ(NX_ERROR_EMPTY, nx_stream_acquire(&stream, &block));
    EXPECT_EQ(reads, dr_reads);
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    dma_irq.active = true;
    nx_uart_port_service(&face);
    EXPECT_EQ(NX_ERROR_EMPTY, nx_stream_acquire(&stream, &block));
    dma_irq.active = false;
    nx_uart_port_service(&face);
    ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &block));
    EXPECT_EQ(4U, block.length);
    EXPECT_EQ(slots[0].data, block.data);
    EXPECT_EQ(sram + 16U, DMA1_Channel6->CMAR);
}

TEST_F(F103ReceiveDma, IdlePublishesOnlyConfirmedBytesAndPreservesOldLoan) {
    Prepare();
    Start();
    slots[0].data[0] = 0x41U;
    slots[0].data[1] = 0x52U;
    Signal(0U, USART_SR_IDLE, 2U);
    nx_uart_port_service(&face);
    nx_stream_block_t first{};
    ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &first));
    EXPECT_EQ(2U, first.length);
    EXPECT_EQ(NX_STREAM_BOUNDARY_IDLE, first.flags);
    slots[1].data[0] = 0x63U;
    Signal(DMA_ISR_TCIF6, 0U, 0U);
    nx_uart_port_service(&face);
    EXPECT_EQ(0x41U, first.data[0]);
    EXPECT_EQ(0x52U, first.data[1]);
    EXPECT_EQ(0U, DMA1_Channel6->CCR & DMA_CCR_EN);
    EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_rx_stop(&face));
    nx_stream_block_t second{};
    ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &second));
    EXPECT_EQ(8U, second.length);
    EXPECT_EQ(0x63U, second.data[0]);
    EXPECT_EQ(NX_SUCCESS, nx_stream_release(&stream, &second));
    EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_rx_stop(&face));
    EXPECT_EQ(NX_SUCCESS, nx_stream_release(&stream, &first));
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_rx_stop(&face));
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_uart_rx_dma_close(&owner));
    EXPECT_EQ(7U, dma_irq.priority);
    EXPECT_EQ(11U, uart_irq.priority);
    EXPECT_EQ(0U, RCC->AHBENR & RCC_AHBENR_DMA1EN);
}

TEST_F(F103ReceiveDma, LateErrorAndUnretiredFlagsBlockPublication) {
    Prepare();
    Start();
    late_uart_error = true;
    refuse_dma_clear = true;
    Signal(DMA_ISR_TCIF6, USART_SR_IDLE, 0U);
    *const_cast<volatile uint32_t*>(&DMA1->ISR) |= DMA_ISR_TCIF7;
    nx_uart_port_service(&face);
    EXPECT_EQ(NX_STREAM_SLOT_FILLING, slots[0].state);
    EXPECT_EQ(0U, stream.losses);
    late_uart_error = false;
    refuse_dma_clear = false;
    nx_uart_port_service(&face);
    nx_stream_block_t block{};
    EXPECT_EQ(NX_ERROR_EMPTY, nx_stream_acquire(&stream, &block));
    EXPECT_EQ(1U, stream.losses);
    EXPECT_NE(0U, DMA1->ISR & DMA_ISR_TCIF7);
    Signal(DMA_ISR_TCIF6, 0U, 0U);
    nx_uart_port_service(&face);
    ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &block));
    EXPECT_EQ(NX_STREAM_BOUNDARY_LOSS, block.flags);
    EXPECT_EQ(1U, block.lost_blocks);
}

TEST_F(F103ReceiveDma, StopDetachesAnEmptyFillWithoutPublishingAFakeBoundary) {
    Prepare();
    Start();
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_rx_stop(&face));
    EXPECT_EQ(0U, stream.outstanding);
    EXPECT_EQ(0U, DMA1_Channel6->CCR & DMA_CCR_EN);
    EXPECT_EQ(0U, USART2->CR3 & USART_CR3_DMAR);
    EXPECT_FALSE(uart_irq.enabled);
    EXPECT_FALSE(dma_irq.enabled);
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_uart_rx_dma_close(&owner));
}

TEST_F(F103ReceiveDma, TargetWithdrawalFailureRetainsPartialBlockAndCount) {
    Prepare();
    Start();
    refuse_dma_address_clear = true;
    Signal(0U, USART_SR_IDLE, 2U);
    nx_uart_port_service(&face);
    nx_stream_block_t block{};
    EXPECT_EQ(NX_ERROR_EMPTY, nx_stream_acquire(&stream, &block));
    EXPECT_EQ(NX_STREAM_SLOT_FILLING, slots[0].state);
    EXPECT_EQ(sram, DMA1_Channel6->CMAR);
    EXPECT_EQ(2U, DMA1_Channel6->CNDTR);
    EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_rx_stop(&face));

    refuse_dma_address_clear = false;
    nx_uart_port_service(&face);
    ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &block));
    EXPECT_EQ(2U, block.length);
    EXPECT_EQ(NX_STREAM_BOUNDARY_IDLE, block.flags);
    EXPECT_EQ(0U, DMA1_Channel6->CMAR | DMA1_Channel6->CPAR);
    ASSERT_EQ(NX_SUCCESS, nx_stream_release(&stream, &block));
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_rx_stop(&face));
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_uart_rx_dma_close(&owner));
}
