/**
 * \file            stm32f407_control_cold.h
 * \brief           Typed cold EXTI, PWM and ADC construction and withdrawal
 * \author          Nexus Team
 */
#ifndef NX_STM32F407_CONTROL_COLD_H
#define NX_STM32F407_CONTROL_COLD_H
#include "stm32f407_stream.h"
#include "stm32f4_adc_trigger.h"

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
    uint8_t pull;
} nx_stm32_exti_config_t;

/** \brief Exact general timer channel, counts and declared pad idle level. */
typedef struct {
    uint32_t period_ticks;
    uint32_t duty_ticks;
    uint32_t tick_hz;
    uint16_t prescaler;
    bool inactive_high;
    TIM_TypeDef* registers;
    GPIO_TypeDef* gpio;
    uint8_t channel;
    uint8_t pin;
} nx_stm32_pwm_config_t;

/** \brief Reviewed outputs; package and Board authority remain separate. */
static inline bool
nx_stm32_pwm_route_valid(const nx_stm32_pwm_config_t* config) {
    if (config->channel < 1U || config->channel > 4U)
        return false;
    if (config->registers == TIM3) {
        return config->channel <= 2U ? config->gpio == GPIOA &&
                                           config->pin == config->channel + 5U
                                     : config->gpio == GPIOB &&
                                           config->pin == config->channel - 3U;
    }
    if (config->registers == TIM4)
        return config->gpio == GPIOB && config->pin == config->channel + 5U;
    if (config->registers == TIM5)
        return config->gpio == GPIOA && config->pin == config->channel - 1U;
    if (config->registers == TIM9)
        return config->channel <= 2U && config->gpio == GPIOA &&
               config->pin == config->channel + 1U;
    if (config->registers == TIM12)
        return config->channel <= 2U && config->gpio == GPIOB &&
               config->pin == config->channel + 13U;
    if (config->channel != 1U)
        return false;
    return (config->registers == TIM10 && config->gpio == GPIOB &&
            config->pin == 8U) ||
           (config->registers == TIM11 && config->gpio == GPIOB &&
            config->pin == 9U) ||
           (config->registers == TIM13 && config->gpio == GPIOA &&
            config->pin == 6U) ||
           (config->registers == TIM14 && config->gpio == GPIOA &&
            config->pin == 7U);
}

/** \brief Immutable independent ADC1/2/3 ranks and nominal reference.
 * \note ADC3 uses its distinct pad map. Board facts authorize bonded pins.
 *       One channel has one sample time even when repeated in the sequence.
 */
typedef struct {
    const uint8_t* channels;
    const uint8_t* sample_times;
    size_t channel_count;
    uint32_t reference_mv;
    uint8_t
        resolution; /**< ADC_CR1 RES encoding, including reset value zero. */
    ADC_TypeDef* registers;
} nx_stm32_adc_config_t;

/** \brief Exact converter/TIM3/DMA2 selection, without mode dispatch. */
typedef struct {
    nx_stm32_adc_config_t adc;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
    uint8_t priority;
    uint8_t trigger;
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
        config->line > 15U || config->pull > 2U ||
        config->gpio !=
            (GPIO_TypeDef*)(uintptr_t)(GPIOA_BASE +
                                       (uintptr_t)config->gpio_index *
                                           UINT32_C(0x400)) ||
        config->priority > 15U || config->edge < NX_EXTI_RISING ||
        config->edge > NX_EXTI_BOTH)
        return NX_ERROR_INVALID;
    if (port->initialized || port->cold_owned)
        return NX_ERROR_BUSY;
    uint32_t bit = 1U << config->line;
    if (((EXTI->IMR | EXTI->EMR | EXTI->PR) & bit) != 0U)
        return NX_ERROR_BUSY;
    IRQn_Type irq = config->line < 5U
                        ? (IRQn_Type)((int)EXTI0_IRQn + (int)config->line)
                        : (config->line < 10U ? EXTI9_5_IRQn : EXTI15_10_IRQn);
    uint32_t vector_mask =
        config->line < 5U ? bit : (config->line < 10U ? 0x3E0U : 0xFC00U);
    if (NVIC_GetActive(irq) != 0U ||
        ((EXTI->IMR & vector_mask) != 0U && NVIC_GetEnableIRQ(irq) != 0U &&
         NVIC_GetPriority(irq) != config->priority))
        return NX_ERROR_BUSY;
    if ((RCC->APB2ENR & RCC_APB2ENR_SYSCFGEN) == 0U)
        return NX_ERROR_STATE;
    port->cold_owned = true;
    nx_result_t status = nx_stm32_pin_configure(config->gpio, config->line, 0U,
                                                0U, config->pull, false);
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
        !nx_stm32_pwm_route_valid(config) || config->period_ticks == 0U ||
        (config->registers != TIM5 && config->period_ticks > 65535U) ||
        config->duty_ticks > config->period_ticks || config->tick_hz == 0U)
        return NX_ERROR_INVALID;
    const bool apb2 = nx_stm32_timer_apb2(config->registers);
    const uint32_t source_hz = apb2 ? UINT32_C(168000000) : UINT32_C(84000000);
    if (source_hz / config->tick_hz != (uint32_t)config->prescaler + 1U ||
        source_hz % config->tick_hz != 0U)
        return NX_ERROR_INVALID;
    volatile uint32_t* clock = apb2 ? &RCC->APB2ENR : &RCC->APB1ENR;
    const uint32_t gate = nx_stm32_timer_gate(config->registers);
    if (port->initialized || inactive->initialized ||
        port->cold_effects != 0U || (*clock & gate) != 0U)
        return NX_ERROR_BUSY;
    /* Retain exact gate identity before the first fallible acquisition. */
    port->registers = config->registers;
    port->cold_effects = 1U;
    NX_PLATFORM_REGISTER_WRITE(*clock, *clock | gate);
    nx_arch_dsb();
    if ((*clock & gate) == 0U)
        return NX_ERROR_IO;
    TIM_TypeDef* timer = config->registers;
    const uint32_t channel = 1U << ((config->channel - 1U) * 4U);
    if ((timer->CCER & channel) != 0U ||
        ((timer->CR1 & TIM_CR1_CEN) != 0U &&
         (timer->PSC != config->prescaler ||
          timer->ARR != config->period_ticks - 1U)))
        return NX_ERROR_BUSY;
    inactive->registers = config->gpio;
    inactive->mask = 1U << config->pin;
    inactive->output = true;
    nx_result_t result = nx_stm32_gpio_initialize(
        inactive, config->inactive_high ? inactive->mask : 0U);
    if (result != NX_SUCCESS)
        return result;
    port->inactive_gpio = inactive;
    port->inactive_mask = inactive->mask;
    port->inactive_high = config->inactive_high;
    port->channel = config->channel;
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
    const uint32_t gate = nx_stm32_timer_gate(port->registers);
    if (gate == 0U)
        return NX_ERROR_STATE;
    volatile uint32_t* clock =
        nx_stm32_timer_apb2(port->registers) ? &RCC->APB2ENR : &RCC->APB1ENR;
    if (port->initialized && (*clock & gate) == 0U)
        return NX_ERROR_IO;
    if (port->initialized) {
        nx_result_t result = nx_stm32_pwm_ops.stop(port);
        if (result != NX_SUCCESS)
            return result;
        port->initialized = false;
        inactive->initialized = false;
    }
    if ((port->cold_effects & 1U) != 0U) {
        if ((*clock & gate) != 0U && (port->registers->CCER & 0x1111U) != 0U)
            return NX_ERROR_BUSY;
        NX_PLATFORM_REGISTER_WRITE(*clock, *clock & ~gate);
        nx_arch_dsb();
        if ((*clock & gate) != 0U)
            return NX_ERROR_IO;
        port->cold_effects &= (uint8_t)~1U;
    }
    inactive->initialized = false;
    return NX_SUCCESS;
}

/** \brief Cold identity selects one gate; no register address is inferred. */
static inline uint32_t nx_stm32_adc_clock(const ADC_TypeDef* registers) {
    return registers == ADC1   ? RCC_APB2ENR_ADC1EN
           : registers == ADC2 ? RCC_APB2ENR_ADC2EN
           : registers == ADC3 ? RCC_APB2ENR_ADC3EN
                               : 0U;
}

/** \brief Configure one reviewed external channel; sequence order is separate.
 */
static inline nx_result_t nx_stm32_adc_pin(ADC_TypeDef* registers,
                                           uint8_t channel) {
    if (registers == ADC1 && channel >= 16U && channel <= 18U)
        return NX_SUCCESS;
    GPIO_TypeDef* gpio;
    uint8_t pin;
    if (registers == ADC3) {
        static const uint8_t pads[16] = {
            0x00U, 0x01U, 0x02U, 0x03U, 0x56U, 0x57U, 0x58U, 0x59U,
            0x5AU, 0x53U, 0x20U, 0x21U, 0x22U, 0x23U, 0x54U, 0x55U};
        uint8_t pad = pads[channel];
        gpio = pad < 0x20U ? GPIOA : pad < 0x50U ? GPIOC : GPIOF;
        pin = pad & 15U;
    } else {
        gpio = channel < 8U ? GPIOA : channel < 10U ? GPIOB : GPIOC;
        pin = channel < 8U    ? channel
              : channel < 10U ? (uint8_t)(channel - 8U)
                              : (uint8_t)(channel - 10U);
    }
    return nx_stm32_pin_configure(gpio, pin, 3U, 0U, 0U, false);
}

/** \brief Acquire one converter; clock and controller receipts are distinct. */
static inline nx_result_t
nx_stm32_adc_cold_begin(nx_stm32_adc_state_t* port,
                        const nx_stm32_adc_config_t* config) {
    if (!nx_stm32_control_cold_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL || config == NULL || config->channels == NULL ||
        config->sample_times == NULL || config->channel_count == 0U ||
        config->channel_count > 16U || config->reference_mv == 0U ||
        config->reference_mv > 3600U ||
        nx_stm32_adc_clock(config->registers) == 0U)
        return NX_ERROR_INVALID;
    if (config->resolution > 3U)
        return NX_ERROR_UNSUPPORTED;
    nx_result_t sequence = nx_stm32_adc_sequence_validate(
        config->registers, config->channels, config->sample_times,
        config->channel_count);
    if (sequence != NX_SUCCESS)
        return sequence;
    uint32_t clock = nx_stm32_adc_clock(config->registers);
    if (port->initialized || port->active || port->cold_effects != 0U ||
        (RCC->APB2ENR & clock) != 0U)
        return NX_ERROR_BUSY;
    port->registers = config->registers;
    port->common = ADC;
    port->channels = config->channels;
    port->sample_times = config->sample_times;
    port->channel_count = config->channel_count;
    port->reference_mv = config->reference_mv;
    port->resolution = config->resolution;
    port->cold_effects = NX_STM32_ADC_CLOCK;
    NX_PLATFORM_REGISTER_WRITE(RCC->APB2ENR, RCC->APB2ENR | clock);
    nx_arch_dsb();
    if ((RCC->APB2ENR & clock) == 0U)
        return NX_ERROR_IO;
    if ((port->registers->CR2 & ADC_CR2_ADON) != 0U)
        return NX_ERROR_BUSY;
    if ((port->common->CCR & nx_stm32_adc_internal_mask(port)) != 0U)
        return NX_ERROR_BUSY;
    nx_result_t common = nx_stm32_adc_common_prepare(port);
    if (common != NX_SUCCESS)
        return common;
    port->cold_effects |= NX_STM32_ADC_CONVERTER;
    for (size_t i = 0U; i < config->channel_count; ++i) {
        nx_result_t status =
            nx_stm32_adc_pin(config->registers, config->channels[i]);
        if (status != NX_SUCCESS)
            return status;
    }
    return nx_stm32_adc_internal_prepare(port);
}

/** \brief Stop only acquired ADC effects; failed readback retains the owner. */
static inline nx_result_t nx_stm32_adc_cold_finish(nx_stm32_adc_state_t* port) {
    if (!nx_stm32_control_cold_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL)
        return NX_ERROR_INVALID;
    if (port->cold_effects == 0U)
        return port->initialized || port->active ? NX_ERROR_BUSY : NX_SUCCESS;
    uint32_t clock = nx_stm32_adc_clock(port->registers);
    if (clock == 0U)
        return NX_ERROR_INVALID;
    if ((port->cold_effects & NX_STM32_ADC_CONVERTER) != 0U) {
        if ((RCC->APB2ENR & clock) == 0U)
            return NX_ERROR_IO;
        nx_result_t result = nx_stm32_adc_quiesce(port);
        if (result != NX_SUCCESS)
            return result;
        port->initialized = false;
        port->cold_effects &= (uint8_t)~NX_STM32_ADC_CONVERTER;
    }
    if ((port->cold_effects & NX_STM32_ADC_INTERNAL) != 0U &&
        (RCC->APB2ENR & clock) == 0U)
        return NX_ERROR_IO;
    nx_result_t internal = nx_stm32_adc_internal_close(port);
    if (internal != NX_SUCCESS)
        return internal;
    nx_result_t probes = nx_stm32_adc_probe_close(port);
    if (probes != NX_SUCCESS)
        return probes;
    if ((port->cold_effects & NX_STM32_ADC_CLOCK) != 0U) {
        NX_PLATFORM_REGISTER_WRITE(RCC->APB2ENR, RCC->APB2ENR & ~clock);
        nx_arch_dsb();
        if ((RCC->APB2ENR & clock) != 0U)
            return NX_ERROR_IO;
        port->cold_effects &= (uint8_t)~NX_STM32_ADC_CLOCK;
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
    if (port != NULL && (port->cold_effects & NX_STM32_ADC_TIMER) != 0U)
        return NX_ERROR_STATE;
    return nx_stm32_adc_cold_finish(port);
}

/** \brief Admit one to four ranks before acquiring the common ADC owner. */
static inline nx_result_t
nx_stm32_adc_injected_prepare(nx_stm32_adc_state_t* port,
                              const nx_stm32_adc_config_t* config) {
    if (config == NULL || config->channel_count == 0U ||
        config->channel_count > 4U)
        return NX_ERROR_INVALID;
    return nx_stm32_adc_prepare(port, config);
}

/** \brief The same converter and analog-path owner performs final release. */
static inline nx_result_t
nx_stm32_adc_injected_close(nx_stm32_adc_state_t* port) {
    return nx_stm32_adc_close(port);
}

/** \brief Bind exact stream resources; no block is borrowed during preparation.
 */
static inline nx_result_t
nx_stm32_adc_stream_cold_begin(nx_stm32_adc_stream_state_t* port,
                               const nx_stm32_adc_stream_config_t* config) {
    if (!nx_stm32_control_cold_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL || config == NULL || config->regions == NULL ||
        config->region_count == 0U || config->priority > 15U)
        return NX_ERROR_INVALID;
    if (nx_stm32_adc_clock(config->adc.registers) == 0U)
        return NX_ERROR_UNSUPPORTED;
    const nx_stm32_adc_trigger_fact_t* trigger =
        nx_stm32_adc_trigger_fact(config->trigger);
    if (trigger == NULL)
        return NX_ERROR_UNSUPPORTED;
    volatile uint32_t* timer_clock =
        trigger->apb2 ? &RCC->APB2ENR : &RCC->APB1ENR;
    if ((*timer_clock & trigger->gate) != 0U)
        return NX_ERROR_BUSY;
    nx_result_t result = nx_stm32_adc_cold_begin(&port->adc, &config->adc);
    if (result != NX_SUCCESS)
        return result;
    port->timer = trigger->registers;
    port->trigger = config->trigger;
    port->adc.cold_effects |= NX_STM32_ADC_TIMER;
    NX_PLATFORM_REGISTER_WRITE(*timer_clock, *timer_clock | trigger->gate);
    nx_arch_dsb();
    if ((*timer_clock & trigger->gate) == 0U)
        return NX_ERROR_IO;
    port->dma = DMA2;
    port->memory = nx_stm32_adc_dma_memory(config->adc.registers);
    port->regions = config->regions;
    port->region_count = config->region_count;
    port->timer_clock_hz = nx_stm32_adc_trigger_clock(trigger);
    port->adc_clock_hz = 21000000U;
    return NX_SUCCESS;
}

/** \brief Publish the selected IRQ only after the actual stream initialized. */
static inline nx_result_t
nx_stm32_adc_stream_cold_publish(const nx_stm32_adc_stream_config_t* config) {
    IRQn_Type irq = nx_stm32_adc_dma_irq(config->adc.registers);
    NVIC_SetPriority(irq, config->priority);
    nx_arch_dsb();
    return NVIC_GetPriority(irq) == config->priority &&
                   NVIC_GetEnableIRQ(irq) != 0U
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
    if ((port->adc.cold_effects & NX_STM32_ADC_IRQ) != 0U) {
        IRQn_Type irq = nx_stm32_adc_dma_irq(port->adc.registers);
        NVIC_DisableIRQ(irq);
        NVIC_ClearPendingIRQ(irq);
        nx_arch_dsb();
        if (NVIC_GetEnableIRQ(irq) != 0U || NVIC_GetPendingIRQ(irq) != 0U ||
            NVIC_GetActive(irq) != 0U)
            return NX_ERROR_BUSY;
        port->adc.cold_effects &= (uint8_t)~NX_STM32_ADC_IRQ;
    }
    nx_result_t result = nx_stm32_adc_cold_finish(&port->adc);
    if (result != NX_SUCCESS)
        return result;
    if ((port->adc.cold_effects & NX_STM32_ADC_TIMER) != 0U) {
        const nx_stm32_adc_trigger_fact_t* trigger =
            nx_stm32_adc_trigger_fact(port->trigger);
        if (trigger == NULL || port->timer != trigger->registers)
            return NX_ERROR_STATE;
        if ((port->timer->CR1 & TIM_CR1_CEN) != 0U || port->timer->DIER != 0U)
            return NX_ERROR_BUSY;
        volatile uint32_t* clock =
            trigger->apb2 ? &RCC->APB2ENR : &RCC->APB1ENR;
        NX_PLATFORM_REGISTER_WRITE(*clock, *clock & ~trigger->gate);
        nx_arch_dsb();
        if ((*clock & trigger->gate) != 0U)
            return NX_ERROR_IO;
        port->adc.cold_effects &= (uint8_t)~NX_STM32_ADC_TIMER;
    }
    return NX_SUCCESS;
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
