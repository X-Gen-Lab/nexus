/**
 * \file            stm32_dac_factory_test.cpp
 * \brief           Actual DAC factory, Platform leases and failure recovery
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-08
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"
#include "stm32f407_provider.h"
nx_stm32_system_t g_nx_stm32_system;
NVIC_Type g_f4_model_nvic;
SCB_Type g_f4_model_scb;
SysTick_Type g_f4_model_systick;
uint32_t g_f4_model_mask;
bool g_f4_model_isr;
}
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
bool refuse_disable;
bool refuse_restore;
unsigned starts;
unsigned stops;
nx_time_us_t now;
constexpr uint32_t original_gate = RCC_AHB1ENR_GPIODEN;
constexpr uint32_t original_pins = 0x55555055U;

class DacFactory : public testing::Test {
  protected:
    void* mapping = MAP_FAILED;
    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(0x40000000U), 0x80000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(0x40000000U));
        g_f4_model_nvic = {};
        g_f4_model_scb = {};
        g_f4_model_systick = {};
        g_f4_model_mask = 0U;
        g_f4_model_isr = false;
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = RCC;
        refuse_disable = refuse_restore = false;
        starts = stops = 0U;
        now = 100U;
        RCC->AHB1ENR = original_gate;
        GPIOA->MODER = original_pins;
    }
    void TearDown() override {
        refuse_disable = refuse_restore = false;
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
            EXPECT_EQ(0, munmap(mapping, 0x80000U));
        }
    }
    void Start() {
        const auto result = nx_platform_start();
        ASSERT_EQ(NX_SUCCESS, result.primary);
        ASSERT_EQ(NX_SUCCESS, result.cleanup);
        ASSERT_EQ(0U, result.remaining_effects);
    }
    void Restored() {
        EXPECT_EQ(original_gate, RCC->AHB1ENR);
        EXPECT_EQ(0U, RCC->APB1ENR);
        EXPECT_EQ(original_pins, GPIOA->MODER);
        EXPECT_EQ(0U, DAC->CR & (DAC_CR_EN1 | DAC_CR_EN2));
    }
};
} /* namespace */

TEST_F(DacFactory, PureTypedLookupAndActualProductionCallsRestorePins) {
    const auto* port = nx_factory_dac_ref(NX_DAC_REF_OUTPUT);
    ASSERT_NE(nullptr, port);
    EXPECT_EQ(0U, starts);
    Restored();
    Start();
    nx_dac_info_t info{};
    ASSERT_EQ(NX_SUCCESS, nx_dac_port_info(port, &info));
    EXPECT_EQ(NX_TEST_DAC_MAX, info.maximum_code);
    EXPECT_EQ(3U, info.channel_mask);
    EXPECT_EQ(0x55555f55U, GPIOA->MODER);
    ASSERT_EQ(NX_SUCCESS, nx_dac_port_write_pair(port, 123U, 231U));
    ASSERT_EQ(NX_SUCCESS, nx_dac_port_start(port, 0U, 1000U));
    nx_dac_state_t output{};
    ASSERT_EQ(NX_SUCCESS, nx_dac_port_state(port, &output));
    EXPECT_EQ(123U * NX_TEST_DAC_SCALE, output.digital_output12[0]);
    EXPECT_EQ(231U * NX_TEST_DAC_SCALE, output.digital_output12[1]);
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(1U, starts);
    EXPECT_EQ(1U, stops);
    Restored();
}

TEST_F(DacFactory,
       FailedOutputDisableRetainsPinsSystemAndCleanupResponsibility) {
    Start();
    const auto* port = nx_factory_dac_ref(NX_DAC_REF_OUTPUT);
    ASSERT_EQ(NX_SUCCESS, nx_dac_port_start(port, 0U, 1000U));
    refuse_disable = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_platform_stop());
    EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_DACEN);
    EXPECT_EQ(0x55555f55U, GPIOA->MODER);
    EXPECT_EQ(0U, stops);
    EXPECT_EQ(NX_ERROR_STATE, nx_platform_start().primary);
    refuse_disable = false;
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
    Restored();
}

TEST_F(DacFactory, FailedPadRestoreKeepsOnlyTheUnreleasedLease) {
    Start();
    refuse_restore = true;
    EXPECT_EQ(NX_ERROR_IO, nx_platform_stop());
    EXPECT_EQ(0U, RCC->APB1ENR & RCC_APB1ENR_DACEN);
    EXPECT_NE(0U, RCC->AHB1ENR & RCC_AHB1ENR_GPIOAEN);
    EXPECT_EQ(0U, stops);
    refuse_restore = false;
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(1U, stops);
    Restored();
}

extern "C" bool nx_arch_is_privileged(void) {
    return true;
}
extern "C" bool nx_arch_in_isr(void) {
    return g_f4_model_isr;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return g_f4_model_mask != 0U;
}
extern "C" void nx_arch_dsb(void) {
}
extern "C" void nx_arch_dmb(void) {
}
extern "C" void nx_arch_isb(void) {
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    const nx_arch_irq_state_t previous{g_f4_model_mask};
    g_f4_model_mask = 1U;
    return previous;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    g_f4_model_mask = previous.value;
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    return ++now;
}
extern "C" void nx_dac_model_write(volatile uint32_t* reg, uint32_t value) {
    if (reg == &GPIOA->MODER && refuse_restore && value != *reg) {
        return;
    }
    if (reg == &DAC->CR && refuse_disable) {
        value |= DAC->CR & static_cast<uint32_t>(DAC_CR_EN1 | DAC_CR_EN2);
    }
    *reg = value;
    if (reg == &DAC->DHR12LD || reg == &DAC->DHR12RD || reg == &DAC->DHR8RD) {
        const unsigned shift = reg == &DAC->DHR12LD ? 4U : 0U;
        DAC->DOR1 = reg == &DAC->DHR8RD ? (value & 0xffU) << 4U
                                        : (value >> shift) & 0xfffU;
        DAC->DOR2 = reg == &DAC->DHR8RD ? ((value >> 8U) & 0xffU) << 4U
                                        : (value >> (16U + shift)) & 0xfffU;
    }
}
extern "C" void nx_platform_cold_model_write(volatile uint32_t* reg,
                                             uint32_t value) {
    nx_dac_model_write(reg, value);
}
extern "C" nx_result_t nx_stm32_validate_identity(uint32_t) {
    return NX_SUCCESS;
}
extern "C" nx_result_t nx_stm32_system_preflight(const nx_stm32_system_t*) {
    return NX_SUCCESS;
}
extern "C" nx_stm32_start_result_t nx_stm32_system_start(nx_stm32_system_t*,
                                                         uint32_t) {
    ++starts;
    return {0, 0, 0U};
}
extern "C" int nx_stm32_system_stop(nx_stm32_system_t*, uint32_t) {
    ++stops;
    return 0;
}
