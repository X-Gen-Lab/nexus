/**
 * \file            stm32_static_lifecycle_test.cpp
 * \brief           Real F4 static acquisition, partial effects and cleanup
 * retry. \author          Nexus Team \version         1.0.0 \date 2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f4_lifecycle.h"
nx_stm32_system_t g_nx_stm32_system;
}
#include <array>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <vector>

namespace {
/** \brief Only the external bus decides whether a concrete write is accepted.
 */
class RegisterPort {
  public:
    MOCK_METHOD(bool, Accept, (volatile uint32_t*, uint32_t));
};
RegisterPort* register_port;

/** \brief Actual provider objects share one Platform pad-snapshot owner. */
class STM32StaticLifecycle : public testing::Test {
  protected:
    void* mapping = MAP_FAILED;
    uint16_t snapshots[7]{};
    nx_stm32_platform_pin_config_t pins[7]{
        {GPIOA, &snapshots[0], 0U, 2U}, {GPIOA, &snapshots[1], 0U, 3U},
        {GPIOA, &snapshots[2], 0U, 5U}, {GPIOA, &snapshots[3], 0U, 6U},
        {GPIOA, &snapshots[4], 0U, 7U}, {GPIOB, &snapshots[5], 1U, 0U},
        {GPIOD, &snapshots[6], 3U, 12U}};
    nx_stm32_platform_config_t platform{};
    nx_stm32_platform_pin_receipt_t pin_receipt{};
    nx_stm32_gpio_state_t gpio{};
    nx_stm32_gpio_config_t gpio_config{GPIOD, 1U << 12U, 1U << 12U,
                                       true,  0U,        0U};
    nx_stm32_uart_state_t uart{};
    nx_stm32_uart_cold_receipt_t uart_receipt{};
    uint8_t rx_storage[16]{};
    const IRQn_Type vectors[1]{USART2_IRQn};
    nx_stm32_uart_cold_config_t uart_config{};
    const nx_stm32f4_uart_lifecycle_config_t uart_binding{&uart_config,
                                                          &uart_receipt};
    nx_stm32_spi_state_t spi{};
    nx_stm32_spi_cold_receipt_t spi_receipt{};
    nx_stm32_gpio_state_t cs{};
    nx_stm32_spi_endpoint_state_t endpoint{};
    nx_stm32_spi_pin_config_t spi_pins[4]{{GPIOA, &snapshots[2], 0U, 5U, 5U},
                                          {GPIOA, &snapshots[3], 0U, 6U, 5U},
                                          {GPIOA, &snapshots[4], 0U, 7U, 5U},
                                          {GPIOB, &snapshots[5], 1U, 0U, 0U}};
    nx_stm32_spi_endpoint_config_t endpoint_config{&endpoint, &cs, 1000000U,
                                                   0U};
    nx_stm32_spi_config_t spi_config{SPI1, 84000000U,        spi_pins,
                                     4U,   &endpoint_config, 1U};
    const nx_stm32f4_spi_lifecycle_config_t spi_binding{&spi_config,
                                                        &spi_receipt};
    const nx_lifecycle_entry_t entries[4]{
        {&nx_stm32f4_pins_lifecycle_ops, &pin_receipt, &platform},
        {&nx_stm32f4_gpio_lifecycle_ops, &gpio, &gpio_config},
        {&nx_stm32f4_uart_irq_lifecycle_ops, &uart, &uart_binding},
        {&nx_stm32f4_spi_poll_lifecycle_ops, &spi, &spi_binding}};
    const nx_lifecycle_plan_t plan{entries, 4U, nx_stm32f4_lifecycle_context,
                                   nullptr};
    nx_lifecycle_state_t engine{};

    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(UINT32_C(0x40000000)));
        g_f4_model_nvic = {};
        g_f4_model_scb = {};
        g_f4_model_systick = {};
        g_f4_model_isr = false;
        g_f4_model_mask = 0U;
        register_port = nullptr;
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = RCC;
        platform.pins = pins;
        platform.pin_count = 7U;
        platform.apb1_clocks = RCC_APB1ENR_USART2EN;
        platform.apb2_clocks = RCC_APB2ENR_SPI1EN;
        platform.irq_masks[USART2_IRQn / 32U] = 1U << (USART2_IRQn % 32U);
        uart_config.registers = USART2;
        uart_config.pins = pins;
        uart_config.vectors = vectors;
        uart_config.rx_storage = rx_storage;
        uart_config.rx_capacity = sizeof(rx_storage);
        uart_config.clock_hz = 42000000U;
        uart_config.baud = 115200U;
        uart_config.profile = NX_UART_RX_BYTES;
        uart_config.vector_count = 1U;
        uart_config.priority = 5U;
        USART2->SR = USART_SR_TXE | USART_SR_TC;
    }

    void TearDown() override {
        register_port = nullptr;
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(munmap(mapping, 0x80000U), 0);
        }
    }

    /** \brief Capture all peripheral storage to prove effect-free admission. */
    std::vector<uint8_t> registers() const {
        const auto* begin = static_cast<const uint8_t*>(mapping);
        return {begin, begin + 0x80000U};
    }
};

TEST_F(STM32StaticLifecycle, EveryPerKindAdmissionHasNoHardwareOrStateEffects) {
    const auto before = registers();
    const std::array<uint16_t, 7> original{};
    for (const auto& entry : entries) {
        EXPECT_EQ(entry.ops->admit(entry.state, entry.config), NX_SUCCESS);
    }
    EXPECT_EQ(registers(), before);
    EXPECT_EQ(std::memcmp(snapshots, original.data(), sizeof(snapshots)), 0);
    EXPECT_FALSE(pin_receipt.captured);
    EXPECT_EQ(pin_receipt.clocks_added, 0U);
    EXPECT_EQ(uart_receipt.effects, 0U);
    EXPECT_EQ(spi_receipt.captured, 0U);
    EXPECT_EQ(gpio.registers, nullptr);
    EXPECT_EQ(uart.registers, nullptr);
    EXPECT_EQ(spi.registers, nullptr);
}

TEST_F(STM32StaticLifecycle,
       ExactPlanStartsRealDriversAndRestoresSharedResources) {
    ASSERT_EQ(nx_lifecycle_start(&plan, &engine), NX_SUCCESS);
    EXPECT_TRUE(pin_receipt.captured);
    EXPECT_TRUE(gpio.initialized);
    EXPECT_TRUE(uart.initialized);
    EXPECT_EQ(spi.registers, SPI1);
    EXPECT_EQ(endpoint.port, &spi);
    EXPECT_EQ(endpoint.cs, &cs);
    EXPECT_NE(USART2->CR1 & USART_CR1_UE, 0U);
    EXPECT_NE(NVIC_GetEnableIRQ(USART2_IRQn), 0U);
    ASSERT_EQ(nx_lifecycle_stop(&plan, &engine), NX_SUCCESS);
    EXPECT_EQ(engine.phase, NX_LIFECYCLE_IDLE);
    EXPECT_FALSE(pin_receipt.captured);
    EXPECT_EQ(pin_receipt.clocks_added, 0U);
    EXPECT_FALSE(gpio.initialized);
    EXPECT_EQ(uart.registers, nullptr);
    EXPECT_EQ(spi.registers, nullptr);
    EXPECT_EQ(RCC->AHB1ENR, 0U);
    EXPECT_EQ(RCC->APB1ENR, 0U);
    EXPECT_EQ(RCC->APB2ENR, 0U);
    EXPECT_EQ(NVIC_GetEnableIRQ(USART2_IRQn), 0U);
    EXPECT_EQ(nx_lifecycle_stop(&plan, &engine), NX_SUCCESS);
    EXPECT_EQ(nx_lifecycle_start(&plan, &engine), NX_SUCCESS);
    EXPECT_EQ(nx_lifecycle_stop(&plan, &engine), NX_SUCCESS);
}

TEST_F(STM32StaticLifecycle,
       LateMalformedSPIIsRejectedBeforeAnyGPIOOrUARTWork) {
    const auto before = registers();
    spi_config.clock_hz = 42000000U;
    EXPECT_EQ(nx_lifecycle_start(&plan, &engine), NX_ERROR_INVALID);
    EXPECT_EQ(registers(), before);
    EXPECT_EQ(engine.cursor, 0U);
    EXPECT_FALSE(pin_receipt.captured);
    EXPECT_FALSE(gpio.initialized);
    EXPECT_FALSE(uart.initialized);
}

TEST_F(STM32StaticLifecycle, UnsupportedUARTModeRejectsBeforeSharedPadCapture) {
    const auto before = registers();
    uart_config.profile = NX_UART_RX_BLOCKS;
    EXPECT_EQ(nx_lifecycle_start(&plan, &engine), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(registers(), before);
    EXPECT_FALSE(pin_receipt.captured);
}

TEST_F(STM32StaticLifecycle, RetainedUARTOrSPIOwnerCannotBeRebound) {
    const auto before = registers();
    uart_receipt.clock_added = RCC_APB1ENR_USART2EN;
    EXPECT_EQ(nx_lifecycle_start(&plan, &engine), NX_ERROR_BUSY);
    EXPECT_EQ(registers(), before);
    uart_receipt.clock_added = 0U;
    spi_receipt.rollback_pending = true;
    EXPECT_EQ(nx_lifecycle_start(&plan, &engine), NX_ERROR_BUSY);
    EXPECT_EQ(registers(), before);
}

TEST_F(STM32StaticLifecycle,
       RefusedSPIAndRollbackKeepEarlierDriversAndPadReceipt) {
    testing::NiceMock<RegisterPort> bus;
    EXPECT_CALL(bus, Accept(testing::_, testing::_))
        .WillRepeatedly(testing::Return(true));
    SPI1->CR1 = SPI_CR1_CPOL;
    EXPECT_CALL(bus, Accept(&SPI1->CR1, 0U)).WillOnce(testing::Return(false));
    EXPECT_CALL(bus, Accept(&RCC->APB2ENR, 0U))
        .WillRepeatedly(testing::Return(false));
    register_port = &bus;
    EXPECT_EQ(nx_lifecycle_start(&plan, &engine), NX_ERROR_IO);
    EXPECT_EQ(engine.primary, NX_ERROR_IO);
    EXPECT_EQ(engine.cleanup, NX_ERROR_IO);
    EXPECT_EQ(engine.cursor, 4U);
    EXPECT_EQ(engine.phase, NX_LIFECYCLE_DRAINING);
    EXPECT_TRUE(spi_receipt.rollback_pending);
    EXPECT_TRUE(pin_receipt.captured);
    EXPECT_TRUE(uart.initialized);
    EXPECT_TRUE(gpio.initialized);
    register_port = nullptr;
    EXPECT_EQ(nx_lifecycle_stop(&plan, &engine), NX_SUCCESS);
    EXPECT_FALSE(pin_receipt.captured);
    EXPECT_FALSE(spi_receipt.rollback_pending);
    EXPECT_EQ(engine.primary, NX_ERROR_IO);
    EXPECT_EQ(engine.cleanup, NX_SUCCESS);
    EXPECT_EQ(RCC->AHB1ENR, 0U);
}

TEST_F(STM32StaticLifecycle, BusySPIStopDoesNotWithdrawUARTOrSharedPinLease) {
    ASSERT_EQ(nx_lifecycle_start(&plan, &engine), NX_SUCCESS);
    spi.active = true;
    EXPECT_EQ(nx_lifecycle_stop(&plan, &engine), NX_ERROR_BUSY);
    EXPECT_EQ(engine.cursor, 4U);
    EXPECT_TRUE(pin_receipt.captured);
    EXPECT_TRUE(uart.initialized);
    spi.active = false;
    EXPECT_EQ(nx_lifecycle_stop(&plan, &engine), NX_SUCCESS);
    EXPECT_EQ(engine.phase, NX_LIFECYCLE_IDLE);
}

TEST_F(STM32StaticLifecycle,
       FaultedSPIStateSurvivesFailedCloseAndRestartsAfterCompleteRelease) {
    ASSERT_EQ(nx_lifecycle_start(&plan, &engine), NX_SUCCESS);
    /* Model the legitimate latched state after a polling abort has returned.
     * The actual C close, receipt, readbacks and lifecycle engine still run. */
    spi.fault = true;
    testing::NiceMock<RegisterPort> bus;
    EXPECT_CALL(bus, Accept(testing::_, testing::_))
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(bus, Accept(&RCC->APB2ENR, 0U))
        .WillOnce(testing::Return(false));
    register_port = &bus;
    EXPECT_EQ(nx_lifecycle_stop(&plan, &engine), NX_ERROR_IO);
    EXPECT_EQ(engine.phase, NX_LIFECYCLE_DRAINING);
    EXPECT_EQ(engine.cursor, 4U);
    EXPECT_EQ(spi.registers, SPI1);
    EXPECT_TRUE(spi.fault);
    EXPECT_TRUE(pin_receipt.captured);
    register_port = nullptr;
    ASSERT_EQ(nx_lifecycle_stop(&plan, &engine), NX_SUCCESS);
    EXPECT_EQ(engine.phase, NX_LIFECYCLE_IDLE);
    EXPECT_EQ(spi.registers, nullptr);
    EXPECT_FALSE(spi.fault);
    EXPECT_EQ(nx_lifecycle_start(&plan, &engine), NX_SUCCESS);
    EXPECT_EQ(nx_lifecycle_stop(&plan, &engine), NX_SUCCESS);
}

TEST_F(STM32StaticLifecycle,
       UARTReleaseRefusalDoesNotRepeatCompletedSPIRelease) {
    ASSERT_EQ(nx_lifecycle_start(&plan, &engine), NX_SUCCESS);
    testing::NiceMock<RegisterPort> bus;
    EXPECT_CALL(bus, Accept(testing::_, testing::_))
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(bus, Accept(&USART2->CR1, 0U)).WillOnce(testing::Return(false));
    register_port = &bus;
    EXPECT_EQ(nx_lifecycle_stop(&plan, &engine), NX_ERROR_BUSY);
    EXPECT_EQ(engine.cursor, 3U);
    EXPECT_EQ(spi.registers, nullptr);
    EXPECT_TRUE(pin_receipt.captured);
    testing::Mock::VerifyAndClearExpectations(&bus);
    EXPECT_CALL(bus, Accept(testing::_, testing::_))
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(bus, Accept(&SPI1->CR1, testing::_)).Times(0);
    EXPECT_EQ(nx_lifecycle_stop(&plan, &engine), NX_SUCCESS);
    register_port = nullptr;
}

TEST_F(STM32StaticLifecycle,
       SharedClockReleaseFailureRetainsOnlyPinOwnerForRetry) {
    ASSERT_EQ(nx_lifecycle_start(&plan, &engine), NX_SUCCESS);
    testing::NiceMock<RegisterPort> bus;
    EXPECT_CALL(bus, Accept(testing::_, testing::_))
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(bus, Accept(&RCC->AHB1ENR, 0U))
        .WillOnce(testing::Return(false));
    register_port = &bus;
    EXPECT_EQ(nx_lifecycle_stop(&plan, &engine), NX_ERROR_IO);
    EXPECT_EQ(engine.cursor, 1U);
    EXPECT_FALSE(pin_receipt.captured);
    EXPECT_NE(pin_receipt.clocks_added, 0U);
    EXPECT_EQ(uart.registers, nullptr);
    EXPECT_EQ(spi.registers, nullptr);
    register_port = nullptr;
    EXPECT_EQ(nx_lifecycle_stop(&plan, &engine), NX_SUCCESS);
    EXPECT_EQ(pin_receipt.clocks_added, 0U);
}

TEST_F(STM32StaticLifecycle, ExecutionContextRefusalCreatesNoHardwareEffects) {
    const auto before = registers();
    g_f4_model_isr = true;
    EXPECT_EQ(nx_lifecycle_start(&plan, &engine), NX_ERROR_CONTEXT);
    g_f4_model_isr = false;
    g_f4_model_mask = 1U;
    EXPECT_EQ(nx_lifecycle_start(&plan, &engine), NX_ERROR_CONTEXT);
    EXPECT_EQ(registers(), before);
    g_f4_model_mask = 0U;
    EXPECT_EQ(nx_stm32f4_lifecycle_context(&engine, NX_LIFECYCLE_START),
              NX_ERROR_INVALID);
}
} /* namespace */

extern "C" void nx_static_lifecycle_write(volatile uint32_t* address,
                                          uint32_t value) {
    if (register_port != nullptr && !register_port->Accept(address, value)) {
        return;
    }
    for (size_t bank = 0U; bank < 8U; ++bank) {
        if (address == &NVIC->ICER[bank]) {
            NVIC->ISER[bank] &= ~value;
            return;
        }
        if (address == &NVIC->ICPR[bank]) {
            NVIC->ISPR[bank] &= ~value;
            return;
        }
    }
    *address = value;
}
