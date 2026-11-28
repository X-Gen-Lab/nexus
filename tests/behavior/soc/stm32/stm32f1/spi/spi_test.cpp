/**
 * \file            spi_test.cpp
 * \brief           Exact F103 SPI2 polling, tail and CS recovery tests.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/spi/instance.h"
#include "system/platform.h"
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
constexpr uintptr_t peripheral_base = 0x40000000U;
constexpr size_t peripheral_bytes = 0x24000U;
constexpr uint32_t select_pin = 1U << 12U;
constexpr uint32_t br_divider4 = SPI_CR1_BR_0;
unsigned selects;
unsigned deselects;
unsigned frames;
unsigned tail_ticks;
bool stuck_busy;
bool blocked_reset;
bool blocked_cs_release;
nx_time_us_t now;
uint16_t last_word;
uint32_t last_controls;
uint8_t last_width;

class F1Spi : public testing::Test {
  protected:
    void* mapping = MAP_FAILED;
    nx_stm32f1_spi_t bus{};
    const nx_stm32f1_spi_config_t config{SPI2, GPIOB, 8000000U, 13U,
                                         14U,  15U,   12U};
    nx_stm32f1_spi_endpoint_t selected{&bus, GPIOB, 3000000U, 12U, 3U, false};
    const nx_spi_port_t port{&nx_stm32f1_spi_ops, &bus};
    const nx_spi_endpoint_t bytes{&nx_stm32f1_spi_endpoint_ops, &selected};
    const nx_spi_transaction_endpoint_t transaction{
        &nx_stm32f1_spi_transaction_ops, &selected};

    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(peripheral_base), peripheral_bytes,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(peripheral_base));
        GPIOB->CRL = GPIOB->CRH = UINT32_C(0x44444444);
        SPI2->SR = SPI_SR_TXE;
        stuck_busy = blocked_reset = blocked_cs_release = false;
        now = 10U;
        frames = selects = deselects = tail_ticks = 0U;
        last_word = 0U;
        last_controls = 0U;
        last_width = 0U;
    }

    void TearDown() override {
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(munmap(mapping, peripheral_bytes), 0);
        }
    }

    void prepare() {
        ASSERT_EQ(nx_stm32f1_spi_prepare(&bus, &config), NX_SUCCESS);
        EXPECT_NE(RCC->APB1ENR & RCC_APB1ENR_SPI2EN, 0U);
        EXPECT_NE(GPIOB->ODR & select_pin, 0U);
        selects = deselects = 0U;
    }
};

TEST_F(F1Spi, ByteFacePreservesInPlaceBytesAndOneCsInterval) {
    prepare();
    EXPECT_TRUE(nx_spi_endpoint_on_port(&bytes, &port));
    uint8_t packet[] = {0x12U, 0x98U};
    size_t complete = 37U;
    ASSERT_EQ(
        nx_spi_endpoint_transfer(&bytes, packet, packet, 2U, 1000U, &complete),
        NX_SUCCESS);
    EXPECT_EQ(packet[0], 0xB7U);
    EXPECT_EQ(packet[1], 0x3DU);
    EXPECT_EQ(complete, 2U);
    EXPECT_EQ(last_controls & SPI_CR1_BR, br_divider4);
    EXPECT_EQ(last_controls & (SPI_CR1_CPOL | SPI_CR1_CPHA),
              SPI_CR1_CPOL | SPI_CR1_CPHA);
    EXPECT_EQ(last_controls & SPI_CR1_DFF, 0U);
    EXPECT_EQ(selects, 1U);
    EXPECT_EQ(deselects, 1U);
    ASSERT_EQ(nx_stm32f1_spi_close(&bus), NX_SUCCESS);
    EXPECT_EQ(RCC->APB1ENR & RCC_APB1ENR_SPI2EN, 0U);
    EXPECT_EQ(GPIOB->CRH, UINT32_C(0x44444444));
}

TEST_F(F1Spi, WordTransactionUsesOneSelectionAcrossSegments) {
    prepare();
    const uint16_t address = 0xAB89U;
    uint16_t response[2]{};
    const nx_spi_segment_t phases[] = {{&address, nullptr, 1U, 0U},
                                       {nullptr, response, 2U, 0x5555U}};
    size_t complete = 0U;
    ASSERT_EQ(nx_spi_transaction_transfer(&transaction, phases, 2U, 16U, 1000U,
                                          &complete),
              NX_SUCCESS);
    EXPECT_EQ(complete, 3U);
    EXPECT_EQ(frames, 3U);
    EXPECT_EQ(last_width, 16U);
    EXPECT_NE(last_controls & SPI_CR1_DFF, 0U);
    EXPECT_EQ(last_word, 0x5555U);
    EXPECT_EQ(response[0], 0xF0F0U);
    EXPECT_EQ(response[1], 0xF0F0U);
    EXPECT_EQ(selects, 1U);
    EXPECT_EQ(deselects, 1U);
}

TEST_F(F1Spi, RejectedSpansAndDeadlineHaveNoElectricalEffect) {
    prepare();
    uint8_t packet[8]{};
    size_t complete = 50U;
    EXPECT_EQ(nx_spi_endpoint_transfer(&bytes, packet, packet + 1U, 2U, 1000U,
                                       &complete),
              NX_ERROR_INVALID);
    EXPECT_EQ(
        nx_spi_endpoint_transfer(&bytes, packet, nullptr, 2U, now, &complete),
        NX_ERROR_TIMEOUT);
    EXPECT_EQ(complete, 50U);
    EXPECT_EQ(selects, 0U);
    EXPECT_EQ(frames, 0U);
    EXPECT_EQ(SPI2->CR1, 0U);
}

TEST_F(F1Spi, TimeoutUsesPrivateDrainGraceBeforeReleasingCs) {
    prepare();
    uint8_t packet = 0xACU;
    size_t complete = 13U;
    stuck_busy = true;
    EXPECT_EQ(nx_spi_endpoint_transfer(&bytes, &packet, nullptr, 1U, now + 8U,
                                       &complete),
              NX_ERROR_TIMEOUT);
    EXPECT_TRUE(complete <= 1U);
    EXPECT_EQ(selects, 1U);
    EXPECT_EQ(deselects, 1U);
    EXPECT_EQ(SPI2->SR & SPI_SR_BSY, 0U);
    EXPECT_EQ(SPI2->CR1 & SPI_CR1_SPE, 0U);
}

TEST_F(F1Spi, FailedCsReleaseRetainsOnlyTheStaticRecoveryDuty) {
    prepare();
    uint8_t packet = 0xACU;
    size_t complete = 13U;
    blocked_cs_release = true;
    EXPECT_EQ(nx_spi_endpoint_transfer(&bytes, &packet, nullptr, 1U, 1000U,
                                       &complete),
              NX_ERROR_IO);
    EXPECT_EQ(GPIOB->ODR & select_pin, 0U);
    const unsigned prior = frames;
    EXPECT_EQ(nx_spi_endpoint_transfer(&bytes, &packet, nullptr, 1U, 1000U,
                                       &complete),
              NX_ERROR_STATE);
    EXPECT_EQ(frames, prior);
    EXPECT_EQ(nx_spi_port_recover(&port), NX_ERROR_IO);
    blocked_cs_release = false;
    ASSERT_EQ(nx_spi_port_recover(&port), NX_SUCCESS);
    EXPECT_NE(GPIOB->ODR & select_pin, 0U);
    EXPECT_EQ(nx_spi_endpoint_transfer(&bytes, &packet, nullptr, 1U, 1000U,
                                       &complete),
              NX_SUCCESS);
}

TEST_F(F1Spi, StuckWireAndResetHoldSelectionUntilExplicitRecovery) {
    prepare();
    uint8_t packet = 0x43U;
    size_t complete = 0U;
    blocked_reset = stuck_busy = true;
    EXPECT_EQ(nx_spi_endpoint_transfer(&bytes, &packet, nullptr, 1U, now + 8U,
                                       &complete),
              NX_ERROR_IO);
    EXPECT_EQ(GPIOB->ODR & select_pin, 0U);
    EXPECT_NE(SPI2->SR & SPI_SR_BSY, 0U);
    const unsigned transmitted = frames;
    EXPECT_EQ(nx_spi_port_recover(&port), NX_ERROR_IO);
    blocked_reset = false;
    EXPECT_EQ(nx_spi_port_recover(&port), NX_SUCCESS);
    EXPECT_NE(GPIOB->ODR & select_pin, 0U);
    EXPECT_EQ(frames, transmitted);
}
} /* namespace */

extern "C" {
bool nx_stm32f1_system_live(void) {
    return true;
}
nx_time_us_t nx_time_now_us(void) {
    return now;
}
void nx_spi_model_write(volatile uint32_t* reg, uint32_t value) {
    if (reg == &SPI2->CR1 && (value & SPI_CR1_SPE) != 0U) {
        const uint32_t framing =
            SPI_CR1_DFF | SPI_CR1_CPOL | SPI_CR1_CPHA | SPI_CR1_LSBFIRST;
        EXPECT_EQ((value ^ SPI2->CR1) & framing, 0U);
    }
    if (reg == &RCC->APB1RSTR && (value & RCC_APB1RSTR_SPI2RST) != 0U) {
        if (blocked_reset) {
            return;
        }
        SPI2->CR1 = SPI2->CR2 = SPI2->I2SCFGR = 0U;
        SPI2->SR = SPI_SR_TXE;
        tail_ticks = 0U;
    }
    *reg = value;
}
void nx_spi_model_bsrr(GPIO_TypeDef* reg, uint32_t value) {
    if (reg == GPIOB && (value & select_pin) != 0U && blocked_cs_release) {
        return;
    }
    const uint32_t previous = reg->ODR;
    reg->ODR = (previous & ~(value >> 16U)) | (value & 0xFFFFU);
    if (reg == GPIOB && ((previous ^ reg->ODR) & select_pin) != 0U) {
        if ((reg->ODR & select_pin) != 0U) {
            EXPECT_EQ(SPI2->SR & SPI_SR_BSY, 0U);
            ++deselects;
        } else {
            ++selects;
        }
    }
}
uint32_t nx_spi_model_status(SPI_TypeDef* reg) {
    ++now;
    if (tail_ticks != 0U && !stuck_busy && --tail_ticks == 0U) {
        reg->SR &= ~static_cast<uint32_t>(SPI_SR_BSY);
    }
    return reg->SR;
}
void nx_spi_model_send(SPI_TypeDef* reg, uint16_t value, uint8_t bits) {
    EXPECT_EQ(reg, SPI2);
    EXPECT_EQ(GPIOB->ODR & select_pin, 0U);
    EXPECT_NE(reg->CR1 & SPI_CR1_SPE, 0U);
    last_word = value;
    last_controls = reg->CR1;
    last_width = bits;
    ++frames;
    tail_ticks = 3U;
    reg->SR = SPI_SR_RXNE | SPI_SR_TXE | SPI_SR_BSY;
    reg->DR = value ^ (bits == 8U ? 0xA5U : 0xA5A5U);
}
uint16_t nx_spi_model_receive(SPI_TypeDef* reg, uint8_t bits) {
    EXPECT_EQ(bits, last_width);
    reg->SR &= ~static_cast<uint32_t>(SPI_SR_RXNE);
    return static_cast<uint16_t>(reg->DR);
}
}
