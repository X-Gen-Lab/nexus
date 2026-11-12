/**
 * \file            adc_stream.c
 *
 * \brief           Discrete timer-triggered ADC scan blocks with proved DMA
 *                  detach
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-10
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f4_adc_trigger.h"
#include "stm32f4xx_ll_adc.h"
#include "stm32f4xx_ll_tim.h"

#ifndef NX_STM32_DMA_DISABLE
#define NX_STM32_DMA_DISABLE(stream, mask) ((stream)->CR &= ~(uint32_t)(mask))
#endif

/** \brief Acknowledge only the selected converter's fixed DMA2 lane. */
static void acknowledge(nx_stm32_adc_stream_state_t* state) {
    nx_stm32_adc_dma_ack(state, 0x3DU);
#ifndef NEXUS_STM32_MODEL
    NVIC_ClearPendingIRQ(nx_stm32_adc_dma_irq(state->adc.registers));
#endif
}

/** \brief Stop trigger/conversion before stopping their memory write source. */
static void detach(nx_stm32_adc_stream_state_t* state) {
    state->warming = false;
    LL_TIM_DisableCounter(state->timer);
    state->adc.registers->CR2 &=
        ~(uint32_t)(ADC_CR2_EXTEN | ADC_CR2_DMA | ADC_CR2_DDS | ADC_CR2_ADON);
    nx_arch_dsb();
    NX_STM32_DMA_DISABLE(state->memory, DMA_SxCR_EN | DMA_SxCR_TCIE |
                                            DMA_SxCR_HTIE | DMA_SxCR_TEIE |
                                            DMA_SxCR_DMEIE);
    nx_arch_dsb();
    acknowledge(state);
}

/** \brief Independent readbacks prove every trigger/conversion/memory source
 * stopped. */
static bool quiescent(const nx_stm32_adc_stream_state_t* state) {
    uint32_t sources = DMA_SxCR_EN | DMA_SxCR_TCIE | DMA_SxCR_HTIE |
                       DMA_SxCR_TEIE | DMA_SxCR_DMEIE;
    return (state->memory->CR & sources) == 0U &&
           (state->timer->CR1 & TIM_CR1_CEN) == 0U &&
           (state->adc.registers->CR2 &
            (ADC_CR2_EXTEN | ADC_CR2_DMA | ADC_CR2_DDS | ADC_CR2_ADON)) == 0U;
}

/** \brief Cold initialization rejects alternate routes before touching
 * hardware. */
nx_result_t nx_stm32_adc_stream_initialize(nx_stm32_adc_stream_state_t* state) {
    if (state == NULL || state->timer == NULL || state->dma == NULL ||
        state->memory == NULL || state->regions == NULL ||
        state->region_count == 0U || state->adc_clock_hz == 0U ||
        state->adc_clock_hz > 36000000U || state->timer_clock_hz == 0U ||
        state->timer_clock_hz > 168000000U) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    const nx_stm32_adc_trigger_fact_t* trigger =
        nx_stm32_adc_trigger_fact(state->trigger);
    if (trigger == NULL ||
        state->timer_clock_hz != nx_stm32_adc_trigger_clock(trigger)) {
        return NX_ERROR_UNSUPPORTED;
    }
#ifndef NEXUS_STM32_MODEL
    if ((state->adc.registers != ADC1 && state->adc.registers != ADC2 &&
         state->adc.registers != ADC3) ||
        state->timer != trigger->registers || state->dma != DMA2 ||
        state->memory != nx_stm32_adc_dma_memory(state->adc.registers)) {
        return NX_ERROR_UNSUPPORTED;
    }
    IRQn_Type irq = nx_stm32_adc_dma_irq(state->adc.registers);
    if (NVIC_GetEnableIRQ(irq) != 0U || NVIC_GetPendingIRQ(irq) != 0U ||
        NVIC_GetActive(irq) != 0U) {
        return NX_ERROR_BUSY;
    }
#endif
    if (state->stream != NULL || state->adc.initialized ||
        (state->memory->CR & DMA_SxCR_EN) != 0U ||
        (state->timer->CR1 & TIM_CR1_CEN) != 0U) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result = nx_stm32_adc_initialize(&state->adc);
    if (result != NX_SUCCESS) {
        return result;
    }
    state->filling = false;
    state->stopping = true;
    acknowledge(state);
#ifndef NEXUS_STM32_MODEL
    state->adc.cold_effects |= NX_STM32_ADC_IRQ;
    NVIC_SetPriority(irq, 5U);
    NVIC_EnableIRQ(irq);
#endif
    return NX_SUCCESS;
}

/** \brief Validate the whole ADC scan, not merely the timer divider. */
nx_result_t nx_stm32_adc_stream_rate(nx_stm32_adc_stream_state_t* state,
                                     uint32_t trigger_hz, uint16_t* prescaler,
                                     uint16_t* period) {
    static const uint16_t sample_cycles[] = {3U,  15U,  28U,  56U,
                                             84U, 112U, 144U, 480U};
    if (trigger_hz == 0U || state->timer_clock_hz == 0U ||
        state->timer_clock_hz > 168000000U || state->adc_clock_hz == 0U ||
        state->adc_clock_hz > 36000000U || state->adc.resolution > 3U) {
        return NX_ERROR_INVALID;
    }
    uint32_t cycles = 0U;
    for (size_t i = 0U; i < state->adc.channel_count; ++i) {
        cycles += sample_cycles[state->adc.sample_times[i]] +
                  (12U - 2U * state->adc.resolution);
    }
    if (cycles == 0U || trigger_hz > state->adc_clock_hz / cycles ||
        state->timer_clock_hz % trigger_hz != 0U) {
        return NX_ERROR_UNSUPPORTED;
    }
    uint32_t ticks = state->timer_clock_hz / trigger_hz;
    if (ticks == 0U) {
        return NX_ERROR_UNSUPPORTED;
    }
    uint32_t first = 1U + (ticks - 1U) / 65536U;
    for (uint32_t divider = first; divider <= 65536U; ++divider) {
        if (ticks % divider == 0U && ticks / divider <= 65536U) {
            *prescaler = (uint16_t)(divider - 1U);
            *period = (uint16_t)(ticks / divider - 1U);
            return NX_SUCCESS;
        }
    }
    return NX_ERROR_UNSUPPORTED;
}

/** \brief Treat invalid spans as aliases, including wrapped end addresses. */
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    uintptr_t ap = (uintptr_t)a, bp = (uintptr_t)b;
    return a == NULL || b == NULL || an == 0U || bn == 0U ||
           an > UINTPTR_MAX - ap || bn > UINTPTR_MAX - bp ||
           (ap < bp + bn && bp < ap + an);
}

/** \brief Preserve the concrete owner and facts through all future DMA loans.
 */
static bool independent(const nx_stm32_adc_stream_state_t* state,
                        size_t state_bytes, const void* data, size_t bytes) {
    return !overlap(data, bytes, state, state_bytes) &&
           !overlap(data, bytes, state->regions,
                    state->region_count * sizeof(*state->regions)) &&
           !overlap(data, bytes, state->adc.channels,
                    state->adc.channel_count * sizeof(*state->adc.channels)) &&
           !overlap(data, bytes, state->adc.sample_times,
                    state->adc.channel_count *
                        sizeof(*state->adc.sample_times)) &&
           (state->wake == NULL ||
            !overlap(data, bytes, state->wake, sizeof(*state->wake)));
}

/** \brief Validate all complete blocks and control spans before any loan. */
nx_result_t nx_stm32_adc_stream_blocks(nx_stm32_adc_stream_state_t* state,
                                       const nx_stream_t* stream,
                                       size_t state_bytes) {
    if (state_bytes < sizeof(*state) ||
        stream->count > SIZE_MAX / sizeof(*stream->slots) ||
        state->region_count > SIZE_MAX / sizeof(*state->regions) ||
        !independent(state, state_bytes, stream, sizeof(*stream)) ||
        !independent(state, state_bytes, stream->slots,
                     stream->count * sizeof(*stream->slots)))
        return NX_ERROR_INVALID;
    size_t scan_bytes = state->adc.channel_count * sizeof(uint16_t);
    for (size_t i = 0U; i < stream->count; ++i) {
        const nx_stream_slot_t* slot = &stream->slots[i];
        if (slot->capacity == 0U || slot->capacity % scan_bytes != 0U ||
            slot->capacity / sizeof(uint16_t) > 65535U ||
            !independent(state, state_bytes, slot->data, slot->capacity)) {
            return NX_ERROR_INVALID;
        }
        nx_result_t result = nx_dma_buffer_validate(
            state->regions, state->region_count, slot->data, slot->capacity,
            _Alignof(uint16_t), NX_DMA_FROM_DEVICE);
        if (result != NX_SUCCESS) {
            return result;
        }
    }
    return NX_SUCCESS;
}

/** \brief Preserve channel order and selected sample time for complete scans.
 */
void nx_stm32_adc_stream_sequence(nx_stm32_adc_stream_state_t* state) {
    ADC_TypeDef* adc = state->adc.registers;
    adc->SQR1 = ((uint32_t)state->adc.channel_count - 1U) << 20U;
    adc->SQR2 = 0U;
    adc->SQR3 = 0U;
    adc->SMPR1 = 0U;
    adc->SMPR2 = 0U;
    for (size_t i = 0U; i < state->adc.channel_count; ++i) {
        uint32_t channel = state->adc.channels[i];
        uint32_t sample = state->adc.sample_times[i];
        if (i < 6U) {
            adc->SQR3 |= channel << (uint32_t)(i * 5U);
        } else if (i < 12U) {
            adc->SQR2 |= channel << (uint32_t)((i - 6U) * 5U);
        } else {
            adc->SQR1 |= channel << (uint32_t)((i - 12U) * 5U);
        }
        if (channel < 10U) {
            adc->SMPR2 |= sample << (channel * 3U);
        } else {
            adc->SMPR1 |= sample << ((channel - 10U) * 3U);
        }
    }
}

/** \brief Arm one borrowed block only; no circular overwrite or hidden next
 * target. */
static void arm(nx_stm32_adc_stream_state_t* state) {
    ADC_TypeDef* adc = state->adc.registers;
    state->terminal = false;
    state->block_flags = NX_STREAM_BOUNDARY_TRIGGER;
    adc->CR2 = 0U;
    adc->CR1 = state->adc.channel_count > 1U ? (uint32_t)ADC_CR1_SCAN : 0U;
    LL_ADC_SetResolution(adc,
                         (uint32_t)state->adc.resolution << ADC_CR1_RES_Pos);
    nx_stm32_adc_stream_sequence(state);
    adc->SR = 0U;
    nx_stm32_adc_trigger_program(state->timer,
                                 nx_stm32_adc_trigger_fact(state->trigger),
                                 state->prescaler, state->period);
    acknowledge(state);
    state->memory->PAR = (uint32_t)(uintptr_t)&adc->DR;
    state->memory->M0AR = (uint32_t)(uintptr_t)state->fill.data;
    state->memory->NDTR = (uint32_t)(state->fill.capacity / sizeof(uint16_t));
    state->memory->FCR = 0U;
    state->memory->CR = nx_stm32_adc_dma_channel(adc) | DMA_SxCR_MINC |
                        DMA_SxCR_PSIZE_0 | DMA_SxCR_MSIZE_0 | DMA_SxCR_PL_1 |
                        DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE;
    nx_arch_dsb();
    state->memory->CR |= DMA_SxCR_EN;
    adc->CR2 = ADC_CR2_ADON | ADC_CR2_DMA | ADC_CR2_DDS |
               ((uint32_t)nx_stm32_adc_trigger_fact(state->trigger)->selection
                << ADC_CR2_EXTSEL_Pos);
    nx_arch_dsb();
    (void)adc->CR2;
    if (LL_ADC_IsEnabled(adc) == 0U ||
        nx_deadline_finite_after(nx_time_now_us(),
                                 nx_stm32_adc_startup_delay_us(&state->adc),
                                 &state->ready_at) != NX_SUCCESS) {
        /* The stream was accepted before hardware was armed. A failed
         * post-store proof keeps its loan until service/stop proves detach. */
        state->fault = true;
        state->terminal = true;
        detach(state);
        return;
    }
    state->warming = true;
}

/** \brief Start after rate, every domain and the exact first free slot
 * validate. */
static nx_result_t stream_start(void* context, nx_stream_t* stream,
                                uint32_t trigger_hz) {
    nx_stm32_adc_stream_state_t* state = context;
    if (state == NULL || stream == NULL || stream->slots == NULL ||
        stream->count == 0U) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->adc.initialized) {
        return NX_ERROR_STATE;
    }
    uint16_t prescaler;
    uint16_t period;
    nx_result_t result =
        nx_stm32_adc_stream_rate(state, trigger_hz, &prescaler, &period);
    if (result == NX_SUCCESS) {
        result = nx_stm32_adc_stream_blocks(state, stream, sizeof(*state));
    }
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_time_us_t ready_at;
    result = nx_deadline_finite_after(
        nx_time_now_us(), nx_stm32_adc_startup_delay_us(&state->adc),
        &ready_at);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (state->stream != NULL || state->adc.active ||
        (state->memory->CR & DMA_SxCR_EN) != 0U ||
        (state->timer->CR1 & TIM_CR1_CEN) != 0U) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    nx_stream_fill_t fill;
    result = nx_stream_reserve(stream, &fill);
    if (result != NX_SUCCESS) {
        nx_arch_irq_restore(saved);
        return result;
    }
    state->stream = stream;
    state->fill = fill;
    state->filling = true;
    state->adc.active = true;
    state->fault = false;
    state->stopping = false;
    state->trigger_hz = trigger_hz;
    state->prescaler = prescaler;
    state->period = period;
    arm(state);
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}

/** \brief Faulted or incomplete scans are discarded, never published as valid
 * counts. */
static bool finish(nx_stm32_adc_stream_state_t* state) {
    if (!state->terminal || !state->filling || !quiescent(state)) {
        return false;
    }
    nx_result_t result;
    if (state->fault || state->stopping) {
        if (state->fault) {
            nx_stream_note_loss(state->stream, 1U);
        }
        result = nx_stream_abort(state->stream, &state->fill, true);
    } else {
        result = nx_stream_publish(state->stream, &state->fill,
                                   state->fill.capacity, state->block_flags);
    }
    if (result == NX_SUCCESS) {
        state->filling = false;
        state->terminal = false;
        return true;
    }
    return false;
}

/** \brief DMA TC triggers a source stop, then publication only after actual
 * readback. */
void nx_stm32_adc_stream_irq(nx_stm32_adc_stream_state_t* state) {
    if (state == NULL || state->stream == NULL || !state->filling ||
        state->stopping) {
        return;
    }
    uint32_t status = nx_stm32_adc_dma_flags(state);
    if ((status & 0x2DU) == 0U) {
        return;
    }
    state->terminal = true;
    state->fault = state->fault || (status & 0x0DU) != 0U ||
                   state->memory->NDTR != 0U ||
                   (state->adc.registers->SR & ADC_SR_OVR) != 0U;
    detach(state);
    (void)finish(state);
    (void)nx_irq_wake_signal(state->wake);
}

/** \brief The application executor explicitly rearms a new free block after
 * each gap. */
static nx_result_t stream_service(void* context) {
    nx_stm32_adc_stream_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (state->stream == NULL || state->stopping) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_STATE;
    }
    if (state->filling && !state->terminal &&
        (state->adc.registers->SR & ADC_SR_OVR) != 0U) {
        state->fault = true;
        state->terminal = true;
        detach(state);
    }
    bool notify = finish(state);
    nx_result_t result = NX_SUCCESS;
    if (state->warming &&
        nx_deadline_expired(state->ready_at, nx_time_now_us())) {
        state->adc.registers->CR2 |= ADC_CR2_EXTEN_0;
        nx_arch_dsb();
        LL_TIM_EnableCounter(state->timer);
        state->warming = false;
    }
    if (state->fault) {
        result = NX_ERROR_IO;
    } else if (state->terminal || state->warming) {
        result = NX_ERROR_BUSY;
    } else if (!state->filling) {
        if (!nx_stream_can_reserve(state->stream)) {
            result = NX_ERROR_BUSY;
        } else {
            result = nx_stream_reserve(state->stream, &state->fill);
            if (result == NX_SUCCESS) {
                state->filling = true;
                arm(state);
                result = NX_ERROR_BUSY;
            }
        }
    }
    const nx_irq_wake_t* wake = notify ? state->wake : NULL;
    nx_arch_irq_restore(saved);
    (void)nx_irq_wake_signal(wake);
    return result;
}

/** \brief Stop retains every mutable DMA loan and every immutable consumer
 * loan. */
static nx_result_t stream_stop(void* context) {
    nx_stm32_adc_stream_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (state->stream == NULL) {
        nx_arch_irq_restore(saved);
        return NX_SUCCESS;
    }
    state->stopping = true;
    state->terminal = true;
    detach(state);
    if (!quiescent(state)) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    (void)finish(state);
    nx_result_t result = nx_stream_stop(state->stream);
    if (result == NX_SUCCESS) {
        state->stream = NULL;
        state->adc.active = false;
        state->wake = NULL;
    }
    nx_arch_irq_restore(saved);
    return result;
}

/** \brief Validate the actual DMA vector before publishing a borrowed hint
 * target. */
static nx_result_t attach_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t syscall_ceiling) {
    nx_stm32_adc_stream_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->adc.initialized) {
        return NX_ERROR_STATE;
    }
    nx_result_t result = nx_stm32_irq_wake_validate(
        wake, nx_stm32_adc_dma_irq(state->adc.registers), syscall_ceiling);
    if (result == NX_SUCCESS) {
        nx_arch_irq_state_t saved = nx_arch_irq_save();
        state->wake = wake;
        nx_arch_irq_restore(saved);
    }
    return result;
}

/** \brief Polling facts are shared; block ownership keeps competing samples
 * BUSY. */
const nx_adc_ops_t nx_stm32_adc_stream_ops = {
    .info = nx_stm32_adc_info,
    .sample = nx_stm32_adc_sample,
    .stream_start = stream_start,
    .stream_stop = stream_stop,
    .stream_service = stream_service,
    .attach_wake = attach_wake,
};
