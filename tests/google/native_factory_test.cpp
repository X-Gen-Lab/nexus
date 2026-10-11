/**
 * \file            native_factory_test.cpp
 *
 * \brief           Execute the actual generated multi-instance Native assembly.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-10
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/native/model.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "provider.h"
#include <gtest/gtest.h>

namespace {

/** \brief Reset assembly effects without changing its static identities. */
class NativeFactory : public testing::Test {
  protected:
    void SetUp() override {
        ASSERT_EQ(nx_platform_stop(), NX_SUCCESS);
        ASSERT_EQ(nx_native_clock_configure(true, 0), NX_SUCCESS);
    }

    void TearDown() override {
        EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    }
};

TEST_F(NativeFactory, LookupBeforeStartupHasNoEffectsAndChecksEveryClass) {
    const nx_gpio_port_t* first = nx_factory_gpio(NX_GPIO_ID_LED_A);
    const nx_gpio_port_t* second = nx_factory_gpio(NX_GPIO_ID_LED_B);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_NE(first, second);
    EXPECT_NE(first->context, second->context);
    EXPECT_EQ(first->ops, second->ops);
    EXPECT_EQ(nx_factory_gpio(NX_GPIO_ID_LED_A), first);
    EXPECT_EQ(nx_gpio_port_write(first, 1, 0), NX_ERROR_INVALID);
    EXPECT_EQ(nx_factory_gpio(NX_GPIO_ID_COUNT), nullptr);
    EXPECT_EQ(nx_factory_gpio(static_cast<nx_gpio_id_t>(UINT16_MAX)), nullptr);
    EXPECT_EQ(nx_factory_uart(static_cast<nx_uart_id_t>(UINT16_MAX)), nullptr);
    EXPECT_EQ(nx_factory_spi(static_cast<nx_spi_id_t>(UINT16_MAX)), nullptr);
    EXPECT_EQ(nx_factory_i2c(static_cast<nx_i2c_id_t>(UINT16_MAX)), nullptr);
    EXPECT_EQ(nx_factory_flash(static_cast<nx_flash_id_t>(UINT16_MAX)),
              nullptr);
    EXPECT_EQ(nx_factory_watchdog(static_cast<nx_watchdog_id_t>(UINT16_MAX)),
              nullptr);
    EXPECT_EQ(nx_factory_exti(static_cast<nx_exti_id_t>(UINT16_MAX)), nullptr);
    EXPECT_EQ(nx_factory_pwm(static_cast<nx_pwm_id_t>(UINT16_MAX)), nullptr);
    EXPECT_EQ(nx_factory_adc(static_cast<nx_adc_id_t>(UINT16_MAX)), nullptr);
    EXPECT_EQ(
        nx_factory_spi_device(static_cast<nx_spi_device_id_t>(UINT16_MAX)),
        nullptr);
    EXPECT_EQ(
        nx_factory_i2c_device(static_cast<nx_i2c_device_id_t>(UINT16_MAX)),
        nullptr);
    EXPECT_EQ(nx_factory_uart(NX_UART_ID_COUNT), nullptr);
    EXPECT_EQ(nx_factory_spi(NX_SPI_ID_COUNT), nullptr);
    EXPECT_EQ(nx_factory_i2c(NX_I2C_ID_COUNT), nullptr);
    EXPECT_EQ(nx_factory_flash(NX_FLASH_ID_COUNT), nullptr);
    EXPECT_EQ(nx_factory_watchdog(NX_WATCHDOG_ID_COUNT), nullptr);
    EXPECT_EQ(nx_factory_exti(NX_EXTI_ID_COUNT), nullptr);
    EXPECT_EQ(nx_factory_pwm(NX_PWM_ID_COUNT), nullptr);
    EXPECT_EQ(nx_factory_adc(NX_ADC_ID_COUNT), nullptr);
    EXPECT_EQ(nx_factory_spi_device(NX_SPI_DEVICE_ID_COUNT), nullptr);
    EXPECT_EQ(nx_factory_i2c_device(NX_I2C_DEVICE_ID_COUNT), nullptr);
    const nx_uart_port_t* uart = nx_factory_uart(NX_UART_ID_LINK_A);
    uint8_t byte = 1;
    nx_uart_tx_request_t request = {};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&request, &byte, 1, 100), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_submit(uart, &request), NX_ERROR_STATE);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
}

TEST_F(NativeFactory, GeneratedGPIOAndUARTHaveIndependentStateAndStorage) {
    const nx_gpio_port_t* led_a = nx_factory_gpio(NX_GPIO_ID_LED_A);
    const nx_gpio_port_t* led_b = nx_factory_gpio(NX_GPIO_ID_LED_B);
    const nx_uart_port_t* uart_a = nx_factory_uart(NX_UART_ID_LINK_A);
    const nx_uart_port_t* uart_b = nx_factory_uart(NX_UART_ID_LINK_B);
    ASSERT_NE(uart_a, nullptr);
    ASSERT_NE(uart_b, nullptr);
    EXPECT_NE(uart_a, uart_b);
    EXPECT_NE(uart_a->context, uart_b->context);
    EXPECT_EQ(uart_a->ops, uart_b->ops);
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    ASSERT_EQ(nx_gpio_port_write(led_a, 1, 0), NX_SUCCESS);
    EXPECT_EQ(nx_native_gpio_model_output(led_a), 1u);
    EXPECT_EQ(nx_native_gpio_model_output(led_b), 0u);
    ASSERT_EQ(nx_gpio_port_write(led_b, 8, 0), NX_SUCCESS);
    EXPECT_EQ(nx_native_gpio_model_output(led_a), 1u);
    EXPECT_EQ(nx_native_gpio_model_output(led_b), 8u);
    ASSERT_EQ(nx_native_uart_model_receive(uart_a, 17, 0), NX_SUCCESS);
    ASSERT_EQ(nx_native_uart_model_receive(uart_b, 29, 0), NX_SUCCESS);
    nx_uart_rx_event_t event = {};
    size_t count = 0;
    ASSERT_EQ(nx_uart_port_read_events(uart_a, &event, 1, &count), NX_SUCCESS);
    EXPECT_EQ(event.byte, 17);
    ASSERT_EQ(nx_uart_port_read_events(uart_b, &event, 1, &count), NX_SUCCESS);
    EXPECT_EQ(event.byte, 29);
    EXPECT_EQ(nx_uart_port_read_events(uart_a, &event, 1, &count),
              NX_ERROR_EMPTY);
    uint8_t bytes[] = {1, 2};
    nx_uart_tx_request_t first_request = {};
    nx_uart_tx_request_t second_request = {};
    nx_request_initialize(&first_request.base);
    nx_request_initialize(&second_request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&first_request, bytes, 2, 100), NX_SUCCESS);
    ASSERT_EQ(nx_uart_tx_prepare(&second_request, bytes, 2, 100), NX_SUCCESS);
    nx_native_uart_model_fault(uart_a, true, false);
    ASSERT_EQ(nx_uart_port_submit(uart_a, &first_request), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(uart_b, &second_request), NX_SUCCESS);
    nx_uart_port_service(uart_a);
    EXPECT_EQ(nx_request_state(&first_request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(nx_request_state(&second_request.base), NX_REQUEST_ACTIVE);
    for (unsigned i = 0; i < 3; ++i) {
        nx_uart_port_service(uart_b);
    }
    EXPECT_EQ(nx_request_state(&second_request.base), NX_REQUEST_SETTLED);
    nx_result_t result = NX_ERROR_IO;
    ASSERT_EQ(nx_request_result(&second_request.base, &result, &count),
              NX_SUCCESS);
    EXPECT_EQ(result, NX_SUCCESS);
    EXPECT_EQ(count, 2u);
}

TEST_F(NativeFactory, GeneratedBusControllersAndEndpointMemoriesAreDistinct) {
    const nx_spi_port_t* spi_a = nx_factory_spi(NX_SPI_ID_BUS_SPI_A);
    const nx_spi_port_t* spi_b = nx_factory_spi(NX_SPI_ID_BUS_SPI_B);
    const nx_spi_endpoint_t* child_a =
        nx_factory_spi_device(NX_SPI_DEVICE_ID_SENSOR_SPI_A);
    const nx_spi_endpoint_t* child_b =
        nx_factory_spi_device(NX_SPI_DEVICE_ID_SENSOR_SPI_B);
    const nx_spi_endpoint_t* child_c =
        nx_factory_spi_device(NX_SPI_DEVICE_ID_SENSOR_SPI_C);
    ASSERT_NE(spi_a, nullptr);
    ASSERT_NE(spi_b, nullptr);
    EXPECT_NE(spi_a->context, spi_b->context);
    EXPECT_EQ(spi_a->ops, spi_b->ops);
    EXPECT_NE(child_a->context, child_b->context);
    EXPECT_EQ(child_a->ops, child_b->ops);
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    uint8_t first_data[2] = {0, 17};
    uint8_t second_data[2] = {0, 29};
    size_t count = 0;
    ASSERT_EQ(
        nx_spi_endpoint_transfer(child_a, first_data, nullptr, 2, 100, &count),
        NX_SUCCESS);
    ASSERT_EQ(
        nx_spi_endpoint_transfer(child_b, second_data, nullptr, 2, 100, &count),
        NX_SUCCESS);
    uint8_t read_command[2] = {0x80, 0};
    uint8_t received[2] = {};
    ASSERT_EQ(nx_spi_endpoint_transfer(child_a, read_command, received, 2, 100,
                                       &count),
              NX_SUCCESS);
    EXPECT_EQ(received[1], 17);
    ASSERT_EQ(nx_spi_endpoint_transfer(child_b, read_command, received, 2, 100,
                                       &count),
              NX_SUCCESS);
    EXPECT_EQ(received[1], 29);
    nx_native_spi_model_fault(spi_a, SIZE_MAX, true);
    EXPECT_EQ(nx_spi_endpoint_transfer(child_a, read_command, received, 2, 100,
                                       &count),
              NX_ERROR_BUSY);
    EXPECT_EQ(nx_spi_endpoint_transfer(child_b, read_command, received, 2, 100,
                                       &count),
              NX_ERROR_BUSY);
    EXPECT_EQ(nx_spi_endpoint_transfer(child_c, read_command, received, 2, 100,
                                       &count),
              NX_SUCCESS);
    nx_native_spi_model_fault(spi_a, SIZE_MAX, false);
    const nx_i2c_port_t* i2c_a = nx_factory_i2c(NX_I2C_ID_BUS_I2C_A);
    const nx_i2c_endpoint_t* i2c_child_a =
        nx_factory_i2c_device(NX_I2C_DEVICE_ID_SENSOR_I2C_A);
    const nx_i2c_endpoint_t* i2c_child_b =
        nx_factory_i2c_device(NX_I2C_DEVICE_ID_SENSOR_I2C_B);
    const nx_i2c_endpoint_t* i2c_child_c =
        nx_factory_i2c_device(NX_I2C_DEVICE_ID_SENSOR_I2C_C);
    nx_i2c_message_t first_write = {first_data, 2, false};
    nx_i2c_message_t second_write = {second_data, 2, false};
    ASSERT_EQ(
        nx_i2c_endpoint_transaction(i2c_child_a, &first_write, 1, 100, &count),
        NX_SUCCESS);
    ASSERT_EQ(
        nx_i2c_endpoint_transaction(i2c_child_b, &second_write, 1, 100, &count),
        NX_SUCCESS);
    uint8_t address = 0;
    uint8_t value = 0;
    nx_i2c_message_t messages[] = {{&address, 1, false}, {&value, 1, true}};
    ASSERT_EQ(
        nx_i2c_endpoint_transaction(i2c_child_a, messages, 2, 100, &count),
        NX_SUCCESS);
    EXPECT_EQ(value, 17);
    ASSERT_EQ(
        nx_i2c_endpoint_transaction(i2c_child_b, messages, 2, 100, &count),
        NX_SUCCESS);
    EXPECT_EQ(value, 29);
    nx_native_i2c_model_fault(i2c_a, NX_ERROR_NACK, false);
    EXPECT_EQ(
        nx_i2c_endpoint_transaction(i2c_child_a, messages, 2, 100, &count),
        NX_ERROR_NACK);
    EXPECT_EQ(
        nx_i2c_endpoint_transaction(i2c_child_b, messages, 2, 100, &count),
        NX_ERROR_NACK);
    EXPECT_EQ(
        nx_i2c_endpoint_transaction(i2c_child_c, messages, 2, 100, &count),
        NX_SUCCESS);
    EXPECT_EQ(nx_i2c_port_recover(i2c_a), NX_SUCCESS);
}

TEST_F(NativeFactory, StopRevokesOperationsAndRestartRetainsStaticIdentity) {
    const nx_gpio_port_t* gpio = nx_factory_gpio(NX_GPIO_ID_LED_A);
    const nx_uart_port_t* uart = nx_factory_uart(NX_UART_ID_LINK_A);
    const nx_spi_endpoint_t* spi =
        nx_factory_spi_device(NX_SPI_DEVICE_ID_SENSOR_SPI_A);
    const nx_i2c_endpoint_t* i2c =
        nx_factory_i2c_device(NX_I2C_DEVICE_ID_SENSOR_I2C_A);
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    ASSERT_EQ(nx_gpio_port_write(gpio, 1, 0), NX_SUCCESS);
    ASSERT_EQ(nx_platform_stop(), NX_SUCCESS);
    EXPECT_EQ(nx_factory_gpio(NX_GPIO_ID_LED_A), gpio);
    EXPECT_EQ(nx_factory_uart(NX_UART_ID_LINK_A), uart);
    EXPECT_EQ(nx_gpio_port_write(gpio, 1, 0), NX_ERROR_INVALID);
    uint8_t tx = 1;
    size_t count = 0;
    EXPECT_EQ(nx_spi_endpoint_transfer(spi, &tx, nullptr, 1, 100, &count),
              NX_ERROR_STATE);
    nx_i2c_message_t message = {&tx, 1, false};
    EXPECT_EQ(nx_i2c_endpoint_transaction(i2c, &message, 1, 100, &count),
              NX_ERROR_STATE);
    EXPECT_EQ(nx_native_uart_model_receive(uart, 1, 0), NX_ERROR_STATE);
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    EXPECT_EQ(nx_native_gpio_model_output(gpio), 0u);
    EXPECT_EQ(nx_gpio_port_write(gpio, 1, 0), NX_SUCCESS);
    EXPECT_EQ(nx_native_uart_model_receive(uart, 1, 0), NX_SUCCESS);
}

TEST_F(NativeFactory, StopRetainsQuarantinedBorrowUntilProviderDrain) {
    const nx_uart_port_t* uart = nx_factory_uart(NX_UART_ID_LINK_A);
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    uint8_t bytes[] = {1, 2};
    nx_uart_tx_request_t request = {};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&request, bytes, 2, 100), NX_SUCCESS);
    nx_native_uart_model_fault(uart, false, true);
    ASSERT_EQ(nx_uart_port_submit(uart, &request), NX_SUCCESS);
    EXPECT_EQ(nx_platform_stop(), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(nx_factory_uart(NX_UART_ID_LINK_A), uart);
    EXPECT_EQ(nx_uart_tx_prepare(&request, bytes, 2, 100), NX_ERROR_STATE);
    EXPECT_EQ(nx_platform_start().primary, NX_ERROR_STATE);
    nx_native_uart_model_fault(uart, false, false);
    nx_uart_port_service(uart);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    EXPECT_EQ(nx_native_uart_model_receive(uart, 1, 0), NX_ERROR_STATE);
}

TEST_F(NativeFactory, AllNineClassesStopAndRestartWithRetainedWatchdogEffect) {
    const nx_flash_port_t* flash = nx_factory_flash(NX_FLASH_ID_FLASH0);
    const nx_adc_port_t* adc = nx_factory_adc(NX_ADC_ID_ADC0);
    const nx_pwm_port_t* pwm = nx_factory_pwm(NX_PWM_ID_PWM0);
    const nx_exti_port_t* exti = nx_factory_exti(NX_EXTI_ID_EXTI0);
    const nx_watchdog_port_t* watchdog =
        nx_factory_watchdog(NX_WATCHDOG_ID_WATCHDOG0);
    ASSERT_NE(flash, nullptr);
    ASSERT_NE(adc, nullptr);
    ASSERT_NE(pwm, nullptr);
    ASSERT_NE(exti, nullptr);
    ASSERT_NE(watchdog, nullptr);
    EXPECT_EQ(nx_flash_port_geometry(flash), nullptr);
    nx_adc_info_t info = {};
    EXPECT_EQ(nx_adc_port_info(adc, &info), NX_ERROR_INVALID);
    EXPECT_EQ(nx_pwm_port_start(pwm), NX_ERROR_STATE);
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    ASSERT_NE(nx_flash_port_geometry(flash), nullptr);
    uint8_t programmed = 0x37;
    ASSERT_EQ(nx_flash_port_program(flash, 0, &programmed, 1, 100), NX_SUCCESS);
    ASSERT_EQ(nx_adc_port_info(adc, &info), NX_SUCCESS);
    EXPECT_EQ(info.channel_count, 2u);
    ASSERT_EQ(nx_pwm_port_start(pwm), NX_SUCCESS);
    ASSERT_EQ(nx_native_exti_model_emit(exti, 6, NX_EXTI_RISING), NX_SUCCESS);
    nx_exti_event_t event = {};
    size_t count = 0;
    ASSERT_EQ(nx_exti_port_read(exti, &event, 1, &count), NX_SUCCESS);
    EXPECT_EQ(event.line, 6);
    nx_watchdog_state_t effect = {};
    nx_result_t enabled =
        nx_watchdog_port_enable(watchdog, 100000, false, &effect);
    EXPECT_TRUE(enabled == NX_SUCCESS || enabled == NX_ERROR_STATE);
    ASSERT_TRUE(effect.enabled);
    ASSERT_TRUE(effect.irreversible);
    ASSERT_EQ(nx_platform_stop(), NX_SUCCESS);
    EXPECT_EQ(nx_flash_port_geometry(flash), nullptr);
    uint8_t observed = 0;
    EXPECT_EQ(nx_flash_port_read(flash, 0, &observed, 1), NX_ERROR_INVALID);
    EXPECT_EQ(nx_adc_port_info(adc, &info), NX_ERROR_INVALID);
    uint16_t samples[2] = {};
    EXPECT_EQ(nx_adc_port_sample(adc, samples, 2, 100, &count),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_pwm_port_start(pwm), NX_ERROR_STATE);
    EXPECT_EQ(nx_pwm_port_set(pwm, 1000, 1), NX_ERROR_STATE);
    EXPECT_EQ(nx_native_exti_model_emit(exti, 6, NX_EXTI_RISING),
              NX_ERROR_STATE);
    ASSERT_EQ(nx_watchdog_port_state(watchdog, &effect), NX_SUCCESS);
    EXPECT_TRUE(effect.enabled);
    EXPECT_EQ(nx_watchdog_port_feed(watchdog), NX_SUCCESS);
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    EXPECT_EQ(nx_factory_flash(NX_FLASH_ID_FLASH0), flash);
    EXPECT_EQ(nx_factory_adc(NX_ADC_ID_ADC0), adc);
    EXPECT_EQ(nx_factory_pwm(NX_PWM_ID_PWM0), pwm);
    EXPECT_EQ(nx_factory_exti(NX_EXTI_ID_EXTI0), exti);
    EXPECT_EQ(nx_factory_watchdog(NX_WATCHDOG_ID_WATCHDOG0), watchdog);
    ASSERT_EQ(nx_flash_port_read(flash, 0, &observed, 1), NX_SUCCESS);
    EXPECT_EQ(observed, programmed);
    EXPECT_EQ(nx_watchdog_port_enable(watchdog, 200000, false, &effect),
              NX_ERROR_STATE);
    EXPECT_TRUE(effect.enabled);
    EXPECT_EQ(nx_adc_port_info(adc, &info), NX_SUCCESS);
    EXPECT_EQ(nx_pwm_port_start(pwm), NX_SUCCESS);
    EXPECT_EQ(nx_native_exti_model_emit(exti, 6, NX_EXTI_RISING), NX_SUCCESS);
}

TEST_F(NativeFactory, PlatformStopKeepsADCStreamLoanUntilConsumerRelease) {
    const nx_adc_port_t* adc = nx_factory_adc(NX_ADC_ID_ADC0);
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    alignas(uint16_t) uint8_t bytes[4] = {};
    nx_stream_slot_t slot = {};
    slot.data = bytes;
    slot.capacity = sizeof(bytes);
    nx_stream_t stream = {};
    ASSERT_EQ(nx_stream_initialize(&stream, &slot, 1), NX_SUCCESS);
    ASSERT_EQ(nx_adc_port_stream_start(adc, &stream, 1000), NX_SUCCESS);
    ASSERT_EQ(nx_native_adc_model_trigger(adc), NX_SUCCESS);
    nx_stream_block_t block = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(nx_platform_stop(), NX_ERROR_BUSY);
    EXPECT_EQ(nx_native_adc_model_trigger(adc), NX_ERROR_STATE);
    EXPECT_EQ(block.length, sizeof(bytes));
    EXPECT_EQ(nx_stream_release(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    nx_adc_info_t info = {};
    EXPECT_EQ(nx_adc_port_info(adc, &info), NX_ERROR_INVALID);
}

} /* namespace */
