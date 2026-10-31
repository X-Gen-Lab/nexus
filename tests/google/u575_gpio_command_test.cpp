/**
 * \file            u575_gpio_command_test.cpp
 * \brief           Actual U575 HAL GPIO commands and owned latch admission
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-30
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32u575_provider.h"
#include "stm32u5xx_hal_gpio.h"
#include "u575_gpio_command_model.h"
#include <array>
#include <gtest/gtest.h>
#include <vector>
namespace {
/* Both halves overlap, so this cannot be an admitted nonempty command. */
constexpr uint32_t unobserved = 0xa5a5a5a5U;
struct Command {
    GPIO_TypeDef* registers;
    uint32_t value;
    uint32_t modes;
};
std::vector<Command> commands;
GPIO_TypeDef* refused_latch;
std::array<GPIO_TypeDef*, 8> Ports() {
    return {&g_u575_model.gpio,          &g_u575_model.gpioa,
            &g_u575_model.extra_gpio[0], &g_u575_model.extra_gpio[1],
            &g_u575_model.extra_gpio[2], &g_u575_model.extra_gpio[3],
            &g_u575_model.extra_gpio[4], &g_u575_model.extra_gpio[5]};
}
void Observe(GPIO_TypeDef* registers) {
    uint32_t command = registers->BSRR;
    if (command == unobserved)
        return;
    commands.push_back({registers, command, registers->MODER});
    /* This RAM latch records a completed command between separate observations;
     * resetting its sentinel does not replace or intercept the vendor store. */
    registers->BSRR = unobserved;
    if (registers != refused_latch) {
        registers->ODR |= command & 0xffffU;
        registers->ODR &= ~(command >> 16U);
    }
}
} /* namespace */
/** \brief Apply external latch facts when the provider observes its ODR. */
extern "C" uint32_t nx_u575_gpio_command_read(volatile uint32_t* reg) {
    for (auto* registers : Ports())
        if (reg == &registers->ODR)
            Observe(registers);
    return nx_u575_model_read(reg);
}
TEST(U575OfficialGpioCommand, ResetThenSetUsesTheSelectedPortAndOnePackedWord) {
    GPIO_TypeDef selected{};
    GPIO_TypeDef other{};
    selected.BSRR = unobserved;
    selected.ODR = 0x2468U;
    selected.LCKR = 0x1357U;
    selected.BRR = 0x4321U;
    other.BSRR = 0x89abcdefU;
    HAL_GPIO_WriteMultipleStatePin(&selected, 0x8000U, 3U);
    EXPECT_EQ(0x80000003U, selected.BSRR);
    EXPECT_EQ(0x2468U, selected.ODR);
    EXPECT_EQ(0x1357U, selected.LCKR);
    EXPECT_EQ(0x4321U, selected.BRR);
    EXPECT_EQ(0x89abcdefU, other.BSRR);
}
class U575GpioDefaultCommand : public testing::Test {
  protected:
    nx_stm32u575_gpio_state_t state{};
    const nx_gpio_port_t face{&nx_stm32u575_gpio_ops, &state};
    void SetUp() override {
        nx_u575_model_reset();
        commands.clear();
        refused_latch = nullptr;
        for (auto* registers : Ports())
            registers->BSRR = unobserved;
        state = {&g_u575_model.gpio, 0x81U, true, true, true};
    }
    void RefuseLatch(GPIO_TypeDef* registers) {
        refused_latch = registers;
        /* The unchanged pre-refactor command path uses the existing MMIO
         * boundary; the real default HAL path is observed independently. */
        g_u575_model.ignore_write =
            registers == nullptr ? nullptr : &registers->BSRR;
    }
};
TEST_F(U575GpioDefaultCommand,
       PublicWritePreservesPackingAndControllerIdentity) {
    ASSERT_EQ(NX_SUCCESS, nx_gpio_port_write(&face, 0x80U, 1U));
    Observe(state.registers);
    ASSERT_EQ(1U, commands.size());
    EXPECT_EQ(state.registers, commands[0].registers);
    EXPECT_EQ(0x00010080U, commands[0].value);
    EXPECT_EQ(0x80U, state.registers->ODR);
    EXPECT_EQ(unobserved, g_u575_model.gpioa.BSRR);
}
TEST_F(U575GpioDefaultCommand, InvalidMasksAndPrivilegeRejectBeforeAnyCommand) {
    EXPECT_EQ(NX_ERROR_INVALID, nx_gpio_port_write(&face, 1U, 1U));
    EXPECT_EQ(NX_ERROR_PERMISSION, nx_gpio_port_write(&face, 2U, 0U));
    g_u575_model.privileged = false;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_gpio_port_write(&face, 0x80U, 0U));
    EXPECT_EQ(unobserved, state.registers->BSRR);
    EXPECT_TRUE(commands.empty());
    EXPECT_EQ(0U, g_u575_model.writes);
}
TEST_F(U575GpioDefaultCommand,
       EmptyPrivateCommandDoesNotCallTheNonemptyHalLeaf) {
    /* Nexus already accepts empty masks. The private HAL adapter additionally
     * avoids calling an official function whose contract requires nonempty. */
    EXPECT_EQ(NX_SUCCESS, nx_gpio_port_write(&face, 0U, 0U));
    EXPECT_EQ(unobserved, state.registers->BSRR);
    Observe(state.registers);
    EXPECT_TRUE(commands.empty());
}
TEST_F(U575GpioDefaultCommand, InitializationObservesPreloadBeforeOutputMode) {
    state.initialized = state.retained = false;
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_gpio_initialize(&state, 0x80U));
    ASSERT_EQ(1U, commands.size());
    EXPECT_EQ(0x00010080U, commands[0].value);
    EXPECT_EQ(0U, commands[0].modes);
    EXPECT_EQ(0x80U, state.registers->ODR);
    EXPECT_EQ(1U, state.registers->MODER & 3U);
    EXPECT_EQ(1U, (state.registers->MODER >> 14U) & 3U);
    EXPECT_TRUE(state.initialized);
}
TEST_F(U575GpioDefaultCommand,
       RefusedPreloadRetainsCleanupWithoutExposingMode) {
    state.initialized = state.retained = false;
    state.registers->ODR = state.mask;
    RefuseLatch(state.registers);
    EXPECT_EQ(NX_ERROR_IO, nx_stm32u575_gpio_initialize(&state, 0U));
    EXPECT_FALSE(state.initialized);
    EXPECT_TRUE(state.retained);
    EXPECT_EQ(0U, state.registers->MODER);
    EXPECT_EQ(NX_ERROR_IO, nx_stm32u575_gpio_stop(&state, 0U));
    EXPECT_TRUE(state.retained);
    RefuseLatch(nullptr);
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_gpio_stop(&state, 0U));
    EXPECT_EQ(0U, state.registers->ODR);
    EXPECT_FALSE(state.retained);
}
TEST_F(U575GpioDefaultCommand, StopObservesInactiveLatchBeforeReleasingOwner) {
    state.registers->ODR = state.mask;
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_gpio_stop(&state, 1U));
    ASSERT_EQ(1U, commands.size());
    EXPECT_EQ(0x00800001U, commands[0].value);
    EXPECT_EQ(1U, state.registers->ODR);
    EXPECT_FALSE(state.initialized);
    EXPECT_FALSE(state.retained);
}
TEST_F(U575GpioDefaultCommand,
       RefusedStopLatchKeepsItsOwnerUntilObservedRetry) {
    state.registers->ODR = state.mask;
    RefuseLatch(state.registers);
    EXPECT_EQ(NX_ERROR_IO, nx_stm32u575_gpio_stop(&state, 0U));
    EXPECT_TRUE(state.initialized);
    EXPECT_TRUE(state.retained);
    RefuseLatch(nullptr);
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_gpio_stop(&state, 0U));
    EXPECT_FALSE(state.retained);
}
TEST_F(U575GpioDefaultCommand,
       BothSerialPreloadsAreObservedBeforeAlternateMode) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_usart1_pins_prepare());
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_usart2_pins_prepare());
    ASSERT_EQ(2U, commands.size());
    EXPECT_EQ(&g_u575_model.gpioa, commands[0].registers);
    EXPECT_EQ(1U << 9U, commands[0].value);
    EXPECT_EQ(0U, commands[0].modes & (3U << 18U));
    EXPECT_EQ(&g_u575_model.gpioa, commands[1].registers);
    EXPECT_EQ(1U << 2U, commands[1].value);
    EXPECT_EQ(0U, commands[1].modes & (3U << 4U));
    EXPECT_EQ(2U, (g_u575_model.gpioa.MODER >> 18U) & 3U);
    EXPECT_EQ(2U, (g_u575_model.gpioa.MODER >> 4U) & 3U);
}
TEST_F(U575GpioDefaultCommand, RefusedSerialLatchCannotExposeAlternateOutput) {
    RefuseLatch(&g_u575_model.gpioa);
    EXPECT_EQ(NX_ERROR_IO, nx_stm32u575_usart1_pins_prepare());
    EXPECT_EQ(0U, g_u575_model.gpioa.MODER);
}
TEST_F(U575GpioDefaultCommand,
       ToggleUsesTheObservedLatchAndSameDefaultCommand) {
    state.registers->ODR = 1U;
    ASSERT_EQ(NX_SUCCESS, nx_gpio_port_toggle(&face, state.mask));
    Observe(state.registers);
    ASSERT_EQ(1U, commands.size());
    EXPECT_EQ(0x00010080U, commands[0].value);
    EXPECT_EQ(0x80U, state.registers->ODR);
}
