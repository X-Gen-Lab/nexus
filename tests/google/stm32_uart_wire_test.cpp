/**
 * \file            stm32_uart_wire_test.cpp
 * \brief           Real USART wire-mode TX, RX, errors and cold admission
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f407_uart_wire.h"
}
#include "nexus/io/native/model.h"
#include <gtest/gtest.h>

class UartWire : public testing::Test {
  protected:
    USART_TypeDef registers = {};
    nx_stm32_uart_state_t state = {};
    nx_uart_rx_event_t storage[8] = {};
    nx_uart_port_t face = {&nx_stm32_uart_ops, &state};
    nx_uart_tx_request_t request = {};
    uint8_t payload[2] = {0xA5U, 0x5AU};
    void SetUp() override {
        ASSERT_EQ(nx_native_clock_configure(true, 1U), NX_SUCCESS);
        state.registers = &registers;
        state.irq = USART1_IRQn;
        state.baud = 9600U;
        state.rx_storage = storage;
        state.rx_capacity = 8U;
        state.profile = NX_UART_RX_EVENTS;
        nx_request_initialize(&request.base);
    }
    void TearDown() override {
        registers.CR1 = 0U;
        registers.CR3 = 0U;
        state.active = nullptr;
        (void)nx_stm32_uart_stop(&state);
    }
    nx_uart_wire_config_t config(nx_uart_wire_mode_t mode) {
        nx_uart_wire_config_t selected = {};
        selected.mode = mode;
        if (mode == NX_UART_WIRE_LIN)
            selected.lin_break_bits = 11U;
        if (mode == NX_UART_WIRE_IRDA)
            selected.prescaler = 42U;
        if (mode == NX_UART_WIRE_SMARTCARD) {
            selected.prescaler = 21U;
            selected.guard_time = 16U;
            selected.smartcard_nack = true;
        }
        return selected;
    }
    void irq(uint32_t status) {
        registers.SR = status;
        nx_stm32_uart_format_irq(&state);
    }
};

TEST_F(UartWire, EveryWirePersonalityStartsWithItsCompleteHardwareTuple) {
    for (auto mode :
         {NX_UART_WIRE_SYNCHRONOUS, NX_UART_WIRE_HALF_DUPLEX, NX_UART_WIRE_LIN,
          NX_UART_WIRE_IRDA, NX_UART_WIRE_SMARTCARD}) {
        auto selected = config(mode);
        ASSERT_EQ(nx_stm32_uart_wire_initialize(&state, 84000000U, &selected),
                  NX_SUCCESS);
        EXPECT_NE(registers.CR1 & USART_CR1_UE, 0U);
        EXPECT_EQ((registers.CR2 & USART_CR2_CLKEN) != 0U,
                  mode == NX_UART_WIRE_SYNCHRONOUS ||
                      mode == NX_UART_WIRE_SMARTCARD);
        EXPECT_EQ((registers.CR2 & USART_CR2_LINEN) != 0U,
                  mode == NX_UART_WIRE_LIN);
        EXPECT_EQ((registers.CR3 & USART_CR3_HDSEL) != 0U,
                  mode == NX_UART_WIRE_HALF_DUPLEX);
        EXPECT_EQ((registers.CR3 & USART_CR3_IREN) != 0U,
                  mode == NX_UART_WIRE_IRDA);
        EXPECT_EQ((registers.CR3 & USART_CR3_SCEN) != 0U,
                  mode == NX_UART_WIRE_SMARTCARD);
        if (mode == NX_UART_WIRE_SMARTCARD) {
            EXPECT_EQ(registers.CR1 & (USART_CR1_M | USART_CR1_PCE),
                      USART_CR1_M | USART_CR1_PCE);
            EXPECT_EQ(registers.CR2 & USART_CR2_STOP, USART_CR2_STOP);
            EXPECT_EQ(registers.GTPR, 0x1015U);
            EXPECT_NE(registers.CR3 & USART_CR3_NACK, 0U);
        }
        ASSERT_EQ(nx_stm32_uart_stop(&state), NX_SUCCESS);
    }
}

TEST_F(UartWire, AllModesTransmitReceiveCancelAndRestartThroughTheSameFace) {
    for (auto mode :
         {NX_UART_WIRE_SYNCHRONOUS, NX_UART_WIRE_HALF_DUPLEX, NX_UART_WIRE_LIN,
          NX_UART_WIRE_IRDA, NX_UART_WIRE_SMARTCARD}) {
        auto selected = config(mode);
        ASSERT_EQ(nx_stm32_uart_wire_initialize(&state, 84000000U, &selected),
                  NX_SUCCESS);
        nx_request_initialize(&request.base);
        ASSERT_EQ(nx_uart_tx_prepare(&request, payload, 1U, 1000U), NX_SUCCESS);
        ASSERT_EQ(nx_uart_port_submit(&face, &request), NX_SUCCESS);
        irq(USART_SR_TXE);
        EXPECT_EQ(registers.DR, payload[0]);
        ASSERT_EQ(nx_uart_port_cancel(&face, &request), NX_SUCCESS);
        nx_uart_port_service(&face);
        EXPECT_EQ(state.active, &request);
        EXPECT_EQ(nx_uart_port_stop(&face), NX_ERROR_BUSY);
        registers.DR = 0x3CU;
        irq(USART_SR_RXNE | USART_SR_TC);
        nx_uart_port_service(&face);
        EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
        EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
        nx_uart_rx_event_t event = {};
        size_t count = 0U;
        ASSERT_EQ(nx_uart_port_read_events(&face, &event, 1U, &count),
                  NX_SUCCESS);
        EXPECT_EQ(event.byte, 0x3CU);
        EXPECT_EQ(count, 1U);
        ASSERT_EQ(nx_uart_port_stop(&face), NX_SUCCESS);
        EXPECT_EQ(registers.CR2 & USART_CR2_LBDIE, 0U);
    }
}

TEST_F(UartWire, RejectsUnsupportedInstanceAndIllegalModeTupleBeforeEffects) {
    for (auto irq : {UART4_IRQn, UART5_IRQn}) {
        state.irq = irq;
        for (auto mode : {NX_UART_WIRE_SYNCHRONOUS, NX_UART_WIRE_SMARTCARD}) {
            auto selected = config(mode);
            EXPECT_EQ(
                nx_stm32_uart_wire_initialize(&state, 42000000U, &selected),
                NX_ERROR_UNSUPPORTED);
            EXPECT_EQ(registers.CR1, 0U);
            EXPECT_FALSE(state.initialized);
        }
    }
    state.irq = USART1_IRQn;
    auto invalid = config(NX_UART_WIRE_LIN);
    invalid.lin_break_bits = 9U;
    EXPECT_EQ(nx_stm32_uart_wire_initialize(&state, 84000000U, &invalid),
              NX_ERROR_INVALID);
    EXPECT_EQ(registers.CR1, 0U);
    invalid = config(NX_UART_WIRE_IRDA);
    state.baud = 230400U;
    EXPECT_EQ(nx_stm32_uart_wire_initialize(&state, 84000000U, &invalid),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(registers.CR1, 0U);
}

TEST_F(UartWire, SmartcardNackStopsTheCurrentCharacterAndSurfacesRxError) {
    auto selected = config(NX_UART_WIRE_SMARTCARD);
    ASSERT_EQ(nx_stm32_uart_wire_initialize(&state, 84000000U, &selected),
              NX_SUCCESS);
    ASSERT_EQ(nx_uart_tx_prepare(&request, payload, 2U, 1000U), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_submit(&face, &request), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(state.active, nullptr);
    ASSERT_EQ(nx_uart_tx_prepare(&request, payload, 1U, 1000U), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&face, &request), NX_SUCCESS);
    irq(USART_SR_TXE);
    irq(USART_SR_FE);
    nx_uart_port_service(&face);
    EXPECT_EQ(state.active, &request);
    irq(USART_SR_TC);
    nx_uart_port_service(&face);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_NE(storage[0].flags & NX_UART_EVENT_FRAMING, 0U);
}

TEST_F(UartWire, LinBreakCancellationRetainsBorrowUntilBreakAndWireDrain) {
    auto selected = config(NX_UART_WIRE_LIN);
    ASSERT_EQ(nx_stm32_uart_wire_initialize(&state, 84000000U, &selected),
              NX_SUCCESS);
    ASSERT_EQ(nx_uart_lin_break_prepare(&request, 1000U), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&face, &request), NX_SUCCESS);
    EXPECT_EQ(request.data, nullptr);
    EXPECT_EQ(request.length, 0U);
    EXPECT_NE(registers.CR1 & USART_CR1_SBK, 0U);
    irq(USART_SR_TC);
    nx_uart_port_service(&face);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    ASSERT_EQ(nx_uart_port_cancel(&face, &request), NX_SUCCESS);
    ASSERT_EQ(nx_native_clock_advance(30000U), NX_SUCCESS);
    nx_uart_port_service(&face);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(state.active, &request);
    EXPECT_EQ(nx_uart_port_stop(&face), NX_ERROR_BUSY);
    registers.CR1 &= ~static_cast<uint32_t>(USART_CR1_SBK);
    irq(USART_SR_TC);
    nx_uart_port_service(&face);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(state.active, nullptr);
    ASSERT_EQ(nx_uart_port_stop(&face), NX_SUCCESS);
    EXPECT_EQ(registers.CR2, 0U);
}

TEST_F(UartWire, LinBreakReceptionPreservesDelimiterAndFramingFacts) {
    auto selected = config(NX_UART_WIRE_LIN);
    ASSERT_EQ(nx_stm32_uart_wire_initialize(&state, 84000000U, &selected),
              NX_SUCCESS);
    registers.DR = 0U;
    irq(USART_SR_LBD | USART_SR_RXNE | USART_SR_FE);
    nx_uart_rx_event_t event = {};
    size_t count = 0U;
    ASSERT_EQ(nx_uart_port_read_events(&face, &event, 1U, &count), NX_SUCCESS);
    EXPECT_EQ(count, 1U);
    EXPECT_EQ(event.flags, NX_UART_EVENT_BREAK | NX_UART_EVENT_NO_BYTE |
                               NX_UART_EVENT_FRAMING);
    EXPECT_EQ(registers.SR & USART_SR_LBD, 0U);
}

TEST_F(UartWire, IrdaLowPowerRejectsAnInvalidPulseClockBeforeEnable) {
    auto selected = config(NX_UART_WIRE_IRDA);
    selected.irda_low_power = true;
    selected.prescaler = 1U;
    EXPECT_EQ(nx_stm32_uart_wire_initialize(&state, 84000000U, &selected),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(registers.CR1, 0U);
    selected.prescaler = 42U;
    ASSERT_EQ(nx_stm32_uart_wire_initialize(&state, 84000000U, &selected),
              NX_SUCCESS);
    EXPECT_EQ(registers.CR3 & (USART_CR3_IREN | USART_CR3_IRLP),
              USART_CR3_IREN | USART_CR3_IRLP);
}
