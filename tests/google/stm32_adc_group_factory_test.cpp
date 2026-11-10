/**
 * \file            stm32_adc_group_factory_test.cpp
 * \brief           Real generated ADC group, pin leases and joined Platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"
#include "stm32f407_adc_group.h"
#include "stm32f407_clock.h"
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
nx_time_us_t now;
unsigned starts, stops;
bool hold_dma;
class AdcGroupFactory : public testing::Test {
  protected:
    void* mmio = MAP_FAILED;
    void* ram = MAP_FAILED;
    void SetUp() override {
        mmio = mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000U,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_NE(mmio, MAP_FAILED);
        ram = mmap(reinterpret_cast<void*>(UINT32_C(0x20000000)), 0x20000U,
                   PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_NE(ram, MAP_FAILED);
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = RCC;
        g_f4_model_nvic = {};
        g_f4_model_scb = {};
        g_f4_model_systick = {};
        g_f4_model_mask = 0U;
        g_f4_model_isr = false;
        now = 100U;
        starts = stops = 0U;
        hold_dma = false;
        RCC->AHB1ENR = RCC_AHB1ENR_GPIODEN;
    }
    void TearDown() override {
        hold_dma = false;
        if (mmio != MAP_FAILED) {
            EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
            EXPECT_EQ(munmap(mmio, 0x80000U), 0);
        }
        if (ram != MAP_FAILED) {
            EXPECT_EQ(munmap(ram, 0x20000U), 0);
        }
    }
    void Start() {
        const nx_platform_start_result_t result = nx_platform_start();
        ASSERT_EQ(result.primary, NX_SUCCESS);
        ASSERT_EQ(result.cleanup, NX_SUCCESS);
        ASSERT_EQ(result.remaining_effects, 0U);
    }
    const nx_adc_group_port_t* Port() {
        return nx_factory_adc_group_ref(NX_ADC_GROUP_REF_GROUP);
    }
};

TEST_F(AdcGroupFactory, FactoryLookupIsPureBeforeColdStart) {
    const nx_adc_group_port_t* first = Port();
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first, Port());
    EXPECT_EQ(starts, 0U);
    EXPECT_EQ(RCC->APB2ENR, 0U);
    EXPECT_EQ(GPIOA->MODER, 0U);
    nx_adc_group_info_t info{};
    EXPECT_EQ(nx_adc_group_info(first, &info), NX_ERROR_STATE);
}
TEST_F(AdcGroupFactory, StartConfiguresSelectedPadsButDoesNotSample) {
    Start();
    EXPECT_EQ(starts, 1U);
    EXPECT_EQ(GPIOA->MODER & 255U, 255U);
    EXPECT_EQ(ADC1->CR2 & ADC_CR2_ADON, 0U);
    EXPECT_EQ(ADC2->CR2 & ADC_CR2_ADON, 0U);
    EXPECT_EQ(ADC3->CR2 & ADC_CR2_ADON, 0U);
    EXPECT_EQ(DMA2_Stream0->CR & DMA_SxCR_EN, 0U);
    nx_adc_group_info_t info{};
    ASSERT_EQ(nx_adc_group_info(Port(), &info), NX_SUCCESS);
    EXPECT_EQ(info.format.channel_count, 4U);
    EXPECT_EQ(info.converters[0], 1U);
    EXPECT_EQ(info.converters[1], 2U);
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    EXPECT_EQ(stops, 1U);
    EXPECT_EQ(RCC->AHB1ENR, RCC_AHB1ENR_GPIODEN);
    EXPECT_EQ(RCC->APB2ENR, 0U);
    EXPECT_EQ(GPIOA->MODER, 0U);
}
TEST_F(AdcGroupFactory, PlatformCloseJoinsTheActualSelectedVectorAndRequest) {
    Start();
    nx_adc_group_request_t request{};
    nx_request_initialize(&request.base);
    auto* data = static_cast<uint16_t*>(ram);
    ASSERT_EQ(nx_adc_group_prepare(&request, data, 8U, 1U, 10000U), NX_SUCCESS);
    ASSERT_EQ(nx_adc_group_submit(Port(), &request), NX_SUCCESS);
    now += 20U;
    ASSERT_EQ(nx_adc_group_service(Port()), NX_SUCCESS);
    EXPECT_NE(NVIC_GetEnableIRQ(ADC_IRQn), 0U);
#if defined(NX_ADC_GROUP_FACTORY_DMA)
    EXPECT_NE(NVIC_GetEnableIRQ(DMA2_Stream0_IRQn), 0U);
    hold_dma = true;
    EXPECT_NE(nx_platform_stop(), NX_SUCCESS);
    EXPECT_EQ(stops, 0U);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_NE(DMA2_Stream0->M0AR, 0U);
    now += 1001U;
    EXPECT_NE(nx_platform_stop(), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    hold_dma = false;
#endif
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(stops, 1U);
    EXPECT_EQ(NVIC_GetEnableIRQ(ADC_IRQn), 0U);
    EXPECT_EQ(NVIC_GetPendingIRQ(ADC_IRQn), 0U);
    EXPECT_EQ(NVIC_GetEnableIRQ(DMA2_Stream0_IRQn), 0U);
    EXPECT_EQ(DMA2_Stream0->M0AR, 0U);
}
} /* namespace */

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
    nx_arch_irq_state_t previous{g_f4_model_mask};
    g_f4_model_mask = 1U;
    return previous;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    g_f4_model_mask = previous.value;
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    return now;
}
extern "C" void nx_platform_cold_model_write(volatile uint32_t* target,
                                             uint32_t value) {
    *target = value;
}
extern "C" void nx_adc_group_factory_ack(void* raw, uint32_t flags) {
    auto* dma = static_cast<DMA_TypeDef*>(raw);
    dma->LIFCR = flags;
    dma->LISR &= ~flags;
}
extern "C" void nx_adc_group_factory_disable(void* raw, uint32_t mask) {
    auto* dma = static_cast<DMA_Stream_TypeDef*>(raw);
    dma->CR &= ~(mask & ~(hold_dma ? uint32_t(DMA_SxCR_EN) : 0U));
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
    g_nx_stm32_system.started = true;
    g_nx_stm32_system.core_hz = NEXUS_CORE_HZ;
    ++g_nx_stm32_system.time_epoch;
    RCC->CR = RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY;
    RCC->PLLCFGR = NEXUS_CLOCK_PLLCFGR;
    RCC->CFGR = NEXUS_CLOCK_CFGR | RCC_CFGR_SW_PLL | RCC_CFGR_SWS_PLL;
    return {0, 0, 0U};
}
extern "C" int nx_stm32_system_stop(nx_stm32_system_t*, uint32_t) {
    ++stops;
    g_nx_stm32_system.started = false;
    return 0;
}
