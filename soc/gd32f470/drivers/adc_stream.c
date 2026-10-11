/**
 * \file            adc_stream.c
 * \brief           GD32 ADC0 complete-scan DMA loans with explicit timer gaps
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f470_adc_stream.h"
#include "gd32f4xx.h"
#include "gd32f4xx_dma.h"
#include "private/system.h"

#ifndef NX_GD32_ADC_DMA_DISABLE
#define NX_GD32_ADC_DMA_DISABLE(mask) (DMA_CH0CTL(DMA1) &= ~(uint32_t)(mask))
#endif
#ifndef NX_GD32_ADC_DMA_ACK
#define NX_GD32_ADC_DMA_ACK(flags) (DMA_INTC0(DMA1) = (uint32_t)(flags))
#endif

#define NX_GD32_ADC_DMA_INTERRUPTS                                             \
    (DMA_CHXCTL_FTFIE | DMA_CHXCTL_HTFIE | DMA_CHXCTL_TAEIE | DMA_CHXCTL_SDEIE)
#define NX_GD32_ADC_DMA_ERRORS                                                 \
    (DMA_INTF_FEEIF | DMA_INTF_SDEIF | DMA_INTF_TAEIF)

enum { PHASE_IDLE, PHASE_STABILITY, PHASE_CALIBRATION, PHASE_TRIGGER };

/** \brief Acknowledge exactly channel0, including stale vector entry. */
static void acknowledge(void) {
    NX_GD32_ADC_DMA_ACK(0x3DU);
    NVIC_ClearPendingIRQ(DMA1_Channel0_IRQn);
}

/** \brief Stop the trigger and converter before detaching memory ownership. */
static void detach(nx_gd32_adc_stream_state_t* state) {
    TIMER_CTL0(TIMER2) &= ~(uint32_t)TIMER_CTL0_CEN;
    ADC_CTL1(ADC0) &= ~(uint32_t)(ADC_CTL1_ETMRC | ADC_CTL1_ADCON |
                                  ADC_CTL1_DMA | ADC_CTL1_DDM);
    nx_gd32_peripheral_barrier();
    (void)ADC_CTL1(ADC0);
    NX_GD32_ADC_DMA_DISABLE(DMA_CHXCTL_CHEN | NX_GD32_ADC_DMA_INTERRUPTS);
    DMA_CH0FCTL(DMA1) &= ~(uint32_t)DMA_CHXFCTL_FEEIE;
    NVIC_DisableIRQ(DMA1_Channel0_IRQn);
    acknowledge();
    nx_gd32_peripheral_barrier();
    state->phase = PHASE_IDLE;
}

/** \brief Read engine and source facts instead of assuming a disable succeeded.
 */
static bool quiescent(void) {
    return (DMA_CH0CTL(DMA1) &
            (DMA_CHXCTL_CHEN | NX_GD32_ADC_DMA_INTERRUPTS)) == 0U &&
           (DMA_CH0FCTL(DMA1) & DMA_CHXFCTL_FEEIE) == 0U &&
           (ADC_CTL1(ADC0) & (ADC_CTL1_ETMRC | ADC_CTL1_ADCON | ADC_CTL1_DMA |
                              ADC_CTL1_DDM)) == 0U &&
           (TIMER_CTL0(TIMER2) & TIMER_CTL0_CEN) == 0U;
}

/** \brief Finish one retained producer loan only after the last possible write.
 */
static nx_result_t finish(nx_gd32_adc_stream_state_t* state) {
    if (!state->filling) {
        return NX_SUCCESS;
    }
    if (!quiescent()) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result;
    if (state->completed && !state->stopping && state->fault == NX_SUCCESS) {
        result =
            nx_stream_publish(state->stream, &state->fill, state->fill.capacity,
                              NX_STREAM_BOUNDARY_TRIGGER);
    } else {
        result = nx_stream_abort(state->stream, &state->fill, true);
        if (result == NX_SUCCESS && state->fault == NX_ERROR_IO) {
            nx_stream_note_loss(state->stream, 1U);
        }
    }
    if (result == NX_SUCCESS) {
        state->filling = false;
        state->completed = false;
    }
    return result;
}

/** \brief Bind an exact rate within 16-bit timer factors and full-scan cost. */
static nx_result_t timing(size_t channels, uint32_t hz, uint16_t* prescaler,
                          uint16_t* reload) {
    if (hz == 0U) {
        return NX_ERROR_INVALID;
    }
    if (hz > UINT32_C(12500000) / (27U * channels) ||
        UINT32_C(100000000) % hz != 0U) {
        return NX_ERROR_UNSUPPORTED;
    }
    uint32_t ticks = UINT32_C(100000000) / hz;
    uint32_t divider = (ticks + 65535U) / 65536U;
    for (; divider <= 65536U; ++divider) {
        if (ticks % divider == 0U && ticks / divider <= 65536U) {
            *prescaler = (uint16_t)(divider - 1U);
            *reload = (uint16_t)(ticks / divider - 1U);
            return NX_SUCCESS;
        }
    }
    return NX_ERROR_UNSUPPORTED;
}

/** \brief Validate every block, including later DMA destinations, before loan.
 */
static nx_result_t validate(nx_gd32_adc_stream_state_t* state,
                            nx_stream_t* stream, uint32_t hz,
                            uint16_t* prescaler, uint16_t* reload) {
    if (stream == NULL || stream->slots == NULL || stream->count == 0U ||
        stream->stopping) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result =
        timing(state->adc.channel_count, hz, prescaler, reload);
    if (result != NX_SUCCESS) {
        return result;
    }
    size_t scan_bytes = state->adc.channel_count * sizeof(uint16_t);
    for (size_t i = 0U; i < stream->count; ++i) {
        nx_stream_slot_t* slot = &stream->slots[i];
        if (slot->capacity == 0U || slot->capacity % scan_bytes != 0U ||
            slot->capacity / sizeof(uint16_t) > 65535U) {
            return NX_ERROR_INVALID;
        }
        if (slot->state != NX_STREAM_SLOT_FREE) {
            return NX_ERROR_BUSY;
        }
        result = nx_dma_buffer_validate(state->regions, state->region_count,
                                        slot->data, slot->capacity, 2U,
                                        NX_DMA_FROM_DEVICE);
        if (result != NX_SUCCESS) {
            return result;
        }
    }
    return NX_SUCCESS;
}

/** \brief Program the fixed scan and selected fifteen-cycle sample time. */
static void sequence(const nx_gd32_adc_state_t* adc) {
    ADC_CTL0(ADC0) = adc->channel_count > 1U ? ADC_CTL0_SM : 0U;
    ADC_SAMPT0(ADC0) = 0U;
    ADC_SAMPT1(ADC0) = 0U;
    ADC_RSQ0(ADC0) = (uint32_t)(adc->channel_count - 1U) << 20U;
    ADC_RSQ1(ADC0) = 0U;
    ADC_RSQ2(ADC0) = 0U;
    for (size_t i = 0U; i < adc->channel_count; ++i) {
        uint32_t channel = adc->channels[i];
        if (channel < 10U) {
            ADC_SAMPT1(ADC0) |= 1U << (channel * 3U);
        } else {
            ADC_SAMPT0(ADC0) |= 1U << ((channel - 10U) * 3U);
        }
        if (i < 6U) {
            ADC_RSQ2(ADC0) |= channel << (i * 5U);
        } else if (i < 12U) {
            ADC_RSQ1(ADC0) |= channel << ((i - 6U) * 5U);
        } else {
            ADC_RSQ0(ADC0) |= channel << ((i - 12U) * 5U);
        }
    }
}

/** \brief Reserve one free block without inventing loss during intentional gap.
 */
static nx_result_t arm(nx_gd32_adc_stream_state_t* state) {
    if (!nx_stream_can_reserve(state->stream)) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result = nx_stream_reserve(state->stream, &state->fill);
    if (result != NX_SUCCESS) {
        return result;
    }
    state->filling = true;
    state->completed = false;
    acknowledge();
    sequence(&state->adc);
    TIMER_CTL0(TIMER2) = TIMER_CTL0_ARSE;
    TIMER_CTL1(TIMER2) = TIMER_TRI_OUT_SRC_UPDATE;
    TIMER_DMAINTEN(TIMER2) = 0U;
    TIMER_PSC(TIMER2) = state->prescaler;
    TIMER_CAR(TIMER2) = state->reload;
    TIMER_CNT(TIMER2) = 0U;
    TIMER_SWEVG(TIMER2) = TIMER_SWEVG_UPG;
    TIMER_INTF(TIMER2) = 0U;
    ADC_STAT(ADC0) = 0U;
    DMA_CH0CTL(DMA1) = DMA_CHXCTL_MNAGA | (1U << 11U) | (1U << 13U) |
                       DMA_CHXCTL_FTFIE | DMA_CHXCTL_TAEIE | DMA_CHXCTL_SDEIE;
    DMA_CH0CNT(DMA1) = (uint32_t)(state->fill.capacity / sizeof(uint16_t));
    DMA_CH0PADDR(DMA1) = (uint32_t)(ADC0 + 0x4CU);
    DMA_CH0M0ADDR(DMA1) = (uint32_t)(uintptr_t)state->fill.data;
    DMA_CH0FCTL(DMA1) = 0U;
    NVIC_EnableIRQ(DMA1_Channel0_IRQn);
    DMA_CH0CTL(DMA1) |= DMA_CHXCTL_CHEN;
    ADC_CTL1(ADC0) = ADC_CTL1_ADCON | ADC_EXTTRIG_ROUTINE_T2_TRGO;
    nx_gd32_peripheral_barrier();
    (void)ADC_CTL1(ADC0);
    state->ready_at = nx_deadline_after(nx_time_now_us(), 2U);
    state->phase = PHASE_STABILITY;
    return NX_SUCCESS;
}

/** \brief Acquire unique timer ownership before touching ADC Board wiring. */
nx_result_t nx_gd32_adc_stream_initialize(nx_gd32_adc_stream_state_t* state,
                                          const uint8_t* channels, size_t count,
                                          uint32_t reference_mv,
                                          nx_time_us_t deadline,
                                          unsigned priority) {
    if (state == NULL || state->regions == NULL || state->region_count == 0U ||
        priority >= 16U) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if ((RCU_APB1EN & RCU_APB1EN_TIMER2EN) != 0U ||
        (DMA_CH0CTL(DMA1) & DMA_CHXCTL_CHEN) != 0U) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result = nx_gd32_adc_initialize(&state->adc, channels, count,
                                                reference_mv, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    rcu_periph_clock_enable(RCU_TIMER2);
    timer_deinit(TIMER2);
    ADC_CTL1(ADC0) = 0U;
    acknowledge();
    NVIC_DisableIRQ(DMA1_Channel0_IRQn);
    NVIC_SetPriority(DMA1_Channel0_IRQn, priority);
    state->stream = NULL;
    state->wake = NULL;
    state->phase = PHASE_IDLE;
    state->filling = false;
    state->completed = false;
    state->stopping = false;
    state->fault = NX_SUCCESS;
    return NX_SUCCESS;
}

/** \brief Admit validated blocks without starting an unstable ADC trigger. */
static nx_result_t start(void* context, nx_stream_t* stream, uint32_t hz) {
    nx_gd32_adc_stream_state_t* state = context;
    if (state == NULL || !state->adc.initialized) {
        return NX_ERROR_STATE;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->stream != NULL || state->adc.active ||
        (DMA_CH0CTL(DMA1) & DMA_CHXCTL_CHEN) != 0U) {
        return NX_ERROR_BUSY;
    }
    uint16_t prescaler, reload;
    nx_result_t result = validate(state, stream, hz, &prescaler, &reload);
    if (result != NX_SUCCESS) {
        return result;
    }
    uint32_t saved = nx_gd32_critical_enter();
    state->stream = stream;
    state->prescaler = prescaler;
    state->reload = reload;
    state->stopping = false;
    state->fault = NX_SUCCESS;
    state->adc.active = true;
    result = arm(state);
    if (result != NX_SUCCESS) {
        state->stream = NULL;
        state->adc.active = false;
    }
    nx_gd32_critical_leave(saved);
    return result;
}

/** \brief Drive bounded calibration and resume only through the task executor.
 */
static nx_result_t service(void* context) {
    nx_gd32_adc_stream_state_t* state = context;
    if (state == NULL || !state->adc.initialized || state->stream == NULL ||
        state->stopping) {
        return NX_ERROR_STATE;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    uint32_t saved = nx_gd32_critical_enter();
    nx_result_t result = NX_SUCCESS;
    if (state->completed || state->fault != NX_SUCCESS) {
        detach(state);
        result = finish(state);
        if (result == NX_SUCCESS && state->fault != NX_SUCCESS) {
            result = state->fault;
            state->fault = NX_SUCCESS;
        }
    }
    if (result == NX_SUCCESS && !state->filling) {
        result = arm(state);
    }
    if (result == NX_SUCCESS && state->phase == PHASE_STABILITY) {
        nx_time_us_t now = nx_time_now_us();
        if (!nx_deadline_expired(state->ready_at, now)) {
            result = NX_ERROR_BUSY;
        } else {
            ADC_CTL1(ADC0) |= ADC_CTL1_CLB;
            state->ready_at = nx_deadline_after(now, 100U);
            state->phase = PHASE_CALIBRATION;
            result = NX_ERROR_BUSY;
        }
    } else if (result == NX_SUCCESS && state->phase == PHASE_CALIBRATION) {
        nx_time_us_t now = nx_time_now_us();
        if ((ADC_CTL1(ADC0) & ADC_CTL1_CLB) == 0U) {
            ADC_CTL1(ADC0) |= ADC_CTL1_DMA | ADC_CTL1_DDM | (1U << 28U);
            TIMER_CTL0(TIMER2) |= TIMER_CTL0_CEN;
            nx_gd32_peripheral_barrier();
            state->phase = PHASE_TRIGGER;
        } else if (nx_deadline_expired(state->ready_at, now)) {
            state->fault = NX_ERROR_TIMEOUT;
            detach(state);
            result = finish(state);
            if (result == NX_SUCCESS) {
                result = state->fault;
                state->fault = NX_SUCCESS;
            }
        } else {
            result = NX_ERROR_BUSY;
        }
    }
    nx_gd32_critical_leave(saved);
    return result;
}

/** \brief Stop closes acquisition while retaining every live producer/consumer.
 */
static nx_result_t stop(void* context) {
    nx_gd32_adc_stream_state_t* state = context;
    if (state == NULL || !state->adc.initialized) {
        return NX_ERROR_STATE;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    uint32_t saved = nx_gd32_critical_enter();
    state->stopping = true;
    detach(state);
    nx_result_t result = quiescent() ? NX_SUCCESS : NX_ERROR_BUSY;
    if (state->stream != NULL) {
        if (result == NX_SUCCESS) {
            result = finish(state);
        }
        nx_result_t joined = nx_stream_stop(state->stream);
        if (result == NX_SUCCESS) {
            result = joined;
        }
        if (result == NX_SUCCESS) {
            state->stream = NULL;
            state->adc.active = false;
            state->fault = NX_SUCCESS;
            state->wake = NULL;
        }
    }
    nx_gd32_critical_leave(saved);
    return result;
}

/** \brief Release exclusive peripherals after producer and consumers join. */
nx_result_t nx_gd32_adc_stream_stop(nx_gd32_adc_stream_state_t* state) {
    nx_result_t result = stop(state);
    if (result != NX_SUCCESS) {
        return result;
    }
    result = nx_gd32_adc_stop(&state->adc);
    if (result == NX_SUCCESS) {
        rcu_periph_clock_disable(RCU_TIMER2);
        state->wake = NULL;
    }
    return result;
}

/** \brief Latch real DMA boundaries before quiescence, publication and hint. */
void nx_gd32_adc_stream_irq(nx_gd32_adc_stream_state_t* state) {
    uint32_t flags = DMA_INTF0(DMA1) & 0x3DU;
    acknowledge();
    if (state == NULL || !state->adc.initialized || state->stream == NULL ||
        !state->filling || state->stopping) {
        return;
    }
    if ((flags & NX_GD32_ADC_DMA_ERRORS) != 0U ||
        (ADC_STAT(ADC0) & ADC_STAT_ROVF) != 0U ||
        ((flags & DMA_INTF_FTFIF) != 0U && DMA_CH0CNT(DMA1) != 0U)) {
        state->fault = NX_ERROR_IO;
    } else if ((flags & DMA_INTF_FTFIF) != 0U) {
        state->completed = true;
    } else {
        return;
    }
    detach(state);
    (void)finish(state);
    (void)nx_irq_wake_signal(state->wake);
}

/** \brief Validate the actual DMA publisher before borrowing a wake sink. */
static nx_result_t attach_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t syscall_ceiling) {
    nx_gd32_adc_stream_state_t* state = context;
    if (state == NULL || !state->adc.initialized || state->stopping) {
        return NX_ERROR_STATE;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_result_t result =
        nx_gd32_irq_wake_validate(wake, DMA1_Channel0_IRQn, syscall_ceiling);
    if (result == NX_SUCCESS) {
        uint32_t saved = nx_gd32_critical_enter();
        state->wake = wake;
        nx_gd32_critical_leave(saved);
    }
    return result;
}

/** \brief Share methods in read-only memory; polling is a separate mode. */
const nx_adc_ops_t nx_gd32_adc_stream_ops = {
    .info = nx_gd32_adc_info,
    .stream_start = start,
    .stream_stop = stop,
    .stream_service = service,
    .attach_wake = attach_wake,
};
