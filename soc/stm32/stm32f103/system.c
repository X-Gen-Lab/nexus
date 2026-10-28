/**
 * \file            system.c
 * \brief           Reset HSI8 admission and independent F103 TIM2 ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/fault.h"
#include "nexus/core/time.h"
#include "stm32f103_system.h"
#ifndef NX_F103_MODEL
nx_stm32f103_system_t g_nx_stm32f103_system = {
    .rcc = RCC, .flash = FLASH, .timer = TIM2};
#endif
#ifndef NX_F103_FLASH_KIB
#define NX_F103_FLASH_KIB() (*(volatile const uint16_t*)UINT32_C(0x1ffff7e0))
#endif
#define NX_F103_COUNTER_MAX    UINT32_C(65535)
#define NX_F103_COUNTER_PERIOD UINT64_C(65536)
static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool checked(volatile uint32_t* reg, uint32_t value, uint32_t mask) {
    NX_F103_WRITE(reg, value);
    nx_arch_dsb();
    return (NX_F103_READ(reg) & mask) == (value & mask);
}
static bool timer_idle(const nx_stm32f103_system_t* s) {
    return NX_F103_READ(&s->timer->CR1) == 0U &&
           NX_F103_READ(&s->timer->CR2) == 0U &&
           NX_F103_READ(&s->timer->SMCR) == 0U &&
           NX_F103_READ(&s->timer->DIER) == 0U &&
           NX_F103_READ(&s->timer->CCER) == 0U &&
           NX_F103_READ(&s->timer->CCMR1) == 0U &&
           NX_F103_READ(&s->timer->CCMR2) == 0U &&
           NX_F103_READ(&s->timer->DCR) == 0U;
}
static _Noreturn void timer_park(void) {
    (void)nx_arch_irq_save();
    nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_IO, __FILE__, __LINE__);
    for (;;) {
    }
}
void SystemInit(void) {
#ifndef NX_F103_MODEL
    /* The official startup calls this before .data/.bss exist. */
    SCB->VTOR = FLASH_BASE;
    NVIC_SetPriorityGrouping(3U);
    nx_arch_dsb();
    nx_arch_isb();
#endif
}
nx_result_t nx_stm32f103_validate_identity(void) {
    /* ES096 Rev15 2.2.2: normal software cannot authenticate the debug-only
     * IDCODE. RM0008 30.1.1 permits CPU reads of the factory size halfword.
     * This validates density only, never device group, revision or package. */
    return NX_F103_FLASH_KIB() == 128U ? NX_SUCCESS : NX_ERROR_IO;
}
nx_result_t nx_stm32f103_system_preflight(const nx_stm32f103_system_t* s) {
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (s == NULL || s->rcc == NULL || s->flash == NULL || s->timer == NULL) {
        return NX_ERROR_INVALID;
    }
    uint32_t cr = NX_F103_READ(&s->rcc->CR);
    uint32_t cfgr = NX_F103_READ(&s->rcc->CFGR);
    if (s->started || s->remaining_effects != 0U ||
        (cr & (RCC_CR_HSION | RCC_CR_HSIRDY)) !=
            (RCC_CR_HSION | RCC_CR_HSIRDY) ||
        (cr & RCC_CR_HSITRIM) != (16U << RCC_CR_HSITRIM_Pos) ||
        (cr & (RCC_CR_PLLON | RCC_CR_PLLRDY | RCC_CR_HSEON | RCC_CR_HSERDY |
               RCC_CR_HSEBYP | RCC_CR_CSSON)) != 0U ||
        (cfgr & (RCC_CFGR_SW | RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE1 |
                 RCC_CFGR_PPRE2)) != 0U ||
        (NX_F103_READ(&s->rcc->APB1ENR) & RCC_APB1ENR_TIM2EN) != 0U ||
        (NX_F103_READ(&s->rcc->APB1RSTR) & RCC_APB1RSTR_TIM2RST) != 0U ||
        !timer_idle(s) || (NX_F103_READ(&s->flash->SR) & FLASH_SR_BSY) != 0U ||
        !NX_F103_IRQ_AVAILABLE(TIM2_IRQn)) {
        return NX_ERROR_BUSY;
    }
    return NX_SUCCESS;
}
nx_stm32f103_start_result_t nx_stm32f103_system_start(nx_stm32f103_system_t* s,
                                                      uint32_t limit) {
    nx_stm32f103_start_result_t r = {NX_SUCCESS, NX_SUCCESS, 0U};
    r.primary = nx_stm32f103_system_preflight(s);
    if (r.primary != NX_SUCCESS) {
        return r;
    }
    if (limit == 0U) {
        r.primary = NX_ERROR_INVALID;
        return r;
    }
    r.primary = nx_stm32f103_validate_identity();
    if (r.primary != NX_SUCCESS) {
        return r;
    }
    s->reset_cause = NX_F103_READ(&s->rcc->CSR);
    s->timer_error = NX_SUCCESS;
    s->remaining_effects = NX_STM32F103_EFFECT_GATE;
    s->core_hz = 8000000U;
    bool valid = checked(&s->rcc->APB1ENR,
                         NX_F103_READ(&s->rcc->APB1ENR) | RCC_APB1ENR_TIM2EN,
                         RCC_APB1ENR_TIM2EN);
    if (!valid) {
        r.primary = NX_ERROR_IO;
        if (checked(&s->rcc->APB1ENR,
                    NX_F103_READ(&s->rcc->APB1ENR) &
                        ~(uint32_t)RCC_APB1ENR_TIM2EN,
                    RCC_APB1ENR_TIM2EN)) {
            s->remaining_effects = 0U;
        } else {
            r.cleanup = NX_ERROR_IO;
            r.remaining_effects = s->remaining_effects;
        }
        return r;
    }
    /* Gated-off register reads need not reveal retained configuration. Inspect
     * again after enabling access, before UG could emit a foreign trigger. */
    if (!timer_idle(s)) {
        r.primary = NX_ERROR_BUSY;
        if (checked(&s->rcc->APB1ENR,
                    NX_F103_READ(&s->rcc->APB1ENR) &
                        ~(uint32_t)RCC_APB1ENR_TIM2EN,
                    RCC_APB1ENR_TIM2EN)) {
            s->remaining_effects = 0U;
        } else {
            r.cleanup = NX_ERROR_IO;
            r.remaining_effects = s->remaining_effects;
        }
        return r;
    }
    s->remaining_effects |= NX_STM32F103_EFFECT_TIMER;
    valid = checked(&s->timer->CR1, 0U, UINT32_MAX) && valid;
    valid = checked(&s->timer->DIER, 0U, UINT32_MAX) && valid;
    valid = checked(&s->timer->PSC, 7U, 0xffffU) && valid;
    valid = checked(&s->timer->ARR, NX_F103_COUNTER_MAX, NX_F103_COUNTER_MAX) &&
            valid;
    valid = checked(&s->timer->SR, 0U, 0xffffU) && valid;
    if (!valid) {
        r.primary = NX_ERROR_IO;
        goto rollback;
    }
    /* MMS reset exports UG as TRGO. Select CEN while stopped to suppress the
     * prescaler update pulse,then restore reset routing before enabling CEN. */
    if (!checked(&s->timer->CR2, TIM_CR2_MMS_0, UINT32_MAX)) {
        r.primary = NX_ERROR_IO;
        goto rollback;
    }
    NX_F103_WRITE(&s->timer->EGR, TIM_EGR_UG);
    nx_arch_dsb();
    while (limit-- != 0U) {
        NX_F103_SYSTEM_POLL(s);
        if ((NX_F103_READ(&s->timer->SR) & TIM_SR_UIF) != 0U) {
            break;
        }
    }
    if ((NX_F103_READ(&s->timer->SR) & TIM_SR_UIF) == 0U) {
        r.primary = NX_ERROR_TIMEOUT;
        goto rollback;
    }
    if (!checked(&s->timer->SR, 0U, 0xffffU) ||
        !checked(&s->timer->CNT, 0U, NX_F103_COUNTER_MAX)) {
        r.primary = NX_ERROR_IO;
        goto rollback;
    }
    if (!checked(&s->timer->CR2, 0U, UINT32_MAX)) {
        r.primary = NX_ERROR_IO;
        goto rollback;
    }
    s->overflow_us = 0U;
    if (!checked(&s->timer->DIER, TIM_DIER_UIE, UINT32_MAX) ||
        !checked(&s->timer->CR1, TIM_CR1_CEN, UINT32_MAX)) {
        r.primary = NX_ERROR_IO;
        goto rollback;
    }
    r.primary = NX_F103_IRQ_CONTROL(TIM2_IRQn, true, 0U);
    if (r.primary != NX_SUCCESS) {
        goto rollback;
    }
    s->started = true;
    return r;
rollback:
    r.cleanup = nx_stm32f103_system_stop(s, 1U);
    r.remaining_effects = s->remaining_effects;
    return r;
}
nx_result_t nx_stm32f103_system_stop(nx_stm32f103_system_t* s, uint32_t limit) {
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (s == NULL || s->rcc == NULL || s->timer == NULL || limit == 0U) {
        return NX_ERROR_INVALID;
    }
    s->started = false;
    if ((s->remaining_effects & NX_STM32F103_EFFECT_TIMER) != 0U) {
        if (!checked(&s->timer->DIER, 0U, UINT32_MAX) ||
            !checked(&s->timer->CR1, 0U, UINT32_MAX) ||
            !checked(&s->timer->CR2, 0U, UINT32_MAX) ||
            !checked(&s->timer->SR, 0U, 0xffffU)) {
            return NX_ERROR_IO;
        }
        /* Withdraw the source before the final pending-vector proof. A wrap
         * between these operations must not survive the timer lease. */
        nx_result_t irq = NX_F103_IRQ_CONTROL(TIM2_IRQn, false, 0U);
        if (irq != NX_SUCCESS) {
            return irq;
        }
        s->remaining_effects &= ~NX_STM32F103_EFFECT_TIMER;
    }
    /* A gate lease alone never permits touching a foreign retained timer. */
    if ((s->remaining_effects & NX_STM32F103_EFFECT_GATE) != 0U) {
        if (!checked(&s->rcc->APB1ENR,
                     NX_F103_READ(&s->rcc->APB1ENR) &
                         ~(uint32_t)RCC_APB1ENR_TIM2EN,
                     RCC_APB1ENR_TIM2EN)) {
            return NX_ERROR_IO;
        }
        s->remaining_effects &= ~NX_STM32F103_EFFECT_GATE;
    }
    return NX_SUCCESS;
}
void nx_stm32f103_system_timer_irq(nx_stm32f103_system_t* s) {
    if (s == NULL || (s->remaining_effects & NX_STM32F103_EFFECT_TIMER) == 0U ||
        (NX_F103_READ(&s->timer->SR) & TIM_SR_UIF) == 0U) {
        return;
    }
    NX_F103_WRITE(&s->timer->SR, ~(uint32_t)TIM_SR_UIF);
    nx_arch_dsb();
    if ((NX_F103_READ(&s->timer->SR) & TIM_SR_UIF) != 0U) {
        s->started = false;
        s->timer_error = NX_ERROR_IO;
        (void)checked(&s->timer->DIER, 0U, UINT32_MAX);
        (void)checked(&s->timer->CR1, 0U, TIM_CR1_CEN);
        (void)NX_F103_IRQ_CONTROL(TIM2_IRQn, false, 0U);
        timer_park();
    }
    if (s->started) {
        s->overflow_us += NX_F103_COUNTER_PERIOD;
    }
}
uint64_t nx_stm32f103_system_now(nx_stm32f103_system_t* s) {
    if (s == NULL || !s->started || !nx_arch_is_privileged()) {
        return 0U;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (!s->started) {
        nx_arch_irq_restore(saved);
        return 0U;
    }
    uint64_t high = s->overflow_us;
    uint32_t count = NX_F103_READ(&s->timer->CNT) & NX_F103_COUNTER_MAX;
    if ((NX_F103_READ(&s->timer->SR) & TIM_SR_UIF) != 0U) {
        high += NX_F103_COUNTER_PERIOD;
        count = NX_F103_READ(&s->timer->CNT) & NX_F103_COUNTER_MAX;
    }
    nx_arch_irq_restore(saved);
    return high + count;
}
nx_time_us_t nx_time_now_us(void) {
    return nx_stm32f103_system_now(&g_nx_stm32f103_system);
}
void TIM2_IRQHandler(void) {
    nx_stm32f103_system_timer_irq(&g_nx_stm32f103_system);
}

_Static_assert(sizeof(nx_stm32f103_platform_resources_t) == 8U,
               "exact cold receipt RAM budget");
_Static_assert((NX_STM32F103_OWN_GPIOA | NX_STM32F103_OWN_AFIO |
                NX_STM32F103_OWN_SPI1 | NX_STM32F103_OWN_ADC1 |
                NX_STM32F103_OWN_USART2 | NX_STM32F103_OWN_TIM3 |
                NX_STM32F103_OWN_DMA1 | NX_STM32F103_OWN_PINS) <= UINT8_MAX,
               "every ownership bit fits in its receipt");
_Static_assert(UINT8_MAX >= 0xffU, "all validated PA0..PA7 bits fit");
static bool platform_selection_valid(const nx_stm32f103_platform_config_t* c) {
    if (c == NULL) {
        return false;
    }
    uint32_t required = c->exti_lines;
    if ((c->peripherals & NX_STM32F103_PLATFORM_UART) != 0U) {
        required |= 12U;
    }
    if ((c->peripherals & NX_STM32F103_PLATFORM_SPI) != 0U) {
        required |= 240U;
    }
    if ((c->peripherals & NX_STM32F103_PLATFORM_ADC) != 0U) {
        required |= 1U;
    }
    if ((c->peripherals & NX_STM32F103_PLATFORM_PWM) != 0U) {
        required |= 64U;
    }
    return (required & ~(uint32_t)c->pins) == 0U &&
           (c->pins & ~UINT16_C(0xff)) == 0U && (c->exti_lines & ~3U) == 0U &&
           (c->peripherals & ~127U) == 0U &&
           ((c->peripherals & NX_STM32F103_PLATFORM_DMA_TX) == 0U ||
            (c->peripherals & NX_STM32F103_PLATFORM_UART) != 0U) &&
           ((c->peripherals & NX_STM32F103_PLATFORM_DMA_RX) == 0U ||
            (c->peripherals & NX_STM32F103_PLATFORM_DMA_TX) != 0U);
}
static bool platform_pins_available(uint32_t pins) {
    if ((NX_F103_READ(&GPIOA->LCKR) & pins) != 0U) {
        return false;
    }
    uint32_t modes = NX_F103_READ(&GPIOA->CRL);
    for (unsigned pin = 0U; pin < 8U; ++pin) {
        if ((pins & (1U << pin)) != 0U) {
            uint32_t mode = (modes >> (pin * 4U)) & 15U;
            if (mode != 0U && mode != 4U) {
                return false;
            }
        }
    }
    return true;
}
nx_result_t
nx_stm32f103_platform_preflight(const nx_stm32f103_platform_config_t* c) {
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!platform_selection_valid(c)) {
        return NX_ERROR_INVALID;
    }
    uint8_t selected = c->peripherals;
    if (((selected & NX_STM32F103_PLATFORM_DMA_TX) != 0U &&
         !NX_F103_IRQ_AVAILABLE(DMA1_Channel7_IRQn)) ||
        ((selected & NX_STM32F103_PLATFORM_DMA_RX) != 0U &&
         !NX_F103_IRQ_AVAILABLE(DMA1_Channel6_IRQn))) {
        return NX_ERROR_BUSY;
    }
    nx_result_t system = nx_stm32f103_system_preflight(&g_nx_stm32f103_system);
    if (system != NX_SUCCESS) {
        return system;
    }
    uint32_t apb2 = NX_F103_READ(&RCC->APB2ENR);
    if ((selected & NX_STM32F103_PLATFORM_UART) != 0U &&
        ((NX_F103_READ(&RCC->APB1ENR) & RCC_APB1ENR_USART2EN) != 0U ||
         !NX_F103_IRQ_AVAILABLE(USART2_IRQn) ||
         ((apb2 & RCC_APB2ENR_AFIOEN) != 0U &&
          (NX_F103_READ(&AFIO->MAPR) & AFIO_MAPR_USART2_REMAP) != 0U))) {
        return NX_ERROR_BUSY;
    }
    for (unsigned line = 0U; line < 2U; ++line) {
        uint32_t mask = 1U << line;
        if ((c->exti_lines & mask) != 0U &&
            (((NX_F103_READ(&EXTI->IMR) | NX_F103_READ(&EXTI->EMR) |
               NX_F103_READ(&EXTI->RTSR) | NX_F103_READ(&EXTI->FTSR) |
               NX_F103_READ(&EXTI->PR)) &
              mask) != 0U ||
             !NX_F103_IRQ_AVAILABLE((int)EXTI0_IRQn + (int)line))) {
            return NX_ERROR_BUSY;
        }
    }
    if ((selected & NX_STM32F103_PLATFORM_SPI) != 0U &&
        ((apb2 & RCC_APB2ENR_SPI1EN) != 0U ||
         (NX_F103_READ(&RCC->APB2RSTR) & RCC_APB2RSTR_SPI1RST) != 0U ||
         !NX_F103_IRQ_AVAILABLE(SPI1_IRQn))) {
        return NX_ERROR_BUSY;
    }
    if ((selected & NX_STM32F103_PLATFORM_ADC) != 0U &&
        ((apb2 & RCC_APB2ENR_ADC1EN) != 0U ||
         (NX_F103_READ(&RCC->APB2RSTR) & RCC_APB2RSTR_ADC1RST) != 0U ||
         (NX_F103_READ(&RCC->CFGR) & RCC_CFGR_ADCPRE) != 0U ||
         !NX_F103_IRQ_AVAILABLE(ADC1_2_IRQn))) {
        return NX_ERROR_BUSY;
    }
    if ((selected & NX_STM32F103_PLATFORM_PWM) != 0U &&
        ((NX_F103_READ(&RCC->APB1ENR) & RCC_APB1ENR_TIM3EN) != 0U ||
         (NX_F103_READ(&RCC->APB1RSTR) & RCC_APB1RSTR_TIM3RST) != 0U ||
         !NX_F103_IRQ_AVAILABLE(TIM3_IRQn) ||
         ((apb2 & RCC_APB2ENR_AFIOEN) != 0U &&
          (NX_F103_READ(&AFIO->MAPR) & AFIO_MAPR_TIM3_REMAP) != 0U))) {
        return NX_ERROR_BUSY;
    }
    if ((selected & NX_STM32F103_PLATFORM_FLASH) != 0U &&
        !NX_F103_IRQ_AVAILABLE(FLASH_IRQn)) {
        return NX_ERROR_BUSY;
    }
    if (c->pins != 0U && (apb2 & RCC_APB2ENR_IOPAEN) != 0U &&
        !platform_pins_available(c->pins)) {
        return NX_ERROR_BUSY;
    }
    return NX_SUCCESS;
}
static uint32_t platform_pin_fields(uint32_t pins, uint32_t nibble) {
    uint32_t result = 0U;
    for (unsigned pin = 0U; pin < 8U; ++pin) {
        if ((pins & (1U << pin)) != 0U) {
            result |= nibble << (4U * pin);
        }
    }
    return result;
}
/* The private byte keeps matching low APB2 bits and one shifted high byte.
 * Compile-time checks bind this lossless representation to the exact SDK. */
_Static_assert((RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN) ==
                   (NX_STM32F103_OWN_GPIOA | NX_STM32F103_OWN_AFIO),
               "exact APB2 low receipt bits");
_Static_assert(((RCC_APB2ENR_SPI1EN | RCC_APB2ENR_ADC1EN) >> 8U) ==
                   (NX_STM32F103_OWN_SPI1 | NX_STM32F103_OWN_ADC1),
               "exact APB2 high receipt bits");
static uint8_t platform_apb2_receipt(uint32_t gates) {
    const uint32_t low = NX_STM32F103_OWN_GPIOA | NX_STM32F103_OWN_AFIO;
    const uint32_t high = NX_STM32F103_OWN_SPI1 | NX_STM32F103_OWN_ADC1;
    return (uint8_t)((gates & low) | ((gates >> 8U) & high));
}
static uint32_t platform_apb2_gates(uint8_t owned) {
    const uint32_t low = NX_STM32F103_OWN_GPIOA | NX_STM32F103_OWN_AFIO;
    const uint32_t high = NX_STM32F103_OWN_SPI1 | NX_STM32F103_OWN_ADC1;
    return ((uint32_t)owned & low) | (((uint32_t)owned & high) << 8U);
}
nx_result_t nx_stm32f103_platform_resources_acquire(
    nx_stm32f103_platform_resources_t* p,
    const nx_stm32f103_platform_config_t* c) {
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (p == NULL || !platform_selection_valid(c)) {
        return NX_ERROR_INVALID;
    }
    if (p->owned != 0U) {
        return NX_ERROR_BUSY;
    }
    if (!g_nx_stm32f103_system.started ||
        g_nx_stm32f103_system.core_hz != 8000000U) {
        return NX_ERROR_STATE;
    }
    uint8_t selected = c->peripherals;
    /* Selection validation proves that only eight GPIOA bits are stored. */
    p->pins = (uint8_t)c->pins;
    if ((selected & NX_STM32F103_PLATFORM_DMA_TX) != 0U) {
        if ((NX_F103_READ(&RCC->AHBENR) & RCC_AHBENR_DMA1EN) == 0U) {
            p->owned |= NX_STM32F103_OWN_DMA1;
        }
        if (!checked(&RCC->AHBENR,
                     NX_F103_READ(&RCC->AHBENR) | RCC_AHBENR_DMA1EN,
                     RCC_AHBENR_DMA1EN)) {
            return NX_ERROR_IO;
        }
    }
    if (c->pins == 0U) {
        return NX_SUCCESS;
    }
    uint32_t apb2 = RCC_APB2ENR_IOPAEN;
    uint32_t apb1 = 0U;
    if ((selected & (NX_STM32F103_PLATFORM_UART | NX_STM32F103_PLATFORM_SPI |
                     NX_STM32F103_PLATFORM_PWM)) != 0U ||
        c->exti_lines != 0U) {
        apb2 |= RCC_APB2ENR_AFIOEN;
    }
    if ((selected & NX_STM32F103_PLATFORM_SPI) != 0U) {
        apb2 |= RCC_APB2ENR_SPI1EN;
    }
    if ((selected & NX_STM32F103_PLATFORM_ADC) != 0U) {
        apb2 |= RCC_APB2ENR_ADC1EN;
    }
    if ((selected & NX_STM32F103_PLATFORM_UART) != 0U) {
        apb1 |= RCC_APB1ENR_USART2EN;
    }
    if ((selected & NX_STM32F103_PLATFORM_PWM) != 0U) {
        apb1 |= RCC_APB1ENR_TIM3EN;
    }
    p->owned |= platform_apb2_receipt(~NX_F103_READ(&RCC->APB2ENR) & apb2);
    if (apb1 != 0U) {
        uint32_t added = ~NX_F103_READ(&RCC->APB1ENR) & apb1;
        if ((added & RCC_APB1ENR_USART2EN) != 0U) {
            p->owned |= NX_STM32F103_OWN_USART2;
        }
        if ((added & RCC_APB1ENR_TIM3EN) != 0U) {
            p->owned |= NX_STM32F103_OWN_TIM3;
        }
        if (!checked(&RCC->APB1ENR, NX_F103_READ(&RCC->APB1ENR) | apb1, apb1)) {
            return NX_ERROR_IO;
        }
    }
    if (!checked(&RCC->APB2ENR, NX_F103_READ(&RCC->APB2ENR) | apb2, apb2)) {
        return NX_ERROR_IO;
    }
    /* Recheck after gate visibility before capturing or modifying any pad. */
    uint32_t remaps = 0U;
    if ((selected & NX_STM32F103_PLATFORM_UART) != 0U) {
        remaps |= AFIO_MAPR_USART2_REMAP;
    }
    if ((selected & NX_STM32F103_PLATFORM_SPI) != 0U) {
        remaps |= AFIO_MAPR_SPI1_REMAP;
        if (NX_F103_READ(&SPI1->CR1) != 0U || NX_F103_READ(&SPI1->CR2) != 0U ||
            (NX_F103_READ(&SPI1->SR) &
             (SPI_SR_BSY | SPI_SR_RXNE | SPI_SR_OVR | SPI_SR_MODF)) != 0U) {
            return NX_ERROR_BUSY;
        }
    }
    if ((selected & NX_STM32F103_PLATFORM_PWM) != 0U) {
        remaps |= AFIO_MAPR_TIM3_REMAP;
    }
    if ((remaps != 0U && (NX_F103_READ(&AFIO->MAPR) & remaps) != 0U) ||
        !platform_pins_available(c->pins)) {
        return NX_ERROR_BUSY;
    }
    p->saved_crl =
        NX_F103_READ(&GPIOA->CRL) & platform_pin_fields(c->pins, 15U);
    p->saved_odr = (uint8_t)(NX_F103_READ(&GPIOA->ODR) & c->pins);
    p->owned |= NX_STM32F103_OWN_PINS;
    return NX_SUCCESS;
}
nx_result_t
nx_stm32f103_platform_resources_release(nx_stm32f103_platform_resources_t* p) {
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (p == NULL) {
        return NX_ERROR_INVALID;
    }
    if ((p->owned & NX_STM32F103_OWN_DMA1) != 0U) {
        if (!checked(&RCC->AHBENR,
                     NX_F103_READ(&RCC->AHBENR) & ~(uint32_t)RCC_AHBENR_DMA1EN,
                     RCC_AHBENR_DMA1EN)) {
            return NX_ERROR_IO;
        }
        p->owned &= (uint8_t)~NX_STM32F103_OWN_DMA1;
    }
    if ((p->owned & NX_STM32F103_OWN_PINS) != 0U) {
        uint32_t fields = platform_pin_fields(p->pins, 15U);
        if (!checked(&GPIOA->CRL,
                     (NX_F103_READ(&GPIOA->CRL) & ~fields) |
                         platform_pin_fields(p->pins, 4U),
                     fields)) {
            return NX_ERROR_IO;
        }
        NX_F103_WRITE(
            &GPIOA->BSRR,
            (uint32_t)p->saved_odr |
                (((uint32_t)p->pins & ~(uint32_t)p->saved_odr) << 16U));
        nx_arch_dsb();
        if ((NX_F103_READ(&GPIOA->ODR) & p->pins) != p->saved_odr ||
            !checked(&GPIOA->CRL,
                     (NX_F103_READ(&GPIOA->CRL) & ~fields) | p->saved_crl,
                     fields)) {
            return NX_ERROR_IO;
        }
        p->owned &= (uint8_t)~NX_STM32F103_OWN_PINS;
    }
    uint32_t apb1 = 0U;
    if ((p->owned & NX_STM32F103_OWN_USART2) != 0U) {
        apb1 |= RCC_APB1ENR_USART2EN;
    }
    if ((p->owned & NX_STM32F103_OWN_TIM3) != 0U) {
        apb1 |= RCC_APB1ENR_TIM3EN;
    }
    if (apb1 != 0U) {
        if (!checked(&RCC->APB1ENR, NX_F103_READ(&RCC->APB1ENR) & ~apb1,
                     apb1)) {
            return NX_ERROR_IO;
        }
        p->owned &=
            (uint8_t) ~(NX_STM32F103_OWN_USART2 | NX_STM32F103_OWN_TIM3);
    }
    uint32_t apb2 = platform_apb2_gates(p->owned);
    if (apb2 != 0U) {
        if (!checked(&RCC->APB2ENR, NX_F103_READ(&RCC->APB2ENR) & ~apb2,
                     apb2)) {
            return NX_ERROR_IO;
        }
        p->owned &= (uint8_t) ~(NX_STM32F103_OWN_GPIOA | NX_STM32F103_OWN_AFIO |
                                NX_STM32F103_OWN_SPI1 | NX_STM32F103_OWN_ADC1);
    }
    p->pins = 0U;
    return NX_SUCCESS;
}
