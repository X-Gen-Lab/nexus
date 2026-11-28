/**
 * \file            i2s2_test.cpp
 * \brief           F103ZE exact audio clock, finite DMA and frame loan tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/audio/i2s2.h"
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
    uint32_t priority;
};
Vector dma_irq;
Vector spi_irq;
nx_time_us_t now;
bool refuse_dma_stop;
bool refuse_address_clear;
bool late_dma_error;
unsigned enables;
unsigned disables;

Vector& vector(IRQn_Type irq) {
    EXPECT_TRUE(irq == DMA1_Channel5_IRQn || irq == SPI2_IRQn);
    return irq == DMA1_Channel5_IRQn ? dma_irq : spi_irq;
}

void* map_at(uintptr_t address, size_t bytes) {
    return mmap(reinterpret_cast<void*>(address), bytes, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
}

class F103I2s2 : public testing::Test {
  protected:
    void* bus = MAP_FAILED;
    void* ram = MAP_FAILED;
    nx_dma_memory_region_t region{sram, sram_bytes, NX_DMA_MEMORY_READ};
    nx_stm32f1_i2s2_config_t config{&region, 1U, {15625U, 1U}, 8U, false, 5U};
    nx_stm32f1_i2s2_t owner{};
    const nx_audio_port_t face{&nx_stm32f1_i2s2_ops, &owner};
    nx_stream_t stream{};
    nx_stream_slot_t slots[1]{};
    nx_frame_stream_t framed{};
    nx_frame_dma_loan_t loan{};
    nx_audio_request_t request{};

    void SetUp() override {
        bus = map_at(mmio, mmio_bytes);
        ASSERT_EQ(bus, reinterpret_cast<void*>(mmio));
        ram = map_at(sram, sram_bytes);
        ASSERT_EQ(ram, reinterpret_cast<void*>(sram));
        now = 10U;
        dma_irq = {false, false, false, 7U};
        spi_irq = {false, false, false, 9U};
        refuse_dma_stop = refuse_address_clear = late_dma_error = false;
        enables = disables = 0U;
        GPIOB->CRL = GPIOB->CRH = 0x44444444U;
        RCC->CR = RCC_CR_HSION | RCC_CR_HSIRDY;
        SPI2->I2SPR = 2U;
        slots[0].data = reinterpret_cast<uint8_t*>(sram);
        slots[0].capacity = 8U;
        ASSERT_EQ(NX_SUCCESS, nx_stream_initialize(&stream, slots, 1U));
        nx_frame_audio_format_t format{};
        format.kind = NX_FRAME_AUDIO_I2S;
        format.sample = NX_FRAME_PCM_SIGNED;
        format.alignment = NX_FRAME_SAMPLE_LSB;
        format.frames = 2U;
        format.active_slots = 3U;
        format.slots = 2U;
        format.sample_bits = 16U;
        format.slot_bits = 16U;
        nx_frame_layout_t geometry{};
        ASSERT_EQ(NX_SUCCESS, nx_frame_layout_audio(&format, &geometry));
        ASSERT_EQ(NX_SUCCESS,
                  nx_frame_stream_initialize(&framed, &stream, &geometry));
        nx_stream_fill_t fill{};
        ASSERT_EQ(NX_SUCCESS, nx_stream_reserve(&stream, &fill));
        fill.data[0] = 0x21U;
        ASSERT_EQ(NX_SUCCESS, nx_frame_stream_publish(&framed, &fill, 0U));
        ASSERT_EQ(NX_SUCCESS, nx_frame_stream_acquire(&framed, &loan));
        nx_request_initialize(&request.base);
        ASSERT_EQ(NX_SUCCESS, nx_request_prepare(&request.base, 1000U));
        request.tx = &loan;
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
        ASSERT_EQ(NX_SUCCESS, nx_stm32f1_i2s2_prepare(&owner, &config));
    }

    void Start() {
        ASSERT_EQ(NX_SUCCESS, nx_audio_port_submit(&face, &request));
        EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
        EXPECT_EQ(4U, DMA1_Channel5->CNDTR);
        EXPECT_EQ(sram, DMA1_Channel5->CMAR);
        EXPECT_EQ(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&SPI2->DR)),
                  DMA1_Channel5->CPAR);
        EXPECT_EQ(1U, enables);
    }

    void Signal(uint32_t status) {
        if ((status & DMA_ISR_TCIF5) != 0U)
            DMA1_Channel5->CNDTR = 0U;
        *const_cast<volatile uint32_t*>(&DMA1->ISR) |= status;
        dma_irq.active = true;
        nx_stm32f1_i2s2_irq(&owner);
        dma_irq.active = false;
    }

    void Settled(nx_result_t expected, size_t bytes) {
        nx_result_t result{};
        size_t transferred{};
        ASSERT_EQ(NX_SUCCESS,
                  nx_request_result(&request.base, &result, &transferred));
        EXPECT_EQ(expected, result);
        EXPECT_EQ(bytes, transferred);
        EXPECT_EQ(nullptr, loan.identity);
        EXPECT_EQ(NX_STREAM_SLOT_FREE, slots[0].state);
    }
};
} /* namespace */

extern "C" void nx_i2s2_model_write(volatile uint32_t* reg, uint32_t value) {
    if (reg == &DMA1->IFCR) {
        constexpr uint32_t lane =
            DMA_IFCR_CTCIF5 | DMA_IFCR_CHTIF5 | DMA_IFCR_CTEIF5;
        EXPECT_EQ(0U, value & ~lane);
        *const_cast<volatile uint32_t*>(&DMA1->ISR) &= ~value;
        return;
    }
    if (reg == &DMA1_Channel5->CCR && (DMA1_Channel5->CCR & DMA_CCR_EN) != 0U &&
        (value & DMA_CCR_EN) == 0U) {
        EXPECT_EQ(0U, SPI2->CR2 & SPI_CR2_TXDMAEN);
        if (refuse_dma_stop)
            return;
        if (late_dma_error)
            *const_cast<volatile uint32_t*>(&DMA1->ISR) |= DMA_ISR_TEIF5;
    }
    if (reg == &DMA1_Channel5->CMAR && value == 0U && refuse_address_clear)
        return;
    if (reg == &SPI2->I2SCFGR) {
        const bool was_enabled = (SPI2->I2SCFGR & SPI_I2SCFGR_I2SE) != 0U;
        const bool enabled = (value & SPI_I2SCFGR_I2SE) != 0U;
        if (!was_enabled && enabled) {
            EXPECT_EQ(SPI_I2SCFGR_I2SMOD | SPI_I2SCFGR_I2SCFG_1,
                      value & ~SPI_I2SCFGR_I2SE);
            EXPECT_NE(0U, SPI2->CR2 & SPI_CR2_TXDMAEN);
            EXPECT_EQ(DMA_CCR_PSIZE_0 | DMA_CCR_MSIZE_0,
                      DMA1_Channel5->CCR & (DMA_CCR_PSIZE | DMA_CCR_MSIZE));
            EXPECT_NE(0U, DMA1_Channel5->CCR & DMA_CCR_EN);
            EXPECT_TRUE(dma_irq.enabled);
            ++enables;
            SPI2->SR = SPI_SR_BSY;
        } else if (was_enabled && !enabled) {
            EXPECT_EQ(SPI_SR_TXE, SPI2->SR & (SPI_SR_TXE | SPI_SR_BSY));
            EXPECT_EQ(0U, DMA1_Channel5->CCR & DMA_CCR_EN);
            ++disables;
            SPI2->SR = 0U;
        }
    }
    *reg = value;
}
extern "C" void nx_i2s2_model_bsrr(GPIO_TypeDef* reg, uint32_t value) {
    reg->ODR = (reg->ODR | (value & 0xffffU)) & ~(value >> 16U);
}
extern "C" uint32_t nx_i2s2_model_status(SPI_TypeDef* reg) {
    return reg->SR;
}
extern "C" bool nx_i2s2_model_irq_enabled(IRQn_Type irq) {
    return vector(irq).enabled;
}
extern "C" bool nx_i2s2_model_irq_pending(IRQn_Type irq) {
    return vector(irq).pending;
}
extern "C" bool nx_i2s2_model_irq_active(IRQn_Type irq) {
    return vector(irq).active;
}
extern "C" uint32_t nx_i2s2_model_irq_priority(IRQn_Type irq) {
    return vector(irq).priority;
}
extern "C" void nx_i2s2_model_irq_priority_set(IRQn_Type irq, uint32_t value) {
    vector(irq).priority = value;
}
extern "C" void nx_i2s2_model_irq_enable(IRQn_Type irq) {
    vector(irq).enabled = true;
}
extern "C" void nx_i2s2_model_irq_disable(IRQn_Type irq) {
    vector(irq).enabled = false;
}
extern "C" void nx_i2s2_model_irq_clear(IRQn_Type irq) {
    vector(irq).pending = false;
}
extern "C" bool nx_stm32f1_system_live(void) {
    return true;
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    return now;
}

TEST_F(F103I2s2, RejectsWrongStaticRateBeforeAnyEffects) {
    config.rate.numerator_hz = 16000U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32f1_i2s2_admit(&owner, &config));
    EXPECT_EQ(0U, RCC->APB1ENR & RCC_APB1ENR_SPI2EN);
    EXPECT_EQ(0U, RCC->AHBENR & RCC_AHBENR_DMA1EN);
    config.rate = {15625U, 1U};
    Prepare();
    nx_audio_rate_t rate{};
    ASSERT_EQ(NX_SUCCESS, nx_audio_port_rate(&face, &rate));
    EXPECT_EQ(15625U, rate.numerator_hz);
    EXPECT_EQ(1U, rate.denominator);
    SPI2->I2SPR ^= SPI_I2SPR_ODD;
    EXPECT_EQ(NX_ERROR_STATE, nx_audio_port_rate(&face, &rate));
}

TEST_F(F103I2s2, KeepsTheLoanUntilWireAndDmaAddressBothRetire) {
    Prepare();
    Start();
    SPI2->SR = SPI_SR_TXE | SPI_SR_BSY;
    Signal(DMA_ISR_TCIF5);
    EXPECT_EQ(NX_ERROR_BUSY, nx_audio_port_service(&face));
    EXPECT_EQ(0U, disables);
    EXPECT_NE(nullptr, loan.identity);
    SPI2->SR = SPI_SR_TXE;
    refuse_address_clear = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_audio_port_service(&face));
    EXPECT_EQ(1U, disables);
    EXPECT_NE(nullptr, loan.identity);
    refuse_address_clear = false;
    EXPECT_EQ(NX_SUCCESS, nx_audio_port_service(&face));
    Settled(NX_SUCCESS, 8U);
    EXPECT_EQ(0U, DMA1_Channel5->CMAR);
    EXPECT_EQ(0U, DMA1_Channel5->CPAR);
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_i2s2_close(&owner));
    EXPECT_EQ(7U, dma_irq.priority);
    EXPECT_EQ(0x44444444U, GPIOB->CRH);
}

TEST_F(F103I2s2, CancellationCannotReclaimAnIncompleteWireOrStuckDma) {
    Prepare();
    Start();
    refuse_dma_stop = true;
    DMA1_Channel5->CNDTR = 3U;
    EXPECT_EQ(NX_SUCCESS, nx_audio_port_cancel(&face, &request));
    now = 100000U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_audio_port_service(&face));
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_NE(nullptr, loan.identity);
    refuse_dma_stop = false;
    SPI2->SR = SPI_SR_TXE | SPI_SR_BSY;
    EXPECT_EQ(NX_ERROR_BUSY, nx_audio_port_service(&face));
    SPI2->SR = SPI_SR_TXE;
    EXPECT_EQ(NX_ERROR_CANCELLED, nx_audio_port_service(&face));
    Settled(NX_ERROR_CANCELLED, 0U);
}

TEST_F(F103I2s2, LateTransferErrorCannotReportSuccess) {
    Prepare();
    Start();
    late_dma_error = true;
    SPI2->SR = SPI_SR_TXE;
    Signal(DMA_ISR_TCIF5);
    EXPECT_EQ(NX_ERROR_IO, nx_audio_port_service(&face));
    Settled(NX_ERROR_IO, 0U);
}

TEST_F(F103I2s2, RejectsForeignControllerAndUnreadableLoan) {
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_i2s2_prepare(&owner, &config));
    RCC->APB1ENR &= ~static_cast<uint32_t>(RCC_APB1ENR_SPI2EN);
    Prepare();
    region.permissions = NX_DMA_MEMORY_WRITE;
    EXPECT_EQ(NX_ERROR_PERMISSION, nx_audio_port_submit(&face, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(0U, enables);
    EXPECT_NE(nullptr, loan.identity);
}

TEST_F(F103I2s2, PublicStopLeavesPreparedPortAvailableForTheNextRequest) {
    Prepare();
    EXPECT_EQ(NX_SUCCESS, nx_audio_port_stop(&face));
    EXPECT_EQ(NX_SUCCESS, nx_audio_port_submit(&face, &request));
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
}

TEST_F(F103I2s2, QueriesCannotModifyTheActiveFrameOrItsLoan) {
    Prepare();
    Start();
    const uint8_t first_sample = slots[0].data[0];
    EXPECT_EQ(
        NX_ERROR_INVALID,
        nx_audio_port_rate(&face, reinterpret_cast<nx_audio_rate_t*>(sram)));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_audio_port_layout(
                  &face, reinterpret_cast<nx_audio_layout_t*>(&loan)));
    EXPECT_EQ(first_sample, slots[0].data[0]);
    EXPECT_EQ(&loan, loan.identity);
}
