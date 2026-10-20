/**
 * \file            uart_multi_controller_test.cpp
 * \brief           Independent finite TX DMA controllers, IRQ RX and drain
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
extern "C" {
#if defined(NX_DMA_MULTI_STM32)
#include "stm32f407_dma.h"
#define DMA_STATE nx_stm32_uart_dma_state_t
#define DMA_OPS   nx_stm32_uart_dma_ops
#define DMA_IRQ   nx_stm32_uart_dma_irq
#define UART_IRQ  nx_stm32_uart_dma_uart_irq
#define UART0     USART1
#define UART1     USART2
#define TXIRQ0    DMA2_Stream7_IRQn
#define TXIRQ1    DMA1_Stream6_IRQn
#define UIRQ0     USART1_IRQn
#define UIRQ1     USART2_IRQn
#define STATUS(i) (uarts[i]->SR)
#define BAUD(i)   (uarts[i]->BRR)
#define DATA(i)   (uarts[i]->DR)
#define TC        USART_SR_TC
#define RX_READY  USART_SR_RXNE
#else
#include "gd32f470_dma.h"
#include "gd32f4xx_dma.h"
#define DMA_STATE nx_gd32_uart_dma_state_t
#define DMA_OPS   nx_gd32_uart_dma_ops
#define DMA_IRQ   nx_gd32_uart_dma_irq
#define UART_IRQ  nx_gd32_uart_dma_uart_irq
#define UART0     USART0
#define UART1     USART1
#define TXIRQ0    DMA1_Channel7_IRQn
#define TXIRQ1    DMA0_Channel6_IRQn
#define UIRQ0     USART0_IRQn
#define UIRQ1     USART1_IRQn
#define STATUS(i) USART_STAT0(uarts[i])
#define BAUD(i)   USART_BAUD(uarts[i])
#define DATA(i)   USART_DATA(uarts[i])
#define TC        USART_STAT0_TC
#define RX_READY  USART_STAT0_RBNE
#endif
}
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>

extern "C" {
extern uint64_t g_gd32_model_now;
extern bool g_gd32_model_isr;
extern uint32_t g_gd32_model_mask;
}
namespace {
volatile uint32_t* held;
class UARTMultiple : public testing::Test {
  protected:
    DMA_STATE states[2]{};
    nx_uart_port_t ports[2]{{&DMA_OPS, &states[0]}, {&DMA_OPS, &states[1]}};
    nx_uart_tx_request_t requests[2]{};
    uint8_t payload[2][4]{{1U, 2U, 3U, 4U}, {5U, 6U, 7U, 8U}};
    uint8_t rings[2][8]{};
    nx_dma_memory_region_t regions[1]{};
#if defined(NX_DMA_MULTI_STM32)
    USART_TypeDef* uarts[2]{UART0, UART1};
    volatile uint32_t* controls[2]{&DMA2_Stream7->CR, &DMA1_Stream6->CR};
    volatile uint32_t* counts[2]{&DMA2_Stream7->NDTR, &DMA1_Stream6->NDTR};
    volatile uint32_t* flags[2]{&DMA2->HISR, &DMA1->HISR};
    volatile uint32_t* destinations[2]{&DMA2_Stream7->PAR, &DMA1_Stream6->PAR};
#else
    uint32_t uarts[2]{UART0, UART1};
    volatile uint32_t* controls[2]{&DMA_CH7CTL(DMA1), &DMA_CH6CTL(DMA0)};
    volatile uint32_t* counts[2]{&DMA_CH7CNT(DMA1), &DMA_CH6CNT(DMA0)};
    volatile uint32_t* flags[2]{&DMA_INTF1(DMA1), &DMA_INTF1(DMA0)};
    volatile uint32_t* destinations[2]{&DMA_CH7PADDR(DMA1),
                                       &DMA_CH6PADDR(DMA0)};
#endif
    unsigned shifts[2]{22U, 16U};
    nx_result_t Initialize(unsigned i) {
#if defined(NX_DMA_MULTI_STM32)
        return nx_stm32_uart_dma_initialize(&states[i],
                                            i == 0U ? 84000000U : 42000000U);
#else
        return nx_gd32_uart_dma_initialize_at(
            &states[i],
            i == 0U ? &nx_gd32_usart0_controller : &nx_gd32_usart1_controller,
            115200U, NX_UART_RX_BYTES, rings[i], sizeof(rings[i]), 6U);
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
        regions[0] = {(uintptr_t)payload, sizeof(payload), NX_DMA_MEMORY_READ};
        for (unsigned i = 0U; i < 2U; ++i) {
            states[i].resources.regions = regions;
            states[i].resources.region_count = 1U;
#if defined(NX_DMA_MULTI_STM32)
            states[i].uart.registers = uarts[i];
            states[i].uart.baud = 115200U;
            states[i].uart.profile = NX_UART_RX_BYTES;
            states[i].uart.rx_storage = rings[i];
            states[i].uart.rx_capacity = sizeof(rings[i]);
            states[i].uart.irq = i == 0U ? UIRQ0 : UIRQ1;
            states[i].resources.dma = i == 0U ? DMA2 : DMA1;
            states[i].transmit.tx = i == 0U ? DMA2_Stream7 : DMA1_Stream6;
            states[i].transmit.stream = i == 0U ? 7U : 6U;
            states[i].transmit.channel = 4U;
            states[i].transmit.dma_irq = i == 0U ? TXIRQ0 : TXIRQ1;
#endif
            nx_request_initialize(&requests[i].base);
            ASSERT_EQ(nx_uart_tx_prepare(&requests[i], payload[i],
                                         sizeof(payload[i]), 1000000U),
                      NX_SUCCESS);
        }
    }
    void TearDown() override {
        held = nullptr;
        g_gd32_model_isr = false;
        g_gd32_model_mask = 0U;
        for (unsigned i = 0U; i < 2U; ++i) {
            if (states[i].uart.initialized) {
                if (states[i].uart.active != nullptr) {
                    EXPECT_EQ(nx_uart_port_cancel(&ports[i], &requests[i]),
                              NX_SUCCESS);
                    STATUS(i) |= TC;
                    UART_IRQ(&states[i]);
                    nx_uart_port_service(&ports[i]);
                }
                EXPECT_EQ(nx_uart_port_stop(&ports[i]), NX_SUCCESS);
            }
        }
        EXPECT_EQ(munmap((void*)(uintptr_t)0x40000000U, 0x80000U), 0);
    }
    void Both() {
        ASSERT_EQ(Initialize(0U), NX_SUCCESS);
        ASSERT_EQ(Initialize(1U), NX_SUCCESS);
    }
    void SubmitBoth() {
        for (unsigned i = 0U; i < 2U; ++i)
            ASSERT_EQ(nx_uart_port_submit(&ports[i], &requests[i]), NX_SUCCESS);
    }
    bool Enabled(IRQn_Type irq) const {
        return (NVIC->ISER[(unsigned)irq / 32U] &
                (1U << ((unsigned)irq % 32U))) != 0U;
    }
};

TEST_F(UARTMultiple, SecondaryColdInitPreservesPrimaryRegistersAndIrqs) {
    ASSERT_EQ(Initialize(0U), NX_SUCCESS);
    STATUS(0U) = 0x123U;
    *controls[0] = 0x800U;
    NVIC_SetPriority(TXIRQ0, 9U);
    ASSERT_EQ(Initialize(1U), NX_SUCCESS);
    EXPECT_EQ(STATUS(0U), 0x123U);
    EXPECT_EQ(*controls[0], 0x800U);
    EXPECT_EQ(NVIC_GetPriority(TXIRQ0), 9U);
    EXPECT_TRUE(Enabled(TXIRQ0));
    EXPECT_TRUE(Enabled(TXIRQ1));
    EXPECT_TRUE(Enabled(UIRQ0));
    EXPECT_TRUE(Enabled(UIRQ1));
#if defined(NX_DMA_MULTI_STM32)
    constexpr uint32_t peripheral_clock = 42000000U;
#else
    constexpr uint32_t peripheral_clock = 50000000U;
#endif
    EXPECT_EQ(BAUD(1U), (peripheral_clock + 57600U) / 115200U);
}
TEST_F(UARTMultiple, SeparateRequestsRequireTheirOwnDmaAndWireCompletion) {
    ASSERT_NO_FATAL_FAILURE(Both());
    ASSERT_NO_FATAL_FAILURE(SubmitBoth());
    EXPECT_EQ(*destinations[0], (uint32_t)(uintptr_t)&DATA(0U));
    EXPECT_EQ(*destinations[1], (uint32_t)(uintptr_t)&DATA(1U));
    *counts[0] = 0U;
    *flags[0] = 0x20U << shifts[0];
    *flags[1] = 0x20U << shifts[1];
    DMA_IRQ(&states[0]);
    EXPECT_EQ(*flags[1], 0x20U << shifts[1]);
    EXPECT_NE(*controls[1] & 1U, 0U);
    nx_uart_port_service(&ports[0]);
    EXPECT_EQ(states[0].uart.active, &requests[0]);
    STATUS(0U) |= TC;
    UART_IRQ(&states[0]);
    nx_uart_port_service(&ports[0]);
    EXPECT_EQ(nx_request_state(&requests[0].base), NX_REQUEST_SETTLED);
    EXPECT_EQ(requests[0].base.result, NX_SUCCESS);
    EXPECT_EQ(requests[0].base.transferred, sizeof(payload[0]));
    EXPECT_EQ(nx_request_state(&requests[1].base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(states[1].uart.active, &requests[1]);
}
TEST_F(UARTMultiple, FailedSecondaryDrainRetainsOnlyItsExactBorrow) {
    ASSERT_NO_FATAL_FAILURE(Both());
    ASSERT_NO_FATAL_FAILURE(SubmitBoth());
    *counts[1] = 1U;
    held = controls[1];
    ASSERT_EQ(nx_uart_port_cancel(&ports[1], &requests[1]), NX_SUCCESS);
    STATUS(1U) |= TC;
    UART_IRQ(&states[1]);
    g_gd32_model_now = 2000000U;
    nx_uart_port_service(&ports[1]);
    EXPECT_EQ(nx_request_state(&requests[1].base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(states[1].uart.active, &requests[1]);
    EXPECT_EQ(nx_request_state(&requests[0].base), NX_REQUEST_ACTIVE);
    EXPECT_NE(*controls[0] & 1U, 0U);
    held = nullptr;
    *controls[1] &= ~1U;
    nx_uart_port_service(&ports[1]);
    EXPECT_EQ(nx_request_state(&requests[1].base), NX_REQUEST_SETTLED);
    EXPECT_EQ(requests[1].base.result, NX_ERROR_CANCELLED);
}
TEST_F(UARTMultiple, SecondaryStopPreservesPrimaryPublishersAndClock) {
    ASSERT_NO_FATAL_FAILURE(Both());
    NVIC->ISPR[(unsigned)TXIRQ0 / 32U] |= 1U << ((unsigned)TXIRQ0 % 32U);
    NVIC->ISPR[(unsigned)TXIRQ1 / 32U] |= 1U << ((unsigned)TXIRQ1 % 32U);
    ASSERT_EQ(nx_uart_port_stop(&ports[1]), NX_SUCCESS);
    EXPECT_TRUE(Enabled(UIRQ0));
    EXPECT_TRUE(Enabled(TXIRQ0));
    EXPECT_FALSE(Enabled(UIRQ1));
    EXPECT_FALSE(Enabled(TXIRQ1));
    EXPECT_NE(NVIC->ISPR[(unsigned)TXIRQ0 / 32U] &
                  (1U << ((unsigned)TXIRQ0 % 32U)),
              0U);
    EXPECT_EQ(NVIC->ISPR[(unsigned)TXIRQ1 / 32U] &
                  (1U << ((unsigned)TXIRQ1 % 32U)),
              0U);
#if !defined(NX_DMA_MULTI_STM32)
    EXPECT_NE(RCU_APB2EN & RCU_APB2EN_USART0EN, 0U);
    EXPECT_EQ(RCU_APB1EN & RCU_APB1EN_USART1EN, 0U);
#endif
}
TEST_F(UARTMultiple, SecondaryIrqReceiveCannotWritePrimaryRing) {
    ASSERT_NO_FATAL_FAILURE(Both());
    STATUS(1U) = RX_READY;
    DATA(1U) = 0x62U;
    UART_IRQ(&states[1]);
    uint8_t byte = 0U;
    size_t count = 0U;
    ASSERT_EQ(nx_uart_port_read_bytes(&ports[1], &byte, 1U, &count),
              NX_SUCCESS);
    EXPECT_EQ(count, 1U);
    EXPECT_EQ(byte, 0x62U);
    ASSERT_EQ(nx_uart_port_read_bytes(&ports[0], &byte, 1U, &count),
              NX_ERROR_EMPTY);
    EXPECT_EQ(count, 0U);
}
TEST_F(UARTMultiple, SelectedPublisherCeilingRejectsUnsafeSecondaryVector) {
    ASSERT_NO_FATAL_FAILURE(Both());
    testing::StrictMock<testing::MockFunction<nx_result_t()>> observer;
    nx_irq_wake_t wake{
        &observer,
        [](void* p) {
            return static_cast<testing::MockFunction<nx_result_t()>*>(p)
                ->Call();
        },
        true};
    NVIC_SetPriority(TXIRQ1, 4U);
    EXPECT_EQ(nx_uart_port_attach_wake(&ports[1], &wake, 5U),
              NX_ERROR_PERMISSION);
    EXPECT_EQ(states[1].uart.wake, nullptr);
    NVIC_SetPriority(TXIRQ1, 6U);
    EXPECT_EQ(nx_uart_port_attach_wake(&ports[1], &wake, 5U), NX_SUCCESS);
    EXPECT_EQ(states[0].uart.wake, nullptr);
}
TEST_F(UARTMultiple, LivePhysicalControllerCannotBeAcquiredTwice) {
    ASSERT_EQ(Initialize(1U), NX_SUCCESS);
    DMA_STATE copy = states[1];
    copy.uart.initialized = false;
    states[1] = copy;
    const uint32_t divisor = BAUD(1U);
    NVIC_SetPriority(TXIRQ1, 9U);
    EXPECT_EQ(Initialize(1U), NX_ERROR_BUSY);
    EXPECT_EQ(BAUD(1U), divisor);
    EXPECT_EQ(NVIC_GetPriority(TXIRQ1), 9U);
    states[1].uart.initialized = true;
}
TEST_F(UARTMultiple, ContextAndSharedEngineConflictRejectBeforeAcquisition) {
    g_gd32_model_isr = true;
    EXPECT_EQ(Initialize(1U), NX_ERROR_CONTEXT);
    EXPECT_FALSE(Enabled(TXIRQ1));
    g_gd32_model_isr = false;
    *controls[1] = 1U;
    EXPECT_EQ(Initialize(1U), NX_ERROR_BUSY);
    EXPECT_FALSE(states[1].uart.initialized);
    EXPECT_FALSE(Enabled(TXIRQ1));
    *controls[1] = 0U;
}
TEST_F(UARTMultiple, IdleForeignPublisherRejectsBeforeClockOrFlagMutation) {
    NVIC_EnableIRQ(TXIRQ1);
    NVIC_SetPriority(TXIRQ1, 9U);
    *flags[1] = 0x08U << 16U;
    EXPECT_EQ(Initialize(1U), NX_ERROR_BUSY);
    EXPECT_FALSE(states[1].uart.initialized);
    EXPECT_EQ(BAUD(1U), 0U);
    EXPECT_EQ(*flags[1], 0x08U << 16U);
    EXPECT_EQ(NVIC_GetPriority(TXIRQ1), 9U);
    EXPECT_TRUE(Enabled(TXIRQ1));
}
TEST_F(UARTMultiple, SecondaryCannotAcquireForeignColdControllerFacts) {
#if defined(NX_DMA_MULTI_STM32)
    states[1].transmit.tx = DMA2_Stream7;
    EXPECT_EQ(Initialize(1U), NX_ERROR_UNSUPPORTED);
    states[1].transmit.tx = DMA1_Stream6;
    EXPECT_EQ(nx_stm32_uart_dma_initialize(&states[1], 84000000U),
              NX_ERROR_UNSUPPORTED);
#else
    const nx_gd32_uart_controller_t copied = nx_gd32_usart1_controller;
    EXPECT_EQ(nx_gd32_uart_dma_initialize_at(&states[1], &copied, 115200U,
                                             NX_UART_RX_BYTES, rings[1],
                                             sizeof(rings[1]), 6U),
              NX_ERROR_INVALID);
#endif
    EXPECT_FALSE(states[1].uart.initialized);
    EXPECT_FALSE(Enabled(TXIRQ0));
    EXPECT_FALSE(Enabled(TXIRQ1));
    EXPECT_EQ(BAUD(1U), 0U);
}
TEST_F(UARTMultiple, SecondaryAcknowledgesOnlyItsOwnHighStatusGroup) {
    ASSERT_NO_FATAL_FAILURE(Both());
    *flags[1] = (0x20U << 16U) | (0x20U << 22U);
    DMA_IRQ(&states[1]);
    EXPECT_EQ(*flags[1], 0x20U << 22U);
    EXPECT_TRUE(Enabled(TXIRQ0));
}
} /* namespace */

/** \brief Actual DMA W1C layout: status is two words before its clear register.
 */
extern "C" void nx_uart_multi_dma_ack(volatile uint32_t* clear, uint32_t mask) {
    *clear = mask;
    *(clear - 2) &= ~mask;
}
/** \brief Refused EN readback retains only the explicitly injected engine. */
extern "C" void nx_uart_multi_dma_disable(volatile uint32_t* control,
                                          uint32_t mask) {
    *control &= ~(control == held ? mask & ~1U : mask);
}
/** \brief This fixture dispatches each UART through its explicit retained
 * state. */
extern "C" void USART0_IRQHandler(void) {
}
