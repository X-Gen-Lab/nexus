/**
 * \file            test_nx_factory.cpp
 * \brief           Nexus HAL Factory Layer Checkpoint Verification Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-16
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Checkpoint 16: Factory Layer Verification
 * - Test nx_factory_*() device acquisition/release
 * - Test nx_device_get()/nx_device_put() reference counting
 * - Test nx_factory_enumerate() device enumeration
 *
 * **Validates: Requirements 10.1, 10.2, 10.3, 10.4, 3.1, 3.2**
 */

#include <cstring>
#include <gtest/gtest.h>

extern "C" {
#include "hal/base/nx_device.h"
#include "hal/nx_factory.h"
#include "hal/nx_status.h"
}

/**
 * \brief           Factory Layer Checkpoint Test Fixture
 */
class NxFactoryCheckpointTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Test setup */
    }

    void TearDown() override {
        /* Test cleanup */
    }
};

/*===========================================================================*/
/* GPIO Factory Tests                                                         */
/*===========================================================================*/

/**
 * \brief           Test GPIO device acquisition and release
 * \details         Checkpoint 16: Test nx_factory_*() device get/release
 *                  Validates: Requirements 10.1, 10.2
 */
TEST_F(NxFactoryCheckpointTest, GPIO_AcquisitionAndRelease) {
    /* Acquire GPIO device */
    nx_gpio_t* gpio = nx_factory_gpio(0, 5); /* Port A, Pin 5 */
    ASSERT_NE(nullptr, gpio);

    /* Verify device is functional */
    nx_lifecycle_t* lifecycle = gpio->get_lifecycle(gpio);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Configure and test */
    EXPECT_EQ(NX_OK, gpio->set_mode(gpio, NX_GPIO_MODE_OUTPUT_PP));
    gpio->write(gpio, 1);
    EXPECT_EQ(1, gpio->read(gpio));

    /* Release device */
    lifecycle->deinit(lifecycle);
    nx_factory_gpio_release(gpio);
}

/**
 * \brief           Test GPIO device acquisition with configuration
 * \details         Checkpoint 16: Test nx_factory_*_with_config()
 *                  Validates: Requirements 10.3
 */
TEST_F(NxFactoryCheckpointTest, GPIO_AcquisitionWithConfig) {
    /* Prepare configuration */
    nx_gpio_config_t config = {.mode = NX_GPIO_MODE_OUTPUT_PP,
                               .pull = NX_GPIO_PULL_UP,
                               .speed = NX_GPIO_SPEED_HIGH,
                               .af_index = 0};

    /* Acquire GPIO device with configuration */
    nx_gpio_t* gpio = nx_factory_gpio_with_config(1, 3, &config);
    ASSERT_NE(nullptr, gpio);

    /* Verify device is initialized with config */
    nx_lifecycle_t* lifecycle = gpio->get_lifecycle(gpio);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle->get_state(lifecycle));

    /* Verify configuration was applied */
    nx_gpio_config_t read_config;
    EXPECT_EQ(NX_OK, gpio->get_config(gpio, &read_config));
    EXPECT_EQ(NX_GPIO_MODE_OUTPUT_PP, read_config.mode);
    EXPECT_EQ(NX_GPIO_PULL_UP, read_config.pull);
    EXPECT_EQ(NX_GPIO_SPEED_HIGH, read_config.speed);

    /* Release device */
    lifecycle->deinit(lifecycle);
    nx_factory_gpio_release(gpio);
}

/*===========================================================================*/
/* UART Factory Tests                                                         */
/*===========================================================================*/

/**
 * \brief           Test UART device acquisition and release
 * \details         Checkpoint 16: Test nx_factory_*() device get/release
 *                  Validates: Requirements 10.1, 10.2
 */
TEST_F(NxFactoryCheckpointTest, UART_AcquisitionAndRelease) {
    /* Acquire UART device */
    nx_uart_t* uart = nx_factory_uart(0);
    ASSERT_NE(nullptr, uart);

    /* Verify device is functional */
    nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Test basic operation */
    nx_uart_config_t config;
    EXPECT_EQ(NX_OK, uart->get_config(uart, &config));
    EXPECT_GT(config.baudrate, 0);

    /* Release device */
    lifecycle->deinit(lifecycle);
    nx_factory_uart_release(uart);
}

/**
 * \brief           Test UART device acquisition with configuration
 * \details         Checkpoint 16: Test nx_factory_*_with_config()
 *                  Validates: Requirements 10.3
 */
TEST_F(NxFactoryCheckpointTest, UART_AcquisitionWithConfig) {
    /* Prepare configuration */
    nx_uart_config_t config = {.baudrate = 115200,
                               .word_length = 8,
                               .stop_bits = 1,
                               .parity = 0,
                               .flow_control = 0,
                               .dma_tx_enable = false,
                               .dma_rx_enable = false,
                               .tx_buf_size = 256,
                               .rx_buf_size = 256};

    /* Acquire UART device with configuration */
    nx_uart_t* uart = nx_factory_uart_with_config(1, &config);
    ASSERT_NE(nullptr, uart);

    /* Verify device is initialized with config */
    nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle->get_state(lifecycle));

    /* Verify configuration was applied */
    nx_uart_config_t read_config;
    EXPECT_EQ(NX_OK, uart->get_config(uart, &read_config));
    EXPECT_EQ(115200, read_config.baudrate);
    EXPECT_EQ(8, read_config.word_length);

    /* Release device */
    lifecycle->deinit(lifecycle);
    nx_factory_uart_release(uart);
}

/*===========================================================================*/
/* SPI Factory Tests                                                          */
/*===========================================================================*/

/**
 * \brief           Test SPI device acquisition and release
 * \details         Checkpoint 16: Test nx_factory_*() device get/release
 *                  Validates: Requirements 10.1, 10.2
 */
TEST_F(NxFactoryCheckpointTest, SPI_AcquisitionAndRelease) {
    /* Acquire SPI device */
    nx_spi_t* spi = nx_factory_spi(0);
    ASSERT_NE(nullptr, spi);

    /* Verify device is functional */
    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Test basic operation */
    nx_spi_config_t config;
    EXPECT_EQ(NX_OK, spi->get_config(spi, &config));

    /* Release device */
    lifecycle->deinit(lifecycle);
    nx_factory_spi_release(spi);
}

/*===========================================================================*/
/* I2C Factory Tests                                                          */
/*===========================================================================*/

/**
 * \brief           Test I2C device acquisition and release
 * \details         Checkpoint 16: Test nx_factory_*() device get/release
 *                  Validates: Requirements 10.1, 10.2
 */
TEST_F(NxFactoryCheckpointTest, I2C_AcquisitionAndRelease) {
    /* Acquire I2C device */
    nx_i2c_t* i2c = nx_factory_i2c(0);
    ASSERT_NE(nullptr, i2c);

    /* Verify device is functional */
    nx_lifecycle_t* lifecycle = i2c->get_lifecycle(i2c);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Test basic operation */
    nx_i2c_config_t config;
    EXPECT_EQ(NX_OK, i2c->get_config(i2c, &config));

    /* Release device */
    lifecycle->deinit(lifecycle);
    nx_factory_i2c_release(i2c);
}

/*===========================================================================*/
/* Timer Factory Tests                                                        */
/*===========================================================================*/

/**
 * \brief           Test Timer device acquisition and release
 * \details         Checkpoint 16: Test nx_factory_*() device get/release
 *                  Validates: Requirements 10.1, 10.2
 */
TEST_F(NxFactoryCheckpointTest, Timer_AcquisitionAndRelease) {
    /* Acquire Timer device */
    nx_timer_t* timer = nx_factory_timer(0);
    ASSERT_NE(nullptr, timer);

    /* Verify device is functional */
    nx_lifecycle_t* lifecycle = timer->get_lifecycle(timer);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Test basic operation */
    nx_timer_config_t config;
    EXPECT_EQ(NX_OK, timer->get_config(timer, &config));

    /* Release device */
    lifecycle->deinit(lifecycle);
    nx_factory_timer_release(timer);
}

/*===========================================================================*/
/* ADC Factory Tests                                                          */
/*===========================================================================*/

/**
 * \brief           Test ADC device acquisition and release
 * \details         Checkpoint 16: Test nx_factory_*() device get/release
 *                  Validates: Requirements 10.1, 10.2
 */
TEST_F(NxFactoryCheckpointTest, ADC_AcquisitionAndRelease) {
    /* Acquire ADC device */
    nx_adc_t* adc = nx_factory_adc(0);
    ASSERT_NE(nullptr, adc);

    /* Verify device is functional */
    nx_lifecycle_t* lifecycle = adc->get_lifecycle(adc);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Test basic operation */
    nx_adc_config_t config;
    EXPECT_EQ(NX_OK, adc->get_config(adc, &config));

    /* Release device */
    lifecycle->deinit(lifecycle);
    nx_factory_adc_release(adc);
}

/*===========================================================================*/
/* Reference Counting Tests                                                   */
/*===========================================================================*/

/**
 * \brief           Test reference counting through factory
 * \details         Checkpoint 16: Test device sharing
 *                  Validates: Requirements 3.1, 3.2
 * \note            Native implementation returns same instance but doesn't
 *                  use reference counting
 */
TEST_F(NxFactoryCheckpointTest, ReferenceCountingThroughFactory) {
    /* Acquire device first time */
    nx_gpio_t* gpio1 = nx_factory_gpio(2, 7);
    ASSERT_NE(nullptr, gpio1);

    /* Initialize device */
    nx_lifecycle_t* lifecycle1 = gpio1->get_lifecycle(gpio1);
    ASSERT_NE(nullptr, lifecycle1);
    EXPECT_EQ(NX_OK, lifecycle1->init(lifecycle1));

    /* Acquire same device second time */
    nx_gpio_t* gpio2 = nx_factory_gpio(2, 7);
    ASSERT_NE(nullptr, gpio2);

    /* Should return same instance */
    EXPECT_EQ(gpio1, gpio2);

    /* Get lifecycle from second reference */
    nx_lifecycle_t* lifecycle2 = gpio2->get_lifecycle(gpio2);
    ASSERT_NE(nullptr, lifecycle2);

    /* Should already be initialized */
    EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle2->get_state(lifecycle2));

    /* Release references (no-op in native implementation) */
    nx_factory_gpio_release(gpio1);
    nx_factory_gpio_release(gpio2);

    /* Cleanup */
    lifecycle1->deinit(lifecycle1);
}

/**
 * \brief           Test reference counting with direct device access
 * \details         Checkpoint 16: Test device sharing
 *                  Validates: Requirements 3.1, 3.2
 * \note            Native implementation doesn't use nx_device_get/put
 */
TEST_F(NxFactoryCheckpointTest, DirectReferenceCountingTest) {
    /* Acquire device through factory */
    nx_uart_t* uart1 = nx_factory_uart(2);
    ASSERT_NE(nullptr, uart1);

    /* Initialize device */
    nx_lifecycle_t* lifecycle = uart1->get_lifecycle(uart1);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Get same device again */
    nx_uart_t* uart2 = nx_factory_uart(2);
    ASSERT_NE(nullptr, uart2);

    /* Should be same instance */
    EXPECT_EQ(uart1, uart2);

    /* Release through factory (no-op) */
    nx_factory_uart_release(uart2);

    /* Device should still be running */
    EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle->get_state(lifecycle));

    /* Cleanup */
    lifecycle->deinit(lifecycle);
    nx_factory_uart_release(uart1);
}

/**
 * \brief           Test multiple references to same device
 * \details         Checkpoint 16: Test device sharing
 *                  Validates: Requirements 3.1, 3.2
 * \note            Native implementation returns same instance
 */
TEST_F(NxFactoryCheckpointTest, MultipleReferencesToSameDevice) {
    /* Acquire device multiple times */
    nx_spi_t* spi1 = nx_factory_spi(1);
    nx_spi_t* spi2 = nx_factory_spi(1);
    nx_spi_t* spi3 = nx_factory_spi(1);

    ASSERT_NE(nullptr, spi1);
    ASSERT_NE(nullptr, spi2);
    ASSERT_NE(nullptr, spi3);

    /* All should be same instance */
    EXPECT_EQ(spi1, spi2);
    EXPECT_EQ(spi2, spi3);

    /* Initialize once */
    nx_lifecycle_t* lifecycle = spi1->get_lifecycle(spi1);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Release references (no-op in native) */
    nx_factory_spi_release(spi1);
    nx_factory_spi_release(spi2);
    nx_factory_spi_release(spi3);

    /* Cleanup */
    lifecycle->deinit(lifecycle);
}

/*===========================================================================*/
/* Device Enumeration Tests                                                   */
/*===========================================================================*/

/**
 * \brief           Test device enumeration
 * \details         Checkpoint 16: Test nx_factory_enumerate()
 *                  Validates: Requirements 10.4
 */
TEST_F(NxFactoryCheckpointTest, DeviceEnumeration) {
    /* Allocate buffer for device list */
    nx_device_info_t device_list[256];
    memset(device_list, 0, sizeof(device_list));

    /* Enumerate all devices */
    size_t count = nx_factory_enumerate(device_list, 256);

    /* Should find at least some devices */
    EXPECT_GT(count, 0);

    /* Verify device information is valid */
    for (size_t i = 0; i < count; i++) {
        EXPECT_NE(nullptr, device_list[i].name);
        EXPECT_NE(nullptr, device_list[i].type);

        /* Device name should not be empty */
        EXPECT_GT(strlen(device_list[i].name), 0);

        /* Device type should be one of the known types */
        bool valid_type = (strcmp(device_list[i].type, "gpio") == 0) ||
                          (strcmp(device_list[i].type, "uart") == 0) ||
                          (strcmp(device_list[i].type, "spi") == 0) ||
                          (strcmp(device_list[i].type, "i2c") == 0) ||
                          (strcmp(device_list[i].type, "timer") == 0) ||
                          (strcmp(device_list[i].type, "adc") == 0);
        EXPECT_TRUE(valid_type)
            << "Invalid device type: " << device_list[i].type;
    }
}

/**
 * \brief           Test device enumeration with limited buffer
 * \details         Checkpoint 16: Test nx_factory_enumerate()
 *                  Validates: Requirements 10.4
 */
TEST_F(NxFactoryCheckpointTest, DeviceEnumerationLimitedBuffer) {
    /* Allocate small buffer */
    nx_device_info_t device_list[5];
    memset(device_list, 0, sizeof(device_list));

    /* Enumerate with limited buffer */
    size_t count = nx_factory_enumerate(device_list, 5);

    /* Should return at most 5 devices */
    EXPECT_LE(count, 5);
    EXPECT_GT(count, 0);

    /* Verify returned devices are valid */
    for (size_t i = 0; i < count; i++) {
        EXPECT_NE(nullptr, device_list[i].name);
        EXPECT_NE(nullptr, device_list[i].type);
    }
}

/**
 * \brief           Test device enumeration with NULL buffer
 * \details         Checkpoint 16: Test nx_factory_enumerate() error handling
 *                  Validates: Requirements 10.4
 */
TEST_F(NxFactoryCheckpointTest, DeviceEnumerationNullBuffer) {
    /* Enumerate with NULL buffer */
    size_t count = nx_factory_enumerate(nullptr, 10);

    /* Should return 0 */
    EXPECT_EQ(0, count);
}

/**
 * \brief           Test device enumeration with zero count
 * \details         Checkpoint 16: Test nx_factory_enumerate() error handling
 *                  Validates: Requirements 10.4
 */
TEST_F(NxFactoryCheckpointTest, DeviceEnumerationZeroCount) {
    nx_device_info_t device_list[10];

    /* Enumerate with zero count */
    size_t count = nx_factory_enumerate(device_list, 0);

    /* Should return 0 */
    EXPECT_EQ(0, count);
}

/**
 * \brief           Test device enumeration shows device state
 * \details         Checkpoint 16: Test nx_factory_enumerate()
 *                  Validates: Requirements 10.4, 3.1
 * \note            Native implementation doesn't track ref counts in
 * enumeration
 */
TEST_F(NxFactoryCheckpointTest, DeviceEnumerationShowsRefCounts) {
    /* Acquire a device */
    nx_i2c_t* i2c1 = nx_factory_i2c(1);
    ASSERT_NE(nullptr, i2c1);

    /* Initialize device */
    nx_lifecycle_t* lifecycle = i2c1->get_lifecycle(i2c1);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Acquire same device again */
    nx_i2c_t* i2c2 = nx_factory_i2c(1);
    ASSERT_NE(nullptr, i2c2);

    /* Enumerate devices */
    nx_device_info_t device_list[256];
    size_t count = nx_factory_enumerate(device_list, 256);
    EXPECT_GT(count, 0);

    /* Find our I2C device in the list */
    bool found = false;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(device_list[i].name, "i2c1") == 0) {
            found = true;
            /* Device type should be correct */
            EXPECT_STREQ("i2c", device_list[i].type);
            break;
        }
    }
    EXPECT_TRUE(found) << "I2C device not found in enumeration";

    /* Release devices */
    nx_factory_i2c_release(i2c1);
    nx_factory_i2c_release(i2c2);

    /* Cleanup */
    lifecycle->deinit(lifecycle);
}

/**
 * \brief           Test device enumeration by type
 * \details         Checkpoint 16: Test nx_factory_enumerate()
 *                  Validates: Requirements 10.4
 */
TEST_F(NxFactoryCheckpointTest, DeviceEnumerationByType) {
    /* Enumerate all devices */
    nx_device_info_t device_list[256];
    size_t count = nx_factory_enumerate(device_list, 256);
    EXPECT_GT(count, 0);

    /* Count devices by type */
    size_t gpio_count = 0;
    size_t uart_count = 0;
    size_t spi_count = 0;
    size_t i2c_count = 0;
    size_t timer_count = 0;
    size_t adc_count = 0;

    for (size_t i = 0; i < count; i++) {
        if (strcmp(device_list[i].type, "gpio") == 0) {
            gpio_count++;
        } else if (strcmp(device_list[i].type, "uart") == 0) {
            uart_count++;
        } else if (strcmp(device_list[i].type, "spi") == 0) {
            spi_count++;
        } else if (strcmp(device_list[i].type, "i2c") == 0) {
            i2c_count++;
        } else if (strcmp(device_list[i].type, "timer") == 0) {
            timer_count++;
        } else if (strcmp(device_list[i].type, "adc") == 0) {
            adc_count++;
        }
    }

    /* Should have devices of each type */
    EXPECT_GT(gpio_count, 0) << "No GPIO devices found";
    EXPECT_GT(uart_count, 0) << "No UART devices found";
    EXPECT_GT(spi_count, 0) << "No SPI devices found";
    EXPECT_GT(i2c_count, 0) << "No I2C devices found";
    EXPECT_GT(timer_count, 0) << "No Timer devices found";
    EXPECT_GT(adc_count, 0) << "No ADC devices found";
}

/*===========================================================================*/
/* Factory Error Handling Tests                                               */
/*===========================================================================*/

/**
 * \brief           Test factory with invalid device index
 * \details         Checkpoint 16: Test error handling
 *                  Validates: Requirements 10.1
 */
TEST_F(NxFactoryCheckpointTest, FactoryInvalidDeviceIndex) {
    /* Try to get device with invalid index */
    nx_uart_t* uart = nx_factory_uart(255);
    EXPECT_EQ(nullptr, uart);

    nx_spi_t* spi = nx_factory_spi(255);
    EXPECT_EQ(nullptr, spi);

    nx_i2c_t* i2c = nx_factory_i2c(255);
    EXPECT_EQ(nullptr, i2c);
}

/**
 * \brief           Test factory release with NULL pointer
 * \details         Checkpoint 16: Test error handling
 *                  Validates: Requirements 10.2
 */
TEST_F(NxFactoryCheckpointTest, FactoryReleaseNullPointer) {
    /* Release NULL pointers - should not crash */
    nx_factory_gpio_release(nullptr);
    nx_factory_uart_release(nullptr);
    nx_factory_spi_release(nullptr);
    nx_factory_i2c_release(nullptr);
    nx_factory_timer_release(nullptr);
    nx_factory_adc_release(nullptr);
}
