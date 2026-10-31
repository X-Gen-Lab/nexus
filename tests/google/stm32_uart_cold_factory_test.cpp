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
NVIC_Type g_gd32_model_nvic;
SCB_Type g_gd32_model_scb;
SysTick_Type g_gd32_model_systick;
#endif
uint32_t g_gd32_model_mask;
bool g_gd32_model_isr;
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
        g_gd32_model_nvic = {};
        g_gd32_model_scb = {};
        g_gd32_model_systick = {};
#endif
        g_gd32_model_mask = 0U;
        g_gd32_model_isr = false;
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
        GPIOA->MODER = 3U;
        GPIOA->AFR[0] = 0xFU;
        for (IRQn_Type irq : vectors)
            NVIC->IPR[(unsigned)irq] = 0xB0U;
    }
    void TearDown() override {
        privileged = true;
        g_gd32_model_mask = 0U;
        g_gd32_model_isr = false;
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

extern "C" bool nx_arch_is_privileged(void) {
    return privileged;
}
extern "C" bool nx_arch_in_isr(void) {
    return g_gd32_model_isr;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return g_gd32_model_mask != 0U;
}
extern "C" void nx_arch_dsb(void) {
}
extern "C" void nx_arch_dmb(void) {
}
extern "C" void nx_arch_isb(void) {
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    const nx_arch_irq_state_t previous{g_gd32_model_mask};
    g_gd32_model_mask = 1U;
    return previous;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    g_gd32_model_mask = previous.value;
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
