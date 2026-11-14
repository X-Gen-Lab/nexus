/**
 * \file            cold.h
 * \brief           Static ADC and stream preparation with retained effects
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_DRIVERS_ADC_COLD_H
#define NEXUS_STM32F4_DRIVERS_ADC_COLD_H

/* Selected configuration must precede the pad-dependent helpers. */
#include "nexus/arch/arch.h"
#include "system/stm32f407_clock.h"

#include "drivers/adc/instance.h"
#include "drivers/adc/stm32f4_adc_pins.h"
#include "drivers/adc/stm32f4_adc_trigger.h"
#include "drivers/adc/stream.h"
#include "private/cold_context.h"

#ifdef __cplusplus
extern "C" {
#endif

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

/** \brief Cold identity selects one gate; no register address is inferred. */
static inline uint32_t nx_stm32_adc_clock(const ADC_TypeDef* registers) {
    return registers == ADC1   ? RCC_APB2ENR_ADC1EN
           : registers == ADC2 ? RCC_APB2ENR_ADC2EN
           : registers == ADC3 ? RCC_APB2ENR_ADC3EN
                               : 0U;
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
    /* Reject an unselected external connection before acquiring any clock.
     * Internal channels have already been admitted by sequence validation. */
    for (size_t i = 0U; i < config->channel_count; ++i) {
        nx_stm32_adc_pad_t pad;
        if (config->channels[i] <= 15U &&
            !nx_stm32_adc_pad(config->registers, config->channels[i], &pad))
            return NX_ERROR_INVALID;
    }
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
    port->adc_clock_hz = NEXUS_ADC_HZ;
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

#ifdef __cplusplus
}
#endif
#endif
