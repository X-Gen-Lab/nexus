/**
 * \file            native_instances_test.cpp
 *
 * \brief           Independent static Native state and provider substitution.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-10
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "provider.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>

namespace {

/** \brief Each case starts in an explicit deterministic clock domain. */
class NativeInstances : public testing::Test {
  protected:
    void SetUp() override {
        ASSERT_EQ(nx_native_clock_configure(true, 0), NX_SUCCESS);
    }
};

/** \brief Model alternate provider effects without Native state assumptions. */
class GPIOProvider {
  public:
    MOCK_METHOD(nx_result_t, Write, (uint32_t, uint32_t));
};

/** \brief Dispatch passes the actual context to an unrelated provider. */
nx_result_t alternate_write(void* context, uint32_t set, uint32_t reset) {
    return static_cast<GPIOProvider*>(context)->Write(set, reset);
}

TEST_F(NativeInstances, GPIOBindingsAndAlternateProviderRemainIndependent) {
    nx_native_gpio_state_t first = {};
    nx_native_gpio_state_t second = {};
    const nx_gpio_port_t first_api = {&nx_native_gpio_ops, &first};
    const nx_gpio_port_t second_api = {&nx_native_gpio_ops, &second};
    ASSERT_EQ(nx_native_gpio_configure_instance(&first, 3, 1, true),
              NX_SUCCESS);
    ASSERT_EQ(nx_native_gpio_configure_instance(&second, 12, 8, true),
              NX_SUCCESS);
    EXPECT_EQ(first_api.ops, second_api.ops);
    ASSERT_EQ(nx_gpio_port_write(&first_api, 2, 1), NX_SUCCESS);
    EXPECT_EQ(nx_native_gpio_output_instance(&first), 2u);
    EXPECT_EQ(nx_native_gpio_output_instance(&second), 8u);
    EXPECT_EQ(nx_gpio_port_write(&first_api, 8, 0), NX_ERROR_PERMISSION);
    testing::StrictMock<GPIOProvider> alternate;
    const nx_gpio_ops_t alternate_ops = {alternate_write, nullptr, nullptr};
    const nx_gpio_port_t alternate_api = {&alternate_ops, &alternate};
    EXPECT_CALL(alternate, Write(16, 4)).WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_EQ(nx_gpio_port_write(&alternate_api, 16, 4), NX_SUCCESS);
    EXPECT_EQ(nx_native_gpio_output_instance(&first), 2u);
}

TEST_F(NativeInstances, GPIOInputViewRejectsWritesWithoutChangingSnapshot) {
    nx_native_gpio_state_t state = {};
    const nx_gpio_port_t api = {&nx_native_gpio_ops, &state};
    ASSERT_EQ(nx_native_gpio_configure_instance(&state, 3, 0, false),
              NX_SUCCESS);
    nx_native_gpio_input_instance(&state, 2);
    EXPECT_EQ(nx_gpio_port_write(&api, 1, 0), NX_ERROR_INVALID);
    EXPECT_EQ(nx_gpio_port_toggle(&api, 1), NX_ERROR_INVALID);
    uint32_t snapshot = 0;
    ASSERT_EQ(nx_gpio_port_read(&api, &snapshot), NX_SUCCESS);
    EXPECT_EQ(snapshot, 2u);
    EXPECT_EQ(nx_native_gpio_output_instance(&state), 0u);
}

TEST_F(NativeInstances, PublicModelHelpersSelectOnlyTheirNativeFace) {
    nx_native_gpio_state_t state = {};
    const nx_gpio_port_t native = {&nx_native_gpio_ops, &state};
    const nx_gpio_ops_t alternate_ops = {alternate_write, nullptr, nullptr};
    testing::StrictMock<GPIOProvider> alternate;
    const nx_gpio_port_t alternate_api = {&alternate_ops, &alternate};
    ASSERT_EQ(nx_native_gpio_configure_instance(&state, 3, 1, true),
              NX_SUCCESS);
    nx_native_gpio_model_input(&native, 2);
    nx_native_gpio_model_input(&alternate_api, 0);
    EXPECT_EQ(nx_native_gpio_model_output(&native), 1u);
    EXPECT_EQ(nx_native_gpio_model_output(&alternate_api), 0u);
    uint32_t value = 0;
    ASSERT_EQ(nx_gpio_port_read(&native, &value), NX_SUCCESS);
    EXPECT_EQ(value, 2u);
    EXPECT_EQ(nx_native_spi_model_configure(nullptr, nullptr, 0),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_native_uart_model_configure(nullptr, nullptr),
              NX_ERROR_INVALID);
}

TEST_F(NativeInstances, UARTInterruptsAndBorrowSettlementStayOnTheirInstance) {
    nx_native_uart_state_t first = {};
    nx_native_uart_state_t second = {};
    const nx_uart_port_t first_api = {&nx_native_uart_ops, &first};
    const nx_uart_port_t second_api = {&nx_native_uart_ops, &second};
    uint8_t first_rx[2] = {};
    uint8_t second_rx[2] = {};
    uint8_t first_log[2] = {};
    uint8_t second_log[2] = {};
    const nx_native_uart_config_t first_config = {
        NX_UART_RX_BYTES, first_rx, 2, first_log, 2, false};
    const nx_native_uart_config_t second_config = {
        NX_UART_RX_BYTES, second_rx, 2, second_log, 2, false};
    ASSERT_EQ(nx_native_uart_configure_instance(&first, &first_config),
              NX_SUCCESS);
    ASSERT_EQ(nx_native_uart_configure_instance(&second, &second_config),
              NX_SUCCESS);
    uint8_t first_data = 17;
    uint8_t second_data = 29;
    nx_uart_tx_request_t first_request = {};
    nx_uart_tx_request_t second_request = {};
    nx_request_initialize(&first_request.base);
    nx_request_initialize(&second_request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&first_request, &first_data, 1, 100),
              NX_SUCCESS);
    ASSERT_EQ(nx_uart_tx_prepare(&second_request, &second_data, 1, 100),
              NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&first_api, &first_request), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&second_api, &second_request), NX_SUCCESS);
    nx_native_uart_irq_step_instance(&first);
    nx_native_uart_irq_step_instance(&first);
    nx_uart_port_service(&first_api);
    EXPECT_EQ(nx_request_state(&first_request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(nx_request_state(&second_request.base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(first_log[0], 17);
    EXPECT_EQ(nx_native_uart_transmitted_instance(&second), 0u);
    EXPECT_EQ(nx_native_uart_configure_instance(&second, &second_config),
              NX_ERROR_BUSY);
    ASSERT_EQ(nx_native_uart_receive_instance(&first, 3, 0), NX_SUCCESS);
    ASSERT_EQ(nx_native_uart_receive_instance(&second, 7, 0), NX_SUCCESS);
    uint8_t received = 0;
    size_t count = 0;
    ASSERT_EQ(nx_uart_port_read_bytes(&first_api, &received, 1, &count),
              NX_SUCCESS);
    EXPECT_EQ(received, 3);
    ASSERT_EQ(nx_uart_port_read_bytes(&second_api, &received, 1, &count),
              NX_SUCCESS);
    EXPECT_EQ(received, 7);
    ASSERT_EQ(nx_uart_port_stop(&second_api), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&second_request.base), NX_REQUEST_SETTLED);
    ASSERT_EQ(nx_uart_port_stop(&first_api), NX_SUCCESS);
}

TEST_F(NativeInstances, SPIEndpointsShareArbitrationAndKeepDeviceMemory) {
    nx_native_spi_state_t bus = {};
    nx_native_spi_state_t other_bus = {};
    nx_native_spi_endpoint_state_t first = {};
    nx_native_spi_endpoint_state_t second = {};
    nx_native_spi_endpoint_state_t third = {};
    const nx_spi_port_t bus_api = {&nx_native_spi_ops, &bus};
    const nx_spi_endpoint_t first_api = {&nx_native_spi_endpoint_ops, &first};
    const nx_spi_endpoint_t second_api = {&nx_native_spi_endpoint_ops, &second};
    const nx_spi_endpoint_t third_api = {&nx_native_spi_endpoint_ops, &third};
    uint8_t first_memory[2] = {11, 12};
    uint8_t second_memory[2] = {21, 22};
    uint8_t third_memory[2] = {31, 32};
    ASSERT_EQ(nx_native_spi_port_configure_instance(&bus), NX_SUCCESS);
    ASSERT_EQ(nx_native_spi_port_configure_instance(&other_bus), NX_SUCCESS);
    ASSERT_EQ(nx_native_spi_configure_instance(&first, &bus, first_memory, 2),
              NX_SUCCESS);
    ASSERT_EQ(nx_native_spi_configure_instance(&second, &bus, second_memory, 2),
              NX_SUCCESS);
    ASSERT_EQ(
        nx_native_spi_configure_instance(&third, &other_bus, third_memory, 2),
        NX_SUCCESS);
    uint8_t tx[2] = {0x80, 0};
    uint8_t rx[2] = {};
    size_t count = 0;
    ASSERT_EQ(nx_spi_endpoint_transfer(&first_api, tx, rx, 2, 100, &count),
              NX_SUCCESS);
    EXPECT_EQ(rx[1], 11);
    ASSERT_EQ(nx_spi_endpoint_transfer(&second_api, tx, rx, 2, 100, &count),
              NX_SUCCESS);
    EXPECT_EQ(rx[1], 21);
    nx_native_spi_fault_instance(&bus, SIZE_MAX, true);
    EXPECT_EQ(nx_spi_endpoint_transfer(&first_api, tx, rx, 2, 100, &count),
              NX_ERROR_BUSY);
    EXPECT_EQ(nx_spi_endpoint_transfer(&second_api, tx, rx, 2, 100, &count),
              NX_ERROR_BUSY);
    EXPECT_EQ(nx_spi_endpoint_transfer(&third_api, tx, rx, 2, 100, &count),
              NX_SUCCESS);
    EXPECT_EQ(rx[1], 31);
    EXPECT_EQ(nx_spi_port_recover(&bus_api), NX_ERROR_BUSY);
    nx_native_spi_fault_instance(&bus, SIZE_MAX, false);
    EXPECT_EQ(nx_spi_port_recover(&bus_api), NX_SUCCESS);
}

TEST_F(NativeInstances, I2CEndpointCursorAndFaultsHaveExplicitControllerScope) {
    nx_native_i2c_state_t bus = {};
    nx_native_i2c_state_t other_bus = {};
    nx_native_i2c_endpoint_state_t first = {};
    nx_native_i2c_endpoint_state_t second = {};
    nx_native_i2c_endpoint_state_t third = {};
    const nx_i2c_port_t bus_api = {&nx_native_i2c_ops, &bus};
    const nx_i2c_endpoint_t first_api = {&nx_native_i2c_endpoint_ops, &first};
    const nx_i2c_endpoint_t second_api = {&nx_native_i2c_endpoint_ops, &second};
    const nx_i2c_endpoint_t third_api = {&nx_native_i2c_endpoint_ops, &third};
    uint8_t first_memory[2] = {11, 12};
    uint8_t second_memory[2] = {21, 22};
    uint8_t third_memory[2] = {31, 32};
    ASSERT_EQ(nx_native_i2c_port_configure_instance(&bus), NX_SUCCESS);
    ASSERT_EQ(nx_native_i2c_port_configure_instance(&other_bus), NX_SUCCESS);
    ASSERT_EQ(
        nx_native_i2c_configure_instance(&first, &bus, first_memory, 2, 0x20),
        NX_SUCCESS);
    ASSERT_EQ(
        nx_native_i2c_configure_instance(&second, &bus, second_memory, 2, 0x21),
        NX_SUCCESS);
    ASSERT_EQ(nx_native_i2c_configure_instance(&third, &other_bus, third_memory,
                                               2, 0x22),
              NX_SUCCESS);
    uint8_t address = 0;
    uint8_t value = 0;
    nx_i2c_message_t messages[] = {{&address, 1, false}, {&value, 1, true}};
    size_t count = 0;
    ASSERT_EQ(nx_i2c_endpoint_transaction(&first_api, messages, 2, 100, &count),
              NX_SUCCESS);
    EXPECT_EQ(value, 11);
    ASSERT_EQ(
        nx_i2c_endpoint_transaction(&second_api, messages, 2, 100, &count),
        NX_SUCCESS);
    EXPECT_EQ(value, 21);
    nx_native_i2c_fault_instance(&bus, NX_ERROR_NACK, false);
    EXPECT_EQ(nx_i2c_endpoint_transaction(&first_api, messages, 2, 100, &count),
              NX_ERROR_NACK);
    EXPECT_EQ(nx_i2c_endpoint_transaction(&third_api, messages, 2, 100, &count),
              NX_SUCCESS);
    EXPECT_EQ(value, 31);
    ASSERT_EQ(nx_i2c_port_recover(&bus_api), NX_SUCCESS);
    EXPECT_EQ(nx_i2c_endpoint_transaction(&first_api, messages, 2, 100, &count),
              NX_SUCCESS);
}

TEST_F(NativeInstances, FlashProgramAndFaultsDoNotCrossInstanceMemory) {
    nx_native_flash_state_t first = {};
    nx_native_flash_state_t second = {};
    const nx_flash_port_t first_api = {&nx_native_flash_ops, &first};
    const nx_flash_port_t second_api = {&nx_native_flash_ops, &second};
    uint8_t first_memory[8] = {};
    uint8_t second_memory[8] = {};
    const nx_flash_sector_t sectors[] = {{0, 8}};
    const nx_flash_geometry_t geometry = {0, 8, 1, sectors, 1};
    ASSERT_EQ(
        nx_native_flash_configure_instance(&first, first_memory, &geometry),
        NX_SUCCESS);
    ASSERT_EQ(
        nx_native_flash_configure_instance(&second, second_memory, &geometry),
        NX_SUCCESS);
    uint8_t byte = 0x37;
    nx_native_flash_fault_instance(&first, 0);
    EXPECT_EQ(nx_flash_port_program(&first_api, 0, &byte, 1, 100), NX_ERROR_IO);
    ASSERT_EQ(nx_flash_port_program(&second_api, 0, &byte, 1, 100), NX_SUCCESS);
    EXPECT_EQ(first_memory[0], 0xff);
    EXPECT_EQ(second_memory[0], byte);
}

TEST_F(NativeInstances, EXTIQueuesAndStopAreIndependent) {
    nx_native_exti_state_t first = {};
    nx_native_exti_state_t second = {};
    const nx_exti_port_t first_api = {&nx_native_exti_ops, &first};
    const nx_exti_port_t second_api = {&nx_native_exti_ops, &second};
    nx_exti_event_t first_storage[1] = {};
    nx_exti_event_t second_storage[1] = {};
    ASSERT_EQ(nx_native_exti_configure_instance(&first, 1, NX_EXTI_RISING,
                                                first_storage, 1),
              NX_SUCCESS);
    ASSERT_EQ(nx_native_exti_configure_instance(&second, 2, NX_EXTI_BOTH,
                                                second_storage, 1),
              NX_SUCCESS);
    ASSERT_EQ(nx_native_exti_emit_instance(&first, 1, NX_EXTI_RISING),
              NX_SUCCESS);
    EXPECT_EQ(nx_native_exti_emit_instance(&first, 1, NX_EXTI_RISING),
              NX_ERROR_OVERFLOW);
    ASSERT_EQ(nx_native_exti_emit_instance(&second, 2, NX_EXTI_FALLING),
              NX_SUCCESS);
    ASSERT_EQ(nx_exti_port_stop(&first_api), NX_SUCCESS);
    nx_exti_event_t result = {};
    size_t count = 0;
    ASSERT_EQ(nx_exti_port_read(&second_api, &result, 1, &count), NX_SUCCESS);
    EXPECT_EQ(result.line, 2);
    EXPECT_EQ(result.flags, 0u);
}

TEST_F(NativeInstances, PWMBasesAndADCSequencesAreIndependent) {
    nx_native_pwm_state_t first_pwm = {};
    nx_native_pwm_state_t second_pwm = {};
    const nx_pwm_port_t first_pwm_api = {&nx_native_pwm_ops, &first_pwm};
    const nx_pwm_port_t second_pwm_api = {&nx_native_pwm_ops, &second_pwm};
    ASSERT_EQ(nx_native_pwm_configure_instance(&first_pwm, 1000, 10),
              NX_SUCCESS);
    ASSERT_EQ(nx_native_pwm_configure_instance(&second_pwm, 2000, 20),
              NX_SUCCESS);
    ASSERT_EQ(nx_pwm_port_set(&first_pwm_api, 10, 3), NX_SUCCESS);
    ASSERT_EQ(nx_pwm_port_start(&first_pwm_api), NX_SUCCESS);
    nx_pwm_state_t pwm = {};
    ASSERT_EQ(nx_pwm_port_state(&second_pwm_api, &pwm), NX_SUCCESS);
    EXPECT_FALSE(pwm.running);
    EXPECT_EQ(pwm.period_ticks, 20u);
    EXPECT_EQ(pwm.duty_ticks, 0u);
    nx_native_adc_state_t first_adc = {};
    nx_native_adc_state_t second_adc = {};
    const nx_adc_port_t first_adc_api = {&nx_native_adc_ops, &first_adc};
    const nx_adc_port_t second_adc_api = {&nx_native_adc_ops, &second_adc};
    const uint16_t first_samples[] = {12, 13};
    const uint16_t second_samples[] = {21, 22};
    ASSERT_EQ(nx_native_adc_configure_instance(&first_adc, first_samples, 2, 12,
                                               3300),
              NX_SUCCESS);
    ASSERT_EQ(nx_native_adc_configure_instance(&second_adc, second_samples, 2,
                                               12, 3300),
              NX_SUCCESS);
    nx_native_adc_fault_instance(&first_adc, 1);
    uint16_t samples[2] = {};
    size_t count = 0;
    EXPECT_EQ(nx_adc_port_sample(&first_adc_api, samples, 2, 100, &count),
              NX_ERROR_IO);
    EXPECT_EQ(count, 1u);
    ASSERT_EQ(nx_adc_port_sample(&second_adc_api, samples, 2, 100, &count),
              NX_SUCCESS);
    EXPECT_EQ(count, 2u);
    EXPECT_EQ(samples[0], 21);
}

TEST_F(NativeInstances, WatchdogEffectsRemainInstanceLocal) {
    nx_native_watchdog_state_t first = {};
    nx_native_watchdog_state_t second = {};
    const nx_watchdog_port_t first_api = {&nx_native_watchdog_ops, &first};
    const nx_watchdog_port_t second_api = {&nx_native_watchdog_ops, &second};
    nx_native_watchdog_boot_instance(&first, NX_RESET_PIN);
    nx_native_watchdog_boot_instance(&second, NX_RESET_POWER_ON);
    nx_watchdog_state_t state = {};
    ASSERT_EQ(nx_watchdog_port_enable(&first_api, 1000, false, &state),
              NX_SUCCESS);
    ASSERT_EQ(nx_watchdog_port_enable(&second_api, 2000, false, &state),
              NX_SUCCESS);
    ASSERT_EQ(nx_native_clock_advance(1000), NX_SUCCESS);
    EXPECT_TRUE(nx_native_watchdog_expired_instance(&first));
    EXPECT_FALSE(nx_native_watchdog_expired_instance(&second));
    EXPECT_EQ(nx_watchdog_port_feed(&first_api), NX_ERROR_IO);
    EXPECT_EQ(nx_watchdog_port_feed(&second_api), NX_SUCCESS);
}

TEST_F(NativeInstances, PlatformStopWithdrawsFlashADCAndPWMAdmission) {
    nx_native_flash_state_t flash = {};
    const nx_flash_port_t flash_api = {&nx_native_flash_ops, &flash};
    uint8_t bytes[8] = {};
    const nx_flash_sector_t sector = {0, 8};
    const nx_flash_geometry_t geometry = {0, 8, 1, &sector, 1};
    ASSERT_EQ(nx_native_flash_configure_instance(&flash, bytes, &geometry),
              NX_SUCCESS);
    ASSERT_EQ(nx_native_flash_stop_instance(&flash), NX_SUCCESS);
    EXPECT_EQ(nx_flash_port_geometry(&flash_api), nullptr);
    uint8_t value = 0;
    EXPECT_EQ(nx_flash_port_read(&flash_api, 0, &value, 1), NX_ERROR_INVALID);
    nx_native_adc_state_t adc = {};
    const nx_adc_port_t adc_api = {&nx_native_adc_ops, &adc};
    const uint16_t samples[] = {17};
    ASSERT_EQ(nx_native_adc_configure_instance(&adc, samples, 1, 12, 3300),
              NX_SUCCESS);
    ASSERT_EQ(nx_native_adc_stop_instance(&adc), NX_SUCCESS);
    nx_adc_info_t info = {};
    EXPECT_EQ(nx_adc_port_info(&adc_api, &info), NX_ERROR_INVALID);
    uint16_t sample = 0;
    size_t count = 0;
    EXPECT_EQ(nx_adc_port_sample(&adc_api, &sample, 1, 100, &count),
              NX_ERROR_INVALID);
    nx_native_pwm_state_t pwm = {};
    const nx_pwm_port_t pwm_api = {&nx_native_pwm_ops, &pwm};
    ASSERT_EQ(nx_native_pwm_configure_instance(&pwm, 1000, 10), NX_SUCCESS);
    ASSERT_EQ(nx_pwm_port_start(&pwm_api), NX_SUCCESS);
    ASSERT_EQ(nx_native_pwm_stop_instance(&pwm), NX_SUCCESS);
    EXPECT_EQ(nx_pwm_port_start(&pwm_api), NX_ERROR_STATE);
    EXPECT_EQ(nx_pwm_port_set(&pwm_api, 10, 1), NX_ERROR_STATE);
}

TEST_F(NativeInstances, WatchdogAssemblyRestartPreservesIrreversibleEffect) {
    nx_native_watchdog_state_t state = {};
    const nx_watchdog_port_t api = {&nx_native_watchdog_ops, &state};
    nx_watchdog_state_t effect = {};
    EXPECT_EQ(nx_watchdog_port_enable(&api, 1000, false, &effect),
              NX_ERROR_STATE);
    ASSERT_EQ(nx_native_watchdog_initialize_instance(&state), NX_SUCCESS);
    ASSERT_EQ(nx_watchdog_port_enable(&api, 1000, false, &effect), NX_SUCCESS);
    ASSERT_TRUE(effect.enabled);
    ASSERT_EQ(nx_native_watchdog_initialize_instance(&state), NX_SUCCESS);
    EXPECT_EQ(nx_watchdog_port_enable(&api, 2000, false, &effect),
              NX_ERROR_STATE);
    EXPECT_TRUE(effect.enabled);
    EXPECT_EQ(nx_watchdog_port_feed(&api), NX_SUCCESS);
}

TEST_F(NativeInstances, I2CLongReadUsesTheSameBoundedMessageContract) {
    nx_native_i2c_state_t state = {};
    nx_native_i2c_endpoint_state_t device = {};
    const nx_i2c_endpoint_t endpoint = {&nx_native_i2c_endpoint_ops, &device};
    uint8_t memory[256] = {};
    uint8_t output[257] = {};
    for (size_t i = 0; i < sizeof(memory); ++i) {
        memory[i] = static_cast<uint8_t>(i);
    }
    ASSERT_EQ(nx_native_i2c_port_configure_instance(&state), NX_SUCCESS);
    ASSERT_EQ(nx_native_i2c_configure_instance(&device, &state, memory,
                                               sizeof(memory), 0x50),
              NX_SUCCESS);
    size_t transferred = 0;
    nx_i2c_message_t read = {output, 256, true};
    ASSERT_EQ(
        nx_i2c_endpoint_transaction(&endpoint, &read, 1, 1000, &transferred),
        NX_SUCCESS);
    EXPECT_EQ(transferred, 256U);
    EXPECT_THAT(std::vector<uint8_t>(output, output + 256),
                testing::ElementsAreArray(memory, 256));
    device.cursor = 0;
    read.length = 257;
    EXPECT_EQ(
        nx_i2c_endpoint_transaction(&endpoint, &read, 1, 1000, &transferred),
        NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(transferred, 0U);
    EXPECT_EQ(device.cursor, 0U);
    read.length = 3;
    EXPECT_EQ(nx_i2c_endpoint_transaction(&endpoint, &read, 1,
                                          NX_DEADLINE_NEVER, &transferred),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(device.cursor, 0U);
    nx_i2c_message_t messages[9] = {};
    for (auto& message : messages) {
        message = {output, 1, true};
    }
    EXPECT_EQ(
        nx_i2c_endpoint_transaction(&endpoint, messages, 9, 1000, &transferred),
        NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(device.cursor, 0U);
}

TEST_F(NativeInstances, I2CDeadlineAndRecoveryPreserveExecutionContracts) {
    nx_native_i2c_state_t state = {};
    nx_native_i2c_endpoint_state_t device = {};
    const nx_i2c_endpoint_t endpoint = {&nx_native_i2c_endpoint_ops, &device};
    const nx_i2c_port_t port = {&nx_native_i2c_ops, &state};
    uint8_t memory[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    uint8_t output[8] = {};
    ASSERT_EQ(nx_native_i2c_port_configure_instance(&state), NX_SUCCESS);
    ASSERT_EQ(nx_native_i2c_configure_instance(&device, &state, memory,
                                               sizeof(memory), 0x50),
              NX_SUCCESS);
    nx_i2c_message_t read = {output, sizeof(output), true};
    size_t transferred = 0;
    EXPECT_EQ(nx_i2c_endpoint_transaction(&endpoint, &read, 1, 3, &transferred),
              NX_ERROR_TIMEOUT);
    EXPECT_EQ(transferred, 3U);
    EXPECT_THAT(output, testing::ElementsAre(1, 2, 3, 0, 0, 0, 0, 0));
    EXPECT_FALSE(state.active);
    nx_native_i2c_fault_instance(&state, NX_ERROR_NACK, false);
    const nx_arch_irq_state_t incoming = nx_arch_irq_save();
    const nx_result_t recovery = nx_i2c_port_recover(&port);
    nx_arch_irq_restore(incoming);
    EXPECT_EQ(recovery, NX_ERROR_CONTEXT);
    EXPECT_EQ(state.fault, NX_ERROR_NACK);
    ASSERT_EQ(nx_i2c_port_recover(&port), NX_SUCCESS);
    ASSERT_EQ(nx_native_i2c_stop_instance(&state), NX_SUCCESS);
    EXPECT_EQ(nx_i2c_port_recover(&port), NX_ERROR_STATE);
}

TEST_F(NativeInstances, I2CDeadlineIncludesTheLastByteInterval) {
    nx_native_i2c_state_t state = {};
    nx_native_i2c_endpoint_state_t device = {};
    const nx_i2c_endpoint_t endpoint = {&nx_native_i2c_endpoint_ops, &device};
    uint8_t memory[3] = {1, 2, 3};
    uint8_t output[3] = {};
    ASSERT_EQ(nx_native_i2c_port_configure_instance(&state), NX_SUCCESS);
    ASSERT_EQ(nx_native_i2c_configure_instance(&device, &state, memory,
                                               sizeof(memory), 0x50),
              NX_SUCCESS);
    nx_i2c_message_t read = {output, sizeof(output), true};
    size_t transferred = 0;
    EXPECT_EQ(nx_i2c_endpoint_transaction(&endpoint, &read, 1, 3, &transferred),
              NX_ERROR_TIMEOUT);
    EXPECT_EQ(transferred, 3U);
    EXPECT_THAT(output, testing::ElementsAre(1, 2, 3));
    EXPECT_FALSE(state.active);
}

} /* namespace */
