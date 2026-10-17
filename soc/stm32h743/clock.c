/**
 * \file            clock.c
 * \brief           Optional H743 cold HSI/PLL1 clock ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32h743_clock.h"
#ifndef NX_H743_SILICON_ID
#define NX_H743_SILICON_ID() NX_H743_READ(&DBGMCU->IDCODE)
#endif
/** \brief The pinned HAL uses different HSI user-trim layouts by known cut.
 * \note ES0392 Rev15 Table2 identifies Y/W=1003, X=2001 and V=2003.
 *       Locked HAL RCC calibration adjustment selects the legacy 6-bit field
 *       through Y, then the 7-bit field. Never apply this as an admission
 *       threshold to an unknown revision or change a foreign calibration.
 */
static nx_result_t factory_trim(const nx_stm32h743_system_t* s) {
    const uint32_t identity = NX_H743_SILICON_ID();
    if ((identity & 0xfffU) != 0x450U)
        return NX_ERROR_UNSUPPORTED;
    uint32_t mask;
    uint32_t expected;
    switch (identity >> 16U) {
        case 0x1003U:
            mask = 0x0003f000U;
            expected = 0x00020000U;
            break;
        case 0x2001U:
        case 0x2003U:
            mask = RCC_HSICFGR_HSITRIM;
            expected = 0x40U << RCC_HSICFGR_HSITRIM_Pos;
            break;
        default:
            return NX_ERROR_UNSUPPORTED;
    }
    return (NX_H743_READ(&s->rcc->HSICFGR) & mask) == expected ? NX_SUCCESS
                                                               : NX_ERROR_STATE;
}
nx_result_t nx_stm32h743_pll384_preflight(const nx_stm32h743_system_t* s) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (s == NULL || s->rcc == NULL || s->flash == NULL || s->power == NULL ||
        s->timer == NULL || s->core == NULL)
        return NX_ERROR_INVALID;
    nx_result_t result = factory_trim(s);
    if (result != NX_SUCCESS)
        return result;
    if ((NX_H743_READ(&s->power->D3CR) & (PWR_D3CR_VOS | PWR_D3CR_VOSRDY)) !=
            (PWR_D3CR_VOS_0 | PWR_D3CR_VOSRDY) ||
        (NX_H743_READ(&s->power->CSR1) &
         (PWR_CSR1_ACTVOS | PWR_CSR1_ACTVOSRDY)) !=
            (PWR_CSR1_ACTVOS_0 | PWR_CSR1_ACTVOSRDY) ||
        (NX_H743_READ(&s->flash->ACR) &
         (FLASH_ACR_LATENCY | FLASH_ACR_WRHIGHFREQ)) != FLASH_ACR_LATENCY_7WS)
        return NX_ERROR_STATE;
    result = nx_stm32h743_system_preflight(s);
    if (result != NX_SUCCESS)
        return result;
    /* CTRL is read-to-clear. These side-effect-free counter/pending reads
     * can refute cold reset; they cannot establish ENABLE=0 or past history.
     * Only the real exclusive boot sequence supplies that precondition. */
    if (NX_H743_READ(&SysTick->LOAD) != 0U ||
        NX_H743_READ(&SysTick->VAL) != 0U ||
        (NX_H743_READ(&s->core->ICSR) &
         (SCB_ICSR_PENDSTSET_Msk | SCB_ICSR_PENDSVSET_Msk)) != 0U)
        return NX_ERROR_BUSY;
    /* PLL registers require OFF and not-ready. Matching readable reset
     * configuration is not proof of an old fractional latch's history. */
    if ((NX_H743_READ(&s->rcc->CR) &
         (RCC_CR_PLL1RDY | RCC_CR_PLL2RDY | RCC_CR_PLL3RDY)) != 0U ||
        NX_H743_READ(&s->rcc->PLLCKSELR) != 0x02020200U ||
        NX_H743_READ(&s->rcc->PLLCFGR) != 0x01ff0000U ||
        NX_H743_READ(&s->rcc->PLL1DIVR) != 0x01010280U ||
        NX_H743_READ(&s->rcc->PLL1FRACR) != 0U ||
        NX_H743_READ(&s->rcc->CFGR) != 0U ||
        NX_H743_READ(&s->rcc->CIER) != 0U || !NX_H743_IRQ_AVAILABLE(RCC_IRQn) ||
        (NX_H743_READ(&s->rcc->D1CCIPR) | NX_H743_READ(&s->rcc->D2CCIP1R) |
         NX_H743_READ(&s->rcc->D2CCIP2R) | NX_H743_READ(&s->rcc->D3CCIPR)) !=
            0U)
        return NX_ERROR_BUSY;
    if ((NX_H743_READ(&s->rcc->AHB1ENR) | NX_H743_READ(&s->rcc->AHB3ENR) |
         NX_H743_READ(&s->rcc->AHB4ENR) | NX_H743_READ(&s->rcc->APB1LENR) |
         NX_H743_READ(&s->rcc->APB1HENR) | NX_H743_READ(&s->rcc->APB2ENR) |
         NX_H743_READ(&s->rcc->APB3ENR) | NX_H743_READ(&s->rcc->APB4ENR)) != 0U)
        return NX_ERROR_BUSY;
    uint32_t boot_sram = 0U;
#if defined(NEXUS_MEMORY_SRAM1) && NEXUS_MEMORY_SRAM1 == 1
    boot_sram = RCC_AHB2ENR_SRAM1EN;
#endif
    return (NX_H743_READ(&s->rcc->AHB2ENR) & ~boot_sram) == 0U ? NX_SUCCESS
                                                               : NX_ERROR_BUSY;
}

#define CLOCK_GATE    1U
#define CLOCK_VOS     2U
#define CLOCK_FLASH   4U
#define CLOCK_BUS     8U
#define CLOCK_CONFIG  16U
#define CLOCK_PLL     32U
#define CLOCK_SOURCE  64U
#define CLOCK_READY   128U
#define CLOCK_EFFECTS 255U
#define PLL_ON_MASK   ((uint32_t)(RCC_CR_PLL1ON | RCC_CR_PLL2ON | RCC_CR_PLL3ON))
#define PLL_READY_MASK                                                         \
    ((uint32_t)(RCC_CR_PLL1RDY | RCC_CR_PLL2RDY | RCC_CR_PLL3RDY))
#define PLL_SELECT_MASK                                                        \
    ((uint32_t)(RCC_PLLCKSELR_PLLSRC | RCC_PLLCKSELR_DIVM1 |                   \
                RCC_PLLCKSELR_DIVM2 | RCC_PLLCKSELR_DIVM3))
#define PLL_CONTROL_MASK                                                       \
    ((uint32_t)(RCC_PLLCFGR_PLL1FRACEN | RCC_PLLCFGR_PLL1VCOSEL |              \
                RCC_PLLCFGR_PLL1RGE | RCC_PLLCFGR_DIVP1EN |                    \
                RCC_PLLCFGR_DIVQ1EN | RCC_PLLCFGR_DIVR1EN))
#define PLL_CONTROL_TARGET                                                     \
    ((uint32_t)(RCC_PLLCFGR_PLL1RGE_2 | RCC_PLLCFGR_DIVP1EN))
#define PLL_DIVIDER_MASK                                                       \
    ((uint32_t)(RCC_PLL1DIVR_N1 | RCC_PLL1DIVR_P1 | RCC_PLL1DIVR_Q1 |          \
                RCC_PLL1DIVR_R1))
#define D1_MASK                                                                \
    ((uint32_t)(RCC_D1CFGR_D1CPRE | RCC_D1CFGR_HPRE | RCC_D1CFGR_D1PPRE))
#define D1_TARGET ((uint32_t)(RCC_D1CFGR_HPRE_DIV2 | RCC_D1CFGR_D1PPRE_DIV2))
#define D2_MASK   ((uint32_t)(RCC_D2CFGR_D2PPRE1 | RCC_D2CFGR_D2PPRE2))
#define D2_TARGET                                                              \
    ((uint32_t)(RCC_D2CFGR_D2PPRE1_DIV2 | RCC_D2CFGR_D2PPRE2_DIV2))
#define D3_MASK           ((uint32_t)RCC_D3CFGR_D3PPRE)
#define D3_TARGET         ((uint32_t)RCC_D3CFGR_D3PPRE_DIV2)
#define FLASH_TIMING_MASK ((uint32_t)(FLASH_ACR_LATENCY | FLASH_ACR_WRHIGHFREQ))
#define FLASH_TIMING_TARGET                                                    \
    ((uint32_t)(FLASH_ACR_LATENCY_2WS | FLASH_ACR_WRHIGHFREQ_1))
_Static_assert(sizeof(nx_stm32h743_pll384_state_t) == 4U,
               "Optional H743 clock owner exceeds its static type budget");
static bool clock_task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool clock_context_valid(const nx_stm32h743_system_t* s) {
    return s->rcc != NULL && s->power != NULL && s->flash != NULL &&
           s->timer != NULL && s->core != NULL;
}
static bool clock_write(volatile uint32_t* reg, uint32_t value, uint32_t mask) {
    const uint32_t combined = (NX_H743_READ(reg) & ~mask) | (value & mask);
    NX_H743_WRITE(reg, combined);
    nx_arch_dsb();
    return (NX_H743_READ(reg) & mask) == (value & mask);
}
static nx_result_t clock_restore(volatile uint32_t* reg, uint32_t mask,
                                 uint32_t baseline, uint32_t selected) {
    const uint32_t current = NX_H743_READ(reg) & mask;
    if (current == (baseline & mask))
        return NX_SUCCESS;
    if (current != (selected & mask))
        return NX_ERROR_STATE;
    return clock_write(reg, baseline, mask) ? NX_SUCCESS : NX_ERROR_IO;
}
static bool voltage_ready(const nx_stm32h743_system_t* s, uint32_t scale) {
    return (NX_H743_READ(&s->power->D3CR) & (PWR_D3CR_VOS | PWR_D3CR_VOSRDY)) ==
               (scale | PWR_D3CR_VOSRDY) &&
           (NX_H743_READ(&s->power->CSR1) &
            (PWR_CSR1_ACTVOS | PWR_CSR1_ACTVOSRDY)) ==
               (scale | PWR_CSR1_ACTVOSRDY);
}
static nx_result_t wait_voltage(const nx_stm32h743_system_t* s, uint32_t scale,
                                uint32_t limit) {
    while (limit-- != 0U) {
        NX_H743_CLOCK_POLL();
        if (voltage_ready(s, scale))
            return NX_SUCCESS;
    }
    return NX_ERROR_TIMEOUT;
}
static nx_result_t wait_pll(const nx_stm32h743_system_t* s, bool enabled,
                            uint32_t limit) {
    const uint32_t expected = enabled ? RCC_CR_PLL1RDY : 0U;
    while (limit-- != 0U) {
        NX_H743_CLOCK_POLL();
        if ((NX_H743_READ(&s->rcc->CR) & RCC_CR_PLL1RDY) == expected)
            return NX_SUCCESS;
    }
    return NX_ERROR_TIMEOUT;
}
static nx_result_t wait_source(const nx_stm32h743_system_t* s, bool pll,
                               uint32_t limit) {
    const uint32_t expected = pll ? RCC_CFGR_SW_PLL1 | RCC_CFGR_SWS_PLL1 : 0U;
    while (limit-- != 0U) {
        NX_H743_CLOCK_POLL();
        if ((NX_H743_READ(&s->rcc->CFGR) & (RCC_CFGR_SW | RCC_CFGR_SWS)) ==
            expected)
            return NX_SUCCESS;
    }
    return NX_ERROR_TIMEOUT;
}
/** \brief A gate may be boot-owned SRAM1, never an unjoined DMA or device. */
static bool consumers_quiet(const nx_stm32h743_system_t* s,
                            uint32_t permitted_apb4) {
    uint32_t boot_sram = 0U;
#if defined(NEXUS_MEMORY_SRAM1) && NEXUS_MEMORY_SRAM1 == 1
    boot_sram = RCC_AHB2ENR_SRAM1EN;
#endif
    return (NX_H743_READ(&s->rcc->AHB1ENR) | NX_H743_READ(&s->rcc->AHB3ENR) |
            NX_H743_READ(&s->rcc->AHB4ENR) | NX_H743_READ(&s->rcc->APB1LENR) |
            NX_H743_READ(&s->rcc->APB1HENR) | NX_H743_READ(&s->rcc->APB2ENR) |
            NX_H743_READ(&s->rcc->APB3ENR)) == 0U &&
           (NX_H743_READ(&s->rcc->APB4ENR) & ~permitted_apb4) == 0U &&
           (NX_H743_READ(&s->rcc->AHB2ENR) & ~boot_sram) == 0U;
}
static bool flash_quiet(const nx_stm32h743_system_t* s) {
    return ((NX_H743_READ(&s->flash->SR1) | NX_H743_READ(&s->flash->SR2)) &
            (FLASH_SR_BSY | FLASH_SR_QW | FLASH_SR_WBNE)) == 0U &&
           ((NX_H743_READ(&s->flash->CR1) | NX_H743_READ(&s->flash->CR2)) &
            ~(uint32_t)(FLASH_CR_LOCK | FLASH_CR_PSIZE)) == 0U;
}
nx_result_t nx_stm32h743_pll384_stop(nx_stm32h743_pll384_state_t* state,
                                     uint32_t limit) {
    if (!clock_task())
        return NX_ERROR_CONTEXT;
    if (state == NULL || limit == 0U)
        return NX_ERROR_INVALID;
    if (state->remaining_effects == 0U)
        return NX_SUCCESS;
    nx_stm32h743_system_t* s = &g_nx_stm32h743_system;
    if (!clock_context_valid(s))
        return NX_ERROR_INVALID;
    if ((state->remaining_effects & ~CLOCK_EFFECTS) != 0U)
        return NX_ERROR_STATE;
    if (s->started || s->remaining_effects != 0U ||
        (NX_H743_READ(&s->core->CCR) & (SCB_CCR_DC_Msk | SCB_CCR_IC_Msk)) != 0U)
        return NX_ERROR_BUSY;
#if NX_H743_CACHE_CONTROL_ENABLED
    if (s->cache.owned_mask != 0U || s->cache.selector_pending)
        return NX_ERROR_BUSY;
#endif
    const uint32_t permitted_apb4 =
        (state->remaining_effects & CLOCK_GATE) != 0U ? RCC_APB4ENR_SYSCFGEN
                                                      : 0U;
    if (!consumers_quiet(s, permitted_apb4) || !flash_quiet(s) ||
        !NX_H743_IRQ_AVAILABLE(RCC_IRQn) || !NX_H743_IRQ_AVAILABLE(TIM2_IRQn))
        return NX_ERROR_BUSY;
    const uint32_t cr = NX_H743_READ(&s->rcc->CR);
    if ((cr & (RCC_CR_HSION | RCC_CR_HSIRDY | RCC_CR_HSIDIV)) !=
            (RCC_CR_HSION | RCC_CR_HSIRDY) ||
        (NX_H743_READ(&s->rcc->CFGR) &
         ~(uint32_t)(RCC_CFGR_SW | RCC_CFGR_SWS)) != 0U)
        return NX_ERROR_STATE;
    state->remaining_effects &= ~CLOCK_READY;
    /* No lower-voltage/Flash/bus/PLL withdrawal is safe until SWS proves HSI.
     * Requested SW alone cannot establish the processor's actual clock. */
    nx_result_t result;
    if ((state->remaining_effects & CLOCK_SOURCE) != 0U) {
        if (!clock_write(&s->rcc->CFGR, RCC_CFGR_SW_HSI, RCC_CFGR_SW))
            return NX_ERROR_IO;
        result = wait_source(s, false, limit);
        if (result != NX_SUCCESS)
            return result;
        state->remaining_effects &= ~CLOCK_SOURCE;
    } else if ((NX_H743_READ(&s->rcc->CFGR) & (RCC_CFGR_SW | RCC_CFGR_SWS)) !=
               0U) {
        return NX_ERROR_STATE;
    }
    s->core_hz = 64000000U;
    if ((state->remaining_effects & CLOCK_PLL) != 0U) {
        if (!clock_write(&s->rcc->CR, 0U, RCC_CR_PLL1ON))
            return NX_ERROR_IO;
        result = wait_pll(s, false, limit);
        if (result != NX_SUCCESS)
            return result;
        state->remaining_effects &= ~CLOCK_PLL;
    }
    if ((state->remaining_effects & CLOCK_CONFIG) != 0U) {
        if ((NX_H743_READ(&s->rcc->CR) & (PLL_ON_MASK | PLL_READY_MASK)) != 0U)
            return NX_ERROR_BUSY;
        result = clock_restore(&s->rcc->PLL1DIVR, PLL_DIVIDER_MASK, 0x01010280U,
                               0x01010277U);
        if (result != NX_SUCCESS)
            return result;
        result = clock_restore(&s->rcc->PLLCFGR, PLL_CONTROL_MASK, 0x01ff0000U,
                               PLL_CONTROL_TARGET);
        if (result != NX_SUCCESS)
            return result;
        result = clock_restore(&s->rcc->PLLCKSELR, PLL_SELECT_MASK, 0x02020200U,
                               10U << RCC_PLLCKSELR_DIVM1_Pos);
        if (result != NX_SUCCESS)
            return result;
        if ((NX_H743_READ(&s->rcc->PLL1FRACR) & RCC_PLL1FRACR_FRACN1) != 0U)
            return NX_ERROR_STATE;
        state->remaining_effects &= ~CLOCK_CONFIG;
    }
    if ((state->remaining_effects & CLOCK_BUS) != 0U) {
        result = clock_restore(&s->rcc->D3CFGR, D3_MASK, 0U, D3_TARGET);
        if (result != NX_SUCCESS)
            return result;
        result = clock_restore(&s->rcc->D2CFGR, D2_MASK, 0U, D2_TARGET);
        if (result != NX_SUCCESS)
            return result;
        result = clock_restore(&s->rcc->D1CFGR, D1_MASK, 0U, D1_TARGET);
        if (result != NX_SUCCESS)
            return result;
        state->remaining_effects &= ~CLOCK_BUS;
    }
    if ((state->remaining_effects & CLOCK_FLASH) != 0U) {
        result = clock_restore(&s->flash->ACR, FLASH_TIMING_MASK,
                               FLASH_ACR_LATENCY_7WS, FLASH_TIMING_TARGET);
        if (result != NX_SUCCESS)
            return result;
        nx_arch_isb();
        state->remaining_effects &= ~CLOCK_FLASH;
    }
    if ((state->remaining_effects & CLOCK_VOS) != 0U) {
        if ((NX_H743_READ(&s->rcc->APB4ENR) & RCC_APB4ENR_SYSCFGEN) == 0U ||
            (NX_H743_READ(&SYSCFG->PWRCR) & SYSCFG_PWRCR_ODEN) != 0U)
            return NX_ERROR_STATE;
        result = clock_restore(&s->power->D3CR, PWR_D3CR_VOS, PWR_D3CR_VOS_0,
                               PWR_D3CR_VOS);
        if (result != NX_SUCCESS)
            return result;
        result = wait_voltage(s, PWR_D3CR_VOS_0, limit);
        if (result != NX_SUCCESS)
            return result;
        state->remaining_effects &= ~CLOCK_VOS;
    }
    if ((state->remaining_effects & CLOCK_GATE) != 0U) {
        if (!clock_write(&s->rcc->APB4ENR, 0U, RCC_APB4ENR_SYSCFGEN))
            return NX_ERROR_IO;
        state->remaining_effects &= ~CLOCK_GATE;
    }
    return NX_SUCCESS;
}
nx_stm32h743_start_result_t
nx_stm32h743_pll384_start(nx_stm32h743_pll384_state_t* state, uint32_t limit) {
    nx_stm32h743_start_result_t result = {NX_SUCCESS, NX_SUCCESS, 0U};
    if (!clock_task()) {
        result.primary = NX_ERROR_CONTEXT;
        return result;
    }
    if (state == NULL || limit == 0U) {
        result.primary = NX_ERROR_INVALID;
        return result;
    }
    if (state->remaining_effects != 0U) {
        result.primary = NX_ERROR_BUSY;
        result.remaining_effects = state->remaining_effects;
        return result;
    }
    nx_stm32h743_system_t* s = &g_nx_stm32h743_system;
    result.primary = nx_stm32h743_pll384_preflight(s);
    if (result.primary != NX_SUCCESS)
        return result;
    /* A disabled gate may hide ODEN. Never raise VOS into an unobserved
     * overdrive configuration, nor write a reserved/foreign ODEN bit. */
    state->remaining_effects = CLOCK_GATE;
    if (!clock_write(&s->rcc->APB4ENR, RCC_APB4ENR_SYSCFGEN,
                     RCC_APB4ENR_SYSCFGEN)) {
        result.primary = NX_ERROR_IO;
        goto rollback;
    }
    if ((NX_H743_READ(&SYSCFG->PWRCR) & SYSCFG_PWRCR_ODEN) != 0U) {
        result.primary = NX_ERROR_BUSY;
        goto rollback;
    }
    state->remaining_effects |= CLOCK_VOS;
    if (!clock_write(&s->power->D3CR, PWR_D3CR_VOS, PWR_D3CR_VOS)) {
        result.primary = NX_ERROR_IO;
        goto rollback;
    }
    result.primary = wait_voltage(s, PWR_D3CR_VOS, limit);
    if (result.primary != NX_SUCCESS)
        goto rollback;
    state->remaining_effects |= CLOCK_FLASH;
    if (!clock_write(&s->flash->ACR, FLASH_TIMING_TARGET, FLASH_TIMING_MASK)) {
        result.primary = NX_ERROR_IO;
        goto rollback;
    }
    nx_arch_isb();
    state->remaining_effects |= CLOCK_BUS;
    if (!clock_write(&s->rcc->D1CFGR, D1_TARGET, D1_MASK) ||
        !clock_write(&s->rcc->D2CFGR, D2_TARGET, D2_MASK) ||
        !clock_write(&s->rcc->D3CFGR, D3_TARGET, D3_MASK)) {
        result.primary = NX_ERROR_IO;
        goto rollback;
    }
    /* RM0433 Rev9 8.7.8: HPRE may take16 slowest-APB periods, whereas
     * APB prescalers take16 HCLK cycles. All selected APBs divide HCLK by2;
     * 32 ordered volatile RCC reads cover that bound before PLL programming.
     * This narrow cold profile never changes D1CPRE or adopts other ratios. */
    for (unsigned i = 0U; i < 32U; ++i)
        (void)NX_H743_READ(&s->rcc->D1CFGR);
    nx_arch_dsb();
    state->remaining_effects |= CLOCK_CONFIG;
    if (!clock_write(&s->rcc->PLLCKSELR, 10U << RCC_PLLCKSELR_DIVM1_Pos,
                     PLL_SELECT_MASK) ||
        !clock_write(&s->rcc->PLLCFGR, PLL_CONTROL_TARGET, PLL_CONTROL_MASK) ||
        !clock_write(&s->rcc->PLL1DIVR, 0x01010277U, PLL_DIVIDER_MASK)) {
        result.primary = NX_ERROR_IO;
        goto rollback;
    }
    state->remaining_effects |= CLOCK_PLL;
    if (!clock_write(&s->rcc->CR, RCC_CR_PLL1ON, RCC_CR_PLL1ON)) {
        result.primary = NX_ERROR_IO;
        goto rollback;
    }
    result.primary = wait_pll(s, true, limit);
    if (result.primary != NX_SUCCESS)
        goto rollback;
    state->remaining_effects |= CLOCK_SOURCE;
    if (!clock_write(&s->rcc->CFGR, RCC_CFGR_SW_PLL1, RCC_CFGR_SW)) {
        result.primary = NX_ERROR_IO;
        goto rollback;
    }
    result.primary = wait_source(s, true, limit);
    if (result.primary != NX_SUCCESS)
        goto rollback;
    nx_arch_dsb();
    nx_arch_isb();
    s->core_hz = 384000000U;
    state->remaining_effects |= CLOCK_READY;
    return result;
rollback:
    result.cleanup = nx_stm32h743_pll384_stop(state, limit);
    result.remaining_effects = state->remaining_effects;
    return result;
}
