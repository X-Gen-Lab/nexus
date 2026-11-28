/**
 * \file            spi_dma_test.cpp
 * \brief           F103 SPI2 DMA, wire, vector and CS withdrawal evidence
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/spi/spi_dma.h"
#include "system/platform.h"
#if defined(NX_STM32F1_SPI_DMA_SEQUENCE)
#include "drivers/spi/spi_dma_sequence_backend.h"
#endif
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
constexpr uintptr_t mmio = 0x40000000U;
constexpr uintptr_t sram = 0x20000000U;
constexpr size_t mmio_bytes = 0x24000U;
constexpr size_t sram_bytes = 0x2000U;
constexpr uint32_t cs = 1U << 12U;
struct Vector {
    bool enabled;
    bool pending;
    bool active;
    uint32_t priority;
};
Vector rx_irq;
Vector tx_irq;
Vector spi_irq;
nx_time_us_t now;
bool refuse_rx_stop;
bool refuse_cs_release;
bool late_error;
unsigned selections;
unsigned releases;
uint8_t receive_bits;

Vector& vector(IRQn_Type irq) {
    EXPECT_TRUE(irq == DMA1_Channel4_IRQn || irq == DMA1_Channel5_IRQn ||
                irq == SPI2_IRQn);
    return irq == DMA1_Channel4_IRQn   ? rx_irq
           : irq == DMA1_Channel5_IRQn ? tx_irq
                                       : spi_irq;
}

void* map_at(uintptr_t address, size_t bytes) {
    return mmap(reinterpret_cast<void*>(address), bytes, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
}

class F103SpiDma : public testing::Test {
  protected:
    void* bus = MAP_FAILED;
    void* ram = MAP_FAILED;
    nx_dma_memory_region_t region{sram, sram_bytes,
                                  NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE};
    nx_stm32f1_spi_dma_config_t config{
        {SPI2, GPIOB, 8000000U, 13U, 14U, 15U, 12U},
        DMA1,
        DMA1_Channel4,
        DMA1_Channel5,
        &region,
        1U,
        5U};
    nx_stm32f1_spi_dma_t owner{};
    nx_stm32f1_spi_dma_endpoint_t endpoint{&owner, GPIOB, 1000000U,
                                           12U,    0U,    false};
    const nx_spi_port_t port{&nx_stm32f1_spi_dma_ops, &owner};
    const nx_spi_endpoint_t face{&nx_stm32f1_spi_dma_endpoint_ops, &endpoint};
    nx_spi_request_t request{};
    uint8_t* const tx = reinterpret_cast<uint8_t*>(sram);
    uint8_t* const rx = reinterpret_cast<uint8_t*>(sram + 32U);

    void SetUp() override {
        bus = map_at(mmio, mmio_bytes);
        ASSERT_EQ(bus, reinterpret_cast<void*>(mmio));
        ram = map_at(sram, sram_bytes);
        ASSERT_EQ(ram, reinterpret_cast<void*>(sram));
        now = 10U;
        rx_irq = {false, false, false, 7U};
        tx_irq = {false, false, false, 11U};
        spi_irq = {false, false, false, 9U};
        refuse_rx_stop = refuse_cs_release = late_error = false;
        selections = releases = receive_bits = 0U;
        GPIOB->CRL = GPIOB->CRH = 0x44444444U;
        SPI2->SR = SPI_SR_TXE;
        nx_request_initialize(&request.base);
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
        ASSERT_EQ(NX_SUCCESS, nx_stm32f1_spi_dma_prepare(&owner, &config));
        EXPECT_TRUE(nx_spi_endpoint_on_port(&face, &port));
        EXPECT_NE(0U, GPIOB->ODR & cs);
        selections = releases = 0U;
    }

    void Start(nx_time_us_t deadline = 1000U) {
        ASSERT_EQ(NX_SUCCESS,
                  nx_spi_request_prepare(&request, tx, rx, 4U, deadline));
        ASSERT_EQ(NX_SUCCESS, nx_spi_endpoint_submit(&face, &request));
        EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
        EXPECT_EQ(0U, GPIOB->ODR & cs);
        EXPECT_EQ(1U, selections);
        EXPECT_EQ(4U, DMA1_Channel4->CNDTR);
        EXPECT_EQ(4U, DMA1_Channel5->CNDTR);
        EXPECT_EQ(sram + 32U, DMA1_Channel4->CMAR);
        EXPECT_EQ(sram, DMA1_Channel5->CMAR);
        EXPECT_EQ(0U, DMA1_Channel4->CCR & DMA_CCR_DIR);
        EXPECT_NE(0U, DMA1_Channel5->CCR & DMA_CCR_DIR);
        EXPECT_EQ(0U, SPI2->CR1 & (SPI_CR1_CRCEN | SPI_CR1_DFF));
    }

    void Signal(uint32_t flags) {
        if ((flags & DMA_ISR_TCIF4) != 0U)
            DMA1_Channel4->CNDTR = 0U;
        if ((flags & DMA_ISR_TCIF5) != 0U)
            DMA1_Channel5->CNDTR = 0U;
        *const_cast<volatile uint32_t*>(&DMA1->ISR) |= flags;
        auto& irq = (flags & DMA_ISR_TCIF4) != 0U ? rx_irq : tx_irq;
        irq.active = true;
        nx_stm32f1_spi_dma_irq(&owner);
        irq.active = false;
    }

    void Settled(nx_result_t expected, size_t bytes) {
        nx_result_t result{};
        size_t transferred{};
        ASSERT_EQ(NX_SUCCESS,
                  nx_request_result(&request.base, &result, &transferred));
        EXPECT_EQ(expected, result);
        EXPECT_EQ(bytes, transferred);
    }
};
} /* namespace */

extern "C" void nx_spi_model_write(volatile uint32_t* reg, uint32_t value) {
    if (reg == &DMA1->IFCR) {
        constexpr uint32_t selected = DMA_IFCR_CTCIF4 | DMA_IFCR_CHTIF4 |
                                      DMA_IFCR_CTEIF4 | DMA_IFCR_CTCIF5 |
                                      DMA_IFCR_CHTIF5 | DMA_IFCR_CTEIF5;
        EXPECT_EQ(0U, value & ~selected);
        *const_cast<volatile uint32_t*>(&DMA1->ISR) &= ~value;
        return;
    }
    if (reg == &DMA1_Channel4->CCR && (DMA1_Channel4->CCR & DMA_CCR_EN) != 0U &&
        (value & DMA_CCR_EN) == 0U) {
        EXPECT_EQ(0U, SPI2->CR2 & SPI_CR2_RXDMAEN);
        if (refuse_rx_stop)
            return;
        if (late_error)
            *const_cast<volatile uint32_t*>(&DMA1->ISR) |= DMA_ISR_TEIF4;
    }
    if (reg == &SPI2->CR2 && (value & SPI_CR2_TXDMAEN) != 0U) {
        EXPECT_EQ(0U, GPIOB->ODR & cs);
        EXPECT_NE(0U, value & SPI_CR2_RXDMAEN);
        EXPECT_NE(0U, SPI2->CR1 & SPI_CR1_SPE);
        EXPECT_NE(0U, DMA1_Channel4->CCR & DMA_CCR_EN);
        EXPECT_NE(0U, DMA1_Channel5->CCR & DMA_CCR_EN);
    }
    if (reg == &RCC->APB1RSTR && (value & RCC_APB1RSTR_SPI2RST) != 0U) {
        SPI2->CR1 = SPI2->CR2 = SPI2->I2SCFGR = 0U;
        SPI2->SR = SPI_SR_TXE;
    }
    *reg = value;
}

extern "C" void nx_spi_model_bsrr(GPIO_TypeDef* reg, uint32_t value) {
    if ((value & (cs << 16U)) != 0U)
        ++selections;
    if ((value & cs) != 0U && (reg->ODR & cs) == 0U) {
        if (refuse_cs_release)
            return;
        EXPECT_EQ(0U, SPI2->SR & SPI_SR_BSY);
        EXPECT_EQ(0U, (DMA1_Channel4->CCR | DMA1_Channel5->CCR) & DMA_CCR_EN);
        ++releases;
    }
    reg->ODR = (reg->ODR | (value & 0xffffU)) & ~(value >> 16U);
}

extern "C" uint32_t nx_spi_model_status(SPI_TypeDef* reg) {
    return reg->SR;
}
extern "C" uint16_t nx_spi_model_receive(SPI_TypeDef* reg, uint8_t bits) {
    receive_bits = bits;
    reg->SR &= ~static_cast<uint32_t>(SPI_SR_RXNE | SPI_SR_OVR);
    return static_cast<uint16_t>(reg->DR);
}
extern "C" void nx_spi_model_send(SPI_TypeDef*, uint16_t, uint8_t) {
    ADD_FAILURE();
}
extern "C" bool nx_spi_model_irq_enabled(IRQn_Type irq) {
    return vector(irq).enabled;
}
extern "C" bool nx_spi_model_irq_pending(IRQn_Type irq) {
    return vector(irq).pending;
}
extern "C" bool nx_spi_model_irq_active(IRQn_Type irq) {
    return vector(irq).active;
}
extern "C" uint32_t nx_spi_model_irq_priority(IRQn_Type irq) {
    return vector(irq).priority;
}
extern "C" void nx_spi_model_irq_priority_set(IRQn_Type irq, uint32_t value) {
    vector(irq).priority = value;
}
extern "C" void nx_spi_model_irq_enable(IRQn_Type irq) {
    vector(irq).enabled = true;
}
extern "C" void nx_spi_model_irq_disable(IRQn_Type irq) {
    vector(irq).enabled = false;
}
extern "C" void nx_spi_model_irq_clear(IRQn_Type irq) {
    vector(irq).pending = false;
}
extern "C" bool nx_stm32f1_system_live(void) {
    return true;
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    return now;
}

TEST_F(F103SpiDma, Spi1CannotAcquireSpi2FixedDmaChannels) {
    config.spi = {SPI1, GPIOA, 8000000U, 5U, 6U, 7U, 4U};
    EXPECT_EQ(nx_stm32f1_spi_dma_admit(&owner, &config), NX_ERROR_INVALID);
    EXPECT_EQ(nx_stm32f1_spi_dma_prepare(&owner, &config), NX_ERROR_INVALID);
    EXPECT_EQ(RCC->AHBENR & RCC_AHBENR_DMA1EN, 0U);
    EXPECT_EQ(RCC->APB2ENR & RCC_APB2ENR_SPI1EN, 0U);
}

TEST_F(F103SpiDma, RequiresTheExactPairAndWritableReceiveDomain) {
    config.rx = DMA1_Channel3;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32f1_spi_dma_prepare(&owner, &config));
    config.rx = DMA1_Channel4;
    spi_irq.pending = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_spi_dma_prepare(&owner, &config));
    spi_irq.pending = false;
    Prepare();
    region.permissions = NX_DMA_MEMORY_READ;
    ASSERT_EQ(NX_SUCCESS, nx_spi_request_prepare(&request, tx, rx, 4U, 1000U));
    EXPECT_EQ(NX_ERROR_PERMISSION, nx_spi_endpoint_submit(&face, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(0U, selections);
}

TEST_F(F103SpiDma, BothDmaEventsThenWireIdleAndBothIrqsBeforeCsRelease) {
    Prepare();
    Start();
    SPI2->SR = SPI_SR_TXE | SPI_SR_BSY;
    Signal(DMA_ISR_TCIF5);
    nx_spi_port_service(&port);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    Signal(DMA_ISR_TCIF4);
    nx_spi_port_service(&port);
    EXPECT_EQ(NX_REQUEST_DRAINING, nx_request_state(&request.base));
    EXPECT_EQ(0U, GPIOB->ODR & cs);
    SPI2->SR = SPI_SR_TXE;
    rx_irq.active = true;
    nx_spi_port_service(&port);
    EXPECT_EQ(0U, releases);
    rx_irq.active = false;
    nx_spi_port_service(&port);
    Settled(NX_SUCCESS, 4U);
    EXPECT_EQ(1U, releases);
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_spi_dma_close(&owner));
    EXPECT_EQ(7U, rx_irq.priority);
    EXPECT_EQ(11U, tx_irq.priority);
}

TEST_F(F103SpiDma, CancellationKeepsTheLoanAcrossAStuckReceiveLane) {
    Prepare();
    Start();
    refuse_rx_stop = true;
    EXPECT_EQ(NX_SUCCESS, nx_spi_port_cancel(&port, &request));
    now = 100000U;
    nx_spi_port_service(&port);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(0U, GPIOB->ODR & cs);
    EXPECT_EQ(NX_ERROR_BUSY, nx_spi_port_stop(&port));
    refuse_rx_stop = false;
    nx_spi_port_service(&port);
    Settled(NX_ERROR_CANCELLED, 0U);
    EXPECT_EQ(1U, releases);
}

TEST_F(F103SpiDma, LateDmaFaultAndCsWithdrawalFailureNeverSucceed) {
    Prepare();
    Start();
    late_error = true;
    refuse_cs_release = true;
    Signal(DMA_ISR_TCIF4 | DMA_ISR_TCIF5);
    nx_spi_port_service(&port);
    EXPECT_EQ(NX_REQUEST_DRAINING, nx_request_state(&request.base));
    now = 100000U;
    nx_spi_port_service(&port);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    refuse_cs_release = false;
    nx_spi_port_service(&port);
    Settled(NX_ERROR_IO, 0U);
    EXPECT_EQ(1U, releases);
}

TEST_F(F103SpiDma, DeadlineAndResidualReceiveAreSafelyRetired) {
    Prepare();
    Start(25U);
    now = 30U;
    SPI2->SR = SPI_SR_TXE | SPI_SR_RXNE | SPI_SR_OVR;
    nx_spi_port_service(&port);
    Settled(NX_ERROR_IO, 0U);
    EXPECT_EQ(1U, releases);
}

TEST_F(F103SpiDma, IrqCompletionPrecedesTheLaterTaskDrainDeadline) {
    Prepare();
    Start(25U);
    SPI2->SR = SPI_SR_TXE | SPI_SR_BSY;
    now = 22U;
    Signal(DMA_ISR_TCIF4 | DMA_ISR_TCIF5);
    EXPECT_EQ(NX_REQUEST_DRAINING, nx_request_state(&request.base));
    now = 30U;
    SPI2->SR = SPI_SR_TXE;
    nx_spi_port_service(&port);
    Settled(NX_SUCCESS, 4U);
}

#if defined(NX_STM32F1_SPI_DMA_SEQUENCE)
namespace {
class F103SpiDmaWords : public F103SpiDma {
  protected:
    nx_spi_dma_sequence_state_t sequence{};
    nx_spi_dma_sequence_endpoint_t child{&sequence, &endpoint};
    const nx_spi_dma_transaction_endpoint_t target{
        &nx_spi_dma_sequence_endpoint_ops, &child};
    const nx_spi_dma_transaction_port_t executor{&nx_spi_dma_sequence_port_ops,
                                                 &sequence};
    nx_spi_dma_transaction_request_t transaction{};

    void VerifyResult(nx_result_t expected, size_t words) const {
        nx_result_t result{};
        size_t count{};
        ASSERT_EQ(NX_SUCCESS,
                  nx_request_result(&transaction.base, &result, &count));
        EXPECT_EQ(expected, result);
        EXPECT_EQ(words, count);
    }

    void Bind() {
        Prepare();
        ASSERT_EQ(NX_SUCCESS,
                  nx_spi_dma_sequence_initialize(
                      &sequence, &nx_stm32f1_spi_dma_sequence_backend, &owner));
        nx_request_initialize(&transaction.base);
    }
};
} /* namespace */

TEST_F(F103SpiDmaWords, HalfwordPhasesRetainOneChipSelectAndCountWords) {
    Bind();
    const nx_spi_dma_segment_t pieces[] = {
        {tx, rx, 3U, 0U},
        {tx + 16U, rx + 16U, 2U,
         NX_SPI_MEMORY_TX_FIXED | NX_SPI_MEMORY_RX_FIXED},
    };
    ASSERT_EQ(NX_SUCCESS, nx_spi_dma_transaction_prepare(&transaction, pieces,
                                                         2U, 16U, 1000U));
    ASSERT_EQ(NX_SUCCESS, nx_spi_dma_transaction_submit(&target, &transaction));
    constexpr uint32_t width = DMA_CCR_PSIZE_0 | DMA_CCR_MSIZE_0;
    EXPECT_EQ(width, DMA1_Channel4->CCR & (DMA_CCR_PSIZE | DMA_CCR_MSIZE));
    EXPECT_EQ(width, DMA1_Channel5->CCR & (DMA_CCR_PSIZE | DMA_CCR_MSIZE));
    EXPECT_EQ(3U, DMA1_Channel4->CNDTR);
    EXPECT_EQ(3U, DMA1_Channel5->CNDTR);
    EXPECT_NE(0U, DMA1_Channel4->CCR & DMA_CCR_MINC);
    EXPECT_EQ(SPI_CR1_DFF, SPI2->CR1 & (SPI_CR1_DFF | SPI_CR1_CRCEN));
    SPI2->SR = SPI_SR_TXE | SPI_SR_BSY;
    Signal(DMA_ISR_TCIF4 | DMA_ISR_TCIF5);
    nx_spi_dma_transaction_service(&executor);
    EXPECT_EQ(0U, releases);
    EXPECT_EQ(0U, GPIOB->ODR & cs);

    SPI2->SR = SPI_SR_TXE;
    nx_spi_dma_transaction_service(&executor);
    EXPECT_EQ(1U, selections);
    EXPECT_EQ(0U, releases);
    EXPECT_EQ(2U, DMA1_Channel4->CNDTR);
    EXPECT_EQ(0U, (DMA1_Channel4->CCR | DMA1_Channel5->CCR) & DMA_CCR_MINC);
    EXPECT_EQ(width, DMA1_Channel4->CCR & (DMA_CCR_PSIZE | DMA_CCR_MSIZE));
    Signal(DMA_ISR_TCIF4 | DMA_ISR_TCIF5);
    nx_spi_dma_transaction_service(&executor);
    VerifyResult(NX_SUCCESS, 5U);
    EXPECT_EQ(1U, selections);
    EXPECT_EQ(1U, releases);
    EXPECT_EQ(0U, DMA1_Channel4->CMAR | DMA1_Channel5->CMAR |
                      DMA1_Channel4->CPAR | DMA1_Channel5->CPAR);
    EXPECT_EQ(NX_SUCCESS, nx_spi_dma_transaction_stop(&executor));
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_spi_dma_close(&owner));
}
TEST_F(F103SpiDmaWords, FutureMemoryWithoutDmaReachabilityIsRejectedBeforeCs) {
    Bind();
    uint16_t outside{};
    const nx_spi_dma_segment_t pieces[] = {
        {tx, rx, 3U, 0U},
        {tx + 16U, &outside, 1U, 0U},
    };
    ASSERT_EQ(NX_SUCCESS, nx_spi_dma_transaction_prepare(&transaction, pieces,
                                                         2U, 16U, 1000U));
    EXPECT_NE(NX_SUCCESS, nx_spi_dma_transaction_submit(&target, &transaction));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&transaction.base));
    EXPECT_EQ(0U, selections);
    EXPECT_EQ(0U, DMA1_Channel4->CMAR | DMA1_Channel5->CMAR);
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_spi_dma_sequence_close(&sequence, &owner));
}

TEST_F(F103SpiDmaWords,
       CancelPreservesCompletedWordsAndRetiresResidualHalfword) {
    Bind();
    const nx_spi_dma_segment_t pieces[] = {
        {tx, rx, 3U, 0U},
        {tx + 16U, rx + 16U, 2U, 0U},
    };
    ASSERT_EQ(NX_SUCCESS, nx_spi_dma_transaction_prepare(&transaction, pieces,
                                                         2U, 16U, 1000U));
    ASSERT_EQ(NX_SUCCESS, nx_spi_dma_transaction_submit(&target, &transaction));
    Signal(DMA_ISR_TCIF4 | DMA_ISR_TCIF5);
    nx_spi_dma_transaction_service(&executor);
    ASSERT_EQ(2U, DMA1_Channel4->CNDTR);
    ASSERT_EQ(NX_SUCCESS,
              nx_spi_dma_transaction_cancel(&executor, &transaction));
    SPI2->SR = SPI_SR_TXE | SPI_SR_BSY | SPI_SR_RXNE | SPI_SR_OVR;
    nx_spi_dma_transaction_service(&executor);
    EXPECT_EQ(NX_REQUEST_DRAINING, nx_request_state(&transaction.base));
    EXPECT_EQ(0U, releases);
    SPI2->SR &= ~static_cast<uint32_t>(SPI_SR_BSY);
    nx_spi_dma_transaction_service(&executor);
    VerifyResult(NX_ERROR_CANCELLED, 3U);
    EXPECT_EQ(16U, receive_bits);
    EXPECT_EQ(1U, releases);
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_spi_dma_sequence_close(&sequence, &owner));
}
#endif
