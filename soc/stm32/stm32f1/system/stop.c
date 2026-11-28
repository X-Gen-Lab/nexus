/**
 * \file            stop.c
 * \brief           Exact F103RB STOP handoff using one external wake IRQ.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "private/hardware.h"
#include "system/platform.h"
#include "system/stop_internal.h"

#if !defined(STM32F103xB) || !defined(NX_STM32F1_STOP_OWNER)
#error "STOP is an explicit F103RB HSI8 baremetal software reference"
#endif

#ifndef NX_STM32F1_STOP_WAIT
#define NX_STM32F1_STOP_WAIT() __WFI()
#endif

#define WAKE_LINE   (UINT32_C(1) << 13U)
#define WAKE_VECTOR (UINT32_C(1) << (EXTI15_10_IRQn - 32U))
#define TIM2_VECTOR (UINT32_C(1) << TIM2_IRQn)

/** \brief Reject all previous pending work without acknowledging any source. */
static bool no_pending(void) {
    const uint32_t exceptions = SCB_ICSR_NMIPENDSET_Msk |
                                SCB_ICSR_PENDSVSET_Msk | SCB_ICSR_PENDSTSET_Msk;
    const uint32_t faults =
        SCB_SHCSR_USGFAULTPENDED_Msk | SCB_SHCSR_MEMFAULTPENDED_Msk |
        SCB_SHCSR_BUSFAULTPENDED_Msk | SCB_SHCSR_SVCALLPENDED_Msk;
    return EXTI->PR == 0U && NVIC->ISPR[0] == 0U && NVIC->ISPR[1] == 0U &&
           RTC->CRH == 0U && (RTC->CRL & RTC_CRL_ALRF) == 0U &&
           (SCB->ICSR & exceptions) == 0U && (SCB->SHCSR & faults) == 0U;
}

/** \brief Preserve only the configured autonomous wake source. */
static bool wake_ready(const nx_stm32f1_exti_t* wake) {
    return wake != NULL && wake->config != NULL &&
           wake->config->gpio == GPIOC && wake->config->line == 13U &&
           wake->config->edge == NX_EXTI_FALLING && wake->line_owned &&
           wake->vector_owned && wake->live;
}

nx_result_t nx_stm32f1_stop_wake_suspend(nx_stm32f1_exti_t* state) {
    return wake_ready(state) ? NX_SUCCESS : NX_ERROR_STATE;
}

nx_result_t nx_stm32f1_stop_wake_resume(nx_stm32f1_exti_t* state) {
    return wake_ready(state) && nx_stm32f1_stop_time_ready() &&
                   (RCC->APB1ENR & RCC_APB1ENR_TIM2EN) != 0U &&
                   (TIM2->CR1 & TIM_CR1_CEN) != 0U &&
                   (NVIC->ISER[0] & TIM2_VECTOR) != 0U &&
                   (RCC->APB1ENR & RCC_APB1ENR_PWREN) == 0U &&
                   (SCB->SCR & SCB_SCR_SLEEPDEEP_Msk) == 0U &&
                   (NVIC->ISER[1] & WAKE_VECTOR) != 0U &&
                   (RCC->APB2ENR & (RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPCEN)) ==
                       (RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPCEN) &&
                   (EXTI->IMR & WAKE_LINE) != 0U &&
                   (EXTI->FTSR & WAKE_LINE) != 0U &&
                   (AFIO->EXTICR[3] & AFIO_EXTICR4_EXTI13) ==
                       AFIO_EXTICR4_EXTI13_PC
               ? NX_SUCCESS
               : NX_ERROR_STATE;
}

/** \brief Fixed PC13 route with only reset-mode PWR and TIM2 open. */
static bool hardware_ready(const nx_stm32f1_exti_t* wake) {
    const uint32_t flash_options = FLASH_OBR_IWDG_SW | FLASH_OBR_nRST_STOP;
    const uint32_t pwr_modes =
        PWR_CR_PDDS | PWR_CR_LPDS | PWR_CR_PVDE | PWR_CR_DBP;
    const uint32_t sleep_modes =
        SCB_SCR_SLEEPONEXIT_Msk | SCB_SCR_SLEEPDEEP_Msk;
    /* Zero SysTick reload/current cannot produce ticks. Never read CTRL,
     * whose COUNTFLAG is cleared by a read. RM0008 7.3.9: RTCSEL can return to
     * zero only through backup reset, so a configured RTC cannot be hidden
     * by gating its unclocked APB register view. */
    return wake_ready(wake) && wake->count == 0U && !wake->loss &&
           RCC->AHBENR == (RCC_AHBENR_FLITFEN | RCC_AHBENR_SRAMEN) &&
           RCC->APB1ENR == (RCC_APB1ENR_TIM2EN | RCC_APB1ENR_PWREN) &&
           RCC->APB2ENR == (RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPCEN) &&
           RCC->APB1RSTR == 0U && RCC->APB2RSTR == 0U &&
           (RCC->BDCR & (RCC_BDCR_RTCEN | RCC_BDCR_RTCSEL)) == 0U &&
           (RCC->CSR & (RCC_CSR_LSION | RCC_CSR_LSIRDY)) == 0U &&
           (FLASH->OBR & (flash_options | FLASH_OBR_OPTERR)) == flash_options &&
           (PWR->CR & pwr_modes) == 0U && (SCB->SCR & sleep_modes) == 0U &&
           SysTick->LOAD == 0U && SysTick->VAL == 0U && NVIC->IABR[0] == 0U &&
           NVIC->IABR[1] == 0U && NVIC->ISER[0] == TIM2_VECTOR &&
           NVIC->ISER[1] == WAKE_VECTOR && EXTI->IMR == WAKE_LINE &&
           EXTI->EMR == 0U && EXTI->FTSR == WAKE_LINE && EXTI->RTSR == 0U &&
           (AFIO->EXTICR[3] & AFIO_EXTICR4_EXTI13) == AFIO_EXTICR4_EXTI13_PC;
}

nx_stm32f1_stop_receipt_t nx_stm32f1_stop_hsi8(nx_clock_barrier_t barrier,
                                               const nx_stm32f1_exti_t* wake) {
    nx_stm32f1_stop_receipt_t receipt = {.status = NX_ERROR_INVALID};
    if (wake == NULL)
        return receipt;
    const nx_arch_irq_masks_t masks = nx_arch_irq_masks();
    if (!nx_stm32f1_cold_context() || masks.primask != 0U ||
        masks.basepri != 0U || masks.faultmask != 0U) {
        receipt.status = NX_ERROR_CONTEXT;
        return receipt;
    }
    const nx_arch_irq_state_t saved = nx_arch_irq_save();
    bool pwr_owned = false;
    receipt.status = nx_clock_domain_quiescent(barrier);
    if (receipt.status != NX_SUCCESS)
        goto done;
    if (!nx_stm32f1_stop_time_ready()) {
        receipt.status = NX_ERROR_STATE;
        goto done;
    }
    if (RCC->APB1ENR != RCC_APB1ENR_TIM2EN) {
        receipt.status = NX_ERROR_BUSY;
        goto done;
    }
    pwr_owned = true;
    if (!nx_stm32f1_write_masked(&RCC->APB1ENR, RCC_APB1ENR_PWREN,
                                 RCC_APB1ENR_PWREN)) {
        receipt.status = NX_ERROR_IO;
        goto done;
    }
    if (!hardware_ready(wake) || !no_pending()) {
        receipt.status = NX_ERROR_BUSY;
        goto done;
    }

    receipt.status = nx_stm32f1_stop_time_suspend(&receipt);
    if (receipt.status == NX_SUCCESS) {
        const uint32_t deep = SCB_SCR_SLEEPDEEP_Msk;
        if (!nx_stm32f1_write_masked(&SCB->SCR, deep, deep)) {
            receipt.status = NX_ERROR_IO;
        } else if (!no_pending()) {
            receipt.status = NX_ERROR_BUSY;
        } else {
            /* RM0008 Table 14 and ES096 2.2.1/2.2.3/2.2.4. WFI is
             * interrupt wake; leave the Flash interface clock on. */
            nx_arch_dsb();
            NX_STM32F1_STOP_WAIT();
            nx_arch_isb();
            receipt.wait_issued = true;
        }
        if (!nx_stm32f1_write_masked(&SCB->SCR, deep, 0U))
            receipt.cleanup = NX_ERROR_IO;
        const nx_result_t restored = nx_stm32f1_stop_time_restore(&receipt);
        if (receipt.cleanup == NX_SUCCESS)
            receipt.cleanup = restored;
    }
done:
    receipt.wake_pending = (EXTI->PR & WAKE_LINE) != 0U;
    if (pwr_owned &&
        !nx_stm32f1_write_masked(&RCC->APB1ENR, RCC_APB1ENR_PWREN, 0U))
        receipt.cleanup = NX_ERROR_IO;
    if (pwr_owned && !nx_stm32f1_system_live()) {
        /* A timestamping IRQ may not run against the retired time domain. */
        NX_STM32F1_WRITE(&NVIC->ICER[1], WAKE_VECTOR);
        nx_arch_dsb();
        if ((NVIC->ISER[1] & WAKE_VECTOR) != 0U) {
            receipt.irq_mask_retained = true;
            return receipt;
        }
    }

    nx_arch_irq_restore(saved);
    return receipt;
}

nx_stm32f1_stop_receipt_t nx_stm32f103_stop_enter(nx_clock_barrier_t barrier,
                                                  const nx_exti_port_t* wake) {
    nx_stm32f1_stop_receipt_t receipt = {.status = NX_ERROR_INVALID};
    if (wake == NULL || wake->ops != &nx_stm32f1_exti_ops ||
        !wake_ready(wake->context))
        return receipt;
    const nx_arch_irq_masks_t masks = nx_arch_irq_masks();
    if (!nx_stm32f1_cold_context() || masks.basepri != 0U ||
        masks.faultmask != 0U) {
        receipt.status = NX_ERROR_CONTEXT;
        return receipt;
    }
    receipt.status = nx_clock_domain_quiescent(barrier);
    if (receipt.status != NX_SUCCESS)
        return receipt;
    receipt.status = nx_platform_suspend();
    if (receipt.status != NX_SUCCESS)
        return receipt;
    receipt = nx_stm32f1_stop_hsi8(barrier, wake->context);
    receipt.platform_suspended = true;
    receipt.resume_ready =
        receipt.cleanup == NX_SUCCESS && !receipt.irq_mask_retained &&
        nx_stm32f1_stop_wake_resume(wake->context) == NX_SUCCESS;
    return receipt;
}
