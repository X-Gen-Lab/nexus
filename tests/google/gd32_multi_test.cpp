/**
 * \file            gd32_multi_test.cpp
 * \brief           Production GD32 controllers retain independent state and
 * IRQs \author          Nexus Team \version         1.0.0 \date 2026-10-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "gd32f470_provider.h"
#include "gd32f4xx.h"
extern uint64_t g_gd32_model_now;
extern bool g_gd32_model_i2c_stop_works;
extern bool g_gd32_model_isr;
extern uint32_t g_gd32_model_mask;
}
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
nx_gd32_uart_state_t* uart0_irq;

/** \brief Exact-address model mappings exercise production MMIO instructions.
 */
class GD32MultiController : public ::testing::Test {
  protected:
    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(UINT32_C(0x40000000)));
        std::memset(&g_gd32_model_nvic, 0, sizeof(g_gd32_model_nvic));
        g_gd32_model_now = 0;
        g_gd32_model_isr = false;
        g_gd32_model_mask = 0;
        g_gd32_model_i2c_stop_works = true;
        uart0_irq = nullptr;
    }
    void TearDown() override {
        uart0_irq = nullptr;
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(munmap(mapping, 0x80000), 0);
        }
    }
    void* mapping = MAP_FAILED;
};

TEST_F(GD32MultiController, ColdPinRollbackPreservesNeighbors) {
    GPIO_CTL(GPIOA) = 0xAA551234;
    GPIO_OMODE(GPIOA) = 0x55AA;
    GPIO_OSPD(GPIOA) = 0x1234ABCD;
    GPIO_PUD(GPIOA) = 0x87654321;
    GPIO_AFSEL1(GPIOA) = 0xAABBCCDD;
    GPIO_OCTL(GPIOA) = GPIO_PIN_9;
    const uint16_t saved = nx_gd32_pin_capture(GPIOA, 9);
    ASSERT_EQ(nx_gd32_pin_configure(GPIOA, 9, GPIO_MODE_AF, 7, GPIO_PUPD_PULLUP,
                                    true),
              NX_SUCCESS);
    nx_gd32_pin_restore(GPIOA, 9, saved);
    EXPECT_EQ(GPIO_CTL(GPIOA), 0xAA551234U);
    EXPECT_EQ(GPIO_OMODE(GPIOA), 0x55AAU);
    EXPECT_EQ(GPIO_OSPD(GPIOA), 0x1234ABCDU);
    EXPECT_EQ(GPIO_PUD(GPIOA), 0x87654321U);
    EXPECT_EQ(GPIO_AFSEL1(GPIOA), 0xAABBCCDDU);
    EXPECT_EQ(GPIO_BOP(GPIOA), GPIO_PIN_9);
    EXPECT_EQ(nx_gd32_pin_configure(GPIOA, 16, GPIO_MODE_AF, 7,
                                    GPIO_PUPD_PULLUP, true),
              NX_ERROR_INVALID);
    EXPECT_EQ(GPIO_CTL(GPIOA), 0xAA551234U);
    EXPECT_EQ(nx_gd32_gpio_clock_enable(9), NX_ERROR_INVALID);
    EXPECT_EQ(RCU_AHB1EN, 0U);
    EXPECT_EQ(nx_gd32_gpio_clock_enable(0), NX_SUCCESS);
    EXPECT_EQ(RCU_AHB1EN, RCU_AHB1EN_PAEN);
}

TEST_F(GD32MultiController, UARTsHaveIndependentBuffersCompletionAndStop) {
    nx_gd32_uart_state_t first = {};
    nx_gd32_uart_state_t second = {};
    nx_uart_rx_event_t first_rx[2] = {};
    nx_uart_rx_event_t second_rx[2] = {};
    const nx_uart_port_t first_api = {&nx_gd32_uart_ops, &first};
    const nx_uart_port_t second_api = {&nx_gd32_uart_ops, &second};
    ASSERT_EQ(nx_gd32_uart_initialize_at(&first, &nx_gd32_usart0_controller,
                                         115200, NX_UART_RX_EVENTS, first_rx, 2,
                                         6),
              NX_SUCCESS);
    ASSERT_EQ(nx_gd32_uart_initialize_at(&second, &nx_gd32_usart1_controller,
                                         115200, NX_UART_RX_EVENTS, second_rx,
                                         2, 7),
              NX_SUCCESS);
    nx_gd32_uart_state_t duplicate = {};
    EXPECT_EQ(nx_gd32_uart_initialize_at(&duplicate, &nx_gd32_usart1_controller,
                                         115200, NX_UART_RX_EVENTS, second_rx,
                                         2, 7),
              NX_ERROR_BUSY);
    const uint8_t first_data = 0x11;
    const uint8_t second_data = 0x22;
    nx_uart_tx_request_t first_request = {};
    nx_uart_tx_request_t second_request = {};
    nx_request_initialize(&first_request.base);
    nx_request_initialize(&second_request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&first_request, &first_data, 1, 1000),
              NX_SUCCESS);
    ASSERT_EQ(nx_uart_tx_prepare(&second_request, &second_data, 1, 1000),
              NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&first_api, &first_request), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&second_api, &second_request), NX_SUCCESS);
    USART_STAT0(USART0) = USART_STAT0_TBE | USART_STAT0_RBNE;
    USART_DATA(USART0) = 0xA1;
    nx_gd32_uart_irq(&first);
    USART_STAT0(USART1) = USART_STAT0_TBE | USART_STAT0_RBNE;
    USART_DATA(USART1) = 0xB2;
    nx_gd32_uart_irq(&second);
    EXPECT_EQ(first_rx[0].byte, 0xA1);
    EXPECT_EQ(second_rx[0].byte, 0xB2);
    EXPECT_EQ(USART_DATA(USART0), first_data);
    EXPECT_EQ(USART_DATA(USART1), second_data);
    USART_STAT0(USART0) = USART_STAT0_TC;
    nx_gd32_uart_irq(&first);
    nx_uart_port_service(&first_api);
    EXPECT_EQ(nx_request_state(&first_request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(nx_request_state(&second_request.base), NX_REQUEST_ACTIVE);
    ASSERT_EQ(nx_uart_port_stop(&first_api), NX_SUCCESS);
    EXPECT_NE(USART_CTL0(USART1) & USART_CTL0_UEN, 0U);
    EXPECT_NE(NVIC->ISER[USART1_IRQn / 32] & (1U << (USART1_IRQn % 32)), 0U);
    USART_STAT0(USART1) = USART_STAT0_TC;
    nx_gd32_uart_irq(&second);
    nx_uart_port_service(&second_api);
    EXPECT_EQ(nx_request_state(&second_request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(nx_uart_port_stop(&second_api), NX_SUCCESS);
}

TEST_F(GD32MultiController, SPIResetAndChipSelectStayOnTheirOwnController) {
    nx_gd32_spi_state_t first = {};
    nx_gd32_spi_state_t second = {};
    nx_gd32_spi_endpoint_state_t first_device = {};
    nx_gd32_spi_endpoint_state_t second_device = {};
    const nx_spi_endpoint_t first_api = {&nx_gd32_spi_endpoint_ops,
                                         &first_device};
    const nx_spi_endpoint_t second_api = {&nx_gd32_spi_endpoint_ops,
                                          &second_device};
    ASSERT_EQ(nx_gd32_spi_initialize_at(&first, &first_device,
                                        &nx_gd32_spi4_controller, GPIOB,
                                        GPIO_PIN_0, 1000000, 0),
              NX_SUCCESS);
    ASSERT_EQ(nx_gd32_spi_initialize_at(&second, &second_device,
                                        &nx_gd32_spi0_controller, GPIOB,
                                        GPIO_PIN_1, 2000000, 3),
              NX_SUCCESS);
    const uint8_t data = 0x35;
    uint8_t rx = 0;
    size_t transferred = 0;
    SPI_STAT(SPI0) = SPI_STAT_TBE | SPI_STAT_RBNE;
    ASSERT_EQ(nx_spi_endpoint_transfer(&second_api, &data, &rx, 1, 1000,
                                       &transferred),
              NX_SUCCESS);
    EXPECT_EQ(rx, data);
    EXPECT_EQ(SPI_DATA(SPI0), data);
    EXPECT_EQ(SPI_DATA(SPI4), 0U);
    EXPECT_EQ(GPIO_BOP(GPIOB), GPIO_PIN_1);
    const uint32_t second_control = SPI_CTL0(SPI0);
    SPI_STAT(SPI4) = SPI_STAT_CONFERR;
    EXPECT_EQ(
        nx_spi_endpoint_transfer(&first_api, &data, &rx, 1, 1000, &transferred),
        NX_ERROR_IO);
    EXPECT_EQ(GPIO_BOP(GPIOB), GPIO_PIN_0);
    EXPECT_EQ(SPI_CTL0(SPI0), second_control);
    ASSERT_EQ(nx_gd32_spi_stop(&first), NX_SUCCESS);
    SPI_STAT(SPI0) = SPI_STAT_TBE | SPI_STAT_RBNE;
    EXPECT_EQ(nx_spi_endpoint_transfer(&second_api, &data, &rx, 1, 1000,
                                       &transferred),
              NX_SUCCESS);
    EXPECT_EQ(nx_gd32_spi_stop(&second), NX_SUCCESS);
}

TEST_F(GD32MultiController, SPIEndpointsShareBusWithoutSharingConfiguration) {
    nx_gd32_spi_state_t controller = {};
    nx_gd32_spi_endpoint_state_t first = {};
    nx_gd32_spi_endpoint_state_t second = {};
    ASSERT_EQ(nx_gd32_spi_initialize_at(&controller, &first,
                                        &nx_gd32_spi0_controller, GPIOB,
                                        GPIO_PIN_0, 1000000, 0),
              NX_SUCCESS);
    const uint32_t first_control = first.control;
    ASSERT_EQ(nx_gd32_spi_endpoint_initialize(&controller, &second, GPIOB,
                                              GPIO_PIN_1, 2000000, 3),
              NX_SUCCESS);
    EXPECT_EQ(first.control, first_control);
    EXPECT_NE(first.control, second.control);
    EXPECT_EQ(first.cs_mask, GPIO_PIN_0);
    EXPECT_EQ(second.cs_mask, GPIO_PIN_1);
    EXPECT_EQ(second.port, &controller);
    EXPECT_EQ(nx_gd32_spi_stop(&controller), NX_SUCCESS);
}

TEST_F(GD32MultiController, I2CRecoveryAndTransactionsUseTheirOwnLines) {
    nx_gd32_i2c_state_t first = {};
    nx_gd32_i2c_state_t second = {};
    nx_gd32_i2c_endpoint_state_t first_device = {};
    nx_gd32_i2c_endpoint_state_t second_device = {};
    const nx_i2c_port_t first_api = {&nx_gd32_i2c_ops, &first};
    const nx_i2c_port_t second_api = {&nx_gd32_i2c_ops, &second};
    const nx_i2c_endpoint_t second_endpoint = {&nx_gd32_i2c_endpoint_ops,
                                               &second_device};
    ASSERT_EQ(nx_gd32_i2c_initialize_at(&first, &first_device,
                                        &nx_gd32_i2c0_controller, 0x50, 100000,
                                        GPIOB, GPIO_PIN_6 | GPIO_PIN_7),
              NX_SUCCESS);
    ASSERT_EQ(nx_gd32_i2c_initialize_at(&second, &second_device,
                                        &nx_gd32_i2c1_controller, 0x51, 100000,
                                        GPIOB, GPIO_PIN_10 | GPIO_PIN_11),
              NX_SUCCESS);
    GPIO_ISTAT(GPIOB) = GPIO_PIN_10 | GPIO_PIN_11;
    EXPECT_EQ(nx_i2c_port_recover(&first_api), NX_ERROR_IO);
    EXPECT_TRUE(first.faulted);
    EXPECT_EQ(nx_i2c_port_recover(&second_api), NX_SUCCESS);
    EXPECT_FALSE(second.faulted);
    uint8_t data = 0x42;
    nx_i2c_message_t message = {&data, 1, false};
    I2C_STAT0(I2C1) =
        I2C_STAT0_SBSEND | I2C_STAT0_ADDSEND | I2C_STAT0_TBE | I2C_STAT0_BTC;
    size_t transferred = 0;
    EXPECT_EQ(nx_i2c_endpoint_transaction(&second_endpoint, &message, 1, 1000,
                                          &transferred),
              NX_SUCCESS);
    EXPECT_EQ(transferred, 1U);
    EXPECT_EQ(nx_gd32_i2c_stop(&first), NX_SUCCESS);
    EXPECT_NE(I2C_CTL0(I2C1) & I2C_CTL0_I2CEN, 0U);
    EXPECT_EQ(nx_gd32_i2c_stop(&second), NX_SUCCESS);
}
} /* namespace */

/** \brief The model's race hook dispatches an explicit fixture-owned state. */
extern "C" void USART0_IRQHandler(void) {
    nx_gd32_uart_irq(uart0_irq);
}
