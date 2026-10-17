/**
 * \file            u575_uart_rtscts_factory_test.cpp
 * \brief           Real generated selected FIFO lifecycle and bounded vectors
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32u575_uart_rtscts.h"
#include "u575_uart_rtscts_factory_model.h"
#include <cstdlib>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>

extern "C" void USART1_IRQHandler(void);
namespace {
constexpr uint32_t kClock = 4000000U;
constexpr unsigned kDepth = 8U;
constexpr uintptr_t kPage = 0x40013000U;
} /* namespace */

class U575UartRtsCtsFactory : public testing::Test {
  protected:
    nx_uart_tx_request_t request{};
    uint8_t payload[6]{1U, 2U, 3U, 4U, 5U, 6U};
    void* mapped = MAP_FAILED;
    const nx_uart_port_t* Port() {
        return nx_factory_uart(NX_UART_ID_CONSOLE);
    }
    nx_stm32u575_uart_rtscts_state_t* State() {
        return static_cast<nx_stm32u575_uart_rtscts_state_t*>(Port()->context);
    }
    void SetUp() override {
        mapped =
            mmap(reinterpret_cast<void*>(kPage), 4096U, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(reinterpret_cast<void*>(kPage), mapped);
        nx_u575_uart_rtscts_model_reset();
        nx_request_initialize(&request.base);
    }
    void TearDown() override {
        if (mapped == MAP_FAILED) {
            return;
        }
        g_u575_uart_rtscts.before_read = nullptr;
        g_u575_uart_rtscts.before_write = nullptr;
        g_u575_uart_rtscts.reject_clear = g_u575_uart_rtscts.reject_set = 0U;
        g_u575_uart_rtscts.reject_register = nullptr;
        g_u575_uart_rtscts.reject_bits_register = nullptr;
        g_u575_uart_rtscts.hold_pending = g_u575_uart_rtscts.hold_priority =
            false;
        g_u575_model.hold_uart_on = g_u575_model.hold_uart_off = false;
        g_u575_model.hold_irq_on = g_u575_model.hold_irq_off = false;
        g_u575_model.ignore_write = nullptr;
        g_u575_model.privileged = true;
        g_u575_model.irq_active[USART1_IRQn] = false;
        g_u575_model.irq_active[TIM2_IRQn] = false;
        if (State()->uart.active != nullptr) {
            (void)nx_uart_port_cancel(Port(), &request);
        }
        nx_u575_uart_rtscts_model_cts(false);
        bool joined = false;
        for (unsigned i = 0U; i < 64U; ++i) {
            Wire(kDepth + 1U);
            nx_uart_port_service(Port());
            if (nx_platform_stop() == NX_SUCCESS) {
                joined = true;
                break;
            }
        }
        if (!joined || State()->uart.retained ||
            State()->uart.active != nullptr) {
            std::abort();
        }
        EXPECT_EQ(0, munmap(mapped, 4096U));
    }
    void Start() {
        const auto result = nx_platform_start();
        ASSERT_EQ(NX_SUCCESS, result.primary);
        ASSERT_EQ(NX_SUCCESS, result.cleanup);
        ASSERT_EQ(0U, result.remaining_effects);
        TIM2->CNT = 100U;
    }
    void Vector() {
        g_u575_model.irq_pending[USART1_IRQn] = false;
        g_u575_model.irq_active[USART1_IRQn] = true;
        USART1_IRQHandler();
        g_u575_model.irq_active[USART1_IRQn] = false;
    }
    void Wire(unsigned count) {
        for (unsigned i = 0U; i < count; ++i) {
            nx_u575_uart_rtscts_model_shift();
            nx_u575_uart_rtscts_model_finish_frame();
        }
    }
    void Submit() {
        ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request, payload,
                                                 sizeof(payload), 10000U));
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(Port(), &request));
    }
};

TEST_F(U575UartRtsCtsFactory, TypedLookupHasNoHardwareOrInitializationEffects) {
    const auto* face = Port();
    ASSERT_NE(nullptr, face);
    EXPECT_EQ(face, Port());
    EXPECT_EQ(nullptr, nx_factory_uart(NX_UART_ID_COUNT));
    EXPECT_EQ(0U, g_u575_model.reads);
    EXPECT_EQ(0U, g_u575_model.writes);
    EXPECT_FALSE(State()->uart.initialized);
    EXPECT_FALSE(State()->uart.retained);
    ASSERT_NO_FATAL_FAILURE(Start());
    EXPECT_EQ(face, Port());
    EXPECT_EQ(&nx_stm32u575_uart_rtscts_ops, face->ops);
}

TEST_F(U575UartRtsCtsFactory, NormalTomlBindsExactProfileClockAndReviewedPins) {
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_NE(nullptr, State()->fifo);
    EXPECT_EQ(2U, State()->fifo->rx_threshold);
    EXPECT_EQ(2U, State()->fifo->tx_threshold);
    EXPECT_EQ(2U, State()->fifo->rx_budget);
    EXPECT_EQ(3U, State()->fifo->tx_budget);
    EXPECT_EQ(kClock, g_nx_stm32u575_system.core_hz);
    EXPECT_EQ(3U, TIM2->PSC);
    EXPECT_EQ(2U, (GPIOA->MODER >> 18U) & 3U);
    EXPECT_EQ(2U, (GPIOA->MODER >> 20U) & 3U);
    EXPECT_EQ(7U, (GPIOA->AFR[1] >> 4U) & 15U);
    EXPECT_EQ(7U, (GPIOA->AFR[1] >> 8U) & 15U);
    EXPECT_NE(0U, USART1_NS->CR1 & USART_CR1_FIFOEN);
    EXPECT_EQ(USART_CR3_RTSE | USART_CR3_CTSE,
              USART1_NS->CR3 & (USART_CR3_RTSE | USART_CR3_CTSE));
    EXPECT_EQ(2U, (GPIOA->MODER >> 22U) & 3U);
    EXPECT_EQ(2U, (GPIOA->MODER >> 24U) & 3U);
    EXPECT_EQ(7U, (GPIOA->AFR[1] >> 12U) & 15U);
    EXPECT_EQ(7U, (GPIOA->AFR[1] >> 16U) & 15U);
    EXPECT_EQ(1U, (GPIOA->PUPDR >> 22U) & 3U);

    EXPECT_EQ(2U,
              (USART1_NS->CR3 & USART_CR3_RXFTCFG) >> USART_CR3_RXFTCFG_Pos);
    EXPECT_EQ(2U,
              (USART1_NS->CR3 & USART_CR3_TXFTCFG) >> USART_CR3_TXFTCFG_Pos);
    EXPECT_EQ(0U, USART1_NS->CR3 & (USART_CR3_DMAT | USART_CR3_DMAR));
    EXPECT_TRUE(g_u575_model.irq_enabled[USART1_IRQn]);
    EXPECT_EQ(5U, g_u575_model.priority[USART1_IRQn]);
}

TEST_F(U575UartRtsCtsFactory, SelectedVectorPerformsTheAuthoredBatchBudget) {
    ASSERT_NO_FATAL_FAILURE(Start());
    for (unsigned i = 0U; i < kDepth / 2U + 1U; ++i) {
        nx_u575_uart_rtscts_model_receive(static_cast<uint8_t>(i), 0U);
    }
    Vector();
    EXPECT_EQ(2U, g_u575_uart_rtscts.rdr_reads);
    EXPECT_EQ(2U, State()->uart.rx_count);
    EXPECT_NE(0U, USART1_NS->CR1 & USART_CR1_RXNEIE);
    for (unsigned i = 0U; i < kDepth; ++i) {
        Vector();
    }
    uint8_t output[kDepth]{};
    size_t count = 0U;
    EXPECT_EQ(NX_SUCCESS,
              nx_uart_port_read_bytes(Port(), output, sizeof(output), &count));
    ASSERT_EQ(kDepth / 2U + 1U, count);
    for (unsigned i = 0U; i < count; ++i) {
        EXPECT_EQ(i, output[i]);
    }
    EXPECT_EQ(0U, g_u575_uart_rtscts.empty_reads);
}

TEST_F(U575UartRtsCtsFactory,
       IdleTailContinuesWithoutFreshArrivalOrSecondIdle) {
    ASSERT_NO_FATAL_FAILURE(Start());
    nx_u575_uart_rtscts_model_receive(0x11U, 0U);
    nx_u575_uart_rtscts_model_receive(0x22U, 0U);
    nx_u575_uart_rtscts_model_receive(0x33U, 0U);
    nx_u575_uart_rtscts_model_idle();
    Vector();
    EXPECT_EQ(2U, State()->uart.rx_count);
    EXPECT_NE(0U, USART1_NS->CR1 & USART_CR1_RXNEIE);
    EXPECT_EQ(0U, USART1_NS->ISR & USART_ISR_IDLE);
    Vector();
    EXPECT_EQ(3U, State()->uart.rx_count);
    EXPECT_EQ(0U, USART1_NS->CR1 & USART_CR1_RXNEIE);
    EXPECT_EQ(3U, g_u575_uart_rtscts.rdr_reads);
}

TEST_F(U575UartRtsCtsFactory, TxBudgetAndWireTcUseTheSameGeneratedOwner) {
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_NO_FATAL_FAILURE(Submit());
    Vector();
    EXPECT_EQ(3U, g_u575_uart_rtscts.tdr_writes);
    EXPECT_EQ(&request, State()->uart.active);
    Wire(3U);
    Vector();
    EXPECT_EQ(6U, g_u575_uart_rtscts.tdr_writes);
    nx_uart_port_service(Port());
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    Wire(3U);
    Vector();
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    nx_uart_port_service(Port());
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(sizeof(payload), request.base.transferred);
}

TEST_F(U575UartRtsCtsFactory,
       CancelRetainsStaticRequestUntilFifoAndShifterEmpty) {
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_NO_FATAL_FAILURE(Submit());
    Vector();
    nx_u575_uart_rtscts_model_shift();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_cancel(Port(), &request));
    const auto writes = g_u575_uart_rtscts.tdr_writes;
    Vector();
    EXPECT_EQ(writes, g_u575_uart_rtscts.tdr_writes);
    nx_uart_port_service(Port());
    EXPECT_EQ(&request, State()->uart.active);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    nx_u575_uart_rtscts_model_finish_frame();
    Wire(kDepth);
    nx_uart_port_service(Port());
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_EQ(writes, request.base.transferred);
}

TEST_F(U575UartRtsCtsFactory,
       SelectedForeignVectorRejectsBeforeAnyColdEffects) {
    for (bool* fact : {&g_u575_model.irq_enabled[USART1_IRQn],
                       &g_u575_model.irq_pending[USART1_IRQn],
                       &g_u575_model.irq_active[USART1_IRQn]}) {
        *fact = true;
        const auto result = nx_platform_start();
        EXPECT_EQ(NX_ERROR_BUSY, result.primary);
        EXPECT_EQ(NX_SUCCESS, result.cleanup);
        EXPECT_EQ(0U, result.remaining_effects);
        EXPECT_EQ(0U, g_u575_model.writes);
        EXPECT_TRUE(*fact);
        *fact = false;
    }
}

TEST_F(U575UartRtsCtsFactory,
       FailedColdReadbackRetainsPinsUntilVerifiedFifoClear) {
    g_u575_uart_rtscts.reject_register = &USART1_NS->CR3;
    g_u575_uart_rtscts.reject_clear = USART_CR1_FIFOEN;
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_NE(NX_SUCCESS, result.cleanup);
    EXPECT_NE(0U, result.remaining_effects);
    EXPECT_TRUE(State()->uart.retained);
    EXPECT_FALSE(State()->uart.initialized);
    EXPECT_NE(0U, RCC->APB2ENR & RCC_APB2ENR_USART1EN);
    EXPECT_EQ(2U, (GPIOA->MODER >> 18U) & 3U);
    g_u575_uart_rtscts.reject_register = nullptr;
    g_u575_uart_rtscts.reject_clear = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_FALSE(State()->uart.retained);
    EXPECT_EQ(0U, RCC->APB2ENR & RCC_APB2ENR_USART1EN);
}

TEST_F(U575UartRtsCtsFactory, JoinedStopAndRestartPreservePureFaceIdentity) {
    const auto* face = Port();
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_FALSE(State()->uart.retained);
    ASSERT_NO_FATAL_FAILURE(Start());
    EXPECT_EQ(face, Port());
    EXPECT_EQ(&nx_stm32u575_uart_rtscts_ops, Port()->ops);
    EXPECT_EQ(0U, g_u575_uart_rtscts.forbidden_config);
    EXPECT_EQ(0U, g_u575_uart_rtscts.full_writes);
}

TEST_F(U575UartRtsCtsFactory,
       FullNormalRingPausesThenPublicReadRearmsTheSelectedVector) {
    ASSERT_NO_FATAL_FAILURE(Start());
    for (unsigned i = 0U; i < 64U; i += 2U) {
        nx_u575_uart_rtscts_model_receive(static_cast<uint8_t>(i), 0U);
        nx_u575_uart_rtscts_model_receive(static_cast<uint8_t>(i + 1U), 0U);
        Vector();
    }
    ASSERT_EQ(64U, State()->uart.rx_count);
    nx_u575_uart_rtscts_model_receive(0xeeU, 0U);
    const uint32_t reads = g_u575_uart_rtscts.rdr_reads;
    Vector();
    EXPECT_EQ(reads, g_u575_uart_rtscts.rdr_reads);
    uint8_t output = 0xffU;
    size_t count = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_read_bytes(Port(), &output, 1U, &count));
    EXPECT_EQ(0U, output);
    EXPECT_EQ(1U, count);
    Vector();
    EXPECT_EQ(64U, State()->uart.rx_count);
    EXPECT_EQ(0U, State()->uart.rx_loss);
    EXPECT_EQ(0U, g_u575_uart_rtscts.received);
}
TEST_F(U575UartRtsCtsFactory,
       CtsBlockedPlatformStopCannotRestorePinsOrRevokeTheBorrow) {
    ASSERT_NO_FATAL_FAILURE(Start());
    nx_u575_uart_rtscts_model_cts(true);
    ASSERT_NO_FATAL_FAILURE(Submit());
    Vector();
    ASSERT_EQ(3U, g_u575_uart_rtscts.queued);
    EXPECT_EQ(NX_ERROR_BUSY, nx_platform_stop());
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_TRUE(State()->uart.retained);
    EXPECT_EQ(7U, (GPIOA->AFR[1] >> 12U) & 15U);
    EXPECT_EQ(7U, (GPIOA->AFR[1] >> 16U) & 15U);
    nx_u575_uart_rtscts_model_cts(false);
    Wire(3U);
    nx_uart_port_service(Port());
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
}
