/**
 * \file            adc_group_automatic.c
 * \brief           One finite regular/automatic injected joint acquisition
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_adc_group_automatic.h"
#include "stm32f407_adc_group_internal.h"
#include "stm32f407_clock.h"
#include "stm32f4_adc_trigger.h"
#include "stm32f4xx_ll_adc.h"
#include <string.h>

static bool task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static unsigned count(const nx_stm32_adc_group_automatic_state_t* s) {
    return s->config->regular.adc.mode == NX_STM32_ADC_TRIPLE_REGULAR ? 3U : 2U;
}
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return a == NULL || b == NULL || an == 0U || bn == 0U ||
           an > UINTPTR_MAX - (uintptr_t)a || bn > UINTPTR_MAX - (uintptr_t)b ||
           ((uintptr_t)a < (uintptr_t)b + bn &&
            (uintptr_t)b < (uintptr_t)a + an);
}
/** \brief Both outputs exclude every cold fact and the complete selected state.
 */
static bool independent(const nx_stm32_adc_group_automatic_state_t* s,
                        const void* p, size_t bytes, size_t alignment) {
    const nx_stm32_adc_group_combined_config_t* c = s->config;
    size_t ranks = (size_t)count(s) * c->injected_ranks;
    return nx_stm32_adc_group_independent(&s->base, sizeof(*s), p, bytes,
                                          alignment) &&
           !overlap(p, bytes, c, sizeof(*c)) &&
           !overlap(p, bytes, c->injected_channels, ranks) &&
           !overlap(p, bytes, c->injected_samples, ranks) &&
           !overlap(p, bytes, c->injected_converters, ranks);
}
/** \brief Numerical gate restoration never repairs a lost active history. */
static bool live(nx_stm32_adc_group_automatic_state_t* s) {
    const nx_stm32_adc_group_config_t* c = s->base.config;
    const nx_stm32_adc_trigger_fact_t* f =
        nx_stm32_adc_trigger_fact(c->trigger);
    uint32_t gates =
        RCC_APB2ENR_ADC1EN | RCC_APB2ENR_ADC2EN | RCC_APB2ENR_ADC3EN;
    bool ok = !s->history_lost && nx_stm32_adc_group_epoch_live(&s->base) &&
              f != NULL && (c->rcc->APB2ENR & gates) == gates &&
              (c->rcc->APB2RSTR & RCC_APB2RSTR_ADCRST) == 0U &&
              (c->rcc->AHB1ENR & RCC_AHB1ENR_DMA2EN) != 0U &&
              (c->rcc->AHB1RSTR & RCC_AHB1RSTR_DMA2RST) == 0U &&
              ((f->apb2 ? c->rcc->APB2ENR : c->rcc->APB1ENR) & f->gate) != 0U &&
              ((f->apb2 ? c->rcc->APB2RSTR : c->rcc->APB1RSTR) & f->gate) == 0U;
    if (!ok && s->base.active != NULL)
        s->history_lost = true;
    return ok;
}
static void drain(nx_stm32_adc_group_automatic_state_t* s, nx_result_t reason) {
    nx_stm32_adc_group_state_t* b = &s->base;
    if (b->phase == NX_ADC_GROUP_DRAINING)
        return;
    b->terminal = reason;
    b->fault |= reason == NX_ERROR_IO;
    b->phase = NX_ADC_GROUP_DRAINING;
    nx_time_us_t now = nx_time_now_us();
    if (nx_deadline_finite_after(now, b->config->drain_timeout_us,
                                 &b->drain_deadline) != NX_SUCCESS)
        b->drain_deadline = now == NX_DEADLINE_NEVER ? now - 1U : now;
    (void)nx_request_transition(&b->active->base, NX_REQUEST_DRAINING);
}
/** \brief TC and all JEOC are independent proofs; neither alone gates ADC. */
void nx_stm32_adc_group_automatic_irq(const nx_adc_group_port_t* port) {
    nx_stm32_adc_group_automatic_state_t* s =
        port == NULL ? NULL : port->context;
    if (s == NULL || s->config == NULL || s->base.active == NULL ||
        s->base.phase != NX_ADC_GROUP_ACQUIRING)
        return;
    nx_stm32_adc_group_state_t* b = &s->base;
    const nx_stm32_adc_group_config_t* c = b->config;
    b->events |= nx_stm32_adc_group_observed(c);
    if (!live(s))
        b->events |= NX_ADC_GROUP_ERROR;
    if ((b->events & NX_ADC_GROUP_TRANSFER) != 0U)
        c->memory->CR &= ~(uint32_t)DMA_SxCR_TCIE;
    for (unsigned i = 0U; i < count(s); ++i)
        if ((b->events & (1U << i)) != 0U)
            c->adc[i]->CR1 &= ~(uint32_t)ADC_CR1_JEOCIE;
    uint32_t completed = ((1U << count(s)) - 1U) | NX_ADC_GROUP_TRANSFER;
    if ((b->events & NX_ADC_GROUP_ERROR) != 0U ||
        (b->events & completed) == completed) {
        /* Silence owned level publishers so Thread can perform actual drain.
         * Masking them never means the RAM engine has withdrawn. */
        c->memory->CR &=
            ~(uint32_t)(DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE);
        c->memory->FCR &= ~(uint32_t)DMA_SxFCR_FEIE;
        nx_stm32_adc_group_gate(c);
    }
}
static bool separate_results(const nx_adc_group_request_t* r) {
    const nx_adc_group_results_t* d = r->output.results;
    return !overlap(d, sizeof(*d), r, sizeof(*r)) &&
           !overlap(d->regular.data, d->regular.bytes, r, sizeof(*r)) &&
           !overlap(d->injected.data, d->injected.bytes, r, sizeof(*r)) &&
           !overlap(d->regular.data, d->regular.bytes, d, sizeof(*d)) &&
           !overlap(d->injected.data, d->injected.bytes, d, sizeof(*d)) &&
           !overlap(d->regular.data, d->regular.bytes, d->injected.data,
                    d->injected.bytes);
}
static nx_result_t submit(void* context, nx_adc_group_request_t* request) {
    nx_stm32_adc_group_automatic_state_t* s = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || s->config == NULL || !s->base.initialized ||
        s->base.closing || s->base.fault || !live(s))
        return NX_ERROR_STATE;
    if (s->base.active != NULL)
        return NX_ERROR_BUSY;
    size_t regular = (size_t)count(s) * s->base.config->ranks * 2U;
    size_t injected = (size_t)count(s) * s->config->injected_ranks * 2U;
    size_t width = count(s) == 3U ? 2U : 4U;
    if (!independent(s, request, sizeof(*request),
                     _Alignof(nx_adc_group_request_t)) ||
        request->results_kind != NX_ADC_GROUP_REGULAR_INJECTED_RESULTS ||
        request->bytes != sizeof(*request->output.results) ||
        request->frame_count != 1U ||
        request->base.deadline == NX_DEADLINE_NEVER ||
        !independent(s, request->output.results,
                     sizeof(*request->output.results),
                     _Alignof(nx_adc_group_results_t)) ||
        request->output.results->regular.bytes != regular ||
        request->output.results->injected.bytes != injected ||
        !independent(s, request->output.results->regular.data, regular,
                     width) ||
        !independent(s, request->output.results->injected.data, injected,
                     _Alignof(uint16_t)) ||
        !separate_results(request) ||
        (uintptr_t)request->output.results->regular.data > UINT32_MAX ||
        regular > UINT32_MAX - (uintptr_t)request->output.results->regular.data)
        return NX_ERROR_INVALID;
    nx_result_t result = nx_dma_buffer_validate(
        s->base.config->regions, s->base.config->region_count,
        request->output.results->regular.data, regular, width,
        NX_DMA_FROM_DEVICE);
    if (result != NX_SUCCESS)
        return result;
    nx_time_us_t now = nx_time_now_us(), warm;
    if (now == NX_DEADLINE_NEVER)
        return NX_ERROR_STATE;
    if (nx_deadline_expired(request->base.deadline, now))
        return NX_ERROR_TIMEOUT;
    result = nx_deadline_finite_after(now, 4U, &warm);
    if (result != NX_SUCCESS)
        return result;
    result = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS)
        return result;
    s->base.active = request;
    s->base.phase = NX_ADC_GROUP_WARM;
    s->base.terminal = NX_SUCCESS;
    s->base.events = 0U;
    result = nx_stm32_adc_group_prime_automatic(&s->base, s->config);
    if (result != NX_SUCCESS)
        drain(s, result);
    return NX_SUCCESS;
}
static nx_result_t service(void* context) {
    nx_stm32_adc_group_automatic_state_t* s = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || s->config == NULL || !s->base.initialized)
        return NX_ERROR_STATE;
    nx_stm32_adc_group_state_t* b = &s->base;
    if (b->active == NULL)
        return NX_SUCCESS;
    bool domain = live(s);
    nx_time_us_t now = nx_time_now_us();
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    b->events |= nx_stm32_adc_group_observed(b->config);
    if (b->phase != NX_ADC_GROUP_DRAINING) {
        if (!domain || now == NX_DEADLINE_NEVER)
            drain(s, NX_ERROR_STATE);
        else if ((b->events & NX_ADC_GROUP_ERROR) != 0U)
            drain(s, NX_ERROR_IO);
        else if (nx_deadline_expired(b->active->base.deadline, now))
            drain(s, NX_ERROR_TIMEOUT);
        else if (b->phase == NX_ADC_GROUP_WARM && now >= b->warm_until) {
            nx_result_t result = nx_stm32_adc_group_arm_result(
                b, b->active->output.results->regular.data,
                b->active->output.results->regular.bytes, true);
            if (result == NX_SUCCESS)
                b->phase = NX_ADC_GROUP_ACQUIRING;
            else
                drain(s, result);
        } else if (b->phase == NX_ADC_GROUP_ACQUIRING) {
            uint32_t completed =
                ((1U << count(s)) - 1U) | NX_ADC_GROUP_TRANSFER;
            if ((b->events & completed) == completed)
                drain(s,
                      b->config->memory->NDTR == 0U ? NX_SUCCESS : NX_ERROR_IO);
        }
    }
    nx_arch_irq_restore(saved);
    if (b->phase != NX_ADC_GROUP_DRAINING)
        return NX_SUCCESS;
    if (!domain || !nx_stm32_adc_group_withdraw(b)) {
        if (!domain ||
            nx_deadline_expired(b->drain_deadline, nx_time_now_us())) {
            b->fault = true;
            (void)nx_request_transition(&b->active->base,
                                        NX_REQUEST_QUARANTINED);
            return domain ? NX_ERROR_IO : NX_ERROR_STATE;
        }
        return NX_ERROR_BUSY;
    }
    nx_result_t result = b->terminal;
    if (result == NX_SUCCESS && (b->events & NX_ADC_GROUP_ERROR) != 0U)
        result = NX_ERROR_IO;
    uint16_t samples[12];
    if (result == NX_SUCCESS) {
        static const uint32_t ranks[] = {LL_ADC_INJ_RANK_1, LL_ADC_INJ_RANK_2,
                                         LL_ADC_INJ_RANK_3, LL_ADC_INJ_RANK_4};
        for (unsigned rank = 0U; rank < s->config->injected_ranks; ++rank)
            for (unsigned i = 0U; i < count(s); ++i)
                samples[rank * count(s) + i] = LL_ADC_INJ_ReadConversionData12(
                    b->config->adc[i], ranks[rank]);
        b->events |= nx_stm32_adc_group_observed(b->config);
        nx_time_us_t final = nx_time_now_us();
        if (!live(s) || final == NX_DEADLINE_NEVER || final < now)
            result = NX_ERROR_STATE;
        else if ((b->events & NX_ADC_GROUP_ERROR) != 0U)
            result = NX_ERROR_IO;
        else if (nx_deadline_expired(b->active->base.deadline, final))
            result = NX_ERROR_TIMEOUT;
        if (result == NX_SUCCESS)
            memcpy(b->active->output.results->injected.data, samples,
                   b->active->output.results->injected.bytes);
    }
    b->fault |= result == NX_ERROR_IO;
    nx_adc_group_request_t* request = b->active;
    b->active = NULL;
    b->phase = NX_ADC_GROUP_IDLE;
    nx_request_settle(&request->base, result, result == NX_SUCCESS ? 1U : 0U);
    return NX_SUCCESS;
}
static nx_result_t cancel(void* context, nx_adc_group_request_t* request) {
    nx_stm32_adc_group_automatic_state_t* s = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->base.initialized || request == NULL ||
        s->base.active != request)
        return NX_ERROR_STATE;
    drain(s, NX_ERROR_CANCELLED);
    return NX_SUCCESS;
}
static nx_result_t stop(void* context) {
    nx_stm32_adc_group_automatic_state_t* s = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->base.initialized)
        return NX_ERROR_STATE;
    if (s->base.active != NULL) {
        drain(s, NX_ERROR_CANCELLED);
        nx_result_t result = service(s);
        if (result != NX_SUCCESS)
            return result;
    }
    if (s->history_lost)
        return NX_ERROR_STATE;
    s->base.fault = false;
    return NX_SUCCESS;
}
/** \brief Query output cannot overwrite either part of an active joint loan. */
static nx_result_t info_selected(const void* context,
                                 nx_adc_group_info_t* output, bool injected) {
    const nx_stm32_adc_group_automatic_state_t* s = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || s->config == NULL || !s->base.initialized)
        return NX_ERROR_STATE;
    if (!independent(s, output, sizeof(*output), _Alignof(nx_adc_group_info_t)))
        return NX_ERROR_INVALID;
    if (s->base.active != NULL) {
        const nx_adc_group_request_t* r = s->base.active;
        if (overlap(output, sizeof(*output), r, sizeof(*r)) ||
            overlap(output, sizeof(*output), r->output.results,
                    sizeof(*r->output.results)) ||
            overlap(output, sizeof(*output), r->output.results->regular.data,
                    r->output.results->regular.bytes) ||
            overlap(output, sizeof(*output), r->output.results->injected.data,
                    r->output.results->injected.bytes))
            return NX_ERROR_INVALID;
    }
    nx_adc_group_info_t value;
    nx_result_t result = nx_stm32_adc_group_info_snapshot(&s->base, &value);
    if (result != NX_SUCCESS)
        return result;
    value.max_frames = 1U;
    if (injected) {
        value.channels = s->config->injected_channels;
        value.sample_times = s->config->injected_samples;
        value.converters = s->config->injected_converters;
        value.format.group = NX_ADC_INJECTED;
        value.format.trigger = NX_ADC_TRIGGER_EVENT;
        value.format.channel_count =
            (uint16_t)(count(s) * s->config->injected_ranks);
        value.format.scan_stride = (size_t)value.format.channel_count * 2U;
        value.physical_ranks = value.format.channel_count;
        value.buffer_alignment = 2U;
        value.physical_scan_halfcycles = 0U;
        for (unsigned rank = 0U; rank < s->config->injected_ranks; ++rank)
            value.physical_scan_halfcycles +=
                value.sample_halfcycles[value.sample_times[(size_t)rank *
                                                           count(s)]] +
                2U * value.format.significant_bits;
    }
    *output = value;
    return NX_SUCCESS;
}
static nx_result_t info(const void* c, nx_adc_group_info_t* out) {
    return info_selected(c, out, false);
}
static nx_result_t injected_info(const void* c, nx_adc_group_info_t* out) {
    return info_selected(c, out, true);
}
nx_result_t nx_stm32_adc_group_automatic_prepare(
    nx_stm32_adc_group_automatic_state_t* s,
    const nx_stm32_adc_group_combined_config_t* c) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !nx_stm32_adc_group_combined_facts(c, true) ||
        overlap(s, sizeof(*s), c, sizeof(*c)) ||
        overlap(s, sizeof(*s), &g_nx_stm32_system, sizeof(g_nx_stm32_system)))
        return NX_ERROR_INVALID;
    if (s->config != NULL)
        return NX_ERROR_BUSY;
    nx_stm32_adc_group_automatic_state_t probe = {
        .base = {.config = &c->regular.adc}, .config = c};
    if (!independent(&probe, s, sizeof(*s),
                     _Alignof(nx_stm32_adc_group_automatic_state_t)))
        return NX_ERROR_INVALID;
    nx_result_t result =
        nx_stm32_adc_group_prepare_once(&s->base, &c->regular.adc);
    if (s->base.config != NULL)
        s->config = c;
    if (result != NX_SUCCESS)
        return result;
    s->history_lost = false;
    return nx_stm32_adc_group_injected_pads(c);
}
nx_result_t
nx_stm32_adc_group_automatic_close(nx_stm32_adc_group_automatic_state_t* s) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (s == NULL)
        return NX_ERROR_INVALID;
    if (s->config == NULL)
        return s->base.config == NULL ? NX_SUCCESS : NX_ERROR_STATE;
    s->base.closing = true;
    if (s->base.active != NULL) {
        nx_result_t result = stop(s);
        if (result != NX_SUCCESS)
            return result;
    }
    if (s->history_lost)
        return NX_ERROR_STATE;
    nx_result_t result = nx_stm32_adc_group_close(&s->base);
    if (result == NX_SUCCESS)
        s->config = NULL;
    return result;
}
const nx_adc_group_ops_t nx_stm32_adc_group_automatic_ops = {.submit = submit,
                                                             .cancel = cancel,
                                                             .service = service,
                                                             .stop = stop,
                                                             .info = info,
                                                             .injected_info =
                                                                 injected_info};
