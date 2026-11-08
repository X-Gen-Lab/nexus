/**
 * \file            stm32_basic_cold_test.cpp
 * \brief           Concrete cold GPIO, Flash and watchdog assembly contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f407_basic_cold.h"
#include "stm32f407_control_cold.h"
nx_stm32_system_t g_nx_stm32_system;
}
#include <gtest/gtest.h>
#include <sys/mman.h>

static bool privileged;
static bool in_isr;
static bool masked;
static bool refuse_restricted_speed;
static nx_time_us_t model_time;
class BasicCold : public testing::Test {
  protected:
    void* mapping = MAP_FAILED;
    nx_stm32_gpio_state_t gpio{};
    nx_stm32_gpio_config_t config{GPIOD, 0x1000U, 0x1000U, true, 0U, 0U};
    nx_stm32_flash_state_t flash{};
    nx_stm32_watchdog_state_t watchdog{};
    const nx_flash_sector_t sectors[8]{
        {0U, 0x4000U},        {0x4000U, 0x4000U},   {0x8000U, 0x4000U},
        {0xC000U, 0x4000U},   {0x10000U, 0x10000U}, {0x20000U, 0x20000U},
        {0x40000U, 0x20000U}, {0x60000U, 0x20000U}};
    const nx_flash_geometry_t geometry{FLASH_BASE, 0x80000U, 4U,
                                       sectors,    8U,       0U};
    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(0x40000000U), 0x80000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(0x40000000U));
        privileged = true;
        in_isr = false;
        masked = false;
        refuse_restricted_speed = false;
        model_time = 100U;
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = RCC;
    }
    void TearDown() override {
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(0, munmap(mapping, 0x80000U));
        }
    }
};

TEST_F(BasicCold, GPIORejectsUnknownBankAndUnauthorizedInitialBeforeEffects) {
    config.registers = reinterpret_cast<GPIO_TypeDef*>(0x40021010U);
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_gpio_prepare(&gpio, &config));
    config.registers = GPIOD;
    config.initial = 0x2000U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_gpio_prepare(&gpio, &config));
    EXPECT_EQ(0U, RCC->AHB1ENR);
    EXPECT_EQ(nullptr, gpio.registers);
}

TEST_F(BasicCold, GPIOBindsExactMaskAndRejectsLiveRebind) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32_gpio_prepare(&gpio, &config));
    EXPECT_EQ(GPIOD, gpio.registers);
    EXPECT_EQ(0x1000U, gpio.mask);
    EXPECT_TRUE(gpio.output);
    EXPECT_TRUE(gpio.initialized);
    EXPECT_EQ(0x1000U, GPIOD->BSRR);
    config.registers = GPIOB;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_gpio_prepare(&gpio, &config));
    EXPECT_EQ(GPIOD, gpio.registers);
    ASSERT_EQ(NX_SUCCESS, nx_stm32_gpio_stop(&gpio, 0x1000U));
    config.registers = GPIOD;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_gpio_prepare(&gpio, &config));
}

TEST_F(BasicCold, GPIOInputUsesSameAuthorizedBankWithoutOutputMode) {
    config.output = false;
    config.initial = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_gpio_prepare(&gpio, &config));
    EXPECT_FALSE(gpio.output);
    EXPECT_EQ(0U, GPIOD->MODER & (3U << 24U));
}

TEST_F(BasicCold, GPIOInputUsesExplicitPullWithoutChangingOtherPins) {
    config.output = false;
    config.initial = 0U;
    const uint32_t original = UINT32_C(0x12345678) & ~(3U << 24U);
    for (uint8_t pull : {uint8_t{1U}, uint8_t{2U}}) {
        gpio = {};
        GPIOD->PUPDR = original;
        config.pull = pull;
        ASSERT_EQ(NX_SUCCESS, nx_stm32_gpio_prepare(&gpio, &config));
        EXPECT_EQ(original | (uint32_t{pull} << 24U), GPIOD->PUPDR);
        EXPECT_EQ(0U, GPIOD->MODER & (3U << 24U));
    }
}

TEST_F(BasicCold, GPIOOpenDrainPreloadsReleaseBeforeOutputMode) {
    config.output_type = 1U;
    const uint32_t original = UINT32_C(0xA5A5) & ~config.mask;
    GPIOD->OTYPER = original;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_gpio_prepare(&gpio, &config));
    EXPECT_EQ(original | config.mask, GPIOD->OTYPER);
    EXPECT_EQ(config.mask, GPIOD->BSRR);
    EXPECT_EQ(1U << 24U, GPIOD->MODER & (3U << 24U));
}

TEST_F(BasicCold, GPIORejectsInvalidElectricalChoicesBeforeClockOrPinWrites) {
    const auto reject = [&] {
        EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_gpio_prepare(&gpio, &config));
        EXPECT_EQ(0U, RCC->AHB1ENR);
        EXPECT_EQ(0U, GPIOD->BSRR);
        EXPECT_EQ(0U, GPIOD->MODER);
        EXPECT_EQ(0U, GPIOD->PUPDR);
        EXPECT_EQ(0U, GPIOD->OTYPER);
        EXPECT_EQ(nullptr, gpio.registers);
    };
    config.pull = 3U;
    reject();
    config.pull = 0U;
    config.output_type = 2U;
    reject();
    config.output_type = 1U;
    config.output = false;
    reject();
}

TEST_F(BasicCold, FlashBindsPhysicalGeometryWithoutMMIOWrites) {
    FLASH->SR = FLASH_SR_EOP;
    FLASH->CR = FLASH_CR_LOCK;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_flash_prepare(&flash, &geometry, 3300U));
    EXPECT_EQ(FLASH, flash.registers);
    EXPECT_EQ(&geometry, flash.geometry);
    EXPECT_EQ(reinterpret_cast<volatile uint8_t*>(FLASH_BASE), flash.memory);
    EXPECT_EQ(FLASH_SR_EOP, FLASH->SR);
    EXPECT_EQ(FLASH_CR_LOCK, FLASH->CR);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_flash_close(&flash));
}

TEST_F(BasicCold, FlashRejectsMalformedGeometryBeforeBinding) {
    auto invalid = geometry;
    invalid.size -= 4U;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32_flash_prepare(&flash, &invalid, 3300U));
    EXPECT_EQ(nullptr, flash.registers);
    EXPECT_EQ(nullptr, flash.geometry);
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_stm32_flash_prepare(&flash, &geometry, 2600U));
    EXPECT_EQ(nullptr, flash.registers);
}

TEST_F(BasicCold, FlashCloseRetainsActiveOrPhysicallyBusyOwner) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32_flash_prepare(&flash, &geometry, 3300U));
    flash.active = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_flash_close(&flash));
    flash.active = false;
    FLASH->SR = FLASH_SR_BSY;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_flash_close(&flash));
    EXPECT_EQ(&geometry, flash.geometry);
    FLASH->SR = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_flash_close(&flash));
}

TEST_F(BasicCold, WatchdogBindingDoesNotStartOscillatorOrCounting) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32_watchdog_prepare(&watchdog, 1000000U));
    EXPECT_EQ(IWDG, watchdog.registers);
    EXPECT_EQ(FLASH, watchdog.flash);
    EXPECT_EQ(DBGMCU, watchdog.debug);
    EXPECT_EQ(RCC, watchdog.rcc);
    EXPECT_EQ(1000000U, watchdog.poll_limit);
    EXPECT_EQ(0U, RCC->CSR);
    EXPECT_EQ(0U, IWDG->KR);
    EXPECT_FALSE(watchdog.state.enabled);
}

TEST_F(BasicCold, WatchdogRebindCannotEraseRetainedOrIrreversibleState) {
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_watchdog_prepare(&watchdog, 0U));
    watchdog.preparation_retained = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_watchdog_prepare(&watchdog, 1000U));
    EXPECT_TRUE(watchdog.preparation_retained);
    watchdog.preparation_retained = false;
    watchdog.state.enabled = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_watchdog_prepare(&watchdog, 1000U));
    EXPECT_TRUE(watchdog.state.enabled);
}

TEST_F(BasicCold, AllPreparationsRejectWrongContextBeforeBindingOrMMIO) {
    privileged = false;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_stm32_gpio_prepare(&gpio, &config));
    EXPECT_EQ(NX_ERROR_CONTEXT,
              nx_stm32_flash_prepare(&flash, &geometry, 3300U));
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_stm32_watchdog_prepare(&watchdog, 1000U));
    EXPECT_EQ(nullptr, gpio.registers);
    EXPECT_EQ(nullptr, flash.registers);
    EXPECT_EQ(nullptr, watchdog.registers);
    EXPECT_EQ(0U, RCC->AHB1ENR);
}

extern "C" bool nx_arch_is_privileged(void) {
    return privileged;
}
extern "C" bool nx_arch_in_isr(void) {
    return in_isr;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return masked;
}
extern "C" void nx_arch_dsb(void) {
    if (refuse_restricted_speed) {
        GPIOC->OSPEEDR |= 3U << 26U;
    }
}

/** \brief External clock input; cold tests never claim physical timing. */
extern "C" nx_time_us_t nx_time_now_us(void) {
    return model_time;
}

TEST_F(BasicCold, AdcExternalRanksConfigureTheReviewedBanksOnly) {
    const uint8_t channels[]{15U, 9U, 3U, 10U, 8U, 0U};
    const uint8_t samples[]{7U, 6U, 5U, 4U, 3U, 2U};
    const nx_stm32_adc_config_t selected{channels, samples, 6U, 3300U, 0U};
    nx_stm32_adc_state_t adc{};
    RCC->AHB1ENR =
        RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;
    GPIOA->MODER = GPIOB->MODER = GPIOC->MODER = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_cold_begin(&adc, &selected));
    EXPECT_EQ(3U | (3U << 6U), GPIOA->MODER);
    EXPECT_EQ(3U | (3U << 2U), GPIOB->MODER);
    EXPECT_EQ(3U | (3U << 10U), GPIOC->MODER);
    EXPECT_EQ(ADC1, adc.registers);
    EXPECT_EQ(6U, adc.channel_count);
    EXPECT_EQ(channels, adc.channels);
    ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_cold_finish(&adc));
    EXPECT_EQ(0U, RCC->APB2ENR & RCC_APB2ENR_ADC1EN);
}

TEST_F(BasicCold, AdcAllowsSixteenRanksAndConsistentRepeatedChannels) {
    const uint8_t channels[]{0U, 1U, 2U,  3U,  4U,  5U,  6U,  7U,
                             8U, 9U, 10U, 11U, 12U, 13U, 14U, 15U};
    const uint8_t samples[16]{};
    const nx_stm32_adc_config_t selected{channels, samples, 16U, 3300U, 0U};
    nx_stm32_adc_state_t adc{};
    ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_cold_begin(&adc, &selected));
    EXPECT_EQ(0xffffU, GPIOA->MODER);
    EXPECT_EQ(0xfU, GPIOB->MODER);
    EXPECT_EQ(0xfffU, GPIOC->MODER);
    ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_cold_finish(&adc));
    adc = {};
    const uint8_t repeated[]{15U, 15U, 15U};
    const uint8_t repeated_samples[]{7U, 7U, 7U};
    const nx_stm32_adc_config_t repeat{repeated, repeated_samples, 3U, 3300U,
                                       0U};
    ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_cold_begin(&adc, &repeat));
    EXPECT_EQ(3U, adc.channel_count);
    ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_cold_finish(&adc));
}

TEST_F(BasicCold, AdcRejectsConflictingSampleTimesBeforeAcquiringClocks) {
    const uint8_t channels[]{0U, 0U};
    const uint8_t samples[]{1U, 7U};
    const nx_stm32_adc_config_t selected{channels, samples, 2U, 3300U, 0U};
    nx_stm32_adc_state_t adc{};
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32_adc_cold_begin(&adc, &selected));
    EXPECT_EQ(0U, RCC->APB2ENR);
    EXPECT_EQ(0U, adc.cold_effects);
    EXPECT_EQ(nullptr, adc.registers);
}

TEST_F(BasicCold, AdcRejectsInvalidResolutionBeforeAnyPadOrGateEffect) {
    const uint8_t channels[]{0U};
    const uint8_t samples[]{7U};
    nx_stm32_adc_config_t selected{channels, samples, 1U, 3300U, 0U};
    selected.resolution = 4U;
    nx_stm32_adc_state_t adc{};
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32_adc_cold_begin(&adc, &selected));
    EXPECT_EQ(0U, RCC->APB2ENR);
    EXPECT_EQ(0U, GPIOA->MODER);
    EXPECT_EQ(0U, adc.cold_effects);
    EXPECT_EQ(nullptr, adc.registers);
}

TEST_F(BasicCold, AdcColdConfigurationTransfersResolutionIntoTheEngine) {
    const uint8_t channels[]{0U};
    const uint8_t samples[]{7U};
    nx_stm32_adc_config_t selected{channels, samples, 1U, 3300U, 0U};
    selected.resolution = 3U;
    nx_stm32_adc_state_t adc{};
    ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_cold_begin(&adc, &selected));
    EXPECT_EQ(3U, adc.resolution);
    ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_cold_finish(&adc));
}

TEST_F(BasicCold, RestrictedGpioCOutputsUseLowSpeedAndPreserveOtherFields) {
    config.registers = GPIOC;
    config.mask = 0xe001U;
    config.initial = config.mask;
    GPIOC->OSPEEDR = 0xffffffffU;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_gpio_prepare(&gpio, &config));
    EXPECT_EQ(0x03ffffffU, GPIOC->OSPEEDR);
    EXPECT_TRUE(gpio.initialized);
}
TEST_F(BasicCold, RestrictedInputAndOrdinaryOutputsPreserveTheirSpeed) {
    config.registers = GPIOC;
    config.mask = 0x2000U;
    config.initial = 0U;
    config.output = false;
    GPIOC->OSPEEDR = 0xffffffffU;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_gpio_prepare(&gpio, &config));
    EXPECT_EQ(0xffffffffU, GPIOC->OSPEEDR);
    ASSERT_EQ(NX_SUCCESS, nx_stm32_gpio_stop(&gpio, 0U));
    config.output = true;
    config.registers = GPIOD;
    GPIOD->OSPEEDR = 0xffffffffU;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_gpio_prepare(&gpio, &config));
    EXPECT_EQ(0xffffffffU, GPIOD->OSPEEDR);
}
TEST_F(BasicCold, RefusedRestrictedSpeedNeverAdmitsOutputOrDropsItsClock) {
    config.registers = GPIOC;
    config.mask = config.initial = 0x2000U;
    GPIOC->OSPEEDR = 3U << 26U;
    refuse_restricted_speed = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_gpio_prepare(&gpio, &config));
    EXPECT_FALSE(gpio.initialized);
    EXPECT_EQ(0U, GPIOC->MODER & (3U << 26U));
    EXPECT_NE(0U, RCC->AHB1ENR & RCC_AHB1ENR_GPIOCEN);
}
