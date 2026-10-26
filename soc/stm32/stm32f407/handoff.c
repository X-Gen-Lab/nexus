/**
 * \file            handoff.c
 * \brief           F407 owner admission and exact Flash-image tail handoff
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "clock_domain_impl.h"
#include "nexus/components/stm32f407_handoff.h"
#include "stm32f407_handoff_hw.h"
#include "stm32f407_system.h"

#if NX_F407_HANDOFF_FLASH_BYTES != 524288 &&                                   \
    NX_F407_HANDOFF_FLASH_BYTES != 1048576
#error "F407 handoff requires exact reviewed VE/ZG Flash geometry"
#endif

extern nx_stm32_system_t g_nx_stm32_system;
static nx_clock_domain_t s_handoff_domain;
_Static_assert(FPU_IRQn == 81, "F407 vector extent must match official SDK");

/** \brief F407 has 82 external vectors, hence a 512-byte VTOR alignment. */
enum {
    NX_F407_VECTOR_BYTES = (16U + 82U) * sizeof(uint32_t),
    NX_F407_VECTOR_ALIGNMENT = 512U,
    NX_F407_MAIN_SRAM_END = 0x20020000U
};

nx_result_t nx_stm32f407_handoff_initialize(size_t consumer_capacity) {
    return nx_clock_domain_construct(&s_handoff_domain, consumer_capacity);
}

nx_clock_domain_t* nx_stm32f407_handoff_domain(void) {
    return &s_handoff_domain;
}

/** \brief Address arithmetic precedes every target Flash dereference. */
static bool vector_valid(uint32_t vector) {
    return vector >= FLASH_BASE &&
           vector - FLASH_BASE <=
               NX_F407_HANDOFF_FLASH_BYTES - NX_F407_VECTOR_BYTES &&
           vector % NX_F407_VECTOR_ALIGNMENT == 0U;
}

/** \brief No enrolled owner authorizes unsupported CPU/exception state. */
static nx_result_t cpu_ready(void) {
    if ((NX_F407_READ(SCB->CPUID) & 0xff00fff0U) != 0x4100c240U) {
        return NX_ERROR_UNSUPPORTED;
    }
    uint32_t cpacr = NX_F407_READ(SCB->CPACR) & (0xfU << 20U);
    if ((cpacr != 0U && cpacr != (0xfU << 20U)) ||
        (NX_F407_READ(FPU->FPCCR) & FPU_FPCCR_LSPACT_Msk) != 0U ||
        NX_F407_READ(MPU->CTRL) != 0U ||
        (NX_F407_READ(SCB->CCR) & ((1U << 16U) | (1U << 17U))) != 0U ||
        NX_F407_READ(SCB->SCR) != 0U ||
        (NX_F407_READ(DCB->DHCSR) & 0xfU) != 0U ||
        NX_F407_READ(DCB->DEMCR) != 0U || NX_F407_READ(FLASH->ACR) != 0U) {
        return NX_ERROR_UNSUPPORTED;
    }
    const uint32_t pending = SCB_ICSR_VECTACTIVE_Msk |
                             SCB_ICSR_VECTPENDING_Msk |
                             SCB_ICSR_ISRPENDING_Msk | SCB_ICSR_ISRPREEMPT_Msk |
                             SCB_ICSR_PENDSTSET_Msk | SCB_ICSR_PENDSVSET_Msk |
                             SCB_ICSR_NMIPENDSET_Msk;
    if ((NX_F407_READ(SCB->ICSR) & pending) != 0U ||
        NX_F407_READ(SCB->SHCSR) != 0U) {
        return NX_ERROR_BUSY;
    }
    /* CTRL is the only actual ENABLE observation. Its architected read also
     * consumes COUNTFLAG; LOAD/VAL alone cannot prove SysTick stopped. */
    if ((NX_F407_READ(SysTick->CTRL) &
         (SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk)) != 0U ||
        NX_F407_READ(SysTick->LOAD) != 0U || NX_F407_READ(SysTick->VAL) != 0U) {
        return NX_ERROR_BUSY;
    }
    for (size_t word = 0U; word < 3U; ++word) {
        if (NX_F407_READ(NVIC->ISER[word]) != 0U ||
            NX_F407_READ(NVIC->ISPR[word]) != 0U ||
            NX_F407_READ(NVIC->IABR[word]) != 0U) {
            return NX_ERROR_BUSY;
        }
    }
    return NX_SUCCESS;
}

/** \brief An off peripheral gate prevents execution, not latent configuration.
 */
static nx_result_t clocks_ready(void) {
    const uint32_t cr_mask = RCC_CR_HSION | RCC_CR_HSIRDY | RCC_CR_HSITRIM |
                             RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_HSEBYP |
                             RCC_CR_CSSON | RCC_CR_PLLON | RCC_CR_PLLRDY |
                             RCC_CR_PLLI2SON | RCC_CR_PLLI2SRDY;
    if ((NX_F407_READ(RCC->CR) & cr_mask) !=
            (RCC_CR_HSION | RCC_CR_HSIRDY | (16U << 3U)) ||
        NX_F407_READ(RCC->CFGR) != 0U ||
        (NX_F407_READ(RCC->SSCGR) & RCC_SSCGR_SSCGEN) != 0U ||
        (NX_F407_READ(RCC->CIR) & 0x00007f00U) != 0U ||
        (NX_F407_READ(RCC->AHB1ENR) & ~(uint32_t)RCC_AHB1ENR_CCMDATARAMEN) !=
            0U ||
        NX_F407_READ(RCC->AHB2ENR) != 0U || NX_F407_READ(RCC->AHB3ENR) != 0U ||
        NX_F407_READ(RCC->APB1ENR) != 0U || NX_F407_READ(RCC->APB2ENR) != 0U ||
        NX_F407_READ(RCC->AHB1RSTR) != 0U ||
        NX_F407_READ(RCC->AHB2RSTR) != 0U ||
        NX_F407_READ(RCC->AHB3RSTR) != 0U ||
        NX_F407_READ(RCC->APB1RSTR) != 0U ||
        NX_F407_READ(RCC->APB2RSTR) != 0U ||
        (NX_F407_READ(FLASH->SR) & FLASH_SR_BSY) != 0U ||
        NX_F407_READ(FLASH->CR) != FLASH_CR_LOCK) {
        return NX_ERROR_BUSY;
    }
    /* RM0090: IWDG forces the LSI oscillator on. This conservative entry
     * rejects any LSI owner; it never attempts to disable a watchdog. */
    if ((NX_F407_READ(RCC->CSR) & (RCC_CSR_LSION | RCC_CSR_LSIRDY)) != 0U) {
        return NX_ERROR_UNSUPPORTED;
    }
    return NX_SUCCESS;
}

nx_result_t nx_stm32f407_handoff(nx_clock_barrier_t owners,
                                 uint32_t vector_address) {
    if (!vector_valid(vector_address) || owners.domain != &s_handoff_domain) {
        return NX_ERROR_INVALID;
    }
    if (!nx_arch_is_privileged() || nx_arch_exception_number() != 0U) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_masks_t masks = nx_arch_irq_masks();
    if (masks.primask != 1U || masks.basepri != 0U || masks.faultmask != 0U ||
        NX_F407_CONTROL() != 0U) {
        return NX_ERROR_CONTEXT;
    }
    nx_result_t result = nx_clock_domain_quiescent(owners);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (g_nx_stm32_system.started ||
        g_nx_stm32_system.remaining_effects != 0U) {
        return NX_ERROR_BUSY;
    }
    if (g_nx_stm32_system.timer_error != NX_SUCCESS) {
        return NX_ERROR_STATE;
    }
    result = cpu_ready();
    if (result != NX_SUCCESS) {
        return result;
    }
    if ((NX_F407_READ(DBGMCU->IDCODE) & 0xfffU) != 0x413U ||
        NX_F407_READ16(FLASHSIZE_BASE) != NX_F407_HANDOFF_FLASH_BYTES / 1024U) {
        return NX_ERROR_IO;
    }
    result = clocks_ready();
    if (result != NX_SUCCESS) {
        return result;
    }
    uint32_t stack =
        NX_F407_READ(*(volatile const uint32_t*)(uintptr_t)vector_address);
    uint32_t pc =
        NX_F407_READ(*(volatile const uint32_t*)(uintptr_t)(vector_address +
                                                            sizeof(uint32_t)));
    if (stack <= SRAM1_BASE || stack > NX_F407_MAIN_SRAM_END ||
        stack % 8U != 0U || (pc & 1U) == 0U || (pc & ~1U) < FLASH_BASE ||
        (pc & ~1U) - FLASH_BASE > NX_F407_HANDOFF_FLASH_BYTES - 2U) {
        return NX_ERROR_INVALID;
    }
    /* The masked admission domain remains closed. No mutable provider,
     * target pointer or old stack reference is used after the tail. */
    NX_F407_TAIL(vector_address, stack, pc);
}
