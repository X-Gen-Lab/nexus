/**
 * \file            gd32_uart_cold_factory_test.cpp
 * \brief           All maintained UART cold wrappers through actual factories
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "gd32f470_provider.h"
#include "gd32f470_uart_cold.h"
#include "nexus/arch/arch.h"
#include "nexus/io/uart_word.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include <cstring>

NVIC_Type g_gd32_model_nvic;
SCB_Type g_gd32_model_scb;
SysTick_Type g_gd32_model_systick;
uint32_t g_gd32_model_mask;
bool g_gd32_model_isr;
}
#include <array>
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
bool privileged;
volatile uint32_t* refused;
uint32_t held_bits;
int refused_priority;
bool arm_clear_refusal;
unsigned starts;
unsigned writes;
nx_time_us_t now;
constexpr std::array<IRQn_Type, 1U + NX_COLD_RX + NX_COLD_TX> vectors {
    USART0_IRQn,
#if NX_COLD_RX
        DMA1_Channel2_IRQn,
#endif
#if NX_COLD_TX
        DMA1_Channel7_IRQn,
#endif
};
constexpr uint32_t original_ahb1 = RCU_AHB1EN_PCEN | RCU_AHB1EN_DMA0EN;
constexpr uint32_t original_apb1 = RCU_APB1EN_TIMER5EN;
#ifdef NX_COLD_WORDS
const nx_uart_word_port_t* Factory() {
    return nx_factory_uart_word((nx_uart_word_id_t)0);
}
#else
const nx_uart_port_t* Factory() {
    return nx_factory_uart((nx_uart_id_t)0);
}
#endif
#if defined(NX_COLD_WORDS)
using ColdState = nx_gd32_uart_word_state_t;
using ColdConfig = nx_gd32_uart_cold_config_t;
#define NX_SELECTED_PREPARE nx_gd32_uart_word_cold_prepare
#define NX_SELECTED_CLOSE   nx_gd32_uart_word_cold_close
#elif NX_COLD_RX && NX_COLD_TX
using ColdState = nx_gd32_uart_duplex_state_t;
using ColdConfig = nx_gd32_uart_dma_cold_config_t;
#define NX_SELECTED_PREPARE nx_gd32_uart_duplex_cold_prepare
#define NX_SELECTED_CLOSE   nx_gd32_uart_duplex_cold_close
#elif NX_COLD_RX
using ColdState = nx_gd32_uart_receive_state_t;
using ColdConfig = nx_gd32_uart_dma_cold_config_t;
#define NX_SELECTED_PREPARE nx_gd32_uart_receive_cold_prepare
#define NX_SELECTED_CLOSE   nx_gd32_uart_receive_cold_close
#elif NX_COLD_TX
using ColdState = nx_gd32_uart_dma_state_t;
using ColdConfig = nx_gd32_uart_dma_cold_config_t;
#define NX_SELECTED_PREPARE nx_gd32_uart_dma_cold_prepare
#define NX_SELECTED_CLOSE   nx_gd32_uart_dma_cold_close
#elif defined(NX_COLD_BLOCKS)
using ColdState = nx_gd32_uart_stream_state_t;
using ColdConfig = nx_gd32_uart_cold_config_t;
#define NX_SELECTED_PREPARE nx_gd32_uart_stream_cold_prepare
#define NX_SELECTED_CLOSE   nx_gd32_uart_stream_cold_close
#else
using ColdState = nx_gd32_uart_state_t;
using ColdConfig = nx_gd32_uart_cold_config_t;
#define NX_SELECTED_PREPARE nx_gd32_uart_byte_cold_prepare
#define NX_SELECTED_CLOSE   nx_gd32_uart_byte_cold_close
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
        g_gd32_model_nvic = {};
        g_gd32_model_scb = {};
        g_gd32_model_systick = {};
        g_gd32_model_mask = 0U;
        g_gd32_model_isr = false;
        privileged = true;
        refused = nullptr;
        refused_priority = -1;
        arm_clear_refusal = false;
        held_bits = starts = writes = 0U;
        now = 100U;
        RCU_AHB1EN = original_ahb1;
        RCU_APB1EN = original_apb1;
        USART_STAT0(USART0) = USART_STAT0_TC | USART_STAT0_TBE;
        GPIO_CTL(GPIOA) = 3U;
        GPIO_AFSEL0(GPIOA) = 0xFU;
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
        EXPECT_EQ(original_ahb1, RCU_AHB1EN);
        EXPECT_EQ(original_apb1, RCU_APB1EN);
        EXPECT_EQ(0U, RCU_APB2EN & RCU_APB2EN_USART0EN);
        EXPECT_EQ(3U, GPIO_CTL(GPIOA));
        EXPECT_EQ(0xFU, GPIO_AFSEL0(GPIOA));
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
    EXPECT_NE(0U, USART_CTL0(USART0) & USART_CTL0_UEN);
    EXPECT_EQ((2U << 18U) | (2U << 20U), GPIO_CTL(GPIOA) & 0x3C0000U);
    for (IRQn_Type irq : vectors) {
        EXPECT_TRUE(Enabled(irq));
        EXPECT_EQ(0x50U, NVIC->IPR[(unsigned)irq]);
    }
#if NX_COLD_RX || NX_COLD_TX
    EXPECT_NE(0U, RCU_AHB1EN & RCU_AHB1EN_DMA1EN);
#else
    EXPECT_EQ(0U, RCU_AHB1EN & RCU_AHB1EN_DMA1EN);
#endif
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
    Restored();
    Start();
}

TEST_F(UartColdFactory, RefusedPublisherPriorityCannotCreateFalseReady) {
    refused_priority = (int)USART0_IRQn;
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
    const unsigned irq = (unsigned)USART0_IRQn;
    refused = &NVIC->ICER[irq / 32U];
    held_bits = 1U << (irq % 32U);
    EXPECT_NE(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(NX_ERROR_STATE, nx_platform_start().primary);
    EXPECT_TRUE(Enabled(USART0_IRQn));
    EXPECT_NE(0U, RCU_AHB1EN & RCU_AHB1EN_PAEN);
    EXPECT_NE(0U, RCU_APB2EN & RCU_APB2EN_USART0EN);
    refused = nullptr;
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
    Restored();
    Start();
}

TEST_F(UartColdFactory, RefusedPriorityRestoreRetainsColdLeaseForRetry) {
    Start();
    refused_priority = (int)USART0_IRQn;
    EXPECT_EQ(NX_ERROR_IO, nx_platform_stop());
    EXPECT_EQ(NX_ERROR_STATE, nx_platform_start().primary);
    EXPECT_FALSE(Enabled(USART0_IRQn));
    EXPECT_EQ(0x50U, NVIC->IPR[(unsigned)USART0_IRQn]);
    EXPECT_NE(0U, RCU_AHB1EN & RCU_AHB1EN_PAEN);
    refused_priority = -1;
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
    Restored();
    Start();
}

TEST_F(UartColdFactory, RejectedConstructionCannotStopAnUnacquiredOwner) {
    Start();
    auto* state = static_cast<ColdState*>(Factory()->context);
    std::array<unsigned char, sizeof(ColdState)> before{};
    std::memcpy(before.data(), state, before.size());
    std::array<nx_gd32_cold_pin_config_t, 2> pins{{
        {GPIOA, 9U, 7U, 2U, 1U, false},
        {GPIOA, 10U, 7U, 2U, 1U, false},
    }};
    std::array<uint16_t, 2> snapshots{};
    std::array<uint8_t, vectors.size()> priorities{};
    std::array<uint16_t, 32> receive{};
    ColdConfig config{};
#if NX_COLD_RX || NX_COLD_TX
    auto& selected = config.uart;
    const nx_dma_memory_region_t domain{};
    config.resources = {&domain, 1U};
#else
    auto& selected = config;
#endif
    selected.controller = &nx_gd32_usart0_controller;
    selected.pins = pins.data();
    selected.snapshots = snapshots.data();
    selected.vectors = vectors.data();
    selected.priorities = priorities.data();
    selected.vector_count = (uint8_t)vectors.size();
    selected.receive = receive.data();
    selected.capacity = receive.size();
    selected.profile = NX_UART_RX_BYTES;
    selected.baud = 115200U;
    selected.priority = 5U;
    nx_gd32_uart_cold_t unrelated{};
    EXPECT_EQ(NX_ERROR_BUSY, NX_SELECTED_PREPARE(state, &config, &unrelated));
    EXPECT_EQ(0U, unrelated.effects);
    EXPECT_EQ(NX_ERROR_BUSY, NX_SELECTED_CLOSE(state, &config, &unrelated));
    EXPECT_EQ(0, std::memcmp(before.data(), state, before.size()));
    EXPECT_TRUE(Enabled(USART0_IRQn));
    EXPECT_NE(0U, USART_CTL0(USART0) & USART_CTL0_UEN);
}

TEST_F(UartColdFactory, PublicDriverStopLeavesColdLeaseForPlatformCleanup) {
    Start();
#ifdef NX_COLD_WORDS
    ASSERT_EQ(NX_SUCCESS, nx_uart_word_port_stop(Factory()));
#else
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_stop(Factory()));
#endif
    EXPECT_EQ(0U, USART_CTL0(USART0) & USART_CTL0_UEN);
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
    Restored();
    Start();
}

TEST_F(UartColdFactory, FailedPartialInitializationRetainsGatePadsAndRetry) {
    refused_priority = (int)USART0_IRQn;
    arm_clear_refusal = true;
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_NE(NX_SUCCESS, result.cleanup);
    EXPECT_NE(0U, result.remaining_effects);
    EXPECT_NE(0U, USART_CTL0(USART0) & USART_CTL0_UEN);
    EXPECT_NE(0U, RCU_APB2EN & RCU_APB2EN_USART0EN);
    EXPECT_NE(0U, RCU_AHB1EN & RCU_AHB1EN_PAEN);
    EXPECT_EQ((2U << 18U) | (2U << 20U), GPIO_CTL(GPIOA) & 0x3C0000U);
    EXPECT_EQ(NX_ERROR_STATE, nx_platform_start().primary);
    refused = nullptr;
    refused_priority = -1;
    arm_clear_refusal = false;
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
    Restored();
    Start();
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
extern "C" void nx_gd32_uart_cold_model_write(volatile uint32_t* reg,
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
    for (uint32_t bank = GPIOA; bank <= GPIOI; bank += GPIOB - GPIOA) {
        if (reg == &GPIO_BOP(bank)) {
            GPIO_OCTL(bank) =
                (GPIO_OCTL(bank) | (value & 0xFFFFU)) & ~(value >> 16U);
            return;
        }
    }
    *reg = value;
}
extern "C" void nx_gd32_uart_cold_model_clear(volatile uint32_t* reg,
                                              uint32_t mask) {
    *reg &= ~mask;
}
extern "C" void nx_uart_cold_model_priority_write(volatile uint8_t* reg,
                                                  uint8_t value) {
    if (refused_priority < 0 || reg != &NVIC->IPR[(unsigned)refused_priority]) {
        *reg = value;
    } else if (arm_clear_refusal) {
        /* The initializer has enabled UEN before publishing its IRQ. Refuse
         * only its later withdrawal, so this is a genuine retained partial. */
        refused = &USART_CTL0(USART0);
        held_bits = USART_CTL0_UEN;
    }
}
extern "C" void nx_gd32_uart_cold_model_priority(IRQn_Type irq,
                                                 uint32_t priority) {
    nx_uart_cold_model_priority_write(&NVIC->IPR[(unsigned)irq],
                                      (uint8_t)(priority << 4U));
}
extern "C" uint32_t nx_uart_cold_model_priority_read(IRQn_Type irq) {
    return NVIC->IPR[(unsigned)irq] >> 4U;
}
extern "C" void nx_gd32_uart_cold_model_enable(IRQn_Type irq) {
    const unsigned index = (unsigned)irq;
    nx_gd32_uart_cold_model_write(&NVIC->ISER[index / 32U],
                                  1U << (index % 32U));
}
extern "C" void nx_uart_cold_model_disable(IRQn_Type irq) {
    const unsigned index = (unsigned)irq;
    nx_gd32_uart_cold_model_write(&NVIC->ICER[index / 32U],
                                  1U << (index % 32U));
}
extern "C" void nx_uart_cold_model_pending_clear(IRQn_Type irq) {
    const unsigned index = (unsigned)irq;
    nx_gd32_uart_cold_model_write(&NVIC->ICPR[index / 32U],
                                  1U << (index % 32U));
}
extern "C" nx_result_t nx_gd32_soc_preflight(void) {
    return NX_SUCCESS;
}
extern "C" int nx_gd32_soc_start(void) {
    ++starts;
    return 0;
}
extern "C" int nx_gd32_soc_stop(void) {
    return 0;
}
