/**
 * \file            test_nx_integration.cpp
 * \brief           Integration Tests for Native Platform Peripherals
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-19
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Integration tests for peripheral interactions.
 *                  Requirements: 10.7
 */

#include <chrono>
#include <cstring>
#include <gtest/gtest.h>
#include <thread>

extern "C" {
#include "hal/interface/nx_flash.h"
#include "hal/interface/nx_gpio.h"
#include "hal/interface/nx_i2c.h"
#include "hal/interface/nx_rtc.h"
#include "hal/interface/nx_spi.h"
#include "hal/interface/nx_timer.h"
#include "hal/interface/nx_uart.h"
#include "hal/interface/nx_usb.h"
#include "hal/nx_factory.h"
#include "tests/hal/native/devices/native_flash_helpers.h"
#include "tests/hal/native/devices/native_gpio_helpers.h"
#include "tests/hal/native/devices/native_i2c_helpers.h"
#include "tests/hal/native/devices/native_rtc_helpers.h"
#include "tests/hal/native/devices/native_spi_helpers.h"
#include "tests/hal/native/devices/native_timer_helpers.h"
#include "tests/hal/native/devices/native_uart_helpers.h"
#include "tests/hal/native/devices/native_usb_helpers.h"
}

/*---------------------------------------------------------------------------*/
/* GPIO + Timer Integration Tests - Requirements 10.7                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           GPIO + Timer Integration Test Fixture
 */
class GPIOTimerIntegrationTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Reset all instances */
        native_gpio_reset_all();
        native_timer_reset_all();

        /* Get GPIO instance (Port A, Pin 0) */
        gpio = nx_factory_gpio('A', 0);
        ASSERT_NE(nullptr, gpio);

        /* Get Timer PWM instance */
        timer_pwm = nx_factory_timer_pwm(0);
        ASSERT_NE(nullptr, timer_pwm);

        /* Initialize GPIO as output */
        nx_lifecycle_t* gpio_lifecycle = gpio->get_lifecycle(gpio);
        ASSERT_NE(nullptr, gpio_lifecycle);
        ASSERT_EQ(NX_OK, gpio_lifecycle->init(gpio_lifecycle));

        /* Initialize Timer PWM */
        nx_lifecycle_t* timer_lifecycle = timer_pwm->get_lifecycle(timer_pwm);
        ASSERT_NE(nullptr, timer_lifecycle);
        ASSERT_EQ(NX_OK, timer_lifecycle->init(timer_lifecycle));
    }

    void TearDown() override {
        /* Deinitialize peripherals */
        if (gpio != nullptr) {
            nx_lifecycle_t* lifecycle = gpio->get_lifecycle(gpio);
            if (lifecycle != nullptr) {
                lifecycle->deinit(lifecycle);
            }
        }

        if (timer_pwm != nullptr) {
            nx_lifecycle_t* lifecycle = timer_pwm->get_lifecycle(timer_pwm);
            if (lifecycle != nullptr) {
                lifecycle->deinit(lifecycle);
            }
        }

        /* Reset all instances */
        native_gpio_reset_all();
        native_timer_reset_all();
    }

    nx_gpio_t* gpio = nullptr;
    nx_timer_pwm_t* timer_pwm = nullptr;
};

TEST_F(GPIOTimerIntegrationTest, PWMOutputOnGPIO) {
    /* Configure PWM on channel 0 */
    EXPECT_EQ(NX_OK, timer_pwm->init_channel(timer_pwm, 0, 1000, 5000));

    /* Start PWM */
    EXPECT_EQ(NX_OK, timer_pwm->start(timer_pwm, 0));

    /* Verify PWM is running */
    EXPECT_TRUE(native_pwm_is_running(0, 0));

    /* Verify PWM frequency */
    EXPECT_EQ(1000U, native_pwm_get_frequency(0, 0));

    /* Verify PWM duty cycle (50%) */
    EXPECT_EQ(5000U, native_pwm_get_duty_cycle(0, 0));

    /* Stop PWM */
    EXPECT_EQ(NX_OK, timer_pwm->stop(timer_pwm, 0));

    /* Verify PWM is stopped */
    EXPECT_FALSE(native_pwm_is_running(0, 0));
}

TEST_F(GPIOTimerIntegrationTest, PWMDutyCycleControl) {
    /* Initialize PWM with 50% duty cycle */
    EXPECT_EQ(NX_OK, timer_pwm->init_channel(timer_pwm, 0, 1000, 5000));
    EXPECT_EQ(NX_OK, timer_pwm->start(timer_pwm, 0));

    /* Verify initial duty cycle */
    EXPECT_EQ(5000U, native_pwm_get_duty_cycle(0, 0));

    /* Change duty cycle to 25% */
    EXPECT_EQ(NX_OK, timer_pwm->set_duty_cycle(timer_pwm, 0, 2500));
    EXPECT_EQ(2500U, native_pwm_get_duty_cycle(0, 0));

    /* Change duty cycle to 75% */
    EXPECT_EQ(NX_OK, timer_pwm->set_duty_cycle(timer_pwm, 0, 7500));
    EXPECT_EQ(7500U, native_pwm_get_duty_cycle(0, 0));

    /* Change duty cycle to 0% (off) */
    EXPECT_EQ(NX_OK, timer_pwm->set_duty_cycle(timer_pwm, 0, 0));
    EXPECT_EQ(0U, native_pwm_get_duty_cycle(0, 0));

    /* Change duty cycle to 100% (full on) */
    EXPECT_EQ(NX_OK, timer_pwm->set_duty_cycle(timer_pwm, 0, 10000));
    EXPECT_EQ(10000U, native_pwm_get_duty_cycle(0, 0));
}

TEST_F(GPIOTimerIntegrationTest, PWMFrequencyControl) {
    /* Initialize PWM with 1kHz frequency */
    EXPECT_EQ(NX_OK, timer_pwm->init_channel(timer_pwm, 0, 1000, 5000));
    EXPECT_EQ(NX_OK, timer_pwm->start(timer_pwm, 0));

    /* Verify initial frequency */
    EXPECT_EQ(1000U, native_pwm_get_frequency(0, 0));

    /* Change frequency to 500Hz */
    EXPECT_EQ(NX_OK, timer_pwm->set_frequency(timer_pwm, 0, 500));
    EXPECT_EQ(500U, native_pwm_get_frequency(0, 0));

    /* Change frequency to 2kHz */
    EXPECT_EQ(NX_OK, timer_pwm->set_frequency(timer_pwm, 0, 2000));
    EXPECT_EQ(2000U, native_pwm_get_frequency(0, 0));
}

TEST_F(GPIOTimerIntegrationTest, MultipleChannelPWM) {
    /* Initialize multiple PWM channels */
    EXPECT_EQ(NX_OK, timer_pwm->init_channel(timer_pwm, 0, 1000, 2500));
    EXPECT_EQ(NX_OK, timer_pwm->init_channel(timer_pwm, 1, 1000, 5000));
    EXPECT_EQ(NX_OK, timer_pwm->init_channel(timer_pwm, 2, 1000, 7500));

    /* Start all channels */
    EXPECT_EQ(NX_OK, timer_pwm->start(timer_pwm, 0));
    EXPECT_EQ(NX_OK, timer_pwm->start(timer_pwm, 1));
    EXPECT_EQ(NX_OK, timer_pwm->start(timer_pwm, 2));

    /* Verify all channels are running with correct duty cycles */
    EXPECT_TRUE(native_pwm_is_running(0, 0));
    EXPECT_TRUE(native_pwm_is_running(0, 1));
    EXPECT_TRUE(native_pwm_is_running(0, 2));

    EXPECT_EQ(2500U, native_pwm_get_duty_cycle(0, 0));
    EXPECT_EQ(5000U, native_pwm_get_duty_cycle(0, 1));
    EXPECT_EQ(7500U, native_pwm_get_duty_cycle(0, 2));

    /* Stop all channels */
    EXPECT_EQ(NX_OK, timer_pwm->stop(timer_pwm, 0));
    EXPECT_EQ(NX_OK, timer_pwm->stop(timer_pwm, 1));
    EXPECT_EQ(NX_OK, timer_pwm->stop(timer_pwm, 2));

    /* Verify all channels are stopped */
    EXPECT_FALSE(native_pwm_is_running(0, 0));
    EXPECT_FALSE(native_pwm_is_running(0, 1));
    EXPECT_FALSE(native_pwm_is_running(0, 2));
}

/*---------------------------------------------------------------------------*/
/* SPI + Flash Integration Tests - Requirements 10.7                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           SPI + Flash Integration Test Fixture
 */
class SPIFlashIntegrationTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Reset all instances */
        native_spi_reset_all();
        native_flash_reset_all();

        /* Get Flash instance */
        flash = nx_factory_flash(0);
        ASSERT_NE(nullptr, flash);

        /* Initialize Flash */
        nx_lifecycle_t* flash_lifecycle = flash->get_lifecycle(flash);
        ASSERT_NE(nullptr, flash_lifecycle);
        ASSERT_EQ(NX_OK, flash_lifecycle->init(flash_lifecycle));
    }

    void TearDown() override {
        /* Deinitialize Flash */
        if (flash != nullptr) {
            nx_lifecycle_t* lifecycle = flash->get_lifecycle(flash);
            if (lifecycle != nullptr) {
                lifecycle->deinit(lifecycle);
            }
        }

        /* Reset all instances */
        native_spi_reset_all();
        native_flash_reset_all();
    }

    nx_internal_flash_t* flash = nullptr;
};

TEST_F(SPIFlashIntegrationTest, FlashEraseWriteRead) {
    /* Erase a sector */
    uint32_t sector_addr = 0x1000;
    EXPECT_EQ(NX_OK, flash->erase(flash, sector_addr, 4096));

    /* Verify sector is erased */
    EXPECT_TRUE(native_flash_is_erased(0, sector_addr, 4096));

    /* Write data to flash */
    uint8_t write_data[256];
    for (size_t i = 0; i < sizeof(write_data); i++) {
        write_data[i] = (uint8_t)(i & 0xFF);
    }

    EXPECT_EQ(NX_OK,
              flash->write(flash, sector_addr, write_data, sizeof(write_data)));

    /* Read data back */
    uint8_t read_data[256];
    EXPECT_EQ(NX_OK,
              flash->read(flash, sector_addr, read_data, sizeof(read_data)));

    /* Verify data matches */
    EXPECT_EQ(0, memcmp(write_data, read_data, sizeof(write_data)));
}

TEST_F(SPIFlashIntegrationTest, FlashMultipleSectorOperations) {
    /* Erase multiple sectors */
    uint32_t sector1_addr = 0x0000;
    uint32_t sector2_addr = 0x1000;
    uint32_t sector3_addr = 0x2000;

    EXPECT_EQ(NX_OK, flash->erase(flash, sector1_addr, 4096));
    EXPECT_EQ(NX_OK, flash->erase(flash, sector2_addr, 4096));
    EXPECT_EQ(NX_OK, flash->erase(flash, sector3_addr, 4096));

    /* Write different patterns to each sector */
    uint8_t pattern1[128];
    uint8_t pattern2[128];
    uint8_t pattern3[128];

    for (size_t i = 0; i < 128; i++) {
        pattern1[i] = 0xAA;
        pattern2[i] = 0x55;
        pattern3[i] = (uint8_t)i;
    }

    EXPECT_EQ(NX_OK, flash->write(flash, sector1_addr, pattern1, 128));
    EXPECT_EQ(NX_OK, flash->write(flash, sector2_addr, pattern2, 128));
    EXPECT_EQ(NX_OK, flash->write(flash, sector3_addr, pattern3, 128));

    /* Read back and verify each sector */
    uint8_t read_buffer[128];

    EXPECT_EQ(NX_OK, flash->read(flash, sector1_addr, read_buffer, 128));
    EXPECT_EQ(0, memcmp(pattern1, read_buffer, 128));

    EXPECT_EQ(NX_OK, flash->read(flash, sector2_addr, read_buffer, 128));
    EXPECT_EQ(0, memcmp(pattern2, read_buffer, 128));

    EXPECT_EQ(NX_OK, flash->read(flash, sector3_addr, read_buffer, 128));
    EXPECT_EQ(0, memcmp(pattern3, read_buffer, 128));
}

TEST_F(SPIFlashIntegrationTest, FlashWriteWithoutEraseFails) {
    /* Write data without erasing first */
    uint8_t write_data[64];
    memset(write_data, 0xAA, sizeof(write_data));

    /* This should fail because sector is not erased */
    nx_status_t result =
        flash->write(flash, 0x1000, write_data, sizeof(write_data));

    /* Expect error (sector not erased) */
    EXPECT_NE(NX_OK, result);
}

TEST_F(SPIFlashIntegrationTest, FlashLargeDataTransfer) {
    /* Erase multiple sectors for large transfer */
    uint32_t start_addr = 0x0000;
    size_t total_size = 8192; /* 2 sectors */

    EXPECT_EQ(NX_OK, flash->erase(flash, start_addr, total_size));

    /* Write large data */
    uint8_t* large_write_data = new uint8_t[total_size];
    for (size_t i = 0; i < total_size; i++) {
        large_write_data[i] = (uint8_t)(i & 0xFF);
    }

    EXPECT_EQ(NX_OK,
              flash->write(flash, start_addr, large_write_data, total_size));

    /* Read back large data */
    uint8_t* large_read_data = new uint8_t[total_size];
    EXPECT_EQ(NX_OK,
              flash->read(flash, start_addr, large_read_data, total_size));

    /* Verify data matches */
    EXPECT_EQ(0, memcmp(large_write_data, large_read_data, total_size));

    delete[] large_write_data;
    delete[] large_read_data;
}

/*---------------------------------------------------------------------------*/
/* I2C + RTC Integration Tests - Requirements 10.7                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           I2C + RTC Integration Test Fixture
 */
class I2CRTCIntegrationTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Reset all instances */
        native_i2c_reset_all();
        native_rtc_reset_all();

        /* Get I2C instance */
        i2c = nx_factory_i2c(0);
        ASSERT_NE(nullptr, i2c);

        /* Get RTC instance */
        rtc = nx_factory_rtc(0);
        ASSERT_NE(nullptr, rtc);

        /* Initialize I2C */
        nx_lifecycle_t* i2c_lifecycle = i2c->get_lifecycle(i2c);
        ASSERT_NE(nullptr, i2c_lifecycle);
        ASSERT_EQ(NX_OK, i2c_lifecycle->init(i2c_lifecycle));

        /* Initialize RTC */
        nx_lifecycle_t* rtc_lifecycle = rtc->get_lifecycle(rtc);
        ASSERT_NE(nullptr, rtc_lifecycle);
        ASSERT_EQ(NX_OK, rtc_lifecycle->init(rtc_lifecycle));

        /* Add simulated I2C RTC device at address 0x68 (common RTC address) */
        ASSERT_TRUE(native_i2c_add_device(0, 0x68, true));
    }

    void TearDown() override {
        /* Deinitialize peripherals */
        if (i2c != nullptr) {
            nx_lifecycle_t* lifecycle = i2c->get_lifecycle(i2c);
            if (lifecycle != nullptr) {
                lifecycle->deinit(lifecycle);
            }
        }

        if (rtc != nullptr) {
            nx_lifecycle_t* lifecycle = rtc->get_lifecycle(rtc);
            if (lifecycle != nullptr) {
                lifecycle->deinit(lifecycle);
            }
        }

        /* Reset all instances */
        native_i2c_reset_all();
        native_rtc_reset_all();
    }

    nx_i2c_t* i2c = nullptr;
    nx_rtc_t* rtc = nullptr;
};

TEST_F(I2CRTCIntegrationTest, I2CRTCDeviceCommunication) {
    /* Simulate writing RTC time via I2C */
    uint16_t rtc_dev_addr = 0x68;
    uint16_t time_reg_addr = 0x00;

    /* RTC time data (BCD format: 14:30:45) */
    uint8_t time_data[3] = {0x45, 0x30, 0x14}; /* seconds, minutes, hours */

    /* Write time to I2C RTC device */
    EXPECT_TRUE(native_i2c_write_device_memory(0, rtc_dev_addr, time_reg_addr,
                                               time_data, sizeof(time_data)));

    /* Read time back from I2C RTC device */
    uint8_t read_time[3];
    EXPECT_TRUE(native_i2c_read_device_memory(0, rtc_dev_addr, time_reg_addr,
                                              read_time, sizeof(read_time)));

    /* Verify data matches */
    EXPECT_EQ(0, memcmp(time_data, read_time, sizeof(time_data)));
}

TEST_F(I2CRTCIntegrationTest, I2CRTCDateCommunication) {
    /* Simulate writing RTC date via I2C */
    uint16_t rtc_dev_addr = 0x68;
    uint16_t date_reg_addr = 0x04;

    /* RTC date data (BCD format: 2026-01-19) */
    uint8_t date_data[3] = {0x19, 0x01, 0x26}; /* day, month, year */

    /* Write date to I2C RTC device */
    EXPECT_TRUE(native_i2c_write_device_memory(0, rtc_dev_addr, date_reg_addr,
                                               date_data, sizeof(date_data)));

    /* Read date back from I2C RTC device */
    uint8_t read_date[3];
    EXPECT_TRUE(native_i2c_read_device_memory(0, rtc_dev_addr, date_reg_addr,
                                              read_date, sizeof(read_date)));

    /* Verify data matches */
    EXPECT_EQ(0, memcmp(date_data, read_date, sizeof(date_data)));
}

TEST_F(I2CRTCIntegrationTest, I2CDeviceReadyCheck) {
    /* Check if RTC device is ready */
    uint16_t rtc_dev_addr = 0x68;

    /* Device should be ready (we added it in SetUp) */
    EXPECT_EQ(NX_OK, i2c->is_device_ready(i2c, rtc_dev_addr, 1, 100));

    /* Set device to not ready */
    EXPECT_TRUE(native_i2c_set_device_ready(0, rtc_dev_addr, false));

    /* Device should not be ready now */
    EXPECT_NE(NX_OK, i2c->is_device_ready(i2c, rtc_dev_addr, 1, 100));

    /* Set device back to ready */
    EXPECT_TRUE(native_i2c_set_device_ready(0, rtc_dev_addr, true));

    /* Device should be ready again */
    EXPECT_EQ(NX_OK, i2c->is_device_ready(i2c, rtc_dev_addr, 1, 100));
}

TEST_F(I2CRTCIntegrationTest, I2CMultipleDevices) {
    /* Add multiple I2C devices (simulating multiple RTCs or sensors) */
    uint16_t rtc1_addr = 0x68;
    uint16_t rtc2_addr = 0x69;

    EXPECT_TRUE(native_i2c_add_device(0, rtc2_addr, true));

    /* Write different data to each device */
    uint8_t data1[4] = {0x11, 0x22, 0x33, 0x44};
    uint8_t data2[4] = {0xAA, 0xBB, 0xCC, 0xDD};

    EXPECT_TRUE(native_i2c_write_device_memory(0, rtc1_addr, 0x00, data1, 4));
    EXPECT_TRUE(native_i2c_write_device_memory(0, rtc2_addr, 0x00, data2, 4));

    /* Read back from each device */
    uint8_t read1[4];
    uint8_t read2[4];

    EXPECT_TRUE(native_i2c_read_device_memory(0, rtc1_addr, 0x00, read1, 4));
    EXPECT_TRUE(native_i2c_read_device_memory(0, rtc2_addr, 0x00, read2, 4));

    /* Verify data is correct for each device */
    EXPECT_EQ(0, memcmp(data1, read1, 4));
    EXPECT_EQ(0, memcmp(data2, read2, 4));
}

TEST_F(I2CRTCIntegrationTest, I2CRTCAlarmConfiguration) {
    /* Simulate writing RTC alarm configuration via I2C */
    uint16_t rtc_dev_addr = 0x68;
    uint16_t alarm_reg_addr = 0x07;

    /* Alarm time data (BCD format: 15:00:00) */
    uint8_t alarm_data[3] = {0x00, 0x00, 0x15}; /* seconds, minutes, hours */

    /* Write alarm configuration */
    EXPECT_TRUE(native_i2c_write_device_memory(0, rtc_dev_addr, alarm_reg_addr,
                                               alarm_data, sizeof(alarm_data)));

    /* Read alarm configuration back */
    uint8_t read_alarm[3];
    EXPECT_TRUE(native_i2c_read_device_memory(0, rtc_dev_addr, alarm_reg_addr,
                                              read_alarm, sizeof(read_alarm)));

    /* Verify alarm configuration matches */
    EXPECT_EQ(0, memcmp(alarm_data, read_alarm, sizeof(alarm_data)));
}

/*---------------------------------------------------------------------------*/
/* USB + UART Integration Tests - Requirements 10.7                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           USB + UART Integration Test Fixture
 */
class USBUARTIntegrationTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Reset all instances */
        native_usb_reset_all();
        native_uart_reset_all();

        /* Get USB instance */
        usb = nx_factory_usb(0);
        ASSERT_NE(nullptr, usb);

        /* Get UART instance */
        uart = nx_factory_uart(0);
        ASSERT_NE(nullptr, uart);

        /* Initialize USB */
        nx_lifecycle_t* usb_lifecycle = usb->get_lifecycle(usb);
        ASSERT_NE(nullptr, usb_lifecycle);
        ASSERT_EQ(NX_OK, usb_lifecycle->init(usb_lifecycle));

        /* Initialize UART */
        nx_lifecycle_t* uart_lifecycle = uart->get_lifecycle(uart);
        ASSERT_NE(nullptr, uart_lifecycle);
        ASSERT_EQ(NX_OK, uart_lifecycle->init(uart_lifecycle));

        /* Simulate USB connection */
        ASSERT_EQ(NX_OK, native_usb_simulate_connect(0));
    }

    void TearDown() override {
        /* Simulate USB disconnection */
        if (usb != nullptr) {
            native_usb_simulate_disconnect(0);
        }

        /* Deinitialize peripherals */
        if (usb != nullptr) {
            nx_lifecycle_t* lifecycle = usb->get_lifecycle(usb);
            if (lifecycle != nullptr) {
                lifecycle->deinit(lifecycle);
            }
        }

        if (uart != nullptr) {
            nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
            if (lifecycle != nullptr) {
                lifecycle->deinit(lifecycle);
            }
        }

        /* Reset all instances */
        native_usb_reset_all();
        native_uart_reset_all();
    }

    nx_usb_t* usb = nullptr;
    nx_uart_t* uart = nullptr;
};

TEST_F(USBUARTIntegrationTest, USBCDCDataTransfer) {
    /* Simulate USB CDC (virtual COM port) data transfer */

    /* Data to send from UART to USB */
    const char* uart_data = "Hello from UART!";
    size_t uart_data_len = strlen(uart_data);

    /* Inject data into UART RX buffer (simulating UART receiving data) */
    EXPECT_TRUE(native_uart_inject_rx_data(0, (const uint8_t*)uart_data,
                                           uart_data_len));

    /* Read data from UART (as if USB CDC is reading from UART) */
    uint8_t uart_rx_buffer[64];
    size_t bytes_read =
        uart->read(uart, uart_rx_buffer, sizeof(uart_rx_buffer), 100);

    EXPECT_EQ(uart_data_len, bytes_read);
    EXPECT_EQ(0, memcmp(uart_data, uart_rx_buffer, uart_data_len));

    /* Now simulate sending data from USB to UART */
    const char* usb_data = "Hello from USB!";
    size_t usb_data_len = strlen(usb_data);

    /* Inject data into USB RX buffer */
    EXPECT_EQ(NX_OK,
              native_usb_inject_rx(0, (const uint8_t*)usb_data, usb_data_len));

    /* Write data to UART (as if USB CDC is writing to UART) */
    size_t bytes_written =
        uart->write(uart, (const uint8_t*)usb_data, usb_data_len, 100);

    EXPECT_EQ(usb_data_len, bytes_written);

    /* Read back from UART TX buffer to verify */
    uint8_t uart_tx_buffer[64];
    size_t tx_bytes =
        native_uart_get_tx_data(0, uart_tx_buffer, sizeof(uart_tx_buffer));

    EXPECT_EQ(usb_data_len, tx_bytes);
    EXPECT_EQ(0, memcmp(usb_data, uart_tx_buffer, usb_data_len));
}

TEST_F(USBUARTIntegrationTest, USBCDCBidirectionalTransfer) {
    /* Test bidirectional data transfer between USB and UART */

    /* Send data from UART to USB */
    const char* uart_to_usb = "UART->USB";
    EXPECT_TRUE(native_uart_inject_rx_data(0, (const uint8_t*)uart_to_usb,
                                           strlen(uart_to_usb)));

    uint8_t buffer1[32];
    size_t read1 = uart->read(uart, buffer1, sizeof(buffer1), 100);
    EXPECT_EQ(strlen(uart_to_usb), read1);
    EXPECT_EQ(0, memcmp(uart_to_usb, buffer1, read1));

    /* Send data from USB to UART */
    const char* usb_to_uart = "USB->UART";
    EXPECT_EQ(NX_OK, native_usb_inject_rx(0, (const uint8_t*)usb_to_uart,
                                          strlen(usb_to_uart)));

    size_t written = uart->write(uart, (const uint8_t*)usb_to_uart,
                                 strlen(usb_to_uart), 100);
    EXPECT_EQ(strlen(usb_to_uart), written);

    uint8_t buffer2[32];
    size_t read2 = native_uart_get_tx_data(0, buffer2, sizeof(buffer2));
    EXPECT_EQ(strlen(usb_to_uart), read2);
    EXPECT_EQ(0, memcmp(usb_to_uart, buffer2, read2));
}

TEST_F(USBUARTIntegrationTest, USBConnectionEvents) {
    /* Test USB connection/disconnection events */

    /* USB should be connected (from SetUp) */
    bool initialized = false;
    bool suspended = false;
    EXPECT_EQ(NX_OK, native_usb_get_state(0, &initialized, &suspended));
    EXPECT_TRUE(initialized);

    /* Simulate disconnection */
    EXPECT_EQ(NX_OK, native_usb_simulate_disconnect(0));

    /* Simulate reconnection */
    EXPECT_EQ(NX_OK, native_usb_simulate_connect(0));

    /* USB should still be initialized */
    EXPECT_EQ(NX_OK, native_usb_get_state(0, &initialized, &suspended));
    EXPECT_TRUE(initialized);
}

TEST_F(USBUARTIntegrationTest, USBSuspendResume) {
    /* Test USB suspend/resume events */

    /* Simulate suspend */
    EXPECT_EQ(NX_OK, native_usb_simulate_suspend(0));

    bool initialized = false;
    bool suspended = false;
    EXPECT_EQ(NX_OK, native_usb_get_state(0, &initialized, &suspended));
    EXPECT_TRUE(suspended);

    /* Simulate resume */
    EXPECT_EQ(NX_OK, native_usb_simulate_resume(0));

    EXPECT_EQ(NX_OK, native_usb_get_state(0, &initialized, &suspended));
    EXPECT_FALSE(suspended);
}

TEST_F(USBUARTIntegrationTest, USBCDCLargeDataTransfer) {
    /* Test large data transfer through USB CDC */

    /* Create large data buffer (1KB) */
    uint8_t large_data[1024];
    for (size_t i = 0; i < sizeof(large_data); i++) {
        large_data[i] = (uint8_t)(i & 0xFF);
    }

    /* Inject large data into USB */
    EXPECT_EQ(NX_OK, native_usb_inject_rx(0, large_data, sizeof(large_data)));

    /* Write to UART in chunks */
    size_t total_written = 0;
    size_t chunk_size = 256;

    while (total_written < sizeof(large_data)) {
        size_t to_write = (sizeof(large_data) - total_written < chunk_size)
                              ? (sizeof(large_data) - total_written)
                              : chunk_size;

        size_t written =
            uart->write(uart, large_data + total_written, to_write, 100);
        EXPECT_EQ(to_write, written);
        total_written += written;
    }

    EXPECT_EQ(sizeof(large_data), total_written);

    /* Verify data in UART TX buffer */
    uint8_t verify_buffer[1024];
    size_t verified =
        native_uart_get_tx_data(0, verify_buffer, sizeof(verify_buffer));

    EXPECT_EQ(sizeof(large_data), verified);
    EXPECT_EQ(0, memcmp(large_data, verify_buffer, sizeof(large_data)));
}
