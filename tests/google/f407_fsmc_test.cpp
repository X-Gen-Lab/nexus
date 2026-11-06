/**
 * \file            f407_fsmc_test.cpp
 * \brief           Real FSMC LL configuration, partial cleanup and bank loans
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/components/ili9341.h"
#include "nexus/io/external_bus.h"
#include "nexus/io/memory_window.h"
#include "nexus/io/parallel16.h"
#include "stm32f407_fsmc.h"
}
#include <algorithm>
#include <array>
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <vector>

namespace {
bool privileged;
bool in_isr;
bool masked;
nx_time_us_t model_time;
bool refuse_gate_release;
const volatile uint32_t* bad_register;
uint32_t bad_when_value;
uint32_t bad_xor;
unsigned word_time;
std::vector<uint16_t> read_words;
size_t read_position;
std::vector<std::pair<uintptr_t, uint16_t>> writes;
nx_stm32_fsmc_lease_t* release_during_write;
nx_result_t reentry_result;
struct BarrierObservation {
    uint32_t sram_control;
    uint32_t lcd_control;
    uint32_t gate;
    bool lease_held;
};
std::vector<BarrierObservation> barriers;
nx_stm32_fsmc_lease_t* watched_lease;

/** \brief Fault only selected observation boundaries; LL writes stay real. */
uint32_t observed(const volatile uint32_t* reg) {
    uint32_t value = *reg;
    if (refuse_gate_release && reg == &RCC->AHB3ENR)
        value |= RCC_AHB3ENR_FSMCEN;
    if (reg == bad_register && value == bad_when_value)
        value ^= bad_xor;
    return value;
}

/** \brief Exact modeled board pins, with one Platform-owned snapshot each. */
class FSMC : public testing::Test {
  protected:
    std::array<void*, 3> mapping{MAP_FAILED, MAP_FAILED, MAP_FAILED};
    nx_stm32_fsmc_t bus{};
    std::array<nx_stm32_fsmc_bank_state_t, 2> states{};
    std::array<uint16_t, 41> snapshots{};
    std::vector<nx_stm32_platform_pin_config_t> pins;
    std::array<nx_stm32_fsmc_bank_config_t, 2> banks{
        nx_stm32_fsmc_bank_config_t{
            NX_STM32_FSMC_SRAM_NE3, {2U, 1U, 8U, 1U}, {1U, 1U, 5U, 1U}},
        nx_stm32_fsmc_bank_config_t{
            NX_STM32_FSMC_LCD_NE4, {4U, 1U, 20U, 2U}, {2U, 1U, 8U, 1U}}};
    nx_stm32_fsmc_config_t config{};
    static nx_time_us_t clock(void*) {
        return model_time;
    }
    void SetUp() override {
        const std::array<uintptr_t, 3> bases{0x40000000U, 0x60000000U,
                                             0x6c000000U};
        const std::array<size_t, 3> sizes{0x80000U, 0x1000U, 0x4000U};
        for (size_t i = 0U; i < mapping.size(); ++i) {
            mapping[i] =
                mmap(reinterpret_cast<void*>(bases[i]), sizes[i],
                     PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
            ASSERT_EQ(mapping[i], reinterpret_cast<void*>(bases[i]));
        }
        privileged = true;
        in_isr = false;
        masked = false;
        model_time = 0U;
        refuse_gate_release = false;
        bad_register = nullptr;
        bad_when_value = 0U;
        bad_xor = 0U;
        word_time = 0U;
        read_words.clear();
        read_position = 0U;
        writes.clear();
        release_during_write = nullptr;
        reentry_result = NX_SUCCESS;
        barriers.clear();
        watched_lease = nullptr;
        FSMC_Bank1->BTCR[0] = 0x30dbU;
        for (size_t i = 1U; i < 4U; ++i)
            FSMC_Bank1->BTCR[i * 2U] = 0x30d2U;
        RCC->AHB1ENR = RCC_AHB1ENR_GPIODEN | RCC_AHB1ENR_GPIOEEN |
                       RCC_AHB1ENR_GPIOFEN | RCC_AHB1ENR_GPIOGEN;
        pins.reserve(snapshots.size());
        const std::array<uint16_t, 4> masks{0xff33U, 0xff83U, 0xf03fU, 0x143fU};
        const std::array<GPIO_TypeDef*, 4> ports{GPIOD, GPIOE, GPIOF, GPIOG};
        for (size_t p = 0U; p < ports.size(); ++p) {
            for (uint8_t pin = 0U; pin < 16U; ++pin) {
                if ((masks[p] & (1U << pin)) != 0U) {
                    pins.push_back({ports[p], &snapshots[pins.size()],
                                    static_cast<uint8_t>(3U + p), pin});
                }
            }
        }
        config = {pins.data(),  pins.size(),      banks.data(),
                  banks.size(), {clock, nullptr}, false};
    }
    void TearDown() override {
        const std::array<size_t, 3> sizes{0x80000U, 0x1000U, 0x4000U};
        for (size_t i = 0U; i < mapping.size(); ++i) {
            if (mapping[i] != MAP_FAILED) {
                EXPECT_EQ(0, munmap(mapping[i], sizes[i]));
            }
        }
    }
    nx_result_t prepare() {
        return nx_stm32_fsmc_prepare(&bus, &config, states.data(),
                                     states.size());
    }
};

TEST_F(FSMC, MalformedOrBootBackedConfigurationHasNoHardwareEffect) {
    config.boot_backed = true;
    EXPECT_EQ(prepare(), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(RCC->AHB3ENR, 0U);
    config.boot_backed = false;
    banks[0].read.data_setup = 0U;
    EXPECT_EQ(prepare(), NX_ERROR_INVALID);
    EXPECT_EQ(RCC->AHB3ENR, 0U);
    EXPECT_EQ(bus.config, nullptr);
}

TEST_F(FSMC, WrongContextOrUnpoweredPadsRejectBeforeGateChanges) {
    privileged = false;
    EXPECT_EQ(prepare(), NX_ERROR_CONTEXT);
    privileged = true;
    RCC->AHB1ENR &= ~uint32_t{RCC_AHB1ENR_GPIOFEN};
    EXPECT_EQ(prepare(), NX_ERROR_STATE);
    EXPECT_EQ(RCC->AHB3ENR, 0U);
}

TEST_F(FSMC, MissingPadOrDuplicateBankIsNotAReviewedRoute) {
    --config.pin_count;
    EXPECT_EQ(prepare(), NX_ERROR_INVALID);
    ++config.pin_count;
    banks[1].kind = NX_STM32_FSMC_SRAM_NE3;
    EXPECT_EQ(prepare(), NX_ERROR_INVALID);
    EXPECT_EQ(RCC->AHB3ENR, 0U);
}

TEST_F(FSMC, OneSharedOwnerProgramsDistinctBankTimingsAndRestoresThem) {
    const uint32_t initial3 = FSMC_Bank1->BTCR[4];
    const uint32_t initial4 = FSMC_Bank1->BTCR[6];
    ASSERT_EQ(prepare(), NX_SUCCESS);
    EXPECT_NE(RCC->AHB3ENR & RCC_AHB3ENR_FSMCEN, 0U);
    EXPECT_EQ(FSMC_Bank1->BTCR[0], 0x30dbU);
    EXPECT_NE(FSMC_Bank1->BTCR[4] & FSMC_BCR1_MBKEN, 0U);
    EXPECT_NE(FSMC_Bank1->BTCR[6] & FSMC_BCR1_MBKEN, 0U);
    EXPECT_EQ((FSMC_Bank1->BTCR[5] >> 8U) & 0xffU, 8U);
    EXPECT_EQ((FSMC_Bank1->BTCR[7] >> 8U) & 0xffU, 20U);
    EXPECT_EQ((FSMC_Bank1E->BWTR[4] >> 8U) & 0xffU, 5U);
    EXPECT_EQ((GPIOG->AFR[1] >> 16U) & 15U, 12U);
    EXPECT_EQ(nx_stm32_fsmc_stop(&bus), NX_SUCCESS);
    EXPECT_EQ(FSMC_Bank1->BTCR[4], initial3);
    EXPECT_EQ(FSMC_Bank1->BTCR[6], initial4);
    EXPECT_EQ(RCC->AHB3ENR & RCC_AHB3ENR_FSMCEN, 0U);
    EXPECT_EQ(bus.config, nullptr);
    EXPECT_EQ((GPIOG->AFR[1] >> 16U) & 15U, 12U);
}

TEST_F(FSMC, ExistingGateRemainsOwnedByItsOriginalOwner) {
    RCC->AHB3ENR = RCC_AHB3ENR_FSMCEN;
    FSMC_Bank1->BTCR[0] &= ~uint32_t{FSMC_BCR1_MBKEN};
    ASSERT_EQ(prepare(), NX_SUCCESS);
    EXPECT_FALSE(bus.added_gate);
    ASSERT_EQ(nx_stm32_fsmc_stop(&bus), NX_SUCCESS);
    EXPECT_NE(RCC->AHB3ENR & RCC_AHB3ENR_FSMCEN, 0U);
}

TEST_F(FSMC, ActiveSelectedBankIsRejectedAndOriginalClockStateRestored) {
    FSMC_Bank1->BTCR[4] |= FSMC_BCR1_MBKEN;
    EXPECT_EQ(prepare(), NX_ERROR_BUSY);
    EXPECT_EQ(RCC->AHB3ENR, 0U);
    EXPECT_EQ(bus.config, nullptr);
    EXPECT_NE(FSMC_Bank1->BTCR[4] & FSMC_BCR1_MBKEN, 0U);
}

TEST_F(FSMC, ActiveForeignBankWithExistingGateIsNotSharedImplicitly) {
    RCC->AHB3ENR = RCC_AHB3ENR_FSMCEN;
    EXPECT_EQ(prepare(), NX_ERROR_BUSY);
    EXPECT_EQ(bus.config, nullptr);
    EXPECT_EQ(FSMC_Bank1->BTCR[0], 0x30dbU);
}

TEST_F(FSMC, BorrowedSRAMPreventsStopWithoutAnImplicitLifetimeTimeout) {
    ASSERT_EQ(prepare(), NX_SUCCESS);
    nx_stm32_fsmc_lease_t lease{};
    ASSERT_EQ(nx_stm32_fsmc_sram_acquire(&bus, &lease), NX_SUCCESS);
    EXPECT_EQ(lease.address, 0x68000000U);
    EXPECT_EQ(lease.bytes, 0x100000U);
    const uint32_t live = FSMC_Bank1->BTCR[4];
    model_time = UINT64_MAX - 1U;
    EXPECT_EQ(nx_stm32_fsmc_stop(&bus), NX_ERROR_BUSY);
    EXPECT_EQ(FSMC_Bank1->BTCR[4], live);
    ASSERT_EQ(nx_stm32_fsmc_release(&lease, lease.epoch), NX_SUCCESS);
    EXPECT_EQ(nx_stm32_fsmc_stop(&bus), NX_SUCCESS);
}

TEST_F(FSMC, CopiesAndStaleEpochsCannotReturnAnActiveWindow) {
    ASSERT_EQ(prepare(), NX_SUCCESS);
    nx_stm32_fsmc_lease_t lease{};
    ASSERT_EQ(nx_stm32_fsmc_sram_acquire(&bus, &lease), NX_SUCCESS);
    auto copy = lease;
    EXPECT_EQ(nx_stm32_fsmc_release(&copy, copy.epoch), NX_ERROR_INVALID);
    const uint64_t old = lease.epoch;
    ASSERT_EQ(nx_stm32_fsmc_release(&lease, old), NX_SUCCESS);
    ASSERT_EQ(nx_stm32_fsmc_sram_acquire(&bus, &lease), NX_SUCCESS);
    EXPECT_NE(lease.epoch, old);
    EXPECT_EQ(nx_stm32_fsmc_release(&lease, old), NX_ERROR_STATE);
    EXPECT_EQ(nx_stm32_fsmc_stop(&bus), NX_ERROR_BUSY);
    EXPECT_EQ(nx_stm32_fsmc_release(&lease, lease.epoch), NX_SUCCESS);
}

TEST_F(FSMC, GateWithdrawalFailureRetainsReceiptForObservedRetry) {
    ASSERT_EQ(prepare(), NX_SUCCESS);
    refuse_gate_release = true;
    EXPECT_EQ(nx_stm32_fsmc_stop(&bus), NX_ERROR_IO);
    EXPECT_TRUE(bus.added_gate);
    EXPECT_NE(bus.config, nullptr);
    refuse_gate_release = false;
    EXPECT_EQ(nx_stm32_fsmc_stop(&bus), NX_SUCCESS);
    EXPECT_FALSE(bus.added_gate);
    EXPECT_EQ(bus.config, nullptr);
}

TEST_F(FSMC, FailedBankDisableRetainsBankAndGateWithoutClaimingCleanStop) {
    ASSERT_EQ(prepare(), NX_SUCCESS);
    bad_register = &FSMC_Bank1->BTCR[4];
    bad_when_value = FSMC_Bank1->BTCR[4] & ~uint32_t{FSMC_BCR1_MBKEN};
    bad_xor = FSMC_BCR1_MBKEN;
    EXPECT_EQ(nx_stm32_fsmc_stop(&bus), NX_ERROR_IO);
    EXPECT_TRUE(states[0].captured);
    EXPECT_NE(RCC->AHB3ENR & RCC_AHB3ENR_FSMCEN, 0U);
    bad_register = nullptr;
    EXPECT_EQ(nx_stm32_fsmc_stop(&bus), NX_SUCCESS);
}

TEST_F(FSMC, LCDPortUsesA12OnASixteenBitBusWithoutRetainingSpans) {
    ASSERT_EQ(prepare(), NX_SUCCESS);
    nx_stm32_fsmc_lcd_t lcd{};
    const nx_parallel16_endpoint_t port{&nx_stm32_fsmc_parallel16_ops, &lcd};
    ASSERT_EQ(nx_stm32_fsmc_lcd_prepare(&bus, &lcd), NX_SUCCESS);
    EXPECT_EQ(port.ops->command(port.context, 0x2cU, 100U), NX_SUCCESS);
    const std::array<uint16_t, 3> pixels{0xf800U, 0x07e0U, 0x001fU};
    size_t done = 99U;
    EXPECT_EQ(port.ops->write(port.context, pixels.data(), pixels.size(), &done,
                              100U),
              NX_SUCCESS);
    EXPECT_EQ(done, pixels.size());
    ASSERT_EQ(writes.size(), 4U);
    EXPECT_EQ(writes[0],
              std::make_pair(uintptr_t{0x6c000000U}, uint16_t{0x2cU}));
    EXPECT_EQ(writes[1],
              std::make_pair(uintptr_t{0x6c002000U}, uint16_t{0xf800U}));
    EXPECT_EQ(nx_stm32_fsmc_stop(&bus), NX_ERROR_BUSY);
    EXPECT_EQ(nx_stm32_fsmc_release(&lcd.lease, lcd.lease.epoch), NX_SUCCESS);
    EXPECT_EQ(port.ops->command(port.context, 0x2cU, 100U), NX_ERROR_STATE);
    EXPECT_EQ(nx_stm32_fsmc_stop(&bus), NX_SUCCESS);
}

TEST_F(FSMC, LCDDeadlineHasAnExactBoundedWordPrefixAndNoOuterBorrow) {
    ASSERT_EQ(prepare(), NX_SUCCESS);
    nx_stm32_fsmc_lcd_t lcd{};
    const nx_parallel16_endpoint_t port{&nx_stm32_fsmc_parallel16_ops, &lcd};
    ASSERT_EQ(nx_stm32_fsmc_lcd_prepare(&bus, &lcd), NX_SUCCESS);
    std::array<uint16_t, 64> pixels{};
    word_time = 1U;
    size_t done{};
    EXPECT_EQ(
        port.ops->write(port.context, pixels.data(), pixels.size(), &done, 10U),
        NX_ERROR_TIMEOUT);
    EXPECT_EQ(done, 32U);
    EXPECT_EQ(writes.size(), done);
    EXPECT_FALSE(lcd.in_call);
    EXPECT_EQ(nx_stm32_fsmc_release(&lcd.lease, lcd.lease.epoch), NX_SUCCESS);
}

TEST_F(FSMC, LCDCannotReleaseItsBankFromInsideATransportAccess) {
    ASSERT_EQ(prepare(), NX_SUCCESS);
    nx_stm32_fsmc_lcd_t lcd{};
    const nx_parallel16_endpoint_t port{&nx_stm32_fsmc_parallel16_ops, &lcd};
    ASSERT_EQ(nx_stm32_fsmc_lcd_prepare(&bus, &lcd), NX_SUCCESS);
    release_during_write = &lcd.lease;
    EXPECT_EQ(port.ops->command(port.context, 0x2cU, 100U), NX_SUCCESS);
    EXPECT_EQ(reentry_result, NX_ERROR_BUSY);
    EXPECT_EQ(nx_stm32_fsmc_stop(&bus), NX_ERROR_BUSY);
    EXPECT_EQ(nx_stm32_fsmc_release(&lcd.lease, lcd.lease.epoch), NX_SUCCESS);
}

TEST_F(FSMC, ExhaustedEpochNeverWrapsIntoAReusableLease) {
    ASSERT_EQ(prepare(), NX_SUCCESS);
    bus.epoch = UINT64_MAX;
    nx_stm32_fsmc_lease_t lease{};
    EXPECT_EQ(nx_stm32_fsmc_sram_acquire(&bus, &lease), NX_ERROR_EXHAUSTED);
    EXPECT_EQ(lease.owner, nullptr);
    EXPECT_EQ(nx_stm32_fsmc_stop(&bus), NX_SUCCESS);
}

TEST_F(FSMC, PlatformSnapshotsCannotAliasMutableBusReceipts) {
    pins[0].snapshot = reinterpret_cast<uint16_t*>(&bus.epoch);
    EXPECT_EQ(prepare(), NX_ERROR_INVALID);
    EXPECT_EQ(bus.config, nullptr);
    EXPECT_EQ(RCC->AHB3ENR & RCC_AHB3ENR_FSMCEN, 0U);
}

TEST_F(FSMC, LCDReceiveAndProgressOutputsCannotOverwriteBusOwnership) {
    ASSERT_EQ(prepare(), NX_SUCCESS);
    nx_stm32_fsmc_lcd_t lcd{};
    const nx_parallel16_endpoint_t port{&nx_stm32_fsmc_parallel16_ops, &lcd};
    ASSERT_EQ(nx_stm32_fsmc_lcd_prepare(&bus, &lcd), NX_SUCCESS);
    size_t done = 99U;
    const uint64_t epoch = bus.epoch;
    EXPECT_EQ(port.ops->read(port.context,
                             reinterpret_cast<uint16_t*>(&bus.epoch), 1U, &done,
                             100U),
              NX_ERROR_INVALID);
    EXPECT_EQ(bus.epoch, epoch);
    EXPECT_EQ(done, 99U);
    const uint16_t pixel = 0U;
    EXPECT_EQ(port.ops->write(port.context, &pixel, 1U,
                              reinterpret_cast<size_t*>(&bus.epoch), 100U),
              NX_ERROR_INVALID);
    EXPECT_EQ(bus.epoch, epoch);
    EXPECT_TRUE(writes.empty());
}

TEST_F(FSMC, LCDCannotOverwritePlatformSnapshotsOrAnotherBankLease) {
    ASSERT_EQ(prepare(), NX_SUCCESS);
    nx_stm32_fsmc_lease_t sram{};
    ASSERT_EQ(nx_stm32_fsmc_sram_acquire(&bus, &sram), NX_SUCCESS);
    nx_stm32_fsmc_lcd_t lcd{};
    const nx_parallel16_endpoint_t port{&nx_stm32_fsmc_parallel16_ops, &lcd};
    ASSERT_EQ(nx_stm32_fsmc_lcd_prepare(&bus, &lcd), NX_SUCCESS);
    size_t done = 99U;
    const uint64_t epoch = sram.epoch;
    snapshots[0] = 0xdeadU;
    EXPECT_EQ(port.ops->read(port.context, snapshots.data(), 1U, &done, 100U),
              NX_ERROR_INVALID);
    EXPECT_EQ(snapshots[0], 0xdeadU);
    EXPECT_EQ(port.ops->read(port.context,
                             reinterpret_cast<uint16_t*>(&sram.epoch), 1U,
                             &done, 100U),
              NX_ERROR_INVALID);
    EXPECT_EQ(sram.epoch, epoch);
    EXPECT_EQ(done, 99U);
}

TEST_F(FSMC, RealILI9341ControllerUsesTheSameSettledBankAndClockOwner) {
    ASSERT_EQ(prepare(), NX_SUCCESS);
    nx_stm32_fsmc_lcd_t lcd{};
    const nx_parallel16_endpoint_t port{&nx_stm32_fsmc_parallel16_ops, &lcd};
    ASSERT_EQ(nx_stm32_fsmc_lcd_prepare(&bus, &lcd), NX_SUCCESS);
    read_words = {0U, 0U, 0x93U, 0x41U};
    nx_ili9341_t display{};
    const nx_ili9341_config_t orientation{&port, config.clock, 0x28U};
    ASSERT_EQ(nx_ili9341_initialize(&display, &orientation, 500000U),
              NX_SUCCESS);
    EXPECT_EQ(nx_stm32_fsmc_stop(&bus), NX_ERROR_BUSY);
    model_time = 120000U;
    ASSERT_EQ(nx_ili9341_service(&display), NX_ERROR_BUSY);
    model_time = 240000U;
    ASSERT_EQ(nx_ili9341_service(&display), NX_SUCCESS);
    const uint16_t red = 0xf800U;
    size_t done{};
    EXPECT_EQ(nx_ili9341_write(&display, 319U, 239U, 1U, 1U, &red, 1U, &done,
                               500000U),
              NX_SUCCESS);
    EXPECT_EQ(done, 1U);
    EXPECT_EQ(writes.back(),
              std::make_pair(uintptr_t{0x6c002000U}, uint16_t{0xf800U}));
    ASSERT_EQ(nx_ili9341_stop(&display, 500000U), NX_SUCCESS);
    ASSERT_EQ(nx_stm32_fsmc_release(&lcd.lease, lcd.lease.epoch), NX_SUCCESS);
    EXPECT_EQ(nx_stm32_fsmc_stop(&bus), NX_SUCCESS);
}

TEST_F(FSMC, LCDOnlyUsesTwentyPadsAndNoGPIOFOrSRAMAddressOwnership) {
    const std::array<uint16_t, 4> selected{0xc733U, 0xff80U, 0U, 0x1004U};
    pins.erase(std::remove_if(pins.begin(), pins.end(),
                              [&](const auto& pin) {
                                  return (selected[pin.port_index - 3U] &
                                          (1U << pin.pin)) == 0U;
                              }),
               pins.end());
    config.pin_count = pins.size();
    config.banks = &banks[1];
    config.bank_count = 1U;
    RCC->AHB1ENR &= ~uint32_t{RCC_AHB1ENR_GPIOFEN};
    GPIOF->MODER = 0xaaaaaaaaU;
    GPIOG->AFR[0] = 0x76543210U;
    nx_stm32_fsmc_bank_state_t single{};
    ASSERT_EQ(config.pin_count, 20U);
    ASSERT_EQ(nx_stm32_fsmc_prepare(&bus, &config, &single, 1U), NX_SUCCESS);
    EXPECT_EQ(GPIOF->MODER, 0xaaaaaaaaU);
    EXPECT_EQ(GPIOG->AFR[0] & 0xfffff0ffU, 0x76543010U);
    EXPECT_EQ(FSMC_Bank1->BTCR[4], 0x30d2U);
    nx_stm32_fsmc_lease_t sram{};
    EXPECT_EQ(nx_stm32_fsmc_sram_acquire(&bus, &sram), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_stm32_fsmc_stop(&bus), NX_SUCCESS);
}

TEST_F(FSMC, SRAMOnlyUsesFortyPadsAndDoesNotReconfigureNE4) {
    pins.erase(std::remove_if(pins.begin(), pins.end(),
                              [](const auto& pin) {
                                  return pin.port_index == 6U && pin.pin == 12U;
                              }),
               pins.end());
    config.pin_count = pins.size();
    config.bank_count = 1U;
    GPIOG->AFR[1] = 0x11111111U;
    nx_stm32_fsmc_bank_state_t single{};
    ASSERT_EQ(config.pin_count, 40U);
    ASSERT_EQ(nx_stm32_fsmc_prepare(&bus, &config, &single, 1U), NX_SUCCESS);
    EXPECT_EQ((GPIOG->AFR[1] >> 16U) & 15U, 1U);
    EXPECT_EQ(FSMC_Bank1->BTCR[6], 0x30d2U);
    nx_stm32_fsmc_lcd_t lcd{};
    const nx_parallel16_endpoint_t port{&nx_stm32_fsmc_parallel16_ops, &lcd};
    EXPECT_EQ(nx_stm32_fsmc_lcd_prepare(&bus, &lcd), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(port.context, &lcd);
    EXPECT_EQ(nx_stm32_fsmc_stop(&bus), NX_SUCCESS);
}

TEST_F(FSMC, LCDOnlyRejectsMissingOrForeignSRAMPadsBeforeEffects) {
    const auto foreign =
        *std::find_if(pins.begin(), pins.end(), [](const auto& p) {
            return p.port_index == 5U && p.pin == 0U;
        });
    const std::array<uint16_t, 4> selected{0xc733U, 0xff80U, 0U, 0x1004U};
    pins.erase(std::remove_if(pins.begin(), pins.end(),
                              [&](const auto& pin) {
                                  return (selected[pin.port_index - 3U] &
                                          (1U << pin.pin)) == 0U;
                              }),
               pins.end());
    config.banks = &banks[1];
    config.bank_count = 1U;
    nx_stm32_fsmc_bank_state_t single{};
    pins.push_back(foreign);
    config.pin_count = pins.size();
    EXPECT_EQ(nx_stm32_fsmc_prepare(&bus, &config, &single, 1U),
              NX_ERROR_INVALID);
    pins.pop_back();
    pins.pop_back();
    config.pin_count = pins.size();
    EXPECT_EQ(nx_stm32_fsmc_prepare(&bus, &config, &single, 1U),
              NX_ERROR_INVALID);
    EXPECT_EQ(RCC->AHB3ENR, 0U);
}

TEST_F(FSMC, DirectCPUStoresJoinBeforeLeasePublicationAndBankWithdrawal) {
    ASSERT_EQ(prepare(), NX_SUCCESS);
    nx_stm32_fsmc_lease_t lease{};
    ASSERT_EQ(nx_stm32_fsmc_sram_acquire(&bus, &lease), NX_SUCCESS);
    watched_lease = &lease;
    barriers.clear();
    ASSERT_EQ(nx_stm32_fsmc_release(&lease, lease.epoch), NX_SUCCESS);
    ASSERT_FALSE(barriers.empty());
    EXPECT_TRUE(barriers.front().lease_held);
    barriers.clear();
    ASSERT_EQ(nx_stm32_fsmc_stop(&bus), NX_SUCCESS);
    ASSERT_FALSE(barriers.empty());
    EXPECT_NE(barriers.front().sram_control & FSMC_BCR1_MBKEN, 0U);
    EXPECT_NE(barriers.front().lcd_control & FSMC_BCR1_MBKEN, 0U);
    EXPECT_NE(barriers.front().gate & RCC_AHB3ENR_FSMCEN, 0U);
}
TEST_F(FSMC, StopFlushesCPUAccessBeforeTheFirstBankDisable) {
    ASSERT_EQ(prepare(), NX_SUCCESS);
    barriers.clear();
    ASSERT_EQ(nx_stm32_fsmc_stop(&bus), NX_SUCCESS);
    ASSERT_FALSE(barriers.empty());
    EXPECT_NE(barriers.front().sram_control & FSMC_BCR1_MBKEN, 0U);
    EXPECT_NE(barriers.front().lcd_control & FSMC_BCR1_MBKEN, 0U);
    EXPECT_NE(barriers.front().gate & RCC_AHB3ENR_FSMCEN, 0U);
}

TEST_F(FSMC, ImmutablePublicFacesUseExactOwnerAndNoAcquisitionAtLookup) {
    const nx_external_bus_port_t owner{&nx_stm32_fsmc_external_bus_ops, &bus};
    const nx_memory_window_port_t memory{&nx_stm32_fsmc_memory_window_ops,
                                         &bus};
    EXPECT_EQ(owner.ops->state(owner.context), NX_EXTERNAL_BUS_STOPPED);
    EXPECT_EQ(bus.config, nullptr);
    EXPECT_EQ(RCC->AHB3ENR, 0U);
    ASSERT_EQ(prepare(), NX_SUCCESS);
    EXPECT_EQ(owner.ops->state(owner.context), NX_EXTERNAL_BUS_PREPARED);
    nx_memory_window_lease_t loan{};
    ASSERT_EQ(memory.ops->acquire(memory.context, &loan), NX_SUCCESS);
    nx_stm32_fsmc_t foreign{};
    EXPECT_EQ(memory.ops->release(&foreign, &loan, loan.epoch),
              NX_ERROR_INVALID);
    EXPECT_NE(loan.owner, nullptr);
    EXPECT_EQ(owner.ops->stop(owner.context), NX_ERROR_BUSY);
    ASSERT_EQ(memory.ops->release(memory.context, &loan, loan.epoch),
              NX_SUCCESS);
    ASSERT_EQ(owner.ops->stop(owner.context), NX_SUCCESS);
    EXPECT_EQ(owner.ops->state(owner.context), NX_EXTERNAL_BUS_STOPPED);
}

TEST_F(FSMC, PublicParallelCommandPreservesSixteenBitsAndExplicitClose) {
    ASSERT_EQ(prepare(), NX_SUCCESS);
    nx_stm32_fsmc_lcd_t lcd{};
    const nx_parallel16_endpoint_t port{&nx_stm32_fsmc_parallel16_ops, &lcd};
    ASSERT_EQ(nx_stm32_fsmc_lcd_prepare(&bus, &lcd), NX_SUCCESS);
    ASSERT_EQ(port.ops->command(port.context, 0xabcdU, 100U), NX_SUCCESS);
    ASSERT_EQ(writes.size(), 1U);
    EXPECT_EQ(writes[0].second, 0xabcdU);
    ASSERT_EQ(nx_stm32_fsmc_lcd_close(&lcd), NX_SUCCESS);
    EXPECT_EQ(port.ops->command(port.context, 0x2cU, 100U), NX_ERROR_STATE);
    EXPECT_EQ(nx_stm32_fsmc_lcd_close(&lcd), NX_SUCCESS);
    EXPECT_EQ(nx_stm32_fsmc_stop(&bus), NX_SUCCESS);
}

TEST_F(FSMC, OfficialResetReservedWriteTimingBitsDoNotRejectValidLLSetup) {
    for (size_t bank = 0U; bank < 4U; ++bank) {
        FSMC_Bank1->BTCR[bank * 2U + 1U] = 0x0fffffffU;
        FSMC_Bank1E->BWTR[bank * 2U] = 0x0fffffffU;
    }
    ASSERT_EQ(prepare(), NX_SUCCESS);
    EXPECT_EQ((FSMC_Bank1E->BWTR[4] >> 8U) & 0xffU, 5U);
    EXPECT_EQ(FSMC_Bank1E->BWTR[4] & 0x0ff00000U, 0x0ff00000U);
    ASSERT_EQ(nx_stm32_fsmc_stop(&bus), NX_SUCCESS);
    EXPECT_EQ(FSMC_Bank1E->BWTR[4], 0x0fffffffU);
    EXPECT_EQ(FSMC_Bank1E->BWTR[6], 0x0fffffffU);
}
TEST_F(FSMC, CPUWindowGeometryDoesNotAcquireAndCannotOverwriteOwner) {
    ASSERT_NE(nx_stm32_fsmc_memory_window_ops.geometry, nullptr);
    nx_memory_window_geometry_t geometry{123U, 456U, true};
    EXPECT_EQ(nx_stm32_fsmc_memory_window_ops.geometry(&bus, &geometry),
              NX_ERROR_STATE);
    EXPECT_EQ(geometry.address, 123U);
    ASSERT_EQ(prepare(), NX_SUCCESS);
    EXPECT_EQ(nx_stm32_fsmc_memory_window_ops.geometry(&bus, &geometry),
              NX_SUCCESS);
    EXPECT_EQ(geometry.address, 0x68000000U);
    EXPECT_EQ(geometry.bytes, 0x100000U);
    EXPECT_FALSE(geometry.startup_backing);
    EXPECT_EQ(states[0].lease, nullptr);
    EXPECT_EQ(nx_stm32_fsmc_memory_window_ops.geometry(
                  &bus, reinterpret_cast<nx_memory_window_geometry_t*>(&bus)),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_stm32_fsmc_stop(&bus), NX_SUCCESS);
}
TEST_F(FSMC, PublicProgressRejectionCannotOverwriteOpaqueProviderOwnership) {
    ASSERT_EQ(prepare(), NX_SUCCESS);
    nx_stm32_fsmc_lcd_t lcd{};
    const nx_parallel16_endpoint_t port{&nx_stm32_fsmc_parallel16_ops, &lcd};
    ASSERT_EQ(nx_stm32_fsmc_lcd_prepare(&bus, &lcd), NX_SUCCESS);
    const uint16_t pixel = 0xf800U;
    void* owner = lcd.lease.owner;
    const uint64_t epoch = lcd.lease.epoch;
    EXPECT_EQ(nx_parallel16_endpoint_write(
                  &port, &pixel, 1U,
                  reinterpret_cast<size_t*>(&lcd.lease.owner), 100U),
              NX_ERROR_INVALID);
    EXPECT_EQ(lcd.lease.owner, owner);
    EXPECT_EQ(lcd.lease.epoch, epoch);
    EXPECT_TRUE(writes.empty());
}
TEST_F(FSMC, LCDPayloadCannotReadFromOrWriteToKnownPeripheralRegisters) {
    ASSERT_EQ(prepare(), NX_SUCCESS);
    nx_stm32_fsmc_lcd_t lcd{};
    const nx_parallel16_endpoint_t port{&nx_stm32_fsmc_parallel16_ops, &lcd};
    ASSERT_EQ(nx_stm32_fsmc_lcd_prepare(&bus, &lcd), NX_SUCCESS);
    const auto address = uintptr_t{RCC_BASE} + offsetof(RCC_TypeDef, AHB3ENR);
    auto* output = reinterpret_cast<uint16_t*>(address);
    size_t done = 99U;
    const uint32_t before = RCC->AHB3ENR;
    EXPECT_EQ(port.ops->read(port.context, output, 1U, &done, 100U),
              NX_ERROR_INVALID);
    EXPECT_EQ(RCC->AHB3ENR, before);
    EXPECT_EQ(done, 99U);
    EXPECT_EQ(port.ops->write(port.context, output, 1U, &done, 100U),
              NX_ERROR_INVALID);
    EXPECT_EQ(done, 99U);
    EXPECT_TRUE(writes.empty());
}

TEST_F(FSMC, LCDProgressCannotZeroKnownPeripheralRegistersBeforeAdmission) {
    ASSERT_EQ(prepare(), NX_SUCCESS);
    nx_stm32_fsmc_lcd_t lcd{};
    const nx_parallel16_endpoint_t port{&nx_stm32_fsmc_parallel16_ops, &lcd};
    ASSERT_EQ(nx_stm32_fsmc_lcd_prepare(&bus, &lcd), NX_SUCCESS);
    const auto address = uintptr_t{RCC_BASE} + offsetof(RCC_TypeDef, AHB3ENR);
    auto* progress = reinterpret_cast<size_t*>(address);
    const uint32_t before = RCC->AHB3ENR;
    const uint16_t word = 0x1234U;
    EXPECT_EQ(port.ops->write(port.context, &word, 1U, progress, 100U),
              NX_ERROR_INVALID);
    EXPECT_EQ(RCC->AHB3ENR, before);
    EXPECT_TRUE(writes.empty());
}

TEST_F(FSMC, LCDDeviceApertureIsNotAnOrdinaryPayloadSpan) {
    ASSERT_EQ(prepare(), NX_SUCCESS);
    nx_stm32_fsmc_lcd_t lcd{};
    const nx_parallel16_endpoint_t port{&nx_stm32_fsmc_parallel16_ops, &lcd};
    ASSERT_EQ(nx_stm32_fsmc_lcd_prepare(&bus, &lcd), NX_SUCCESS);
    auto* device = reinterpret_cast<uint16_t*>(uintptr_t{0x6c002000U});
    size_t done = 99U;
    EXPECT_EQ(port.ops->read(port.context, device, 1U, &done, 100U),
              NX_ERROR_INVALID);
    EXPECT_EQ(done, 99U);
    EXPECT_EQ(port.ops->write(port.context, device, 1U, &done, 100U),
              NX_ERROR_INVALID);
    EXPECT_EQ(done, 99U);
    EXPECT_TRUE(writes.empty());
}

TEST_F(FSMC, MutableBusRecordsCannotBePlacedInPeripheralAddressSpace) {
    auto* control = reinterpret_cast<nx_stm32_fsmc_t*>(uintptr_t{0x40010000U});
    std::array<unsigned char, sizeof(nx_stm32_fsmc_t)> before{};
    std::memcpy(before.data(), control, before.size());
    EXPECT_EQ(
        nx_stm32_fsmc_prepare(control, &config, states.data(), states.size()),
        NX_ERROR_INVALID);
    EXPECT_EQ(0, std::memcmp(before.data(), control, before.size()));
    EXPECT_EQ(RCC->AHB3ENR, 0U);
}

TEST_F(FSMC, LiveNE3LoanCanSupplyAndReceiveLCDFramebufferWords) {
    auto* frame = static_cast<uint16_t*>(
        mmap(reinterpret_cast<void*>(uintptr_t{0x68000000U}), 0x100000U,
             PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0));
    ASSERT_EQ(reinterpret_cast<void*>(uintptr_t{0x68000000U}), frame);
    ASSERT_EQ(prepare(), NX_SUCCESS);
    nx_stm32_fsmc_lease_t memory{};
    ASSERT_EQ(nx_stm32_fsmc_sram_acquire(&bus, &memory), NX_SUCCESS);
    nx_stm32_fsmc_lcd_t lcd{};
    const nx_parallel16_endpoint_t port{&nx_stm32_fsmc_parallel16_ops, &lcd};
    ASSERT_EQ(nx_stm32_fsmc_lcd_prepare(&bus, &lcd), NX_SUCCESS);
    frame[0] = 0xf800U;
    frame[1] = 0x07e0U;
    frame[2] = 0x001fU;
    size_t done = 99U;
    ASSERT_EQ(port.ops->write(port.context, frame, 3U, &done, 100U),
              NX_SUCCESS);
    EXPECT_EQ(done, 3U);
    ASSERT_EQ(writes.size(), 3U);
    EXPECT_EQ(writes[0].second, 0xf800U);
    EXPECT_EQ(writes[1].second, 0x07e0U);
    EXPECT_EQ(writes[2].second, 0x001fU);
    read_words = {0x1111U, 0x2222U, 0x3333U};
    read_position = 0U;
    ASSERT_EQ(port.ops->read(port.context, frame, 3U, &done, 100U), NX_SUCCESS);
    EXPECT_EQ(done, 3U);
    EXPECT_EQ(frame[0], 0x1111U);
    EXPECT_EQ(frame[1], 0x2222U);
    EXPECT_EQ(frame[2], 0x3333U);
    EXPECT_EQ(nx_stm32_fsmc_release(&memory, memory.epoch), NX_SUCCESS);
    done = 99U;
    EXPECT_EQ(port.ops->write(port.context, frame, 3U, &done, 100U),
              NX_ERROR_INVALID);
    EXPECT_EQ(done, 99U);
    EXPECT_EQ(nx_stm32_fsmc_lcd_close(&lcd), NX_SUCCESS);
    EXPECT_EQ(nx_stm32_fsmc_stop(&bus), NX_SUCCESS);
    EXPECT_EQ(0, munmap(frame, 0x100000U));
}
TEST_F(FSMC, InternalFlashCanSupplyPixelsButCannotReceiveReadback) {
    void* address = reinterpret_cast<void*>(uintptr_t{0x08000000U});
    void* storage =
        mmap(address, 0x1000U, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    ASSERT_EQ(storage, address);
    auto* pixel = static_cast<uint16_t*>(storage);
    *pixel = 0xf800U;
    ASSERT_EQ(prepare(), NX_SUCCESS);
    nx_stm32_fsmc_lcd_t lcd{};
    ASSERT_EQ(nx_stm32_fsmc_lcd_prepare(&bus, &lcd), NX_SUCCESS);
    size_t done = 99U;
    EXPECT_EQ(nx_stm32_fsmc_parallel16_ops.write(&lcd, pixel, 1U, &done, 100U),
              NX_SUCCESS);
    EXPECT_EQ(done, 1U);
    done = 99U;
    EXPECT_EQ(nx_stm32_fsmc_parallel16_ops.read(&lcd, pixel, 1U, &done, 100U),
              NX_ERROR_INVALID);
    EXPECT_EQ(done, 99U);
    EXPECT_EQ(*pixel, 0xf800U);
    EXPECT_EQ(nx_stm32_fsmc_lcd_close(&lcd), NX_SUCCESS);
    EXPECT_EQ(nx_stm32_fsmc_stop(&bus), NX_SUCCESS);
    EXPECT_EQ(0, munmap(storage, 0x1000U));
}
TEST_F(FSMC, ColdOwnerCannotOverwriteTheKnownClockContextAnchor) {
    config.clock.context = &bus;
    EXPECT_EQ(prepare(), NX_ERROR_INVALID);
    EXPECT_EQ(bus.config, nullptr);
    EXPECT_EQ(RCC->AHB3ENR, 0U);
}

TEST_F(FSMC, ColdBankReceiptsCannotOverwriteTheKnownClockContextAnchor) {
    config.clock.context = &states[0];
    EXPECT_EQ(prepare(), NX_ERROR_INVALID);
    EXPECT_EQ(bus.config, nullptr);
    EXPECT_EQ(RCC->AHB3ENR, 0U);
}
} /* namespace */

extern "C" bool nx_fsmc_privileged(void) {
    return privileged;
}
extern "C" bool nx_fsmc_in_isr(void) {
    return in_isr;
}
extern "C" bool nx_fsmc_masked(void) {
    return masked;
}
extern "C" void nx_fsmc_dsb(void) {
    barriers.push_back(
        {FSMC_Bank1->BTCR[4], FSMC_Bank1->BTCR[6], RCC->AHB3ENR,
         watched_lease != nullptr && watched_lease->owner != nullptr});
}
extern "C" uint32_t nx_fsmc_model_read(const volatile uint32_t* reg) {
    return observed(reg);
}
extern "C" uint16_t nx_fsmc_model_read16(const volatile uint16_t* address) {
    model_time += word_time;
    if (read_position < read_words.size())
        return read_words[read_position++];
    return *address;
}
extern "C" void nx_fsmc_model_write16(volatile uint16_t* address,
                                      uint16_t value) {
    if (release_during_write != nullptr) {
        auto* lease = release_during_write;
        release_during_write = nullptr;
        reentry_result = nx_stm32_fsmc_release(lease, lease->epoch);
    }
    writes.emplace_back(reinterpret_cast<uintptr_t>(address), value);
    *address = value;
    model_time += word_time;
}
