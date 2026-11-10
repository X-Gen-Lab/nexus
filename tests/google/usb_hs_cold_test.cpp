/**
 * \file            usb_hs_cold_test.cpp
 * \brief           Real HS device cold plan, partial pin rollback and drain
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/io/native/model.h"
#include "stm32f407_clock.h"
#include "stm32f407_usb_hs_cold.h"
#include "usb_device.h"
}
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>

using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;

class HsPads {
  public:
    MOCK_METHOD(nx_result_t, Clock, (unsigned));
    MOCK_METHOD(nx_result_t, Configure, (uint8_t));
    MOCK_METHOD(uint16_t, Capture, (uint8_t));
    MOCK_METHOD(void, Restore, (uint8_t, uint16_t));
    static HsPads* active;
};
HsPads* HsPads::active;
extern "C" nx_result_t nx_stm32_gpio_clock_enable(unsigned bank) {
    return HsPads::active->Clock(bank);
}
extern "C" nx_result_t nx_stm32_pin_configure(GPIO_TypeDef*, uint8_t pin,
                                              uint8_t, uint8_t, uint8_t, bool) {
    return HsPads::active->Configure(pin);
}
extern "C" uint16_t nx_stm32_pin_capture(const GPIO_TypeDef*, uint8_t pin) {
    return HsPads::active->Capture(pin);
}
extern "C" void nx_stm32_pin_restore(GPIO_TypeDef*, uint8_t pin,
                                     uint16_t saved) {
    HsPads::active->Restore(pin, saved);
}
extern "C" void nx_usb_clock_model_write(volatile uint32_t* reg,
                                         uint32_t value) {
    *reg = value;
}
extern "C" void nx_usb_clock_model_poll(void) {
}

class UsbHsCold : public testing::Test {
  protected:
    NiceMock<HsPads> pads;
    nx_usb_controller_t controller = {};
    nx_stm32_usb_hs_clock_state_t clock = {};
    nx_usb_endpoint_t endpoints[4] = {};
    nx_usb_event_t events[2] = {};
    uint16_t saved[2] = {};
    nx_stm32_platform_pin_config_t pins[2] = {{GPIOB, &saved[0], 1U, 14U},
                                              {GPIOB, &saved[1], 1U, 15U}};
    nx_stm32_usb_endpoint_config_t plan[4] = {
        {0U, true, NX_USB_PROFILE_CONTROL, 64U, 128U, 32U},
        {0U, false, NX_USB_PROFILE_CONTROL, 64U, 0U, 0U},
        {1U, true, NX_USB_PROFILE_BULK, 64U, 160U, 32U},
        {1U, false, NX_USB_PROFILE_BULK, 64U, 0U, 0U}};
    nx_stm32_usb_hs_cold_config_t config = {};
    nx_stm32_usb_hs_cold_receipt_t receipt = {};
    nx_usb_dma_storage_t staging[4] = {};
    nx_dma_memory_region_t region = {0x20000000U, 4096U,
                                     NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE};
    nx_native_usb_hardware_t* hardware = nullptr;
    void SetUp() override {
        ASSERT_NE(mmap(reinterpret_cast<void*>(0x40000000U), 0x80000U,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0),
                  MAP_FAILED);
        ASSERT_NE(mmap(reinterpret_cast<void*>(0x20000000U), 4096U,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0),
                  MAP_FAILED);
        for (size_t i = 0U; i < 4U; ++i)
            staging[i] = {reinterpret_cast<uint8_t*>(0x20000000U + i * 64U),
                          64U};
        hardware = reinterpret_cast<nx_native_usb_hardware_t*>(0x40040000U);
        hardware->endpoint_numbers = 6U;
        hardware->registers[0x10U / 4U] = 1U << 31U;
        HsPads::active = &pads;
        ON_CALL(pads, Clock(_)).WillByDefault(Invoke([](unsigned bank) {
            RCC->AHB1ENR |= 1U << bank;
            return NX_SUCCESS;
        }));
        ON_CALL(pads, Configure(_)).WillByDefault(Return(NX_SUCCESS));
        ON_CALL(pads, Capture(_)).WillByDefault(Invoke([](uint8_t pin) {
            return static_cast<uint16_t>(pin == 14U ? 0x123U : 0x456U);
        }));
        ASSERT_EQ(nx_native_clock_configure(true, 100U), NX_SUCCESS);
        RCC->CR = RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY;
        RCC->PLLCFGR = NEXUS_CLOCK_PLLCFGR;
        RCC->CFGR = NEXUS_CLOCK_CFGR | NEXUS_CLOCK_SW | (NEXUS_CLOCK_SW << 2U);
        config.clock = &clock;
        config.pins = pins;
        config.endpoints = endpoints;
        config.plan = plan;
        config.events = events;
        config.endpoint_count = 4U;
        config.capacity = 2U;
        config.irq_policy = {NX_IRQ_KERNEL_NONE, 82U, 4U, 0U};
        config.poll_limit = 4U;
        config.drain_timeout_us = 20U;
        config.clock_timeout_us = 1000U;
        config.rx_fifo_words = 128U;
        config.priority = 5U;
        config.pin_count = 2U;
        config.phy = NX_STM32_USB_HS_INTERNAL_FS;
    }
    void TearDown() override {
        hardware->hold_flush = hardware->hold_disable = false;
        if (receipt.config != nullptr) {
            EXPECT_EQ(
                nx_stm32_usb_hs_cold_close(&controller, &config, &receipt),
                NX_SUCCESS);
        }
        EXPECT_EQ(munmap(reinterpret_cast<void*>(0x40000000U), 0x80000U), 0);
        EXPECT_EQ(munmap(reinterpret_cast<void*>(0x20000000U), 4096U), 0);
    }
};

TEST_F(UsbHsCold, InvalidPacketPlanRejectsBeforeAnyClockOrPadEffect) {
    plan[2].max_packet = 65U;
    EXPECT_CALL(pads, Clock(_)).Times(0);
    EXPECT_CALL(pads, Configure(_)).Times(0);
    EXPECT_EQ(nx_stm32_usb_hs_cold_prepare(&controller, &config, &receipt),
              NX_ERROR_INVALID);
    EXPECT_EQ(receipt.config, nullptr);
    EXPECT_EQ(RCC->AHB1ENR, 0U);
}

TEST_F(UsbHsCold, PartialPadFailureRetainsExactOwnerAndRestoresInReverse) {
    EXPECT_CALL(pads, Configure(14U)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(pads, Configure(15U)).WillOnce(Return(NX_ERROR_IO));
    EXPECT_EQ(nx_stm32_usb_hs_cold_prepare(&controller, &config, &receipt),
              NX_ERROR_IO);
    EXPECT_EQ(receipt.config, &config);
    EXPECT_EQ(receipt.leases, 3U);
    EXPECT_EQ(clock.phase, 0U);
    auto foreign = config;
    EXPECT_EQ(nx_stm32_usb_hs_cold_close(&controller, &foreign, &receipt),
              NX_ERROR_BUSY);
    {
        testing::InSequence order;
        EXPECT_CALL(pads, Restore(15U, 0x456U));
        EXPECT_CALL(pads, Restore(14U, 0x123U));
    }
    EXPECT_EQ(nx_stm32_usb_hs_cold_close(&controller, &config, &receipt),
              NX_SUCCESS);
    EXPECT_EQ(receipt.leases, 0U);
    EXPECT_EQ(receipt.config, nullptr);
}

TEST_F(UsbHsCold, FailedCoreDrainKeepsClockConsumerAndAllPadsUntilRetry) {
    ASSERT_EQ(nx_stm32_usb_hs_cold_prepare(&controller, &config, &receipt),
              NX_SUCCESS);
    const nx_usb_activation_port_t activation = {&nx_usb_fs_activation_ops,
                                                 &controller};
    ASSERT_EQ(nx_usb_activation_set(&activation, controller.snapshot.epoch,
                                    0x81U, true),
              NX_SUCCESS);
    nx_usb_request_t request = {};
    uint8_t bytes[3] = {1U, 2U, 3U};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_usb_in_prepare(&request, bytes, sizeof(bytes), NX_USB_DATA, 0U,
                                200U),
              NX_SUCCESS);
    const nx_usb_endpoint_port_t endpoint = {&nx_usb_fs_endpoint_ops,
                                             &endpoints[2]};
    ASSERT_EQ(nx_usb_endpoint_submit(&endpoint, &request), NX_SUCCESS);
    hardware->hold_flush = true;
    EXPECT_CALL(pads, Restore(_, _)).Times(0);
    EXPECT_EQ(nx_stm32_usb_hs_cold_close(&controller, &config, &receipt),
              NX_ERROR_BUSY);
    EXPECT_EQ(clock.consumers, 1U);
    EXPECT_EQ(receipt.leases, 3U);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    testing::Mock::VerifyAndClearExpectations(&pads);
    hardware->hold_flush = false;
    EXPECT_EQ(nx_stm32_usb_hs_cold_close(&controller, &config, &receipt),
              NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(clock.consumers, 0U);
    EXPECT_EQ(clock.phase, 0U);
    EXPECT_EQ(receipt.config, nullptr);
}

TEST_F(UsbHsCold, DmaColdPlanBindsStaticStorageBeforeTransportAcquisition) {
    config.dma = true;
    config.dma_storage = staging;
    config.dma_regions = &region;
    config.dma_region_count = 1U;
    ASSERT_EQ(nx_stm32_usb_hs_cold_prepare(&controller, &config, &receipt),
              NX_SUCCESS);
    EXPECT_TRUE(controller.dma);
    EXPECT_EQ(controller.dma_storage, staging);
    EXPECT_EQ(controller.dma_regions, &region);
    EXPECT_EQ(hardware->registers[0xB14U / 4U], 0x20000040U);
    EXPECT_NE(hardware->registers[8U / 4U] & (1U << 5U), 0U);
}
