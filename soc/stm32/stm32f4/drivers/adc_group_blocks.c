/**
 * \file            adc_group_blocks.c
 * \brief           Common-ADC CDR blocks with proved double-buffer ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_adc_group_blocks.h"
#include "stm32f407_adc_group_combined.h"
#include "stm32f407_adc_group_internal.h"
#include "stm32f4_adc_pins.h"
#include "stm32f4_adc_trigger.h"
#include "stm32f4xx_ll_adc.h"
#include "stm32f4xx_ll_tim.h"
#include <string.h>

#ifndef NX_STM32_ADC_GROUP_ACK
#define NX_STM32_ADC_GROUP_ACK(dma, flags) ((dma)->LIFCR = (flags))
#endif
#ifndef NX_STM32_ADC_GROUP_TARGET
#define NX_STM32_ADC_GROUP_TARGET(memory, bank, address)                       \
    do {                                                                       \
        if ((bank) == 0U)                                                      \
            (memory)->M0AR = (address);                                        \
        else                                                                   \
            (memory)->M1AR = (address);                                        \
    } while (0)
#endif

enum { INJECTED_TIMER_OWNER = 8U, INJECTED_TIMER_CLOCK = 16U };

static const uint16_t s_sample_cycles[8] = {3U,  15U,  28U,  56U,
                                            84U, 112U, 144U, 480U};
static bool task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    uintptr_t ap = (uintptr_t)a, bp = (uintptr_t)b;
    return a == NULL || b == NULL || an == 0U || bn == 0U ||
           an > UINTPTR_MAX - ap || bn > UINTPTR_MAX - bp ||
           (ap < bp + bn && bp < ap + an);
}
static unsigned width(const nx_stm32_adc_group_blocks_state_t* s) {
    return nx_stm32_adc_group_width(&s->config->adc);
}
static size_t scan_bytes(const nx_stm32_adc_group_blocks_state_t* s) {
    return (size_t)nx_stm32_adc_group_samples(&s->config->adc) *
           nx_stm32_adc_group_storage(&s->config->adc);
}
/** \brief Keep mutable metadata and DMA capacity away from the entire owner.
 */
static size_t
owner_bytes(const nx_stm32_adc_group_combined_config_t* combined) {
    return combined == NULL ? sizeof(nx_stm32_adc_group_blocks_state_t)
                            : sizeof(nx_stm32_adc_group_combined_state_t);
}
static bool independent(const nx_stm32_adc_group_blocks_state_t* s,
                        const void* data, size_t bytes, size_t alignment,
                        const nx_stm32_adc_group_combined_config_t* combined) {
    if (!nx_stm32_adc_group_independent(&s->base, owner_bytes(combined), data,
                                        bytes, alignment) ||
        overlap(data, bytes, s->config, sizeof(*s->config)))
        return false;
    if (combined != NULL) {
        size_t count =
            combined->regular.adc.mode == NX_STM32_ADC_TRIPLE_REGULAR ? 3U : 2U;
        size_t ranks = count * combined->injected_ranks;
        if ((combined->injected_timer != NULL &&
             overlap(data, bytes, combined->injected_timer,
                     sizeof(*combined->injected_timer))) ||
            overlap(data, bytes, combined, sizeof(*combined)) ||
            overlap(data, bytes, combined->injected_channels, ranks) ||
            overlap(data, bytes, combined->injected_samples, ranks) ||
            overlap(data, bytes, combined->injected_converters, ranks))
            return false;
    }
    return s->base.active == NULL ||
           (!overlap(data, bytes, s->base.active, sizeof(*s->base.active)) &&
            !overlap(data, bytes, s->base.active->output.frames,
                     s->base.active->bytes));
}
static bool controls(const nx_stm32_adc_group_blocks_state_t* s,
                     const void* data, size_t bytes, size_t alignment,
                     const nx_stm32_adc_group_combined_config_t* combined) {
    return independent(s, data, bytes, alignment, combined) &&
           (s->observer == NULL ||
            (!overlap(data, bytes, s->observer, sizeof(*s->observer)) &&
             !overlap(data, bytes, s->observer->records,
                      s->observer->count * sizeof(*s->observer->records))));
}
/** \brief Lost gated-register history cannot be repaired by a reset facade. */
static bool live(nx_stm32_adc_group_blocks_state_t* s) {
    const nx_stm32_adc_group_config_t* c = &s->config->adc;
    bool software = nx_stm32_adc_group_interleaved(c);
    const nx_stm32_adc_trigger_fact_t* trigger =
        software ? NULL : nx_stm32_adc_trigger_fact(c->trigger);
    uint32_t gates =
        RCC_APB2ENR_ADC1EN | RCC_APB2ENR_ADC2EN | RCC_APB2ENR_ADC3EN;
    bool ok =
        !s->history_lost && nx_stm32_adc_group_epoch_live(&s->base) &&
        (software || trigger != NULL) && (c->rcc->APB2ENR & gates) == gates &&
        (c->rcc->APB2RSTR & RCC_APB2RSTR_ADCRST) == 0U &&
        (c->rcc->AHB1ENR & RCC_AHB1ENR_DMA2EN) != 0U &&
        (c->rcc->AHB1RSTR & RCC_AHB1RSTR_DMA2RST) == 0U &&
        (software || ((((trigger->apb2 ? c->rcc->APB2ENR : c->rcc->APB1ENR) &
                        trigger->gate) != 0U) &&
                      (((trigger->apb2 ? c->rcc->APB2RSTR : c->rcc->APB1RSTR) &
                        trigger->gate) == 0U)));
    if (!ok && s->stream != NULL)
        s->history_lost = true;
    return ok;
}
static void quiet_vectors(void) {
#ifndef NEXUS_STM32_MODEL
    NVIC_DisableIRQ(ADC_IRQn);
    NVIC_DisableIRQ(DMA2_Stream0_IRQn);
#endif
}
/** \brief IRQ only records facts; the task performs all buffer publication. */
void nx_stm32_adc_group_blocks_irq(const nx_adc_group_port_t* port) {
    if (port == NULL || port->context == NULL)
        return;
    nx_stm32_adc_group_blocks_state_t* s = port->context;
    if (s->config == NULL || s->stream == NULL ||
        s->base.phase != NX_ADC_GROUP_ACQUIRING)
        return;
    if (!live(s)) {
        quiet_vectors();
        return;
    }
    const nx_stm32_adc_group_config_t* c = &s->config->adc;
    uint32_t events = nx_stm32_adc_group_observed(c);
    uint32_t flags = c->dma->LISR & UINT32_C(0x3d);
    NX_STM32_ADC_GROUP_ACK(c->dma, flags);
    nx_arch_dsb();
    events |= nx_stm32_adc_group_observed(c) & NX_ADC_GROUP_ERROR;
    if ((c->dma->LISR & flags) != 0U)
        events |= NX_ADC_GROUP_ERROR;
    __atomic_fetch_or(&s->base.events, events, __ATOMIC_RELAXED);
    if ((events & NX_ADC_GROUP_ERROR) != 0U) {
        /* A sticky W1C source must not prevent the task from reaching drain.
         * Quiet only owned publishers. EN/addresses and every producer loan
         * remain live until the separate withdrawal proof succeeds. */
        quiet_vectors();
        c->memory->CR &= ~(uint32_t)(DMA_SxCR_TCIE | DMA_SxCR_HTIE |
                                     DMA_SxCR_TEIE | DMA_SxCR_DMEIE);
        c->memory->FCR &= ~(uint32_t)DMA_SxFCR_FEIE;
        nx_stm32_adc_group_gate(c);
    }
}
/** \brief Stop sampling without converting uncertainty into a memory release.
 */
static void halt(nx_stm32_adc_group_blocks_state_t* s, nx_result_t result) {
    if (s->base.phase != NX_ADC_GROUP_DRAINING) {
        s->base.terminal = result;
        s->base.fault = result != NX_SUCCESS && result != NX_ERROR_BUSY;
        s->base.phase = NX_ADC_GROUP_DRAINING;
        nx_time_us_t now = nx_time_now_us();
        if (nx_deadline_finite_after(now, s->config->adc.drain_timeout_us,
                                     &s->base.drain_deadline) != NX_SUCCESS)
            s->base.drain_deadline = now == NX_DEADLINE_NEVER ? now - 1U : now;
        if (result != NX_SUCCESS) {
            nx_stream_note_loss(s->stream, 1U);
            if (s->observer != NULL)
                s->observer->discontinuity = true;
        }
    }
    if (live(s))
        nx_stm32_adc_group_gate(&s->config->adc);
    else
        quiet_vectors();
}
/** \brief LIFO abort is legal only after the common hardware owner withdraws.
 */
static nx_result_t drain(nx_stm32_adc_group_blocks_state_t* s) {
    if (!live(s))
        return NX_ERROR_STATE;
    /* The helper does bounded register/IRQ observations, never polls. This
     * short guard also protects a hint acknowledged by the new IRQ entry. */
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    bool stopped = nx_stm32_adc_group_withdraw(&s->base);
    nx_arch_irq_restore(saved);
    if (!stopped)
        return nx_deadline_expired(s->base.drain_deadline, nx_time_now_us())
                   ? NX_ERROR_IO
                   : NX_ERROR_BUSY;
    if (s->replacement_live) {
        nx_result_t result =
            nx_stream_post_abort(&s->posted, &s->replacement, true);
        if (result != NX_SUCCESS)
            return result;
        s->replacement_live = false;
    }
    if (s->posted.pending == 2U) {
        nx_result_t result = nx_stream_post_abort(
            &s->posted, &s->targets[s->expected_bank ^ 1U], true);
        if (result != NX_SUCCESS)
            return result;
    }
    return s->posted.pending == 1U
               ? nx_stream_post_abort(&s->posted, &s->targets[s->expected_bank],
                                      true)
               : NX_SUCCESS;
}
static bool three_free(const nx_stm32_adc_group_blocks_state_t* s) {
    if (s->posted.pending != 0U || !nx_stream_post_can_reserve(&s->posted))
        return false;
    size_t next = s->posted.cursor;
    for (unsigned i = 0U; i < 3U; ++i) {
        if (s->stream->slots[next].state != NX_STREAM_SLOT_FREE)
            return false;
        if (++next == s->stream->count)
            next = 0U;
    }
    return true;
}
/** \brief Reserve both future banks before powering a new sampling epoch. */
static nx_result_t warm(nx_stm32_adc_group_blocks_state_t* s,
                        const nx_stm32_adc_group_combined_config_t* combined) {
    /* A held consumer must leave room for both banks and the next replacement
     * before a new sampling epoch is allowed to produce another full block. */
    if (!three_free(s))
        return NX_ERROR_BUSY;
    if (s->stream->epoch > UINT64_MAX - 2U)
        return NX_ERROR_EXHAUSTED;
    nx_result_t result = nx_stream_post_reserve(&s->posted, &s->targets[0]);
    if (result != NX_SUCCESS)
        return result;
    s->expected_bank = 0U;
    result = nx_stream_post_reserve(&s->posted, &s->targets[1]);
    if (result == NX_SUCCESS) {
        s->base.events = 0U;
        s->base.phase = NX_ADC_GROUP_WARM;
        result = combined == NULL
                     ? nx_stm32_adc_group_prime(&s->base)
                     : nx_stm32_adc_group_prime_combined(&s->base, combined);
    }
    if (result != NX_SUCCESS)
        halt(s, result);
    return result;
}
/** \brief LL cold programming is shared; this engine owns exact DBM targets.
 */
static nx_result_t arm(nx_stm32_adc_group_blocks_state_t* s,
                       nx_time_us_t observed) {
    const nx_stm32_adc_group_config_t* c = &s->config->adc;
    bool software = nx_stm32_adc_group_interleaved(c);
    const nx_stm32_adc_trigger_fact_t* trigger =
        software ? NULL : nx_stm32_adc_trigger_fact(c->trigger);
    DMA_Stream_TypeDef* m = c->memory;
    uint32_t words = (uint32_t)(s->targets[0].capacity / width(s));
    if (!software)
        nx_stm32_adc_trigger_program(c->timer, trigger, s->base.prescaler,
                                     s->base.period);
    NX_STM32_ADC_GROUP_ACK(c->dma, UINT32_C(0x3d));
    uint32_t control = DMA_SxCR_DBM | DMA_SxCR_CIRC | DMA_SxCR_MINC |
                       DMA_SxCR_PL_1 | DMA_SxCR_TCIE | DMA_SxCR_TEIE |
                       DMA_SxCR_DMEIE |
                       (width(s) == 4U ? DMA_SxCR_PSIZE_1 | DMA_SxCR_MSIZE_1
                                       : DMA_SxCR_PSIZE_0 | DMA_SxCR_MSIZE_0);
    m->CR = control;
    m->FCR = DMA_SxFCR_FEIE;
    m->PAR = (uint32_t)(uintptr_t)&c->common->CDR;
    m->M0AR = (uint32_t)(uintptr_t)s->targets[0].data;
    m->M1AR = (uint32_t)(uintptr_t)s->targets[1].data;
    m->NDTR = words;
    nx_arch_dsb();
    if (!live(s))
        return NX_ERROR_STATE;
    /* Width, direction, channel, DBM and SINGLE/direct transport are part of
     * the capacity/time proof, not preferences inherited from a prior owner. */
    if (m->CR != control ||
        (m->FCR & (DMA_SxFCR_DMDIS | DMA_SxFCR_FEIE | DMA_SxFCR_FTH)) !=
            DMA_SxFCR_FEIE ||
        m->PAR != (uint32_t)(uintptr_t)&c->common->CDR ||
        m->M0AR != (uint32_t)(uintptr_t)s->targets[0].data ||
        m->M1AR != (uint32_t)(uintptr_t)s->targets[1].data ||
        m->NDTR != words || (c->dma->LISR & UINT32_C(0x3d)) != 0U)
        return NX_ERROR_IO;
    /* Retain the observation before DMA/trigger enable. A long preemption
     * after the last store must remain visible to the following service. */
    s->observed_at = observed;
    s->base.phase = NX_ADC_GROUP_ACQUIRING;
    nx_stm32_adc_group_vectors_enable(&s->base);
    m->CR |= DMA_SxCR_EN;
    if (software) {
        for (unsigned i = 0U; i < nx_stm32_adc_group_count(c); ++i)
            LL_ADC_REG_SetContinuousMode(c->adc[i], LL_ADC_REG_CONV_CONTINUOUS);
    } else {
        c->adc[0]->CR2 |= ((uint32_t)trigger->selection << ADC_CR2_EXTSEL_Pos) |
                          ADC_CR2_EXTEN_0;
    }
    nx_arch_dsb();
    if ((m->CR & DMA_SxCR_EN) == 0U)
        return NX_ERROR_IO;
    if (software) {
        for (unsigned i = 0U; i < nx_stm32_adc_group_count(c); ++i)
            if ((c->adc[i]->CR2 &
                 (ADC_CR2_ADON | ADC_CR2_CONT | ADC_CR2_EXTEN)) !=
                (ADC_CR2_ADON | ADC_CR2_CONT))
                return NX_ERROR_IO;
        LL_ADC_REG_StartConversionSWStart(c->adc[0]);
    } else {
        if ((c->adc[0]->CR2 & ADC_CR2_EXTEN) != ADC_CR2_EXTEN_0)
            return NX_ERROR_IO;
        LL_TIM_EnableCounter(c->timer);
    }
    nx_arch_dsb();
    return software ? (live(s) ? NX_SUCCESS : NX_ERROR_STATE)
                    : ((c->timer->CR1 & TIM_CR1_CEN) != 0U ? NX_SUCCESS
                                                           : NX_ERROR_IO);
}
/** \brief Two service windows must not hide a complete second DMA bank. */
static bool bank_budget(const nx_stm32_adc_group_blocks_state_t* s,
                        size_t bytes) {
    const nx_stm32_adc_group_config_t* c = &s->config->adc;
    uint64_t guard = UINT64_C(2) * s->config->max_service_gap_us + 2U;
    if (!nx_stm32_adc_group_interleaved(c))
        return (uint64_t)(bytes / scan_bytes(s)) * c->period_us >
               guard + c->period_us;
    unsigned converters = nx_stm32_adc_group_count(c);
    uint64_t storage = nx_stm32_adc_group_storage(c);
    uint64_t samples = bytes / storage;
    /* Conservatively reserve each DR/current conversion plus the documented
     * four-word FIFO and one SINGLE beat at each independent AHB master.
     * DELAY can lengthen spacing; it is never treated as a fixed aperture. */
    uint64_t backlog = UINT64_C(2) * converters + 24U / storage;
    if (samples <= backlog)
        return false;
    uint64_t cycles =
        s_sample_cycles[c->sample_times[0]] + 12U - 2U * c->resolution;
    return (samples - backlog) * cycles * 1000000U >
           guard * converters * c->adc_clock_hz;
}
/** \brief Check the whole concrete owner before the first producer loan. */
static nx_result_t
start_selected(void* context, nx_stream_t* stream,
               const nx_stm32_adc_group_combined_config_t* combined) {
    nx_stm32_adc_group_blocks_state_t* s = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->base.initialized || s->base.closing)
        return NX_ERROR_STATE;
    if (s->stream != NULL || s->base.fault)
        return NX_ERROR_BUSY;
    if (!live(s))
        return NX_ERROR_STATE;
    if (stream == NULL || stream->count < 3U ||
        stream->count > SIZE_MAX / sizeof(*stream->slots) ||
        !controls(s, stream, sizeof(*stream), _Alignof(nx_stream_t),
                  combined) ||
        !controls(s, stream->slots, stream->count * sizeof(*stream->slots),
                  _Alignof(nx_stream_slot_t), combined))
        return NX_ERROR_INVALID;
    if (stream->stopping || stream->outstanding != 0U)
        return NX_ERROR_BUSY;
    if (stream->epoch > UINT64_MAX - 2U)
        return NX_ERROR_EXHAUSTED;
    size_t bytes = stream->slots[0].capacity;
    size_t scan = scan_bytes(s);
    if (bytes == 0U || bytes % scan != 0U || bytes / width(s) > 65535U)
        return NX_ERROR_INVALID;
    if (!bank_budget(s, bytes))
        return NX_ERROR_INVALID;
    for (size_t i = 0U; i < stream->count; ++i) {
        const nx_stream_slot_t* slot = &stream->slots[i];
        if (slot->capacity != bytes || slot->state != NX_STREAM_SLOT_FREE ||
            !controls(s, slot->data, slot->capacity, width(s), combined))
            return NX_ERROR_INVALID;
        nx_result_t result = nx_dma_buffer_validate(
            s->config->adc.regions, s->config->adc.region_count, slot->data,
            slot->capacity, width(s), NX_DMA_FROM_DEVICE);
        if (result != NX_SUCCESS)
            return result;
    }
    nx_time_us_t until;
    nx_result_t result = nx_deadline_finite_after(nx_time_now_us(), 4U, &until);
    if (result != NX_SUCCESS)
        return result;
    if (s->observer != NULL) {
        result =
            nx_adc_observer_bind(s->observer, stream, s, owner_bytes(combined));
        if (result != NX_SUCCESS)
            return result;
    }
    result = nx_stream_post_initialize(&s->posted, stream);
    if (result != NX_SUCCESS) {
        (void)nx_adc_observer_unbind(s->observer);
        return result;
    }
    s->stream = stream;
    s->stopping = false;
    result = warm(s, combined);
    if (result != NX_SUCCESS && s->posted.pending == 0U) {
        s->stream = NULL;
        memset(&s->posted, 0, sizeof(s->posted));
        (void)nx_adc_observer_unbind(s->observer);
        return result;
    }
    return NX_SUCCESS;
}
/** \brief Replace the inactive address before any release-publication. */
static nx_result_t progress(nx_stm32_adc_group_blocks_state_t* s,
                            nx_time_us_t observed) {
    const nx_stm32_adc_group_config_t* c = &s->config->adc;
    unsigned bank = s->expected_bank;
    uint32_t current = (c->memory->CR & DMA_SxCR_CT) != 0U ? 1U : 0U;
    if (current != (bank ^ 1U) || (c->memory->CR & DMA_SxCR_EN) == 0U)
        return NX_ERROR_IO;
    if (s->stream->epoch == UINT64_MAX)
        return NX_ERROR_EXHAUSTED;
    if (!nx_stream_post_can_reserve(&s->posted))
        return NX_ERROR_BUSY;
    nx_result_t result = nx_stream_post_reserve(&s->posted, &s->replacement);
    if (result != NX_SUCCESS)
        return result;
    s->replacement_live = true;
    uint32_t address = (uint32_t)(uintptr_t)s->replacement.data;
    NX_STM32_ADC_GROUP_TARGET(c->memory, bank, address);
    nx_arch_dsb();
    if (!live(s))
        return NX_ERROR_STATE;
    if ((bank == 0U ? c->memory->M0AR : c->memory->M1AR) != address ||
        ((c->memory->CR & DMA_SxCR_CT) != 0U ? 1U : 0U) != current ||
        (c->dma->LISR & DMA_LISR_TCIF0) != 0U ||
        nx_time_now_us() - observed > s->config->max_service_gap_us)
        return NX_ERROR_IO;
    nx_stm32_adc_group_blocks_irq(&(nx_adc_group_port_t){
        .ops = &nx_stm32_adc_group_blocks_ops, .context = s});
    if (!live(s))
        return NX_ERROR_STATE;
    if ((s->base.events & NX_ADC_GROUP_ERROR) != 0U)
        return NX_ERROR_IO;
    result = s->observer == NULL
                 ? NX_SUCCESS
                 : nx_adc_observer_record(s->observer, &s->targets[bank],
                                          nx_time_now_us());
    if (result == NX_SUCCESS)
        result = nx_stream_post_publish(
            &s->posted, &s->targets[bank], s->targets[bank].capacity,
            nx_stm32_adc_group_interleaved(c) ? 0U
                                              : NX_STREAM_BOUNDARY_TRIGGER);
    if (result == NX_SUCCESS) {
        s->targets[bank] = s->replacement;
        s->replacement_live = false;
        s->expected_bank ^= 1U;
        s->base.events &= ~(uint32_t)NX_ADC_GROUP_TRANSFER;
    }
    return result;
}
static nx_result_t
service_selected(void* context,
                 const nx_stm32_adc_group_combined_config_t* combined) {
    nx_stm32_adc_group_blocks_state_t* s = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || s->stream == NULL || s->stopping)
        return NX_ERROR_STATE;
    nx_time_us_t observed = nx_time_now_us();
    if (observed == NX_DEADLINE_NEVER || !live(s)) {
        halt(s, NX_ERROR_STATE);
        return NX_ERROR_STATE;
    }
    if (s->base.phase == NX_ADC_GROUP_WARM) {
        if (!nx_deadline_expired(s->base.warm_until, observed))
            return NX_ERROR_BUSY;
        nx_arch_irq_state_t saved = nx_arch_irq_save();
        nx_result_t result = arm(s, observed);
        nx_arch_irq_restore(saved);
        if (result != NX_SUCCESS)
            halt(s, result);
        return result;
    }
    if (s->base.phase == NX_ADC_GROUP_ACQUIRING) {
        nx_arch_irq_state_t saved = nx_arch_irq_save();
        nx_stm32_adc_group_blocks_irq(&(nx_adc_group_port_t){
            .ops = &nx_stm32_adc_group_blocks_ops, .context = s});
        nx_result_t result = NX_SUCCESS;
        if (!live(s))
            result = NX_ERROR_STATE;
        else if (observed < s->observed_at ||
                 observed - s->observed_at > s->config->max_service_gap_us ||
                 (s->base.events & NX_ADC_GROUP_ERROR) != 0U)
            result = NX_ERROR_IO;
        else if ((s->base.events & NX_ADC_GROUP_TRANSFER) != 0U)
            result = progress(s, observed);
        nx_arch_irq_restore(saved);
        if (result != NX_SUCCESS)
            halt(s, result);
        else {
            s->observed_at = observed;
            return NX_SUCCESS;
        }
    }
    nx_result_t result = drain(s);
    if (result != NX_SUCCESS)
        return result;
    if (s->base.fault)
        return s->base.terminal;
    /* A shared injected borrow must join before any stopped ADC is rearmed. */
    if (s->base.active != NULL)
        return NX_ERROR_BUSY;
    result = warm(s, combined);
    return result == NX_SUCCESS ? NX_ERROR_BUSY : result;
}
static nx_result_t stop(void* context) {
    nx_stm32_adc_group_blocks_state_t* s = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->base.initialized)
        return NX_ERROR_STATE;
    if (s->stream == NULL)
        return NX_SUCCESS;
    s->stopping = true;
    halt(s, NX_SUCCESS);
    nx_result_t result = drain(s);
    if (result == NX_SUCCESS)
        result = nx_stream_stop(s->stream);
    if (result == NX_SUCCESS)
        result = nx_adc_observer_unbind(s->observer);
    if (result == NX_SUCCESS) {
        s->stream = NULL;
        memset(&s->posted, 0, sizeof(s->posted));
        s->base.phase = NX_ADC_GROUP_IDLE;
        s->base.fault = false;
        s->base.events = 0U;
        s->base.terminal = NX_SUCCESS;
        s->stopping = false;
    }
    return result;
}
static nx_result_t
attach_selected(void* context, nx_adc_observer_t* observer,
                const nx_stm32_adc_group_combined_config_t* combined) {
    nx_stm32_adc_group_blocks_state_t* s = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->base.initialized || s->base.closing)
        return NX_ERROR_STATE;
    if (s->stream != NULL)
        return NX_ERROR_BUSY;
    if (observer != NULL &&
        (!independent(s, observer, sizeof(*observer),
                      _Alignof(nx_adc_observer_t), combined) ||
         observer->count == 0U ||
         observer->count > SIZE_MAX / sizeof(*observer->records) ||
         !independent(s, observer->records,
                      observer->count * sizeof(*observer->records),
                      _Alignof(nx_adc_observation_t), combined) ||
         overlap(observer, sizeof(*observer), observer->records,
                 observer->count * sizeof(*observer->records))))
        return NX_ERROR_INVALID;
    if (observer != NULL && observer->stream != NULL)
        return NX_ERROR_BUSY;
    s->observer = observer;
    return NX_SUCCESS;
}
static bool output(const nx_stm32_adc_group_blocks_state_t* s, const void* data,
                   size_t bytes, size_t alignment,
                   const nx_stm32_adc_group_combined_config_t* combined) {
    if (!controls(s, data, bytes, alignment, combined))
        return false;
    if (s->stream == NULL)
        return true;
    if (overlap(data, bytes, s->stream, sizeof(*s->stream)) ||
        overlap(data, bytes, s->stream->slots,
                s->stream->count * sizeof(*s->stream->slots)))
        return false;
    for (size_t i = 0U; i < s->stream->count; ++i)
        if (overlap(data, bytes, s->stream->slots[i].data,
                    s->stream->slots[i].capacity))
            return false;
    return true;
}
static nx_result_t
info_selected(const void* context, nx_adc_group_info_t* result,
              const nx_stm32_adc_group_combined_config_t* combined) {
    const nx_stm32_adc_group_blocks_state_t* s = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->base.initialized)
        return NX_ERROR_STATE;
    if (!output(s, result, sizeof(*result), _Alignof(nx_adc_group_info_t),
                combined))
        return NX_ERROR_INVALID;
    nx_adc_group_info_t value;
    nx_result_t status = nx_stm32_adc_group_info_snapshot(&s->base, &value);
    if (status == NX_SUCCESS) {
        value.active = s->stream != NULL;
        value.fault = s->base.fault;
        value.quarantined =
            s->stream != NULL &&
            (s->history_lost || (s->base.phase == NX_ADC_GROUP_DRAINING &&
                                 s->posted.pending != 0U));
        *result = value;
    }
    return status;
}
static nx_result_t
observation_selected(const void* context, const nx_stream_block_t* block,
                     nx_adc_observation_t* result,
                     const nx_stm32_adc_group_combined_config_t* combined) {
    const nx_stm32_adc_group_blocks_state_t* s = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->base.initialized || s->observer == NULL)
        return NX_ERROR_UNSUPPORTED;
    if (!output(s, result, sizeof(*result), _Alignof(nx_adc_observation_t),
                combined))
        return NX_ERROR_INVALID;
    return nx_adc_observer_read(s->observer, block, result);
}
nx_result_t nx_stm32_adc_group_blocks_prepare(
    nx_stm32_adc_group_blocks_state_t* s,
    const nx_stm32_adc_group_blocks_config_t* config) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || config == NULL || config->max_service_gap_us == 0U ||
        (unsigned)config->adc.mode > NX_STM32_ADC_TRIPLE_INTERLEAVED ||
        overlap(s, sizeof(*s), config, sizeof(*config)))
        return NX_ERROR_INVALID;
    if (s->config != NULL)
        return NX_ERROR_BUSY;
    s->config = config;
    nx_result_t result = nx_stm32_adc_group_prepare(&s->base, &config->adc);
    if (s->base.config == NULL)
        s->config = NULL;
    return result;
}
nx_result_t
nx_stm32_adc_group_blocks_close(nx_stm32_adc_group_blocks_state_t* s) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (s == NULL)
        return NX_ERROR_INVALID;
    s->base.closing = true;
    if (s->stream != NULL) {
        nx_result_t result = stop(s);
        if (result != NX_SUCCESS)
            return result;
    }
    nx_result_t result = nx_stm32_adc_group_close(&s->base);
    if (result == NX_SUCCESS) {
        s->config = NULL;
        s->observer = NULL;
    }
    return result;
}
static nx_result_t start(void* context, nx_stream_t* stream) {
    return start_selected(context, stream, NULL);
}
static nx_result_t service(void* context) {
    return service_selected(context, NULL);
}
static nx_result_t attach(void* context, nx_adc_observer_t* observer) {
    return attach_selected(context, observer, NULL);
}
static nx_result_t info(const void* context, nx_adc_group_info_t* result) {
    return info_selected(context, result, NULL);
}
static nx_result_t observation(const void* context,
                               const nx_stream_block_t* block,
                               nx_adc_observation_t* result) {
    return observation_selected(context, block, result, NULL);
}
/** \brief Finite storage and ordinary tables do not gain continuous state. */
const nx_adc_group_ops_t nx_stm32_adc_group_blocks_ops = {
    .submit = NULL,
    .cancel = NULL,
    .service = service,
    .stop = stop,
    .info = info,
    .start_blocks = start,
    .attach_observer = attach,
    .block_observation = observation,
};

#ifndef NX_STM32_ADC_INJECTED_ACK
#define NX_STM32_ADC_INJECTED_ACK(adc) LL_ADC_ClearFlag_JEOS(adc)
#endif
static ADC_TypeDef* const s_combined_adcs[3] = {ADC1, ADC2, ADC3};

static unsigned combined_count(const nx_stm32_adc_group_combined_config_t* c) {
    return c->regular.adc.mode == NX_STM32_ADC_TRIPLE_REGULAR ? 3U : 2U;
}
static const nx_stm32_adc_group_combined_config_t*
combined_config(const nx_stm32_adc_group_combined_state_t* s) {
    return s == NULL
               ? NULL
               : (const nx_stm32_adc_group_combined_config_t*)s->regular.config;
}
/** \brief The immutable extra timer keeps a separate exact clock receipt. */
static volatile uint32_t*
injected_gate(const nx_stm32_adc_group_combined_config_t* c) {
    const nx_stm32_adc_trigger_fact_t* f =
        nx_stm32_adc_injected_trigger_fact(c->injected_timer->trigger);
    return f->apb2 ? &c->regular.adc.rcc->APB2ENR
                   : &c->regular.adc.rcc->APB1ENR;
}
static bool
injected_timer_facts(const nx_stm32_adc_group_combined_config_t* c) {
    const nx_stm32_adc_group_injected_timer_t* t = c->injected_timer;
    if (t == NULL)
        return true;
    const nx_stm32_adc_trigger_fact_t* f =
        nx_stm32_adc_injected_trigger_fact(t->trigger);
    const nx_stm32_adc_trigger_fact_t* regular =
        nx_stm32_adc_trigger_fact(c->regular.adc.trigger);
    if (f == NULL || regular == NULL || t->timer == NULL ||
        t->timer == c->regular.adc.timer ||
        f->registers == regular->registers || t->trigger_identity == 0U ||
        t->delay_us == 0U ||
        t->timer_clock_hz != nx_stm32_adc_trigger_clock(f) ||
        (uint64_t)(t->prescaler + 1U) * (t->period + 1U) * 1000000U !=
            (uint64_t)t->delay_us * t->timer_clock_hz)
        return false;
#ifndef NEXUS_STM32_MODEL
    if (t->timer != f->registers)
        return false;
#endif
    return true;
}
static bool injected_timer_idle(const TIM_TypeDef* timer) {
    return (timer->CR1 & TIM_CR1_CEN) == 0U &&
           (timer->SMCR & (TIM_SMCR_SMS | TIM_SMCR_ECE)) == 0U &&
           timer->CCER == 0U && timer->DIER == 0U;
}
static nx_result_t
injected_timer_prepare(nx_stm32_adc_group_combined_state_t* s) {
    const nx_stm32_adc_group_combined_config_t* c = combined_config(s);
    const nx_stm32_adc_group_injected_timer_t* t = c->injected_timer;
    if (t == NULL)
        return NX_SUCCESS;
    const nx_stm32_adc_trigger_fact_t* f =
        nx_stm32_adc_injected_trigger_fact(t->trigger);
    volatile uint32_t* gate = injected_gate(c);
    if ((*gate & f->gate) == 0U) {
        s->regular.base.effects |= INJECTED_TIMER_CLOCK;
        *gate |= f->gate;
        nx_arch_dsb();
        if ((*gate & f->gate) == 0U)
            return NX_ERROR_IO;
    }
    /* Probe clocks may reveal a foreign source. Do not stop or reprogram it. */
    if (!injected_timer_idle(t->timer))
        return NX_ERROR_BUSY;
    s->regular.base.effects |= INJECTED_TIMER_OWNER;
    return NX_SUCCESS;
}
/** \brief Withdraw the one pulse before trusting stable JDR or a joined stop.
 */
static bool
injected_source_quiet(const nx_stm32_adc_group_combined_config_t* c) {
    if (c->injected_timer == NULL)
        return true;
    const nx_stm32_adc_trigger_fact_t* f =
        nx_stm32_adc_injected_trigger_fact(c->injected_timer->trigger);
    if ((*injected_gate(c) & f->gate) == 0U)
        return false;
    LL_TIM_DisableCounter(c->injected_timer->timer);
    for (unsigned i = 0U; i < combined_count(c); ++i)
        c->regular.adc.adc[i]->CR2 &= ~(uint32_t)ADC_CR2_JEXTEN;
    nx_arch_dsb();
    bool quiet = injected_timer_idle(c->injected_timer->timer);
    for (unsigned i = 0U; i < combined_count(c); ++i)
        quiet &= (c->regular.adc.adc[i]->CR2 & ADC_CR2_JEXTEN) == 0U;
    return quiet;
}
static nx_result_t
injected_timer_release(nx_stm32_adc_group_combined_state_t* s) {
    const nx_stm32_adc_group_combined_config_t* c = combined_config(s);
    if (c == NULL || c->injected_timer == NULL)
        return NX_SUCCESS;
    nx_stm32_adc_group_state_t* base = &s->regular.base;
    const nx_stm32_adc_trigger_fact_t* f =
        nx_stm32_adc_injected_trigger_fact(c->injected_timer->trigger);
    volatile uint32_t* gate = injected_gate(c);
    if ((base->effects & INJECTED_TIMER_OWNER) != 0U) {
        if (!injected_source_quiet(c))
            return NX_ERROR_BUSY;
        base->effects &= (uint8_t)~INJECTED_TIMER_OWNER;
    } else if ((*gate & f->gate) != 0U &&
               !injected_timer_idle(c->injected_timer->timer)) {
        return NX_ERROR_BUSY;
    }
    if ((base->effects & INJECTED_TIMER_CLOCK) != 0U) {
        *gate &= ~f->gate;
        nx_arch_dsb();
        if ((*gate & f->gate) != 0U)
            return NX_ERROR_IO;
        base->effects &= (uint8_t)~INJECTED_TIMER_CLOCK;
    }
    return NX_SUCCESS;
}
/** \brief Setup UG precedes JEXTEN; starting the counter is a later commit. */
static bool injected_source_arm(const nx_stm32_adc_group_combined_config_t* c) {
    const nx_stm32_adc_group_injected_timer_t* t = c->injected_timer;
    if (t == NULL)
        return true;
    const nx_stm32_adc_trigger_fact_t* f =
        nx_stm32_adc_injected_trigger_fact(t->trigger);
    nx_stm32_adc_trigger_program(t->timer, f, t->prescaler, t->period);
    t->timer->CR1 |= TIM_CR1_OPM;
    nx_arch_dsb();
    if (t->timer->PSC != t->prescaler || t->timer->ARR != t->period ||
        !injected_timer_idle(t->timer) || (t->timer->CR1 & TIM_CR1_OPM) == 0U)
        return false;
    const uint32_t external =
        ((uint32_t)f->selection << ADC_CR2_JEXTSEL_Pos) | ADC_CR2_JEXTEN_0;
    c->regular.adc.adc[0]->CR2 =
        (c->regular.adc.adc[0]->CR2 &
         ~(uint32_t)(ADC_CR2_JEXTSEL | ADC_CR2_JEXTEN)) |
        external;
    nx_arch_dsb();
    return (c->regular.adc.adc[0]->CR2 & (ADC_CR2_JEXTSEL | ADC_CR2_JEXTEN)) ==
           external;
}

/** \brief Lost extra trigger history cannot be repaired by reopening a gate. */
static bool combined_live(nx_stm32_adc_group_combined_state_t* s) {
    const nx_stm32_adc_group_combined_config_t* c = combined_config(s);
    if (c == NULL)
        return false;
    bool ok = live(&s->regular);
    if (c->injected_timer != NULL) {
        const nx_stm32_adc_trigger_fact_t* f =
            nx_stm32_adc_injected_trigger_fact(c->injected_timer->trigger);
        uint32_t reset = f->apb2 ? c->regular.adc.rcc->APB2RSTR
                                 : c->regular.adc.rcc->APB1RSTR;
        ok = ok && (s->regular.base.effects & INJECTED_TIMER_OWNER) != 0U &&
             (*injected_gate(c) & f->gate) != 0U && (reset & f->gate) == 0U;
    }
    if (!ok && s->regular.stream != NULL)
        s->regular.history_lost = true;
    return ok;
}

/** \brief Static pacing includes the conversion reset by an injection. */
bool nx_stm32_adc_group_combined_facts(
    const nx_stm32_adc_group_combined_config_t* c, bool automatic) {
    if (c == NULL || c->injected_channels == NULL ||
        c->injected_samples == NULL || c->injected_converters == NULL ||
        c->injected_ranks == 0U || c->injected_ranks > 4U ||
        (c->regular.adc.mode != NX_STM32_ADC_DUAL_REGULAR &&
         c->regular.adc.mode != NX_STM32_ADC_TRIPLE_REGULAR) ||
        c->regular.adc.channels == NULL ||
        c->regular.adc.sample_times == NULL || c->regular.adc.ranks == 0U ||
        c->regular.adc.ranks > 16U || c->regular.adc.resolution > 3U ||
        c->regular.adc.period_us > UINT32_MAX - 2U ||
        (automatic
             ? (c->injected_timer != NULL ||
                c->min_injected_interval_us != 0U ||
                c->regular.max_service_gap_us != 0U)
             : c->min_injected_interval_us < c->regular.adc.period_us + 2U) ||
        !injected_timer_facts(c))
        return false;
    unsigned count = combined_count(c);
    unsigned conversion = 12U - 2U * c->regular.adc.resolution;
    uint32_t regular_cycles = 0U, injected_cycles = 0U, maximum_rank = 0U;
    for (unsigned rank = 0U; rank < c->regular.adc.ranks; ++rank) {
        unsigned code = c->regular.adc.sample_times[(size_t)rank * count];
        if (code > 7U)
            return false;
        uint32_t cost = s_sample_cycles[code] + conversion;
        regular_cycles += cost;
        if (cost > maximum_rank)
            maximum_rank = cost;
    }
    for (unsigned rank = 0U; rank < c->injected_ranks; ++rank) {
        for (unsigned i = 0U; i < count; ++i) {
            unsigned index = rank * count + i;
            unsigned channel = c->injected_channels[index];
            unsigned sample = c->injected_samples[index];
            if (channel > 15U || sample > 7U ||
                c->injected_converters[index] != i + 1U ||
                sample != c->injected_samples[(size_t)rank * count])
                return false;
            nx_stm32_adc_pad_t pad, other;
            if (!nx_stm32_adc_pad(s_combined_adcs[i], (uint8_t)channel, &pad))
                return false;
            for (unsigned prior = 0U; prior < i; ++prior) {
                if (!nx_stm32_adc_pad(
                        s_combined_adcs[prior],
                        c->injected_channels[rank * count + prior], &other) ||
                    (pad.gpio == other.gpio && pad.pin == other.pin))
                    return false;
            }
            for (unsigned prior = 0U; prior < c->regular.adc.ranks; ++prior)
                if (c->regular.adc.channels[prior * count + i] == channel &&
                    c->regular.adc.sample_times[prior * count + i] != sample)
                    return false;
            for (unsigned prior = 0U; prior < rank; ++prior)
                if (c->injected_channels[prior * count + i] == channel &&
                    c->injected_samples[prior * count + i] != sample)
                    return false;
        }
        injected_cycles +=
            s_sample_cycles[c->injected_samples[(size_t)rank * count]] +
            conversion;
    }
    /* A one-tick observation can underestimate real spacing by one tick. Two
     * tick phases and one reset regular conversion are reserved explicitly. */
    return automatic ||
           (uint64_t)c->regular.adc.period_us * c->regular.adc.adc_clock_hz >
               (uint64_t)(regular_cycles + injected_cycles + maximum_rank +
                          2U) *
                   1000000U;
}
nx_result_t nx_stm32_adc_group_injected_pads(
    const nx_stm32_adc_group_combined_config_t* c) {
    for (unsigned rank = 0U; rank < c->injected_ranks; ++rank)
        for (unsigned i = 0U; i < combined_count(c); ++i) {
            uint8_t channel =
                c->injected_channels[rank * combined_count(c) + i];
            nx_result_t result = nx_stm32_adc_pin(s_combined_adcs[i], channel);
            if (result != NX_SUCCESS)
                return result;
        }
    nx_arch_dsb();
    for (unsigned rank = 0U; rank < c->injected_ranks; ++rank)
        for (unsigned i = 0U; i < combined_count(c); ++i) {
            nx_stm32_adc_pad_t pad;
            uint8_t channel =
                c->injected_channels[rank * combined_count(c) + i];
            if (!nx_stm32_adc_pad(s_combined_adcs[i], channel, &pad) ||
                (pad.gpio->MODER & (3U << ((uint32_t)pad.pin * 2U))) !=
                    (3U << ((uint32_t)pad.pin * 2U)) ||
                (pad.gpio->PUPDR & (3U << ((uint32_t)pad.pin * 2U))) != 0U)
                return NX_ERROR_IO;
        }
    return NX_SUCCESS;
}
nx_result_t nx_stm32_adc_group_combined_prepare(
    nx_stm32_adc_group_combined_state_t* s,
    const nx_stm32_adc_group_combined_config_t* c) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !nx_stm32_adc_group_combined_facts(c, false) ||
        overlap(s, sizeof(*s), c, sizeof(*c)))
        return NX_ERROR_INVALID;
    if (c->injected_timer != NULL) {
        const nx_stm32_adc_trigger_fact_t* f =
            nx_stm32_adc_injected_trigger_fact(c->injected_timer->trigger);
        if (overlap(s, sizeof(*s), c->injected_timer,
                    sizeof(*c->injected_timer)) ||
            overlap(c->injected_timer, sizeof(*c->injected_timer),
                    &g_nx_stm32_system, sizeof(g_nx_stm32_system)))
            return NX_ERROR_INVALID;
        if ((*injected_gate(c) & f->gate) != 0U &&
            !injected_timer_idle(c->injected_timer->timer))
            return NX_ERROR_BUSY;
    }
    size_t samples = (size_t)combined_count(c) * c->injected_ranks;
    size_t regular = (size_t)combined_count(c) * c->regular.adc.ranks;
    if (c->regular.adc.region_count >
            SIZE_MAX / sizeof(*c->regular.adc.regions) ||
        overlap(s, sizeof(*s), c->regular.adc.channels, regular) ||
        overlap(s, sizeof(*s), c->regular.adc.sample_times, regular) ||
        overlap(s, sizeof(*s), c->regular.adc.converters, regular) ||
        overlap(s, sizeof(*s), c->regular.adc.regions,
                c->regular.adc.region_count *
                    sizeof(*c->regular.adc.regions)) ||
        overlap(s, sizeof(*s), c->injected_channels, samples) ||
        overlap(s, sizeof(*s), c->injected_samples, samples) ||
        overlap(s, sizeof(*s), c->injected_converters, samples))
        return NX_ERROR_INVALID;
    nx_result_t result =
        nx_stm32_adc_group_blocks_prepare(&s->regular, &c->regular);
    if (result != NX_SUCCESS)
        return result;
    result = nx_stm32_adc_group_injected_pads(c);
    if (result != NX_SUCCESS)
        return result;
    s->last_injected = 0U;
    s->injected_result = NX_SUCCESS;
    s->injected_before = false;
    return injected_timer_prepare(s);
}
/** \brief Mask each completed JEOC publisher without stopping regular DMA. */
void nx_stm32_adc_group_combined_irq(const nx_adc_group_port_t* port) {
    nx_stm32_adc_group_combined_state_t* s =
        port == NULL ? NULL : port->context;
    const nx_stm32_adc_group_combined_config_t* c = combined_config(s);
    if (c == NULL || !s->regular.base.initialized ||
        s->regular.stream == NULL ||
        s->regular.base.phase != NX_ADC_GROUP_ACQUIRING)
        return;
    if (!combined_live(s)) {
        halt(&s->regular, NX_ERROR_STATE);
        return;
    }
    uint32_t events = 0U;
    for (unsigned i = 0U; i < combined_count(c); ++i) {
        if ((c->regular.adc.adc[i]->SR & ADC_SR_JEOC) != 0U) {
            events |= 1U << i;
            c->regular.adc.adc[i]->CR1 &= ~(uint32_t)ADC_CR1_JEOCIE;
        }
    }
    if (events != 0U && s->regular.base.active == NULL)
        events |= NX_ADC_GROUP_ERROR;
    __atomic_fetch_or(&s->regular.base.events, events, __ATOMIC_RELAXED);
    nx_stm32_adc_group_blocks_irq(port);
}
static void injection_reason(nx_stm32_adc_group_combined_state_t* s,
                             nx_result_t reason) {
    if (s->injected_result == NX_SUCCESS)
        s->injected_result = reason;
    if (s->regular.base.active != NULL)
        (void)nx_request_transition(&s->regular.base.active->base,
                                    NX_REQUEST_DRAINING);
}
/** \brief No IRQ carries a descriptor reference after this guarded detach. */
static void injection_finish(nx_stm32_adc_group_combined_state_t* s,
                             nx_result_t result) {
    nx_adc_group_request_t* request = s->regular.base.active;
    s->regular.base.active = NULL;
    s->regular.base.events &= ~UINT32_C(7);
    s->injected_result = NX_SUCCESS;
    nx_request_settle(&request->base, result, result == NX_SUCCESS ? 1U : 0U);
}
static bool injection_quiet(const nx_stm32_adc_group_combined_config_t* c) {
    bool quiet = true;
    for (unsigned i = 0U; i < combined_count(c); ++i) {
        ADC_TypeDef* adc = c->regular.adc.adc[i];
        adc->CR1 &= ~(uint32_t)ADC_CR1_JEOCIE;
        NX_STM32_ADC_INJECTED_ACK(adc);
        nx_arch_dsb();
        quiet &= (adc->CR1 & ADC_CR1_JEOCIE) == 0U &&
                 (adc->SR & ADC_SR_JEOC) == 0U &&
                 (adc->CR2 & (ADC_CR2_JEXTEN | ADC_CR2_JSWSTART)) == 0U;
    }
    return quiet;
}
/** \brief Complete one JDR scan without consuming a regular DMA target. */
static void injection_progress(nx_stm32_adc_group_combined_state_t* s,
                               nx_time_us_t observed) {
    nx_stm32_adc_group_blocks_state_t* b = &s->regular;
    const nx_stm32_adc_group_combined_config_t* c = combined_config(s);
    if (b->base.active == NULL || b->base.phase != NX_ADC_GROUP_ACQUIRING)
        return;
    if (!combined_live(s)) {
        injection_reason(s, NX_ERROR_STATE);
        halt(b, NX_ERROR_STATE);
        return;
    }
    if ((b->base.events & NX_ADC_GROUP_ERROR) != 0U) {
        injection_reason(s, NX_ERROR_IO);
        halt(b, NX_ERROR_IO);
        return;
    }
    if (observed == NX_DEADLINE_NEVER ||
        (s->injected_before && observed < s->last_injected)) {
        injection_reason(s, NX_ERROR_STATE);
        halt(b, NX_ERROR_STATE);
        return;
    }
    const uint32_t complete = (1U << combined_count(c)) - 1U;
    if ((b->base.events & complete) != complete) {
        if (nx_deadline_expired(b->base.active->base.deadline, observed)) {
            injection_reason(s, NX_ERROR_TIMEOUT);
            halt(b, s->injected_result);
        }
        return;
    }
    if (!injected_source_quiet(c)) {
        injection_reason(s, NX_ERROR_IO);
        halt(b, NX_ERROR_IO);
        return;
    }
    static const uint32_t ranks[4] = {LL_ADC_INJ_RANK_1, LL_ADC_INJ_RANK_2,
                                      LL_ADC_INJ_RANK_3, LL_ADC_INJ_RANK_4};
    uint16_t values[12];
    for (unsigned rank = 0U; rank < c->injected_ranks; ++rank)
        for (unsigned i = 0U; i < combined_count(c); ++i)
            values[rank * combined_count(c) + i] =
                LL_ADC_INJ_ReadConversionData12(c->regular.adc.adc[i],
                                                ranks[rank]);
    nx_arch_dsb();
    if (!combined_live(s)) {
        injection_reason(s, NX_ERROR_STATE);
        halt(b, NX_ERROR_STATE);
        return;
    }
    if (!injection_quiet(c) || (nx_stm32_adc_group_observed(&c->regular.adc) &
                                NX_ADC_GROUP_ERROR) != 0U) {
        injection_reason(s, NX_ERROR_IO);
        halt(b, NX_ERROR_IO);
        return;
    }
    nx_time_us_t now = nx_time_now_us();
    if (now == NX_DEADLINE_NEVER || now < observed || !combined_live(s)) {
        injection_reason(s, NX_ERROR_STATE);
        halt(b, NX_ERROR_STATE);
        return;
    }
    if (nx_deadline_expired(b->base.active->base.deadline, now))
        injection_reason(s, NX_ERROR_TIMEOUT);
    nx_result_t result = s->injected_result;
    if (result == NX_SUCCESS)
        memcpy(b->base.active->output.frames, values,
               (size_t)combined_count(c) * c->injected_ranks *
                   sizeof(uint16_t));
    injection_finish(s, result);
}
static nx_result_t combined_submit(void* context,
                                   nx_adc_group_request_t* request) {
    nx_stm32_adc_group_combined_state_t* s = context;
    const nx_stm32_adc_group_combined_config_t* c = combined_config(s);
    if (!task())
        return NX_ERROR_CONTEXT;
    if (c == NULL || !s->regular.base.initialized || s->regular.base.closing ||
        s->regular.stream == NULL || s->regular.stopping ||
        s->regular.base.phase != NX_ADC_GROUP_ACQUIRING ||
        s->regular.base.fault || !combined_live(s))
        return NX_ERROR_STATE;
    nx_stm32_adc_group_blocks_state_t* b = &s->regular;
    if (b->base.active != NULL)
        return NX_ERROR_BUSY;
    const size_t bytes = (size_t)combined_count(c) * c->injected_ranks * 2U;
    if (request == NULL ||
        !output(b, request, sizeof(*request), _Alignof(nx_adc_group_request_t),
                c) ||
        request->results_kind != NX_ADC_GROUP_RAW_RESULTS ||
        request->frame_count != 1U || request->bytes != bytes ||
        request->base.deadline == NX_DEADLINE_NEVER ||
        !output(b, request->output.frames, request->bytes, _Alignof(uint16_t),
                c) ||
        overlap(request, sizeof(*request), request->output.frames,
                request->bytes))
        return NX_ERROR_INVALID;
    nx_time_us_t now = nx_time_now_us();
    if (now == NX_DEADLINE_NEVER ||
        (s->injected_before && now < s->last_injected))
        return NX_ERROR_STATE;
    if (nx_deadline_expired(request->base.deadline, now))
        return NX_ERROR_TIMEOUT;
    if (s->injected_before &&
        now - s->last_injected < c->min_injected_interval_us)
        return NX_ERROR_BUSY;
    if (c->injected_timer != NULL) {
        const nx_stm32_adc_trigger_fact_t* f =
            nx_stm32_adc_injected_trigger_fact(c->injected_timer->trigger);
        if ((b->base.effects & INJECTED_TIMER_OWNER) == 0U ||
            (*injected_gate(c) & f->gate) == 0U ||
            !injected_timer_idle(c->injected_timer->timer))
            return NX_ERROR_STATE;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    uint32_t expected = combined_count(c) == 3U
                            ? LL_ADC_MULTI_TRIPLE_REG_SIM_INJ_SIM
                            : LL_ADC_MULTI_DUAL_REG_SIM_INJ_SIM;
    bool ready = (c->regular.adc.common->CCR & ADC_CCR_MULTI) == expected;
    for (unsigned i = 0U; i < combined_count(c); ++i) {
        ADC_TypeDef* adc = c->regular.adc.adc[i];
        ready &=
            (adc->CR1 & (ADC_CR1_JAUTO | ADC_CR1_JDISCEN | ADC_CR1_JEOCIE)) ==
                0U &&
            (adc->CR2 & (ADC_CR2_JEXTEN | ADC_CR2_JSWSTART | ADC_CR2_ADON)) ==
                ADC_CR2_ADON &&
            (adc->SR & (ADC_SR_JEOC | ADC_SR_OVR)) == 0U;
    }
    nx_result_t result =
        ready ? nx_request_admit(&request->base, NX_REQUEST_ACTIVE)
              : NX_ERROR_STATE;
    if (result == NX_SUCCESS) {
        b->base.active = request;
        b->base.events &= ~UINT32_C(7);
        s->injected_result = NX_SUCCESS;
        for (unsigned i = 0U; i < combined_count(c); ++i)
            c->regular.adc.adc[i]->CR1 |= ADC_CR1_JEOCIE;
        nx_arch_dsb();
        bool enabled = combined_live(s) && injected_source_arm(c);
        for (unsigned i = 0U; i < combined_count(c); ++i)
            enabled &= (c->regular.adc.adc[i]->CR1 & ADC_CR1_JEOCIE) != 0U;
        /* Admission precedes this MMIO/readback phase. A fresh observation
         * guards the command; accepted storage remains borrowed on failure. */
        nx_time_us_t armed = nx_time_now_us();
        nx_result_t failure =
            !enabled                                    ? NX_ERROR_IO
            : armed == NX_DEADLINE_NEVER || armed < now ? NX_ERROR_STATE
            : nx_deadline_expired(request->base.deadline, armed)
                ? NX_ERROR_TIMEOUT
                : NX_SUCCESS;
        if (failure == NX_SUCCESS) {
            if (c->injected_timer == NULL) {
                LL_ADC_INJ_StartConversionSWStart(c->regular.adc.adc[0]);
            } else {
                LL_TIM_EnableCounter(c->injected_timer->timer);
            }
            nx_arch_dsb();
            if (c->injected_timer != NULL &&
                (c->injected_timer->timer->CR1 & TIM_CR1_CEN) == 0U &&
                (c->injected_timer->timer->SR & TIM_SR_UIF) == 0U)
                failure = NX_ERROR_IO;
            s->last_injected = nx_time_now_us();
            s->injected_before = true;
            if (s->last_injected == NX_DEADLINE_NEVER ||
                s->last_injected < armed)
                failure = NX_ERROR_STATE;
            else if (nx_deadline_expired(request->base.deadline,
                                         s->last_injected))
                failure = NX_ERROR_TIMEOUT;
        }
        if (failure != NX_SUCCESS) {
            injection_reason(s, failure);
            halt(b, failure);
        }
    }
    nx_arch_irq_restore(saved);
    return result;
}
static nx_result_t combined_cancel(void* context,
                                   nx_adc_group_request_t* request) {
    nx_stm32_adc_group_combined_state_t* s = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || s->regular.base.active != request || request == NULL)
        return NX_ERROR_STATE;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    injection_reason(s, NX_ERROR_CANCELLED);
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}
/** \brief Joint failure never releases injection while DMA withdrawal refuses.
 */
static void injection_join(nx_stm32_adc_group_combined_state_t* s) {
    nx_stm32_adc_group_blocks_state_t* b = &s->regular;
    if (b->base.active == NULL || !combined_live(s) ||
        (b->base.phase != NX_ADC_GROUP_DRAINING && !b->stopping))
        return;
    if (!injected_source_quiet(combined_config(s)) ||
        !nx_stm32_adc_group_withdraw(&b->base)) {
        if (nx_deadline_expired(b->base.drain_deadline, nx_time_now_us()))
            (void)nx_request_transition(&b->base.active->base,
                                        NX_REQUEST_QUARANTINED);
        return;
    }
    nx_result_t result = s->injected_result;
    if (result == NX_SUCCESS)
        result = b->base.fault ? b->base.terminal : NX_ERROR_CANCELLED;
    injection_finish(s, result);
}
static nx_result_t combined_service(void* context) {
    nx_stm32_adc_group_combined_state_t* s = context;
    const nx_stm32_adc_group_combined_config_t* c = combined_config(s);
    if (!task())
        return NX_ERROR_CONTEXT;
    if (c == NULL || s->regular.stream == NULL || s->regular.stopping)
        return NX_ERROR_STATE;
    if (!combined_live(s)) {
        injection_reason(s, NX_ERROR_STATE);
        halt(&s->regular, NX_ERROR_STATE);
        return NX_ERROR_STATE;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_stm32_adc_group_combined_irq(&(nx_adc_group_port_t){
        .ops = &nx_stm32_adc_group_combined_ops, .context = s});
    injection_progress(s, nx_time_now_us());
    nx_arch_irq_restore(saved);
    nx_result_t result = service_selected(&s->regular, c);
    saved = nx_arch_irq_save();
    injection_join(s);
    nx_arch_irq_restore(saved);
    return result;
}
static nx_result_t combined_stop(void* context) {
    nx_stm32_adc_group_combined_state_t* s = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->regular.base.initialized)
        return NX_ERROR_STATE;
    if (!combined_live(s)) {
        injection_reason(s, NX_ERROR_STATE);
        halt(&s->regular, NX_ERROR_STATE);
        return NX_ERROR_STATE;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (!injected_source_quiet(combined_config(s))) {
        injection_reason(s, NX_ERROR_IO);
        halt(&s->regular, NX_ERROR_IO);
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    if (s->regular.base.active != NULL)
        injection_reason(s, s->regular.base.fault ? s->regular.base.terminal
                                                  : NX_ERROR_CANCELLED);
    nx_arch_irq_restore(saved);
    nx_result_t result = stop(&s->regular);
    saved = nx_arch_irq_save();
    if (result == NX_SUCCESS && s->regular.base.active != NULL)
        injection_finish(s, s->injected_result);
    else
        injection_join(s);
    nx_arch_irq_restore(saved);
    return result;
}
nx_result_t
nx_stm32_adc_group_combined_close(nx_stm32_adc_group_combined_state_t* s) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (s == NULL)
        return NX_ERROR_INVALID;
    s->regular.base.closing = true;
    if (s->regular.stream != NULL || s->regular.base.active != NULL) {
        nx_result_t result = combined_stop(s);
        if (result != NX_SUCCESS)
            return result;
    }
    nx_result_t released = injected_timer_release(s);
    return released == NX_SUCCESS ? nx_stm32_adc_group_blocks_close(&s->regular)
                                  : released;
}
static nx_result_t combined_start(void* context, nx_stream_t* stream) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (context == NULL || !combined_live(context))
        return NX_ERROR_STATE;
    return start_selected(context, stream, combined_config(context));
}
static nx_result_t combined_attach(void* context, nx_adc_observer_t* observer) {
    return attach_selected(context, observer, combined_config(context));
}
static nx_result_t combined_info(const void* context,
                                 nx_adc_group_info_t* result) {
    return info_selected(context, result, combined_config(context));
}
static nx_result_t combined_observation(const void* context,
                                        const nx_stream_block_t* block,
                                        nx_adc_observation_t* result) {
    return observation_selected(context, block, result,
                                combined_config(context));
}
static nx_result_t injected_info(const void* context,
                                 nx_adc_group_info_t* result) {
    const nx_stm32_adc_group_combined_state_t* s = context;
    const nx_stm32_adc_group_combined_config_t* c = combined_config(s);
    if (!task())
        return NX_ERROR_CONTEXT;
    if (c == NULL || !s->regular.base.initialized)
        return NX_ERROR_STATE;
    if (!output(&s->regular, result, sizeof(*result),
                _Alignof(nx_adc_group_info_t), c))
        return NX_ERROR_INVALID;
    nx_adc_group_info_t value;
    nx_result_t status =
        nx_stm32_adc_group_info_snapshot(&s->regular.base, &value);
    if (status != NX_SUCCESS)
        return status;
    const unsigned samples = combined_count(c) * c->injected_ranks;
    value.format.scan_stride = (size_t)samples * 2U;
    value.format.sample_stride = 2U;
    value.format.channel_count = (uint16_t)samples;
    value.format.group = NX_ADC_INJECTED;
    value.format.trigger = c->injected_timer == NULL ? NX_ADC_TRIGGER_SOFTWARE
                                                     : NX_ADC_TRIGGER_TIMER;
    value.format.trigger_identity =
        c->injected_timer == NULL ? 0U : c->injected_timer->trigger_identity;
    value.channels = c->injected_channels;
    value.sample_times = c->injected_samples;
    value.converters = c->injected_converters;
    value.physical_scan_halfcycles = 0U;
    for (unsigned rank = 0U; rank < c->injected_ranks; ++rank)
        value.physical_scan_halfcycles +=
            2U * (s_sample_cycles[c->injected_samples[(size_t)rank *
                                                      combined_count(c)]] +
                  12U - 2U * c->regular.adc.resolution);
    value.timer_period_us =
        c->injected_timer == NULL ? 0U : c->injected_timer->delay_us;
    value.max_frames = 1U;
    value.physical_ranks = (uint16_t)samples;
    value.buffer_alignment = 2U;
    *result = value;
    return NX_SUCCESS;
}
/** \brief Combined faces retain only their optional extra pacing state. */
const nx_adc_group_ops_t nx_stm32_adc_group_combined_ops = {
    .submit = NULL,
    .cancel = combined_cancel,
    .service = combined_service,
    .stop = combined_stop,
    .info = combined_info,
    .start_blocks = combined_start,
    .attach_observer = combined_attach,
    .block_observation = combined_observation,
    .submit_injected = combined_submit,
    .injected_info = injected_info,
};
