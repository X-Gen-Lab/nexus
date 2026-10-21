/**
 * \file            system.c
 * \brief           STM32F407 precise clock startup, rollback and timer timebase
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/fault.h"
#include "nexus/core/time.h"
#include "nexus_config.h"
#include "stm32f407_system.h"
#if defined(NEXUS_MEMORY_CCM) && NEXUS_MEMORY_CCM
#include "memory_init.h"
#endif
#if defined(NEXUS_TIMEBASE_LOWPOWER) && NEXUS_TIMEBASE_LOWPOWER
#include "stm32f407_lowpower.h"
#endif

#ifndef NEXUS_STM32_MODEL
nx_stm32_system_t g_nx_stm32_system = {
    .rcc = RCC, .flash = FLASH, .power = PWR, .timer = TIM2};
#endif

extern nx_stm32_system_t g_nx_stm32_system;

#ifndef NX_STM32_TIMEBASE_ACK
#define NX_STM32_TIMEBASE_ACK(system, value) ((system)->timer->SR = (value))
#endif

/** \brief Read the unique fixed provider clock without an OS dependency. */
nx_time_us_t nx_time_now_us(void) {
    return nx_stm32_system_now(&g_nx_stm32_system);
}

/** \brief Forward the explicitly reserved timer vector to the static context.
 */
void TIM2_IRQHandler(void) {
#if defined(NEXUS_TIMEBASE_LOWPOWER) && NEXUS_TIMEBASE_LOWPOWER
    nx_arch_irq_state_t saved = nx_arch_irq_save();
#endif
    nx_stm32_system_timer_irq(&g_nx_stm32_system);
#if defined(NEXUS_TIMEBASE_LOWPOWER) && NEXUS_TIMEBASE_LOWPOWER
    if (g_nx_stm32_system.started) {
        nx_stm32_lowpower_alarm_irq();
    }
    nx_arch_irq_restore(saved);
#endif
}

#ifndef NX_STM32_POLL
#define NX_STM32_POLL(system) ((void)(system))
#endif

#define NX_STM32_EFFECT_CLOCK       1U
#define NX_STM32_EFFECT_TIMER       2U
#define NX_STM32_EFFECT_TIMER_GATE  4U
#define NX_STM32_EFFECT_TIMER_QUIET 8U
#if defined(NEXUS_F407_I2S2_SLAVE_TX_SELECTED) &&                              \
    NEXUS_F407_I2S2_SLAVE_TX_SELECTED
#define NX_STM32_EFFECT_GPIOB_GATE 16U
#define NX_STM32_EFFECT_DMA1_GATE  32U
#endif
#ifndef NX_STM32_WRITE
#define NX_STM32_WRITE(reg, value) (*(reg) = (uint32_t)(value))
#endif
#ifndef NX_STM32_IRQ_AVAILABLE
#ifdef NEXUS_STM32_MODEL
#define NX_STM32_IRQ_AVAILABLE(irq) ((void)(irq), true)
#else
#define NX_STM32_IRQ_AVAILABLE(irq)                                            \
    (NVIC_GetEnableIRQ(irq) == 0U && NVIC_GetPendingIRQ(irq) == 0U &&          \
     NVIC_GetActive(irq) == 0U)
#endif
#endif
#ifndef NX_STM32_IRQ_CONTROL
/** \brief Observe reserved vector withdrawal before releasing its
 * responsibility.
 */
static int timer_irq_control(IRQn_Type irq, bool enabled) {
#ifndef NEXUS_STM32_MODEL
    if (enabled) {
        NVIC_SetPriority(irq, 0U);
        NVIC_ClearPendingIRQ(irq);
        NVIC_EnableIRQ(irq);
    } else {
        NVIC_DisableIRQ(irq);
        NVIC_ClearPendingIRQ(irq);
    }
    nx_arch_dsb();
    nx_arch_isb();
    if (NVIC_GetEnableIRQ(irq) != (enabled ? 1U : 0U) ||
        (!enabled &&
         (NVIC_GetPendingIRQ(irq) != 0U || NVIC_GetActive(irq) != 0U))) {
        return -1;
    }
#else
    (void)irq;
    (void)enabled;
#endif
    return 0;
}
#define NX_STM32_IRQ_CONTROL(irq, enabled) timer_irq_control(irq, enabled)
#endif

#ifndef NX_STM32_IRQ_ISOLATE
/** \brief Isolate this active reserved handler, retaining its ownership. */
static int timer_irq_isolate(IRQn_Type irq) {
#ifndef NEXUS_STM32_MODEL
    NVIC_DisableIRQ(irq);
    NVIC_ClearPendingIRQ(irq);
    nx_arch_dsb();
    nx_arch_isb();
    /* The caller is this vector: its own ACTIVE bit must remain set until
     * exception return. Task withdrawal still requires ACTIVE to clear. */
    if (NVIC_GetEnableIRQ(irq) != 0U || NVIC_GetPendingIRQ(irq) != 0U)
        return -1;
#else
    (void)irq;
#endif
    return 0;
}
#define NX_STM32_IRQ_ISOLATE(irq) timer_irq_isolate(irq)
#endif

/** \brief Retain masks and effects after loss of the monotonic time domain. */
static _Noreturn void timer_park(void) {
    (void)nx_arch_irq_save();
    nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_IO, __FILE__, __LINE__);
    for (;;) {
    }
}

/** \brief A critical write is established only after a barrier and readback. */
static bool write_checked(volatile uint32_t* reg, uint32_t value,
                          uint32_t mask) {
    NX_STM32_WRITE(reg, value);
    nx_arch_dsb();
    return (*reg & mask) == (value & mask);
}

/** \brief Exclude retained slave, trigger, channel and DMA-burst owners. */
static bool timer_mode_idle(const TIM_TypeDef* timer) {
    return timer->CR1 == 0U && timer->CR2 == 0U && timer->SMCR == 0U &&
           timer->DIER == 0U && timer->CCMR1 == 0U && timer->CCMR2 == 0U &&
           timer->CCER == 0U && timer->DCR == 0U;
}

/** \brief Check cold ownership before any global clock or Flash change. */
nx_result_t nx_stm32_system_preflight(const nx_stm32_system_t* system) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() ||
        nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (system == NULL || system->rcc == NULL || system->flash == NULL ||
        system->power == NULL || system->timer == NULL) {
        return NX_ERROR_INVALID;
    }
    uint32_t clock = system->rcc->CR;
    uint32_t bus_mask = (uint32_t)(RCC_CFGR_SW | RCC_CFGR_SWS | RCC_CFGR_HPRE |
                                   RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);
    if (system->started || system->remaining_effects != 0U ||
        (clock & (RCC_CR_HSION | RCC_CR_HSIRDY)) !=
            (RCC_CR_HSION | RCC_CR_HSIRDY) ||
        (clock & (RCC_CR_HSEON | RCC_CR_HSEBYP | RCC_CR_CSSON | RCC_CR_PLLON |
                  RCC_CR_PLLRDY | RCC_CR_PLLI2SON | RCC_CR_PLLI2SRDY)) != 0U ||
        (system->rcc->CFGR & bus_mask) != 0U ||
        (system->rcc->SSCGR & RCC_SSCGR_SSCGEN) != 0U ||
        (system->rcc->APB1ENR & RCC_APB1ENR_TIM2EN) != 0U ||
        (system->rcc->APB1RSTR & RCC_APB1RSTR_TIM2RST) != 0U ||
        !timer_mode_idle(system->timer) ||
        (system->flash->SR & FLASH_SR_BSY) != 0U ||
        !NX_STM32_IRQ_AVAILABLE(TIM2_IRQn)) {
        return NX_ERROR_BUSY;
    }
    return NX_SUCCESS;
}

#if defined(NEXUS_MEMORY_CCM) && NEXUS_MEMORY_CCM
#ifndef NX_STM32_BOOT_FAILURE
/** \brief Fail before constructors without observing initialized global RAM. */
static _Noreturn void boot_failure(uint32_t reason) {
    volatile uint32_t retained_reason = reason;
    (void)nx_arch_irq_save();
    for (;;) {
        (void)retained_reason;
        __NOP();
    }
}
#define NX_STM32_BOOT_FAILURE(reason) boot_failure(reason)
#endif
#endif

/** \brief Wait for one hardware condition within an explicit boot poll bound.
 */
static bool wait_bits(nx_stm32_system_t* system, volatile uint32_t* reg,
                      uint32_t mask, uint32_t value, uint32_t limit) {
    while (limit-- != 0U) {
        NX_STM32_POLL(system);
        if ((*reg & mask) == value) {
            return true;
        }
    }
    return false;
}

/** \brief Restore HSI only after its ready and source status are observed. */
static int restore_hsi(nx_stm32_system_t* system, uint32_t limit) {
    NX_STM32_WRITE(&system->rcc->CR, system->rcc->CR | RCC_CR_HSION);
    if (!wait_bits(system, &system->rcc->CR, RCC_CR_HSIRDY, RCC_CR_HSIRDY,
                   limit)) {
        return -1;
    }
    NX_STM32_WRITE(&system->rcc->CFGR,
                   system->rcc->CFGR & ~(uint32_t)RCC_CFGR_SW);
    if (!wait_bits(system, &system->rcc->CFGR, RCC_CFGR_SWS, 0U, limit)) {
        return -1;
    }
    NX_STM32_WRITE(&system->rcc->CR,
                   system->rcc->CR & ~(uint32_t)(RCC_CR_PLLON | RCC_CR_HSEON));
    if (!wait_bits(system, &system->rcc->CR, RCC_CR_PLLRDY | RCC_CR_HSERDY, 0U,
                   limit)) {
        return -1;
    }
    if (!write_checked(
            &system->rcc->CFGR,
            system->rcc->CFGR &
                ~(uint32_t)(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2),
            RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2)) {
        return -1;
    }
    system->core_hz = 16000000U;
    if (!write_checked(&system->rcc->PLLCFGR, system->saved_pll, UINT32_MAX) ||
        !write_checked(&system->flash->ACR, system->saved_flash, UINT32_MAX)) {
        return -1;
    }
    if (system->power_snapshotted &&
        !write_checked(&system->power->CR,
                       (system->power->CR & ~(uint32_t)PWR_CR_VOS) |
                           system->saved_power,
                       PWR_CR_VOS)) {
        return -1;
    }
    if (system->owned_power_clock &&
        !write_checked(&system->rcc->APB1ENR,
                       system->rcc->APB1ENR & ~(uint32_t)RCC_APB1ENR_PWREN,
                       RCC_APB1ENR_PWREN)) {
        return -1;
    }
    system->remaining_effects &= ~NX_STM32_EFFECT_CLOCK;
    return 0;
}

/** \brief Initialize CPU registers without touching not-yet-initialized RAM. */
void SystemInit(void) {
#ifndef NEXUS_STM32_MODEL
    SCB->CPACR |= (0xFU << 20U);
    __DSB();
    __ISB();
    RCC->CR |= RCC_CR_HSION;
    RCC->CFGR = 0U;
    RCC->CR &= ~(uint32_t)(RCC_CR_HSEON | RCC_CR_CSSON | RCC_CR_PLLON);
    RCC->PLLCFGR = 0x24003010U;
    RCC->CIR = 0U;
    SCB->VTOR = FLASH_BASE;
    NVIC_SetPriorityGrouping(3U);
#endif
#if defined(NEXUS_MEMORY_CCM) && NEXUS_MEMORY_CCM
    /* Official ST startup has not copied DATA or cleared BSS yet. The plan
     * comes only from linker symbols/Flash and uses the internal SRAM stack. */
    RCC->AHB1ENR |= RCC_AHB1ENR_CCMDATARAMEN;
    nx_arch_dsb();
    if ((RCC->AHB1ENR & RCC_AHB1ENR_CCMDATARAMEN) == 0U) {
        NX_STM32_BOOT_FAILURE(1U);
    }
    if (!nx_stm32_memory_boot()) {
        NX_STM32_BOOT_FAILURE(2U);
    }
    nx_arch_dsb();
#endif
}

/** \brief Establish the one supported fixed clock profile and TIM2 source. */
nx_stm32_start_result_t nx_stm32_system_start(nx_stm32_system_t* system,
                                              uint32_t poll_limit) {
    nx_stm32_start_result_t result = {0, 0, 0U};
    if (poll_limit == 0U || nx_stm32_system_preflight(system) != NX_SUCCESS) {
        result.primary = -2;
        return result;
    }
    system->timer_error = NX_SUCCESS;
    system->reset_cause = system->rcc->CSR;
    system->saved_pll = system->rcc->PLLCFGR;
    system->saved_flash = system->flash->ACR;
    system->owned_power_clock =
        (system->rcc->APB1ENR & RCC_APB1ENR_PWREN) == 0U;
    system->power_snapshotted = false;
    system->remaining_effects = NX_STM32_EFFECT_CLOCK;
    if (!write_checked(&system->rcc->APB1ENR,
                       system->rcc->APB1ENR | RCC_APB1ENR_PWREN,
                       RCC_APB1ENR_PWREN)) {
        result.primary = -6;
        goto rollback;
    }
    system->saved_power = system->power->CR & PWR_CR_VOS;
    system->power_snapshotted = true;
    if (!write_checked(&system->power->CR, system->power->CR | PWR_CR_VOS,
                       PWR_CR_VOS) ||
        !write_checked(&system->flash->ACR,
                       FLASH_ACR_LATENCY_5WS | FLASH_ACR_ICEN | FLASH_ACR_DCEN |
                           FLASH_ACR_PRFTEN,
                       FLASH_ACR_LATENCY | FLASH_ACR_ICEN | FLASH_ACR_DCEN |
                           FLASH_ACR_PRFTEN)) {
        result.primary = -6;
        goto rollback;
    }
    NX_STM32_WRITE(&system->rcc->CR, system->rcc->CR | RCC_CR_HSEON);
    if (!wait_bits(system, &system->rcc->CR, RCC_CR_HSERDY, RCC_CR_HSERDY,
                   poll_limit)) {
        result.primary = -3;
        goto rollback;
    }
    if (!write_checked(&system->rcc->PLLCFGR,
                       8U | (336U << 6U) | RCC_PLLCFGR_PLLSRC_HSE | (7U << 24U),
                       UINT32_MAX) ||
        !write_checked(&system->rcc->CFGR,
                       RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV2,
                       RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2)) {
        result.primary = -6;
        goto rollback;
    }
    NX_STM32_WRITE(&system->rcc->CR, system->rcc->CR | RCC_CR_PLLON);
    if (!wait_bits(system, &system->rcc->CR, RCC_CR_PLLRDY, RCC_CR_PLLRDY,
                   poll_limit)) {
        result.primary = -4;
        goto rollback;
    }
    NX_STM32_WRITE(&system->rcc->CFGR, system->rcc->CFGR | RCC_CFGR_SW_PLL);
    if (!wait_bits(system, &system->rcc->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_PLL,
                   poll_limit)) {
        result.primary = -5;
        goto rollback;
    }
    system->remaining_effects |= NX_STM32_EFFECT_TIMER_GATE;
    if (!write_checked(&system->rcc->APB1ENR,
                       system->rcc->APB1ENR | RCC_APB1ENR_TIM2EN,
                       RCC_APB1ENR_TIM2EN)) {
        result.primary = -6;
        goto rollback;
    }
    /* Gated MMIO may have read as zero during preflight. A foreign timer
     * discovered now lends no register or IRQ ownership to cleanup. */
    if (!timer_mode_idle(system->timer) ||
        (system->rcc->APB1RSTR & RCC_APB1RSTR_TIM2RST) != 0U) {
        result.primary = -6;
        goto rollback;
    }
    system->remaining_effects |= NX_STM32_EFFECT_TIMER;
    if (!write_checked(&system->rcc->APB1RSTR,
                       system->rcc->APB1RSTR | RCC_APB1RSTR_TIM2RST,
                       RCC_APB1RSTR_TIM2RST) ||
        !write_checked(&system->rcc->APB1RSTR,
                       system->rcc->APB1RSTR & ~(uint32_t)RCC_APB1RSTR_TIM2RST,
                       RCC_APB1RSTR_TIM2RST) ||
        !timer_mode_idle(system->timer)) {
        result.primary = -6;
        goto rollback;
    }
    bool valid = write_checked(&system->timer->CR1, 0U, UINT32_MAX) &&
                 write_checked(&system->timer->PSC, 83U, UINT32_MAX) &&
                 write_checked(&system->timer->ARR, UINT32_MAX, UINT32_MAX) &&
                 write_checked(&system->timer->SR, 0U, UINT32_MAX);
    if (!valid) {
        result.primary = -6;
        goto rollback;
    }
    /* MMS=reset exports UG as TRGO. While CEN is zero, MMS=enable keeps
     * TRGO quiet through shadow transfer. Restore reset before enabling. */
    system->remaining_effects |= NX_STM32_EFFECT_TIMER_QUIET;
    if (!write_checked(&system->timer->CR2, TIM_CR2_MMS_0, TIM_CR2_MMS)) {
        result.primary = -6;
        goto rollback;
    }
    /* Observe a fresh update event before accepting the prescaler shadow.
     * An old UIF or a refused UG cannot establish this new time epoch. */
    NX_STM32_WRITE(&system->timer->EGR, TIM_EGR_UG);
    nx_arch_dsb();
    if (!wait_bits(system, &system->timer->SR, TIM_SR_UIF, TIM_SR_UIF,
                   poll_limit) ||
        !write_checked(&system->timer->SR, 0U, UINT32_MAX) ||
        !write_checked(&system->timer->CNT, 0U, UINT32_MAX) ||
        !write_checked(&system->timer->CR2, 0U, TIM_CR2_MMS)) {
        result.primary = -6;
        goto rollback;
    }
    system->remaining_effects &= ~NX_STM32_EFFECT_TIMER_QUIET;
    system->overflow_us = 0U;
    valid = write_checked(&system->timer->DIER, TIM_DIER_UIE, UINT32_MAX) &&
            write_checked(&system->timer->CR1, TIM_CR1_CEN, UINT32_MAX) &&
            valid;
    if (!valid || NX_STM32_IRQ_CONTROL(TIM2_IRQn, true) != 0) {
        result.primary = -6;
        goto rollback;
    }
#if defined(NEXUS_F407_I2S2_SLAVE_TX_SELECTED) &&                              \
    NEXUS_F407_I2S2_SLAVE_TX_SELECTED
    /* Normal cold-reset assembly excludes every running peripheral/master.
     * Enabling a gated domain is not proof that a foreign old EN was idle.
     * Shared gates live with Platform, beyond every pin/stream-local owner. */
    uint32_t shared = RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_DMA1EN;
    uint32_t missing = shared & ~system->rcc->AHB1ENR;
    if ((missing & RCC_AHB1ENR_GPIOBEN) != 0U)
        system->remaining_effects |= NX_STM32_EFFECT_GPIOB_GATE;
    if ((missing & RCC_AHB1ENR_DMA1EN) != 0U)
        system->remaining_effects |= NX_STM32_EFFECT_DMA1_GATE;
    if (!write_checked(&system->rcc->AHB1ENR, system->rcc->AHB1ENR | shared,
                       shared)) {
        result.primary = -6;
        goto rollback;
    }
#endif
    system->core_hz = 168000000U;
    system->started = true;
    return result;
rollback:
    result.cleanup = nx_stm32_system_stop(system, poll_limit);
    result.remaining_effects = system->remaining_effects;
    return result;
}

/** \brief Stop timer access before attempting clock cleanup. */
int nx_stm32_system_stop(nx_stm32_system_t* system, uint32_t poll_limit) {
    if (system == NULL || system->rcc == NULL || system->timer == NULL ||
        system->flash == NULL || system->power == NULL || poll_limit == 0U) {
        return -2;
    }
#if defined(NEXUS_TIMEBASE_LOWPOWER) && NEXUS_TIMEBASE_LOWPOWER
    if (nx_stm32_lowpower_busy()) {
        return -1;
    }
#endif
#if defined(NEXUS_F407_I2S2_SLAVE_TX_SELECTED) &&                              \
    NEXUS_F407_I2S2_SLAVE_TX_SELECTED
    /* All Platform consumers, including external HAL users, have joined.
     * Retain attempted additions until their actual withdrawal reads back. */
    uint32_t shared = 0U;
    if ((system->remaining_effects & NX_STM32_EFFECT_GPIOB_GATE) != 0U)
        shared |= RCC_AHB1ENR_GPIOBEN;
    if ((system->remaining_effects & NX_STM32_EFFECT_DMA1_GATE) != 0U)
        shared |= RCC_AHB1ENR_DMA1EN;
    if (shared != 0U) {
        if (!write_checked(&system->rcc->AHB1ENR,
                           system->rcc->AHB1ENR & ~shared, shared))
            return -1;
        system->remaining_effects &=
            ~(NX_STM32_EFFECT_GPIOB_GATE | NX_STM32_EFFECT_DMA1_GATE);
    }
#endif
    if ((system->remaining_effects & NX_STM32_EFFECT_TIMER) != 0U) {
        system->started = false;
        /* Initial withdrawal is best effort: an asserted source can repend
         * while it is still enabled. Its final proof follows source clear. */
        (void)NX_STM32_IRQ_CONTROL(TIM2_IRQn, false);
        if (!write_checked(&system->rcc->APB1RSTR,
                           system->rcc->APB1RSTR &
                               ~(uint32_t)RCC_APB1RSTR_TIM2RST,
                           RCC_APB1RSTR_TIM2RST) ||
            !write_checked(&system->timer->DIER, 0U, UINT32_MAX) ||
            !write_checked(&system->timer->CR1, 0U, UINT32_MAX) ||
            !write_checked(&system->timer->SR, 0U, UINT32_MAX)) {
            return -1;
        }
        if ((system->remaining_effects & NX_STM32_EFFECT_TIMER_QUIET) != 0U) {
            if (!write_checked(&system->timer->CR2, 0U, TIM_CR2_MMS))
                return -1;
            system->remaining_effects &= ~NX_STM32_EFFECT_TIMER_QUIET;
        }
        /* The source can repend after the first vector withdrawal. Only a
         * stopped, cleared peripheral permits final vector/owner release. */
        if (NX_STM32_IRQ_CONTROL(TIM2_IRQn, false) != 0)
            return -1;
        system->remaining_effects &= ~NX_STM32_EFFECT_TIMER;
    }
    if ((system->remaining_effects & NX_STM32_EFFECT_TIMER_GATE) != 0U) {
        if (!write_checked(&system->rcc->APB1ENR,
                           system->rcc->APB1ENR & ~(uint32_t)RCC_APB1ENR_TIM2EN,
                           RCC_APB1ENR_TIM2EN)) {
            return -1;
        }
        system->remaining_effects &= ~NX_STM32_EFFECT_TIMER_GATE;
    }
    if ((system->remaining_effects & NX_STM32_EFFECT_CLOCK) != 0U) {
        return restore_hsi(system, poll_limit);
    }
    return 0;
}

/** \brief Count a hardware overflow once, clearing only the update source. */
void nx_stm32_system_timer_irq(nx_stm32_system_t* system) {
    if (system != NULL &&
        (system->started ||
         (system->remaining_effects & NX_STM32_EFFECT_TIMER) != 0U) &&
        (system->timer->SR & TIM_SR_UIF) != 0U) {
        if (system->timer_error != NX_SUCCESS)
            return;
        NX_STM32_TIMEBASE_ACK(system, ~(uint32_t)TIM_SR_UIF);
        nx_arch_dsb();
        if ((system->timer->SR & TIM_SR_UIF) != 0U) {
            /* An unconfirmed wrap invalidates all old deadlines. Retain the
             * timer and clock through terminal observation; never count twice.
             */
            system->started = false;
            system->timer_error = NX_ERROR_IO;
            (void)write_checked(&system->timer->DIER, 0U, UINT32_MAX);
            (void)write_checked(&system->timer->CR1, 0U, TIM_CR1_CEN);
            (void)NX_STM32_IRQ_ISOLATE(TIM2_IRQn);
            /* Isolation cannot recover deadlines already held by consumers.
             * Always observe the lost domain and retain terminal ownership. */
            timer_park();
        }
        /* Retained IRQs still acknowledge their level source during failed
         * withdrawal; that does not make a withdrawn epoch valid again. */
        if (system->started) {
            system->overflow_us += (UINT64_C(1) << 32U);
        }
    }
}

/** \brief Fold pending overflow into one interrupt-protected timer snapshot. */
uint64_t nx_stm32_system_now(nx_stm32_system_t* system) {
    if (system == NULL || !system->started) {
        return 0U;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    /* A legal timer IRQ may have ended this epoch before exclusion. */
    if (!system->started) {
        nx_arch_irq_restore(mask);
        return 0U;
    }
    uint64_t high = system->overflow_us;
    uint32_t count = system->timer->CNT;
    if ((system->timer->SR & TIM_SR_UIF) != 0U) {
        high += (UINT64_C(1) << 32U);
        count = system->timer->CNT;
    }
    nx_arch_irq_restore(mask);
    return high + count;
}

/** \brief Validate observed density without pretending DEV_ID encodes package.
 */
nx_result_t nx_stm32_validate_identity(uint32_t expected_flash_bytes) {
    if (expected_flash_bytes != 524288U && expected_flash_bytes != 1048576U) {
        return NX_ERROR_INVALID;
    }
#ifndef NX_STM32_FLASH_KIB
#define NX_STM32_FLASH_KIB() (*(volatile const uint16_t*)FLASHSIZE_BASE)
#define NX_STM32_DEVICE_ID() (DBGMCU->IDCODE)
#endif
    uint32_t observed_bytes = (uint32_t)NX_STM32_FLASH_KIB() * 1024U;
    if (observed_bytes != expected_flash_bytes ||
        (NX_STM32_DEVICE_ID() & 0xFFFU) != 0x413U) {
        return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}
