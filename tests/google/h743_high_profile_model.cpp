/**
 * \file            h743_high_profile_model.cpp
 * \brief           Register-derived clock domains, not selected mode flags
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_high_profile_model.h"
#include "stm32h743_provider.h"
extern "C" {
nx_h743_high_model_behavior_t g_h743_high_behavior;
nx_h743_high_model_clocks_t nx_h743_high_model_clocks(void) {
    const auto& r = g_h743_model.rcc;
    const uint32_t hsi_input = g_h743_high_behavior.hsi_input_hz != 0U
                                   ? g_h743_high_behavior.hsi_input_hz
                                   : 64000000U;
    const uint32_t hsi =
        hsi_input >> ((r.CR & uint32_t(RCC_CR_HSIDIV)) >> RCC_CR_HSIDIV_Pos);
    uint32_t source = hsi;
    if ((r.CFGR & RCC_CFGR_SWS) == RCC_CFGR_SWS_PLL1) {
        const uint32_t m = (r.PLLCKSELR & uint32_t(RCC_PLLCKSELR_DIVM1)) >>
                           RCC_PLLCKSELR_DIVM1_Pos;
        const uint32_t n = (r.PLL1DIVR & uint32_t(RCC_PLL1DIVR_N1)) + 1U;
        const uint32_t p =
            ((r.PLL1DIVR & uint32_t(RCC_PLL1DIVR_P1)) >> RCC_PLL1DIVR_P1_Pos) +
            1U;
        const uint32_t scale = g_h743_high_behavior.pll_scale_ppm != 0U
                                   ? g_h743_high_behavior.pll_scale_ppm
                                   : 1000000U;
        source = m != 0U
                     ? uint32_t(uint64_t(hsi) * n * scale / m / p / 1000000U)
                     : 0U;
    }
    static const uint16_t ahb_divisor[16] = {
        1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 2U, 4U, 8U, 16U, 64U, 128U, 256U, 512U};
    static const uint8_t apb_divisor[8] = {1U, 1U, 1U, 1U, 2U, 4U, 8U, 16U};
    nx_h743_high_model_clocks_t c{};
    const uint32_t effective_d1 = g_h743_clock_behavior.delayed_hpre
                                      ? g_h743_clock_behavior.effective_d1cfgr
                                      : r.D1CFGR;
    c.cpu_hz = source / ahb_divisor[(r.D1CFGR & uint32_t(RCC_D1CFGR_D1CPRE)) >>
                                    RCC_D1CFGR_D1CPRE_Pos];
    c.hclk_hz =
        c.cpu_hz / ahb_divisor[(effective_d1 & uint32_t(RCC_D1CFGR_HPRE)) >>
                               RCC_D1CFGR_HPRE_Pos];
    const uint32_t encodings[4] = {
        (r.D2CFGR & uint32_t(RCC_D2CFGR_D2PPRE1)) >> RCC_D2CFGR_D2PPRE1_Pos,
        (r.D2CFGR & uint32_t(RCC_D2CFGR_D2PPRE2)) >> RCC_D2CFGR_D2PPRE2_Pos,
        (r.D1CFGR & uint32_t(RCC_D1CFGR_D1PPRE)) >> RCC_D1CFGR_D1PPRE_Pos,
        (r.D3CFGR & uint32_t(RCC_D3CFGR_D3PPRE)) >> RCC_D3CFGR_D3PPRE_Pos};
    for (unsigned i = 0U; i < 4U; ++i)
        c.pclk_hz[i] = c.hclk_hz / apb_divisor[encodings[i]];
    const uint32_t timer_multiplier = apb_divisor[encodings[0]] == 1U ? 1U : 2U;
    c.tim2_hz = c.tim3_hz =
        (r.CFGR & RCC_CFGR_TIMPRE) == 0U ? c.pclk_hz[0] * timer_multiplier : 0U;
    const uint32_t uart = (r.D2CCIP2R & uint32_t(RCC_D2CCIP2R_USART28SEL)) >>
                          RCC_D2CCIP2R_USART28SEL_Pos;
    c.usart3_hz = uart == 0U ? c.pclk_hz[0] : (uart == 3U ? hsi : 0U);
    const uint32_t i2c = (r.D2CCIP2R & uint32_t(RCC_D2CCIP2R_I2C123SEL)) >>
                         RCC_D2CCIP2R_I2C123SEL_Pos;
    c.i2c1_hz = i2c == 0U ? c.pclk_hz[0] : (i2c == 2U ? hsi : 0U);
    c.spi4_hz = (r.D2CCIP1R & RCC_D2CCIP1R_SPI45SEL) == 0U ? c.pclk_hz[1] : 0U;
    if ((r.D3CCIPR & RCC_D3CCIPR_ADCSEL) == RCC_D3CCIPR_ADCSEL_1 &&
        (r.D1CCIPR & RCC_D1CCIPR_CKPERSEL) == 0U)
        c.adc12_input_hz = hsi;
    /* The joint owner selects an input, not a running ADC or its divider.
     * Only the selected provider may configure/enable conversion clocks. */
    if (g_h743_model.adc == nullptr ||
        (g_h743_model.adc->CR & (ADC_CR_ADEN | ADC_CR_ADCAL)) == 0U)
        return c;
    if ((g_h743_model.adc_common.CCR & uint32_t(ADC_CCR_CKMODE)) != 0U) {
        const uint32_t mode =
            (g_h743_model.adc_common.CCR & uint32_t(ADC_CCR_CKMODE)) >>
            ADC_CCR_CKMODE_Pos;
        c.adc_hz = c.hclk_hz / (mode == 1U ? 1U : (mode == 2U ? 2U : 4U));
    } else if (c.adc12_input_hz != 0U) {
        static const uint16_t dividers[12] = {1U,  2U,  4U,  6U,  8U,   10U,
                                              12U, 16U, 32U, 64U, 128U, 256U};
        const uint32_t encoding =
            (g_h743_model.adc_common.CCR & uint32_t(ADC_CCR_PRESC)) >>
            ADC_CCR_PRESC_Pos;
        c.adc_hz = encoding < 12U ? c.adc12_input_hz / dividers[encoding] : 0U;
    }
    return c;
}
void nx_h743_high_model_advance_hclk(uint32_t cycles) {
    const auto c = nx_h743_high_model_clocks();
    auto& timer = g_h743_model.timer;
    if ((g_h743_model.rcc.APB1LENR & RCC_APB1LENR_TIM2EN) == 0U ||
        (timer.CR1 & TIM_CR1_CEN) == 0U || c.hclk_hz == 0U)
        return;
    const uint64_t input = uint64_t(cycles) * c.tim2_hz / c.hclk_hz +
                           g_h743_high_behavior.timer_input_remainder;
    const uint32_t divider = g_h743_high_behavior.timer_active_prescaler + 1U;
    const uint64_t count = uint64_t(timer.CNT) + input / divider;
    timer.CNT = uint32_t(count);
    if (count > UINT32_MAX) {
        timer.SR |= TIM_SR_UIF;
        if ((timer.DIER & TIM_DIER_UIE) != 0U)
            g_h743_model.irq_pending[TIM2_IRQn] = true;
    }
    g_h743_high_behavior.timer_input_remainder = uint32_t(input % divider);
}
void nx_h743_high_model_write(volatile uint32_t* reg, uint32_t value) {
    const auto& r = g_h743_model.rcc;
    const uint32_t previous = *reg;
    if ((reg == &r.D1CCIPR || reg == &r.D2CCIP2R || reg == &r.D3CCIPR) &&
        (r.APB1LENR != 0U || r.APB2ENR != 0U || r.AHB1ENR != 0U))
        ++g_h743_high_behavior.mux_writes_with_consumers;
    if (reg == &g_h743_model.flash.KEYR1 || reg == &g_h743_model.flash.KEYR2) {
        const auto c = nx_h743_high_model_clocks();
        if ((r.CFGR & RCC_CFGR_SWS) == RCC_CFGR_SWS_PLL1 &&
            ((g_h743_model.flash.ACR &
              (FLASH_ACR_LATENCY | FLASH_ACR_WRHIGHFREQ)) !=
                 (FLASH_ACR_LATENCY_2WS | FLASH_ACR_WRHIGHFREQ_1) ||
             (g_h743_model.power.CSR1 &
              (PWR_CSR1_ACTVOS | PWR_CSR1_ACTVOSRDY)) !=
                 (PWR_CSR1_ACTVOS | PWR_CSR1_ACTVOSRDY) ||
             c.hclk_hz > 200000000U))
            ++g_h743_high_behavior.unsafe_flash_unlocks;
    }
    nx_h743_clock_write(reg, value);
    if (reg == &r.D2CCIP2R &&
        (value & g_h743_high_behavior.refused_kernel_field) != 0U &&
        reg != g_h743_model.ignore_write) {
        /* One selector can refuse acquisition while its adjacent field
         * accepts the same software write. Keep the attempted MMIO real. */
        const uint32_t mask = g_h743_high_behavior.refused_kernel_field;
        *reg = (*reg & ~mask) | (previous & mask);
    }
    if (g_h743_model.spi != nullptr && reg == &g_h743_model.spi->CR1 &&
        (value & SPI_CR1_CSTART) != 0U && (*reg & SPI_CR1_CSTART) != 0U) {
        /* Observe the configured divisor at the real start. A successful
         * provider reset deliberately erases CFG1 before synchronous return. */
        g_h743_high_behavior.spi_started_divisor =
            2U << ((g_h743_model.spi->CFG1 & uint32_t(SPI_CFG1_MBR)) >>
                   SPI_CFG1_MBR_Pos);
    }
    if (reg == &g_h743_model.timer.EGR && (value & TIM_EGR_UG) != 0U &&
        reg != g_h743_model.ignore_write) {
        g_h743_high_behavior.timer_active_prescaler = g_h743_model.timer.PSC;
        g_h743_high_behavior.timer_input_remainder = 0U;
    }
    if (g_h743_model.adc != nullptr && reg == &g_h743_model.adc->CR &&
        (value & ADC_CR_ADEN) != 0U &&
        nx_h743_high_model_clocks().adc_hz > 16250000U)
        /* Selected HSI64/4 profile bound, not the V-cut silicon ADC limit. */
        ++g_h743_high_behavior.adc_enables_outside_profile;
}
uint8_t nx_h743_high_model_spi_read8(volatile uint32_t* reg) {
    const uint8_t result = nx_h743_model_spi_read8(reg);
    if (g_h743_model.spi_received == g_h743_model.spi->CR2) {
        const uint32_t divisor =
            2U << ((g_h743_model.spi->CFG1 & uint32_t(SPI_CFG1_MBR)) >>
                   SPI_CFG1_MBR_Pos);
        const uint32_t kernel = nx_h743_high_model_clocks().spi4_hz;
        if (kernel != 0U)
            g_h743_model.spi_tail_until =
                g_h743_model.timer.CNT +
                uint32_t((uint64_t(divisor) * 1000000U + kernel - 1U) / kernel);
    }
    return result;
}
}
