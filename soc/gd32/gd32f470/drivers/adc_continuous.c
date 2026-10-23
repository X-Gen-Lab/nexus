/**
 * \file            adc_continuous.c
 * \brief           ADC0 switch-buffer DMA with bounded posted block ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
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
#ifndef NX_GD32_ADC_CONTINUOUS_TARGET
#define NX_GD32_ADC_CONTINUOUS_TARGET(bank, address)                           \
    do {                                                                       \
        if ((bank) == 0U) {                                                    \
            DMA_CH0M0ADDR(DMA1) = (address);                                   \
        } else {                                                               \
            DMA_CH0M1ADDR(DMA1) = (address);                                   \
        }                                                                      \
    } while (0)
#endif

#define ADC_DMA_INTERRUPTS                                                     \
    (DMA_CHXCTL_FTFIE | DMA_CHXCTL_HTFIE | DMA_CHXCTL_TAEIE | DMA_CHXCTL_SDEIE)
#define ADC_DMA_ERRORS (DMA_INTF_FEEIF | DMA_INTF_SDEIF | DMA_INTF_TAEIF)
enum { PHASE_IDLE, PHASE_STABILITY, PHASE_CALIBRATION, PHASE_TRIGGER };

/** \brief Acknowledge only the selected stream and its stale vector entry. */
static void acknowledge(uint32_t flags) {
    NX_GD32_ADC_DMA_ACK(flags);
    NVIC_ClearPendingIRQ(DMA1_Channel0_IRQn);
}

/** \brief Read source and memory facts after bounded stop stores. */
static bool detach(nx_gd32_adc_stream_state_t* base) {
    TIMER_CTL0(TIMER2) &= ~(uint32_t)TIMER_CTL0_CEN;
    ADC_CTL1(ADC0) &= ~(uint32_t)(ADC_CTL1_ETMRC | ADC_CTL1_ADCON |
                                  ADC_CTL1_DMA | ADC_CTL1_DDM);
    nx_gd32_peripheral_barrier();
    (void)ADC_CTL1(ADC0);
    NX_GD32_ADC_DMA_DISABLE(DMA_CHXCTL_CHEN | ADC_DMA_INTERRUPTS);
    DMA_CH0FCTL(DMA1) &= ~(uint32_t)DMA_CHXFCTL_FEEIE;
    NVIC_DisableIRQ(DMA1_Channel0_IRQn);
    nx_gd32_peripheral_barrier();
    acknowledge(0x3DU);
    base->phase = PHASE_IDLE;
    return (DMA_CH0CTL(DMA1) & (DMA_CHXCTL_CHEN | ADC_DMA_INTERRUPTS)) == 0U &&
           (DMA_CH0FCTL(DMA1) & DMA_CHXFCTL_FEEIE) == 0U &&
           (ADC_CTL1(ADC0) & (ADC_CTL1_ETMRC | ADC_CTL1_ADCON | ADC_CTL1_DMA |
                              ADC_CTL1_DDM)) == 0U &&
           (TIMER_CTL0(TIMER2) & TIMER_CTL0_CEN) == 0U;
}

/** \brief Failed source drain never revokes either target or the spare loan. */
static nx_result_t rollback(nx_gd32_adc_continuous_state_t* state) {
    nx_gd32_adc_stream_state_t* base = &state->base;
    if (!detach(base)) {
        return NX_ERROR_BUSY;
    }
    if (base->filling) {
        nx_result_t result =
            nx_stream_post_abort(&state->posted, &base->fill, true);
        if (result != NX_SUCCESS) {
            return result;
        }
        base->filling = false;
    }
    if (state->posted.pending == 2U) {
        nx_result_t result = nx_stream_post_abort(
            &state->posted, &state->targets[state->expected_bank ^ 1U], true);
        if (result != NX_SUCCESS) {
            return result;
        }
    }
    if (state->posted.pending == 1U) {
        return nx_stream_post_abort(
            &state->posted, &state->targets[state->expected_bank], true);
    }
    return NX_SUCCESS;
}

/** \brief Two future blocks are acquired before switch-buffer mode can write.
 */
static nx_result_t arm(nx_gd32_adc_continuous_state_t* state) {
    nx_gd32_adc_stream_state_t* base = &state->base;
    if (!nx_stream_post_can_reserve(&state->posted)) {
        return base->stream->epoch == UINT64_MAX ? NX_ERROR_EXHAUSTED
                                                 : NX_ERROR_BUSY;
    }
    nx_result_t result =
        nx_stream_post_reserve(&state->posted, &state->targets[0]);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (!nx_stream_post_can_reserve(&state->posted)) {
        (void)nx_stream_post_abort(&state->posted, &state->targets[0], true);
        return base->stream->epoch == UINT64_MAX ? NX_ERROR_EXHAUSTED
                                                 : NX_ERROR_BUSY;
    }
    result = nx_stream_post_reserve(&state->posted, &state->targets[1]);
    if (result != NX_SUCCESS) {
        (void)nx_stream_post_abort(&state->posted, &state->targets[0], true);
        return result;
    }
    state->expected_bank = 0U;
    base->filling = false;
    acknowledge(0x3DU);
    nx_gd32_adc_stream_sequence(&base->adc, state->sample_times);
    TIMER_CTL0(TIMER2) = TIMER_CTL0_ARSE;
    TIMER_CTL1(TIMER2) = TIMER_TRI_OUT_SRC_UPDATE;
    TIMER_DMAINTEN(TIMER2) = 0U;
    TIMER_PSC(TIMER2) = base->prescaler;
    TIMER_CAR(TIMER2) = base->reload;
    TIMER_CNT(TIMER2) = 0U;
    TIMER_SWEVG(TIMER2) = TIMER_SWEVG_UPG;
    TIMER_INTF(TIMER2) = 0U;
    ADC_STAT(ADC0) = 0U;
    DMA_CH0CTL(DMA1) = DMA_CHXCTL_SBMEN | DMA_CHXCTL_CMEN | DMA_CHXCTL_MNAGA |
                       (1U << 11U) | (1U << 13U) | DMA_CHXCTL_FTFIE |
                       DMA_CHXCTL_TAEIE | DMA_CHXCTL_SDEIE;
    DMA_CH0CNT(DMA1) =
        (uint32_t)(state->targets[0].capacity / sizeof(uint16_t));
    DMA_CH0PADDR(DMA1) = (uint32_t)(ADC0 + 0x4CU);
    DMA_CH0M0ADDR(DMA1) = (uint32_t)(uintptr_t)state->targets[0].data;
    DMA_CH0M1ADDR(DMA1) = (uint32_t)(uintptr_t)state->targets[1].data;
    DMA_CH0FCTL(DMA1) = DMA_CHXFCTL_FEEIE;
    NVIC_EnableIRQ(DMA1_Channel0_IRQn);
    DMA_CH0CTL(DMA1) |= DMA_CHXCTL_CHEN;
    ADC_CTL1(ADC0) = ADC_CTL1_ADCON | ADC_EXTTRIG_ROUTINE_T2_TRGO;
    nx_gd32_peripheral_barrier();
    (void)ADC_CTL1(ADC0);
    base->ready_at = nx_deadline_after(nx_time_now_us(), 2U);
    base->phase = PHASE_STABILITY;
    state->halted = false;
    return NX_SUCCESS;
}

/** \brief Reject bad sample codes before acquiring any peripheral or loan. */
nx_result_t
nx_gd32_adc_continuous_initialize(nx_gd32_adc_continuous_state_t* state,
                                  const uint8_t* channels, size_t count,
                                  uint32_t reference_mv, nx_time_us_t deadline,
                                  unsigned priority) {
    if (state == NULL || state->sample_times == NULL || count == 0U ||
        count > 16U) {
        return NX_ERROR_INVALID;
    }
    for (size_t i = 0U; i < count; ++i) {
        if (state->sample_times[i] > 7U) {
            return NX_ERROR_INVALID;
        }
    }
    return nx_gd32_adc_stream_initialize(&state->base, channels, count,
                                         reference_mv, deadline, priority);
}

/** \brief Validate all equal complete-scan future destinations before loans. */
static nx_result_t start(void* context, nx_stream_t* stream, uint32_t hz) {
    nx_gd32_adc_continuous_state_t* state = context;
    if (state == NULL || stream == NULL || stream->slots == NULL ||
        stream->count < 3U) {
        return NX_ERROR_INVALID;
    }
    nx_gd32_adc_stream_state_t* base = &state->base;
    if (!base->adc.initialized) {
        return NX_ERROR_STATE;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    uint16_t prescaler, reload;
    nx_result_t result = nx_gd32_adc_stream_timing(
        base->adc.channel_count, state->sample_times, hz, &prescaler, &reload);
    if (result != NX_SUCCESS) {
        return result;
    }
    size_t scan = base->adc.channel_count * sizeof(uint16_t);
    for (size_t i = 0U; i < stream->count; ++i) {
        nx_stream_slot_t* slot = &stream->slots[i];
        if (slot->capacity == 0U ||
            slot->capacity != stream->slots[0].capacity ||
            slot->capacity % scan != 0U ||
            slot->capacity / sizeof(uint16_t) > 65535U) {
            return NX_ERROR_INVALID;
        }
        result = nx_dma_buffer_validate(base->regions, base->region_count,
                                        slot->data, slot->capacity, 2U,
                                        NX_DMA_FROM_DEVICE);
        if (result != NX_SUCCESS) {
            return result;
        }
    }
    uint32_t saved = nx_gd32_critical_enter();
    if (base->stream != NULL || base->adc.active ||
        (DMA_CH0CTL(DMA1) & DMA_CHXCTL_CHEN) != 0U ||
        (TIMER_CTL0(TIMER2) & TIMER_CTL0_CEN) != 0U) {
        nx_gd32_critical_leave(saved);
        return NX_ERROR_BUSY;
    }
    result = state->observer == NULL
                 ? NX_SUCCESS
                 : nx_adc_observer_bind(state->observer, stream, state,
                                        sizeof(*state));
    if (result == NX_SUCCESS) {
        result = nx_stream_post_initialize(&state->posted, stream);
        if (result != NX_SUCCESS) {
            (void)nx_adc_observer_unbind(state->observer);
        }
    }
    if (result == NX_SUCCESS) {
        base->stream = stream;
        base->prescaler = prescaler;
        base->reload = reload;
        base->stopping = false;
        base->fault = NX_SUCCESS;
        base->adc.active = true;
        result = arm(state);
        if (result != NX_SUCCESS) {
            base->stream = NULL;
            base->adc.active = false;
            (void)nx_adc_observer_unbind(state->observer);
        }
    }
    nx_gd32_critical_leave(saved);
    return result;
}

/** \brief Record one observed rejected boundary, never guess missed triggers.
 */
static void halt(nx_gd32_adc_continuous_state_t* state, nx_result_t fault,
                 bool observed_boundary) {
    state->base.fault = fault;
    state->halted = true;
    if (state->observer != NULL) {
        state->observer->discontinuity = true;
    }
    if (observed_boundary) {
        nx_stream_note_loss(state->base.stream, 1U);
    }
    (void)rollback(state);
}

/** \brief An inactive address readback, stable bank and clean flags prove loan.
 */
void nx_gd32_adc_continuous_irq(nx_gd32_adc_continuous_state_t* state) {
    if (state == NULL || state->base.stream == NULL || state->base.stopping ||
        state->halted) {
        return;
    }
    uint32_t flags = DMA_INTF0(DMA1) & 0x3DU;
    if ((flags & (ADC_DMA_ERRORS | DMA_INTF_FTFIF)) == 0U) {
        return;
    }
    unsigned active = (DMA_CH0CTL(DMA1) & DMA_CHXCTL_MBS) != 0U ? 1U : 0U;
    unsigned bank = active ^ 1U;
    if ((flags & ADC_DMA_ERRORS) != 0U || bank != state->expected_bank ||
        (ADC_STAT(ADC0) & ADC_STAT_ROVF) != 0U) {
        halt(state, NX_ERROR_IO, true);
    } else if (!nx_stream_post_can_reserve(&state->posted)) {
        halt(state,
             state->base.stream->epoch == UINT64_MAX ? NX_ERROR_EXHAUSTED
                                                     : NX_SUCCESS,
             true);
    } else {
        nx_result_t result =
            nx_stream_post_reserve(&state->posted, &state->base.fill);
        if (result != NX_SUCCESS) {
            halt(state, NX_ERROR_IO, true);
        } else {
            state->base.filling = true;
            acknowledge(flags);
            NX_GD32_ADC_CONTINUOUS_TARGET(
                bank, (uint32_t)(uintptr_t)state->base.fill.data);
            nx_gd32_peripheral_barrier();
            uint32_t old = (uint32_t)(uintptr_t)state->targets[bank].data;
            uint32_t replacement = (uint32_t)(uintptr_t)state->base.fill.data;
            uint32_t other = (uint32_t)(uintptr_t)state->targets[active].data;
            bool addresses = bank == 0U ? DMA_CH0M0ADDR(DMA1) == replacement &&
                                              DMA_CH0M1ADDR(DMA1) == other
                                        : DMA_CH0M1ADDR(DMA1) == replacement &&
                                              DMA_CH0M0ADDR(DMA1) == other;
            bool detached =
                addresses && DMA_CH0M0ADDR(DMA1) != old &&
                DMA_CH0M1ADDR(DMA1) != old &&
                ((DMA_CH0CTL(DMA1) & DMA_CHXCTL_MBS) != 0U ? 1U : 0U) ==
                    active &&
                (DMA_CH0CTL(DMA1) & DMA_CHXCTL_CHEN) != 0U &&
                (DMA_INTF0(DMA1) & (ADC_DMA_ERRORS | DMA_INTF_FTFIF)) == 0U &&
                (ADC_STAT(ADC0) & ADC_STAT_ROVF) == 0U;
            if (detached) {
                result = state->observer == NULL
                             ? NX_SUCCESS
                             : nx_adc_observer_record(state->observer,
                                                      &state->targets[bank],
                                                      nx_time_now_us());
                if (result == NX_SUCCESS) {
                    result = nx_stream_post_publish(
                        &state->posted, &state->targets[bank],
                        state->targets[bank].capacity,
                        NX_STREAM_BOUNDARY_TRIGGER);
                }
                if (result == NX_SUCCESS) {
                    state->targets[bank] = state->base.fill;
                    state->base.filling = false;
                    state->expected_bank ^= 1U;
                } else {
                    halt(state, result, true);
                }
            } else {
                halt(state, NX_ERROR_IO, true);
            }
        }
    }
    (void)nx_irq_wake_signal(state->base.wake);
}

/** \brief Calibration is bounded and resumes only through the task executor. */
static nx_result_t service(void* context) {
    nx_gd32_adc_continuous_state_t* state = context;
    if (state == NULL || state->base.stream == NULL || state->base.stopping) {
        return NX_ERROR_STATE;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_gd32_adc_stream_state_t* base = &state->base;
    uint32_t saved = nx_gd32_critical_enter();
    nx_result_t result = NX_SUCCESS;
    if (!state->halted && (ADC_STAT(ADC0) & ADC_STAT_ROVF) != 0U) {
        halt(state, NX_ERROR_IO, true);
    }
    if (state->halted) {
        result = rollback(state);
        if (result == NX_SUCCESS) {
            result = base->fault != NX_SUCCESS ? base->fault : arm(state);
            if (result == NX_SUCCESS) {
                result = NX_ERROR_BUSY;
            }
        }
    } else if (base->phase == PHASE_STABILITY) {
        nx_time_us_t now = nx_time_now_us();
        result = NX_ERROR_BUSY;
        if (nx_deadline_expired(base->ready_at, now)) {
            ADC_CTL1(ADC0) |= ADC_CTL1_CLB;
            base->ready_at = nx_deadline_after(now, 100U);
            base->phase = PHASE_CALIBRATION;
        }
    } else if (base->phase == PHASE_CALIBRATION) {
        nx_time_us_t now = nx_time_now_us();
        if ((ADC_CTL1(ADC0) & ADC_CTL1_CLB) == 0U) {
            ADC_CTL1(ADC0) |= ADC_CTL1_DMA | ADC_CTL1_DDM | (1U << 28U);
            TIMER_CTL0(TIMER2) |= TIMER_CTL0_CEN;
            nx_gd32_peripheral_barrier();
            base->phase = PHASE_TRIGGER;
        } else if (nx_deadline_expired(base->ready_at, now)) {
            halt(state, NX_ERROR_TIMEOUT, false);
            result =
                state->posted.pending == 0U ? NX_ERROR_TIMEOUT : NX_ERROR_BUSY;
        } else {
            result = NX_ERROR_BUSY;
        }
    }
    nx_gd32_critical_leave(saved);
    return result;
}

/** \brief Close admission and retain storage until every actor has joined. */
static nx_result_t stop(void* context) {
    nx_gd32_adc_continuous_state_t* state = context;
    if (state == NULL || !state->base.adc.initialized) {
        return NX_ERROR_STATE;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_gd32_adc_stream_state_t* base = &state->base;
    if (base->stream == NULL) {
        return NX_SUCCESS;
    }
    uint32_t saved = nx_gd32_critical_enter();
    base->stopping = true;
    nx_result_t result = rollback(state);
    nx_result_t joined = nx_stream_stop(base->stream);
    if (result == NX_SUCCESS) {
        result = joined;
    }
    if (result == NX_SUCCESS) {
        result = nx_adc_observer_unbind(state->observer);
    }
    if (result == NX_SUCCESS) {
        base->stream = NULL;
        base->adc.active = false;
        base->wake = NULL;
    }
    nx_gd32_critical_leave(saved);
    return result;
}

/** \brief Release unique peripheral clocks only after the complete join. */
nx_result_t nx_gd32_adc_continuous_stop(nx_gd32_adc_continuous_state_t* state) {
    nx_result_t result = stop(state);
    if (result == NX_SUCCESS) {
        result = nx_gd32_adc_stop(&state->base.adc);
        if (result == NX_SUCCESS) {
            rcu_periph_clock_disable(RCU_TIMER2);
        }
    }
    return result;
}

/** \brief Borrow only a hint target allowed by this actual vector policy. */
static nx_result_t attach_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t syscall_ceiling) {
    nx_gd32_adc_continuous_state_t* state = context;
    return state == NULL ? NX_ERROR_INVALID
                         : nx_gd32_adc_stream_ops.attach_wake(
                               &state->base, wake, syscall_ceiling);
}

/** \brief Cold facts describe cycles and channel order, never sample instants.
 */
static nx_result_t sequence_info(const void* context,
                                 nx_adc_sequence_info_t* info) {
    const nx_gd32_adc_continuous_state_t* state = context;
    if (state == NULL || info == NULL || !state->base.adc.initialized) {
        return NX_ERROR_INVALID;
    }
    const nx_gd32_adc_stream_state_t* base = &state->base;
    static const uint16_t cycles[] = {3U, 15U, 28U, 56U, 84U, 112U, 144U, 480U};
    uint32_t scan = 0U;
    for (size_t i = 0U; i < base->adc.channel_count; ++i) {
        scan += cycles[state->sample_times[i]] + 12U;
    }
    *info = (nx_adc_sequence_info_t){.channels = base->adc.channels,
                                     .sample_times = state->sample_times,
                                     .sample_cycles = cycles,
                                     .channel_count = base->adc.channel_count,
                                     .sample_code_count = 8U,
                                     .adc_clock_hz = 12500000U,
                                     .timer_clock_hz = 100000000U,
                                     .scan_cycles = scan,
                                     .conversion_cycles = 12U};
    return NX_SUCCESS;
}

/** \brief Attach only before any block or hardware producer becomes live. */
static nx_result_t attach_observer(void* context, nx_adc_observer_t* observer) {
    nx_gd32_adc_continuous_state_t* state = context;
    if (state == NULL || !state->base.adc.initialized) {
        return NX_ERROR_STATE;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    uint32_t saved = nx_gd32_critical_enter();
    nx_result_t result = NX_SUCCESS;
    if (state->base.stream != NULL || state->base.adc.active) {
        result = NX_ERROR_BUSY;
    } else if (observer != NULL &&
               (observer->records == NULL || observer->count == 0U ||
                observer->stream != NULL)) {
        result = NX_ERROR_INVALID;
    } else {
        state->observer = observer;
    }
    nx_gd32_critical_leave(saved);
    return result;
}

/** \brief Released/stale epochs never expose another loan's observation. */
static nx_result_t block_observation(const void* context,
                                     const nx_stream_block_t* block,
                                     nx_adc_observation_t* observation) {
    const nx_gd32_adc_continuous_state_t* state = context;
    if (state == NULL || !state->base.adc.initialized ||
        state->observer == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return nx_adc_observer_read(state->observer, block, observation);
}

/** \brief Read-only methods share one statically owned continuous state. */
const nx_adc_ops_t nx_gd32_adc_continuous_ops = {
    .info = nx_gd32_adc_info,
    .stream_start = start,
    .stream_stop = stop,
    .stream_service = service,
    .attach_wake = attach_wake,
    .sequence_info = sequence_info,
    .attach_observer = attach_observer,
    .block_observation = block_observation,
};
