/**
 * \file            test_nx_platform.cpp
 * \brief           Native Platform Initialization and Resource Management Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-20
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Unit tests for platform initialization, device registration,
 *                  DMA channel management, and ISR management.
 *                  Requirements: 15.1-17.5
 */

#include <cstring>
#include <gtest/gtest.h>

extern "C" {
#include "hal/base/nx_device.h"
#include "hal/nx_factory.h"
#include "hal/nx_status.h"
#include "hal/resource/nx_dma_manager.h"
#include "hal/resource/nx_isr_manager.h"

/* Platform initialization functions */
nx_status_t nx_platform_init(void);
nx_status_t nx_platform_deinit(void);
bool nx_platform_is_initialized(void);

/* ISR simulation function */
void nx_isr_simulate(uint32_t irq);

/* Reset functions for peripherals */
void native_gpio_reset_all(void);
void native_uart_reset_all(void);
void native_spi_reset_all(void);
void native_i2c_reset_all(void);
}

/*---------------------------------------------------------------------------*/
/* Platform Initialization Tests - Requirements 15.1-15.4                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Platform Initialization Test Fixture
 */
class PlatformInitTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Ensure platform is deinitialized before each test */
        nx_platform_deinit();
    }

    void TearDown() override {
        /* Clean up after each test */
        nx_platform_deinit();
    }
};

TEST_F(PlatformInitTest, InitializeSuccess) {
    /* Platform should not be initialized initially */
    EXPECT_FALSE(nx_platform_is_initialized());

    /* Initialize platform */
    EXPECT_EQ(NX_OK, nx_platform_init());

    /* Platform should be initialized */
    EXPECT_TRUE(nx_platform_is_initialized());
}

TEST_F(PlatformInitTest, InitializeIdempotent) {
    /* Initialize platform */
    EXPECT_EQ(NX_OK, nx_platform_init());
    EXPECT_TRUE(nx_platform_is_initialized());

    /* Initialize again - should succeed (idempotent) */
    EXPECT_EQ(NX_OK, nx_platform_init());
    EXPECT_TRUE(nx_platform_is_initialized());
}

TEST_F(PlatformInitTest, DeinitializeSuccess) {
    /* Initialize platform */
    EXPECT_EQ(NX_OK, nx_platform_init());
    EXPECT_TRUE(nx_platform_is_initialized());

    /* Deinitialize platform */
    EXPECT_EQ(NX_OK, nx_platform_deinit());

    /* Platform should not be initialized */
    EXPECT_FALSE(nx_platform_is_initialized());
}

TEST_F(PlatformInitTest, DeinitializeIdempotent) {
    /* Initialize platform */
    EXPECT_EQ(NX_OK, nx_platform_init());

    /* Deinitialize platform */
    EXPECT_EQ(NX_OK, nx_platform_deinit());
    EXPECT_FALSE(nx_platform_is_initialized());

    /* Deinitialize again - should succeed (idempotent) */
    EXPECT_EQ(NX_OK, nx_platform_deinit());
    EXPECT_FALSE(nx_platform_is_initialized());
}

TEST_F(PlatformInitTest, InitDeinitCycle) {
    /* Test multiple init/deinit cycles */
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(NX_OK, nx_platform_init());
        EXPECT_TRUE(nx_platform_is_initialized());

        EXPECT_EQ(NX_OK, nx_platform_deinit());
        EXPECT_FALSE(nx_platform_is_initialized());
    }
}

/*---------------------------------------------------------------------------*/
/* Device Registration Tests - Requirements 15.2-15.3                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Device Registration Test Fixture
 */
class DeviceRegistrationTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Initialize platform */
        nx_platform_init();
    }

    void TearDown() override {
        /* Deinitialize platform */
        nx_platform_deinit();
    }
};

TEST_F(DeviceRegistrationTest, FindRegisteredDevice) {
    /* Find a known registered device (GPIO) */
    const nx_device_t* dev = nx_device_find("GPIOA0");
    EXPECT_NE(nullptr, dev);

    if (dev != nullptr) {
        EXPECT_STREQ("GPIOA0", dev->name);
    }
}

TEST_F(DeviceRegistrationTest, FindNonExistentDevice) {
    /* Try to find a device that doesn't exist */
    const nx_device_t* dev = nx_device_find("NONEXISTENT");
    EXPECT_EQ(nullptr, dev);
}

TEST_F(DeviceRegistrationTest, GetDeviceByName) {
    /* Get device by name (find + init) */
    void* api = nx_device_get("UART0");
    EXPECT_NE(nullptr, api);
}

TEST_F(DeviceRegistrationTest, GetMultipleDevices) {
    /* Get multiple different devices */
    void* uart0 = nx_device_get("UART0");
    void* spi0 = nx_device_get("SPI0");
    void* i2c0 = nx_device_get("I2C0");

    EXPECT_NE(nullptr, uart0);
    EXPECT_NE(nullptr, spi0);
    EXPECT_NE(nullptr, i2c0);

    /* Devices should be different */
    EXPECT_NE(uart0, spi0);
    EXPECT_NE(uart0, i2c0);
    EXPECT_NE(spi0, i2c0);
}

TEST_F(DeviceRegistrationTest, GetSameDeviceTwice) {
    /* Get same device twice - should return cached API */
    void* api1 = nx_device_get("UART0");
    void* api2 = nx_device_get("UART0");

    EXPECT_NE(nullptr, api1);
    EXPECT_EQ(api1, api2); /* Should be same cached pointer */
}

TEST_F(DeviceRegistrationTest, FactoryFunctionsWork) {
    /* Test factory functions work correctly */
    auto* gpio = nx_factory_gpio('A', 0);
    auto* uart = nx_factory_uart(0);
    auto* spi = nx_factory_spi(0);
    auto* i2c = nx_factory_i2c(0);

    EXPECT_NE(nullptr, gpio);
    EXPECT_NE(nullptr, uart);
    EXPECT_NE(nullptr, spi);
    EXPECT_NE(nullptr, i2c);
}

TEST_F(DeviceRegistrationTest, FactoryUnknownMaximumIndicesDoNotAliasDevices) {
    /* The widest uint8_t index must resolve its full name, never device zero. */
    const uint8_t unknown = UINT8_MAX;
    EXPECT_EQ(nullptr, nx_factory_gpio_read_write('A', unknown));
    EXPECT_EQ(nullptr, nx_factory_gpio_read('A', unknown));
    EXPECT_EQ(nullptr, nx_factory_gpio_write('A', unknown));
    EXPECT_EQ(nullptr, nx_factory_uart(unknown));
    EXPECT_EQ(nullptr, nx_factory_spi(unknown));
    EXPECT_EQ(nullptr, nx_factory_i2c(unknown));
    EXPECT_EQ(nullptr, nx_factory_timer(unknown));
    EXPECT_EQ(nullptr, nx_factory_timer_pwm(unknown));
    EXPECT_EQ(nullptr, nx_factory_timer_encoder(unknown));
    EXPECT_EQ(nullptr, nx_factory_adc(unknown));
    EXPECT_EQ(nullptr, nx_factory_flash(unknown));
    EXPECT_EQ(nullptr, nx_factory_can(unknown));
    EXPECT_EQ(nullptr, nx_factory_usb(unknown));
    EXPECT_EQ(nullptr, nx_factory_rtc(unknown));
    EXPECT_EQ(nullptr, nx_factory_watchdog(unknown));
    EXPECT_EQ(nullptr, nx_factory_dac(unknown));
    EXPECT_EQ(nullptr, nx_factory_sdio(unknown));
    EXPECT_EQ(nullptr, nx_factory_crc(unknown));
    EXPECT_EQ(nullptr, nx_factory_option_bytes(unknown));

    EXPECT_EQ(nx_device_get("UART0"), nx_factory_uart(0));
    EXPECT_EQ(nx_device_get("SPI0"), nx_factory_spi(0));
    EXPECT_EQ(nx_device_get("I2C0"), nx_factory_i2c(0));
}

/*---------------------------------------------------------------------------*/
/* DMA Channel Management Tests - Requirements 16.1-16.5                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           DMA Channel Management Test Fixture
 */
class DMAManagementTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Initialize platform */
        nx_platform_init();
    }

    void TearDown() override {
        /* Deinitialize platform */
        nx_platform_deinit();
    }
};

TEST_F(DMAManagementTest, AllocateChannel) {
    /* Allocate a DMA channel */
    nx_dma_channel_t* channel = nx_dma_allocate_channel(0, 0);
    EXPECT_NE(nullptr, channel);

    /* Release channel */
    if (channel != nullptr) {
        EXPECT_EQ(NX_OK, nx_dma_release_channel(channel));
    }
}

TEST_F(DMAManagementTest, AllocateMultipleChannels) {
    /* Allocate multiple channels */
    nx_dma_channel_t* ch0 = nx_dma_allocate_channel(0, 0);
    nx_dma_channel_t* ch1 = nx_dma_allocate_channel(0, 1);
    nx_dma_channel_t* ch2 = nx_dma_allocate_channel(0, 2);

    EXPECT_NE(nullptr, ch0);
    EXPECT_NE(nullptr, ch1);
    EXPECT_NE(nullptr, ch2);

    /* Channels should be different */
    EXPECT_NE(ch0, ch1);
    EXPECT_NE(ch0, ch2);
    EXPECT_NE(ch1, ch2);

    /* Release channels */
    if (ch0 != nullptr) {
        EXPECT_EQ(NX_OK, nx_dma_release_channel(ch0));
    }
    if (ch1 != nullptr) {
        EXPECT_EQ(NX_OK, nx_dma_release_channel(ch1));
    }
    if (ch2 != nullptr) {
        EXPECT_EQ(NX_OK, nx_dma_release_channel(ch2));
    }
}

TEST_F(DMAManagementTest, AllocateSameChannelTwiceFails) {
    /* Allocate a channel */
    nx_dma_channel_t* ch1 = nx_dma_allocate_channel(0, 0);
    EXPECT_NE(nullptr, ch1);

    /* Try to allocate same channel again - should fail */
    nx_dma_channel_t* ch2 = nx_dma_allocate_channel(0, 0);
    EXPECT_EQ(nullptr, ch2);

    /* Release channel */
    if (ch1 != nullptr) {
        EXPECT_EQ(NX_OK, nx_dma_release_channel(ch1));
    }
}

TEST_F(DMAManagementTest, ReleaseAndReallocate) {
    /* Allocate a channel */
    nx_dma_channel_t* ch1 = nx_dma_allocate_channel(0, 0);
    EXPECT_NE(nullptr, ch1);

    /* Release channel */
    EXPECT_EQ(NX_OK, nx_dma_release_channel(ch1));

    /* Allocate same channel again - should succeed */
    nx_dma_channel_t* ch2 = nx_dma_allocate_channel(0, 0);
    EXPECT_NE(nullptr, ch2);

    /* Release channel */
    if (ch2 != nullptr) {
        EXPECT_EQ(NX_OK, nx_dma_release_channel(ch2));
    }
}

TEST_F(DMAManagementTest, ConfigureChannel) {
    /* Allocate a channel */
    nx_dma_channel_t* channel = nx_dma_allocate_channel(0, 0);
    ASSERT_NE(nullptr, channel);

    /* Configure channel */
    uint8_t src_data[64];
    uint8_t dst_data[64];

    nx_dma_config_t config = {
        .src_addr = src_data,
        .dst_addr = dst_data,
        .size = sizeof(src_data),
        .src_inc = 1,
        .dst_inc = 1,
        .data_width = 1,
        .circular = false,
    };

    EXPECT_EQ(NX_OK, channel->configure(channel, &config));

    /* Release channel */
    EXPECT_EQ(NX_OK, nx_dma_release_channel(channel));
}

TEST_F(DMAManagementTest, StartAndStopTransfer) {
    /* Allocate and configure channel */
    nx_dma_channel_t* channel = nx_dma_allocate_channel(0, 0);
    ASSERT_NE(nullptr, channel);

    uint8_t src_data[64];
    uint8_t dst_data[64];

    nx_dma_config_t config = {
        .src_addr = src_data,
        .dst_addr = dst_data,
        .size = sizeof(src_data),
        .src_inc = 1,
        .dst_inc = 1,
        .data_width = 1,
        .circular = false,
    };

    EXPECT_EQ(NX_OK, channel->configure(channel, &config));

    /* Start transfer */
    EXPECT_EQ(NX_OK, channel->start(channel));

    /* For non-circular mode, transfer completes immediately */
    EXPECT_EQ(0U, channel->get_remaining(channel));

    /* Release channel */
    EXPECT_EQ(NX_OK, nx_dma_release_channel(channel));
}

TEST_F(DMAManagementTest, TransferCallback) {
    /* Allocate and configure channel */
    nx_dma_channel_t* channel = nx_dma_allocate_channel(0, 0);
    ASSERT_NE(nullptr, channel);

    uint8_t src_data[64];
    uint8_t dst_data[64];

    nx_dma_config_t config = {
        .src_addr = src_data,
        .dst_addr = dst_data,
        .size = sizeof(src_data),
        .src_inc = 1,
        .dst_inc = 1,
        .data_width = 1,
        .circular = false,
    };

    EXPECT_EQ(NX_OK, channel->configure(channel, &config));

    /* Set callback */
    bool callback_called = false;
    auto callback = [](void* user_data) {
        bool* flag = static_cast<bool*>(user_data);
        *flag = true;
    };

    EXPECT_EQ(NX_OK,
              channel->set_callback(channel, callback, &callback_called));

    /* Start transfer */
    EXPECT_EQ(NX_OK, channel->start(channel));

    /* Callback should have been called */
    EXPECT_TRUE(callback_called);

    /* Release channel */
    EXPECT_EQ(NX_OK, nx_dma_release_channel(channel));
}

TEST_F(DMAManagementTest, InvalidParameters) {
    /* Try to allocate with invalid DMA index */
    nx_dma_channel_t* ch1 = nx_dma_allocate_channel(255, 0);
    EXPECT_EQ(nullptr, ch1);

    /* Try to allocate with invalid channel number */
    nx_dma_channel_t* ch2 = nx_dma_allocate_channel(0, 255);
    EXPECT_EQ(nullptr, ch2);

    /* Try to release NULL channel */
    EXPECT_NE(NX_OK, nx_dma_release_channel(nullptr));
}

/*---------------------------------------------------------------------------*/
/* ISR Management Tests - Requirements 17.1-17.5                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           ISR Management Test Fixture
 */
class ISRManagementTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Initialize platform */
        nx_platform_init();

        /* Get ISR manager */
        isr_mgr = nx_isr_manager_get();
        ASSERT_NE(nullptr, isr_mgr);
    }

    void TearDown() override {
        for (uint32_t irq=0; irq<64; ++irq) (void)isr_mgr->disconnect(isr_mgr,irq);
        nx_platform_deinit();
    }

    nx_isr_manager_t* isr_mgr = nullptr;
};

TEST_F(ISRManagementTest, ConnectEnablesAndPreservesContext) {
    unsigned calls=0;
    auto handler=[](void* context) { ++*static_cast<unsigned*>(context); };
    ASSERT_EQ(NX_OK,isr_mgr->connect(isr_mgr,10,handler,&calls,5));
    nx_isr_simulate(10);
    EXPECT_EQ(1U,calls);
    ASSERT_EQ(NX_OK,isr_mgr->disconnect(isr_mgr,10));
    nx_isr_simulate(10);
    EXPECT_EQ(1U,calls);
}
TEST_F(ISRManagementTest, DuplicateOwnerRejected) {
    unsigned first=0,second=0;
    auto handler=[](void* context) { ++*static_cast<unsigned*>(context); };
    ASSERT_EQ(NX_OK,isr_mgr->connect(isr_mgr,10,handler,&first,5));
    EXPECT_EQ(NX_ERR_BUSY,isr_mgr->connect(isr_mgr,10,handler,&second,0));
    nx_isr_simulate(10);
    EXPECT_EQ(1U,first); EXPECT_EQ(0U,second);
    EXPECT_EQ(NX_OK,isr_mgr->disconnect(isr_mgr,10));
}
TEST_F(ISRManagementTest, IndependentIRQs) {
    unsigned first=0,second=0;
    auto handler=[](void* context) { ++*static_cast<unsigned*>(context); };
    ASSERT_EQ(NX_OK,isr_mgr->connect(isr_mgr,10,handler,&first,0));
    ASSERT_EQ(NX_OK,isr_mgr->connect(isr_mgr,11,handler,&second,15));
    nx_isr_simulate(10); nx_isr_simulate(11); nx_isr_simulate(11);
    EXPECT_EQ(1U,first); EXPECT_EQ(2U,second);
    EXPECT_EQ(NX_OK,isr_mgr->disconnect(isr_mgr,10));
    EXPECT_EQ(NX_OK,isr_mgr->disconnect(isr_mgr,11));
}
TEST_F(ISRManagementTest, DisconnectCannotReclaimRunningCallback) {
    nx_status_t observed=NX_OK;
    auto handler=[](void* context) {
        auto result=static_cast<nx_status_t*>(context);
        nx_isr_manager_t* manager=nx_isr_manager_get();
        *result=manager->disconnect(manager,10);
        nx_isr_simulate(10); // Recursion must not invoke the handler again.
    };
    ASSERT_EQ(NX_OK,isr_mgr->connect(isr_mgr,10,handler,&observed,5));
    nx_isr_simulate(10);
    EXPECT_EQ(NX_ERR_BUSY,observed);
    EXPECT_EQ(NX_OK,isr_mgr->disconnect(isr_mgr,10));
}
TEST_F(ISRManagementTest, InvalidParameters) {
    auto handler=[](void*) {};
    EXPECT_EQ(NX_ERR_NULL_PTR,isr_mgr->connect(isr_mgr,10,nullptr,nullptr,5));
    EXPECT_EQ(NX_ERR_INVALID_PARAM,isr_mgr->connect(isr_mgr,64,handler,nullptr,5));
    EXPECT_EQ(NX_ERR_INVALID_PARAM,isr_mgr->connect(isr_mgr,10,handler,nullptr,16));
    EXPECT_EQ(NX_ERR_INVALID_PARAM,isr_mgr->disconnect(isr_mgr,64));
    EXPECT_EQ(NX_ERR_NOT_FOUND,isr_mgr->disconnect(isr_mgr,10));
}
TEST_F(ISRManagementTest, ReconnectChangesContextOnlyAfterDisconnect) {
    unsigned first=0,second=0;
    auto handler=[](void* context) { ++*static_cast<unsigned*>(context); };
    for(unsigned i=0;i<100;++i) {
        ASSERT_EQ(NX_OK,isr_mgr->connect(isr_mgr,10,handler,&first,5));
        nx_isr_simulate(10);
        ASSERT_EQ(NX_OK,isr_mgr->disconnect(isr_mgr,10));
        ASSERT_EQ(NX_OK,isr_mgr->connect(isr_mgr,10,handler,&second,15));
        nx_isr_simulate(10);
        ASSERT_EQ(NX_OK,isr_mgr->disconnect(isr_mgr,10));
    }
    EXPECT_EQ(100U,first); EXPECT_EQ(100U,second);
}
