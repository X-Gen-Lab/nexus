/**
 * \file            timer_waveform.c
 * \brief           F407 coherent timer groups with retained output withdrawal
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_timer_output.h"
#include "stm32f407_timer_waveform.h"
#include <string.h>

#define WAVE_PULSE_DONE (1U << 0U)
#define WAVE_DMA_DONE   (1U << 1U)
#define WAVE_DMA_ERROR  (1U << 2U)
#if NEXUS_STM32_TIMER_PULSE
static nx_result_t pulse_stop(void* context);
#endif
#if NEXUS_STM32_TIMER_SEQUENCE
static nx_result_t sequence_stop(void* context);
#endif

static uint32_t base_cr1(const nx_stm32_waveform_config_t* c) {
    return TIM_CR1_ARPE | TIM_CR1_URS |
           (c->kind == NX_STM32_WAVE_PULSE ? TIM_CR1_OPM : 0U);
}
static uint32_t stored_compare(const nx_stm32_waveform_config_t* c,
                               uint32_t duty) {
    return c->kind == NX_STM32_WAVE_PULSE ? c->period_ticks - duty : duty;
}

static bool advanced(const nx_stm32_waveform_config_t* c) {
    return c->controller == 1U || c->controller == 8U;
}
static bool apb2(const nx_stm32_waveform_config_t* c) {
    return advanced(c) || (c->controller >= 9U && c->controller <= 11U);
}
static bool master_capable(const nx_stm32_waveform_config_t* c) {
    return advanced(c) || (c->controller >= 3U && c->controller <= 5U);
}
static bool slave_capable(const nx_stm32_waveform_config_t* c) {
    return master_capable(c) || c->controller == 9U || c->controller == 12U;
}
#if NEXUS_STM32_TIMER_SEQUENCE
static uint32_t dma_gate(const nx_stm32_waveform_config_t* c) {
    return advanced(c) ? RCC_AHB1ENR_DMA2EN : RCC_AHB1ENR_DMA1EN;
}
static bool dma_valid(const nx_stm32_waveform_config_t* c) {
    const nx_stm32_av_dma_t* d = &c->dma;
    uint8_t lane = 0U, channel = 0U;
    IRQn_Type irq;
    switch (c->controller) {
        case 1U:
            lane = 5U;
            channel = 6U;
            irq = DMA2_Stream5_IRQn;
            break;
        case 8U:
            lane = 1U;
            channel = 7U;
            irq = DMA2_Stream1_IRQn;
            break;
        case 3U:
            lane = 2U;
            channel = 5U;
            irq = DMA1_Stream2_IRQn;
            break;
        case 4U:
            lane = 6U;
            channel = 2U;
            irq = DMA1_Stream6_IRQn;
            break;
        case 5U:
            channel = 6U;
            irq = DMA1_Stream0_IRQn;
            break;
        default:
            return false;
    }
    if (d->dma == NULL || d->stream == NULL || d->lane != lane ||
        d->channel != channel || d->irq != irq)
        return false;
#ifndef NEXUS_STM32_MODEL
    DMA_Stream_TypeDef* streams[5] = {DMA2_Stream5, DMA2_Stream1, DMA1_Stream2,
                                      DMA1_Stream6, DMA1_Stream0};
    unsigned index = c->controller == 1U   ? 0U
                     : c->controller == 8U ? 1U
                     : c->controller == 3U ? 2U
                     : c->controller == 4U ? 3U
                                           : 4U;
    if (d->dma != (advanced(c) ? DMA2 : DMA1) || d->stream != streams[index])
        return false;
#endif
    return true;
}
static bool dma_foreign(const nx_stm32_av_dma_t* d) {
    return (NX_AV_READ(&d->stream->CR) &
            (DMA_SxCR_EN | DMA_SxCR_TCIE | DMA_SxCR_HTIE | DMA_SxCR_TEIE |
             DMA_SxCR_DMEIE)) != 0U ||
           (NX_AV_READ(&d->stream->FCR) & DMA_SxFCR_FEIE) != 0U ||
           NX_AV_IRQ_ENABLED(d->irq) != 0U || NX_AV_IRQ_PENDING(d->irq) != 0U ||
           NX_AV_IRQ_ACTIVE(d->irq) != 0U;
}
#endif
static uint32_t gate(const nx_stm32_waveform_config_t* c) {
    switch (c->controller) {
        case 1U:
            return RCC_APB2ENR_TIM1EN;
        case 3U:
            return RCC_APB1ENR_TIM3EN;
        case 4U:
            return RCC_APB1ENR_TIM4EN;
        case 5U:
            return RCC_APB1ENR_TIM5EN;
        case 8U:
            return RCC_APB2ENR_TIM8EN;
        case 9U:
            return RCC_APB2ENR_TIM9EN;
        case 10U:
            return RCC_APB2ENR_TIM10EN;
        case 11U:
            return RCC_APB2ENR_TIM11EN;
        case 12U:
            return RCC_APB1ENR_TIM12EN;
        case 13U:
            return RCC_APB1ENR_TIM13EN;
        case 14U:
            return RCC_APB1ENR_TIM14EN;
        default:
            return 0U;
    }
}
static volatile uint32_t* clock_register(const nx_stm32_waveform_config_t* c,
                                         bool reset) {
    RCC_TypeDef* rcc = g_nx_stm32_system.rcc;
    return apb2(c) ? (reset ? &rcc->APB2RSTR : &rcc->APB2ENR)
                   : (reset ? &rcc->APB1RSTR : &rcc->APB1ENR);
}
static unsigned channel_count(uint8_t channels) {
    unsigned result = 0U;
    for (unsigned i = 0U; i < 4U; ++i)
        result += ((unsigned)channels >> i) & 1U;
    return result;
}
static volatile uint32_t* compare(TIM_TypeDef* timer, unsigned channel) {
    switch (channel) {
        case 0U:
            return &timer->CCR1;
        case 1U:
            return &timer->CCR2;
        case 2U:
            return &timer->CCR3;
        default:
            return &timer->CCR4;
    }
}
static uint32_t ccmr(const nx_stm32_waveform_config_t* c, unsigned bank) {
    uint32_t result = 0U;
    for (unsigned i = 0U; i < 2U; ++i) {
        unsigned channel = bank * 2U + i;
        if ((c->channels & (1U << channel)) != 0U)
            result |= (c->kind == NX_STM32_WAVE_PULSE ? 0x78U : 0x68U)
                      << (i * 8U);
        else if (channel == 3U && c->internal_trigger)
            result |= 0x78U << (i * 8U);
    }
    return result;
}
static uint32_t output_enable(const nx_stm32_waveform_config_t* c) {
    uint32_t result = 0U;
    for (unsigned i = 0U; i < 4U; ++i)
        if ((c->channels & (1U << i)) != 0U)
            result |= (c->complementary ? 5U : 1U) << (i * 4U);
    return result;
}
static uint32_t idle_bits(const nx_stm32_waveform_config_t* c) {
    uint32_t result = 0U;
    for (unsigned i = 0U; i < 4U; ++i)
        if ((c->channels & (1U << i)) != 0U)
            result |= ((c->idle_high ? 1U : 0U) |
                       (c->complementary && c->idle_complement_high ? 2U : 0U))
                      << (8U + i * 2U);
    return result;
}
static bool valid(const nx_stm32_waveform_config_t* c) {
    if (c == NULL ||
        (uintptr_t)c % _Alignof(nx_stm32_waveform_config_t) != 0U ||
        c->registers == NULL || gate(c) == 0U || c->channels == 0U ||
        c->channels > 15U || c->common.poll_limit == 0U ||
        c->common.priority > 15U || c->common.pins == NULL ||
        c->tick_hz == 0U || c->timer_hz == 0U ||
        c->timer_hz != (apb2(c) ? NEXUS_APB2_TIMER_HZ : NEXUS_APB1_TIMER_HZ) ||
        c->timer_hz % c->tick_hz != 0U || c->timer_hz / c->tick_hz > 65536U ||
        c->period_ticks == 0U ||
        (c->controller != 5U && c->period_ticks > 65535U) ||
        (!advanced(c) && (c->deadtime_ticks != 0U || c->complementary)) ||
        (c->complementary && (c->channels & 8U) != 0U) ||
        (c->internal_trigger &&
         (!master_capable(c) || (c->channels & 8U) != 0U ||
          c->period_ticks < 2U)) ||
        (c->master_mode != 0U && c->master_mode != 1U && c->master_mode != 2U &&
         c->master_mode != 7U) ||
        (!master_capable(c) && c->master_mode != 0U) ||
        (c->master_mode == 7U && !c->internal_trigger) ||
        (c->sync_mode != 0U && c->sync_mode != 4U && c->sync_mode != 5U &&
         c->sync_mode != 6U) ||
        (!slave_capable(c) && c->sync_mode != 0U) || c->itr > 3U ||
        (c->kind == NX_STM32_WAVE_PULSE &&
         (c->complementary || c->internal_trigger)))
        return false;
#if NEXUS_STM32_TIMER_SEQUENCE
    if (c->kind == NX_STM32_WAVE_SEQUENCE &&
        (!dma_valid(c) || c->internal_trigger || c->common.regions == NULL ||
         c->common.region_count == 0U || c->period_ticks > 65535U ||
         c->burst_budget_hclk_cycles == 0U ||
         (uint64_t)c->burst_budget_hclk_cycles * c->tick_hz >=
             (uint64_t)c->period_ticks * NEXUS_CORE_HZ))
        return false;
#endif
    uint8_t maximum = c->controller == 9U || c->controller == 12U    ? 3U
                      : c->controller >= 10U && c->controller != 12U ? 1U
                                                                     : 15U;
    if ((c->channels & ~maximum) != 0U ||
        c->common.pin_count !=
            channel_count(c->channels) * (c->complementary ? 2U : 1U) +
                (advanced(c) ? 1U : 0U))
        return false;
    for (size_t i = 0U; i < c->common.pin_count; ++i) {
        const nx_stm32_av_pin_t* pin = &c->common.pins[i];
        if (pin->registers == NULL || pin->snapshot == NULL || pin->bank > 8U ||
            pin->pin > 15U || pin->af > 15U)
            return false;
    }
    uint32_t code = 0U;
    if (advanced(c) &&
        (c->break_input == NULL || c->break_mask == 0U ||
         c->irq !=
             (c->controller == 1U ? TIM1_BRK_TIM9_IRQn : TIM8_BRK_TIM12_IRQn) ||
         (c->break_mask & (c->break_mask - 1U)) != 0U ||
         !nx_stm32_timer_deadtime_code(c->deadtime_ticks, &code)))
        return false;
#ifndef NEXUS_STM32_MODEL
    TIM_TypeDef* expected = NULL;
    switch (c->controller) {
        case 1U:
            expected = TIM1;
            break;
        case 3U:
            expected = TIM3;
            break;
        case 4U:
            expected = TIM4;
            break;
        case 5U:
            expected = TIM5;
            break;
        case 8U:
            expected = TIM8;
            break;
        case 9U:
            expected = TIM9;
            break;
        case 10U:
            expected = TIM10;
            break;
        case 11U:
            expected = TIM11;
            break;
        case 12U:
            expected = TIM12;
            break;
        case 13U:
            expected = TIM13;
            break;
        case 14U:
            expected = TIM14;
            break;
        default:
            break;
    }
    if (c->registers != expected)
        return false;
#endif
    return true;
}
static bool live(nx_stm32_waveform_state_t* s) {
    const nx_stm32_waveform_config_t* c = s->config;
    bool ok = !s->history_lost && g_nx_stm32_system.started &&
              nx_stm32_clock_profile_matches(g_nx_stm32_system.rcc, NULL) &&
              (NX_AV_READ(clock_register(c, false)) & gate(c)) != 0U &&
              (NX_AV_READ(clock_register(c, true)) & gate(c)) == 0U;
    if (s->active != NULL)
        ok = ok && nx_stm32_av_timebase() &&
             g_nx_stm32_system.timer_error == NX_SUCCESS &&
             nx_stm32_av_epoch() == s->time_epoch;
#if NEXUS_STM32_TIMER_SEQUENCE
    if (c->kind == NX_STM32_WAVE_SEQUENCE)
        ok =
            ok &&
            (NX_AV_READ(&g_nx_stm32_system.rcc->AHB1ENR) & dma_gate(c)) != 0U &&
            (NX_AV_READ(&g_nx_stm32_system.rcc->AHB1RSTR) & dma_gate(c)) == 0U;
#endif
    if (!ok && s->active != NULL)
        s->history_lost = true;
    return ok;
}
static bool observe_break(nx_stm32_waveform_state_t* s) {
    const nx_stm32_waveform_config_t* c = s->config;
    return advanced(c) && nx_stm32_timer_break_observe(
                              c->registers, c->break_input, c->break_mask,
                              c->break_active_high, &s->fault);
}
static nx_result_t idle_pins(nx_stm32_waveform_state_t* s) {
    const nx_stm32_waveform_config_t* c = s->config;
    size_t outputs =
        (size_t)channel_count(c->channels) * (c->complementary ? 2U : 1U);
    for (size_t i = 0U; i < outputs; ++i) {
        const nx_stm32_av_pin_t* pin = &c->common.pins[i];
        bool high = c->complementary && (i & 1U) != 0U ? c->idle_complement_high
                                                       : c->idle_high;
        if ((NX_AV_READ(&g_nx_stm32_system.rcc->AHB1ENR) & (1U << pin->bank)) ==
            0U)
            return NX_ERROR_STATE;
        NX_AV_WRITE(&pin->registers->BSRR,
                    1U << (pin->pin + (high ? 0U : 16U)));
        nx_arch_dsb();
        if (((NX_AV_READ(&pin->registers->ODR) >> pin->pin) & 1U) !=
                (high ? 1U : 0U) ||
            !nx_stm32_av_bits(&pin->registers->MODER, 3U << (pin->pin * 2U),
                              1U << (pin->pin * 2U)))
            return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}
static nx_result_t quiet(nx_stm32_waveform_state_t* s) {
    if (!live(s))
        return NX_ERROR_STATE;
    (void)observe_break(s);
    bool disarmed =
        !slave_capable(s->config) ||
        nx_stm32_av_write(&s->config->registers->SMCR, 0U, UINT32_MAX);
    bool stopped = nx_stm32_timer_output_stop(
        s->config->registers, advanced(s->config), UINT32_MAX, UINT32_MAX);
    if (stopped && master_capable(s->config))
        stopped = nx_stm32_av_bits(&s->config->registers->CR2, TIM_CR2_MMS,
                                   1U << TIM_CR2_MMS_Pos);
    if (!stopped || !disarmed) {
        s->quarantined = true;
        return NX_ERROR_BUSY;
    }
    s->outputs_armed = false;
    if (s->timer_irq_owned && !nx_stm32_av_irq_detach(s->config->irq)) {
        s->quarantined = true;
        return NX_ERROR_BUSY;
    }
    nx_result_t result = idle_pins(s);
    s->quarantined = result != NX_SUCCESS;
    return result;
}
static bool invariant(nx_stm32_waveform_state_t* s) {
    const nx_stm32_waveform_config_t* c = s->config;
    TIM_TypeDef* t = c->registers;
    if (NX_AV_READ(&t->PSC) != c->timer_hz / c->tick_hz - 1U ||
        NX_AV_READ(&t->ARR) != c->period_ticks - 1U ||
        (NX_AV_READ(&t->CR1) & ~TIM_CR1_CEN) != base_cr1(c) ||
        NX_AV_READ(&t->CCMR1) != ccmr(c, 0U) ||
        (master_capable(c) &&
         (NX_AV_READ(&t->CCMR2) != ccmr(c, 1U) ||
          NX_AV_READ(&t->CR2) != ((advanced(c) ? idle_bits(c) : 0U) |
                                  ((s->outputs_armed ? c->master_mode : 1U)
                                   << TIM_CR2_MMS_Pos)))) ||
        (slave_capable(c) &&
         NX_AV_READ(&t->SMCR) !=
             (s->outputs_armed && c->sync_mode != 0U
                  ? (uint32_t)c->sync_mode | ((uint32_t)c->itr << 4U)
                  : 0U)))
        return false;
    for (unsigned i = 0U; i < 4U; ++i)
        if (((c->channels & (1U << i)) != 0U ||
             (i == 3U && c->internal_trigger)) &&
            NX_AV_READ(compare(t, i)) !=
                (i == 3U && c->internal_trigger
                     ? s->trigger_ticks
                     : stored_compare(c, s->duties[i])))
            return false;
    if (advanced(c)) {
        uint32_t code = 0U;
        (void)nx_stm32_timer_deadtime_code(c->deadtime_ticks, &code);
        uint32_t protection = code | TIM_BDTR_OSSI | TIM_BDTR_OSSR |
                              TIM_BDTR_BKE |
                              (c->break_active_high ? TIM_BDTR_BKP : 0U);
        if ((NX_AV_READ(&t->BDTR) & ~TIM_BDTR_MOE) != protection)
            return false;
    }
    return true;
}
nx_result_t nx_stm32_waveform_admit(const nx_stm32_waveform_state_t* s,
                                    const nx_stm32_waveform_config_t* c) {
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (c != NULL &&
        (uintptr_t)c % _Alignof(nx_stm32_waveform_config_t) == 0U &&
        c->controller == 2U)
        return NX_ERROR_PERMISSION;
    if (s == NULL || (uintptr_t)s % _Alignof(nx_stm32_waveform_state_t) != 0U ||
        !valid(c) || s->owned ||
        nx_stm32_av_overlap(s, sizeof(*s), c, sizeof(*c)))
        return NX_ERROR_INVALID;
    if (c->kind != NX_STM32_WAVE_GROUP
#if NEXUS_STM32_TIMER_PULSE
        && c->kind != NX_STM32_WAVE_PULSE
#endif
#if NEXUS_STM32_TIMER_SEQUENCE
        && c->kind != NX_STM32_WAVE_SEQUENCE
#endif
    )
        return NX_ERROR_UNSUPPORTED;
    if (g_nx_stm32_system.rcc == NULL)
        return NX_ERROR_STATE;
    if ((NX_AV_READ(clock_register(c, false)) & gate(c)) != 0U ||
        (NX_AV_READ(clock_register(c, true)) & gate(c)) != 0U ||
        (advanced(c) &&
         (NX_AV_IRQ_ENABLED(c->irq) != 0U || NX_AV_IRQ_PENDING(c->irq) != 0U ||
          NX_AV_IRQ_ACTIVE(c->irq) != 0U)))
        return NX_ERROR_BUSY;
#if NEXUS_STM32_TIMER_SEQUENCE
    if (c->kind == NX_STM32_WAVE_SEQUENCE &&
        (NX_AV_READ(&g_nx_stm32_system.rcc->AHB1ENR) & dma_gate(c)) != 0U &&
        dma_foreign(&c->dma))
        return NX_ERROR_BUSY;
#endif
    return NX_SUCCESS;
}
nx_result_t nx_stm32_waveform_prepare(nx_stm32_waveform_state_t* s,
                                      const nx_stm32_waveform_config_t* c) {
    nx_result_t result = nx_stm32_waveform_admit(s, c);
    if (result != NX_SUCCESS)
        return result;
    if (!g_nx_stm32_system.started ||
        !nx_stm32_clock_profile_matches(g_nx_stm32_system.rcc, NULL))
        return NX_ERROR_STATE;
#if NEXUS_STM32_TIMER_SEQUENCE
    if (c->kind == NX_STM32_WAVE_SEQUENCE &&
        ((NX_AV_READ(&g_nx_stm32_system.rcc->AHB1ENR) & dma_gate(c)) == 0U ||
         (NX_AV_READ(&g_nx_stm32_system.rcc->AHB1RSTR) & dma_gate(c)) != 0U))
        return NX_ERROR_STATE;
#endif
    for (size_t i = 0U; i < c->common.pin_count; ++i)
        if ((NX_AV_READ(&g_nx_stm32_system.rcc->AHB1ENR) &
             (1U << c->common.pins[i].bank)) == 0U)
            return NX_ERROR_STATE;
    s->config = c;
    s->owned = true;
    if (advanced(c))
        s->priorities[0] = NX_AV_IRQ_PRIORITY_READ(c->irq);
#if NEXUS_STM32_TIMER_SEQUENCE
    if (c->kind == NX_STM32_WAVE_SEQUENCE)
        s->priorities[1] = NX_AV_IRQ_PRIORITY_READ(c->dma.irq);
#endif
    if (!nx_stm32_av_bits(clock_register(c, false), gate(c), gate(c)))
        return NX_ERROR_IO;
    TIM_TypeDef* t = c->registers;
    if ((NX_AV_READ(&t->CR1) & TIM_CR1_CEN) != 0U ||
        NX_AV_READ(&t->CCER) != 0U || NX_AV_READ(&t->DIER) != 0U ||
        (advanced(c) &&
         (NX_AV_READ(&t->BDTR) & (TIM_BDTR_MOE | TIM_BDTR_LOCK)) != 0U))
        return NX_ERROR_BUSY;
    result = nx_stm32_av_pins(&c->common);
    if (result != NX_SUCCESS)
        return result;
    result = idle_pins(s);
    if (result != NX_SUCCESS)
        return result;
    s->timer_touched = true;
    bool ok = nx_stm32_av_write(&t->CR1, base_cr1(c), UINT32_MAX);
    ok =
        nx_stm32_av_write(&t->PSC, c->timer_hz / c->tick_hz - 1U, UINT32_MAX) &&
        ok;
    ok = nx_stm32_av_write(&t->ARR, c->period_ticks - 1U, UINT32_MAX) && ok;
    ok = nx_stm32_av_write(&t->CNT, 0U, UINT32_MAX) && ok;
    ok = nx_stm32_av_write(&t->CCMR1, ccmr(c, 0U), UINT32_MAX) && ok;
    if (master_capable(c)) {
        ok = nx_stm32_av_write(&t->CCMR2, ccmr(c, 1U), UINT32_MAX) && ok;
        /* MMS=RESET emits UG as TRGO. ENABLE with a stopped counter is low,
         * allowing software preload updates without triggering armed peers. */
        ok = nx_stm32_av_write(&t->CR2,
                               (advanced(c) ? idle_bits(c) : 0U) |
                                   (1U << TIM_CR2_MMS_Pos),
                               UINT32_MAX) &&
             ok;
        ok = nx_stm32_av_write(&t->DCR, 0U, UINT32_MAX) && ok;
    }
    if (slave_capable(c))
        ok = nx_stm32_av_write(&t->SMCR, 0U, UINT32_MAX) && ok;
    s->trigger_ticks = c->internal_trigger ? c->period_ticks / 2U : 0U;
    for (unsigned i = 0U; i < 4U; ++i)
        if ((c->channels & (1U << i)) != 0U || (i == 3U && c->internal_trigger))
            ok = nx_stm32_av_write(compare(t, i),
                                   i == 3U && c->internal_trigger
                                       ? s->trigger_ticks
                                       : stored_compare(c, 0U),
                                   UINT32_MAX) &&
                 ok;
    if (advanced(c)) {
        uint32_t code = 0U;
        (void)nx_stm32_timer_deadtime_code(c->deadtime_ticks, &code);
        ok = nx_stm32_av_write(&t->RCR, 0U, UINT32_MAX) && ok;
        ok = nx_stm32_av_write(&t->BDTR,
                               code | TIM_BDTR_OSSI | TIM_BDTR_OSSR |
                                   TIM_BDTR_BKE |
                                   (c->break_active_high ? TIM_BDTR_BKP : 0U),
                               UINT32_MAX) &&
             ok;
    }
    NX_AV_WRITE(&t->EGR, TIM_EGR_UG);
    NX_AV_WRITE(&t->SR,
                ~(uint32_t)(TIM_SR_UIF | (advanced(c) ? TIM_SR_BIF : 0U)));
    nx_arch_dsb();
    if (!ok)
        return NX_ERROR_IO;
    s->shadow_valid = true;
    s->initialized = true;
    (void)observe_break(s);
    return NX_SUCCESS;
}
static nx_result_t failed_commit(nx_stm32_waveform_state_t* s) {
    s->fault = true;
    s->shadow_valid = false;
    nx_result_t stopped = quiet(s);
    return stopped == NX_SUCCESS ? NX_ERROR_IO : stopped;
}
static nx_result_t stage_values(nx_stm32_waveform_state_t* s,
                                const uint32_t duty[4], uint32_t trigger) {
    const nx_stm32_waveform_config_t* c = s->config;
    if (!live(s))
        return NX_ERROR_STATE;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    TIM_TypeDef* t = c->registers;
    if (!nx_stm32_av_bits(&t->CR1, TIM_CR1_UDIS, TIM_CR1_UDIS)) {
        nx_result_t result = failed_commit(s);
        nx_arch_irq_restore(saved);
        return result;
    }
    bool ok = true;
    for (unsigned i = 0U; i < 4U; ++i)
        if ((c->channels & (1U << i)) != 0U || (i == 3U && c->internal_trigger))
            ok = nx_stm32_av_write(compare(t, i),
                                   i == 3U && c->internal_trigger
                                       ? trigger
                                       : stored_compare(c, duty[i]),
                                   UINT32_MAX) &&
                 ok;
    if (observe_break(s) || !ok) {
        nx_result_t result = failed_commit(s);
        nx_arch_irq_restore(saved);
        return result;
    }
    if (!nx_stm32_av_bits(&t->CR1, TIM_CR1_UDIS, 0U)) {
        nx_result_t result = failed_commit(s);
        nx_arch_irq_restore(saved);
        return result;
    }
    memcpy(s->duties, duty, sizeof(s->duties));
    s->trigger_ticks = trigger;
    s->shadow_valid = true;
    s->quarantined = false;
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}
static nx_result_t group_commit(void* context,
                                const nx_pwm_group_frame_t* frame) {
    nx_stm32_waveform_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->initialized || s->closing || frame == NULL ||
        s->config->kind != NX_STM32_WAVE_GROUP ||
        frame->channel_count != channel_count(s->config->channels) ||
        frame->duty_ticks == NULL ||
        nx_stm32_av_overlap(frame, sizeof(*frame), s, sizeof(*s)) ||
        nx_stm32_av_overlap(frame->duty_ticks,
                            frame->channel_count * sizeof(uint32_t), s,
                            sizeof(*s)))
        return NX_ERROR_INVALID;
    const nx_stm32_waveform_config_t* c = s->config;
    uint32_t duty[4] = {0U};
    for (unsigned i = 0U, item = 0U; i < 4U; ++i)
        if ((c->channels & (1U << i)) != 0U) {
            duty[i] = frame->duty_ticks[item++];
            if (duty[i] > c->period_ticks)
                return NX_ERROR_INVALID;
        }
    if ((c->internal_trigger && (frame->trigger_ticks == 0U ||
                                 frame->trigger_ticks >= c->period_ticks)) ||
        (!c->internal_trigger && frame->trigger_ticks != 0U))
        return NX_ERROR_INVALID;
    return stage_values(s, duty, frame->trigger_ticks);
}
static nx_result_t group_start(void* context) {
    nx_stm32_waveform_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->initialized || s->closing)
        return NX_ERROR_INVALID;
    if (!live(s))
        return NX_ERROR_STATE;
    if (observe_break(s) || s->fault || s->quarantined || !s->shadow_valid ||
        !invariant(s))
        return NX_ERROR_STATE;
    const nx_stm32_waveform_config_t* c = s->config;
    TIM_TypeDef* t = c->registers;
    if ((NX_AV_READ(&t->CR1) & TIM_CR1_CEN) != 0U || NX_AV_READ(&t->CCER) != 0U)
        return NX_ERROR_BUSY;
    nx_result_t result = nx_stm32_av_pins(&c->common);
    if (result != NX_SUCCESS)
        return result;
    NX_AV_WRITE(&t->EGR, TIM_EGR_UG);
    NX_AV_WRITE(&t->SR,
                ~(uint32_t)(TIM_SR_UIF | (advanced(c) ? TIM_SR_BIF : 0U)));
    if ((NX_AV_READ(&t->SR) & TIM_SR_UIF) != 0U) {
        s->fault = true;
        (void)quiet(s);
        return NX_ERROR_IO;
    }
    bool ok = nx_stm32_av_write(&t->CCER, output_enable(c), UINT32_MAX);
    if (advanced(c)) {
        s->timer_irq_owned = true;
        NX_AV_IRQ_PRIORITY(c->irq, c->common.priority);
        NX_AV_IRQ_CLEAR(c->irq);
        NX_AV_IRQ_ENABLE(c->irq);
        ok = nx_stm32_av_bits(&t->DIER, TIM_DIER_BIE, TIM_DIER_BIE) && ok;
    }
    if (!ok) {
        s->fault = true;
        (void)quiet(s);
        return NX_ERROR_IO;
    }
    if (advanced(c) && c->kind == NX_STM32_WAVE_PULSE) {
        if (observe_break(s) || s->fault ||
            !nx_stm32_av_bits(&t->BDTR, TIM_BDTR_MOE, TIM_BDTR_MOE)) {
            s->fault = true;
            (void)quiet(s);
            return NX_ERROR_IO;
        }
    }
    if (advanced(c) && c->kind != NX_STM32_WAVE_PULSE && c->sync_mode != 0U)
        ok = nx_stm32_av_bits(&t->BDTR, TIM_BDTR_MOE, TIM_BDTR_MOE) && ok;
    if (master_capable(c))
        ok = nx_stm32_av_bits(&t->CR2, TIM_CR2_MMS,
                              (uint32_t)c->master_mode << TIM_CR2_MMS_Pos) &&
             ok;
    if (!ok) {
        s->fault = true;
        (void)quiet(s);
        return NX_ERROR_IO;
    }
    if (slave_capable(c))
        ok = nx_stm32_av_write(&t->SMCR,
                               c->sync_mode != 0U ? (uint32_t)c->sync_mode |
                                                        ((uint32_t)c->itr << 4U)
                                                  : 0U,
                               UINT32_MAX) &&
             ok;
    if (!ok) {
        s->fault = true;
        (void)quiet(s);
        return NX_ERROR_IO;
    }
    bool started = c->sync_mode == 6U ||
                   nx_stm32_av_bits(&t->CR1, TIM_CR1_CEN, TIM_CR1_CEN);
    if (!started && c->kind == NX_STM32_WAVE_PULSE &&
        (NX_AV_READ(&t->CR1) & TIM_CR1_CEN) == 0U &&
        (NX_AV_READ(&t->SR) & TIM_SR_UIF) != 0U)
        started = true;
    ok = started && ok;
    if (observe_break(s) || !ok) {
        s->fault = true;
        (void)quiet(s);
        return NX_ERROR_IO;
    }
    if (advanced(c) && c->kind != NX_STM32_WAVE_PULSE && c->sync_mode == 0U &&
        !nx_stm32_av_bits(&t->BDTR, TIM_BDTR_MOE, TIM_BDTR_MOE)) {
        s->fault = true;
        (void)quiet(s);
        return NX_ERROR_IO;
    }
    if (observe_break(s)) {
        (void)quiet(s);
        return NX_ERROR_IO;
    }
    s->outputs_armed = true;
    return NX_SUCCESS;
}
static nx_result_t group_stop(void* context) {
    nx_stm32_waveform_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    return s == NULL || !s->owned ? NX_ERROR_STATE : quiet(s);
}
static nx_result_t group_clear(void* context) {
    nx_stm32_waveform_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->initialized || !s->shadow_valid)
        return NX_ERROR_STATE;
    if (!live(s) || !invariant(s) || observe_break(s))
        return NX_ERROR_STATE;
    TIM_TypeDef* t = s->config->registers;
    if ((NX_AV_READ(&t->CR1) & TIM_CR1_CEN) != 0U ||
        NX_AV_READ(&t->CCER) != 0U ||
        (advanced(s->config) && (NX_AV_READ(&t->BDTR) & TIM_BDTR_MOE) != 0U))
        return NX_ERROR_BUSY;
    s->fault = false;
    s->quarantined = false;
    return NX_SUCCESS;
}
static nx_result_t group_state(const void* context, nx_pwm_group_state_t* out) {
    nx_stm32_waveform_state_t* s = (nx_stm32_waveform_state_t*)context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->owned || out == NULL ||
        nx_stm32_av_overlap(out, sizeof(*out), s, sizeof(*s)) ||
        nx_stm32_av_overlap(out, sizeof(*out), s->config, sizeof(*s->config)))
        return NX_ERROR_INVALID;
    if (!live(s))
        return NX_ERROR_STATE;
    (void)observe_break(s);
    const nx_stm32_waveform_config_t* c = s->config;
    *out = (nx_pwm_group_state_t){
        c->period_ticks,
        c->tick_hz,
        c->deadtime_ticks,
        c->timer_hz,
        channel_count(c->channels),
        (NX_AV_READ(&c->registers->CR1) & TIM_CR1_CEN) != 0U,
        s->fault,
        s->quarantined};
    return NX_SUCCESS;
}
nx_result_t nx_stm32_waveform_close(nx_stm32_waveform_state_t* s) {
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL)
        return NX_ERROR_INVALID;
    if (!s->owned)
        return NX_SUCCESS;
    if (!s->timer_touched) {
        /* Only our gate attempt exists. In particular, a foreign retained
         * timer found after ungating has never authorized reset or pin/IRQ
         * edits. Restore the initial gate state without touching that cell. */
        const nx_stm32_waveform_config_t* c = s->config;
        if (!nx_stm32_av_bits(clock_register(c, false), gate(c), 0U))
            return NX_ERROR_IO;
        memset(s, 0, sizeof(*s));
        return NX_SUCCESS;
    }
    s->closing = true;
    s->initialized = false;
#if NEXUS_STM32_TIMER_PULSE
    if (s->config->kind == NX_STM32_WAVE_PULSE) {
        nx_result_t joined = pulse_stop(s);
        if (joined != NX_SUCCESS)
            return joined;
    }
#endif
#if NEXUS_STM32_TIMER_SEQUENCE
    if (s->config->kind == NX_STM32_WAVE_SEQUENCE) {
        nx_result_t joined = sequence_stop(s);
        if (joined != NX_SUCCESS)
            return joined;
    }
#endif
    nx_result_t result = quiet(s);
    if (result != NX_SUCCESS)
        return result;
    const nx_stm32_waveform_config_t* c = s->config;
    if (advanced(c) &&
        !nx_stm32_av_irq_priority_restore(c->irq, s->priorities[0]))
        return NX_ERROR_IO;
#if NEXUS_STM32_TIMER_SEQUENCE
    if (c->kind == NX_STM32_WAVE_SEQUENCE &&
        !nx_stm32_av_irq_priority_restore(c->dma.irq, s->priorities[1]))
        return NX_ERROR_IO;
#endif
    if (!nx_stm32_av_bits(clock_register(c, true), gate(c), gate(c)) ||
        !nx_stm32_av_bits(clock_register(c, true), gate(c), 0U) ||
        !nx_stm32_av_bits(clock_register(c, false), gate(c), 0U))
        return NX_ERROR_IO;
    memset(s, 0, sizeof(*s));
    return NX_SUCCESS;
}
void nx_stm32_waveform_irq(nx_stm32_waveform_state_t* s) {
    if (s != NULL && s->owned && live(s))
        (void)observe_break(s);
}
void nx_stm32_waveform_dma_irq(nx_stm32_waveform_state_t* s) {
#if NEXUS_STM32_TIMER_SEQUENCE
    if (s == NULL || !s->owned || s->active == NULL || !s->dma_armed ||
        s->config->kind != NX_STM32_WAVE_SEQUENCE || !live(s))
        return;
    uint32_t flags = nx_stm32_av_dma_flags(&s->config->dma);
    uint32_t events = ((flags & 0x0dU) != 0U ? WAVE_DMA_ERROR : 0U) |
                      ((flags & 0x20U) != 0U ? WAVE_DMA_DONE : 0U);
    __atomic_fetch_or(&s->events, events, __ATOMIC_RELAXED);
    nx_stm32_av_dma_ack(&s->config->dma, flags);
#else
    (void)s;
#endif
}
const nx_pwm_group_ops_t nx_stm32_pwm_group_ops = {
    group_commit, group_start, group_stop, group_clear, group_state};

#if NEXUS_STM32_TIMER_PULSE || NEXUS_STM32_TIMER_SEQUENCE
/** \brief Validate a complete control span before its first typed load. */
static bool object_span(const void* value, size_t bytes, size_t alignment) {
    uintptr_t address = (uintptr_t)value;
    return value != NULL && bytes != 0U && address % alignment == 0U &&
           bytes <= UINTPTR_MAX - address;
}
static nx_result_t request_quarantine(nx_stm32_waveform_state_t* s,
                                      nx_request_t* request,
                                      nx_result_t result) {
    nx_request_state_t state = nx_request_state(request);
    if (state == NX_REQUEST_QUEUED || state == NX_REQUEST_ACTIVE)
        (void)nx_request_transition(request, NX_REQUEST_DRAINING);
    (void)nx_request_transition(request, NX_REQUEST_QUARANTINED);
    s->quarantined = true;
    return result;
}
#endif
#if NEXUS_STM32_TIMER_PULSE
static nx_result_t pulse_service(void* context);
static nx_result_t pulse_submit(void* context,
                                nx_timer_pulse_request_t* request) {
    nx_stm32_waveform_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->initialized || s->closing ||
        s->config->kind != NX_STM32_WAVE_PULSE ||
        !object_span(request, sizeof(*request),
                     _Alignof(nx_timer_pulse_request_t)))
        return NX_ERROR_INVALID;
    if (s->active != NULL)
        return NX_ERROR_BUSY;
    const nx_stm32_waveform_config_t* c = s->config;
    size_t count = channel_count(c->channels);
    size_t bytes = count * sizeof(uint32_t);
    if (request->channel_count != count ||
        !object_span(request->width_ticks, bytes, _Alignof(uint32_t)) ||
        nx_stm32_av_overlap(request, sizeof(*request), s, sizeof(*s)) ||
        nx_stm32_av_overlap(request, sizeof(*request), c, sizeof(*c)) ||
        nx_stm32_av_overlap(request, sizeof(*request), request->width_ticks,
                            bytes) ||
        nx_stm32_av_overlap(request->width_ticks, bytes, s, sizeof(*s)) ||
        nx_stm32_av_overlap(request->width_ticks, bytes, c, sizeof(*c)))
        return NX_ERROR_INVALID;
    if (!live(s) || !nx_stm32_av_timebase() ||
        g_nx_stm32_system.timer_error != NX_SUCCESS || s->fault ||
        s->quarantined)
        return NX_ERROR_STATE;
    if (nx_request_state(&request->base) != NX_REQUEST_READY)
        return NX_ERROR_STATE;
    nx_time_us_t now = NX_AV_NOW();
    if (nx_deadline_expired(request->base.deadline, now))
        return NX_ERROR_INVALID;
    uint32_t widths[4] = {0U};
    for (unsigned channel = 0U, item = 0U; channel < 4U; ++channel)
        if ((c->channels & (1U << channel)) != 0U) {
            widths[channel] = request->width_ticks[item++];
            if (widths[channel] >= c->period_ticks)
                return NX_ERROR_INVALID;
        }
    nx_result_t accepted = nx_request_admit(&request->base, NX_REQUEST_QUEUED);
    if (accepted != NX_SUCCESS)
        return accepted;
    s->active = request;
    s->observed_at = now;
    s->time_epoch = nx_stm32_av_epoch();
    s->events = 0U;
    s->result = NX_SUCCESS;
    nx_result_t staged = stage_values(s, widths, 0U);
    if (staged == NX_SUCCESS)
        staged = group_start(s);
    if (staged != NX_SUCCESS) {
        s->result = NX_ERROR_IO;
        return NX_SUCCESS;
    }
    (void)nx_request_transition(&request->base, NX_REQUEST_ACTIVE);
    return NX_SUCCESS;
}
static nx_result_t pulse_drain(nx_stm32_waveform_state_t* s) {
    nx_timer_pulse_request_t* request = s->active;
    if (!live(s))
        return request_quarantine(s, &request->base, NX_ERROR_STATE);
    nx_request_state_t state = nx_request_state(&request->base);
    if (state == NX_REQUEST_QUEUED || state == NX_REQUEST_ACTIVE)
        (void)nx_request_transition(&request->base, NX_REQUEST_DRAINING);
    nx_result_t result = quiet(s);
    if (result != NX_SUCCESS)
        return request_quarantine(s, &request->base, result);
    if (s->result == NX_SUCCESS &&
        nx_deadline_expired(request->base.deadline, NX_AV_NOW()))
        s->result = NX_ERROR_TIMEOUT;
    result = s->result;
    s->active = NULL;
    s->events = 0U;
    s->quarantined = false;
    nx_request_settle(&request->base, result, result == NX_SUCCESS ? 1U : 0U);
    return result;
}
static nx_result_t pulse_service(void* context) {
    nx_stm32_waveform_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->owned || s->config->kind != NX_STM32_WAVE_PULSE)
        return NX_ERROR_STATE;
    if (s->active == NULL)
        return NX_SUCCESS;
    nx_timer_pulse_request_t* request = s->active;
    if (!live(s))
        return request_quarantine(s, &request->base, NX_ERROR_STATE);
    nx_time_us_t now = NX_AV_NOW();
    if (now < s->observed_at) {
        s->history_lost = true;
        return request_quarantine(s, &request->base, NX_ERROR_STATE);
    }
    s->observed_at = now;
    (void)observe_break(s);
    if (s->fault && s->result == NX_SUCCESS)
        s->result = NX_ERROR_IO;
    if (nx_deadline_expired(request->base.deadline, now) &&
        s->result == NX_SUCCESS)
        s->result = NX_ERROR_TIMEOUT;
    TIM_TypeDef* t = s->config->registers;
    if ((NX_AV_READ(&t->SR) & TIM_SR_UIF) != 0U &&
        (NX_AV_READ(&t->CR1) & TIM_CR1_CEN) == 0U) {
        __atomic_fetch_or(&s->events, WAVE_PULSE_DONE, __ATOMIC_RELAXED);
        NX_AV_WRITE(&t->SR, ~(uint32_t)TIM_SR_UIF);
    }
    if (s->result != NX_SUCCESS ||
        (__atomic_load_n(&s->events, __ATOMIC_RELAXED) & WAVE_PULSE_DONE) != 0U)
        return pulse_drain(s);
    return NX_ERROR_BUSY;
}
static nx_result_t pulse_cancel(void* context,
                                nx_timer_pulse_request_t* request) {
    nx_stm32_waveform_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || request == NULL || s->active != request ||
        s->config->kind != NX_STM32_WAVE_PULSE)
        return NX_ERROR_STATE;
    if (s->result == NX_SUCCESS)
        s->result = NX_ERROR_CANCELLED;
    return pulse_service(s);
}
static nx_result_t pulse_stop(void* context) {
    nx_stm32_waveform_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->owned || s->config->kind != NX_STM32_WAVE_PULSE)
        return NX_ERROR_STATE;
    if (s->active != NULL) {
        (void)pulse_cancel(s, s->active);
        if (s->active != NULL)
            return NX_ERROR_BUSY;
    }
    return quiet(s);
}
static nx_result_t pulse_state(const void* context,
                               nx_timer_pulse_state_t* out) {
    nx_stm32_waveform_state_t* s = (nx_stm32_waveform_state_t*)context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->owned || s->config->kind != NX_STM32_WAVE_PULSE ||
        !object_span(out, sizeof(*out), _Alignof(nx_timer_pulse_state_t)) ||
        nx_stm32_av_overlap(out, sizeof(*out), s, sizeof(*s)) ||
        nx_stm32_av_overlap(out, sizeof(*out), s->config, sizeof(*s->config)))
        return NX_ERROR_INVALID;
    if (s->active != NULL) {
        nx_timer_pulse_request_t* request = s->active;
        if (nx_stm32_av_overlap(out, sizeof(*out), request, sizeof(*request)) ||
            nx_stm32_av_overlap(out, sizeof(*out), request->width_ticks,
                                request->channel_count * sizeof(uint32_t)))
            return NX_ERROR_INVALID;
    }
    if (!live(s))
        return NX_ERROR_STATE;
    (void)observe_break(s);
    const nx_stm32_waveform_config_t* c = s->config;
    bool running = (NX_AV_READ(&c->registers->CR1) & TIM_CR1_CEN) != 0U;
    *out = (nx_timer_pulse_state_t){
        c->tick_hz,
        c->period_ticks,
        channel_count(c->channels),
        s->active != NULL && c->sync_mode != 0U && !running &&
            (NX_AV_READ(&c->registers->SR) & TIM_SR_UIF) == 0U,
        running,
        s->fault,
        s->quarantined};
    return NX_SUCCESS;
}
const nx_timer_pulse_ops_t nx_stm32_timer_pulse_ops = {
    pulse_submit, pulse_cancel, pulse_service, pulse_stop, pulse_state};
#endif

#if NEXUS_STM32_TIMER_SEQUENCE
static bool sequence_overlap(const nx_stm32_waveform_state_t* s,
                             const nx_timer_sequence_request_t* request,
                             const void* output, size_t bytes) {
    const nx_frame_dma_loan_t* loan = request->source;
    const nx_frame_stream_t* frame = loan->owner;
    const nx_stream_t* stream = frame->stream;
    if (nx_stm32_av_overlap(output, bytes, s, sizeof(*s)) ||
        nx_stm32_av_overlap(output, bytes, s->config, sizeof(*s->config)) ||
        nx_stm32_av_overlap(output, bytes, request, sizeof(*request)) ||
        nx_stm32_av_overlap(output, bytes, loan, sizeof(*loan)) ||
        nx_stm32_av_overlap(output, bytes, frame, sizeof(*frame)) ||
        nx_stm32_av_overlap(output, bytes, stream, sizeof(*stream)) ||
        nx_stm32_av_overlap(output, bytes, stream->slots,
                            stream->count * sizeof(*stream->slots)))
        return true;
    for (size_t i = 0U; i < stream->count; ++i)
        if (nx_stm32_av_overlap(output, bytes, stream->slots[i].data,
                                stream->slots[i].capacity))
            return true;
    return false;
}
static nx_result_t sequence_validate(nx_stm32_waveform_state_t* s,
                                     nx_timer_sequence_request_t* request,
                                     nx_frame_dma_view_t* view) {
    nx_frame_dma_loan_t* loan = request->source;
    if (!object_span(loan, sizeof(*loan), _Alignof(nx_frame_dma_loan_t)) ||
        !object_span(loan->owner, sizeof(*loan->owner),
                     _Alignof(nx_frame_stream_t)) ||
        !object_span(loan->owner->stream, sizeof(*loan->owner->stream),
                     _Alignof(nx_stream_t)) ||
        loan->identity != loan || loan->owner->layout.plane_count != 1U)
        return NX_ERROR_INVALID;
    const nx_frame_plane_t* plane = &loan->owner->layout.planes[0];
    const size_t width = advanced(s->config) ? 4U : 2U;
    if (plane->offset != 0U || plane->row_bytes != 6U * width ||
        plane->stride != plane->row_bytes || plane->rows == 0U ||
        plane->rows > 65535U / 6U ||
        plane->rows * plane->row_bytes != loan->owner->layout.bytes ||
        loan->block.length != loan->owner->layout.bytes)
        return NX_ERROR_INVALID;
    nx_result_t result = nx_frame_dma_view(loan, 0U, 0U, 0U, plane->row_bytes,
                                           plane->rows, view);
    if (result != NX_SUCCESS)
        return result;
    result = nx_dma_buffer_validate(s->config->common.regions,
                                    s->config->common.region_count, view->data,
                                    view->span_bytes, width, NX_DMA_TO_DEVICE);
    if (result != NX_SUCCESS)
        return result;
    nx_stream_t* stream = loan->owner->stream;
    if (nx_stm32_av_overlap(request, sizeof(*request), loan, sizeof(*loan)) ||
        nx_stm32_av_overlap(request, sizeof(*request), loan->owner,
                            sizeof(*loan->owner)) ||
        nx_stm32_av_overlap(request, sizeof(*request), stream,
                            sizeof(*stream)) ||
        nx_stm32_av_overlap(request, sizeof(*request), stream->slots,
                            stream->count * sizeof(*stream->slots)))
        return NX_ERROR_INVALID;
    for (size_t i = 0U; i < stream->count; ++i)
        if (nx_stm32_av_overlap(request, sizeof(*request),
                                stream->slots[i].data,
                                stream->slots[i].capacity) ||
            nx_stm32_av_overlap(s, sizeof(*s), stream->slots[i].data,
                                stream->slots[i].capacity) ||
            nx_stm32_av_overlap(s->config, sizeof(*s->config),
                                stream->slots[i].data,
                                stream->slots[i].capacity))
            return NX_ERROR_INVALID;
    /* This bounded preflight reads the immutable native payload once. Hardware
     * performs every row transfer; no CPU copy or per-update service follows.
     */
    for (size_t row = 0U; row < view->rows; ++row) {
        uint32_t items[6] = {0U};
        for (size_t i = 0U; i < 6U; ++i)
            memcpy(&items[i], view->data + row * view->stride + i * width,
                   width);
        if (items[0] == UINT32_MAX || items[0] + 1U < s->config->period_ticks ||
            items[1] != 0U)
            return NX_ERROR_INVALID;
        for (unsigned i = 0U; i < 4U; ++i)
            if (((s->config->channels & (1U << i)) == 0U &&
                 items[i + 2U] != 0U) ||
                items[i + 2U] > items[0] + 1U)
                return NX_ERROR_INVALID;
    }
    return NX_SUCCESS;
}
static bool sequence_arm(nx_stm32_waveform_state_t* s) {
    const nx_stm32_waveform_config_t* c = s->config;
    const nx_stm32_av_dma_t* d = &c->dma;
    TIM_TypeDef* t = c->registers;
    s->dma_armed = true;
    nx_stm32_av_dma_ack(d, 0x3dU);
    bool ok = nx_stm32_av_write(&t->DCR, 11U | (5U << 8U), UINT32_MAX);
    ok = nx_stm32_av_write(&d->stream->PAR, (uint32_t)(uintptr_t)&t->DMAR,
                           UINT32_MAX) &&
         ok;
    ok = nx_stm32_av_write(&d->stream->M0AR,
                           (uint32_t)(uintptr_t)s->source.data, UINT32_MAX) &&
         ok;
    ok = nx_stm32_av_write(&d->stream->M1AR, 0U, UINT32_MAX) && ok;
    ok = nx_stm32_av_write(&d->stream->NDTR, (uint32_t)s->dma_length,
                           UINT32_MAX) &&
         ok;
    uint32_t width = advanced(c) ? DMA_SxCR_PSIZE_1 | DMA_SxCR_MSIZE_1
                                 : DMA_SxCR_PSIZE_0 | DMA_SxCR_MSIZE_0;
    uint32_t cr = ((uint32_t)d->channel << DMA_SxCR_CHSEL_Pos) | width |
                  DMA_SxCR_DIR_0 | DMA_SxCR_MINC | DMA_SxCR_PL_1 |
                  DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE;
    ok = nx_stm32_av_write(&d->stream->CR, cr, UINT32_MAX) && ok;
    ok = nx_stm32_av_write(&d->stream->FCR, 0U,
                           DMA_SxFCR_DMDIS | DMA_SxFCR_FEIE) &&
         ok;
    if (!ok)
        return false;
    s->dma_irq_owned = true;
    NX_AV_IRQ_PRIORITY(d->irq, c->common.priority);
    NX_AV_IRQ_CLEAR(d->irq);
    NX_AV_IRQ_ENABLE(d->irq);
    return NX_AV_IRQ_ENABLED(d->irq) != 0U &&
           nx_stm32_av_bits(&d->stream->CR, DMA_SxCR_EN, DMA_SxCR_EN);
}
static nx_result_t sequence_submit(void* context,
                                   nx_timer_sequence_request_t* request) {
    nx_stm32_waveform_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->initialized || s->closing ||
        s->config->kind != NX_STM32_WAVE_SEQUENCE ||
        !object_span(request, sizeof(*request),
                     _Alignof(nx_timer_sequence_request_t)) ||
        nx_stm32_av_overlap(request, sizeof(*request), s, sizeof(*s)) ||
        nx_stm32_av_overlap(request, sizeof(*request), s->config,
                            sizeof(*s->config)))
        return NX_ERROR_INVALID;
    if (s->active != NULL)
        return NX_ERROR_BUSY;
    if (!live(s) || !nx_stm32_av_timebase() ||
        g_nx_stm32_system.timer_error != NX_SUCCESS || s->fault ||
        s->quarantined || !s->shadow_valid || !invariant(s))
        return NX_ERROR_STATE;
    nx_time_us_t now = NX_AV_NOW();
    if (nx_request_state(&request->base) != NX_REQUEST_READY ||
        nx_deadline_expired(request->base.deadline, now))
        return NX_ERROR_INVALID;
    if (dma_foreign(&s->config->dma))
        return NX_ERROR_BUSY;
    nx_frame_dma_view_t view;
    nx_result_t result = sequence_validate(s, request, &view);
    if (result != NX_SUCCESS)
        return result;
    result = nx_request_admit(&request->base, NX_REQUEST_QUEUED);
    if (result != NX_SUCCESS)
        return result;
    s->active = request;
    s->source = view;
    s->bytes = view.span_bytes;
    s->dma_length = view.rows * 6U;
    s->remaining = s->dma_length;
    s->observed_at = now;
    s->time_epoch = nx_stm32_av_epoch();
    s->result = NX_SUCCESS;
    s->tail_phase = 0U;
    __atomic_store_n(&s->events, 0U, __ATOMIC_RELAXED);
    /* Software UG is issued by group_start before UPDATE DMA requests exist.
     * The initial idle cycles are explicit; every payload row follows through
     * hardware requests, even when service is delayed for many periods. */
    if (!sequence_arm(s) || group_start(s) != NX_SUCCESS ||
        !nx_stm32_av_bits(&s->config->registers->DIER, TIM_DIER_UDE,
                          TIM_DIER_UDE))
        s->result = NX_ERROR_IO;
    (void)nx_request_transition(&request->base, NX_REQUEST_ACTIVE);
    return NX_SUCCESS;
}
static bool sequence_detach(nx_stm32_waveform_state_t* s) {
    const nx_stm32_av_dma_t* d = &s->config->dma;
    const uint32_t sources = DMA_SxCR_EN | DMA_SxCR_TCIE | DMA_SxCR_HTIE |
                             DMA_SxCR_TEIE | DMA_SxCR_DMEIE;
    (void)nx_stm32_av_bits(&d->stream->CR, sources, 0U);
    (void)nx_stm32_av_bits(&d->stream->FCR, DMA_SxFCR_FEIE, 0U);
    for (uint32_t i = 0U; i < s->config->common.poll_limit; ++i) {
        if ((NX_AV_READ(&d->stream->CR) & sources) == 0U &&
            (NX_AV_READ(&d->stream->FCR) &
             (DMA_SxFCR_DMDIS | DMA_SxFCR_FEIE)) == 0U) {
            /* EN=0 completes the final bus transaction. Preserve its errors
             * before W1C, including an error racing TC or cancellation. */
            nx_stm32_waveform_dma_irq(s);
            return nx_stm32_av_irq_detach(d->irq);
        }
        NX_AV_POLL();
    }
    return false;
}
static bool sequence_idle_profile(nx_stm32_waveform_state_t* s) {
    const nx_stm32_waveform_config_t* c = s->config;
    TIM_TypeDef* t = c->registers;
    bool ok = nx_stm32_av_write(&t->CR1, base_cr1(c), UINT32_MAX);
    ok = nx_stm32_av_write(&t->ARR, c->period_ticks - 1U, UINT32_MAX) && ok;
    ok = nx_stm32_av_write(&t->CNT, 0U, UINT32_MAX) && ok;
    for (unsigned i = 0U; i < 4U; ++i)
        ok = nx_stm32_av_write(compare(t, i), 0U, UINT32_MAX) && ok;
    if (advanced(c))
        ok = nx_stm32_av_write(&t->RCR, 0U, UINT32_MAX) && ok;
    NX_AV_WRITE(&t->EGR, TIM_EGR_UG);
    NX_AV_WRITE(&t->SR, ~(uint32_t)TIM_SR_UIF);
    nx_arch_dsb();
    memset(s->duties, 0, sizeof(s->duties));
    return ok && (NX_AV_READ(&t->SR) & TIM_SR_UIF) == 0U && invariant(s);
}
static nx_result_t sequence_drain(nx_stm32_waveform_state_t* s) {
    nx_timer_sequence_request_t* request = s->active;
    const nx_stm32_waveform_config_t* c = s->config;
    TIM_TypeDef* t = c->registers;
    nx_request_state_t state = nx_request_state(&request->base);
    if (state == NX_REQUEST_QUEUED || state == NX_REQUEST_ACTIVE)
        (void)nx_request_transition(&request->base, NX_REQUEST_DRAINING);
    if (!nx_stm32_av_bits(&t->DIER, TIM_DIER_UDE, 0U))
        return request_quarantine(s, &request->base, NX_ERROR_BUSY);
    if (s->result != NX_SUCCESS && quiet(s) != NX_SUCCESS)
        return request_quarantine(s, &request->base, NX_ERROR_BUSY);
    if (!sequence_detach(s))
        return request_quarantine(s, &request->base, NX_ERROR_BUSY);
    if ((__atomic_load_n(&s->events, __ATOMIC_RELAXED) & WAVE_DMA_ERROR) !=
            0U &&
        s->result == NX_SUCCESS)
        s->result = NX_ERROR_IO;
    s->remaining = NX_AV_READ(&c->dma.stream->NDTR);
    if (s->remaining > s->dma_length)
        return request_quarantine(s, &request->base, NX_ERROR_STATE);
    if (s->result == NX_SUCCESS && s->remaining != 0U)
        s->result = NX_ERROR_IO;
    if (s->result == NX_SUCCESS && s->tail_phase == 0U) {
        /* TC completes the bus write, not shadow application. Clear-and-read
         * creates a fresh observation boundary; coalesced old UIF cannot stand
         * for either the final shadow update or its complete following period.
         * If scheduling is late, these observations merely add tail repeats. */
        NX_AV_WRITE(&t->SR, ~(uint32_t)TIM_SR_UIF);
        nx_arch_dsb();
        if ((NX_AV_READ(&t->SR) & TIM_SR_UIF) != 0U)
            return request_quarantine(s, &request->base, NX_ERROR_BUSY);
        s->tail_phase = 1U;
        return NX_ERROR_BUSY;
    }
    if (s->result == NX_SUCCESS && s->tail_phase < 3U) {
        if ((NX_AV_READ(&t->CR1) & TIM_CR1_CEN) == 0U) {
            s->result = NX_ERROR_IO;
        } else if ((NX_AV_READ(&t->SR) & TIM_SR_UIF) == 0U) {
            return NX_ERROR_BUSY;
        } else {
            NX_AV_WRITE(&t->SR, ~(uint32_t)TIM_SR_UIF);
            nx_arch_dsb();
            if ((NX_AV_READ(&t->SR) & TIM_SR_UIF) != 0U)
                return request_quarantine(s, &request->base, NX_ERROR_BUSY);
            ++s->tail_phase;
            if (s->tail_phase < 3U)
                return NX_ERROR_BUSY;
        }
    }
    if (quiet(s) != NX_SUCCESS || !nx_stm32_av_dma_addresses(&c->dma) ||
        !nx_stm32_av_write(&t->DCR, 0U, UINT32_MAX))
        return request_quarantine(s, &request->base, NX_ERROR_BUSY);
    if (!sequence_idle_profile(s)) {
        if (s->result == NX_SUCCESS)
            s->result = NX_ERROR_IO;
        return request_quarantine(s, &request->base, NX_ERROR_BUSY);
    }
    if (s->result == NX_SUCCESS &&
        nx_deadline_expired(request->base.deadline, NX_AV_NOW()))
        s->result = NX_ERROR_TIMEOUT;
    const nx_dma_drain_facts_t facts = {s->dma_length, s->remaining, true, true,
                                        true};
    nx_result_t released =
        nx_frame_dma_release(request->source->owner, request->source, &facts);
    if (released != NX_SUCCESS)
        return request_quarantine(s, &request->base, released);
    nx_result_t result = s->result;
    size_t transferred = result == NX_SUCCESS ? s->bytes : 0U;
    s->active = NULL;
    memset(&s->source, 0, sizeof(s->source));
    s->dma_armed = false;
    s->quarantined = false;
    s->shadow_valid = true;
    __atomic_store_n(&s->events, 0U, __ATOMIC_RELAXED);
    nx_request_settle(&request->base, result, transferred);
    return result;
}
static nx_result_t sequence_service(void* context) {
    nx_stm32_waveform_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->owned || s->config->kind != NX_STM32_WAVE_SEQUENCE)
        return NX_ERROR_STATE;
    if (s->active == NULL)
        return NX_SUCCESS;
    nx_timer_sequence_request_t* request = s->active;
    if (!live(s))
        return request_quarantine(s, &request->base, NX_ERROR_STATE);
    nx_time_us_t now = NX_AV_NOW();
    if (now < s->observed_at) {
        s->history_lost = true;
        return request_quarantine(s, &request->base, NX_ERROR_STATE);
    }
    s->observed_at = now;
    (void)observe_break(s);
    nx_stm32_waveform_dma_irq(s);
    uint32_t events = __atomic_load_n(&s->events, __ATOMIC_RELAXED);
    if ((s->fault || (events & WAVE_DMA_ERROR) != 0U) &&
        s->result == NX_SUCCESS)
        s->result = NX_ERROR_IO;
    if (nx_deadline_expired(request->base.deadline, now) &&
        s->result == NX_SUCCESS)
        s->result = NX_ERROR_TIMEOUT;
    if (s->result != NX_SUCCESS || (events & WAVE_DMA_DONE) != 0U)
        return sequence_drain(s);
    return NX_ERROR_BUSY;
}
static nx_result_t sequence_cancel(void* context,
                                   nx_timer_sequence_request_t* request) {
    nx_stm32_waveform_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || request == NULL || s->active != request ||
        s->config->kind != NX_STM32_WAVE_SEQUENCE)
        return NX_ERROR_STATE;
    if (s->result == NX_SUCCESS)
        s->result = NX_ERROR_CANCELLED;
    return sequence_service(s);
}
static nx_result_t sequence_stop(void* context) {
    nx_stm32_waveform_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->owned || s->config->kind != NX_STM32_WAVE_SEQUENCE)
        return NX_ERROR_STATE;
    if (s->active != NULL) {
        (void)sequence_cancel(s, s->active);
        if (s->active != NULL)
            return NX_ERROR_BUSY;
    }
    return quiet(s);
}
static nx_result_t sequence_state(const void* context,
                                  nx_timer_sequence_state_t* out) {
    nx_stm32_waveform_state_t* s = (nx_stm32_waveform_state_t*)context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->owned || s->config->kind != NX_STM32_WAVE_SEQUENCE ||
        !object_span(out, sizeof(*out), _Alignof(nx_timer_sequence_state_t)) ||
        nx_stm32_av_overlap(out, sizeof(*out), s, sizeof(*s)) ||
        nx_stm32_av_overlap(out, sizeof(*out), s->config, sizeof(*s->config)) ||
        (s->active != NULL &&
         sequence_overlap(s, s->active, out, sizeof(*out))))
        return NX_ERROR_INVALID;
    if (!live(s))
        return NX_ERROR_STATE;
    (void)observe_break(s);
    const nx_stm32_waveform_config_t* c = s->config;
    *out = (nx_timer_sequence_state_t){
        NX_TIMER_SEQUENCE_ARR_RCR_CCR1_CCR4,
        c->tick_hz,
        c->period_ticks,
        NEXUS_CORE_HZ,
        c->burst_budget_hclk_cycles,
        6U,
        65535U,
        advanced(c) ? 32U : 16U,
        c->channels,
        (NX_AV_READ(&c->registers->CR1) & TIM_CR1_CEN) != 0U,
        s->fault,
        s->quarantined};
    return NX_SUCCESS;
}
const nx_timer_sequence_ops_t nx_stm32_timer_sequence_ops = {
    sequence_submit, sequence_cancel, sequence_service, sequence_stop,
    sequence_state};
#endif
