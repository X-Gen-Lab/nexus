/**
 * \file            single_spi_factory_test.cpp
 * \brief           Public single-SPI startup retains failed cold pin cleanup
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/arch/arch.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#ifdef NX_MULTI_FACTORY_STM32
#include "stm32f407_provider.h"
#define GPIO_GATE RCC->AHB1ENR
#define SPI_GATE  RCC->APB2ENR
#define SPI_BIT   RCC_APB2ENR_SPI1EN
#else
#include "gd32f470_provider.h"
#include "gd32f4xx_gpio.h"
#include "gd32f4xx_rcu.h"
#include "gd32f4xx_spi.h"
#define GPIO_GATE RCU_AHB1EN
#define SPI_GATE  RCU_APB2EN
#define SPI_BIT   RCU_APB2EN_SPI4EN
#endif
}
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>

using testing::_;
using testing::NiceMock;
class SingleSpiPorts {
  public:
    MOCK_METHOD(void, Restore, (unsigned, unsigned));
};
static SingleSpiPorts* s_ports;
static bool s_fail_pin_prepare;
static bool s_refuse_withdraw;
static unsigned s_system_stops;
#ifdef NX_MULTI_FACTORY_STM32
static constexpr unsigned FAILED_PORT = 0U;
static constexpr unsigned FAILED_PIN = 7U;
static constexpr unsigned FAILED_MODE = 2U;
#else
static constexpr unsigned FAILED_PORT = 5U;
static constexpr unsigned FAILED_PIN = 6U;
static constexpr unsigned FAILED_MODE = 1U;
#endif

/** \brief Access the common exact GPIO register layout in both real SDKs. */
static volatile uint32_t& gpio(unsigned port, unsigned offset) {
    return *reinterpret_cast<volatile uint32_t*>(UINT32_C(0x40020000) +
                                                 port * 0x400U + offset);
}
/** \brief Apply pending write-only BSRR effects at external MMIO observation.
 */
static void observe_gpio_commands(void) {
    for (unsigned port = 0U; port < 9U; ++port) {
        const uint32_t command = gpio(port, 24U);
        gpio(port, 20U) =
            (gpio(port, 20U) | (command & 0xffffU)) & ~(command >> 16U);
        gpio(port, 24U) = 0U;
    }
}
/** \brief Hardware refusal affects only the selected physical mode bits. */
extern "C" void nx_multi_factory_write(volatile uint32_t* reg, uint32_t value) {
    observe_gpio_commands();
    if (s_refuse_withdraw && reg == &gpio(FAILED_PORT, 0U)) {
        const uint32_t mask = 3U << (FAILED_PIN * 2U);
        value = (value & ~mask) | (*reg & mask);
    }
    *reg = value;
    observe_gpio_commands();
}
extern "C" bool nx_multi_factory_is_privileged(void) {
    return true;
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    return 100U;
}
/** \brief Model only the reset register leaf used by the real polling driver.
 */
extern "C" void nx_multi_factory_spi_reset(volatile uint32_t* reg,
                                           uint32_t mask, bool active) {
    *reg = active ? *reg | mask : *reg & ~mask;
}
/** \brief Snapshot only the selected twelve-bit pin state. */
static uint16_t capture_pin(unsigned port, unsigned pin) {
    observe_gpio_commands();
    const uint32_t shift = pin * 2U;
    return static_cast<uint16_t>(
        ((gpio(port, 0U) >> shift) & 3U) |
        (((gpio(port, 4U) >> pin) & 1U) << 2U) |
        (((gpio(port, 8U) >> shift) & 3U) << 3U) |
        (((gpio(port, 12U) >> shift) & 3U) << 5U) |
        (((gpio(port, 32U + pin / 8U * 4U) >> (pin % 8U * 4U)) & 15U) << 7U) |
        (((gpio(port, 20U) >> pin) & 1U) << 11U));
}
/** \brief Cold hardware port can fail after actually exposing a selected pin.
 */
static nx_result_t configure_pin(unsigned port, unsigned pin, uint8_t mode,
                                 uint8_t af, uint8_t pull, bool drain) {
    observe_gpio_commands();
    const uint32_t shift = pin * 2U;
    const uint32_t af_shift = pin % 8U * 4U;
    if (mode == 0U) {
        const uint32_t previous = (gpio(port, 0U) >> shift) & 3U;
        EXPECT_TRUE(previous == 0U || previous == 3U);
    }
    gpio(port, 32U + pin / 8U * 4U) =
        (gpio(port, 32U + pin / 8U * 4U) & ~(15U << af_shift)) |
        (static_cast<uint32_t>(af) << af_shift);
    gpio(port, 4U) = (gpio(port, 4U) & ~(1U << pin)) | (drain ? 1U << pin : 0U);
    gpio(port, 12U) = (gpio(port, 12U) & ~(3U << shift)) |
                      (static_cast<uint32_t>(pull) << shift);
    gpio(port, 0U) = (gpio(port, 0U) & ~(3U << shift)) |
                     (static_cast<uint32_t>(mode) << shift);
    return s_fail_pin_prepare && port == FAILED_PORT && pin == FAILED_PIN &&
                   mode == FAILED_MODE
               ? NX_ERROR_IO
               : NX_SUCCESS;
}
/** \brief Observe drive before reproducing the real latch-first restore port.
 */
static void restore_pin(unsigned port, unsigned pin, uint16_t saved) {
    observe_gpio_commands();
    s_ports->Restore(port, pin);
    const uint32_t shift = pin * 2U;
    const uint32_t af_shift = pin % 8U * 4U;
    gpio(port, 20U) = (gpio(port, 20U) & ~(1U << pin)) |
                      (((static_cast<uint32_t>(saved) >> 11U) & 1U) << pin);
    gpio(port, 32U + pin / 8U * 4U) =
        (gpio(port, 32U + pin / 8U * 4U) & ~(15U << af_shift)) |
        (((static_cast<uint32_t>(saved) >> 7U) & 15U) << af_shift);
    gpio(port, 4U) = (gpio(port, 4U) & ~(1U << pin)) |
                     (((static_cast<uint32_t>(saved) >> 2U) & 1U) << pin);
    gpio(port, 8U) = (gpio(port, 8U) & ~(3U << shift)) |
                     (((static_cast<uint32_t>(saved) >> 3U) & 3U) << shift);
    gpio(port, 12U) = (gpio(port, 12U) & ~(3U << shift)) |
                      (((static_cast<uint32_t>(saved) >> 5U) & 3U) << shift);
    gpio(port, 0U) = (gpio(port, 0U) & ~(3U << shift)) |
                     ((static_cast<uint32_t>(saved) & 3U) << shift);
}
#ifdef NX_MULTI_FACTORY_STM32
extern "C" {
nx_stm32_system_t g_nx_stm32_system;
}
extern "C" nx_result_t nx_stm32_validate_identity(uint32_t) {
    return NX_SUCCESS;
}
extern "C" nx_result_t nx_stm32_system_preflight(const nx_stm32_system_t*) {
    return NX_SUCCESS;
}
extern "C" nx_stm32_start_result_t nx_stm32_system_start(nx_stm32_system_t*,
                                                         uint32_t) {
    return {0, 0, 0U};
}
extern "C" int nx_stm32_system_stop(nx_stm32_system_t*, uint32_t) {
    ++s_system_stops;
    return 0;
}
extern "C" nx_result_t nx_stm32_gpio_clock_enable(unsigned port) {
    GPIO_GATE |= 1U << port;
    return NX_SUCCESS;
}
static unsigned port_index(const GPIO_TypeDef* port) {
    return static_cast<unsigned>(
        (reinterpret_cast<uintptr_t>(port) - UINT32_C(0x40020000)) / 0x400U);
}
extern "C" uint16_t nx_stm32_pin_capture(const GPIO_TypeDef* port,
                                         uint8_t pin) {
    return capture_pin(port_index(port), pin);
}
extern "C" void nx_stm32_pin_restore(GPIO_TypeDef* port, uint8_t pin,
                                     uint16_t state) {
    restore_pin(port_index(port), pin, state);
}
extern "C" nx_result_t nx_stm32_pin_configure(GPIO_TypeDef* port, uint8_t pin,
                                              uint8_t mode, uint8_t af,
                                              uint8_t pull, bool drain) {
    return configure_pin(port_index(port), pin, mode, af, pull, drain);
}
#else
extern "C" nx_result_t nx_gd32_soc_preflight(void) {
    return NX_SUCCESS;
}
extern "C" int nx_gd32_soc_start(void) {
    return 0;
}
extern "C" int nx_gd32_soc_stop(void) {
    ++s_system_stops;
    return 0;
}
extern "C" nx_result_t nx_gd32_gpio_clock_enable(unsigned port) {
    GPIO_GATE |= 1U << port;
    return NX_SUCCESS;
}
static unsigned port_index(uint32_t port) {
    return (port - UINT32_C(0x40020000)) / 0x400U;
}
extern "C" uint16_t nx_gd32_pin_capture(uint32_t port, unsigned pin) {
    return capture_pin(port_index(port), pin);
}
extern "C" void nx_gd32_pin_restore(uint32_t port, unsigned pin,
                                    uint16_t state) {
    restore_pin(port_index(port), pin, state);
}
extern "C" nx_result_t nx_gd32_pin_configure(uint32_t port, unsigned pin,
                                             uint8_t mode, uint8_t af,
                                             uint8_t pull, bool drain) {
    return configure_pin(port_index(port), pin, mode, af, pull, drain);
}
#endif

class SingleSpiFactory : public testing::Test {
  protected:
    NiceMock<SingleSpiPorts> ports;
    void* mapping = nullptr;
    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(UINT32_C(0x40000000)));
        s_ports = &ports;
        s_fail_pin_prepare = s_refuse_withdraw = false;
        s_system_stops = 0U;
#ifdef NX_MULTI_FACTORY_STM32
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = RCC;
        SPI1->SR = SPI_SR_TXE;
#else
        SPI_STAT(SPI4) = SPI_STAT_TBE;
#endif
    }
    void TearDown() override {
        s_fail_pin_prepare = s_refuse_withdraw = false;
        EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
        EXPECT_EQ(0, munmap(mapping, 0x80000U));
        s_ports = nullptr;
    }
};

TEST_F(SingleSpiFactory, SuccessfulFullStopRestoresTheOriginalSelectedPins) {
    ASSERT_EQ(NX_SUCCESS, nx_platform_start().primary);
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(0U, GPIO_GATE);
    EXPECT_EQ(0U, SPI_GATE & SPI_BIT);
    EXPECT_EQ(0U, capture_pin(FAILED_PORT, FAILED_PIN));
    EXPECT_EQ(NX_SUCCESS, nx_platform_start().primary);
}
TEST_F(SingleSpiFactory, ColdFailureWithdrawsDriveBeforeRestoringOldFields) {
    s_fail_pin_prepare = true;
    EXPECT_CALL(ports, Restore(_, _))
        .WillRepeatedly([](unsigned port, unsigned pin) {
            const uint32_t mode = (gpio(port, 0U) >> (pin * 2U)) & 3U;
            EXPECT_TRUE(mode == 0U || mode == 3U);
        });
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(0U, GPIO_GATE);
    EXPECT_EQ(0U, SPI_GATE & SPI_BIT);
    EXPECT_EQ(0U, capture_pin(FAILED_PORT, FAILED_PIN));
}
TEST_F(SingleSpiFactory, RefusedColdWithdrawalRetainsPinAndGateUntilRetry) {
    s_fail_pin_prepare = s_refuse_withdraw = true;
    GPIO_GATE = 1U << 2U;
    EXPECT_CALL(ports, Restore(_, _)).Times(testing::AnyNumber());
    EXPECT_CALL(ports, Restore(FAILED_PORT, FAILED_PIN)).Times(0);
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_EQ(NX_ERROR_IO, result.cleanup);
    EXPECT_NE(0U, result.remaining_effects);
    EXPECT_EQ(FAILED_MODE, (gpio(FAILED_PORT, 0U) >> (FAILED_PIN * 2U)) & 3U);
#ifndef NX_MULTI_FACTORY_STM32
    EXPECT_NE(0U, gpio(FAILED_PORT, 20U) & (1U << FAILED_PIN));
#endif
    EXPECT_NE(0U, GPIO_GATE & (1U << FAILED_PORT));
    EXPECT_EQ(0U, SPI_GATE & SPI_BIT);
    EXPECT_EQ(0U, s_system_stops);
    EXPECT_EQ(NX_ERROR_STATE, nx_platform_start().primary);
    EXPECT_EQ(NX_ERROR_IO, nx_platform_stop());
    (void)testing::Mock::VerifyAndClearExpectations(&ports);
    s_fail_pin_prepare = s_refuse_withdraw = false;
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(0U, capture_pin(FAILED_PORT, FAILED_PIN));
    EXPECT_EQ(1U << 2U, GPIO_GATE);
    EXPECT_EQ(1U, s_system_stops);
    EXPECT_EQ(NX_SUCCESS, nx_platform_start().primary);
}
