/**
 * \file            spi_multi_controller_test.cpp
 * \brief           Exact DMA/controller isolation and independent CS drain
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
extern "C" {
#if defined(NX_DMA_MULTI_STM32)
#include "stm32f407_dma.h"
nx_stm32_system_t g_nx_stm32_system;
#define DMA_STATE    nx_stm32_spi_dma_state_t
#define DEVICE_STATE nx_stm32_spi_dma_endpoint_state_t
#define DMA_OPS      nx_stm32_spi_dma_ops
#define DEVICE_OPS   nx_stm32_spi_dma_endpoint_ops
#define DMA_IRQ      nx_stm32_spi_dma_irq
#define SPI0_REG     SPI1
#define SPI1_REG     SPI2
#define RXIRQ0       DMA2_Stream0_IRQn
#define TXIRQ0       DMA2_Stream3_IRQn
#define RXIRQ1       DMA1_Stream3_IRQn
#define TXIRQ1       DMA1_Stream4_IRQn
#define STATUS(i)    (spis[i]->SR)
#define CONTROL(i)   (spis[i]->CR1)
#define DATA(i)      (spis[i]->DR)
#define BUSY         SPI_SR_BSY
#define TX_EMPTY     SPI_SR_TXE
#define ERROR        SPI_SR_MODF
#else
#include "gd32f470_spi_dma.h"
#include "gd32f4xx_dma.h"
#define DMA_STATE    nx_gd32_spi_dma_state_t
#define DEVICE_STATE nx_gd32_spi_dma_endpoint_state_t
#define DMA_OPS      nx_gd32_spi_dma_ops
#define DEVICE_OPS   nx_gd32_spi_dma_endpoint_ops
#define DMA_IRQ      nx_gd32_spi_dma_irq
#define SPI0_REG     SPI4
#define SPI1_REG     SPI0
#define RXIRQ0       DMA1_Channel3_IRQn
#define TXIRQ0       DMA1_Channel4_IRQn
#define RXIRQ1       DMA1_Channel2_IRQn
#define TXIRQ1       DMA1_Channel5_IRQn
#define STATUS(i)    SPI_STAT(spis[i])
#define CONTROL(i)   SPI_CTL0(spis[i])
#define DATA(i)      SPI_DATA(spis[i])
#define BUSY         SPI_STAT_TRANS
#define TX_EMPTY     SPI_STAT_TBE
#define ERROR        SPI_STAT_CONFERR
#endif
extern uint64_t g_gd32_model_now;
extern bool g_gd32_model_isr;
extern uint32_t g_gd32_model_mask;
}
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <vector>

namespace {
volatile uint32_t* held;
volatile uint32_t* error_control;
volatile uint32_t* error_clear;
volatile uint32_t* error_status;
uint32_t error_mask;
std::vector<volatile uint32_t*> resets;
class SPIMultiple : public testing::Test {
  protected:
    DMA_STATE states[2]{};
    DEVICE_STATE children[2]{};
    nx_spi_port_t ports[2]{{&DMA_OPS, &states[0]}, {&DMA_OPS, &states[1]}};
    nx_spi_endpoint_t devices[2]{{&DEVICE_OPS, &children[0]},
                                 {&DEVICE_OPS, &children[1]}};
    nx_spi_request_t requests[2]{};
    uint8_t tx[2][4]{{1U, 2U, 3U, 4U}, {5U, 6U, 7U, 8U}};
    uint8_t rx[2][4]{};
    nx_dma_memory_region_t regions[2]{};
#if defined(NX_DMA_MULTI_STM32)
    SPI_TypeDef* spis[2]{SPI0_REG, SPI1_REG};
    nx_stm32_gpio_state_t cs[2]{{GPIOC, 1U, true, true},
                                { GPIOC,
                                  2U,
                                  true,
                                  true }};
    volatile uint32_t* rx_controls[2]{&DMA2_Stream0->CR, &DMA1_Stream3->CR};
    volatile uint32_t* tx_controls[2]{&DMA2_Stream3->CR, &DMA1_Stream4->CR};
    volatile uint32_t* rx_counts[2]{&DMA2_Stream0->NDTR, &DMA1_Stream3->NDTR};
    volatile uint32_t* tx_counts[2]{&DMA2_Stream3->NDTR, &DMA1_Stream4->NDTR};
    volatile uint32_t* rx_flags[2]{&DMA2->LISR, &DMA1->LISR};
    volatile uint32_t* tx_flags[2]{&DMA2->LISR, &DMA1->HISR};
    volatile uint32_t* peripheral[2]{&DMA2_Stream3->PAR, &DMA1_Stream4->PAR};
#else
    uint32_t spis[2]{SPI0_REG, SPI1_REG};
    volatile uint32_t* rx_controls[2]{&DMA_CH3CTL(DMA1), &DMA_CH2CTL(DMA1)};
    volatile uint32_t* tx_controls[2]{&DMA_CH4CTL(DMA1), &DMA_CH5CTL(DMA1)};
    volatile uint32_t* rx_counts[2]{&DMA_CH3CNT(DMA1), &DMA_CH2CNT(DMA1)};
    volatile uint32_t* tx_counts[2]{&DMA_CH4CNT(DMA1), &DMA_CH5CNT(DMA1)};
    volatile uint32_t* rx_flags[2]{&DMA_INTF0(DMA1), &DMA_INTF0(DMA1)};
    volatile uint32_t* tx_flags[2]{&DMA_INTF1(DMA1), &DMA_INTF1(DMA1)};
    volatile uint32_t* peripheral[2]{&DMA_CH4PADDR(DMA1), &DMA_CH5PADDR(DMA1)};
#endif
    unsigned rx_shifts[2] {
#if defined(NX_DMA_MULTI_STM32)
        0U, 22U
#else
        22U, 16U
#endif
    };
    unsigned tx_shifts[2] {
#if defined(NX_DMA_MULTI_STM32)
        22U, 0U
#else
        0U, 6U
#endif
    };
    nx_result_t Initialize(unsigned i) {
#if defined(NX_DMA_MULTI_STM32)
        return nx_stm32_spi_dma_initialize(&states[i]);
#else
        return nx_gd32_spi_dma_initialize_at(&states[i], &children[i],
                                             i == 0U ? &nx_gd32_spi4_controller
                                                     : &nx_gd32_spi0_controller,
                                             GPIOC, 1U << i, 1000000U, 0U, 6U);
#endif
    }
    void SetUp() override {
        void* p = mmap(
            (void*)(uintptr_t)0x40000000U, 0x80000U, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(p, (void*)(uintptr_t)0x40000000U);
        g_gd32_model_nvic = {};
        g_gd32_model_now = 100U;
        g_gd32_model_isr = false;
        g_gd32_model_mask = 0U;
        held = nullptr;
        error_control = nullptr;
        error_clear = nullptr;
        error_status = nullptr;
        error_mask = 0U;
        resets.clear();
        regions[0] = {(uintptr_t)tx, sizeof(tx), NX_DMA_MEMORY_READ};
        regions[1] = {(uintptr_t)rx, sizeof(rx), NX_DMA_MEMORY_WRITE};
        for (unsigned i = 0U; i < 2U; ++i) {
            states[i].regions = regions;
            states[i].region_count = 2U;
#if defined(NX_DMA_MULTI_STM32)
            states[i].spi.registers = spis[i];
            states[i].spi.rcc = RCC;
            states[i].spi.clock_hz = i == 0U ? 84000000U : 42000000U;
            states[i].dma = i == 0U ? DMA2 : DMA1;
            states[i].rx = i == 0U ? DMA2_Stream0 : DMA1_Stream3;
            states[i].tx = i == 0U ? DMA2_Stream3 : DMA1_Stream4;
            children[i].endpoint = {&states[i].spi, &cs[i], 1U << i, 1000000U,
                                    0U};
            children[i].dma = &states[i];
#endif
            nx_request_initialize(&requests[i].base);
            ASSERT_EQ(nx_spi_request_prepare(&requests[i], tx[i], rx[i],
                                             sizeof(tx[i]), 1000000U),
                      NX_SUCCESS);
        }
    }
    void TearDown() override {
        held = nullptr;
        error_control = nullptr;
        error_clear = nullptr;
        g_gd32_model_isr = false;
        g_gd32_model_mask = 0U;
        for (unsigned i = 0U; i < 2U; ++i) {
            if (states[i].initialized) {
                if (states[i].active != nullptr) {
                    EXPECT_EQ(nx_spi_port_cancel(&ports[i], &requests[i]),
                              NX_SUCCESS);
                    STATUS(i) = TX_EMPTY;
                    nx_spi_port_service(&ports[i]);
                }
                EXPECT_EQ(nx_spi_port_stop(&ports[i]), NX_SUCCESS);
            }
        }
        EXPECT_EQ(munmap((void*)(uintptr_t)0x40000000U, 0x80000U), 0);
    }
    void Both() {
        ASSERT_EQ(Initialize(0U), NX_SUCCESS);
        ASSERT_EQ(Initialize(1U), NX_SUCCESS);
    }
    void SubmitBoth(uint8_t flags = 0U) {
        for (unsigned i = 0U; i < 2U; ++i)
            ASSERT_EQ(
                nx_spi_endpoint_submit_memory(&devices[i], &requests[i], flags),
                NX_SUCCESS);
    }
    void Complete(unsigned i) {
        *rx_counts[i] = 0U;
        *tx_counts[i] = 0U;
        *rx_flags[i] |= 0x20U << rx_shifts[i];
        *tx_flags[i] |= 0x20U << tx_shifts[i];
        DMA_IRQ(&states[i], true);
        DMA_IRQ(&states[i], false);
    }
    bool Enabled(IRQn_Type irq) const {
        return (NVIC->ISER[(unsigned)irq / 32U] &
                (1U << ((unsigned)irq % 32U))) != 0U;
    }
};
TEST_F(SPIMultiple, SecondaryColdInitPreservesPrimaryConfigAndPublishers) {
    ASSERT_EQ(Initialize(0U), NX_SUCCESS);
    CONTROL(0U) = 0x80U;
    NVIC_SetPriority(RXIRQ0, 9U);
    ASSERT_EQ(Initialize(1U), NX_SUCCESS);
    EXPECT_EQ(CONTROL(0U), 0x80U);
    EXPECT_EQ(NVIC_GetPriority(RXIRQ0), 9U);
    EXPECT_TRUE(Enabled(RXIRQ0));
    EXPECT_TRUE(Enabled(RXIRQ1));
}
TEST_F(SPIMultiple, PeerCompletionCannotClearFlagsOrReleaseBorrowedBuffers) {
    ASSERT_NO_FATAL_FAILURE(Both());
    ASSERT_NO_FATAL_FAILURE(SubmitBoth());
    EXPECT_EQ(*peripheral[0], (uint32_t)(uintptr_t)&DATA(0U));
    EXPECT_EQ(*peripheral[1], (uint32_t)(uintptr_t)&DATA(1U));
    *rx_flags[1] |= 0x20U << rx_shifts[1];
    *tx_flags[1] |= 0x20U << tx_shifts[1];
    Complete(0U);
    EXPECT_NE(*rx_flags[1] & (0x20U << rx_shifts[1]), 0U);
    EXPECT_NE(*tx_flags[1] & (0x20U << tx_shifts[1]), 0U);
    STATUS(0U) = TX_EMPTY;
    nx_spi_port_service(&ports[0]);
    EXPECT_EQ(nx_request_state(&requests[0].base), NX_REQUEST_SETTLED);
    EXPECT_EQ(requests[0].base.result, NX_SUCCESS);
    EXPECT_EQ(requests[0].base.transferred, sizeof(tx[0]));
    EXPECT_EQ(nx_request_state(&requests[1].base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(states[1].active, &requests[1]);
    EXPECT_NE(*rx_controls[1] & 1U, 0U);
    EXPECT_NE(*tx_controls[1] & 1U, 0U);
}
TEST_F(SPIMultiple, SecondaryWireBusyCannotReleaseItsCsOrBorrow) {
    ASSERT_NO_FATAL_FAILURE(Both());
    ASSERT_NO_FATAL_FAILURE(SubmitBoth());
    Complete(1U);
    STATUS(1U) = TX_EMPTY | BUSY;
    nx_spi_port_service(&ports[1]);
    EXPECT_EQ(states[1].active, &requests[1]);
    EXPECT_EQ(states[0].active, &requests[0]);
    STATUS(1U) = TX_EMPTY;
    nx_spi_port_service(&ports[1]);
    EXPECT_EQ(requests[1].base.result, NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&requests[1].base), NX_REQUEST_SETTLED);
}
TEST_F(SPIMultiple, RefusedSecondaryEngineWithdrawalRetainsOnlyItsBuffers) {
    ASSERT_NO_FATAL_FAILURE(Both());
    ASSERT_NO_FATAL_FAILURE(SubmitBoth());
    held = rx_controls[1];
    ASSERT_EQ(nx_spi_port_cancel(&ports[1], &requests[1]), NX_SUCCESS);
    STATUS(1U) = TX_EMPTY;
    g_gd32_model_now = 2000000U;
    nx_spi_port_service(&ports[1]);
    EXPECT_EQ(nx_request_state(&requests[1].base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(states[1].active, &requests[1]);
    EXPECT_EQ(nx_request_state(&requests[0].base), NX_REQUEST_ACTIVE);
    EXPECT_NE(*rx_controls[0] & 1U, 0U);
    held = nullptr;
    *rx_controls[1] &= ~1U;
    nx_spi_port_service(&ports[1]);
    EXPECT_EQ(nx_request_state(&requests[1].base), NX_REQUEST_SETTLED);
    EXPECT_EQ(requests[1].base.result, NX_ERROR_CANCELLED);
}
TEST_F(SPIMultiple,
       SecondaryFixedScalarModeKeepsIndependentSpanAndDmaIncrement) {
    ASSERT_NO_FATAL_FAILURE(Both());
    ASSERT_NO_FATAL_FAILURE(
        SubmitBoth(NX_SPI_MEMORY_TX_FIXED | NX_SPI_MEMORY_RX_FIXED));
    EXPECT_EQ(*rx_controls[1] & (1U << 10U), 0U);
    EXPECT_EQ(*tx_controls[1] & (1U << 10U), 0U);
    EXPECT_EQ(*rx_counts[1], sizeof(tx[1]));
    EXPECT_EQ(*tx_counts[1], sizeof(tx[1]));
}
TEST_F(SPIMultiple, IdleForeignPublisherRejectsBeforeControllerAcquisition) {
    NVIC_EnableIRQ(RXIRQ1);
    NVIC_SetPriority(RXIRQ1, 9U);
    *rx_flags[1] = 0x08U << rx_shifts[1];
    EXPECT_EQ(Initialize(1U), NX_ERROR_BUSY);
    EXPECT_FALSE(states[1].initialized);
    EXPECT_EQ(CONTROL(1U), 0U);
    EXPECT_EQ(*rx_flags[1], 0x08U << rx_shifts[1]);
    EXPECT_EQ(NVIC_GetPriority(RXIRQ1), 9U);
    EXPECT_TRUE(Enabled(RXIRQ1));
}
TEST_F(SPIMultiple, DuplicateColdObjectCannotStealLivePhysicalPublishers) {
    ASSERT_NO_FATAL_FAILURE(Both());
    NVIC_SetPriority(RXIRQ1, 9U);
    DMA_STATE duplicate = states[1];
    duplicate.initialized = false;
    DEVICE_STATE endpoint{};
#if defined(NX_DMA_MULTI_STM32)
    (void)endpoint;
    EXPECT_EQ(nx_stm32_spi_dma_initialize(&duplicate), NX_ERROR_BUSY);
#else
    duplicate.spi.initialized = false;
    EXPECT_EQ(nx_gd32_spi_dma_initialize_at(&duplicate, &endpoint,
                                            &nx_gd32_spi0_controller, GPIOC, 4U,
                                            1000000U, 0U, 6U),
              NX_ERROR_BUSY);
#endif
    EXPECT_FALSE(duplicate.initialized);
    EXPECT_TRUE(Enabled(RXIRQ1));
    EXPECT_EQ(NVIC_GetPriority(RXIRQ1), 9U);
}
TEST_F(SPIMultiple, NewReceiveErrorAtDisableCannotBecomeSuccessfulCompletion) {
    ASSERT_NO_FATAL_FAILURE(Both());
    ASSERT_NO_FATAL_FAILURE(SubmitBoth());
    error_control = rx_controls[1];
    error_status = rx_flags[1];
    error_mask = 0x08U << rx_shifts[1];
    Complete(1U);
    STATUS(1U) = TX_EMPTY;
    nx_spi_port_service(&ports[1]);
    ASSERT_EQ(nx_request_state(&requests[1].base), NX_REQUEST_SETTLED);
    EXPECT_EQ(requests[1].base.result, NX_ERROR_IO);
    EXPECT_EQ(requests[1].base.transferred, 0U);
    EXPECT_EQ(states[0].active, &requests[0]);
}
TEST_F(SPIMultiple, NewTransmitErrorBeforeW1cCannotBecomeSuccessfulCompletion) {
    ASSERT_NO_FATAL_FAILURE(Both());
    ASSERT_NO_FATAL_FAILURE(SubmitBoth());
    error_clear = tx_flags[1] + 2;
    error_status = tx_flags[1];
    error_mask = 0x08U << tx_shifts[1];
    Complete(1U);
    STATUS(1U) = TX_EMPTY;
    nx_spi_port_service(&ports[1]);
    ASSERT_EQ(nx_request_state(&requests[1].base), NX_REQUEST_SETTLED);
    EXPECT_EQ(requests[1].base.result, NX_ERROR_IO);
    EXPECT_EQ(requests[1].base.transferred, 0U);
    EXPECT_EQ(states[0].active, &requests[0]);
}
TEST_F(SPIMultiple, ErrorAfterMemoryCompletionIsRecheckedBeforeSettlement) {
    ASSERT_NO_FATAL_FAILURE(Both());
    ASSERT_NO_FATAL_FAILURE(SubmitBoth());
    Complete(1U);
    *rx_flags[1] |= 0x08U << rx_shifts[1];
    STATUS(1U) = TX_EMPTY;
    nx_spi_port_service(&ports[1]);
    ASSERT_EQ(nx_request_state(&requests[1].base), NX_REQUEST_SETTLED);
    EXPECT_EQ(requests[1].base.result, NX_ERROR_IO);
    EXPECT_EQ(requests[1].base.transferred, 0U);
    EXPECT_EQ(states[0].active, &requests[0]);
}
TEST_F(SPIMultiple, SelectedSecondaryStopWithdrawsOnlyItsOwnPublishers) {
    ASSERT_NO_FATAL_FAILURE(Both());
    ASSERT_EQ(nx_spi_port_stop(&ports[1]), NX_SUCCESS);
    EXPECT_TRUE(Enabled(RXIRQ0));
    EXPECT_TRUE(Enabled(TXIRQ0));
    EXPECT_FALSE(Enabled(RXIRQ1));
    EXPECT_FALSE(Enabled(TXIRQ1));
}
TEST_F(SPIMultiple, SecondaryKernelSinkValidatesOnlyActualSelectedDmaVectors) {
    ASSERT_NO_FATAL_FAILURE(Both());
    testing::StrictMock<testing::MockFunction<nx_result_t()>> observer;
    nx_irq_wake_t wake{
        &observer,
        [](void* p) {
            return static_cast<testing::MockFunction<nx_result_t()>*>(p)
                ->Call();
        },
        true};
    NVIC_SetPriority(RXIRQ1, 4U);
    EXPECT_EQ(nx_spi_port_attach_wake(&ports[1], &wake, 5U),
              NX_ERROR_PERMISSION);
    NVIC_SetPriority(RXIRQ1, 6U);
    EXPECT_EQ(nx_spi_port_attach_wake(&ports[1], &wake, 5U), NX_SUCCESS);
    EXPECT_EQ(states[0].wake, nullptr);
}
#if defined(NX_DMA_MULTI_STM32)
TEST_F(SPIMultiple, SecondaryPollingRecoveryPulsesApb1AndPreservesPrimaryWire) {
    STATUS(0U) = 0x81U;
    RCC->APB2RSTR = RCC_APB2RSTR_USART1RST;
    RCC->APB1RSTR = RCC_APB1RSTR_I2C1RST;
    states[1].spi.fault = true;
    STATUS(1U) = ERROR;
    EXPECT_EQ(nx_stm32_spi_recover(&states[1].spi), NX_SUCCESS);
    EXPECT_EQ(resets.size(), 2U);
    if (resets.size() == 2U) {
        EXPECT_EQ(resets[0], &RCC->APB1RSTR);
    }
    EXPECT_EQ(STATUS(0U), 0x81U);
    EXPECT_EQ(RCC->APB2RSTR, RCC_APB2RSTR_USART1RST);
    EXPECT_EQ(RCC->APB1RSTR, RCC_APB1RSTR_I2C1RST);
}
#endif
} /* namespace */

/** \brief W1C only the written selected DMA mask, including shared banks. */
extern "C" void nx_spi_multi_dma_ack(volatile uint32_t* clear, uint32_t mask) {
    if (clear == error_clear) {
        *error_status |= error_mask;
        error_clear = nullptr;
    }
    *clear = mask;
    *(clear - 2) &= ~mask;
}
/** \brief Keep one explicitly injected memory engine enabled on disable. */
extern "C" void nx_spi_multi_dma_disable(volatile uint32_t* control,
                                         uint32_t mask) {
    *control &= ~(control == held ? mask & ~1U : mask);
    if (control == error_control) {
        *error_status |= error_mask;
        error_control = nullptr;
    }
}
/** \brief Record the actual reset register, applying real selected reset
 * effects. */
extern "C" void nx_spi_multi_reset(volatile uint32_t* reset, uint32_t mask,
                                   bool active) {
    resets.push_back(reset);
    if (active) {
        *reset |= mask;
#if defined(NX_DMA_MULTI_STM32)
        SPI_TypeDef* selected = reset == &RCC->APB1RSTR ? SPI2 : SPI1;
        selected->CR1 = 0U;
        selected->CR2 = 0U;
        selected->SR = SPI_SR_TXE;
#endif
    } else {
        *reset &= ~mask;
    }
}
/** \brief No UART publisher is implicitly present in the SPI-only model. */
extern "C" void USART0_IRQHandler(void) {
}
