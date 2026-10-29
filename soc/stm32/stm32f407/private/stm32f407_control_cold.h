/**
 * \file            stm32f407_control_cold.h
 * \brief           Typed cold EXTI, PWM and ADC construction and withdrawal
 * \author          Nexus Team
 */
#ifndef NX_STM32F407_CONTROL_COLD_H
#define NX_STM32F407_CONTROL_COLD_H
#include "stm32f407_stream.h"

#ifndef NX_PLATFORM_REGISTER_WRITE
#define NX_PLATFORM_REGISTER_WRITE(reg, value) ((reg) = (uint32_t)(value))
#endif

/** \brief Static EXTI line, bounded event storage and selected IRQ policy. */
typedef struct {
    GPIO_TypeDef* gpio;
    nx_exti_event_t* storage;
    size_t capacity;
    nx_exti_edge_t edge;
    uint8_t gpio_index;
    uint8_t line;
    uint8_t priority;
} nx_stm32_exti_config_t;

/** \brief Exact TIM3 channel one counts and declared PA6 inactive level. */
typedef struct {
    uint32_t period_ticks;
    uint32_t duty_ticks;
    uint32_t tick_hz;
    uint16_t prescaler;
    bool inactive_high;
} nx_stm32_pwm_config_t;

/** \brief Immutable independent ADC1 PA0/PA1 sequence and nominal reference. */
typedef struct {
    const uint8_t* channels;
    const uint8_t* sample_times;
    size_t channel_count;
    uint32_t reference_mv;
} nx_stm32_adc_config_t;

/** \brief Exact ADC1/TIM3/DMA2 stream four selection, without mode dispatch. */
typedef struct {
    nx_stm32_adc_config_t adc;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
    uint8_t priority;
} nx_stm32_adc_stream_config_t;

/** \brief All cold helpers require serialized privileged unmasked Thread mode.
 */
static inline bool nx_stm32_control_cold_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

/** \brief Prepare an admitted line; Platform owns its unique pad snapshot.
 * \note Failure retains initialized EXTI ownership until close succeeds.
 */
static inline nx_result_t
nx_stm32_exti_prepare(nx_stm32_exti_state_t* port,
                      const nx_stm32_exti_config_t* config) {
    if (!nx_stm32_control_cold_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL || config == NULL || config->gpio == NULL ||
        config->storage == NULL || config->capacity == 0U ||
        config->capacity > 4096U || config->gpio_index > 8U ||
        (config->line != 0U && config->line != 5U && config->line != 6U) ||
        config->priority > 15U || config->edge < NX_EXTI_RISING ||
        config->edge > NX_EXTI_BOTH)
        return NX_ERROR_INVALID;
    if (port->initialized || port->cold_owned)
        return NX_ERROR_BUSY;
    uint32_t bit = 1U << config->line;
    if (((EXTI->IMR | EXTI->EMR | EXTI->PR) & bit) != 0U)
        return NX_ERROR_BUSY;
    if ((RCC->APB2ENR & RCC_APB2ENR_SYSCFGEN) == 0U)
        return NX_ERROR_STATE;
    port->cold_owned = true;
    nx_result_t status =
        nx_stm32_pin_configure(config->gpio, config->line, 0U, 0U, 0U, false);
    if (status != NX_SUCCESS)
        return status;
    port->registers = EXTI;
    port->gpio = config->gpio;
    port->storage = config->storage;
    port->capacity = config->capacity;
    port->line = config->line;
    port->edge = config->edge;
    status = nx_stm32_exti_initialize(port, SYSCFG, config->gpio_index);
    if (status != NX_SUCCESS)
        return status;
    IRQn_Type irq = config->line == 0U ? EXTI0_IRQn : EXTI9_5_IRQn;
    /* A newly armed line or selected peer now owns any pending hint. */
    nx_arch_dsb();
    nx_arch_isb();
    NVIC_SetPriority(irq, config->priority);
    NVIC_EnableIRQ(irq);
    nx_arch_dsb();
    nx_arch_isb();
    return NVIC_GetPriority(irq) == config->priority &&
                   NVIC_GetEnableIRQ(irq) != 0U && NVIC_GetActive(irq) == 0U
               ? NX_SUCCESS
               : NX_ERROR_IO;
}

/** \brief Drain the actual publisher; final pad restoration stays
 * Platform-owned. */
static inline nx_result_t nx_stm32_exti_close(nx_stm32_exti_state_t* port) {
    if (!nx_stm32_control_cold_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL)
        return NX_ERROR_INVALID;
    if (!port->cold_owned)
        return port->initialized ? NX_ERROR_BUSY : NX_SUCCESS;
    nx_result_t result =
        port->initialized ? nx_stm32_exti_ops.stop(port) : NX_SUCCESS;
    if (result == NX_SUCCESS)
        port->cold_owned = false;
    return result;
}

/** \brief Prepare selected PWM; cold_effects occupies existing private padding.
 * \note Platform serializes pad ownership. Failed clock proof requires close;
 *       the preserved effect bit is cleared only after observed gate removal.
 */
static inline nx_result_t
nx_stm32_pwm_prepare(nx_stm32_pwm_state_t* port,
                     nx_stm32_gpio_state_t* inactive,
                     const nx_stm32_pwm_config_t* config) {
    if (!nx_stm32_control_cold_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL || inactive == NULL || config == NULL ||
        config->period_ticks == 0U || config->period_ticks > 65535U ||
        config->duty_ticks > config->period_ticks || config->tick_hz == 0U ||
        UINT32_C(84000000) / config->tick_hz !=
            (uint32_t)config->prescaler + 1U ||
        UINT32_C(84000000) % config->tick_hz != 0U)
        return NX_ERROR_INVALID;
    if (port->initialized || port->cold_effects != 0U ||
        (RCC->APB1ENR & RCC_APB1ENR_TIM3EN) != 0U)
        return NX_ERROR_BUSY;
    port->cold_effects = 1U;
    NX_PLATFORM_REGISTER_WRITE(RCC->APB1ENR, RCC->APB1ENR | RCC_APB1ENR_TIM3EN);
    nx_arch_dsb();
    if ((RCC->APB1ENR & RCC_APB1ENR_TIM3EN) == 0U)
        return NX_ERROR_IO;
    if ((TIM3->CCER & 1U) != 0U || ((TIM3->CR1 & TIM_CR1_CEN) != 0U &&
                                    (TIM3->PSC != config->prescaler ||
                                     TIM3->ARR != config->period_ticks - 1U)))
        return NX_ERROR_BUSY;
    inactive->registers = GPIOA;
    inactive->mask = 1U << 6U;
    inactive->output = true;
    nx_result_t result = nx_stm32_gpio_initialize(
        inactive, config->inactive_high ? 1U << 6U : 0U);
    if (result != NX_SUCCESS)
        return result;
    port->registers = TIM3;
    port->inactive_gpio = inactive;
    port->inactive_mask = 1U << 6U;
    port->inactive_high = config->inactive_high;
    port->channel = 1U;
    port->period_ticks = config->period_ticks;
    port->duty_ticks = config->duty_ticks;
    port->tick_hz = config->tick_hz;
    port->prescaler = config->prescaler;
    return nx_stm32_pwm_initialize(port);
}

/** \brief Observe inactive PWM before removing the selected timer gate.
 * \note A failed stop never permits Platform to release the pad lease.
 */
static inline nx_result_t nx_stm32_pwm_close(nx_stm32_pwm_state_t* port,
                                             nx_stm32_gpio_state_t* inactive) {
    if (!nx_stm32_control_cold_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL || inactive == NULL)
        return NX_ERROR_INVALID;
    if (port->cold_effects == 0U)
        return port->initialized ? NX_ERROR_BUSY : NX_SUCCESS;
    if (port->initialized && (RCC->APB1ENR & RCC_APB1ENR_TIM3EN) == 0U)
        return NX_ERROR_IO;
    if (port->initialized) {
        nx_result_t result = nx_stm32_pwm_ops.stop(port);
        if (result != NX_SUCCESS)
            return result;
        port->initialized = false;
        inactive->initialized = false;
    }
    if ((port->cold_effects & 1U) != 0U) {
        if ((TIM3->CCER & 0x1111U) != 0U)
            return NX_ERROR_BUSY;
        NX_PLATFORM_REGISTER_WRITE(
            RCC->APB1ENR, RCC->APB1ENR & ~(uint32_t)RCC_APB1ENR_TIM3EN);
        nx_arch_dsb();
        if ((RCC->APB1ENR & RCC_APB1ENR_TIM3EN) != 0U)
            return NX_ERROR_IO;
        port->cold_effects &= (uint8_t)~1U;
    }
    inactive->initialized = false;
    return NX_SUCCESS;
}

/** \brief Acquire ADC1 and analog pads; typed callers choose the actual engine.
 */
static inline nx_result_t
nx_stm32_adc_cold_begin(nx_stm32_adc_state_t* port,
                        const nx_stm32_adc_config_t* config) {
    if (!nx_stm32_control_cold_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL || config == NULL || config->channels == NULL ||
        config->sample_times == NULL || config->channel_count == 0U ||
        config->channel_count > 2U || config->reference_mv == 0U ||
        config->reference_mv > 3600U)
        return NX_ERROR_INVALID;
    for (size_t i = 0U; i < config->channel_count; ++i) {
        if (config->channels[i] != i || config->sample_times[i] > 7U)
            return NX_ERROR_UNSUPPORTED;
    }
    if (port->initialized || port->active || port->cold_effects != 0U ||
        (RCC->APB2ENR & RCC_APB2ENR_ADC1EN) != 0U)
        return NX_ERROR_BUSY;
    port->cold_effects = 1U;
    NX_PLATFORM_REGISTER_WRITE(RCC->APB2ENR, RCC->APB2ENR | RCC_APB2ENR_ADC1EN);
    nx_arch_dsb();
    if ((RCC->APB2ENR & RCC_APB2ENR_ADC1EN) == 0U)
        return NX_ERROR_IO;
    if ((ADC1->CR2 & ADC_CR2_ADON) != 0U)
        return NX_ERROR_BUSY;
    for (size_t i = 0U; i < config->channel_count; ++i) {
        nx_result_t status = nx_stm32_pin_configure(GPIOA, config->channels[i],
                                                    3U, 0U, 0U, false);
        if (status != NX_SUCCESS)
            return status;
    }
    port->registers = ADC1;
    port->common = ADC;
    port->channels = config->channels;
    port->sample_times = config->sample_times;
    port->channel_count = config->channel_count;
    port->reference_mv = config->reference_mv;
    return NX_SUCCESS;
}

/** \brief Stop only acquired ADC effects; failed readback retains the owner. */
static inline nx_result_t nx_stm32_adc_cold_finish(nx_stm32_adc_state_t* port) {
    if (!nx_stm32_control_cold_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL)
        return NX_ERROR_INVALID;
    if (port->cold_effects == 0U)
        return port->initialized || port->active ? NX_ERROR_BUSY : NX_SUCCESS;
    if (port->active)
        return NX_ERROR_BUSY;
    if (port->initialized && (RCC->APB2ENR & RCC_APB2ENR_ADC1EN) == 0U)
        return NX_ERROR_IO;
    if ((port->cold_effects & 1U) != 0U &&
        (RCC->APB2ENR & RCC_APB2ENR_ADC1EN) != 0U) {
        NX_PLATFORM_REGISTER_WRITE(ADC1->CR2, 0U);
        nx_arch_dsb();
        if (ADC1->CR2 != 0U)
            return NX_ERROR_IO;
        port->initialized = false;
    }
    if ((port->cold_effects & 2U) != 0U) {
        NX_PLATFORM_REGISTER_WRITE(
            RCC->APB1ENR, RCC->APB1ENR & ~(uint32_t)RCC_APB1ENR_TIM3EN);
        nx_arch_dsb();
        if ((RCC->APB1ENR & RCC_APB1ENR_TIM3EN) != 0U)
            return NX_ERROR_IO;
        port->cold_effects &= (uint8_t)~2U;
    }
    if ((port->cold_effects & 1U) != 0U) {
        NX_PLATFORM_REGISTER_WRITE(
            RCC->APB2ENR, RCC->APB2ENR & ~(uint32_t)RCC_APB2ENR_ADC1EN);
        nx_arch_dsb();
        if ((RCC->APB2ENR & RCC_APB2ENR_ADC1EN) != 0U)
            return NX_ERROR_IO;
        port->cold_effects &= (uint8_t)~1U;
    }
    return NX_SUCCESS;
}

/** \brief Prepare a finite software-shot/low-rate scan without generated logic.
 */
static inline nx_result_t
nx_stm32_adc_prepare(nx_stm32_adc_state_t* port,
                     const nx_stm32_adc_config_t* config) {
    nx_result_t result = nx_stm32_adc_cold_begin(port, config);
    return result == NX_SUCCESS ? nx_stm32_adc_initialize(port) : result;
}

/** \brief Close software shots; submitters have joined before this call. */
static inline nx_result_t nx_stm32_adc_close(nx_stm32_adc_state_t* port) {
    return nx_stm32_adc_cold_finish(port);
}

/** \brief Bind exact stream resources; no block is borrowed during preparation.
 */
static inline nx_result_t
nx_stm32_adc_stream_cold_begin(nx_stm32_adc_stream_state_t* port,
                               const nx_stm32_adc_stream_config_t* config) {
    if (port == NULL || config == NULL || config->regions == NULL ||
        config->region_count == 0U || config->priority > 15U)
        return NX_ERROR_INVALID;
    nx_result_t result = nx_stm32_adc_cold_begin(&port->adc, &config->adc);
    if (result != NX_SUCCESS)
        return result;
    port->adc.cold_effects |= 2U;
    NX_PLATFORM_REGISTER_WRITE(RCC->APB1ENR, RCC->APB1ENR | RCC_APB1ENR_TIM3EN);
    nx_arch_dsb();
    if ((RCC->APB1ENR & RCC_APB1ENR_TIM3EN) == 0U)
        return NX_ERROR_IO;
    port->timer = TIM3;
    port->dma = DMA2;
    port->memory = DMA2_Stream4;
    port->regions = config->regions;
    port->region_count = config->region_count;
    port->timer_clock_hz = 84000000U;
    port->adc_clock_hz = 21000000U;
    return NX_SUCCESS;
}

/** \brief Publish the selected IRQ only after the actual stream initialized. */
static inline nx_result_t
nx_stm32_adc_stream_cold_publish(const nx_stm32_adc_stream_config_t* config) {
    NVIC_SetPriority(DMA2_Stream4_IRQn, config->priority);
    nx_arch_dsb();
    return NVIC_GetPriority(DMA2_Stream4_IRQn) == config->priority &&
                   NVIC_GetEnableIRQ(DMA2_Stream4_IRQn) != 0U
               ? NX_SUCCESS
               : NX_ERROR_IO;
}

/** \brief Prepare finite trigger-DMA; failed initialization still requires
 * close. */
static inline nx_result_t
nx_stm32_adc_stream_prepare(nx_stm32_adc_stream_state_t* port,
                            const nx_stm32_adc_stream_config_t* config) {
    nx_result_t result = nx_stm32_adc_stream_cold_begin(port, config);
    if (result == NX_SUCCESS)
        result = nx_stm32_adc_stream_initialize(port);
    return result == NX_SUCCESS ? nx_stm32_adc_stream_cold_publish(config)
                                : result;
}

/** \brief Prepare continuous double-buffer state without enlarging finite
 * state. */
static inline nx_result_t
nx_stm32_adc_continuous_prepare(nx_stm32_adc_continuous_state_t* port,
                                const nx_stm32_adc_stream_config_t* config) {
    if (port == NULL)
        return NX_ERROR_INVALID;
    nx_result_t result = nx_stm32_adc_stream_cold_begin(&port->base, config);
    if (result == NX_SUCCESS)
        result = nx_stm32_adc_continuous_initialize(port);
    return result == NX_SUCCESS ? nx_stm32_adc_stream_cold_publish(config)
                                : result;
}

/** \brief Withdraw the IRQ after the concrete engine releases every DMA loan.
 */
static inline nx_result_t
nx_stm32_adc_stream_cold_finish(nx_stm32_adc_stream_state_t* port) {
    if (!nx_stm32_control_cold_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL)
        return NX_ERROR_INVALID;
    if (port->adc.cold_effects != 0U) {
        NVIC_DisableIRQ(DMA2_Stream4_IRQn);
        NVIC_ClearPendingIRQ(DMA2_Stream4_IRQn);
        nx_arch_dsb();
        if (NVIC_GetEnableIRQ(DMA2_Stream4_IRQn) != 0U ||
            NVIC_GetPendingIRQ(DMA2_Stream4_IRQn) != 0U ||
            NVIC_GetActive(DMA2_Stream4_IRQn) != 0U)
            return NX_ERROR_BUSY;
    }
    return nx_stm32_adc_cold_finish(&port->adc);
}

/** \brief Drain finite stream ownership before shared clocks and pins. */
static inline nx_result_t
nx_stm32_adc_stream_close(nx_stm32_adc_stream_state_t* port) {
    if (!nx_stm32_control_cold_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL)
        return NX_ERROR_INVALID;
    if (port->adc.cold_effects == 0U)
        return port->adc.initialized || port->adc.active ? NX_ERROR_BUSY
                                                         : NX_SUCCESS;
    if (port->adc.initialized) {
        nx_result_t result = nx_stm32_adc_stream_ops.stream_stop(port);
        if (result != NX_SUCCESS)
            return result;
    }
    return nx_stm32_adc_stream_cold_finish(port);
}

/** \brief Drain continuous stream ownership before shared clocks and pins. */
static inline nx_result_t
nx_stm32_adc_continuous_close(nx_stm32_adc_continuous_state_t* port) {
    if (!nx_stm32_control_cold_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL)
        return NX_ERROR_INVALID;
    if (port->base.adc.cold_effects == 0U)
        return port->base.adc.initialized || port->base.adc.active
                   ? NX_ERROR_BUSY
                   : NX_SUCCESS;
    if (port->base.adc.initialized) {
        nx_result_t result = nx_stm32_adc_continuous_ops.stream_stop(port);
        if (result != NX_SUCCESS)
            return result;
    }
    return nx_stm32_adc_stream_cold_finish(&port->base);
}
#endif
