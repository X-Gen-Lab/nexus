/**
 * \file            uart_formats_provider_test.cpp
 * \brief           Production UART register formats and nine-bit IRQ payloads.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#if defined(NX_UART_FORMAT_STM32)
extern "C" {
#include "stm32f407_uart_word.h"
}
#include "nexus/io/native/model.h"
#else
extern "C" {
#include "gd32f470_uart_word.h"
#include "gd32f4xx.h"
extern uint64_t g_gd32_model_now;
extern uint32_t g_gd32_model_mask;
extern bool g_gd32_model_isr;
void USART0_IRQHandler(void) {
}
}
#include <sys/mman.h>
#endif
#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>

namespace {
bool observe_cold_enable;
unsigned cold_enables;
volatile uint32_t* refused_write;
volatile uint32_t* refused_write_second;
uint32_t held_set;
uint32_t held_clear;
uint32_t held_set_second;
uint32_t held_clear_second;
bool reject_irq_enable;
bool reject_irq_disable;
bool held_irq_pending;
bool held_irq_active;
bool reject_irq_priority;
bool reject_clock_off;
bool model_irq_enabled;
uint32_t model_irq_priority;
void reset_ports() {
    observe_cold_enable = false;
    cold_enables = 0U;
    refused_write = nullptr;
    refused_write_second = nullptr;
    held_set = held_clear = held_set_second = held_clear_second = 0U;
    reject_irq_enable = reject_irq_disable = false;
    held_irq_pending = held_irq_active = false;
    reject_irq_priority = reject_clock_off = false;
    model_irq_enabled = false;
    model_irq_priority = 0U;
#if !defined(NX_UART_FORMAT_STM32)
    /* Each fixture maps a fresh peripheral device; its external NVIC model
     * must be reset as part of the same simulated device power cycle. */
    std::memset(NVIC, 0, sizeof(*NVIC));
#endif
}
} /* namespace */

class UartFormatProvider : public testing::Test {
  protected:
    uint16_t rx[4] = {};
    uint16_t payload[2] = {0x1FFU, 0x100U};
    nx_uart_word_tx_request_t request = {};
#if defined(NX_UART_FORMAT_STM32)
    using ByteState = nx_stm32_uart_state_t;
    nx_result_t initialize_bytes(ByteState* bytes, uint8_t* storage) {
        bytes->registers = &registers;
        bytes->irq = USART1_IRQn;
        bytes->baud = 115200U;
        bytes->rx_storage = storage;
        bytes->rx_capacity = 4U;
        bytes->profile = NX_UART_RX_BYTES;
        return nx_stm32_uart_initialize_format(
            bytes, 84000000U,
            {8U, NX_UART_PARITY_EVEN, NX_UART_STOP_2, NX_UART_FLOW_CTS});
    }
    nx_uart_port_t bytes_face(ByteState* bytes) {
        return {&nx_stm32_uart_ops, bytes};
    }
#else
    using ByteState = nx_gd32_uart_state_t;
    nx_result_t initialize_bytes(ByteState* bytes, uint8_t* storage) {
        return nx_gd32_uart_initialize_format_at(
            bytes, &nx_gd32_usart0_controller, 115200U, NX_UART_RX_BYTES,
            storage, 4U, 5U,
            {8U, NX_UART_PARITY_EVEN, NX_UART_STOP_2, NX_UART_FLOW_CTS});
    }
    nx_uart_port_t bytes_face(ByteState* bytes) {
        return {&nx_gd32_uart_ops, bytes};
    }
#endif
#if defined(NX_UART_FORMAT_STM32)
    alignas(size_t) USART_TypeDef registers = {};
    nx_stm32_uart_word_state_t state = {};
    nx_uart_word_port_t port = {&nx_stm32_uart_word_ops, &state};
    void SetUp() override {
        reset_ports();
        ASSERT_EQ(nx_native_clock_configure(true, 10U), NX_SUCCESS);
        state.registers = &registers;
        state.irq = USART1_IRQn;
        ASSERT_EQ(
            nx_stm32_uart_word_initialize(&state, 84000000U, 115200U, rx, 4U),
            NX_SUCCESS);
        nx_request_initialize(&request.base);
    }
    void TearDown() override {
        reset_ports();
    }
    void irq(uint32_t flags) {
        const_cast<uint32_t&>(registers.SR) = flags;
        nx_stm32_uart_word_irq(&state);
    }
    uint32_t data() {
        return registers.DR;
    }
    void advance(nx_time_us_t duration) {
        ASSERT_EQ(nx_native_clock_advance(duration), NX_SUCCESS);
    }
    static constexpr uint32_t empty = USART_SR_TXE;
    static constexpr uint32_t complete = USART_SR_TC;
    static constexpr uint32_t ready = USART_SR_RXNE;
#else
    void* mapping = MAP_FAILED;
    nx_gd32_uart_word_state_t state = {};
    nx_uart_word_port_t port = {&nx_gd32_uart_word_ops, &state};
    void SetUp() override {
        reset_ports();
        mapping =
            mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(UINT32_C(0x40000000)));
        g_gd32_model_now = 10U;
        g_gd32_model_mask = 0U;
        g_gd32_model_isr = false;
        ASSERT_EQ(nx_gd32_uart_word_initialize(
                      &state, &nx_gd32_usart0_controller, 115200U, rx, 4U, 5U),
                  NX_SUCCESS);
        nx_request_initialize(&request.base);
    }
    void TearDown() override {
        reset_ports();
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(munmap(mapping, 0x80000), 0);
        }
    }
    void irq(uint32_t flags) {
        USART_STAT0(USART0) = flags;
        nx_gd32_uart_word_irq(&state);
    }
    uint32_t data() {
        return USART_DATA(USART0);
    }
    void advance(nx_time_us_t duration) {
        g_gd32_model_now += duration;
    }
    static constexpr uint32_t empty = USART_STAT0_TBE;
    static constexpr uint32_t complete = USART_STAT0_TC;
    static constexpr uint32_t ready = USART_STAT0_RBNE;
#endif
};

TEST_F(UartFormatProvider, WritesAllNineBitsAndWaitsForDistinctWireTc) {
    ASSERT_EQ(nx_uart_word_tx_prepare(&request, payload, 2U, 1000U),
              NX_SUCCESS);
    ASSERT_EQ(nx_uart_word_port_submit(&port, &request), NX_SUCCESS);
    irq(empty | complete);
    EXPECT_EQ(data(), 0x1FFU);
    irq(empty | complete);
    EXPECT_EQ(data(), 0x100U);
    nx_uart_word_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    irq(complete);
    nx_uart_word_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.transferred, 2U);
    EXPECT_EQ(state.active, nullptr);
}

TEST_F(UartFormatProvider, CancellationDoesNotReleaseTheInflightWord) {
    ASSERT_EQ(nx_uart_word_tx_prepare(&request, payload, 2U, 1000U),
              NX_SUCCESS);
    ASSERT_EQ(nx_uart_word_port_submit(&port, &request), NX_SUCCESS);
    irq(empty);
    ASSERT_EQ(nx_uart_word_port_cancel(&port, &request), NX_SUCCESS);
    nx_uart_word_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    EXPECT_EQ(state.active, &request);
    irq(complete);
    nx_uart_word_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(state.active, nullptr);
}

TEST_F(UartFormatProvider, RxPreservesNinthBitWithoutByteFaceStorage) {
#if defined(NX_UART_FORMAT_STM32)
    registers.DR = 0x1AAU;
#else
    USART_DATA(USART0) = 0x1AAU;
#endif
    irq(ready);
    uint16_t value = 0U;
    size_t count = 0U;
    EXPECT_EQ(nx_uart_word_port_read(&port, &value, 1U, &count), NX_SUCCESS);
    EXPECT_EQ(count, 1U);
    EXPECT_EQ(value, 0x1AAU);
}

TEST_F(UartFormatProvider, ReadRejectsLiveBorrowAndMetadataAliases) {
    ASSERT_EQ(nx_uart_word_tx_prepare(&request, payload, 2U, 1000U),
              NX_SUCCESS);
    ASSERT_EQ(nx_uart_word_port_submit(&port, &request), NX_SUCCESS);
    size_t count = 99U;
    EXPECT_EQ(nx_uart_word_port_read(&port, payload, 2U, &count),
              NX_ERROR_INVALID);
    EXPECT_EQ(payload[0], 0x1FFU);
    uint16_t output = 0U;
    EXPECT_EQ(nx_uart_word_port_read(&port, &output, 1U, &state.count),
              NX_ERROR_INVALID);
    EXPECT_EQ(state.active, &request);
    EXPECT_EQ(
        nx_uart_word_port_read(&port, &output, 1U, &request.base.transferred),
        NX_ERROR_INVALID);
}

TEST_F(UartFormatProvider, DeadlineQuarantinesUnprovedDrainUntilLateWireTc) {
    ASSERT_EQ(nx_uart_word_tx_prepare(&request, payload, 2U, 1000U),
              NX_SUCCESS);
    ASSERT_EQ(nx_uart_word_port_submit(&port, &request), NX_SUCCESS);
    irq(empty);
    advance(1000U);
    nx_uart_word_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    advance(1000U);
    nx_uart_word_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(state.active, &request);
    EXPECT_EQ(nx_uart_word_port_stop(&port), NX_ERROR_BUSY);
    irq(complete);
    nx_uart_word_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_TIMEOUT);
    EXPECT_EQ(state.active, nullptr);
    EXPECT_EQ(nx_uart_word_port_stop(&port), NX_SUCCESS);
}

#if defined(NX_UART_FORMAT_STM32)
TEST(UartByteFormat, F407SupportsEightDataBitsWithParityWithoutGrowingState) {
    reset_ports();
    USART_TypeDef registers = {};
    nx_stm32_uart_state_t state = {};
    uint8_t rx[4] = {};
    state.registers = &registers;
    state.baud = 115200U;
    state.rx_storage = rx;
    state.rx_capacity = sizeof(rx);
    state.profile = NX_UART_RX_BYTES;
    ASSERT_EQ(nx_stm32_uart_initialize(&state, 84000000U), NX_SUCCESS);
    const nx_uart_format_t format = {8U, NX_UART_PARITY_EVEN, NX_UART_STOP_2,
                                     NX_UART_FLOW_CTS};
    EXPECT_EQ(nx_stm32_uart_format_configure(&state, format), NX_SUCCESS);
    EXPECT_NE(registers.CR1 & USART_CR1_M, 0U);
    EXPECT_NE(registers.CR1 & USART_CR1_PCE, 0U);
    EXPECT_EQ(registers.CR1 & USART_CR1_PS, 0U);
    EXPECT_EQ(registers.CR2 & USART_CR2_STOP, 2U << 12U);
    EXPECT_NE(registers.CR3 & USART_CR3_CTSE, 0U);
    const uint32_t before = registers.CR1;
    EXPECT_EQ(nx_stm32_uart_format_configure(
                  &state,
                  {9U, NX_UART_PARITY_NONE, NX_UART_STOP_1, NX_UART_FLOW_NONE}),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(registers.CR1, before);
    ASSERT_EQ(nx_stm32_uart_format_configure(
                  &state,
                  {7U, NX_UART_PARITY_EVEN, NX_UART_STOP_1, NX_UART_FLOW_NONE}),
              NX_SUCCESS);
    registers.DR = 0xFAU;
    const_cast<uint32_t&>(registers.SR) = USART_SR_RXNE;
    nx_stm32_uart_format_irq(&state);
    EXPECT_EQ(rx[0], 0x7AU);
}
#else
TEST_F(UartFormatProvider, GdByteFormatSurvivesCancellationReset) {
    ASSERT_EQ(nx_uart_word_port_stop(&port), NX_SUCCESS);
    nx_gd32_uart_state_t bytes = {};
    uint8_t byte_rx[4] = {};
    ASSERT_EQ(nx_gd32_uart_initialize_at(&bytes, &nx_gd32_usart0_controller,
                                         115200U, NX_UART_RX_BYTES, byte_rx, 4U,
                                         5U),
              NX_SUCCESS);
    const nx_uart_format_t format = {8U, NX_UART_PARITY_ODD, NX_UART_STOP_2,
                                     NX_UART_FLOW_CTS};
    ASSERT_EQ(nx_gd32_uart_format_configure(&bytes, format), NX_SUCCESS);
    uint8_t byte = 0x55U;
    nx_uart_tx_request_t tx = {};
    nx_request_initialize(&tx.base);
    ASSERT_EQ(nx_uart_tx_prepare(&tx, &byte, 1U, 1000U), NX_SUCCESS);
    const nx_uart_port_t byte_port = {&nx_gd32_uart_ops, &bytes};
    ASSERT_EQ(nx_uart_port_submit(&byte_port, &tx), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_cancel(&byte_port, &tx), NX_SUCCESS);
    nx_uart_port_service(&byte_port);
    EXPECT_EQ(nx_request_state(&tx.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(USART_CTL0(USART0) &
                  (USART_CTL0_WL | USART_CTL0_PCEN | USART_CTL0_PM),
              USART_CTL0_WL | USART_CTL0_PCEN | USART_CTL0_PM);
    EXPECT_EQ(USART_CTL1(USART0) & USART_CTL1_STB, 2U << 12U);
    EXPECT_NE(USART_CTL2(USART0) & USART_CTL2_CTSEN, 0U);
    size_t count = 0U;
    uint8_t output = 0U;
    EXPECT_EQ(nx_uart_port_read_bytes(&byte_port, &output, 1U, &count),
              NX_ERROR_OVERFLOW);
    ASSERT_EQ(nx_gd32_uart_format_configure(
                  &bytes,
                  {7U, NX_UART_PARITY_EVEN, NX_UART_STOP_1, NX_UART_FLOW_NONE}),
              NX_SUCCESS);
    USART_DATA(USART0) = 0xFAU;
    USART_STAT0(USART0) = USART_STAT0_RBNE;
    nx_gd32_uart_format_irq(&bytes);
    ASSERT_EQ(nx_uart_port_read_bytes(&byte_port, &output, 1U, &count),
              NX_SUCCESS);
    EXPECT_EQ(output, 0x7AU);
}
#endif

#if defined(NX_UART_FORMAT_STM32)
TEST(UartColdFormat, F407FirstEnableAlreadyHasTheSelectedWireFormat) {
    reset_ports();
    USART_TypeDef registers = {};
    nx_stm32_uart_state_t state = {};
    uint8_t rx[4] = {};
    state.registers = &registers;
    state.baud = 115200U;
    state.rx_storage = rx;
    state.rx_capacity = sizeof(rx);
    state.profile = NX_UART_RX_BYTES;
    cold_enables = 0U;
    observe_cold_enable = true;
    EXPECT_EQ(nx_stm32_uart_initialize_format(
                  &state, 84000000U,
                  {8U, NX_UART_PARITY_EVEN, NX_UART_STOP_2, NX_UART_FLOW_CTS}),
              NX_SUCCESS);
    observe_cold_enable = false;
    EXPECT_EQ(cold_enables, 1U);
    EXPECT_TRUE(state.initialized);
}

TEST(UartColdFormat, F407RejectsUnsupportedFormatBeforeAnyHardwareEffects) {
    reset_ports();
    USART_TypeDef registers = {};
    nx_stm32_uart_state_t state = {};
    uint8_t rx[4] = {};
    state.registers = &registers;
    state.baud = 115200U;
    state.rx_storage = rx;
    state.rx_capacity = sizeof(rx);
    state.profile = NX_UART_RX_BYTES;
    cold_enables = 0U;
    EXPECT_EQ(nx_stm32_uart_initialize_format(
                  &state, 84000000U,
                  {9U, NX_UART_PARITY_NONE, NX_UART_STOP_1, NX_UART_FLOW_NONE}),
              NX_ERROR_UNSUPPORTED);
    EXPECT_FALSE(state.initialized);
    EXPECT_EQ(registers.CR1, 0U);
    EXPECT_EQ(registers.CR2, 0U);
    EXPECT_EQ(registers.CR3, 0U);
    EXPECT_EQ(registers.BRR, 0U);
    EXPECT_EQ(cold_enables, 0U);
}
#else
TEST_F(UartFormatProvider, GdFirstEnableAlreadyHasTheSelectedWireFormat) {
    ASSERT_EQ(nx_uart_word_port_stop(&port), NX_SUCCESS);
    nx_gd32_uart_state_t bytes = {};
    uint8_t byte_rx[4] = {};
    cold_enables = 0U;
    observe_cold_enable = true;
    EXPECT_EQ(nx_gd32_uart_initialize_format_at(
                  &bytes, &nx_gd32_usart0_controller, 115200U, NX_UART_RX_BYTES,
                  byte_rx, 4U, 5U,
                  {8U, NX_UART_PARITY_EVEN, NX_UART_STOP_2, NX_UART_FLOW_CTS}),
              NX_SUCCESS);
    observe_cold_enable = false;
    EXPECT_EQ(cold_enables, 1U);
    EXPECT_TRUE(bytes.initialized);
}

TEST_F(UartFormatProvider, GdRejectsUnsupportedFormatBeforeClockOrIrqEffects) {
    ASSERT_EQ(nx_uart_word_port_stop(&port), NX_SUCCESS);
    nx_gd32_uart_state_t bytes = {};
    uint8_t byte_rx[4] = {};
    uint32_t before_clock = RCU_REG_VAL(nx_gd32_usart0_controller.clock);
    unsigned vector = USART0_IRQn;
    uint32_t before_irq = NVIC->ISER[vector / 32U];
    cold_enables = 0U;
    EXPECT_EQ(nx_gd32_uart_initialize_format_at(
                  &bytes, &nx_gd32_usart0_controller, 115200U, NX_UART_RX_BYTES,
                  byte_rx, 4U, 5U,
                  {9U, NX_UART_PARITY_NONE, NX_UART_STOP_1, NX_UART_FLOW_NONE}),
              NX_ERROR_UNSUPPORTED);
    EXPECT_FALSE(bytes.initialized);
    EXPECT_EQ(RCU_REG_VAL(nx_gd32_usart0_controller.clock), before_clock);
    EXPECT_EQ(NVIC->ISER[vector / 32U], before_irq);
    EXPECT_EQ(cold_enables, 0U);
}
#endif

/** \brief This model boundary observes and performs the actual UE/UEN write. */
extern "C" void nx_uart_format_model_enable(uintptr_t base, uint32_t control) {
#if defined(NX_UART_FORMAT_STM32)
    auto* registers = reinterpret_cast<USART_TypeDef*>(base);
    if (observe_cold_enable) {
        EXPECT_EQ(registers->CR1 & USART_CR1_UE, 0U);
        EXPECT_EQ(control & (USART_CR1_M | USART_CR1_PCE | USART_CR1_PS),
                  USART_CR1_M | USART_CR1_PCE);
        EXPECT_EQ(registers->CR2 & USART_CR2_STOP, 2U << 12U);
        EXPECT_EQ(registers->CR3 & (USART_CR3_RTSE | USART_CR3_CTSE),
                  USART_CR3_CTSE);
        ++cold_enables;
    }
    nx_uart_format_model_write(&registers->CR1, control);
#else
    uint32_t registers = static_cast<uint32_t>(base);
    if (observe_cold_enable) {
        EXPECT_EQ(USART_CTL0(registers) & USART_CTL0_UEN, 0U);
        EXPECT_EQ(control & (USART_CTL0_WL | USART_CTL0_PCEN | USART_CTL0_PM),
                  USART_CTL0_WL | USART_CTL0_PCEN);
        EXPECT_EQ(USART_CTL1(registers) & USART_CTL1_STB, 2U << 12U);
        EXPECT_EQ(USART_CTL2(registers) & (USART_CTL2_RTSEN | USART_CTL2_CTSEN),
                  USART_CTL2_CTSEN);
        ++cold_enables;
    }
    nx_uart_format_model_write(&USART_CTL0(registers), control);
#endif
}

TEST_F(UartFormatProvider, ReadRejectsActualUartControlRegisters) {
#if defined(NX_UART_FORMAT_STM32)
    registers.DR = 0x1AAU;
#else
    USART_DATA(USART0) = 0x1AAU;
#endif
    irq(ready);
#if defined(NX_UART_FORMAT_STM32)
    auto* control = &registers.CR1;
    auto* aligned_count = &registers.CR2;
#else
    auto* control = &USART_CTL0(USART0);
    auto* aligned_count = &USART_CTL1(USART0);
#endif
    uint32_t before = *control;
    size_t count = 123U;
    EXPECT_EQ(nx_uart_word_port_read(
                  &port,
                  reinterpret_cast<uint16_t*>(const_cast<uint32_t*>(control)),
                  1U, &count),
              NX_ERROR_INVALID);
    EXPECT_EQ(*control, before);
    uint16_t output = 0U;
    EXPECT_EQ(nx_uart_word_port_read(&port, &output, 1U,
                                     reinterpret_cast<size_t*>(
                                         const_cast<uint32_t*>(aligned_count))),
              NX_ERROR_INVALID);
    *control = before;
}

TEST_F(UartFormatProvider, ColdReceiveStorageCannotBeUartRegisters) {
    ASSERT_EQ(nx_uart_word_port_stop(&port), NX_SUCCESS);
    decltype(state) other{};
#if defined(NX_UART_FORMAT_STM32)
    other.registers = &registers;
    other.irq = USART1_IRQn;
    EXPECT_EQ(
        nx_stm32_uart_word_initialize(
            &other, 84000000U, 115200U,
            reinterpret_cast<uint16_t*>(const_cast<uint32_t*>(&registers.CR1)),
            1U),
        NX_ERROR_INVALID);
#else
    EXPECT_EQ(nx_gd32_uart_word_initialize(
                  &other, &nx_gd32_usart0_controller, 115200U,
                  reinterpret_cast<uint16_t*>(
                      const_cast<uint32_t*>(&USART_CTL0(USART0))),
                  1U, 5U),
              NX_ERROR_INVALID);
#endif
    if (other.initialized) {
        auto cleanup = port;
        cleanup.context = &other;
        EXPECT_EQ(nx_uart_word_port_stop(&cleanup), NX_SUCCESS);
    }
}

TEST_F(UartFormatProvider, StoppedOldOwnerCannotDisableTheNextInstance) {
    ASSERT_EQ(nx_uart_word_port_stop(&port), NX_SUCCESS);
    decltype(state) other{};
    uint16_t other_rx[2] = {};
#if defined(NX_UART_FORMAT_STM32)
    other.registers = &registers;
    other.irq = USART1_IRQn;
    ASSERT_EQ(
        nx_stm32_uart_word_initialize(&other, 84000000U, 115200U, other_rx, 2U),
        NX_SUCCESS);
    uint32_t before = registers.CR1;
#else
    ASSERT_EQ(nx_gd32_uart_word_initialize(&other, &nx_gd32_usart0_controller,
                                           115200U, other_rx, 2U, 5U),
              NX_SUCCESS);
    uint32_t before = USART_CTL0(USART0);
#endif
    EXPECT_EQ(nx_uart_word_port_stop(&port), NX_SUCCESS);
#if defined(NX_UART_FORMAT_STM32)
    EXPECT_EQ(registers.CR1, before);
#else
    EXPECT_EQ(USART_CTL0(USART0), before);
#endif
    auto cleanup = port;
    cleanup.context = &other;
    EXPECT_EQ(nx_uart_word_port_stop(&cleanup), NX_SUCCESS);
}

#if !defined(NX_UART_FORMAT_STM32)
TEST_F(UartFormatProvider, WordColdConstructorDiscardsEarlierOwnersUnreadRx) {
    USART_DATA(USART0) = 0x1ABU;
    USART_STAT0(USART0) = USART_STAT0_RBNE;
    ASSERT_EQ(nx_uart_word_port_stop(&port), NX_SUCCESS);

    nx_gd32_uart_word_state_t next = {};
    uint16_t next_rx[2] = {};
    ASSERT_EQ(nx_gd32_uart_word_initialize(&next, &nx_gd32_usart0_controller,
                                           115200U, next_rx, 2U, 5U),
              NX_SUCCESS);
    const nx_uart_word_port_t next_port = {&nx_gd32_uart_word_ops, &next};
    nx_gd32_uart_word_irq(&next);

    uint16_t received = 0U;
    size_t count = 99U;
    EXPECT_EQ(nx_uart_word_port_read(&next_port, &received, 1U, &count),
              NX_ERROR_EMPTY);
    EXPECT_EQ(count, 0U);
    EXPECT_EQ(received, 0U);
    EXPECT_EQ(nx_uart_word_port_stop(&next_port), NX_SUCCESS);
}
#endif

TEST_F(UartFormatProvider, NeverOwnedColdStopIsIdempotentWithoutMmioAccess) {
    EXPECT_EXIT(
        {
            decltype(state) zero{};
            auto cold = port;
            cold.context = &zero;
            std::_Exit(nx_uart_word_port_stop(&cold) == NX_SUCCESS ? 0 : 3);
        },
        testing::ExitedWithCode(0), "");
}

TEST_F(UartFormatProvider, TransmitCannotBorrowMutableUartDataRegisters) {
#if defined(NX_UART_FORMAT_STM32)
    registers.DR = 0x1ABU;
    auto* data = &registers.DR;
#else
    USART_DATA(USART0) = 0x1ABU;
    auto* data = &USART_DATA(USART0);
#endif
    ASSERT_EQ(nx_uart_word_tx_prepare(&request,
                                      reinterpret_cast<const uint16_t*>(
                                          const_cast<const uint32_t*>(data)),
                                      1U, 1000U),
              NX_SUCCESS);
    nx_result_t result = nx_uart_word_port_submit(&port, &request);
    EXPECT_EQ(result, NX_ERROR_INVALID);
    if (result == NX_SUCCESS) {
        ASSERT_EQ(nx_uart_word_port_cancel(&port, &request), NX_SUCCESS);
        nx_uart_word_port_service(&port);
    }
}

TEST_F(UartFormatProvider, DrainBudgetCoversTwoElevenBitWords) {
    state.baud = 9600U;
#if defined(NX_UART_FORMAT_STM32)
    registers.BRR = 8750U;
#else
    USART_BAUD(USART0) = 10417U;
#endif
    ASSERT_EQ(nx_uart_word_tx_prepare(&request, payload, 2U, NX_DEADLINE_NEVER),
              NX_SUCCESS);
    ASSERT_EQ(nx_uart_word_port_submit(&port, &request), NX_SUCCESS);
    irq(empty);
    irq(empty);
    nx_time_us_t before = nx_time_now_us();
    ASSERT_EQ(nx_uart_word_port_cancel(&port, &request), NX_SUCCESS);
    EXPECT_GE(state.drain_deadline - before,
              (22000000U + 9599U) / 9600U + 100U);
    irq(complete);
    nx_uart_word_port_service(&port);
}

/** \brief Fault injection changes only a selected external MMIO write. */
extern "C" void nx_uart_format_model_write(volatile uint32_t* address,
                                           uint32_t value) {
    if (address == refused_write) {
        value = (value | (*address & held_set)) & ~held_clear;
    }
    if (address == refused_write_second) {
        value = (value | (*address & held_set_second)) & ~held_clear_second;
    }
    *address = value;
}
/** \brief Observe actual model IRQ controller facts with explicit refused
 * ports. */
extern "C" uint32_t nx_uart_format_model_irq(unsigned command, int irq,
                                             uint32_t value) {
#if defined(NX_UART_FORMAT_STM32)
    (void)irq;
    switch (command) {
        case 0U:
            if (!reject_irq_enable)
                model_irq_enabled = true;
            break;
        case 1U:
            if (!reject_irq_disable)
                model_irq_enabled = false;
            break;
        case 3U:
            if (!reject_irq_priority)
                model_irq_priority = value;
            break;
        case 4U:
            return model_irq_enabled ? 1U : 0U;
        case 5U:
            return held_irq_pending ? 1U : 0U;
        case 6U:
            return held_irq_active ? 1U : 0U;
        case 7U:
            return model_irq_priority;
        default:
            break;
    }
#else
    IRQn_Type vector = static_cast<IRQn_Type>(irq);
    switch (command) {
        case 0U:
            if (!reject_irq_enable)
                NVIC_EnableIRQ(vector);
            break;
        case 1U:
            if (!reject_irq_disable)
                NVIC_DisableIRQ(vector);
            break;
        case 2U:
            if (!held_irq_pending)
                NVIC_ClearPendingIRQ(vector);
            break;
        case 3U:
            if (!reject_irq_priority)
                NVIC_SetPriority(vector, value);
            break;
        case 4U:
            return NVIC_GetEnableIRQ(vector);
        case 5U:
            return held_irq_pending ? 1U : NVIC_GetPendingIRQ(vector);
        case 6U:
            return held_irq_active ? 1U : NVIC_GetActive(vector);
        case 7U:
            return NVIC_GetPriority(vector);
        default:
            break;
    }
#endif
    return 0U;
}
/** \brief Clock disable can fail independently of USART and NVIC withdrawal. */
extern "C" void nx_uart_format_model_clock(bool enabled, uint32_t clock) {
#if defined(NX_UART_FORMAT_STM32)
    (void)enabled;
    (void)clock;
#else
    if (enabled) {
        rcu_periph_clock_enable(static_cast<rcu_periph_enum>(clock));
    } else if (!reject_clock_off) {
        rcu_periph_clock_disable(static_cast<rcu_periph_enum>(clock));
    }
#endif
}

TEST_F(UartFormatProvider, SettlementWaitsForTxInterruptWithdrawalReadback) {
    ASSERT_EQ(nx_uart_word_tx_prepare(&request, payload, 2U, 1000U),
              NX_SUCCESS);
    ASSERT_EQ(nx_uart_word_port_submit(&port, &request), NX_SUCCESS);
    irq(empty);
    irq(empty);
    irq(complete);
#if defined(NX_UART_FORMAT_STM32)
    registers.CR1 |= USART_CR1_TCIE;
    refused_write = &registers.CR1;
    held_set = USART_CR1_TCIE;
#else
    USART_CTL0(USART0) |= USART_CTL0_TCIE;
    refused_write = &USART_CTL0(USART0);
    held_set = USART_CTL0_TCIE;
#endif
    nx_uart_word_port_service(&port);
    EXPECT_EQ(state.active, &request);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    refused_write = nullptr;
    nx_uart_word_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
}

TEST_F(UartFormatProvider, StopRetainsLeaseUntilNvicDisableIsObserved) {
    reject_irq_disable = true;
    EXPECT_EQ(nx_uart_word_port_stop(&port), NX_ERROR_BUSY);
    EXPECT_TRUE(state.initialized);
    reject_irq_disable = false;
    EXPECT_EQ(nx_uart_word_port_stop(&port), NX_SUCCESS);
}

TEST_F(UartFormatProvider, StopRetainsLeaseWhileIrqIsPendingOrActive) {
    held_irq_pending = true;
    EXPECT_EQ(nx_uart_word_port_stop(&port), NX_ERROR_BUSY);
    EXPECT_TRUE(state.initialized);
    held_irq_pending = false;
    held_irq_active = true;
    EXPECT_EQ(nx_uart_word_port_stop(&port), NX_ERROR_BUSY);
    EXPECT_TRUE(state.initialized);
    held_irq_active = false;
    EXPECT_EQ(nx_uart_word_port_stop(&port), NX_SUCCESS);
}

TEST_F(UartFormatProvider, ColdInitRejectsRefusedUsartEnable) {
    ASSERT_EQ(nx_uart_word_port_stop(&port), NX_SUCCESS);
#if defined(NX_UART_FORMAT_STM32)
    refused_write = &registers.CR1;
    held_clear = USART_CR1_UE;
    EXPECT_EQ(nx_stm32_uart_word_initialize(&state, 84000000U, 115200U, rx, 4U),
              NX_ERROR_IO);
#else
    refused_write = &USART_CTL0(USART0);
    held_clear = USART_CTL0_UEN;
    EXPECT_EQ(nx_gd32_uart_word_initialize(&state, &nx_gd32_usart0_controller,
                                           115200U, rx, 4U, 5U),
              NX_ERROR_IO);
#endif
    refused_write = nullptr;
    EXPECT_EQ(nx_uart_word_port_stop(&port), NX_SUCCESS);
}

TEST_F(UartFormatProvider, ColdInitRejectsRefusedNvicEnableOrPriority) {
    ASSERT_EQ(nx_uart_word_port_stop(&port), NX_SUCCESS);
    reject_irq_enable = true;
#if defined(NX_UART_FORMAT_STM32)
    EXPECT_EQ(nx_stm32_uart_word_initialize(&state, 84000000U, 115200U, rx, 4U),
              NX_ERROR_IO);
#else
    EXPECT_EQ(nx_gd32_uart_word_initialize(&state, &nx_gd32_usart0_controller,
                                           115200U, rx, 4U, 5U),
              NX_ERROR_IO);
#endif
    reject_irq_enable = false;
    EXPECT_EQ(nx_uart_word_port_stop(&port), NX_SUCCESS);
    reject_irq_priority = true;
#if defined(NX_UART_FORMAT_STM32)
    model_irq_priority = 0U;
    EXPECT_EQ(nx_stm32_uart_word_initialize(&state, 84000000U, 115200U, rx, 4U),
              NX_ERROR_IO);
#else
    NVIC_SetPriority(USART0_IRQn, 0U);
    EXPECT_EQ(nx_gd32_uart_word_initialize(&state, &nx_gd32_usart0_controller,
                                           115200U, rx, 4U, 5U),
              NX_ERROR_IO);
#endif
    reject_irq_priority = false;
    EXPECT_EQ(nx_uart_word_port_stop(&port), NX_SUCCESS);
}

#if !defined(NX_UART_FORMAT_STM32)
TEST_F(UartFormatProvider, StopRetainsLeaseWhenTheOwnedClockRefusesDisable) {
    reject_clock_off = true;
    EXPECT_EQ(nx_uart_word_port_stop(&port), NX_ERROR_BUSY);
    EXPECT_TRUE(state.initialized);
    reject_clock_off = false;
    EXPECT_EQ(nx_uart_word_port_stop(&port), NX_SUCCESS);
}

TEST_F(UartFormatProvider, RxStorageCannotBorrowImmutableControllerFacts) {
    ASSERT_EQ(nx_uart_word_port_stop(&port), NX_SUCCESS);
    decltype(state) other{};
    auto* storage = reinterpret_cast<uint16_t*>(
        const_cast<nx_gd32_uart_controller_t*>(&nx_gd32_usart0_controller));
    EXPECT_EQ(nx_gd32_uart_word_initialize(&other, &nx_gd32_usart0_controller,
                                           115200U, storage, 1U, 5U),
              NX_ERROR_INVALID);
    if (other.initialized) {
        auto cleanup = port;
        cleanup.context = &other;
        EXPECT_EQ(nx_uart_word_port_stop(&cleanup), NX_SUCCESS);
    }
}
#endif

TEST_F(UartFormatProvider, FrameChangeConfirmsUeWithdrawalBeforeFormatWrites) {
    ASSERT_EQ(nx_uart_word_port_stop(&port), NX_SUCCESS);
    uint8_t byte_rx[4] = {};
#if defined(NX_UART_FORMAT_STM32)
    nx_stm32_uart_state_t bytes = {};
    bytes.registers = &registers;
    bytes.baud = 115200U;
    bytes.rx_storage = byte_rx;
    bytes.rx_capacity = sizeof(byte_rx);
    bytes.profile = NX_UART_RX_BYTES;
    ASSERT_EQ(nx_stm32_uart_initialize(&bytes, 84000000U), NX_SUCCESS);
    uint32_t before1 = registers.CR1;
    uint32_t before2 = registers.CR2;
    uint32_t before3 = registers.CR3;
    refused_write = &registers.CR1;
    held_set = USART_CR1_UE;
    EXPECT_EQ(nx_stm32_uart_format_configure(
                  &bytes,
                  {8U, NX_UART_PARITY_EVEN, NX_UART_STOP_2, NX_UART_FLOW_CTS}),
              NX_ERROR_IO);
    EXPECT_EQ(registers.CR1, before1);
    EXPECT_EQ(registers.CR2, before2);
    EXPECT_EQ(registers.CR3, before3);
    const nx_uart_port_t byte_port = {&nx_stm32_uart_ops, &bytes};
#else
    nx_gd32_uart_state_t bytes = {};
    ASSERT_EQ(nx_gd32_uart_initialize_at(&bytes, &nx_gd32_usart0_controller,
                                         115200U, NX_UART_RX_BYTES, byte_rx, 4U,
                                         5U),
              NX_SUCCESS);
    uint32_t before1 = USART_CTL0(USART0);
    uint32_t before2 = USART_CTL1(USART0);
    uint32_t before3 = USART_CTL2(USART0);
    refused_write = &USART_CTL0(USART0);
    held_set = USART_CTL0_UEN;
    EXPECT_EQ(nx_gd32_uart_format_configure(
                  &bytes,
                  {8U, NX_UART_PARITY_EVEN, NX_UART_STOP_2, NX_UART_FLOW_CTS}),
              NX_ERROR_IO);
    EXPECT_EQ(USART_CTL0(USART0), before1);
    EXPECT_EQ(USART_CTL1(USART0), before2);
    EXPECT_EQ(USART_CTL2(USART0), before3);
    const nx_uart_port_t byte_port = {&nx_gd32_uart_ops, &bytes};
#endif
    refused_write = nullptr;
    EXPECT_EQ(nx_uart_port_stop(&byte_port), NX_SUCCESS);
}

TEST_F(UartFormatProvider, FailedFrameRollbackClosesFurtherAdmission) {
    ASSERT_EQ(nx_uart_word_port_stop(&port), NX_SUCCESS);
    uint8_t byte_rx[4] = {};
#if defined(NX_UART_FORMAT_STM32)
    nx_stm32_uart_state_t bytes = {};
    bytes.registers = &registers;
    bytes.baud = 115200U;
    bytes.rx_storage = byte_rx;
    bytes.rx_capacity = sizeof(byte_rx);
    bytes.profile = NX_UART_RX_BYTES;
    ASSERT_EQ(nx_stm32_uart_initialize(&bytes, 84000000U), NX_SUCCESS);
    refused_write = &registers.CR2;
    held_clear = USART_CR2_STOP;
    refused_write_second = &registers.CR1;
    held_clear_second = USART_CR1_UE;
    EXPECT_EQ(nx_stm32_uart_format_configure(
                  &bytes,
                  {8U, NX_UART_PARITY_EVEN, NX_UART_STOP_2, NX_UART_FLOW_NONE}),
              NX_ERROR_IO);
    EXPECT_TRUE(bytes.closing);
    const nx_uart_port_t byte_port = {&nx_stm32_uart_ops, &bytes};
#else
    nx_gd32_uart_state_t bytes = {};
    ASSERT_EQ(nx_gd32_uart_initialize_at(&bytes, &nx_gd32_usart0_controller,
                                         115200U, NX_UART_RX_BYTES, byte_rx, 4U,
                                         5U),
              NX_SUCCESS);
    refused_write = &USART_CTL1(USART0);
    held_clear = USART_CTL1_STB;
    refused_write_second = &USART_CTL0(USART0);
    held_clear_second = USART_CTL0_UEN;
    EXPECT_EQ(nx_gd32_uart_format_configure(
                  &bytes,
                  {8U, NX_UART_PARITY_EVEN, NX_UART_STOP_2, NX_UART_FLOW_NONE}),
              NX_ERROR_IO);
    EXPECT_TRUE(bytes.stopping);
    const nx_uart_port_t byte_port = {&nx_gd32_uart_ops, &bytes};
#endif
    EXPECT_TRUE(bytes.initialized);
    uint8_t payload_byte = 0x55U;
    nx_uart_tx_request_t tx = {};
    nx_request_initialize(&tx.base);
    ASSERT_EQ(nx_uart_tx_prepare(&tx, &payload_byte, 1U, 1000U), NX_SUCCESS);
    nx_result_t admitted = nx_uart_port_submit(&byte_port, &tx);
    EXPECT_EQ(admitted, NX_ERROR_STATE);
    refused_write = nullptr;
    refused_write_second = nullptr;
    if (admitted == NX_SUCCESS) {
        EXPECT_EQ(nx_uart_port_cancel(&byte_port, &tx), NX_SUCCESS);
        nx_uart_port_service(&byte_port);
    }
    EXPECT_EQ(nx_uart_port_stop(&byte_port), NX_SUCCESS);
}

TEST_F(UartFormatProvider, RefusedTxeWithdrawalNeverReadsPastBorrowedWords) {
    uint16_t backing[3] = {0x101U, 0x102U, 0x1ABU};
    ASSERT_EQ(nx_uart_word_tx_prepare(&request, backing, 2U, 1000U),
              NX_SUCCESS);
    ASSERT_EQ(nx_uart_word_port_submit(&port, &request), NX_SUCCESS);
#if defined(NX_UART_FORMAT_STM32)
    refused_write = &registers.CR1;
    held_set = USART_CR1_TXEIE;
#else
    refused_write = &USART_CTL0(USART0);
    held_set = USART_CTL0_TBEIE;
#endif
    irq(empty);
    irq(empty);
    irq(empty);
    EXPECT_EQ(state.position, 2U);
    EXPECT_EQ(data(), 0x102U);
    refused_write = nullptr;
#if defined(NX_UART_FORMAT_STM32)
    registers.CR1 &= ~static_cast<uint32_t>(USART_CR1_TXEIE);
#else
    USART_CTL0(USART0) &= ~static_cast<uint32_t>(USART_CTL0_TBEIE);
#endif
    irq(complete);
    nx_uart_word_port_service(&port);
}

TEST_F(UartFormatProvider, CancelCannotLoadMoreWordsWhenTxeDisableIsRefused) {
    ASSERT_EQ(nx_uart_word_tx_prepare(&request, payload, 2U, 1000U),
              NX_SUCCESS);
    ASSERT_EQ(nx_uart_word_port_submit(&port, &request), NX_SUCCESS);
    irq(empty);
#if defined(NX_UART_FORMAT_STM32)
    refused_write = &registers.CR1;
    held_set = USART_CR1_TXEIE;
#else
    refused_write = &USART_CTL0(USART0);
    held_set = USART_CTL0_TBEIE;
#endif
    ASSERT_EQ(nx_uart_word_port_cancel(&port, &request), NX_SUCCESS);
    irq(empty);
    EXPECT_EQ(state.position, 1U);
    refused_write = nullptr;
#if defined(NX_UART_FORMAT_STM32)
    registers.CR1 &= ~static_cast<uint32_t>(USART_CR1_TXEIE);
#else
    USART_CTL0(USART0) &= ~static_cast<uint32_t>(USART_CTL0_TBEIE);
#endif
    irq(complete);
    nx_uart_word_port_service(&port);
}

TEST_F(UartFormatProvider, FormattedColdConstructorRequiresCompleteNvicProof) {
    ASSERT_EQ(nx_uart_word_port_stop(&port), NX_SUCCESS);
    for (unsigned fault = 0U; fault != 4U; ++fault) {
        SCOPED_TRACE(fault);
        ByteState bytes = {};
        uint8_t storage[4] = {};
        reject_irq_enable = fault == 0U;
        reject_irq_priority = fault == 1U;
        held_irq_pending = fault == 2U;
        held_irq_active = fault == 3U;
#if defined(NX_UART_FORMAT_STM32)
        model_irq_priority = 0U;
#else
        NVIC_SetPriority(USART0_IRQn, 0U);
#endif
        nx_result_t result = initialize_bytes(&bytes, storage);
        EXPECT_NE(result, NX_SUCCESS);
        if (result != NX_SUCCESS && bytes.initialized) {
#if defined(NX_UART_FORMAT_STM32)
            EXPECT_TRUE(bytes.closing);
#else
            EXPECT_TRUE(bytes.stopping);
#endif
        }
        reject_irq_enable = reject_irq_priority = false;
        held_irq_pending = held_irq_active = false;
        auto byte_port = bytes_face(&bytes);
        if (bytes.initialized) {
            EXPECT_EQ(nx_uart_port_stop(&byte_port), NX_SUCCESS);
        }
    }
}

TEST_F(UartFormatProvider, FormattedByteStopRetainsUnprovedIrqOwnership) {
    ASSERT_EQ(nx_uart_word_port_stop(&port), NX_SUCCESS);
    for (unsigned fault = 0U; fault != 3U; ++fault) {
        SCOPED_TRACE(fault);
        ByteState bytes = {};
        uint8_t storage[4] = {};
        ASSERT_EQ(initialize_bytes(&bytes, storage), NX_SUCCESS);
        auto byte_port = bytes_face(&bytes);
        reject_irq_disable = fault == 0U;
        held_irq_pending = fault == 1U;
        held_irq_active = fault == 2U;
        EXPECT_EQ(nx_uart_port_stop(&byte_port), NX_ERROR_BUSY);
        EXPECT_TRUE(bytes.initialized);
        reject_irq_disable = false;
        held_irq_pending = held_irq_active = false;
        EXPECT_EQ(nx_uart_port_stop(&byte_port), NX_SUCCESS);
    }
}
#if !defined(NX_UART_FORMAT_STM32)
TEST_F(UartFormatProvider, FormattedByteStopRetainsTheUnreleasedClockLease) {
    ASSERT_EQ(nx_uart_word_port_stop(&port), NX_SUCCESS);
    ByteState bytes = {};
    uint8_t storage[4] = {};
    ASSERT_EQ(initialize_bytes(&bytes, storage), NX_SUCCESS);
    auto byte_port = bytes_face(&bytes);
    reject_clock_off = true;
    EXPECT_EQ(nx_uart_port_stop(&byte_port), NX_ERROR_BUSY);
    EXPECT_TRUE(bytes.initialized);
    EXPECT_EQ(bytes.controller, &nx_gd32_usart0_controller);
    reject_clock_off = false;
    EXPECT_EQ(nx_uart_port_stop(&byte_port), NX_SUCCESS);
}
#endif

TEST_F(UartFormatProvider, ByteTxeRefusalNeverReadsPastTheBorrowedLength) {
    ASSERT_EQ(nx_uart_word_port_stop(&port), NX_SUCCESS);
    ByteState bytes = {};
    uint8_t storage[4] = {};
    ASSERT_EQ(initialize_bytes(&bytes, storage), NX_SUCCESS);
    auto byte_port = bytes_face(&bytes);
    uint8_t backing[3] = {0x11U, 0x22U, 0xABU};
    nx_uart_tx_request_t tx = {};
    nx_request_initialize(&tx.base);
    ASSERT_EQ(nx_uart_tx_prepare(&tx, backing, 2U, 1000U), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&byte_port, &tx), NX_SUCCESS);
#if defined(NX_UART_FORMAT_STM32)
    refused_write = &registers.CR1;
    held_set = USART_CR1_TXEIE;
    for (unsigned index = 0U; index != 3U; ++index) {
        const_cast<uint32_t&>(registers.SR) = USART_SR_TXE;
        nx_stm32_uart_format_irq(&bytes);
    }
    EXPECT_EQ(registers.DR, 0x22U);
#else
    refused_write = &USART_CTL0(USART0);
    held_set = USART_CTL0_TBEIE;
    for (unsigned index = 0U; index != 3U; ++index) {
        USART_STAT0(USART0) = USART_STAT0_TBE;
        nx_gd32_uart_format_irq(&bytes);
    }
    EXPECT_EQ(USART_DATA(USART0), 0x22U);
#endif
    EXPECT_EQ(bytes.tx_position, 2U);
    refused_write = nullptr;
#if defined(NX_UART_FORMAT_STM32)
    registers.CR1 &= ~static_cast<uint32_t>(USART_CR1_TXEIE);
    const_cast<uint32_t&>(registers.SR) = USART_SR_TC;
#else
    USART_CTL0(USART0) &= ~static_cast<uint32_t>(USART_CTL0_TBEIE);
    USART_STAT0(USART0) = USART_STAT0_TC;
#endif
    EXPECT_EQ(nx_uart_port_cancel(&byte_port, &tx), NX_SUCCESS);
    nx_uart_port_service(&byte_port);
    EXPECT_EQ(nx_uart_port_stop(&byte_port), NX_SUCCESS);
}
