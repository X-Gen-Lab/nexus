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
}
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>

extern "C" {
extern uint64_t g_f4_model_now;
extern bool g_f4_model_isr;
extern uint32_t g_f4_model_mask;
}
namespace {
volatile uint32_t* held;
class UARTMultiple : public testing::Test {
  protected:
    DMA_STATE states[2]{};
    nx_stm32_uart_dma_resources_t resources[2]{};
    nx_uart_port_t ports[2]{{&DMA_OPS, &states[0]}, {&DMA_OPS, &states[1]}};
    nx_uart_tx_request_t requests[2]{};
    uint8_t payload[2][4]{{1U, 2U, 3U, 4U}, {5U, 6U, 7U, 8U}};
    uint8_t rings[2][8]{};
    nx_dma_memory_region_t regions[1]{};
    USART_TypeDef* uarts[2]{UART0, UART1};
    volatile uint32_t* controls[2]{&DMA2_Stream7->CR, &DMA1_Stream6->CR};
    volatile uint32_t* counts[2]{&DMA2_Stream7->NDTR, &DMA1_Stream6->NDTR};
    volatile uint32_t* flags[2]{&DMA2->HISR, &DMA1->HISR};
    volatile uint32_t* destinations[2]{&DMA2_Stream7->PAR, &DMA1_Stream6->PAR};
    unsigned shifts[2]{22U, 16U};
    nx_result_t Initialize(unsigned i) {
        return nx_stm32_uart_dma_initialize(&states[i],
                                            i == 0U ? 84000000U : 42000000U);
    }
    void SetUp() override {
        void* p = mmap(
            (void*)(uintptr_t)0x40000000U, 0x80000U, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(p, (void*)(uintptr_t)0x40000000U);
        g_f4_model_nvic = {};
        g_f4_model_now = 100U;
        g_f4_model_isr = false;
        g_f4_model_mask = 0U;
        held = nullptr;
        regions[0] = {(uintptr_t)payload, sizeof(payload), NX_DMA_MEMORY_READ};
        for (unsigned i = 0U; i < 2U; ++i) {
            states[i].resources = &resources[i];
            resources[i].regions = regions;
            resources[i].region_count = 1U;
            states[i].uart.registers = uarts[i];
            states[i].uart.baud = 115200U;
            states[i].uart.profile = NX_UART_RX_BYTES;
            states[i].uart.rx_storage = rings[i];
            states[i].uart.rx_capacity = sizeof(rings[i]);
            states[i].uart.irq = i == 0U ? UIRQ0 : UIRQ1;
            resources[i].transmit =
                i == 0U ? nx_stm32_dma_route_t{DMA2, DMA2_Stream7,
                                               DMA2_Stream7_IRQn, 7U, 4U}
                        : nx_stm32_dma_route_t{DMA1, DMA1_Stream6,
                                               DMA1_Stream6_IRQn, 6U, 4U};
            nx_request_initialize(&requests[i].base);
            ASSERT_EQ(nx_uart_tx_prepare(&requests[i], payload[i],
                                         sizeof(payload[i]), 1000000U),
                      NX_SUCCESS);
        }
    }
    void TearDown() override {
        held = nullptr;
        g_f4_model_isr = false;
        g_f4_model_mask = 0U;
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

TEST_F(UARTMultiple, AlternateTxRouteUsesSelectedChannelPublisherAndWire) {
    uarts[1] = USART6;
    states[1].uart.registers = USART6;
    states[1].uart.irq = USART6_IRQn;
    resources[1].transmit = {DMA2, DMA2_Stream7, DMA2_Stream7_IRQn, 7U, 5U};
    ASSERT_EQ(NX_SUCCESS, nx_stm32_uart_dma_initialize(&states[1], 84000000U));
    EXPECT_TRUE(Enabled(DMA2_Stream7_IRQn));
    EXPECT_FALSE(Enabled(DMA2_Stream6_IRQn));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&ports[1], &requests[1]));
    EXPECT_EQ(5U << DMA_SxCR_CHSEL_Pos, DMA2_Stream7->CR & DMA_SxCR_CHSEL);
    DMA2_Stream7->NDTR = 0U;
    DMA2->HISR = 0x20U << 22U;
    DMA_IRQ(&states[1]);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&requests[1].base));
    STATUS(1U) = TC;
    UART_IRQ(&states[1]);
    nx_uart_port_service(&ports[1]);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&requests[1].base));
    EXPECT_EQ(NX_SUCCESS, requests[1].base.result);
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_stop(&ports[1]));
    EXPECT_FALSE(Enabled(DMA2_Stream7_IRQn));
}

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
    constexpr uint32_t peripheral_clock = 42000000U;
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
    g_f4_model_now = 2000000U;
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
    g_f4_model_isr = true;
    EXPECT_EQ(Initialize(1U), NX_ERROR_CONTEXT);
    EXPECT_FALSE(Enabled(TXIRQ1));
    g_f4_model_isr = false;
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
    resources[1].transmit.stream = DMA2_Stream7;
    EXPECT_EQ(Initialize(1U), NX_ERROR_UNSUPPORTED);
    resources[1].transmit.stream = DMA1_Stream6;
    EXPECT_EQ(nx_stm32_uart_dma_initialize(&states[1], 84000000U),
              NX_ERROR_UNSUPPORTED);
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
