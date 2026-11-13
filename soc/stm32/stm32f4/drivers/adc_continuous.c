/**
 * \file            adc_continuous.c
 * \brief           Continuous ADC double-buffer DMA with proved target loans
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f4_adc_trigger.h"
#include "stm32f4xx_ll_adc.h"
#include "stm32f4xx_ll_tim.h"

#ifndef NX_STM32_DMA_DISABLE
#define NX_STM32_DMA_DISABLE(stream, mask) ((stream)->CR &= ~(uint32_t)(mask))
#endif
#ifndef NX_STM32_ADC_CONTINUOUS_ACK
#define NX_STM32_ADC_CONTINUOUS_ACK(base, flags)                               \
    nx_stm32_adc_dma_ack((base), (flags))
#endif
#ifndef NX_STM32_ADC_CONTINUOUS_TARGET
#define NX_STM32_ADC_CONTINUOUS_TARGET(memory, bank, address)                  \
    do {                                                                       \
        if ((bank) == 0U) {                                                    \
            (memory)->M0AR = (address);                                        \
        } else {                                                               \
            (memory)->M1AR = (address);                                        \
        }                                                                      \
    } while (0)
#endif

/** \brief Clear exactly the selected lane, never another DMA owner. */
static void acknowledge(nx_stm32_adc_stream_state_t* base, uint32_t flags) {
    NX_STM32_ADC_CONTINUOUS_ACK(base, flags);
#ifndef NEXUS_STM32_MODEL
    NVIC_ClearPendingIRQ(nx_stm32_adc_dma_irq(base->adc.registers));
#endif
}

/** \brief Stop source before memory and independently read back both. */
static bool detach(nx_stm32_adc_stream_state_t* base) {
    base->warming = false;
    LL_TIM_DisableCounter(base->timer);
    base->adc.registers->CR2 &=
        ~(uint32_t)(ADC_CR2_EXTEN | ADC_CR2_DMA | ADC_CR2_DDS | ADC_CR2_ADON);
    nx_arch_dsb();
    NX_STM32_DMA_DISABLE(base->memory, DMA_SxCR_EN | DMA_SxCR_TCIE |
                                           DMA_SxCR_HTIE | DMA_SxCR_TEIE |
                                           DMA_SxCR_DMEIE);
    base->memory->FCR &= ~(uint32_t)DMA_SxFCR_FEIE;
    nx_arch_dsb();
    acknowledge(base, 0x3DU);
    return (base->memory->CR & (DMA_SxCR_EN | DMA_SxCR_TCIE | DMA_SxCR_HTIE |
                                DMA_SxCR_TEIE | DMA_SxCR_DMEIE)) == 0U &&
           (base->memory->FCR & DMA_SxFCR_FEIE) == 0U &&
           (base->timer->CR1 & TIM_CR1_CEN) == 0U &&
           (base->adc.registers->CR2 &
            (ADC_CR2_EXTEN | ADC_CR2_DMA | ADC_CR2_DDS | ADC_CR2_ADON)) == 0U;
}

/** \brief Tail-first rollback after readback; failed drain retains all loans.
 */
static nx_result_t rollback(nx_stm32_adc_continuous_state_t* state) {
    nx_stm32_adc_stream_state_t* base = &state->base;
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

/** \brief Halt without publishing an uncertain last block or guessing loss. */
static void halt(nx_stm32_adc_continuous_state_t* state, nx_result_t fault) {
    state->base.fault = fault != NX_SUCCESS;
    state->fault_result = fault;
    state->halted = true;
    if (state->observer != NULL) {
        state->observer->discontinuity = true;
    }
    nx_stream_note_loss(state->base.stream, 1U);
    (void)rollback(state);
}

/** \brief Acquire two future loans before enabling the double-buffer engine. */
static nx_result_t arm(nx_stm32_adc_continuous_state_t* state) {
    nx_stm32_adc_stream_state_t* base = &state->base;
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
    ADC_TypeDef* adc = base->adc.registers;
    adc->CR2 = 0U;
    adc->CR1 = base->adc.channel_count > 1U ? (uint32_t)ADC_CR1_SCAN : 0U;
    LL_ADC_SetResolution(adc,
                         (uint32_t)base->adc.resolution << ADC_CR1_RES_Pos);
    nx_stm32_adc_stream_sequence(base);
    adc->SR = 0U;
    nx_stm32_adc_trigger_program(base->timer,
                                 nx_stm32_adc_trigger_fact(base->trigger),
                                 base->prescaler, base->period);
    acknowledge(base, 0x3DU);
    base->memory->PAR = (uint32_t)(uintptr_t)&adc->DR;
    base->memory->M0AR = (uint32_t)(uintptr_t)state->targets[0].data;
    base->memory->M1AR = (uint32_t)(uintptr_t)state->targets[1].data;
    base->memory->NDTR =
        (uint32_t)(state->targets[0].capacity / sizeof(uint16_t));
    base->memory->FCR = DMA_SxFCR_FEIE;
    base->memory->CR = nx_stm32_adc_dma_channel(adc) | DMA_SxCR_DBM |
                       DMA_SxCR_CIRC | DMA_SxCR_MINC | DMA_SxCR_PSIZE_0 |
                       DMA_SxCR_MSIZE_0 | DMA_SxCR_PL_1 | DMA_SxCR_TCIE |
                       DMA_SxCR_TEIE | DMA_SxCR_DMEIE;
    nx_arch_dsb();
    base->memory->CR |= DMA_SxCR_EN;
    adc->CR2 = ADC_CR2_ADON | ADC_CR2_DMA | ADC_CR2_DDS |
               ((uint32_t)nx_stm32_adc_trigger_fact(base->trigger)->selection
                << ADC_CR2_EXTSEL_Pos);
    nx_arch_dsb();
    (void)adc->CR2;
    result =
        LL_ADC_IsEnabled(adc) != 0U
            ? nx_deadline_finite_after(
                  nx_time_now_us(), nx_stm32_adc_startup_delay_us(&base->adc),
                  &base->ready_at)
            : NX_ERROR_IO;
    if (result != NX_SUCCESS) {
        halt(state, result);
        return result;
    }
    base->warming = true;
    state->halted = false;
    return NX_SUCCESS;
}

/** \brief Preserve the exact existing route and vendor-private cold checks. */
nx_result_t
nx_stm32_adc_continuous_initialize(nx_stm32_adc_continuous_state_t* state) {
    return state == NULL ? NX_ERROR_INVALID
                         : nx_stm32_adc_stream_initialize(&state->base);
}

/** \brief Validate equal complete-scan DMA blocks before either future loan. */
static nx_result_t start(void* context, nx_stream_t* stream, uint32_t hz) {
    nx_stm32_adc_continuous_state_t* state = context;
    if (state == NULL || stream == NULL || stream->slots == NULL ||
        stream->count < 3U) {
        return NX_ERROR_INVALID;
    }
    nx_stm32_adc_stream_state_t* base = &state->base;
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (!base->adc.initialized) {
        return NX_ERROR_STATE;
    }
    uint16_t prescaler, period;
    nx_result_t result =
        nx_stm32_adc_stream_rate(base, hz, &prescaler, &period);
    if (result == NX_SUCCESS) {
        result = nx_stm32_adc_stream_blocks(base, stream, sizeof(*state));
    }
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_time_us_t ready_at;
    result = nx_deadline_finite_after(
        nx_time_now_us(), nx_stm32_adc_startup_delay_us(&base->adc), &ready_at);
    if (result != NX_SUCCESS) {
        return result;
    }
    for (size_t i = 1U; i < stream->count; ++i) {
        if (stream->slots[i].capacity != stream->slots[0].capacity) {
            return NX_ERROR_INVALID;
        }
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (base->stream != NULL || base->adc.active ||
        (base->memory->CR & DMA_SxCR_EN) != 0U ||
        (base->timer->CR1 & TIM_CR1_CEN) != 0U) {
        nx_arch_irq_restore(saved);
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
        base->period = period;
        base->trigger_hz = hz;
        base->fault = false;
        state->fault_result = NX_SUCCESS;
        base->stopping = false;
        base->adc.active = true;
        result = arm(state);
        if (result != NX_SUCCESS) {
            if (base->fault) {
                /* Hardware effects and loans make this an accepted failure.
                 * service reports it; stop remains the final storage join. */
                result = NX_SUCCESS;
            } else {
                base->stream = NULL;
                base->adc.active = false;
                (void)nx_adc_observer_unbind(state->observer);
            }
        }
    }
    nx_arch_irq_restore(saved);
    return result;
}

/** \brief Swap inactive target and check hardware facts before READY publish.
 */
void nx_stm32_adc_continuous_irq(nx_stm32_adc_continuous_state_t* state) {
    if (state == NULL || state->base.stream == NULL || state->base.stopping ||
        state->halted) {
        return;
    }
    nx_stm32_adc_stream_state_t* base = &state->base;
    uint32_t flags = nx_stm32_adc_dma_flags(base);
    if ((flags & 0x2DU) == 0U) {
        return;
    }
    unsigned active = (base->memory->CR & DMA_SxCR_CT) != 0U ? 1U : 0U;
    unsigned bank = active ^ 1U;
    if ((flags & 0x0DU) != 0U || bank != state->expected_bank ||
        (base->adc.registers->SR & ADC_SR_OVR) != 0U) {
        halt(state, NX_ERROR_IO);
    } else if (!nx_stream_post_can_reserve(&state->posted)) {
        halt(state, base->stream->epoch == UINT64_MAX ? NX_ERROR_EXHAUSTED
                                                      : NX_SUCCESS);
    } else {
        nx_result_t result =
            nx_stream_post_reserve(&state->posted, &base->fill);
        if (result != NX_SUCCESS) {
            halt(state, NX_ERROR_IO);
        } else {
            base->filling = true;
            acknowledge(base, flags);
            NX_STM32_ADC_CONTINUOUS_TARGET(
                base->memory, bank, (uint32_t)(uintptr_t)base->fill.data);
            nx_arch_dsb();
            uint32_t old = (uint32_t)(uintptr_t)state->targets[bank].data;
            uint32_t replacement = (uint32_t)(uintptr_t)base->fill.data;
            uint32_t other = (uint32_t)(uintptr_t)state->targets[active].data;
            bool addresses = bank == 0U ? base->memory->M0AR == replacement &&
                                              base->memory->M1AR == other
                                        : base->memory->M1AR == replacement &&
                                              base->memory->M0AR == other;
            bool detached =
                addresses && base->memory->M0AR != old &&
                base->memory->M1AR != old &&
                ((base->memory->CR & DMA_SxCR_CT) != 0U ? 1U : 0U) == active &&
                (base->memory->CR & DMA_SxCR_EN) != 0U &&
                (nx_stm32_adc_dma_flags(base) & 0x2DU) == 0U &&
                (base->adc.registers->SR & ADC_SR_OVR) == 0U;
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
                    state->targets[bank] = base->fill;
                    base->filling = false;
                    state->expected_bank ^= 1U;
                } else {
                    halt(state, result);
                }
            } else {
                halt(state, NX_ERROR_IO);
            }
        }
    }
    (void)nx_irq_wake_signal(base->wake);
}

/** \brief Task executor alone resumes starvation gaps and ADC warmup. */
static nx_result_t service(void* context) {
    nx_stm32_adc_continuous_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_stm32_adc_stream_state_t* base = &state->base;
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (base->stream == NULL || base->stopping) {
        return NX_ERROR_STATE;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result = NX_SUCCESS;
    if (!state->halted && (base->adc.registers->SR & ADC_SR_OVR) != 0U) {
        halt(state, NX_ERROR_IO);
    }
    if (state->halted) {
        result = rollback(state);
        if (result == NX_SUCCESS) {
            result = base->fault ? state->fault_result : arm(state);
            if (result == NX_SUCCESS) {
                result = NX_ERROR_BUSY;
            }
        }
    } else if (base->warming) {
        if (nx_deadline_expired(base->ready_at, nx_time_now_us())) {
            base->adc.registers->CR2 |= ADC_CR2_EXTEN_0;
            nx_arch_dsb();
            LL_TIM_EnableCounter(base->timer);
            base->warming = false;
        } else {
            result = NX_ERROR_BUSY;
        }
    }
    nx_arch_irq_restore(saved);
    return result;
}

/** \brief Retain caller state through failed source drain and consumer joins.
 */
static nx_result_t stop(void* context) {
    nx_stm32_adc_continuous_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_stm32_adc_stream_state_t* base = &state->base;
    if (base->stream == NULL) {
        return NX_SUCCESS;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
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
    nx_arch_irq_restore(saved);
    return result;
}

/** \brief Reuse the exact reviewed DMA vector policy for optional hints. */
static nx_result_t attach_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t syscall_ceiling) {
    nx_stm32_adc_continuous_state_t* state = context;
    return state == NULL ? NX_ERROR_INVALID
                         : nx_stm32_adc_stream_ops.attach_wake(
                               &state->base, wake, syscall_ceiling);
}

/** \brief Private timing codes yield a copied nominal scan duration, not
 * instants.
 */
static nx_result_t sequence_info(const void* context,
                                 nx_adc_sequence_info_t* info) {
    const nx_stm32_adc_continuous_state_t* state = context;
    if (state == NULL || info == NULL || !state->base.adc.initialized) {
        return NX_ERROR_INVALID;
    }
    const nx_stm32_adc_stream_state_t* base = &state->base;
    static const uint16_t cycles[] = {3U, 15U, 28U, 56U, 84U, 112U, 144U, 480U};
    uint32_t scan = 0U;
    for (size_t i = 0U; i < base->adc.channel_count; ++i) {
        scan += cycles[base->adc.sample_times[i]] +
                (12U - 2U * base->adc.resolution);
    }
    *info = (nx_adc_sequence_info_t){.channel_count = base->adc.channel_count,
                                     .scan_ticks = scan,
                                     .ticks_per_second = base->adc_clock_hz};
    return NX_SUCCESS;
}

/** \brief Attach only before any block or hardware producer becomes live. */
static nx_result_t attach_observer(void* context, nx_adc_observer_t* observer) {
    nx_stm32_adc_continuous_state_t* state = context;
    if (state == NULL || !state->base.adc.initialized) {
        return NX_ERROR_STATE;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
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
    nx_arch_irq_restore(saved);
    return result;
}

/** \brief Released/stale epochs never expose another loan's observation. */
static nx_result_t block_observation(const void* context,
                                     const nx_stream_block_t* block,
                                     nx_adc_observation_t* observation) {
    const nx_stm32_adc_continuous_state_t* state = context;
    if (state == NULL || !state->base.adc.initialized ||
        state->observer == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return nx_adc_observer_read(state->observer, block, observation);
}

/** \brief Immutable continuous provider; common ADC is its first subobject. */
const nx_adc_ops_t nx_stm32_adc_continuous_ops = {
    .info = nx_stm32_adc_info,
    .sample = nx_stm32_adc_sample,
    .stream_start = start,
    .stream_stop = stop,
    .stream_service = service,
    .attach_wake = attach_wake,
    .sequence_info = sequence_info,
    .attach_observer = attach_observer,
    .block_observation = block_observation,
};
