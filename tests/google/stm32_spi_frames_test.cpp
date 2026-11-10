/**
 * \file            stm32_spi_frames_test.cpp
 * \brief           Real IRQ SPI frames, CRC and cancellation boundary contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f407_spi_frames.h"
nx_stm32_system_t g_nx_stm32_system;
GPIO_TypeDef g_nx_stm32_gpioa_model;
}
#include <array>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

static nx_time_us_t model_now;
static bool model_clock_running;
static volatile uint32_t* refused_register;
static uint32_t held_bits;
static uint32_t dropped_bits;
static bool held_only_withdrawal;
static bool held_reset;
static bool held_vector;
static std::array<uint32_t, 82U> enabled;
static std::array<uint32_t, 82U> priorities;
static unsigned transmitted;
static unsigned reset_commands;
static GPIO_TypeDef* gpio_banks[9];
static SPI_TypeDef* spi_banks[3];
static RCC_TypeDef* rcc_bank;
static bool late_receive_on_disable;
static bool clock_loss_on_receive;
static bool clock_loss_on_disable;
static unsigned invalid_time_queries;

extern "C" nx_time_us_t nx_time_now_us(void) {
    if (!g_nx_stm32_system.started ||
        g_nx_stm32_system.timer_error != NX_SUCCESS)
        ++invalid_time_queries;
    return model_clock_running ? ++model_now : model_now;
}

class SpiFrames : public testing::Test {
  protected:
    SPI_TypeDef registers = {};
    RCC_TypeDef rcc = {};
    GPIO_TypeDef gpio = {};
    nx_stm32_spi_frames_config_t config = {};
    nx_stm32_spi_frames_state_t state = {};
    nx_spi_frames_port_t port = {&nx_stm32_spi_frames_ops, &state};
    nx_spi_frames_request_t request = {};
    uint16_t tx[3] = {0x19AU, 0x1234U, 0xEE55U};
    uint16_t rx[3] = {};
    void SetUp() override {
        refused_register = nullptr;
        held_bits = 0U;
        dropped_bits = 0U;
        late_receive_on_disable = clock_loss_on_receive = false;
        clock_loss_on_disable = false;
        invalid_time_queries = 0U;
        held_only_withdrawal = false;
        held_reset = held_vector = false;
        transmitted = reset_commands = 0U;
        enabled.fill(0U);
        priorities.fill(0U);
        model_now = 100U;
        model_clock_running = true;
        std::memset(gpio_banks, 0, sizeof(gpio_banks));
        std::memset(spi_banks, 0, sizeof(spi_banks));
        gpio_banks[0] = &gpio;
        spi_banks[0] = &registers;
        rcc_bank = &rcc;
        rcc.CR = RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY;
        rcc.PLLCFGR = 0x07405408U;
        rcc.CFGR = 0x0000940AU;
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = &rcc;
        g_nx_stm32_system.core_hz = 168000000U;
        g_nx_stm32_system.started = true;
        g_nx_stm32_system.time_epoch = 1U;
        gpio.IDR = 1U << 4U;
        registers.SR = SPI_SR_TXE;
        config.registers = &registers;
        config.rcc = &rcc;
        config.nss_gpio = &gpio;
        config.sck_gpio = &gpio;
        config.sck_mask = 1U << 5U;
        config.nss_mask = 1U << 4U;
        config.clock_hz = 84000000U;
        config.maximum_hz = 500000U;
        config.wire.role = NX_SPI_FRAMES_MASTER;
        config.wire.lines = NX_SPI_FRAMES_TWO_LINE;
        config.wire.word_bits = 16U;
        config.wire.dummy = 0xFFFFU;
        config.wire.irq_latency_us = 2U;
        config.irq = SPI1_IRQn;
        config.priority = 5U;
        nx_request_initialize(&request.base);
    }
    void Initialize() {
        ASSERT_EQ(nx_stm32_spi_frames_initialize(&state, &config), NX_SUCCESS);
    }
    void Prepare(const void* output, void* input, size_t words) {
        ASSERT_EQ(nx_spi_frames_prepare(&port, &request, output, input, words,
                                        10000U),
                  NX_SUCCESS);
    }
    void Submit() {
        ASSERT_EQ(nx_stm32_spi_frames_ops.submit(&state, &request), NX_SUCCESS);
    }
    void Irq(uint32_t flags, uint16_t value = 0U) {
        registers.DR = value;
        registers.SR = flags;
        nx_stm32_spi_frames_irq(&state);
    }
    void Service() {
        nx_stm32_spi_frames_ops.service(&state);
    }
};

TEST_F(SpiFrames, MasterFullDuplexPreservesSixteenBitsUntilDistinctWireDrain) {
    Initialize();
    ASSERT_FALSE(HasFatalFailure());
    Prepare(tx, rx, 2U);
    Submit();
    ASSERT_FALSE(HasFatalFailure());
    EXPECT_NE(registers.CR1 & SPI_CR1_MSTR, 0U);
    EXPECT_NE(registers.CR1 & SPI_CR1_DFF, 0U);
    EXPECT_NE(registers.CR1 & SPI_CR1_SPE, 0U);
    EXPECT_EQ(registers.DR, tx[0]);
    Irq(SPI_SR_RXNE | SPI_SR_TXE | SPI_SR_BSY, 0xABCDU);
    EXPECT_EQ(rx[0], 0xABCDU);
    EXPECT_EQ(registers.DR, tx[1]);
    Irq(SPI_SR_RXNE | SPI_SR_TXE | SPI_SR_BSY, 0x5678U);
    Service();
    EXPECT_EQ(state.active, &request);
    EXPECT_EQ(rx[1], 0x5678U);
    registers.SR = SPI_SR_TXE;
    Service();
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, 2U);
    EXPECT_EQ(state.active, nullptr);
    EXPECT_EQ(registers.CR1 & SPI_CR1_SPE, 0U);
    EXPECT_EQ(registers.CR2 & (SPI_CR2_TXEIE | SPI_CR2_RXNEIE | SPI_CR2_ERRIE),
              0U);
}

TEST_F(SpiFrames, SlaveFullDuplexUsesNssInsteadOfTheErratumStuckBusyFlag) {
    config.wire.role = NX_SPI_FRAMES_SLAVE;
    Initialize();
    ASSERT_FALSE(HasFatalFailure());
    Prepare(tx, rx, 1U);
    Submit();
    ASSERT_FALSE(HasFatalFailure());
    gpio.IDR = 0U;
    Irq(SPI_SR_RXNE | SPI_SR_TXE | SPI_SR_BSY, 0xBEEFU);
    Service();
    EXPECT_EQ(state.active, &request);
    gpio.IDR = config.nss_mask;
    Service();
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(rx[0], 0xBEEFU);
}

TEST_F(SpiFrames, SingleWireDirectionChangesOnlyBetweenSettledRequests) {
    config.wire.lines = NX_SPI_FRAMES_ONE_WIRE;
    Initialize();
    ASSERT_FALSE(HasFatalFailure());
    Prepare(tx, nullptr, 1U);
    Submit();
    ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(registers.CR1 & (SPI_CR1_BIDIMODE | SPI_CR1_BIDIOE),
              SPI_CR1_BIDIMODE | SPI_CR1_BIDIOE);
    Irq(SPI_SR_TXE);
    Service();
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    Prepare(nullptr, rx, 1U);
    Submit();
    ASSERT_FALSE(HasFatalFailure());
    EXPECT_NE(registers.CR1 & SPI_CR1_BIDIMODE, 0U);
    EXPECT_EQ(registers.CR1 & SPI_CR1_BIDIOE, 0U);
    Irq(SPI_SR_RXNE, 0x9A5BU);
    Service();
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(rx[0], 0x9A5BU);
}

TEST_F(SpiFrames, TiFrameSyncAndCrcHaveASeparateFinalIntegrityFrame) {
    config.wire.format = NX_SPI_FRAMES_TI;
    config.wire.crc_polynomial = 0x1021U;
    Initialize();
    ASSERT_FALSE(HasFatalFailure());
    Prepare(tx, rx, 1U);
    Submit();
    ASSERT_FALSE(HasFatalFailure());
    EXPECT_NE(registers.CR2 & SPI_CR2_FRF, 0U);
    EXPECT_NE(registers.CR1 & SPI_CR1_CRCEN, 0U);
    EXPECT_NE(registers.CR1 & SPI_CR1_CRCNEXT, 0U);
    EXPECT_EQ(registers.CRCPR, 0x1021U);
    Irq(SPI_SR_RXNE | SPI_SR_TXE | SPI_SR_BSY, 0xCAFEU);
    Service();
    EXPECT_EQ(state.active, &request);
    EXPECT_EQ(rx[0], 0xCAFEU);
    Irq(SPI_SR_RXNE | SPI_SR_TXE, 0x1234U);
    Service();
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, 1U);
}

TEST_F(SpiFrames, CrcErrorAndCancellationNeverReleaseAnUnprovedSource) {
    config.wire.crc_polynomial = 0x1021U;
    Initialize();
    ASSERT_FALSE(HasFatalFailure());
    Prepare(tx, rx, 2U);
    Submit();
    ASSERT_FALSE(HasFatalFailure());
    Irq(SPI_SR_CRCERR | SPI_SR_TXE | SPI_SR_BSY);
    EXPECT_EQ(nx_stm32_spi_frames_ops.cancel(&state, &request), NX_SUCCESS);
    Service();
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(state.active, &request);
    registers.SR = SPI_SR_TXE;
    Service();
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
}

TEST_F(SpiFrames, RejectionsLeaveNoAdmissionOrHardwareEffect) {
    config.wire.crc_polynomial = 2U;
    EXPECT_EQ(nx_stm32_spi_frames_initialize(&state, &config),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(registers.CR1, 0U);
    EXPECT_EQ(registers.CR2, 0U);
    EXPECT_EQ(state.config, nullptr);
    config.wire.crc_polynomial = 0U;
    Initialize();
    ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(nx_spi_frames_prepare(&port, &request, tx, tx, 1U, 10000U),
              NX_ERROR_INVALID);
    ASSERT_EQ(nx_request_prepare(&request.base, 10000U), NX_SUCCESS);
    request.tx = request.rx = tx;
    request.words = 1U;
    uint32_t before1 = registers.CR1;
    uint32_t before2 = registers.CR2;
    EXPECT_EQ(nx_stm32_spi_frames_ops.submit(&state, &request),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(state.active, nullptr);
    EXPECT_EQ(registers.CR1, before1);
    EXPECT_EQ(registers.CR2, before2);
}

TEST_F(SpiFrames, PersistentErrorDoesNotPostponeTheBoundedResetDrain) {
    Initialize();
    ASSERT_FALSE(HasFatalFailure());
    Prepare(tx, rx, 2U);
    Submit();
    ASSERT_FALSE(HasFatalFailure());
    Irq(SPI_SR_OVR | SPI_SR_BSY);
    nx_time_us_t first_deadline = state.drain_deadline;
    model_now = first_deadline + 1U;
    Service();
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(state.active, nullptr);
    EXPECT_TRUE(state.fault);
    EXPECT_EQ(nx_stm32_spi_frames_ops.recover(&state), NX_SUCCESS);
    EXPECT_FALSE(state.fault);
}

TEST_F(SpiFrames, SlaveSingleWireTransmitDisablesOnTheFinalTxeThenWaitsBusy) {
    config.wire.role = NX_SPI_FRAMES_SLAVE;
    config.wire.lines = NX_SPI_FRAMES_ONE_WIRE;
    Initialize();
    ASSERT_FALSE(HasFatalFailure());
    Prepare(tx, nullptr, 2U);
    Submit();
    ASSERT_FALSE(HasFatalFailure());
    gpio.IDR = 0U;
    Irq(SPI_SR_TXE | SPI_SR_BSY);
    EXPECT_EQ(registers.DR, tx[1]);
    EXPECT_NE(registers.CR2 & SPI_CR2_TXEIE, 0U);
    Irq(SPI_SR_TXE | SPI_SR_BSY);
    EXPECT_EQ(registers.CR1 & SPI_CR1_SPE, 0U);
    gpio.IDR = config.nss_mask;
    Service();
    EXPECT_EQ(state.active, &request);
    registers.SR = SPI_SR_TXE;
    Service();
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, 2U);
}

TEST_F(SpiFrames, RefusedReceiveClockWithdrawalCannotBecomeSuccess) {
    config.wire.lines = NX_SPI_FRAMES_RECEIVE_ONLY;
    Initialize();
    ASSERT_FALSE(HasFatalFailure());
    Prepare(nullptr, rx, 1U);
    refused_register = &registers.CR1;
    held_bits = SPI_CR1_SPE;
    held_only_withdrawal = true;
    Submit();
    ASSERT_FALSE(HasFatalFailure());
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    refused_register = nullptr;
    Irq(SPI_SR_RXNE, 0xACEDU);
    Service();
    model_now += 1000U;
    Service();
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
}

TEST_F(SpiFrames, FrozenReceiveTailClockQuarantinesUntilResetIsProved) {
    config.wire.lines = NX_SPI_FRAMES_RECEIVE_ONLY;
    Initialize();
    ASSERT_FALSE(HasFatalFailure());
    Prepare(nullptr, rx, 1U);
    model_clock_running = false;
    Submit();
    ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    held_reset = true;
    Service();
    EXPECT_EQ(state.active, &request);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    held_reset = false;
    Service();
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
}

TEST_F(SpiFrames, EightBitReceiveOnlyCrcExcludesTheIntegrityWord) {
    config.wire.lines = NX_SPI_FRAMES_RECEIVE_ONLY;
    config.wire.word_bits = 8U;
    config.wire.dummy = 0xFFU;
    config.wire.crc_polynomial = 7U;
    uint8_t input[2] = {};
    Initialize();
    ASSERT_FALSE(HasFatalFailure());
    Prepare(nullptr, input, 2U);
    Submit();
    ASSERT_FALSE(HasFatalFailure());
    Irq(SPI_SR_RXNE | SPI_SR_BSY, 0x52U);
    EXPECT_NE(registers.CR1 & SPI_CR1_CRCNEXT, 0U);
    Irq(SPI_SR_RXNE | SPI_SR_BSY, 0xD7U);
    EXPECT_EQ(registers.CR1 & SPI_CR1_SPE, 0U);
    Service();
    EXPECT_EQ(state.active, &request);
    Irq(SPI_SR_RXNE | SPI_SR_BSY, 0x13U);
    Service();
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, 2U);
    EXPECT_EQ(input[0], 0x52U);
    EXPECT_EQ(input[1], 0xD7U);
}

TEST_F(SpiFrames, FailedSourceWithdrawalRetainsAndNeverReadsFurtherPayload) {
    Initialize();
    ASSERT_FALSE(HasFatalFailure());
    Prepare(tx, rx, 2U);
    Submit();
    ASSERT_FALSE(HasFatalFailure());
    refused_register = &registers.CR2;
    held_bits = SPI_CR2_TXEIE;
    EXPECT_EQ(nx_stm32_spi_frames_ops.cancel(&state, &request), NX_SUCCESS);
    Irq(SPI_SR_TXE);
    EXPECT_EQ(transmitted, 1U);
    Service();
    EXPECT_EQ(state.active, &request);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    refused_register = nullptr;
    Service();
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(nx_stm32_spi_frames_ops.stop(&state), NX_SUCCESS);
    EXPECT_EQ(enabled[SPI1_IRQn], 0U);
}

TEST_F(SpiFrames, ColdOwnerRestoresPadsRegistersPriorityAndUnrelatedClocks) {
    uint16_t snapshots[4] = {};
    nx_stm32_spi_pin_config_t pins[] = {{&gpio, &snapshots[0], 0U, 5U, 5U},
                                        {&gpio, &snapshots[1], 0U, 6U, 5U},
                                        {&gpio, &snapshots[2], 0U, 7U, 5U},
                                        {&gpio, &snapshots[3], 0U, 4U, 5U}};
    nx_stm32_spi_frames_cold_config_t cold = {config, pins, 4U};
    nx_stm32_spi_frames_cold_t receipt = {};
    rcc.AHB1ENR = 1U << 3U;
    rcc.APB2ENR = RCC_APB2ENR_TIM1EN;
    registers.CR1 = SPI_CR1_CPOL;
    registers.CRCPR = 0x65U;
    priorities[SPI1_IRQn] = 12U;
    gpio.MODER = 0xFFFFFFFFU;
    GPIO_TypeDef original = gpio;
    ASSERT_EQ(nx_stm32_spi_frames_admit(&state, &cold, &receipt), NX_SUCCESS);
    ASSERT_EQ(nx_stm32_spi_frames_prepare(&state, &cold, &receipt), NX_SUCCESS);
    EXPECT_EQ(rcc.APB2ENR, RCC_APB2ENR_TIM1EN | RCC_APB2ENR_SPI1EN);
    EXPECT_EQ((gpio.MODER >> 10U) & 3U, 2U);
    EXPECT_EQ((gpio.OSPEEDR >> 10U) & 3U, 2U);
    EXPECT_EQ(nx_stm32_spi_frames_close(&state, &cold, &receipt), NX_SUCCESS);
    EXPECT_EQ(registers.CR1, SPI_CR1_CPOL);
    EXPECT_EQ(registers.CRCPR, 0x65U);
    EXPECT_EQ(rcc.APB2ENR, RCC_APB2ENR_TIM1EN);
    EXPECT_EQ(rcc.AHB1ENR, 1U << 3U);
    EXPECT_EQ(priorities[SPI1_IRQn], 12U);
    EXPECT_EQ(gpio.MODER, original.MODER);
    EXPECT_EQ(gpio.OSPEEDR, original.OSPEEDR);
    EXPECT_EQ(gpio.AFR[0], original.AFR[0]);
    EXPECT_EQ(state.config, nullptr);
    EXPECT_FALSE(receipt.owned);
}

TEST_F(SpiFrames, ColdRestoreRefusalRetainsTheGateAndReceiptForRetry) {
    uint16_t snapshots[4] = {};
    nx_stm32_spi_pin_config_t pins[] = {{&gpio, &snapshots[0], 0U, 5U, 5U},
                                        {&gpio, &snapshots[1], 0U, 6U, 5U},
                                        {&gpio, &snapshots[2], 0U, 7U, 5U},
                                        {&gpio, &snapshots[3], 0U, 4U, 5U}};
    nx_stm32_spi_frames_cold_config_t cold = {config, pins, 4U};
    nx_stm32_spi_frames_cold_t receipt = {};
    ASSERT_EQ(nx_stm32_spi_frames_prepare(&state, &cold, &receipt), NX_SUCCESS);
    refused_register = &registers.CR1;
    held_bits = SPI_CR1_SPE;
    EXPECT_NE(nx_stm32_spi_frames_close(&state, &cold, &receipt), NX_SUCCESS);
    EXPECT_TRUE(receipt.owned);
    EXPECT_NE(rcc.APB2ENR & RCC_APB2ENR_SPI1EN, 0U);
    EXPECT_EQ(state.config, &cold.engine);
    refused_register = nullptr;
    EXPECT_EQ(nx_stm32_spi_frames_close(&state, &cold, &receipt), NX_SUCCESS);
    EXPECT_FALSE(receipt.owned);
    EXPECT_EQ(rcc.APB2ENR & RCC_APB2ENR_SPI1EN, 0U);
}

TEST_F(SpiFrames, LaterPadDisconnectFailureCannotReturnSuccessfulAcquisition) {
    uint16_t snapshots[4] = {};
    nx_stm32_spi_pin_config_t pins[] = {{&gpio, &snapshots[0], 0U, 5U, 5U},
                                        {&gpio, &snapshots[1], 0U, 6U, 5U},
                                        {&gpio, &snapshots[2], 0U, 7U, 5U},
                                        {&gpio, &snapshots[3], 0U, 4U, 5U}};
    nx_stm32_spi_frames_cold_config_t cold = {config, pins, 4U};
    nx_stm32_spi_frames_cold_t receipt = {};
    refused_register = &gpio.MODER;
    dropped_bits = 3U << 12U;
    EXPECT_EQ(nx_stm32_spi_frames_prepare(&state, &cold, &receipt),
              NX_ERROR_IO);
    EXPECT_TRUE(receipt.owned);
    EXPECT_NE(rcc.APB2ENR & RCC_APB2ENR_SPI1EN, 0U);
    refused_register = nullptr;
    EXPECT_EQ(nx_stm32_spi_frames_close(&state, &cold, &receipt), NX_SUCCESS);
}

TEST_F(SpiFrames, ColdReceiptPinsAndSnapshotsCannotBeBorrowedAsPayload) {
    uint16_t snapshots[4] = {};
    nx_stm32_spi_pin_config_t pins[] = {{&gpio, &snapshots[0], 0U, 5U, 5U},
                                        {&gpio, &snapshots[1], 0U, 6U, 5U},
                                        {&gpio, &snapshots[2], 0U, 7U, 5U},
                                        {&gpio, &snapshots[3], 0U, 4U, 5U}};
    nx_stm32_spi_frames_cold_config_t cold = {config, pins, 4U};
    nx_stm32_spi_frames_cold_t receipt = {};
    ASSERT_EQ(nx_stm32_spi_frames_prepare(&state, &cold, &receipt), NX_SUCCESS);
    for (void* storage :
         {static_cast<void*>(&receipt), static_cast<void*>(pins),
          static_cast<void*>(snapshots)}) {
        EXPECT_EQ(
            nx_spi_frames_prepare(&port, &request, tx, storage, 1U, 10000U),
            NX_ERROR_INVALID);
        ASSERT_EQ(nx_request_prepare(&request.base, 10000U), NX_SUCCESS);
        request.tx = tx;
        request.rx = storage;
        request.words = 1U;
        EXPECT_EQ(nx_stm32_spi_frames_ops.submit(&state, &request),
                  NX_ERROR_INVALID);
        EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
        EXPECT_EQ(state.active, nullptr);
        if (state.active != nullptr)
            break;
    }
}

TEST_F(SpiFrames, PublicPreparationAndWrongContextCannotRevokeAnActiveBorrow) {
    Initialize();
    ASSERT_FALSE(HasFatalFailure());
    Prepare(tx, rx, 2U);
    ASSERT_EQ(nx_spi_frames_submit(&port, &request), NX_SUCCESS);
    EXPECT_EQ(
        nx_spi_frames_prepare(&port, &request, tx + 1U, rx + 1U, 1U, 1000U),
        NX_ERROR_STATE);
    EXPECT_EQ(request.tx, tx);
    EXPECT_EQ(request.rx, rx);
    EXPECT_EQ(request.words, 2U);
    nx_arch_irq_state_t incoming = nx_arch_irq_save();
    EXPECT_EQ(nx_spi_frames_cancel(&port, &request), NX_ERROR_CONTEXT);
    EXPECT_EQ(nx_spi_frames_stop(&port), NX_ERROR_CONTEXT);
    nx_spi_frames_service(&port);
    EXPECT_EQ(state.active, &request);
    nx_arch_irq_restore(incoming);
    EXPECT_EQ(nx_spi_frames_stop(&port), NX_ERROR_BUSY);
    registers.SR = SPI_SR_TXE;
    nx_spi_frames_service(&port);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(nx_spi_frames_stop(&port), NX_SUCCESS);
}

TEST_F(SpiFrames, SlaveBudgetUsesThePeersMaximumClockRatherThanMasterDivider) {
    config.wire.role = NX_SPI_FRAMES_SLAVE;
    config.wire.word_bits = 8U;
    config.wire.dummy = 0xFFU;
    config.wire.irq_latency_us = 6U;
    config.maximum_hz = 1000000U;
    EXPECT_EQ(nx_stm32_spi_frames_initialize(&state, &config),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(reset_commands, 0U);
    EXPECT_EQ(state.config, nullptr);
}

TEST_F(SpiFrames, ColdSnapshotCannotOverwriteItsOwnRestorationReceipt) {
    uint16_t snapshots[4] = {};
    nx_stm32_spi_frames_cold_t receipt = {};
    nx_stm32_spi_pin_config_t pins[] = {
        {&gpio, &snapshots[0], 0U, 5U, 5U},
        {&gpio, &snapshots[1], 0U, 6U, 5U},
        {&gpio, reinterpret_cast<uint16_t*>(&receipt.priority), 0U, 7U, 5U},
        {&gpio, &snapshots[3], 0U, 4U, 5U}};
    nx_stm32_spi_frames_cold_config_t cold = {config, pins, 4U};
    EXPECT_EQ(nx_stm32_spi_frames_prepare(&state, &cold, &receipt),
              NX_ERROR_INVALID);
    EXPECT_FALSE(receipt.owned);
    EXPECT_EQ(rcc.APB2ENR, 0U);
}

TEST_F(SpiFrames, SlowExternalSlaveClockHasNoMasterDivideBy256Floor) {
    config.wire.role = NX_SPI_FRAMES_SLAVE;
    config.maximum_hz = 1000U;
    Initialize();
    ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(nx_spi_frames_prepare(&port, &request, tx, rx, 1U, 1000000U),
              NX_SUCCESS);
    Submit();
    ASSERT_FALSE(HasFatalFailure());
    gpio.IDR = 0U;
    model_now += 16000U;
    Irq(SPI_SR_RXNE | SPI_SR_TXE | SPI_SR_BSY, 0x53A7U);
    gpio.IDR = config.nss_mask;
    Service();
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(rx[0], 0x53A7U);
}

TEST_F(SpiFrames, ReceiveDisableBudgetIncludesTheTimersFullRoundingWindow) {
    config.wire.lines = NX_SPI_FRAMES_RECEIVE_ONLY;
    config.wire.word_bits = 8U;
    config.wire.dummy = 0xFFU;
    config.wire.irq_latency_us = 6U;
    config.maximum_hz = 1000000U;
    EXPECT_EQ(nx_stm32_spi_frames_initialize(&state, &config),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(reset_commands, 0U);
    EXPECT_EQ(state.config, nullptr);
}

TEST_F(SpiFrames, ReadyDeadlineCannotCrossTheSystemEpoch) {
    Initialize();
    ASSERT_FALSE(HasFatalFailure());
    Prepare(tx, rx, 1U);
    ++g_nx_stm32_system.time_epoch;
    EXPECT_EQ(nx_spi_frames_submit(&port, &request), NX_ERROR_STATE);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(transmitted, 0U);
    EXPECT_EQ(state.active, nullptr);
}

TEST_F(SpiFrames, ClockLossInIrqStopsAllPayloadAccessAndStillAllowsCleanup) {
    Initialize();
    ASSERT_FALSE(HasFatalFailure());
    Prepare(tx, rx, 2U);
    Submit();
    ASSERT_FALSE(HasFatalFailure());
    g_nx_stm32_system.started = false;
    model_clock_running = false;
    Irq(SPI_SR_RXNE | SPI_SR_TXE | SPI_SR_BSY, 0xAA55U);
    EXPECT_EQ(rx[0], 0U);
    EXPECT_EQ(transmitted, 1U);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    Service();
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(nx_spi_frames_stop(&port), NX_SUCCESS);
    EXPECT_EQ(invalid_time_queries, 0U);
}

TEST_F(SpiFrames, ForeignLiveRccRejectsReadyBeforeAnyNewEffect) {
    Initialize();
    ASSERT_FALSE(HasFatalFailure());
    Prepare(tx, rx, 1U);
    rcc.CFGR ^= RCC_CFGR_PPRE1_0;
    uint32_t before1 = registers.CR1;
    uint32_t before2 = registers.CR2;
    EXPECT_EQ(nx_spi_frames_submit(&port, &request), NX_ERROR_STATE);
    EXPECT_EQ(registers.CR1, before1);
    EXPECT_EQ(registers.CR2, before2);
    EXPECT_EQ(transmitted, 0U);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
}

TEST_F(SpiFrames, ColdRestartInvalidatesTheOldReadyGeneration) {
    uint16_t snapshots[4] = {};
    nx_stm32_spi_pin_config_t pins[] = {{&gpio, &snapshots[0], 0U, 5U, 5U},
                                        {&gpio, &snapshots[1], 0U, 6U, 5U},
                                        {&gpio, &snapshots[2], 0U, 7U, 5U},
                                        {&gpio, &snapshots[3], 0U, 4U, 5U}};
    nx_stm32_spi_frames_cold_config_t cold = {config, pins, 4U};
    nx_stm32_spi_frames_cold_t receipt = {};
    ASSERT_EQ(nx_stm32_spi_frames_prepare(&state, &cold, &receipt), NX_SUCCESS);
    Prepare(tx, rx, 1U);
    ASSERT_EQ(nx_stm32_spi_frames_close(&state, &cold, &receipt), NX_SUCCESS);
    ASSERT_EQ(nx_stm32_spi_frames_prepare(&state, &cold, &receipt), NX_SUCCESS);
    EXPECT_EQ(nx_spi_frames_submit(&port, &request), NX_ERROR_STATE);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(state.active, nullptr);
}

TEST_F(SpiFrames, ExtraRxneAfterTheDeclaredFinalFrameCannotBecomeSuccess) {
    config.wire.role = NX_SPI_FRAMES_SLAVE;
    Initialize();
    ASSERT_FALSE(HasFatalFailure());
    Prepare(tx, rx, 1U);
    Submit();
    ASSERT_FALSE(HasFatalFailure());
    gpio.IDR = 0U;
    Irq(SPI_SR_RXNE | SPI_SR_TXE | SPI_SR_BSY, 0x53A7U);
    registers.DR = 0xAA55U;
    registers.SR = SPI_SR_RXNE | SPI_SR_TXE | SPI_SR_BSY;
    gpio.IDR = config.nss_mask;
    Service();
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(rx[0], 0x53A7U);
}

TEST_F(SpiFrames, LateRxneDuringWithdrawalIsPreservedAsAnError) {
    Initialize();
    ASSERT_FALSE(HasFatalFailure());
    Prepare(tx, rx, 1U);
    Submit();
    ASSERT_FALSE(HasFatalFailure());
    Irq(SPI_SR_RXNE | SPI_SR_TXE, 0x53A7U);
    late_receive_on_disable = true;
    Service();
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(rx[0], 0x53A7U);
}

TEST_F(SpiFrames, ClockLossAtTheHardwareReadCannotWriteTheCallerPayload) {
    Initialize();
    ASSERT_FALSE(HasFatalFailure());
    Prepare(tx, rx, 2U);
    Submit();
    ASSERT_FALSE(HasFatalFailure());
    clock_loss_on_receive = true;
    Irq(SPI_SR_RXNE | SPI_SR_TXE | SPI_SR_BSY, 0xAA55U);
    EXPECT_EQ(rx[0], 0U);
    EXPECT_EQ(transmitted, 1U);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    Service();
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(invalid_time_queries, 0U);
}

TEST_F(SpiFrames, ClockLossDuringFinalWithdrawalCannotPublishSuccess) {
    Initialize();
    ASSERT_FALSE(HasFatalFailure());
    Prepare(tx, rx, 1U);
    Submit();
    ASSERT_FALSE(HasFatalFailure());
    Irq(SPI_SR_RXNE | SPI_SR_TXE, 0x53A7U);
    clock_loss_on_disable = true;
    Service();
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(rx[0], 0x53A7U);
    EXPECT_EQ(invalid_time_queries, 0U);
    EXPECT_EQ(nx_spi_frames_stop(&port), NX_SUCCESS);
}

extern "C" void nx_spi_frames_model_write(volatile uint32_t* address,
                                          uint32_t value) {
    bool refused = address == refused_register &&
                   (!held_only_withdrawal || (*address & held_bits) != 0U);
    uint32_t previous = *address;
    *address = refused ? (value | held_bits) & ~dropped_bits : value;
    for (auto* spi : spi_banks) {
        if (spi != nullptr && address == &spi->CR1 && clock_loss_on_disable &&
            (previous & SPI_CR1_SPE) != 0U && (value & SPI_CR1_SPE) == 0U)
            g_nx_stm32_system.started = false;
        if (spi != nullptr && address == &spi->CR1 && late_receive_on_disable &&
            (previous & SPI_CR1_SPE) != 0U && (value & SPI_CR1_SPE) == 0U) {
            spi->SR |= SPI_SR_RXNE;
            spi->DR = 0xB00BU;
        }
    }
    for (auto* gpio : gpio_banks) {
        if (gpio != nullptr && address == &gpio->BSRR)
            gpio->ODR = (gpio->ODR | (value & 0xFFFFU)) & ~(value >> 16U);
    }
}
extern "C" uintptr_t nx_spi_frames_model_gpio(unsigned port) {
    return port < 9U ? reinterpret_cast<uintptr_t>(gpio_banks[port]) : 0U;
}
extern "C" uintptr_t nx_spi_frames_model_instance(int irq) {
    unsigned index = irq == SPI1_IRQn ? 0U : irq == SPI2_IRQn ? 1U : 2U;
    return reinterpret_cast<uintptr_t>(spi_banks[index]);
}
extern "C" uintptr_t nx_spi_frames_model_rcc(void) {
    return reinterpret_cast<uintptr_t>(rcc_bank);
}
extern "C" void nx_spi_frames_model_tx(uintptr_t raw, uint16_t value,
                                       uint8_t bits) {
    auto* registers = reinterpret_cast<SPI_TypeDef*>(raw);
    registers->DR = bits == 8U ? value & 0xFFU : value;
    registers->SR &= ~static_cast<uint32_t>(SPI_SR_TXE);
    ++transmitted;
}
extern "C" uint16_t nx_spi_frames_model_rx(uintptr_t raw, uint8_t bits) {
    auto* registers = reinterpret_cast<SPI_TypeDef*>(raw);
    uint16_t value = static_cast<uint16_t>(registers->DR);
    registers->SR &= ~static_cast<uint32_t>(SPI_SR_RXNE);
    if (clock_loss_on_receive)
        g_nx_stm32_system.started = false;
    return bits == 8U ? static_cast<uint16_t>(value & 0xFFU) : value;
}
extern "C" uint32_t nx_spi_frames_model_irq(unsigned command, int irq,
                                            uint32_t value) {
    if (irq < 0 || irq >= 82)
        return 0U;
    auto index = static_cast<size_t>(irq);
    switch (command) {
        case 0U:
            enabled[index] = 1U;
            return 0U;
        case 1U:
            if (!held_vector)
                enabled[index] = 0U;
            return 0U;
        case 2U:
            return 0U;
        case 3U:
            priorities[index] = value;
            return 0U;
        case 4U:
            return enabled[index];
        case 5U:
            return 0U;
        case 6U:
            return 0U;
        case 7U:
            return priorities[index];
        default:
            return 0U;
    }
}
extern "C" void nx_spi_frames_model_reset(uintptr_t raw) {
    ++reset_commands;
    auto* registers = reinterpret_cast<SPI_TypeDef*>(raw);
    if (!held_reset) {
        std::memset(registers, 0, sizeof(*registers));
        registers->SR = SPI_SR_TXE;
    }
}
