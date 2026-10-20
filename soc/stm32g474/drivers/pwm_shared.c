/**
 * \file            pwm_shared.c
 * \brief           One fixed TIM3 base with direct readonly PWM channel faces
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32g474_pwm_shared.h"

#define SHARED_PADS ((1U << 6U) | (1U << 7U))
#define SHARED_MODES                                                           \
    ((uint32_t)(TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1PE |        \
                TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2PE))

static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool span(const void* value, size_t size) {
    return value != NULL && size != 0U &&
           (uintptr_t)value <= UINTPTR_MAX - size;
}
static bool overlap(const void* first, size_t first_size, const void* second,
                    size_t second_size) {
    return (uintptr_t)first < (uintptr_t)second + second_size &&
           (uintptr_t)second < (uintptr_t)first + first_size;
}
/** \brief Reject owned control spans before interpreting caller storage. */
static bool control(const void* data, size_t size) {
    return overlap(data, size, TIM3, sizeof(*TIM3)) ||
           overlap(data, size, GPIOA, sizeof(*GPIOA)) ||
           overlap(data, size, RCC, sizeof(*RCC)) ||
           overlap(data, size, FLASH, sizeof(*FLASH)) ||
           overlap(data, size, PWR, sizeof(*PWR)) ||
           overlap(data, size, TIM2, sizeof(*TIM2)) ||
           (g_nx_stm32g474_system.timer != NULL &&
            overlap(data, size, g_nx_stm32g474_system.timer,
                    sizeof(*g_nx_stm32g474_system.timer))) ||
           overlap(data, size, &g_nx_stm32g474_system,
                   sizeof(g_nx_stm32g474_system)) ||
           overlap(data, size, &nx_stm32g474_pwm_shared_ops,
                   sizeof(nx_stm32g474_pwm_shared_ops)) ||
           overlap(data, size, (const void*)(uintptr_t)0xe000e000U, 4096U);
}
static bool owner_storage(const nx_stm32g474_pwm_shared_state_t* owner) {
    return span(owner, sizeof(*owner)) &&
           (uintptr_t)owner % _Alignof(nx_stm32g474_pwm_shared_state_t) == 0U &&
           !control(owner, sizeof(*owner));
}
static bool config_storage(const nx_stm32g474_pwm_shared_state_t* owner,
                           const nx_stm32g474_pwm_shared_config_t* config) {
    return span(config, sizeof(*config)) &&
           (uintptr_t)config % _Alignof(nx_stm32g474_pwm_shared_config_t) ==
               0U &&
           !control(config, sizeof(*config)) &&
           !overlap(config, sizeof(*config), owner, sizeof(*owner));
}
static bool valid_config(const nx_stm32g474_pwm_shared_config_t* config) {
    return config->tick_hz != 0U && 170000000U % config->tick_hz == 0U &&
           170000000U / config->tick_hz <= 65536U &&
           config->period_ticks != 0U && config->period_ticks <= 65535U &&
           config->duty_ticks[0] <= config->period_ticks &&
           config->duty_ticks[1] <= config->period_ticks &&
           (config->inactive & ~SHARED_PADS) == 0U && config->poll_limit != 0U;
}
static const nx_stm32g474_pwm_shared_channel_t* reference(const void* context) {
    const nx_stm32g474_pwm_shared_channel_t* ref = context;
    if (!span(ref, sizeof(*ref)) ||
        (uintptr_t)ref % _Alignof(nx_stm32g474_pwm_shared_channel_t) != 0U ||
        control(ref, sizeof(*ref)))
        return NULL;
    if (!owner_storage(ref->owner) ||
        (ref->channel != 1U && ref->channel != 2U) ||
        overlap(ref, sizeof(*ref), ref->owner, sizeof(*ref->owner)))
        return NULL;
    return ref;
}
/** \brief Access proof stays usable while a failed reset awaits release. */
static bool access_ready(void) {
    RCC_TypeDef* rcc = g_nx_stm32g474_system.rcc;
    return rcc == RCC &&
           (NX_G474_READ(&rcc->APB1ENR1) & RCC_APB1ENR1_TIM3EN) != 0U &&
           (NX_G474_READ(&rcc->AHB2ENR) & RCC_AHB2ENR_GPIOAEN) != 0U;
}
static bool clock_ready(void) {
    return g_nx_stm32g474_system.started &&
           g_nx_stm32g474_system.core_hz == 170000000U && access_ready() &&
           (NX_G474_READ(&RCC->APB1RSTR1) & RCC_APB1RSTR1_TIM3RST) == 0U;
}
static bool checked(volatile uint32_t* reg, uint32_t value) {
    NX_G474_WRITE(reg, value);
    nx_arch_dsb();
    return NX_G474_READ(reg) == value;
}
static bool cold(void) {
    return NX_G474_READ(&TIM3->CR1) == 0U && NX_G474_READ(&TIM3->CR2) == 0U &&
           NX_G474_READ(&TIM3->SMCR) == 0U && NX_G474_READ(&TIM3->DIER) == 0U &&
           NX_G474_READ(&TIM3->CCER) == 0U &&
           NX_G474_READ(&TIM3->CCMR1) == 0U && NX_G474_READ(&TIM3->CCMR2) == 0U;
}
/** \brief The enabled-CEN master selection keeps a stopped UG off TRGO. */
static nx_result_t load_base(nx_stm32g474_pwm_shared_state_t* owner) {
    if (!checked(&TIM3->CR2, TIM_CR2_MMS_0) || !checked(&TIM3->SR, 0U))
        return NX_ERROR_IO;
    NX_G474_WRITE(&TIM3->EGR, TIM_EGR_UG);
    nx_arch_dsb();
    uint32_t remaining = owner->config->poll_limit;
    do {
        NX_G474_PWM_SHARED_POLL(owner);
        if ((NX_G474_READ(&TIM3->SR) & TIM_SR_UIF) != 0U)
            break;
    } while (--remaining != 0U);
    if ((NX_G474_READ(&TIM3->SR) & TIM_SR_UIF) == 0U)
        return NX_ERROR_TIMEOUT;
    if (!checked(&TIM3->SR, 0U) || !checked(&TIM3->CR2, 0U) ||
        !checked(&TIM3->CNT, 0U))
        return NX_ERROR_IO;
    return NX_SUCCESS;
}

nx_result_t nx_stm32g474_pwm_shared_initialize(
    nx_stm32g474_pwm_shared_state_t* owner,
    const nx_stm32g474_pwm_shared_config_t* config) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!owner_storage(owner) || !config_storage(owner, config) ||
        !valid_config(config))
        return NX_ERROR_INVALID;
    if (owner->retained || owner->initialized || owner->running_mask != 0U ||
        owner->idle.retained || owner->idle.initialized)
        return NX_ERROR_BUSY;
    if (!clock_ready())
        return NX_ERROR_STATE;
    if (!NX_G474_IRQ_AVAILABLE(TIM3_IRQn) || !cold())
        return NX_ERROR_BUSY;
    owner->config = config;
    owner->retained = true;
    owner->fault = false;
    owner->duty_ticks[0] = config->duty_ticks[0];
    owner->duty_ticks[1] = config->duty_ticks[1];
    owner->idle.registers = GPIOA;
    owner->idle.mask = SHARED_PADS;
    owner->idle.output = true;
    nx_result_t result =
        nx_stm32g474_gpio_initialize(&owner->idle, config->inactive);
    if (result != NX_SUCCESS)
        return result;
    if (!checked(&TIM3->CR1, TIM_CR1_ARPE) ||
        !checked(&TIM3->PSC, 170000000U / config->tick_hz - 1U) ||
        !checked(&TIM3->ARR, config->period_ticks - 1U) ||
        !checked(&TIM3->CCMR1, SHARED_MODES) ||
        !checked(&TIM3->CCR1, owner->duty_ticks[0]) ||
        !checked(&TIM3->CCR2, owner->duty_ticks[1]))
        return NX_ERROR_IO;
    result = load_base(owner);
    if (result == NX_SUCCESS)
        owner->initialized = true;
    return result;
}

static uint32_t enabled_channels(uint8_t mask) {
    return ((mask & 1U) != 0U ? (uint32_t)TIM_CCER_CC1E : 0U) |
           ((mask & 2U) != 0U ? (uint32_t)TIM_CCER_CC2E : 0U);
}
static bool live(const nx_stm32g474_pwm_shared_state_t* owner) {
    return owner->retained && owner->initialized && !owner->fault &&
           owner->idle.retained && owner->idle.initialized &&
           config_storage(owner, owner->config) && clock_ready();
}
/** \brief The immutable base and both staged shadows have one authority. */
static bool profile(const nx_stm32g474_pwm_shared_state_t* owner) {
    uint32_t tick_hz = owner->config->tick_hz;
    uint32_t period = owner->config->period_ticks;
    return tick_hz != 0U && period != 0U && NX_G474_IRQ_AVAILABLE(TIM3_IRQn) &&
           NX_G474_READ(&TIM3->CR1) ==
               (TIM_CR1_ARPE |
                (owner->running_mask != 0U ? TIM_CR1_CEN : 0U)) &&
           NX_G474_READ(&TIM3->CR2) == 0U && NX_G474_READ(&TIM3->SMCR) == 0U &&
           NX_G474_READ(&TIM3->DIER) == 0U &&
           NX_G474_READ(&TIM3->PSC) == 170000000U / tick_hz - 1U &&
           NX_G474_READ(&TIM3->ARR) == period - 1U &&
           NX_G474_READ(&TIM3->CCMR1) == SHARED_MODES &&
           NX_G474_READ(&TIM3->CCMR2) == 0U &&
           NX_G474_READ(&TIM3->CCER) == enabled_channels(owner->running_mask) &&
           NX_G474_READ(&TIM3->CCR1) == owner->duty_ticks[0] &&
           NX_G474_READ(&TIM3->CCR2) == owner->duty_ticks[1];
}
static nx_result_t set(void* context, uint32_t period, uint32_t duty) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    const nx_stm32g474_pwm_shared_channel_t* ref = reference(context);
    if (ref == NULL || period == 0U || period > 65535U || duty > period)
        return NX_ERROR_INVALID;
    nx_stm32g474_pwm_shared_state_t* owner = ref->owner;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    nx_result_t result = NX_ERROR_STATE;
    if (live(owner) && period == owner->config->period_ticks &&
        profile(owner)) {
        volatile uint32_t* compare =
            ref->channel == 1U ? &TIM3->CCR1 : &TIM3->CCR2;
        if (checked(compare, duty)) {
            owner->duty_ticks[ref->channel - 1U] = duty;
            result = NX_SUCCESS;
        } else {
            owner->fault = true;
            result = NX_ERROR_IO;
        }
    }
    nx_arch_irq_restore(guard);
    return result;
}
/** \brief Safe output preparation touches only the specified owned pads. */
static nx_result_t idle_level(nx_stm32g474_pwm_shared_state_t* owner,
                              uint32_t mask) {
    uint32_t inactive = owner->config->inactive & mask;
    NX_G474_WRITE(&GPIOA->BSRR, inactive | ((mask & ~inactive) << 16U));
    nx_arch_dsb();
    if ((NX_G474_READ(&GPIOA->ODR) & mask) != inactive)
        return NX_ERROR_IO;
    uint32_t fields = ((mask & (1U << 6U)) != 0U ? 3U << 12U : 0U) |
                      ((mask & (1U << 7U)) != 0U ? 3U << 14U : 0U);
    uint32_t output = ((mask & (1U << 6U)) != 0U ? 1U << 12U : 0U) |
                      ((mask & (1U << 7U)) != 0U ? 1U << 14U : 0U);
    if (!checked(&GPIOA->OTYPER, NX_G474_READ(&GPIOA->OTYPER) & ~mask) ||
        !checked(&GPIOA->PUPDR, NX_G474_READ(&GPIOA->PUPDR) & ~fields) ||
        !checked(&GPIOA->MODER,
                 (NX_G474_READ(&GPIOA->MODER) & ~fields) | output))
        return NX_ERROR_IO;
    return NX_SUCCESS;
}
static nx_result_t stop(void* context) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    const nx_stm32g474_pwm_shared_channel_t* ref = reference(context);
    if (ref == NULL)
        return NX_ERROR_INVALID;
    nx_stm32g474_pwm_shared_state_t* owner = ref->owner;
    if (!owner->retained)
        return NX_SUCCESS;
    if (!config_storage(owner, owner->config))
        return NX_ERROR_INVALID;
    if (!access_ready())
        return NX_ERROR_STATE;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    uint32_t enable = ref->channel == 1U ? TIM_CCER_CC1E : TIM_CCER_CC2E;
    uint8_t lane = (uint8_t)(1U << (ref->channel - 1U));
    uint8_t remaining = (uint8_t)(owner->running_mask & (uint8_t)~lane);
    bool quiet = checked(&TIM3->CCER, NX_G474_READ(&TIM3->CCER) & ~enable);
    if (quiet && remaining == 0U &&
        (NX_G474_READ(&TIM3->CCER) & (TIM_CCER_CC1E | TIM_CCER_CC2E)) == 0U)
        quiet = checked(&TIM3->CR1,
                        NX_G474_READ(&TIM3->CR1) & ~(uint32_t)TIM_CR1_CEN);
    nx_result_t result =
        quiet ? idle_level(owner, 1U << (ref->channel + 5U)) : NX_ERROR_IO;
    if (result == NX_SUCCESS)
        owner->running_mask = remaining;
    else
        owner->fault = true;
    nx_arch_irq_restore(guard);
    return result;
}
static nx_result_t start(void* context) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    const nx_stm32g474_pwm_shared_channel_t* ref = reference(context);
    if (ref == NULL)
        return NX_ERROR_INVALID;
    nx_stm32g474_pwm_shared_state_t* owner = ref->owner;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!live(owner) || !profile(owner)) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_STATE;
    }
    uint8_t lane = (uint8_t)(1U << (ref->channel - 1U));
    bool running = (owner->running_mask & lane) != 0U;
    nx_arch_irq_restore(guard);
    if (running)
        return NX_SUCCESS;
    nx_result_t result =
        nx_stm32g474_pin_alternate(GPIOA, ref->channel + 5U, 2U);
    if (result == NX_SUCCESS) {
        guard = nx_arch_irq_save();
        if (!live(owner) || !profile(owner)) {
            result = NX_ERROR_STATE;
        } else {
            uint32_t enable =
                ref->channel == 1U ? TIM_CCER_CC1E : TIM_CCER_CC2E;
            if (!checked(&TIM3->CCER, NX_G474_READ(&TIM3->CCER) | enable) ||
                (owner->running_mask == 0U &&
                 !checked(&TIM3->CR1, TIM_CR1_ARPE | TIM_CR1_CEN)))
                result = NX_ERROR_IO;
            else
                owner->running_mask |= lane;
        }
        nx_arch_irq_restore(guard);
    }
    if (result != NX_SUCCESS) {
        owner->fault = true;
        (void)stop(context);
    }
    return result;
}
static nx_result_t state(const void* context, nx_pwm_state_t* output) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    const nx_stm32g474_pwm_shared_channel_t* ref = reference(context);
    if (ref == NULL || !span(output, sizeof(*output)) ||
        (uintptr_t)output % _Alignof(nx_pwm_state_t) != 0U ||
        control(output, sizeof(*output)) ||
        overlap(output, sizeof(*output), ref, sizeof(*ref)) ||
        overlap(output, sizeof(*output), ref->owner, sizeof(*ref->owner)))
        return NX_ERROR_INVALID;
    const nx_stm32g474_pwm_shared_state_t* owner = ref->owner;
    if (!config_storage(owner, owner->config) ||
        overlap(output, sizeof(*output), owner->config, sizeof(*owner->config)))
        return NX_ERROR_INVALID;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    nx_result_t result = NX_ERROR_STATE;
    if (live(owner) && profile(owner)) {
        *output = (nx_pwm_state_t){
            owner->config->period_ticks, owner->duty_ticks[ref->channel - 1U],
            owner->config->tick_hz,
            (owner->running_mask & (1U << (ref->channel - 1U))) != 0U};
        result = NX_SUCCESS;
    }
    nx_arch_irq_restore(guard);
    return result;
}

nx_result_t
nx_stm32g474_pwm_shared_close(nx_stm32g474_pwm_shared_state_t* owner) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!owner_storage(owner))
        return NX_ERROR_INVALID;
    if (!owner->retained)
        return NX_SUCCESS;
    if (!config_storage(owner, owner->config))
        return NX_ERROR_INVALID;
    if (!access_ready())
        return NX_ERROR_STATE;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    bool asserted = checked(&RCC->APB1RSTR1, NX_G474_READ(&RCC->APB1RSTR1) |
                                                 RCC_APB1RSTR1_TIM3RST);
    nx_arch_irq_restore(guard);
    if (!asserted)
        return NX_ERROR_IO;
    guard = nx_arch_irq_save();
    bool released =
        checked(&RCC->APB1RSTR1, NX_G474_READ(&RCC->APB1RSTR1) &
                                     ~(uint32_t)RCC_APB1RSTR1_TIM3RST);
    nx_arch_irq_restore(guard);
    if (!released || !cold() || NX_G474_READ(&TIM3->PSC) != 0U ||
        NX_G474_READ(&TIM3->ARR) != 0xffffU ||
        NX_G474_READ(&TIM3->CCR1) != 0U || NX_G474_READ(&TIM3->CCR2) != 0U ||
        !NX_G474_IRQ_AVAILABLE(TIM3_IRQn))
        return NX_ERROR_IO;
    guard = nx_arch_irq_save();
    nx_result_t result = idle_level(owner, SHARED_PADS);
    nx_arch_irq_restore(guard);
    if (result != NX_SUCCESS)
        return result;
    result = nx_stm32g474_gpio_stop(&owner->idle, owner->config->inactive);
    if (result != NX_SUCCESS)
        return result;
    owner->running_mask = 0U;
    owner->initialized = false;
    owner->retained = false;
    owner->fault = false;
    return NX_SUCCESS;
}
const nx_pwm_ops_t nx_stm32g474_pwm_shared_ops = {set, start, stop, state};
