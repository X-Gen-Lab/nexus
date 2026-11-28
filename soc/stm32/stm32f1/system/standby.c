/**
 * \file            standby.c
 * \brief           STM32F103RB Standby using the PA0 hardware wakeup pin.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "system/standby.h"
#include "nexus/io/watchdog.h"
#include "nexus_bindings.h"
#include "private/hardware.h"
#include "system/platform.h"

#if !defined(STM32F103xB) || !defined(NX_STM32F1_STANDBY_OWNER) ||             \
    defined(NX_STM32F1_PROFILE_CORE_HZ)
#error "Standby is an explicit F103RB HSI8 baremetal software reference"
#endif

#ifndef NX_STM32F1_STANDBY_WAIT
#define NX_STM32F1_STANDBY_WAIT() __WFI()
#endif

_Static_assert(PWR_CSR_WUF == NX_STM32F103_STANDBY_WAKE_LATCHED &&
                   PWR_CSR_SBF == NX_STM32F103_STANDBY_ENTERED,
               "Selected Standby wake flags match the public evidence");

static uint32_t wake_flags(void) {
    return PWR->CSR & (PWR_CSR_WUF | PWR_CSR_SBF);
}

static bool pin_low(void) {
    return (GPIOA->IDR & GPIO_IDR_IDR0) == 0U;
}

/** \brief Only the two caller-held gates and one unarmed pad are admitted. */
static bool hardware_ready(void) {
    const uint32_t power_modes =
        PWR_CR_PDDS | PWR_CR_LPDS | PWR_CR_PVDE | PWR_CR_DBP;
    const uint32_t options = FLASH_OBR_IWDG_SW | FLASH_OBR_nRST_STDBY;
    const uint32_t clocks = RCC_CR_HSION | RCC_CR_HSIRDY | RCC_CR_HSITRIM |
                            RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_HSEBYP |
                            RCC_CR_CSSON | RCC_CR_PLLON | RCC_CR_PLLRDY;
    const uint32_t hsi8 =
        RCC_CR_HSION | RCC_CR_HSIRDY | (16U << RCC_CR_HSITRIM_Pos);
    const uint32_t bus_clock = RCC_CFGR_SW | RCC_CFGR_SWS | RCC_CFGR_HPRE |
                               RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2;
    return RCC->AHBENR == (RCC_AHBENR_FLITFEN | RCC_AHBENR_SRAMEN) &&
           RCC->APB1ENR == RCC_APB1ENR_PWREN &&
           RCC->APB2ENR == RCC_APB2ENR_IOPAEN && RCC->APB1RSTR == 0U &&
           RCC->APB2RSTR == 0U && (RCC->CR & clocks) == hsi8 &&
           (RCC->CFGR & bus_clock) == 0U &&
           (RCC->CSR & (RCC_CSR_LSION | RCC_CSR_LSIRDY)) == 0U &&
           (RCC->BDCR & (RCC_BDCR_RTCSEL | RCC_BDCR_RTCEN)) == 0U &&
           (FLASH->ACR & (FLASH_ACR_HLFCYA | FLASH_ACR_LATENCY)) == 0U &&
           (FLASH->SR & FLASH_SR_BSY) == 0U &&
           (FLASH->OBR & (options | FLASH_OBR_OPTERR)) == options &&
           (PWR->CR & power_modes) == 0U &&
           (SCB->SCR & (SCB_SCR_SLEEPONEXIT_Msk | SCB_SCR_SLEEPDEEP_Msk)) ==
               0U &&
           SysTick->LOAD == 0U && SysTick->VAL == 0U && NVIC->ISER[0] == 0U &&
           NVIC->ISER[1] == 0U && NVIC->IABR[0] == 0U && NVIC->IABR[1] == 0U &&
           EXTI->IMR == 0U && EXTI->EMR == 0U &&
           (GPIOA->CRL & (GPIO_CRL_MODE0 | GPIO_CRL_CNF0)) == GPIO_CRL_CNF0_0;
}

/** \brief Reject pending work without acknowledging any source.
 * \note Admission already proves the RTC clock source and enable are absent.
 */
static bool pending_clear(void) {
    const uint32_t exceptions = SCB_ICSR_NMIPENDSET_Msk |
                                SCB_ICSR_PENDSVSET_Msk | SCB_ICSR_PENDSTSET_Msk;
    const uint32_t faults =
        SCB_SHCSR_USGFAULTPENDED_Msk | SCB_SHCSR_MEMFAULTPENDED_Msk |
        SCB_SHCSR_BUSFAULTPENDED_Msk | SCB_SHCSR_SVCALLPENDED_Msk;
    return EXTI->PR == 0U && NVIC->ISPR[0] == 0U && NVIC->ISPR[1] == 0U &&
           (SCB->ICSR & exceptions) == 0U && (SCB->SHCSR & faults) == 0U;
}

/** \brief EWUP is the only writable field in F103 PWR_CSR. */
static bool select_pin(uint32_t ewup) {
    NX_STM32F1_WRITE(&PWR->CSR, ewup);
    nx_arch_dsb();
    return (PWR->CSR & PWR_CSR_EWUP) == ewup;
}

/** \brief CWUF is a W1C command; observe its documented two-cycle delay. */
static bool clear_wake(void) {
    NX_STM32F1_WRITE(&PWR->CR, PWR->CR | PWR_CR_CWUF);
    nx_arch_dsb();
    for (unsigned sample = 0U; sample < 8U; ++sample) {
        if ((PWR->CSR & PWR_CSR_WUF) == 0U)
            return true;
    }
    return false;
}

nx_stm32f1_standby_receipt_t nx_stm32f1_standby_pa0(nx_clock_barrier_t barrier,
                                                    bool discard_wuf) {
    nx_stm32f1_standby_receipt_t receipt = {.status = NX_ERROR_CONTEXT};
    const nx_arch_irq_masks_t masks = nx_arch_irq_masks();
    if (!nx_stm32f1_cold_context() || masks.primask != 0U ||
        masks.basepri != 0U || masks.faultmask != 0U)
        return receipt;

    const nx_arch_irq_state_t saved = nx_arch_irq_save();
    receipt.status = nx_clock_domain_quiescent(barrier);
    if (receipt.status != NX_SUCCESS)
        goto done;
    if (nx_stm32f1_system_live()) {
        receipt.status = NX_ERROR_STATE;
        goto done;
    }
    if (!hardware_ready()) {
        receipt.status = NX_ERROR_BUSY;
        goto done;
    }

    const uint32_t previous = PWR->CSR;
    receipt.before_flags = receipt.after_flags =
        previous & (PWR_CSR_WUF | PWR_CSR_SBF);
    const bool clear_previous = discard_wuf && (previous & PWR_CSR_WUF) != 0U;
    if (!pin_low() || !pending_clear() ||
        ((previous & PWR_CSR_WUF) != 0U && !clear_previous)) {
        receipt.status = NX_ERROR_BUSY;
        goto done;
    }
    /* ES096 2.2.5: remove the only wake contributor before acknowledging.
     * A flag appearing during withdrawal is still caller-owned evidence. */
    if (!select_pin(0U)) {
        receipt.status = NX_ERROR_IO;
    } else if (!pin_low() ||
               ((PWR->CSR & PWR_CSR_WUF) != 0U && !clear_previous)) {
        receipt.status = NX_ERROR_BUSY;
    } else if (clear_previous && !clear_wake()) {
        receipt.status = NX_ERROR_TIMEOUT;
    } else if (!nx_stm32f1_write_masked(&PWR->CR, PWR_CR_PDDS, PWR_CR_PDDS) ||
               !nx_stm32f1_write_masked(&SCB->SCR, SCB_SCR_SLEEPDEEP_Msk,
                                        SCB_SCR_SLEEPDEEP_Msk) ||
               !select_pin(PWR_CSR_EWUP)) {
        receipt.status = NX_ERROR_IO;
    } else if (!pin_low() || (PWR->CSR & PWR_CSR_WUF) != 0U ||
               !pending_clear()) {
        receipt.status = NX_ERROR_BUSY;
    } else {
        nx_arch_dsb();
        NX_STM32F1_STANDBY_WAIT();
        nx_arch_isb();
        receipt.status = NX_ERROR_IO;
        receipt.wait_issued = true;
    }

    /* A real Standby reset never reaches here. Withdraw the temporary
     * controls and retain the wake/Standby evidence on a false entry. */
    const bool shallow =
        nx_stm32f1_write_masked(&SCB->SCR, SCB_SCR_SLEEPDEEP_Msk, 0U);
    const bool normal = nx_stm32f1_write_masked(&PWR->CR, PWR_CR_PDDS, 0U);
    const bool restored = select_pin(previous & PWR_CSR_EWUP);
    if (!shallow || !normal || !restored)
        receipt.cleanup = NX_ERROR_IO;
    receipt.after_flags = wake_flags();
done:
    nx_arch_irq_restore(saved);
    return receipt;
}

nx_result_t nx_stm32f103_standby_boot(nx_stm32f103_standby_boot_t* output) {
    const uintptr_t address = (uintptr_t)output;
    if (output == NULL ||
        address % _Alignof(nx_stm32f103_standby_boot_t) != 0U ||
        address > UINTPTR_MAX - sizeof(*output) ||
        (address >= UINT32_C(0x40000000) && address < UINT32_C(0x60000000)) ||
        (address >= UINT32_C(0xe0000000) && address < UINT32_C(0xf0000000))) {
        return NX_ERROR_INVALID;
    }
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!nx_stm32f1_system_live()) {
        return NX_ERROR_STATE;
    }
    if ((RCC->APB1ENR & RCC_APB1ENR_PWREN) != 0U ||
        (RCC->APB1RSTR & RCC_APB1RSTR_PWRRST) != 0U) {
        return NX_ERROR_BUSY;
    }
    const bool powered = nx_stm32f1_write_masked(
        &RCC->APB1ENR, RCC_APB1ENR_PWREN, RCC_APB1ENR_PWREN);
    const uint32_t flags = powered ? wake_flags() : 0U;
    if (!nx_stm32f1_write_masked(&RCC->APB1ENR, RCC_APB1ENR_PWREN, 0U) ||
        !powered) {
        return NX_ERROR_IO;
    }
    *output = (nx_stm32f103_standby_boot_t){nx_reset_cause(), flags};
    return NX_SUCCESS;
}

nx_stm32f1_standby_receipt_t
nx_stm32f103_standby_enter(nx_clock_barrier_t barrier, bool discard_wuf) {
    nx_stm32f1_standby_receipt_t receipt = {.status = NX_ERROR_CONTEXT};
    const nx_arch_irq_masks_t masks = nx_arch_irq_masks();
    if (!nx_stm32f1_cold_context() || masks.primask != 0U ||
        masks.basepri != 0U || masks.faultmask != 0U) {
        return receipt;
    }
    receipt.status = nx_clock_domain_quiescent(barrier);
    if (receipt.status != NX_SUCCESS) {
        return receipt;
    }
    receipt.status = nx_platform_stop();
    if (receipt.status != NX_SUCCESS) {
        return receipt;
    }
    receipt.platform_stopped = true;
    if (RCC->APB1ENR != 0U || RCC->APB2ENR != 0U) {
        receipt.status = NX_ERROR_BUSY;
        return receipt;
    }
    const bool power = nx_stm32f1_write_masked(&RCC->APB1ENR, RCC_APB1ENR_PWREN,
                                               RCC_APB1ENR_PWREN);
    const bool gpio =
        power && nx_stm32f1_write_masked(&RCC->APB2ENR, RCC_APB2ENR_IOPAEN,
                                         RCC_APB2ENR_IOPAEN);
    if (power && gpio) {
        receipt = nx_stm32f1_standby_pa0(barrier, discard_wuf);
        receipt.platform_stopped = true;
    } else {
        receipt.status = NX_ERROR_IO;
    }
    if (receipt.cleanup == NX_SUCCESS) {
        const bool gpio_closed =
            nx_stm32f1_write_masked(&RCC->APB2ENR, RCC_APB2ENR_IOPAEN, 0U);
        const bool power_closed =
            nx_stm32f1_write_masked(&RCC->APB1ENR, RCC_APB1ENR_PWREN, 0U);
        if (!gpio_closed || !power_closed) {
            receipt.cleanup = NX_ERROR_IO;
        }
    }
    return receipt;
}
