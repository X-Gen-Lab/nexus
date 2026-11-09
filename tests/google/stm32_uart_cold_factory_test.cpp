/**
 * \file            stm32_uart_cold_factory_test.cpp
 * \brief           All maintained UART cold wrappers through actual factories
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/io/uart_word.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32f407_provider.h"
#include "stm32f407_uart_cold.h"
#ifdef NX_COLD_HAL
#include "stm32f407_uart_hal.h"
#endif
nx_stm32_system_t g_nx_stm32_system;
#ifdef NX_COLD_HAL
alignas(16) uint8_t g_nx_stm32_scs_model[0x2000U] = {};
#else
NVIC_Type g_f4_model_nvic;
SCB_Type g_f4_model_scb;
SysTick_Type g_f4_model_systick;
#endif
uint32_t g_f4_model_mask;
bool g_f4_model_isr;
}
#include <array>
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <vector>

namespace {
bool privileged;
volatile uint32_t* refused;
uint32_t held_bits;
int refused_priority;
unsigned starts;
unsigned writes;
nx_time_us_t now;
constexpr std::array<IRQn_Type, 1U + NX_COLD_RX + NX_COLD_TX> vectors {
    USART1_IRQn,
#if NX_COLD_RX
        DMA2_Stream2_IRQn,
#endif
#if NX_COLD_TX
        DMA2_Stream7_IRQn,
#endif
};
constexpr uint32_t original_ahb1 = RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_DMA1EN;
constexpr uint32_t original_apb1 = RCC_APB1ENR_TIM6EN;
#if defined(NX_COLD_WORDS)
using ColdState = nx_stm32_uart_word_state_t;
using ColdConfig = nx_stm32_uart_cold_config_t;
#define COLD_PREPARE nx_stm32_uart_word_cold_prepare
#define COLD_CLOSE   nx_stm32_uart_word_cold_close
#elif defined(NX_COLD_BLOCKS)
using ColdState = nx_stm32_uart_stream_state_t;
using ColdConfig = nx_stm32_uart_cold_config_t;
#define COLD_PREPARE nx_stm32_uart_stream_cold_prepare
#define COLD_CLOSE   nx_stm32_uart_stream_cold_close
#elif NX_COLD_RX && NX_COLD_TX
using ColdState = nx_stm32_uart_duplex_state_t;
using ColdConfig = nx_stm32_uart_dma_cold_config_t;
#define COLD_PREPARE nx_stm32_uart_duplex_cold_prepare
#define COLD_CLOSE   nx_stm32_uart_duplex_cold_close
#elif NX_COLD_RX
using ColdState = nx_stm32_uart_receive_state_t;
using ColdConfig = nx_stm32_uart_dma_cold_config_t;
#define COLD_PREPARE nx_stm32_uart_receive_cold_prepare
#define COLD_CLOSE   nx_stm32_uart_receive_cold_close
#elif defined(NX_COLD_HAL)
using ColdState = nx_stm32_uart_hal_state_t;
using ColdConfig = nx_stm32_uart_dma_cold_config_t;
#define COLD_PREPARE nx_stm32_uart_hal_cold_prepare
#define COLD_CLOSE   nx_stm32_uart_hal_cold_close
#elif NX_COLD_TX
using ColdState = nx_stm32_uart_dma_state_t;
using ColdConfig = nx_stm32_uart_dma_cold_config_t;
#define COLD_PREPARE nx_stm32_uart_dma_cold_prepare
#define COLD_CLOSE   nx_stm32_uart_dma_cold_close
#else
using ColdState = nx_stm32_uart_state_t;
using ColdConfig = nx_stm32_uart_cold_config_t;
#define COLD_PREPARE nx_stm32_uart_byte_cold_prepare
#define COLD_CLOSE   nx_stm32_uart_byte_cold_close
#endif
#ifdef NX_COLD_WORDS
const nx_uart_word_port_t* Factory() {
    return nx_factory_uart_word((nx_uart_word_id_t)0);
}
#else
const nx_uart_port_t* Factory() {
    return nx_factory_uart((nx_uart_id_t)0);
}
#endif
bool Enabled(IRQn_Type irq) {
    return (NVIC->ISER[(unsigned)irq / 32U] & (1U << ((unsigned)irq % 32U))) !=
           0U;
}
class UartColdFactory : public testing::Test {
  protected:
    void* mmio = MAP_FAILED;
    void SetUp() override {
        mmio = mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000U,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mmio, reinterpret_cast<void*>(UINT32_C(0x40000000)));
#ifdef NX_COLD_HAL
        std::memset(g_nx_stm32_scs_model, 0, sizeof(g_nx_stm32_scs_model));
        SystemCoreClock = 168000000U;
        RCC->CFGR = RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV2;
#else
        g_f4_model_nvic = {};
        g_f4_model_scb = {};
        g_f4_model_systick = {};
#endif
        g_f4_model_mask = 0U;
        g_f4_model_isr = false;
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = RCC;
        privileged = true;
        refused = nullptr;
        refused_priority = -1;
        held_bits = starts = writes = 0U;
        now = 100U;
        RCC->AHB1ENR = original_ahb1;
        RCC->APB1ENR = original_apb1;
        USART1->SR = USART_SR_TC | USART_SR_TXE;
#ifdef NX_COLD_MULTI
        USART2->SR = USART_SR_TC | USART_SR_TXE;
        USART3->SR = USART_SR_TC | USART_SR_TXE;
        UART4->SR = USART_SR_TC | USART_SR_TXE;
        UART5->SR = USART_SR_TC | USART_SR_TXE;
        USART6->SR = USART_SR_TC | USART_SR_TXE;
#endif
        GPIOA->MODER = 3U;
        GPIOA->AFR[0] = 0xFU;
        for (IRQn_Type irq : vectors)
            NVIC->IPR[(unsigned)irq] = 0xB0U;
    }
    void TearDown() override {
        privileged = true;
        g_f4_model_mask = 0U;
        g_f4_model_isr = false;
        refused = nullptr;
        refused_priority = -1;
        if (mmio != MAP_FAILED) {
            EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
            EXPECT_EQ(0, munmap(mmio, 0x80000U));
        }
    }
    void Start() {
        const auto result = nx_platform_start();
        ASSERT_EQ(NX_SUCCESS, result.primary);
        ASSERT_EQ(NX_SUCCESS, result.cleanup);
        ASSERT_EQ(0U, result.remaining_effects);
    }
    void Restored() {
        EXPECT_EQ(original_ahb1, RCC->AHB1ENR);
        EXPECT_EQ(original_apb1, RCC->APB1ENR);
        EXPECT_EQ(0U, RCC->APB2ENR & RCC_APB2ENR_USART1EN);
        EXPECT_EQ(3U, GPIOA->MODER);
        EXPECT_EQ(0xFU, GPIOA->AFR[0]);
        for (IRQn_Type irq : vectors) {
            EXPECT_FALSE(Enabled(irq));
            EXPECT_EQ(0xB0U, NVIC->IPR[(unsigned)irq]);
        }
    }
};
} /* namespace */

TEST_F(UartColdFactory, FactoryLookupIsPureBeforeColdAcquisition) {
    const auto* port = Factory();
    ASSERT_NE(nullptr, port);
    EXPECT_EQ(port, Factory());
    EXPECT_EQ(0U, starts);
    EXPECT_EQ(0U, writes);
    Restored();
}

TEST_F(UartColdFactory, SelectedPublishersStartStopRestoreAndRestart) {
    Start();
    EXPECT_NE(0U, USART1->CR1 & USART_CR1_UE);
    EXPECT_EQ((2U << 18U) | (2U << 20U), GPIOA->MODER & 0x3C0000U);
    for (IRQn_Type irq : vectors) {
        EXPECT_TRUE(Enabled(irq));
        EXPECT_EQ(0x50U, NVIC->IPR[(unsigned)irq]);
    }
#if NX_COLD_RX || NX_COLD_TX
    EXPECT_NE(0U, RCC->AHB1ENR & RCC_AHB1ENR_DMA2EN);
#else
    EXPECT_EQ(0U, RCC->AHB1ENR & RCC_AHB1ENR_DMA2EN);
#endif
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
    Restored();
    Start();
}

TEST_F(UartColdFactory, RefusedPublisherPriorityCannotCreateFalseReady) {
    refused_priority = (int)USART1_IRQn;
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    Restored();
    refused_priority = -1;
    Start();
}

TEST_F(UartColdFactory, RefusedPublisherWithdrawalRetainsRealOwnerForRetry) {
    Start();
    const unsigned irq = (unsigned)USART1_IRQn;
    refused = &NVIC->ICER[irq / 32U];
    held_bits = 1U << (irq % 32U);
    EXPECT_NE(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(NX_ERROR_STATE, nx_platform_start().primary);
    EXPECT_TRUE(Enabled(USART1_IRQn));
    EXPECT_NE(0U, RCC->AHB1ENR & RCC_AHB1ENR_GPIOAEN);
    EXPECT_NE(0U, RCC->APB2ENR & RCC_APB2ENR_USART1EN);
    refused = nullptr;
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
    Restored();
    Start();
}

TEST_F(UartColdFactory, RejectedAlienReceiptCannotStopTheLiveGeneratedOwner) {
    Start();
    auto* state = static_cast<ColdState*>(Factory()->context);
    const ColdState original = *state;
    uint16_t saved[2] = {};
    const nx_stm32_platform_pin_config_t pins[] = {{GPIOA, &saved[0], 0U, 9U},
                                                   {GPIOA, &saved[1], 0U, 10U}};
    const nx_dma_memory_region_t memory[] = {
        {UINT32_C(0x20000000), UINT32_C(0x20000),
         NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE}};
    ColdConfig config = {};
#if NX_COLD_RX || NX_COLD_TX
    auto& uart = config.uart;
    config.resources.dma = DMA2;
    config.resources.regions = memory;
    config.resources.region_count = 1U;
    config.receive = DMA2_Stream2;
    config.transmit = {DMA2_Stream7, DMA2_Stream7_IRQn, 7U, 4U};
#else
    auto& uart = config;
    (void)memory;
#endif
    uart.registers = USART1;
    uart.pins = pins;
    uart.vectors = vectors.data();
    uart.vector_count = static_cast<uint8_t>(vectors.size());
    uart.clock_hz = 84000000U;
    uart.baud = 115200U;
    nx_stm32_uart_cold_receipt_t alien = {};
    const auto* bytes = static_cast<const uint8_t*>(mmio);
    const std::vector<uint8_t> before(bytes, bytes + 0x80000U);
    const NVIC_Type before_nvic = *NVIC;
    unsigned before_writes = writes;
    EXPECT_EQ(NX_ERROR_BUSY, COLD_PREPARE(state, &config, &alien));
    EXPECT_EQ(NX_ERROR_BUSY, COLD_CLOSE(state, &config, &alien));
    EXPECT_EQ(0, std::memcmp(state, &original, sizeof(original)));
    EXPECT_EQ(0, std::memcmp(bytes, before.data(), before.size()));
    EXPECT_EQ(0, std::memcmp(NVIC, &before_nvic, sizeof(before_nvic)));
    EXPECT_EQ(before_writes, writes);
}

TEST_F(UartColdFactory, EmptyReceiptDoesNotEraseUnownedColdStorage) {
    ColdState state = {};
#if NX_COLD_RX && NX_COLD_TX
    state.receive.uart.registers = USART1;
#elif NX_COLD_RX || NX_COLD_TX || defined(NX_COLD_BLOCKS)
    state.uart.registers = USART1;
#else
    state.registers = USART1;
#endif
    const ColdState before = state;
    ColdConfig config = {};
    nx_stm32_uart_cold_receipt_t alien = {};
    EXPECT_EQ(NX_SUCCESS, COLD_CLOSE(&state, &config, &alien));
    EXPECT_EQ(0, std::memcmp(&state, &before, sizeof(before)));
    EXPECT_EQ(0U, writes);
}

#if NX_COLD_RX
TEST_F(UartColdFactory, AdditionalDmaOwnersWithdrawTheirActualNvicAndPads) {
    const uint32_t original_gates = RCC->AHB1ENR;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN |
                    RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIODEN |
                    RCC_AHB1ENR_DMA1EN | RCC_AHB1ENR_DMA2EN;
    for (size_t i = 1U; i < 6U; ++i) {
        SCOPED_TRACE(i);
        const auto& selected = nx_stm32_uart_hardware[i];
        selected.registers->SR = USART_SR_TC | USART_SR_TXE;
        uint16_t snapshots[2]{};
        const nx_stm32_platform_pin_config_t pins[] = {
            {selected.tx_gpio, &snapshots[0], selected.tx_port,
             selected.tx_pin},
            {selected.rx_gpio, &snapshots[1], selected.rx_port,
             selected.rx_pin}};
        std::array<IRQn_Type, 2U + NX_COLD_TX> publishers {
            selected.irq, selected.receive_irq,
#if NX_COLD_TX
                selected.transmit.irq,
#endif
        };
        const nx_dma_memory_region_t memory{
            UINT32_C(0x20000000), UINT32_C(0x20000),
            NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE};
        ColdConfig config{};
        config.uart = {selected.registers,
                       pins,
                       publishers.data(),
                       nullptr,
                       nullptr,
                       0U,
                       selected.clock_hz,
                       115200U,
                       NX_UART_RX_BLOCKS,
                       (uint8_t)publishers.size(),
                       5U,
                       false,
                       false};
        config.resources = {selected.dma, &memory, 1U};
        config.receive = selected.receive;
#if NX_COLD_TX
        config.transmit = selected.transmit;
#endif
        for (IRQn_Type irq : publishers)
            NVIC->IPR[(unsigned)irq] = 0xB0U;
        ColdState state{};
        nx_stm32_uart_cold_receipt_t receipt{};
        config.receive = DMA2_Stream4;
        const unsigned before = writes;
        EXPECT_EQ(NX_ERROR_INVALID, COLD_PREPARE(&state, &config, &receipt));
        EXPECT_EQ(before, writes);
        config.receive = selected.receive;
        ASSERT_EQ(NX_SUCCESS, COLD_PREPARE(&state, &config, &receipt));
        for (IRQn_Type irq : publishers) {
            EXPECT_TRUE(Enabled(irq));
            EXPECT_EQ(0x50U, NVIC->IPR[(unsigned)irq]);
        }
        const unsigned rx_irq = (unsigned)selected.receive_irq;
        refused = &NVIC->ICER[rx_irq / 32U];
        held_bits = 1U << (rx_irq % 32U);
        EXPECT_NE(NX_SUCCESS, COLD_CLOSE(&state, &config, &receipt));
        EXPECT_TRUE(Enabled(selected.receive_irq));
        EXPECT_NE(0U, receipt.effects);
        EXPECT_EQ(NX_ERROR_BUSY, COLD_PREPARE(&state, &config, &receipt));
        refused = nullptr;
        ASSERT_EQ(NX_SUCCESS, COLD_CLOSE(&state, &config, &receipt));
        for (IRQn_Type irq : publishers) {
            EXPECT_FALSE(Enabled(irq));
            EXPECT_EQ(0xB0U, NVIC->IPR[(unsigned)irq]);
        }
        EXPECT_EQ(0U, receipt.effects);
        EXPECT_EQ(0U, receipt.clock_added);
    }
    RCC->AHB1ENR = original_gates;
}
#endif

#if defined(NX_COLD_WORDS) || defined(NX_COLD_BLOCKS)
TEST_F(UartColdFactory, AdditionalIrqProfilesReceiveThroughTheirOwnPublisher) {
    const uint32_t original_gates = RCC->AHB1ENR;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN |
                    RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIODEN;
    for (size_t i = 1U; i < 6U; ++i) {
        SCOPED_TRACE(i);
        const auto& selected = nx_stm32_uart_hardware[i];
        uint16_t snapshots[2]{};
        const nx_stm32_platform_pin_config_t pins[] = {
            {selected.tx_gpio, &snapshots[0], selected.tx_port,
             selected.tx_pin},
            {selected.rx_gpio, &snapshots[1], selected.rx_port,
             selected.rx_pin}};
        ColdConfig config{};
        config.registers = selected.registers;
        config.pins = pins;
        config.vectors = &selected.irq;
        config.vector_count = 1U;
        config.clock_hz = selected.clock_hz;
        config.baud = 115200U;
        config.priority = 5U;
#ifdef NX_COLD_WORDS
        uint16_t received[4]{};
        config.rx_storage = received;
        config.rx_capacity = 4U;
#else
        config.profile = NX_UART_RX_BLOCKS;
#endif
        selected.registers->SR = USART_SR_TC | USART_SR_TXE;
        ColdState state{};
        nx_stm32_uart_cold_receipt_t receipt{};
        NVIC->IPR[(unsigned)selected.irq] = 0xB0U;
        ASSERT_EQ(NX_SUCCESS, COLD_PREPARE(&state, &config, &receipt));
        EXPECT_TRUE(Enabled(selected.irq));
        EXPECT_EQ(0x50U, NVIC->IPR[(unsigned)selected.irq]);
#ifdef NX_COLD_WORDS
        const nx_uart_word_port_t face{&nx_stm32_uart_word_ops, &state};
        EXPECT_NE(0U, selected.registers->CR1 & USART_CR1_M);
        selected.registers->DR = 0x1ABU;
        selected.registers->SR = USART_SR_RXNE;
        nx_stm32_uart_word_irq(&state);
        uint16_t word{};
        size_t count{};
        ASSERT_EQ(NX_SUCCESS, nx_uart_word_port_read(&face, &word, 1U, &count));
        EXPECT_EQ(1U, count);
        EXPECT_EQ(0x1ABU, word);
#else
        const nx_uart_port_t face{&nx_stm32_uart_stream_ops, &state};
        uint8_t data[2]{};
        nx_stream_slot_t slot{};
        slot.data = data;
        slot.capacity = sizeof(data);
        nx_stream_t stream{};
        ASSERT_EQ(NX_SUCCESS, nx_stream_initialize(&stream, &slot, 1U));
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&face, &stream));
        selected.registers->DR = 0xABU;
        selected.registers->SR = USART_SR_RXNE | USART_SR_IDLE;
        nx_stm32_uart_stream_irq(&state);
        nx_stream_block_t block{};
        ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &block));
        EXPECT_EQ(1U, block.length);
        EXPECT_EQ(0xABU, block.data[0]);
        EXPECT_EQ(NX_ERROR_BUSY, COLD_CLOSE(&state, &config, &receipt));
        EXPECT_NE(0U, receipt.effects);
        ASSERT_EQ(NX_SUCCESS, nx_stream_release(&stream, &block));
#endif
        selected.registers->SR = USART_SR_TC | USART_SR_TXE;
        ASSERT_EQ(NX_SUCCESS, COLD_CLOSE(&state, &config, &receipt));
        EXPECT_FALSE(Enabled(selected.irq));
        EXPECT_EQ(0xB0U, NVIC->IPR[(unsigned)selected.irq]);
        EXPECT_EQ(0U, receipt.effects);
    }
    RCC->AHB1ENR = original_gates;
}
#endif

#if !NX_COLD_RX && !NX_COLD_TX && !defined(NX_COLD_WORDS) &&                   \
    !defined(NX_COLD_BLOCKS)
namespace {
struct ReviewedByteUart {
    USART_TypeDef* registers;
    GPIO_TypeDef* tx_gpio;
    GPIO_TypeDef* rx_gpio;
    IRQn_Type irq;
    uint32_t clock_hz;
    uint32_t gate;
    uint8_t tx_port;
    uint8_t tx_pin;
    uint8_t rx_port;
    uint8_t rx_pin;
    uint8_t alternate;
    bool apb2;
};
const std::array<ReviewedByteUart, 6> byte_uarts{{
    {USART1, GPIOA, GPIOA, USART1_IRQn, 84000000U, RCC_APB2ENR_USART1EN, 0U, 9U,
     0U, 10U, 7U, true},
    {USART2, GPIOA, GPIOA, USART2_IRQn, 42000000U, RCC_APB1ENR_USART2EN, 0U, 2U,
     0U, 3U, 7U, false},
    {USART3, GPIOB, GPIOB, USART3_IRQn, 42000000U, RCC_APB1ENR_USART3EN, 1U,
     10U, 1U, 11U, 7U, false},
    {UART4, GPIOA, GPIOA, UART4_IRQn, 42000000U, RCC_APB1ENR_UART4EN, 0U, 0U,
     0U, 1U, 8U, false},
    {UART5, GPIOC, GPIOD, UART5_IRQn, 42000000U, RCC_APB1ENR_UART5EN, 2U, 12U,
     3U, 2U, 8U, false},
    {USART6, GPIOC, GPIOC, USART6_IRQn, 84000000U, RCC_APB2ENR_USART6EN, 2U, 6U,
     2U, 7U, 8U, true},
}};
struct ByteColdOwner {
    uint16_t saved[2]{};
    uint8_t bytes[8]{};
    nx_stm32_platform_pin_config_t pins[2]{};
    IRQn_Type vector[1]{};
    nx_stm32_uart_state_t state{};
    nx_stm32_uart_cold_receipt_t receipt{};
    nx_stm32_uart_cold_config_t config{};
    explicit ByteColdOwner(const ReviewedByteUart& hw) {
        pins[0] = {hw.tx_gpio, &saved[0], hw.tx_port, hw.tx_pin};
        pins[1] = {hw.rx_gpio, &saved[1], hw.rx_port, hw.rx_pin};
        vector[0] = hw.irq;
        config.registers = hw.registers;
        config.pins = pins;
        config.vectors = vector;
        config.vector_count = 1U;
        config.rx_storage = bytes;
        config.rx_capacity = sizeof(bytes);
        config.clock_hz = hw.clock_hz;
        config.baud = 115200U;
        config.profile = NX_UART_RX_BYTES;
        config.priority = 5U;
    }
};
} /* namespace */

TEST_F(UartColdFactory,
       EveryReviewedByteControllerAcquiresAndRestoresItsOwnBank) {
    for (const auto& hw : byte_uarts) {
        SCOPED_TRACE(static_cast<int>(hw.irq));
        ByteColdOwner owner(hw);
        const uint32_t selected_gpio = (1U << hw.tx_port) | (1U << hw.rx_port);
        RCC->AHB1ENR = original_ahb1 | selected_gpio;
        hw.registers->SR = USART_SR_TC | USART_SR_TXE;
        const uint32_t before1 = RCC->APB1ENR;
        const uint32_t before2 = RCC->APB2ENR;
        const uint16_t before_tx = nx_stm32_pin_capture(hw.tx_gpio, hw.tx_pin);
        const uint16_t before_rx = nx_stm32_pin_capture(hw.rx_gpio, hw.rx_pin);
        NVIC->IPR[static_cast<unsigned>(hw.irq)] = 0xB0U;
        ASSERT_EQ(NX_SUCCESS, nx_stm32_uart_byte_cold_prepare(
                                  &owner.state, &owner.config, &owner.receipt));
        EXPECT_NE(0U, hw.registers->CR1 & USART_CR1_UE);
        EXPECT_NE(0U, (hw.apb2 ? RCC->APB2ENR : RCC->APB1ENR) & hw.gate);
        EXPECT_EQ(hw.clock_hz / 115200U +
                      (hw.clock_hz % 115200U >= 57600U ? 1U : 0U),
                  hw.registers->BRR);
        EXPECT_TRUE(Enabled(hw.irq));
        EXPECT_EQ(hw.alternate,
                  (hw.tx_gpio->AFR[hw.tx_pin / 8U] >> ((hw.tx_pin % 8U) * 4U)) &
                      15U);
        EXPECT_EQ(hw.alternate,
                  (hw.rx_gpio->AFR[hw.rx_pin / 8U] >> ((hw.rx_pin % 8U) * 4U)) &
                      15U);
        hw.registers->DR = static_cast<uint32_t>(hw.tx_pin);
        hw.registers->SR = USART_SR_RXNE;
        nx_stm32_uart_irq(&owner.state);
        hw.registers->SR = USART_SR_TC | USART_SR_TXE;
        uint8_t received = 0U;
        size_t count = 0U;
        EXPECT_EQ(NX_SUCCESS, nx_stm32_uart_ops.read_bytes(
                                  &owner.state, &received, 1U, &count));
        EXPECT_EQ(1U, count);
        EXPECT_EQ(hw.tx_pin, received);
        ASSERT_EQ(NX_SUCCESS, nx_stm32_uart_byte_cold_close(
                                  &owner.state, &owner.config, &owner.receipt));
        EXPECT_EQ(before1, RCC->APB1ENR);
        EXPECT_EQ(before2, RCC->APB2ENR);
        EXPECT_EQ(before_tx, nx_stm32_pin_capture(hw.tx_gpio, hw.tx_pin));
        EXPECT_EQ(before_rx, nx_stm32_pin_capture(hw.rx_gpio, hw.rx_pin));
        EXPECT_FALSE(Enabled(hw.irq));
        EXPECT_EQ(0xB0U, NVIC->IPR[static_cast<unsigned>(hw.irq)]);
    }
}

TEST_F(UartColdFactory, AdditionalByteControllersDoNotRequireAnUnrelatedGpioA) {
    ByteColdOwner owner(byte_uarts[2]);
    RCC->AHB1ENR = RCC_AHB1ENR_GPIOBEN;
    USART3->SR = USART_SR_TC | USART_SR_TXE;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_uart_byte_cold_prepare(
                              &owner.state, &owner.config, &owner.receipt));
    EXPECT_EQ(0U, RCC->AHB1ENR & RCC_AHB1ENR_GPIOAEN);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_uart_byte_cold_close(
                              &owner.state, &owner.config, &owner.receipt));
}

TEST_F(UartColdFactory,
       AdditionalControllerRejectsWrongClockVectorAndPadsWithoutWrites) {
    for (unsigned malformed = 0U; malformed < 5U; ++malformed) {
        ByteColdOwner owner(byte_uarts[2]);
        RCC->AHB1ENR = RCC_AHB1ENR_GPIOBEN;
        switch (malformed) {
            case 0U:
                owner.config.clock_hz = 84000000U;
                break;
            case 1U:
                owner.vector[0] = USART2_IRQn;
                break;
            case 2U:
                owner.pins[0].pin = 9U;
                break;
            case 3U:
                owner.pins[1].port_index = 0U;
                break;
            default:
                owner.pins[1].snapshot = owner.pins[0].snapshot;
                break;
        }
        const auto before = writes;
        EXPECT_EQ(NX_ERROR_INVALID,
                  nx_stm32_uart_cold_acquire(&owner.config, &owner.receipt));
        EXPECT_EQ(before, writes);
        EXPECT_EQ(0U, owner.receipt.effects);
        EXPECT_EQ(0U, owner.receipt.clock_added);
    }
}

TEST_F(UartColdFactory,
       SeparateControllersKeepReceiveAndGateOwnershipIndependent) {
    ByteColdOwner first(byte_uarts[2]);
    ByteColdOwner second(byte_uarts[5]);
    RCC->AHB1ENR = RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;
    USART3->SR = USART6->SR = USART_SR_TC | USART_SR_TXE;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_uart_byte_cold_prepare(
                              &first.state, &first.config, &first.receipt));
    ASSERT_EQ(NX_SUCCESS, nx_stm32_uart_byte_cold_prepare(
                              &second.state, &second.config, &second.receipt));
    USART3->DR = 0x31U;
    USART3->SR = USART_SR_RXNE;
    nx_stm32_uart_irq(&first.state);
    USART3->SR = USART_SR_TC | USART_SR_TXE;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_uart_byte_cold_close(
                              &second.state, &second.config, &second.receipt));
    EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_USART3EN);
    EXPECT_EQ(0U, RCC->APB2ENR & RCC_APB2ENR_USART6EN);
    EXPECT_TRUE(Enabled(USART3_IRQn));
    uint8_t received = 0U;
    size_t count = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_uart_ops.read_bytes(&first.state, &received,
                                                       1U, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(0x31U, received);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_uart_byte_cold_close(
                              &first.state, &first.config, &first.receipt));
}

TEST_F(UartColdFactory, RefusedAdditionalVectorWithdrawalRetainsBothPortPads) {
    ByteColdOwner owner(byte_uarts[4]);
    RCC->AHB1ENR = RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIODEN;
    UART5->SR = USART_SR_TC | USART_SR_TXE;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_uart_byte_cold_prepare(
                              &owner.state, &owner.config, &owner.receipt));
    refused = &NVIC->ICER[static_cast<unsigned>(UART5_IRQn) / 32U];
    held_bits = 1U << (static_cast<unsigned>(UART5_IRQn) % 32U);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_uart_byte_cold_close(
                                 &owner.state, &owner.config, &owner.receipt));
    EXPECT_NE(0U, owner.receipt.effects);
    EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_UART5EN);
    EXPECT_EQ(2U, (GPIOC->MODER >> 24U) & 3U);
    EXPECT_EQ(2U, (GPIOD->MODER >> 4U) & 3U);
    refused = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_uart_byte_cold_close(
                              &owner.state, &owner.config, &owner.receipt));
    EXPECT_EQ(0U, owner.receipt.effects);
    EXPECT_EQ(0U, RCC->APB1ENR & RCC_APB1ENR_UART5EN);
}
#ifdef NX_COLD_MULTI
TEST_F(UartColdFactory,
       SixGeneratedFacesShareOperationsAndKeepDistinctReceivers) {
    const std::array<USART_TypeDef*, 6> registers{USART1, USART2, USART3,
                                                  UART4,  UART5,  USART6};
    const std::array<IRQn_Type, 6> irqs{USART1_IRQn, USART2_IRQn, USART3_IRQn,
                                        UART4_IRQn,  UART5_IRQn,  USART6_IRQn};
    const std::array<nx_uart_id_t, 6> ids{
        NX_UART_ID_CONSOLE, NX_UART_ID_SERIAL2, NX_UART_ID_SERIAL3,
        NX_UART_ID_SERIAL4, NX_UART_ID_SERIAL5, NX_UART_ID_SERIAL6};
    std::array<const nx_uart_port_t*, 6> faces{};
    for (size_t i = 0U; i < faces.size(); ++i) {
        faces[i] = nx_factory_uart(ids[i]);
        ASSERT_NE(nullptr, faces[i]);
        EXPECT_EQ(faces[0]->ops, faces[i]->ops);
        EXPECT_EQ(0U, registers[i]->CR1 & USART_CR1_UE);
        for (size_t j = 0U; j < i; ++j)
            EXPECT_NE(faces[j]->context, faces[i]->context);
    }
    EXPECT_EQ(0U, writes);
    Start();
    for (size_t i = 0U; i < faces.size(); ++i) {
        auto* state = static_cast<nx_stm32_uart_state_t*>(faces[i]->context);
        EXPECT_EQ(registers[i], state->registers);
        EXPECT_EQ(irqs[i], state->irq);
        registers[i]->DR = static_cast<uint32_t>(0x30U + i);
        registers[i]->SR = USART_SR_RXNE;
        nx_stm32_uart_irq(state);
        registers[i]->SR = USART_SR_TC | USART_SR_TXE;
    }
    for (size_t i = 0U; i < faces.size(); ++i) {
        uint8_t received = 0U;
        size_t count = 0U;
        ASSERT_EQ(NX_SUCCESS,
                  nx_uart_port_read_bytes(faces[i], &received, 1U, &count));
        EXPECT_EQ(1U, count);
        EXPECT_EQ(0x30U + i, received);
    }
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
    for (IRQn_Type irq : irqs)
        EXPECT_FALSE(Enabled(irq));
    EXPECT_EQ(original_apb1, RCC->APB1ENR);
    EXPECT_EQ(0U, RCC->APB2ENR & (RCC_APB2ENR_USART1EN | RCC_APB2ENR_USART6EN));
    EXPECT_EQ(original_ahb1, RCC->AHB1ENR);
}
#endif
#endif

extern "C" bool nx_arch_is_privileged(void) {
    return privileged;
}
extern "C" bool nx_arch_in_isr(void) {
    return g_f4_model_isr;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return g_f4_model_mask != 0U;
}
extern "C" void nx_arch_dsb(void) {
}
extern "C" void nx_arch_dmb(void) {
}
extern "C" void nx_arch_isb(void) {
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    const nx_arch_irq_state_t previous{g_f4_model_mask};
    g_f4_model_mask = 1U;
    return previous;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    g_f4_model_mask = previous.value;
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    return ++now;
}
#ifdef NX_COLD_HAL
extern "C" {
uint32_t SystemCoreClock = 168000000U;
const uint8_t AHBPrescTable[16] = {0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
                                   1U, 2U, 3U, 4U, 6U, 7U, 8U, 9U};
const uint8_t APBPrescTable[8] = {0U, 0U, 0U, 0U, 1U, 2U, 3U, 4U};
/* This factory isolates the Platform time port; the runtime suite executes
 * the production System/HAL time bridge with an explicit timer model. */
uint32_t HAL_GetTick(void) {
    return static_cast<uint32_t>(nx_time_now_us() / 1000U);
}
/* The SDK's DMA disable macro writes one 32-bit silicon register. */
void nx_uart_cold_hal_dma_disable(DMA_HandleTypeDef* handle) {
    handle->Instance->CR &= ~static_cast<uint32_t>(DMA_SxCR_EN);
}
}
#endif
extern "C" void nx_platform_cold_model_write(volatile uint32_t* reg,
                                             uint32_t value) {
    ++writes;
    if (reg == refused)
        value = (value & ~held_bits) | (*reg & held_bits);
    for (unsigned bank = 0U; bank < 8U; ++bank) {
        if (reg == &NVIC->ISER[bank]) {
            NVIC->ISER[bank] |= value;
            return;
        }
        if (reg == &NVIC->ICER[bank]) {
            NVIC->ISER[bank] &= ~value;
            return;
        }
        if (reg == &NVIC->ICPR[bank]) {
            NVIC->ISPR[bank] &= ~value;
            return;
        }
    }
    *reg = value;
}
extern "C" void nx_control_model_clear(volatile uint32_t* reg, uint32_t mask) {
    *reg &= ~mask;
}
extern "C" void nx_uart_cold_model_priority_write(volatile uint8_t* reg,
                                                  uint8_t value) {
    if (refused_priority < 0 || reg != &NVIC->IPR[(unsigned)refused_priority])
        *reg = value;
}
extern "C" void nx_control_model_priority(IRQn_Type irq, uint32_t priority) {
    nx_uart_cold_model_priority_write(&NVIC->IPR[(unsigned)irq],
                                      (uint8_t)(priority << 4U));
}
extern "C" uint32_t nx_uart_cold_model_priority_read(IRQn_Type irq) {
    return NVIC->IPR[(unsigned)irq] >> 4U;
}
extern "C" void nx_control_model_enable(IRQn_Type irq) {
    const unsigned index = (unsigned)irq;
    nx_platform_cold_model_write(&NVIC->ISER[index / 32U], 1U << (index % 32U));
}
extern "C" void nx_uart_cold_model_disable(IRQn_Type irq) {
    const unsigned index = (unsigned)irq;
    nx_platform_cold_model_write(&NVIC->ICER[index / 32U], 1U << (index % 32U));
}
extern "C" void nx_uart_cold_model_pending_clear(IRQn_Type irq) {
    const unsigned index = (unsigned)irq;
    nx_platform_cold_model_write(&NVIC->ICPR[index / 32U], 1U << (index % 32U));
}
extern "C" nx_result_t nx_stm32_validate_identity(uint32_t) {
    return NX_SUCCESS;
}
extern "C" nx_result_t nx_stm32_system_preflight(const nx_stm32_system_t*) {
    return NX_SUCCESS;
}
extern "C" nx_stm32_start_result_t nx_stm32_system_start(nx_stm32_system_t*,
                                                         uint32_t) {
    ++starts;
    return {0, 0, 0U};
}
extern "C" int nx_stm32_system_stop(nx_stm32_system_t*, uint32_t) {
    return 0;
}
