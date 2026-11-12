/**
 * \file            uart_formats_provider_test.cpp
 * \brief           Production UART register formats and nine-bit IRQ payloads.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f407_uart_word.h"
}
#include "nexus/io/native/model.h"
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
}
} /* namespace */

class UartFormatProvider : public testing::Test {
  protected:
    uint16_t rx[4] = {};
    uint16_t payload[2] = {0x1FFU, 0x100U};
    nx_uart_word_tx_request_t request = {};
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

TEST_F(UartFormatProvider, UartFourAndFiveRejectHardwareFlowWithoutEffects) {
    for (IRQn_Type irq : {UART4_IRQn, UART5_IRQn}) {
        reset_ports();
        USART_TypeDef hardware = {};
        uint8_t storage[4] = {};
        ByteState bytes = {};
        bytes.registers = &hardware;
        bytes.irq = irq;
        bytes.baud = 115200U;
        bytes.rx_storage = storage;
        bytes.rx_capacity = sizeof(storage);
        bytes.profile = NX_UART_RX_BYTES;
        const nx_uart_format_t flow = {8U, NX_UART_PARITY_NONE, NX_UART_STOP_1,
                                       NX_UART_FLOW_RTS_CTS};
        EXPECT_EQ(nx_stm32_uart_initialize_format(&bytes, 42000000U, flow),
                  NX_ERROR_UNSUPPORTED);
        EXPECT_FALSE(bytes.initialized);
        EXPECT_EQ(hardware.CR1, 0U);
        EXPECT_EQ(hardware.CR3, 0U);
        EXPECT_FALSE(model_irq_enabled);

        ASSERT_EQ(nx_stm32_uart_initialize(&bytes, 42000000U), NX_SUCCESS);
        const uint32_t before1 = hardware.CR1;
        const uint32_t before3 = hardware.CR3;
        EXPECT_EQ(nx_stm32_uart_flow_configure(&bytes, true, true),
                  NX_ERROR_UNSUPPORTED);
        EXPECT_EQ(nx_stm32_uart_format_configure(&bytes, flow),
                  NX_ERROR_UNSUPPORTED);
        EXPECT_EQ(hardware.CR1, before1);
        EXPECT_EQ(hardware.CR3, before3);
        EXPECT_FALSE(bytes.closing);
        ASSERT_EQ(nx_stm32_uart_stop(&bytes), NX_SUCCESS);
    }
}

TEST_F(UartFormatProvider, ByteSettlementRetainsBorrowUntilTxSourcesReadQuiet) {
    ASSERT_EQ(nx_stm32_uart_word_ops.stop(&state), NX_SUCCESS);
    reset_ports();
    registers = {};
    ByteState bytes = {};
    uint8_t storage[4] = {};
    const uint8_t data[2] = {0x12U, 0x34U};
    ASSERT_EQ(initialize_bytes(&bytes, storage), NX_SUCCESS);
    auto face = bytes_face(&bytes);
    nx_uart_tx_request_t transfer = {};
    nx_request_initialize(&transfer.base);
    ASSERT_EQ(nx_uart_tx_prepare(&transfer, data, 2U, 1000U), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&face, &transfer), NX_SUCCESS);
    registers.SR = USART_SR_TXE;
    nx_stm32_uart_format_irq(&bytes);
    refused_write = &registers.CR1;
    held_set = USART_CR1_TXEIE;
    ASSERT_EQ(nx_uart_port_cancel(&face, &transfer), NX_SUCCESS);
    registers.SR = USART_SR_TXE | USART_SR_TC;
    nx_stm32_uart_format_irq(&bytes);
    EXPECT_EQ(bytes.tx_position, 1U);
    nx_uart_port_service(&face);
    EXPECT_NE(nx_request_state(&transfer.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(bytes.active, &transfer);
    refused_write = nullptr;
    registers.SR = USART_SR_TC;
    nx_stm32_uart_format_irq(&bytes);
    nx_uart_port_service(&face);
    EXPECT_EQ(nx_request_state(&transfer.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(transfer.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(bytes.active, nullptr);
    ASSERT_EQ(nx_stm32_uart_stop(&bytes), NX_SUCCESS);
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

TEST_F(UartFormatProvider, ExhaustedWordDrainBudgetRetainsTheInflightWord) {
    ASSERT_EQ(nx_uart_word_tx_prepare(&request, payload, 2U, NX_DEADLINE_NEVER),
              NX_SUCCESS);
    ASSERT_EQ(nx_uart_word_port_submit(&port, &request), NX_SUCCESS);
    irq(empty);
    advance(UINT64_MAX - 11U);
    ASSERT_EQ(nx_uart_word_port_cancel(&port, &request), NX_SUCCESS);
    nx_uart_word_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(state.active, &request);
    irq(complete);
    nx_uart_word_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
}

TEST_F(UartFormatProvider, ExhaustedByteDrainBudgetRetainsTheInflightByte) {
    ASSERT_EQ(nx_stm32_uart_word_ops.stop(&state), NX_SUCCESS);
    reset_ports();
    registers = {};
    ByteState bytes = {};
    uint8_t storage[4] = {};
    const uint8_t data[2] = {0x12U, 0x34U};
    ASSERT_EQ(initialize_bytes(&bytes, storage), NX_SUCCESS);
    auto face = bytes_face(&bytes);
    nx_uart_tx_request_t transfer = {};
    nx_request_initialize(&transfer.base);
    ASSERT_EQ(nx_uart_tx_prepare(&transfer, data, 2U, NX_DEADLINE_NEVER),
              NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&face, &transfer), NX_SUCCESS);
    registers.SR = USART_SR_TXE;
    nx_stm32_uart_format_irq(&bytes);
    advance(UINT64_MAX - 11U);
    ASSERT_EQ(nx_uart_port_cancel(&face, &transfer), NX_SUCCESS);
    nx_uart_port_service(&face);
    EXPECT_EQ(nx_request_state(&transfer.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(bytes.active, &transfer);
    registers.SR = USART_SR_TC;
    nx_stm32_uart_format_irq(&bytes);
    nx_uart_port_service(&face);
    EXPECT_EQ(nx_request_state(&transfer.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(transfer.base.result, NX_ERROR_CANCELLED);
    ASSERT_EQ(nx_stm32_uart_stop(&bytes), NX_SUCCESS);
}

TEST_F(UartFormatProvider, RxPreservesNinthBitWithoutByteFaceStorage) {
    registers.DR = 0x1AAU;
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

TEST(UartByteFormat, LiveRtsFrameChangesRejectWithoutEffects) {
    for (const bool prior_rts : {false, true}) {
        SCOPED_TRACE(prior_rts);
        reset_ports();
        USART_TypeDef registers = {};
        nx_stm32_uart_state_t state = {};
        uint8_t rx[4] = {};
        state.registers = &registers;
        state.irq = USART1_IRQn;
        state.baud = 115200U;
        state.rx_storage = rx;
        state.rx_capacity = sizeof(rx);
        state.profile = NX_UART_RX_BYTES;
        ASSERT_EQ(nx_stm32_uart_initialize(&state, 84000000U), NX_SUCCESS);
        if (prior_rts) {
            ASSERT_EQ(nx_stm32_uart_flow_configure(&state, true, false),
                      NX_SUCCESS);
        }
        unsigned char before_state[sizeof(state)] = {};
        unsigned char before_registers[sizeof(registers)] = {};
        std::memcpy(before_state, &state, sizeof(state));
        std::memcpy(before_registers, &registers, sizeof(registers));
        const nx_uart_format_t selected = {
            8U, NX_UART_PARITY_EVEN, NX_UART_STOP_2,
            prior_rts ? NX_UART_FLOW_NONE : NX_UART_FLOW_RTS};
        EXPECT_EQ(nx_stm32_uart_format_configure(&state, selected),
                  NX_ERROR_UNSUPPORTED);
        EXPECT_EQ(std::memcmp(before_state, &state, sizeof(state)), 0);
        EXPECT_EQ(std::memcmp(before_registers, &registers, sizeof(registers)),
                  0);
        ASSERT_EQ(nx_stm32_uart_stop(&state), NX_SUCCESS);
    }
}

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

/** \brief This model boundary observes and performs the actual UE/UEN write. */
extern "C" void nx_uart_format_model_enable(uintptr_t base, uint32_t control) {
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
}

TEST_F(UartFormatProvider, ReadRejectsActualUartControlRegisters) {
    registers.DR = 0x1AAU;
    irq(ready);
    auto* control = &registers.CR1;
    auto* aligned_count = &registers.CR2;
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
    other.registers = &registers;
    other.irq = USART1_IRQn;
    EXPECT_EQ(
        nx_stm32_uart_word_initialize(
            &other, 84000000U, 115200U,
            reinterpret_cast<uint16_t*>(const_cast<uint32_t*>(&registers.CR1)),
            1U),
        NX_ERROR_INVALID);
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
    other.registers = &registers;
    other.irq = USART1_IRQn;
    ASSERT_EQ(
        nx_stm32_uart_word_initialize(&other, 84000000U, 115200U, other_rx, 2U),
        NX_SUCCESS);
    uint32_t before = registers.CR1;
    EXPECT_EQ(nx_uart_word_port_stop(&port), NX_SUCCESS);
    EXPECT_EQ(registers.CR1, before);
    auto cleanup = port;
    cleanup.context = &other;
    EXPECT_EQ(nx_uart_word_port_stop(&cleanup), NX_SUCCESS);
}

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
    registers.DR = 0x1ABU;
    auto* data = &registers.DR;
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
    registers.BRR = 8750U;
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
    return 0U;
}
/** \brief Clock disable can fail independently of USART and NVIC withdrawal. */
extern "C" void nx_uart_format_model_clock(bool enabled, uint32_t clock) {
    (void)enabled;
    (void)clock;
}

TEST_F(UartFormatProvider, SettlementWaitsForTxInterruptWithdrawalReadback) {
    ASSERT_EQ(nx_uart_word_tx_prepare(&request, payload, 2U, 1000U),
              NX_SUCCESS);
    ASSERT_EQ(nx_uart_word_port_submit(&port, &request), NX_SUCCESS);
    irq(empty);
    irq(empty);
    irq(complete);
    registers.CR1 |= USART_CR1_TCIE;
    refused_write = &registers.CR1;
    held_set = USART_CR1_TCIE;
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
    refused_write = &registers.CR1;
    held_clear = USART_CR1_UE;
    EXPECT_EQ(nx_stm32_uart_word_initialize(&state, 84000000U, 115200U, rx, 4U),
              NX_ERROR_IO);
    refused_write = nullptr;
    EXPECT_EQ(nx_uart_word_port_stop(&port), NX_SUCCESS);
}

TEST_F(UartFormatProvider, ColdInitRejectsRefusedNvicEnableOrPriority) {
    ASSERT_EQ(nx_uart_word_port_stop(&port), NX_SUCCESS);
    reject_irq_enable = true;
    EXPECT_EQ(nx_stm32_uart_word_initialize(&state, 84000000U, 115200U, rx, 4U),
              NX_ERROR_IO);
    reject_irq_enable = false;
    EXPECT_EQ(nx_uart_word_port_stop(&port), NX_SUCCESS);
    reject_irq_priority = true;
    model_irq_priority = 0U;
    EXPECT_EQ(nx_stm32_uart_word_initialize(&state, 84000000U, 115200U, rx, 4U),
              NX_ERROR_IO);
    reject_irq_priority = false;
    EXPECT_EQ(nx_uart_word_port_stop(&port), NX_SUCCESS);
}

TEST_F(UartFormatProvider, FrameChangeConfirmsUeWithdrawalBeforeFormatWrites) {
    ASSERT_EQ(nx_uart_word_port_stop(&port), NX_SUCCESS);
    uint8_t byte_rx[4] = {};
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
    refused_write = nullptr;
    EXPECT_EQ(nx_uart_port_stop(&byte_port), NX_SUCCESS);
}

TEST_F(UartFormatProvider, FailedFrameRollbackClosesFurtherAdmission) {
    ASSERT_EQ(nx_uart_word_port_stop(&port), NX_SUCCESS);
    uint8_t byte_rx[4] = {};
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
    refused_write = &registers.CR1;
    held_set = USART_CR1_TXEIE;
    irq(empty);
    irq(empty);
    irq(empty);
    EXPECT_EQ(state.position, 2U);
    EXPECT_EQ(data(), 0x102U);
    refused_write = nullptr;
    registers.CR1 &= ~static_cast<uint32_t>(USART_CR1_TXEIE);
    irq(complete);
    nx_uart_word_port_service(&port);
}

TEST_F(UartFormatProvider, CancelCannotLoadMoreWordsWhenTxeDisableIsRefused) {
    ASSERT_EQ(nx_uart_word_tx_prepare(&request, payload, 2U, 1000U),
              NX_SUCCESS);
    ASSERT_EQ(nx_uart_word_port_submit(&port, &request), NX_SUCCESS);
    irq(empty);
    refused_write = &registers.CR1;
    held_set = USART_CR1_TXEIE;
    ASSERT_EQ(nx_uart_word_port_cancel(&port, &request), NX_SUCCESS);
    irq(empty);
    EXPECT_EQ(state.position, 1U);
    refused_write = nullptr;
    registers.CR1 &= ~static_cast<uint32_t>(USART_CR1_TXEIE);
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
        model_irq_priority = 0U;
        nx_result_t result = initialize_bytes(&bytes, storage);
        EXPECT_NE(result, NX_SUCCESS);
        if (result != NX_SUCCESS && bytes.initialized) {
            EXPECT_TRUE(bytes.closing);
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
    refused_write = &registers.CR1;
    held_set = USART_CR1_TXEIE;
    for (unsigned index = 0U; index != 3U; ++index) {
        const_cast<uint32_t&>(registers.SR) = USART_SR_TXE;
        nx_stm32_uart_format_irq(&bytes);
    }
    EXPECT_EQ(registers.DR, 0x22U);
    EXPECT_EQ(bytes.tx_position, 2U);
    refused_write = nullptr;
    registers.CR1 &= ~static_cast<uint32_t>(USART_CR1_TXEIE);
    const_cast<uint32_t&>(registers.SR) = USART_SR_TC;
    EXPECT_EQ(nx_uart_port_cancel(&byte_port, &tx), NX_SUCCESS);
    nx_uart_port_service(&byte_port);
    EXPECT_EQ(nx_uart_port_stop(&byte_port), NX_SUCCESS);
}
