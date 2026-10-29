/**
 * \file            stm32_platform_cold_test.cpp
 * \brief           Concrete Platform admission and retained resource contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f407_platform_cold.h"
nx_stm32_system_t g_nx_stm32_system;
NVIC_Type g_gd32_model_nvic;
SCB_Type g_gd32_model_scb;
SysTick_Type g_gd32_model_systick;
uint32_t g_gd32_model_mask;
bool g_gd32_model_isr;
uint16_t nx_platform_actual_pin_capture(const GPIO_TypeDef* gpio, uint8_t pin);
void nx_platform_actual_pin_restore(GPIO_TypeDef* gpio, uint8_t pin,
                                    uint16_t saved);
}
#include <array>
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <vector>

namespace {
bool privileged;
volatile uint32_t* refused;
uint32_t held_bits;
unsigned writes;
unsigned captures;
unsigned restores;
GPIO_TypeDef* refused_restore_gpio;
uint8_t refused_restore_pin;
std::array<GPIO_TypeDef*, 3> selected_gpio{GPIOB, GPIOB, GPIOC};
std::array<uint8_t, 3> selected_pin{6U, 7U, 1U};
std::array<unsigned, 3> restore_counts;
bool require_all_withdrawn;

class PlatformCold : public testing::Test {
  protected:
    void* mmio = MAP_FAILED;
    uint16_t snapshots[3]{};
    nx_stm32_platform_pin_config_t pins[3]{{GPIOB, &snapshots[0], 1U, 6U},
                                           {GPIOB, &snapshots[1], 1U, 7U},
                                           {GPIOC, &snapshots[2], 2U, 1U}};
    DMA_Stream_TypeDef* streams[1]{DMA1_Stream0};
    nx_stm32_platform_config_t config{};
    nx_stm32_platform_pin_receipt_t receipt{};
    void SetUp() override {
        mmio = mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000U,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mmio, reinterpret_cast<void*>(UINT32_C(0x40000000)));
        g_gd32_model_nvic = {};
        g_gd32_model_scb = {};
        g_gd32_model_systick = {};
        g_gd32_model_mask = 0U;
        g_gd32_model_isr = false;
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = RCC;
        privileged = true;
        refused = nullptr;
        held_bits = 0U;
        writes = captures = restores = 0U;
        refused_restore_gpio = nullptr;
        refused_restore_pin = 16U;
        restore_counts.fill(0U);
        require_all_withdrawn = false;
        snapshots[0] = snapshots[1] = snapshots[2] = 0xFFFFU;
        config.pins = pins;
        config.pin_count = 3U;
        config.dma_streams = streams;
        config.dma_stream_count = 1U;
        config.apb1_clocks = RCC_APB1ENR_I2C1EN;
        config.apb2_clocks = RCC_APB2ENR_USART1EN;
        config.ahb1_clocks = RCC_AHB1ENR_DMA1EN;
        config.ahb2_clocks = RCC_AHB2ENR_OTGFSEN;
        config.irq_masks[0] = 1U << 31U;
        config.irq_masks[1] = 1U;
        config.exti_lines = 1U << 5U;
    }
    void TearDown() override {
        if (mmio != MAP_FAILED) {
            EXPECT_EQ(0, munmap(mmio, 0x80000U));
        }
    }
    nx_result_t Acquire() {
        return nx_stm32_platform_pins_acquire(&config, &receipt);
    }
    nx_result_t Release() {
        return nx_stm32_platform_pins_release(&config, &receipt);
    }
    std::vector<uint8_t> Registers() {
        const auto* first = static_cast<const uint8_t*>(mmio);
        return {first, first + 0x80000U};
    }
    void DrivePins() {
        GPIOB->MODER = (GPIOB->MODER & ~0xF000U) | 0x6000U;
        GPIOC->MODER = (GPIOC->MODER & ~0xCU) | 0x8U;
        GPIOB->AFR[0] = 0x44000000U;
        GPIOC->AFR[0] = 0x40U;
    }
};
} /* namespace */

TEST_F(PlatformCold, ColdContextRejectsBeforeAcquiringOrWritingResources) {
    for (unsigned context = 0U; context < 3U; ++context) {
        privileged = context != 0U;
        g_gd32_model_isr = context == 1U;
        g_gd32_model_mask = context == 2U ? 1U : 0U;
        const auto before = Registers();
        EXPECT_EQ(NX_ERROR_CONTEXT,
                  nx_stm32_platform_selected_preflight(&config));
        EXPECT_EQ(NX_ERROR_CONTEXT, Acquire());
        EXPECT_EQ(before, Registers());
        EXPECT_EQ(0U, writes);
        EXPECT_EQ(0U, captures);
        EXPECT_FALSE(receipt.captured);
        EXPECT_EQ(0U, receipt.clocks_added);
    }
}

TEST_F(PlatformCold, EverySelectedClockRefusesBeforeAnyResourceEffect) {
    const std::array<volatile uint32_t*, 4> registers{
        &RCC->APB1ENR, &RCC->APB2ENR, &RCC->AHB1ENR, &RCC->AHB2ENR};
    const std::array<uint32_t, 4> gates{
        RCC_APB1ENR_I2C1EN, RCC_APB2ENR_USART1EN, RCC_AHB1ENR_DMA1EN,
        RCC_AHB2ENR_OTGFSEN};
    for (size_t i = 0U; i < registers.size(); ++i) {
        *registers[i] = gates[i];
        const auto before = Registers();
        EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_platform_selected_preflight(&config));
        EXPECT_EQ(before, Registers());
        EXPECT_EQ(0U, writes);
        EXPECT_EQ(0U, captures);
        *registers[i] = 0U;
    }
}

TEST_F(PlatformCold, IrqEnablePendingAndActiveRemainIndependentOnRefusal) {
    const std::array<uint32_t*, 3> words{NVIC->ISER, NVIC->ISPR, NVIC->IABR};
    for (uint32_t* word : words) {
        word[1] = 1U;
        const auto before = g_gd32_model_nvic;
        EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_platform_selected_preflight(&config));
        EXPECT_EQ(0, std::memcmp(&before, &g_gd32_model_nvic, sizeof(before)));
        EXPECT_EQ(0U, writes);
        word[1] = 0U;
    }
}

TEST_F(PlatformCold, SelectedDmaAndExtiFactsRejectWithoutAcknowledgement) {
    DMA1_Stream0->CR = DMA_SxCR_EN;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_platform_selected_preflight(&config));
    EXPECT_EQ(DMA_SxCR_EN, DMA1_Stream0->CR);
    DMA1_Stream0->CR = 0U;
    for (volatile uint32_t* word : {&EXTI->IMR, &EXTI->EMR, &EXTI->PR}) {
        *word = config.exti_lines;
        EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_platform_selected_preflight(&config));
        EXPECT_EQ(config.exti_lines, *word);
        *word = 0U;
    }
    EXPECT_EQ(0U, writes);
    EXPECT_EQ(0U, captures);
}

TEST_F(PlatformCold, UnselectedControllersAndPublishersRemainUntouched) {
    RCC->APB1ENR = RCC_APB1ENR_TIM3EN;
    RCC->APB2ENR = RCC_APB2ENR_SPI1EN;
    RCC->AHB1ENR = RCC_AHB1ENR_DMA2EN;
    RCC->AHB2ENR = RCC_AHB2ENR_DCMIEN;
    NVIC->ISER[0] = NVIC->ISPR[0] = NVIC->IABR[0] = 1U << 3U;
    DMA1_Stream1->CR = DMA_SxCR_EN;
    EXTI->IMR = EXTI->EMR = EXTI->PR = 1U << 4U;
    const auto before = Registers();
    const auto nvic = g_gd32_model_nvic;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_platform_selected_preflight(&config));
    EXPECT_EQ(before, Registers());
    EXPECT_EQ(0, std::memcmp(&nvic, &g_gd32_model_nvic, sizeof(nvic)));
    EXPECT_EQ(0U, writes);
}

TEST_F(PlatformCold, PoweredForeignPinOrLockRejectsWithoutAcquisition) {
    RCC->AHB1ENR = RCC_AHB1ENR_GPIOBEN;
    for (uint32_t mode : {1U, 2U}) {
        GPIOB->MODER = mode << 12U;
        EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_platform_pins_admit(&config, true));
        EXPECT_EQ(mode << 12U, GPIOB->MODER);
    }
    GPIOB->MODER = 0U;
    GPIOB->LCKR = GPIO_LCKR_LCKK | (1U << 7U);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_platform_pins_admit(&config, true));
    EXPECT_EQ(GPIO_LCKR_LCKK | (1U << 7U), GPIOB->LCKR);
    EXPECT_EQ(0U, writes);
    EXPECT_EQ(0U, captures);
}

TEST_F(PlatformCold, AllUnpoweredPinsAreAdmittedBeforeAnySnapshotIsCaptured) {
    GPIOC->MODER = 2U << 2U;
    RCC->AHB1ENR = RCC_AHB1ENR_GPIOAEN;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_platform_pins_admit(&config, true));
    EXPECT_EQ(NX_ERROR_BUSY, Acquire());
    EXPECT_EQ(0U, captures);
    EXPECT_EQ(0U, restores);
    EXPECT_FALSE(receipt.captured);
    EXPECT_EQ(0xFFFFU, snapshots[0]);
    EXPECT_EQ(0xFFFFU, snapshots[1]);
    EXPECT_EQ(0xFFFFU, snapshots[2]);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_platform_gpio_clocks_release(&receipt));
    EXPECT_EQ(RCC_AHB1ENR_GPIOAEN, RCC->AHB1ENR);
}

TEST_F(PlatformCold, RefusedPinGateAcquireRetainsOnlyItsAttemptedGateLease) {
    RCC->AHB1ENR = RCC_AHB1ENR_GPIOAEN;
    refused = &RCC->AHB1ENR;
    held_bits = RCC_AHB1ENR_GPIOCEN;
    EXPECT_EQ(NX_ERROR_IO, Acquire());
    EXPECT_FALSE(receipt.captured);
    EXPECT_EQ(0U, captures);
    EXPECT_EQ(0U, restores);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_platform_gpio_clocks_release(&receipt));
    EXPECT_EQ(RCC_AHB1ENR_GPIOAEN, RCC->AHB1ENR);
    EXPECT_EQ(0U, receipt.clocks_added);
}

TEST_F(PlatformCold, GateReleaseRefusalKeepsRetryAndDoesNotCapturePins) {
    GPIOC->LCKR = GPIO_LCKR_LCKK | (1U << 1U);
    RCC->AHB1ENR = RCC_AHB1ENR_GPIOAEN;
    EXPECT_EQ(NX_ERROR_BUSY, Acquire());
    refused = &RCC->AHB1ENR;
    held_bits = RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_platform_gpio_clocks_release(&receipt));
    EXPECT_NE(0U, receipt.clocks_added);
    EXPECT_FALSE(receipt.captured);
    EXPECT_EQ(NX_ERROR_STATE, Acquire());
    EXPECT_EQ(0U, captures);
    refused = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_platform_gpio_clocks_release(&receipt));
    EXPECT_EQ(RCC_AHB1ENR_GPIOAEN, RCC->AHB1ENR);
    EXPECT_EQ(0U, receipt.clocks_added);
}

TEST_F(PlatformCold, AcquireKeepsOneSnapshotAndOnlyItsAddedClockBits) {
    RCC->AHB1ENR = RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOAEN;
    GPIOB->MODER = 3U << 12U;
    GPIOB->PUPDR = 2U << 14U;
    GPIOC->AFR[0] = 9U << 4U;
    const auto before = Registers();
    EXPECT_EQ(NX_SUCCESS, Acquire());
    EXPECT_TRUE(receipt.captured);
    EXPECT_EQ(RCC_AHB1ENR_GPIOCEN, receipt.clocks_added);
    EXPECT_EQ(3U, captures);
    EXPECT_NE(0xFFFFU, snapshots[0]);
    EXPECT_NE(0xFFFFU, snapshots[1]);
    EXPECT_NE(0xFFFFU, snapshots[2]);
    const auto count = captures;
    EXPECT_EQ(NX_ERROR_STATE, Acquire());
    EXPECT_EQ(count, captures);
    EXPECT_EQ(NX_SUCCESS, Release());
    EXPECT_EQ(before, Registers());
    EXPECT_FALSE(receipt.captured);
    EXPECT_EQ(0U, receipt.clocks_added);
}

TEST_F(PlatformCold, SystemFailureAbortDropsCaptureWithoutTouchingPinState) {
    RCC->AHB1ENR = RCC_AHB1ENR_GPIOAEN;
    ASSERT_EQ(NX_SUCCESS, Acquire());
    const auto captured =
        std::array<uint16_t, 3>{snapshots[0], snapshots[1], snapshots[2]};
    EXPECT_EQ(NX_SUCCESS, nx_stm32_platform_pins_abort(&receipt));
    EXPECT_FALSE(receipt.captured);
    EXPECT_EQ(0U, receipt.clocks_added);
    EXPECT_EQ(RCC_AHB1ENR_GPIOAEN, RCC->AHB1ENR);
    EXPECT_EQ(0U, restores);
    EXPECT_EQ(captured, (std::array<uint16_t, 3>{snapshots[0], snapshots[1],
                                                 snapshots[2]}));
}

TEST_F(PlatformCold, EveryDrivenPinWithdrawsBeforeAnyOriginalStateIsRestored) {
    ASSERT_EQ(NX_SUCCESS, Acquire());
    DrivePins();
    require_all_withdrawn = true;
    EXPECT_EQ(NX_SUCCESS, Release());
    EXPECT_EQ(3U, restores);
    EXPECT_EQ(0U, RCC->AHB1ENR);
    EXPECT_FALSE(receipt.captured);
}

TEST_F(PlatformCold, WithdrawalRefusalPreservesEveryOriginalSnapshotAndGate) {
    ASSERT_EQ(NX_SUCCESS, Acquire());
    DrivePins();
    const auto captured =
        std::array<uint16_t, 3>{snapshots[0], snapshots[1], snapshots[2]};
    refused = &GPIOC->MODER;
    held_bits = 3U << 2U;
    EXPECT_EQ(NX_ERROR_IO, Release());
    EXPECT_TRUE(receipt.captured);
    EXPECT_NE(0U, receipt.clocks_added);
    EXPECT_EQ(0U, restores);
    EXPECT_EQ(captured, (std::array<uint16_t, 3>{snapshots[0], snapshots[1],
                                                 snapshots[2]}));
    EXPECT_EQ(2U << 2U, GPIOC->MODER & (3U << 2U));
    refused = nullptr;
    require_all_withdrawn = true;
    EXPECT_EQ(NX_SUCCESS, Release());
}

TEST_F(PlatformCold, RestoreReadbackFailureRetainsOnlyTheUnfinishedPinRetry) {
    ASSERT_EQ(NX_SUCCESS, Acquire());
    DrivePins();
    refused_restore_gpio = GPIOB;
    refused_restore_pin = 6U;
    EXPECT_EQ(NX_ERROR_IO, Release());
    EXPECT_TRUE(receipt.captured);
    EXPECT_NE(0U, receipt.clocks_added);
    EXPECT_EQ(NX_ERROR_STATE, Acquire());
    const auto completed = restore_counts;
    refused_restore_gpio = nullptr;
    EXPECT_EQ(NX_SUCCESS, Release());
    EXPECT_EQ(completed[0] + 1U, restore_counts[0]);
    for (size_t i = 1U; i < restore_counts.size(); ++i) {
        if (completed[i] != 0U) {
            EXPECT_EQ(completed[i], restore_counts[i]);
        } else {
            EXPECT_LE(restore_counts[i], 1U);
        }
    }
    EXPECT_FALSE(receipt.captured);
    EXPECT_EQ(0U, receipt.clocks_added);
}

TEST_F(PlatformCold, FinalGpioGateRefusalRetainsOnlyClockCleanup) {
    ASSERT_EQ(NX_SUCCESS, Acquire());
    DrivePins();
    refused = &RCC->AHB1ENR;
    held_bits = RCC_AHB1ENR_GPIOBEN;
    EXPECT_EQ(NX_ERROR_IO, Release());
    EXPECT_FALSE(receipt.captured);
    EXPECT_NE(0U, receipt.clocks_added);
    const auto completed = restore_counts;
    refused = nullptr;
    EXPECT_EQ(NX_SUCCESS, Release());
    EXPECT_EQ(completed, restore_counts);
    EXPECT_EQ(0U, receipt.clocks_added);
}

TEST_F(PlatformCold,
       DmaAcquirePreservesSharedGatesAndDoesNotSnapshotThemTwice) {
    RCC->AHB1ENR = RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_DMA1EN;
    uint32_t added = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_platform_dma_acquire(
                              RCC_AHB1ENR_DMA1EN | RCC_AHB1ENR_DMA2EN, &added));
    EXPECT_EQ(RCC_AHB1ENR_DMA2EN, added);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_platform_dma_release(&added));
    EXPECT_EQ(0U, added);
    EXPECT_EQ(RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_DMA1EN, RCC->AHB1ENR);
}

TEST_F(PlatformCold, DmaGateAcquireRefusalMustFailBeforePublisherPreparation) {
    RCC->AHB1ENR = RCC_AHB1ENR_GPIOAEN;
    refused = &RCC->AHB1ENR;
    held_bits = RCC_AHB1ENR_DMA1EN;
    uint32_t added = 0U;
    EXPECT_EQ(NX_ERROR_IO,
              nx_stm32_platform_dma_acquire(RCC_AHB1ENR_DMA1EN, &added));
    EXPECT_EQ(NX_SUCCESS, nx_stm32_platform_dma_release(&added));
    EXPECT_EQ(RCC_AHB1ENR_GPIOAEN, RCC->AHB1ENR);
    EXPECT_EQ(0U, added);
}

TEST_F(PlatformCold, DmaGateReleaseRefusalKeepsItsMarkerForRetry) {
    uint32_t added = 0U;
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32_platform_dma_acquire(RCC_AHB1ENR_DMA1EN, &added));
    refused = &RCC->AHB1ENR;
    held_bits = RCC_AHB1ENR_DMA1EN;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_platform_dma_release(&added));
    EXPECT_EQ(RCC_AHB1ENR_DMA1EN, added);
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32_platform_dma_acquire(RCC_AHB1ENR_DMA1EN, &added));
    refused = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_platform_dma_release(&added));
    EXPECT_EQ(0U, added);
    EXPECT_EQ(0U, RCC->AHB1ENR);
}

TEST_F(PlatformCold, SyscfgGatePreservesPreviousOwnerAndRetriesRefusedRelease) {
    RCC->APB2ENR = RCC_APB2ENR_USART1EN | RCC_APB2ENR_SYSCFGEN;
    uint32_t added = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_platform_syscfg_acquire(&added));
    EXPECT_EQ(0U, added);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_platform_syscfg_release(&added));
    EXPECT_EQ(RCC_APB2ENR_USART1EN | RCC_APB2ENR_SYSCFGEN, RCC->APB2ENR);
    RCC->APB2ENR &= ~(uint32_t)RCC_APB2ENR_SYSCFGEN;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_platform_syscfg_acquire(&added));
    refused = &RCC->APB2ENR;
    held_bits = RCC_APB2ENR_SYSCFGEN;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_platform_syscfg_release(&added));
    EXPECT_EQ(RCC_APB2ENR_SYSCFGEN, added);
    refused = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_platform_syscfg_release(&added));
    EXPECT_EQ(RCC_APB2ENR_USART1EN, RCC->APB2ENR);
}

extern "C" bool nx_arch_is_privileged(void) {
    return privileged;
}
extern "C" bool nx_arch_in_isr(void) {
    return g_gd32_model_isr;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return g_gd32_model_mask != 0U;
}
extern "C" void nx_arch_dsb(void) {
}
extern "C" void nx_arch_dmb(void) {
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    const nx_arch_irq_state_t previous{g_gd32_model_mask};
    g_gd32_model_mask = 1U;
    return previous;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    g_gd32_model_mask = previous.value;
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    return 100U;
}
extern "C" void nx_platform_cold_model_write(volatile uint32_t* reg,
                                             uint32_t value) {
    ++writes;
    if (reg == refused)
        value = (value & ~held_bits) | (*reg & held_bits);
    *reg = value;
}
extern "C" uint16_t nx_stm32_pin_capture(const GPIO_TypeDef* gpio,
                                         uint8_t pin) {
    ++captures;
    return nx_platform_actual_pin_capture(gpio, pin);
}
extern "C" void nx_stm32_pin_restore(GPIO_TypeDef* gpio, uint8_t pin,
                                     uint16_t saved) {
    ++restores;
    if (require_all_withdrawn) {
        for (size_t i = 0U; i < selected_pin.size(); ++i) {
            const auto mode =
                (selected_gpio[i]->MODER >> (selected_pin[i] * 2U)) & 3U;
            EXPECT_TRUE(mode == 0U || mode == 3U);
        }
    }
    for (size_t i = 0U; i < selected_pin.size(); ++i) {
        if (selected_gpio[i] == gpio && selected_pin[i] == pin)
            ++restore_counts[i];
    }
    if (gpio == refused_restore_gpio && pin == refused_restore_pin)
        return;
    nx_platform_actual_pin_restore(gpio, pin, saved);
    const uint32_t mask = 1U << pin;
    gpio->ODR =
        (gpio->ODR & ~mask) |
        ((static_cast<uint32_t>(saved) & (1U << 11U)) != 0U ? mask : 0U);
}
