/**
 * \file            ltdc.c
 * \brief           Shared-layer HAL scanout with verified normal retirement
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "graphics_checks.h"
#include "stm32f4_ltdc.h"
#include <stddef.h>
#include <string.h>

#define GFX_IRQS   ((uint32_t)(LTDC_IT_LI | LTDC_IT_FU | LTDC_IT_TE | LTDC_IT_RR))
#define GFX_ERRORS ((uint32_t)(LTDC_ISR_TERRIF | LTDC_ISR_FUIF))
#define GFX_CFGR                                                               \
    ((uint32_t)(RCC_CFGR_SW | RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE1 |  \
                RCC_CFGR_PPRE2))
#define GFX_PLL            ((uint32_t)(RCC_PLLCFGR_PLLSRC | RCC_PLLCFGR_PLLM))
#define GFX_SAI            ((uint32_t)(RCC_PLLSAICFGR_PLLSAIN | RCC_PLLSAICFGR_PLLSAIR))
#define GFX_DIV            ((uint32_t)RCC_DCKCFGR_PLLSAIDIVR)
#define GFX_EVENT_ERROR    UINT32_C(1)
#define GFX_EVENT_RELOAD   UINT32_C(2)
#define GFX_RETIRE_PENDING UINT8_C(4)
#define GFX_PROOF_COMMIT   UINT32_C(256)
#define GFX_PROOF_RETIRE   UINT32_C(512)
#define GFX_PROOFS         (GFX_PROOF_COMMIT | GFX_PROOF_RETIRE)

enum { GFX_IDLE, GFX_COMMIT, GFX_LIVE, GFX_RETIRE, GFX_JOIN };
static const IRQn_Type irqs[2] = {LTDC_IRQn, LTDC_ER_IRQn};

/** \brief One saved-mask guard never treats a DSB as DMA quiescence. */
static bool bits(volatile uint32_t* reg, uint32_t mask, uint32_t value) {
    NX_GFX_WRITE(reg, (NX_GFX_READ(reg) & ~mask) | value);
    nx_arch_dsb();
    return (NX_GFX_READ(reg) & mask) == value;
}
/** \brief Check immutable plan identity and ready domains before LTDC MMIO. */
static bool domains(const nx_stm32_ltdc_config_t* c) {
    if (!c->system->started || c->system->time_epoch == 0U ||
        c->system->timer_error != NX_SUCCESS || c->system->rcc != c->rcc ||
        c->system->flash == NULL || c->system->core_hz == 0U)
        return false;
    uint32_t cr = NX_GFX_READ(&c->rcc->CR);
    uint32_t cfgr = NX_GFX_READ(&c->rcc->CFGR);
    uint32_t source = (c->pll_source & RCC_PLLCFGR_PLLSRC) != 0U
                          ? (uint32_t)(RCC_CR_HSEON | RCC_CR_HSERDY)
                          : (uint32_t)(RCC_CR_HSION | RCC_CR_HSIRDY);
    uint32_t system = cfgr & RCC_CFGR_SWS;
    if (system == RCC_CFGR_SWS_PLL)
        source |= (uint32_t)(RCC_CR_PLLON | RCC_CR_PLLRDY);
    else if (system == RCC_CFGR_SWS_HSE)
        source |= (uint32_t)(RCC_CR_HSEON | RCC_CR_HSERDY);
    else if (system == RCC_CFGR_SWS_HSI)
        source |= (uint32_t)(RCC_CR_HSION | RCC_CR_HSIRDY);
    else
        return false;
    uint32_t ready = source | (uint32_t)(RCC_CR_PLLSAION | RCC_CR_PLLSAIRDY);
    return (cr & ready) == ready &&
           (NX_GFX_READ(&c->rcc->APB2RSTR) & RCC_APB2RSTR_LTDCRST) == 0U &&
           (cfgr & GFX_CFGR) == c->system_cfgr &&
           (NX_GFX_READ(&c->rcc->PLLCFGR) & GFX_PLL) == c->pll_source &&
           (NX_GFX_READ(&c->rcc->PLLSAICFGR) & GFX_SAI) == c->pllsai_cfgr &&
           (NX_GFX_READ(&c->rcc->DCKCFGR) & GFX_DIV) == c->pllsai_divr;
}
/** \brief Register access requires both the live System and our own gate. */
static bool clocks(const nx_stm32_ltdc_config_t* c) {
    return domains(c) &&
           (NX_GFX_READ(&c->rcc->APB2ENR) & RCC_APB2ENR_LTDCEN) != 0U;
}
/** \brief Keep the attempted gate receipt until its exact readback succeeds. */
static bool gate(nx_stm32_ltdc_state_t* s, bool enable) {
    if (!enable && !s->gate_owned)
        return true;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    volatile uint32_t* reg = &s->config->rcc->APB2ENR;
    uint32_t mask = (uint32_t)RCC_APB2ENR_LTDCEN;
    if (enable)
        s->gate_owned = true;
    NX_GFX_WRITE(reg, (NX_GFX_READ(reg) & ~mask) | (enable ? mask : 0U));
    nx_arch_dsb();
    bool result = (NX_GFX_READ(reg) & mask) == (enable ? mask : 0U);
    if (result && !enable)
        s->gate_owned = false;
    nx_arch_irq_restore(guard);
    return result;
}
/** \brief Read the active layer view, never the HAL shadow cache. */
static LTDC_Layer_TypeDef* active_layer(nx_stm32_ltdc_state_t* s,
                                        uint32_t layer) {
    return NX_GFX_ACTIVE_LAYER(&s->handle, layer);
}

/** \brief Find a selected readonly hardware-layer record, not a runtime name.
 */
static const nx_stm32_ltdc_layer_t* layer(const nx_stm32_ltdc_config_t* c,
                                          unsigned id) {
    for (size_t i = 0U; i < c->layer_count; ++i)
        if (c->layers[i].limits.layer == id)
            return &c->layers[i];
    return NULL;
}
/** \brief Selected physical layer bits are bounded by the controller's two IPs.
 */
static uint8_t selected(const nx_stm32_ltdc_config_t* c) {
    uint8_t value = 0U;
    for (size_t i = 0U; i < c->layer_count; ++i)
        value |= (uint8_t)(1U << c->layers[i].limits.layer);
    return value;
}
/** \brief Validate the exact route and the one Platform-owned snapshot span. */
static bool pins_valid(nx_stm32_ltdc_state_t* s,
                       const nx_stm32_ltdc_config_t* c) {
    if ((c->pin_count != 20U && c->pin_count != 22U && c->pin_count != 28U) ||
        !nx_graphics_span(c->pins, c->pin_count * sizeof(*c->pins),
                          _Alignof(nx_stm32_ltdc_pin_t)) ||
        !nx_graphics_span(c->snapshots, c->pin_count * sizeof(*c->snapshots),
                          _Alignof(uint16_t)) ||
        nx_graphics_overlap(s, sizeof(*s), c->pins,
                            c->pin_count * sizeof(*c->pins)) ||
        nx_graphics_overlap(s, sizeof(*s), c->snapshots,
                            c->pin_count * sizeof(*c->snapshots)) ||
        nx_graphics_overlap(c->snapshots, c->pin_count * sizeof(*c->snapshots),
                            c, sizeof(*c)) ||
        nx_graphics_overlap(c->snapshots, c->pin_count * sizeof(*c->snapshots),
                            c->system, sizeof(*c->system)) ||
        nx_graphics_overlap(c->snapshots, c->pin_count * sizeof(*c->snapshots),
                            c->pins, c->pin_count * sizeof(*c->pins)) ||
        nx_graphics_overlap(c->snapshots, c->pin_count * sizeof(*c->snapshots),
                            c->regions,
                            c->region_count * sizeof(*c->regions)) ||
        nx_graphics_overlap(c->snapshots, c->pin_count * sizeof(*c->snapshots),
                            c->layers, c->layer_count * sizeof(*c->layers)))
        return false;
    for (size_t i = 0U; i < c->pin_count; ++i) {
        const nx_stm32_ltdc_pin_t* p = &c->pins[i];
        if (p->bank > 8U || p->pin > 15U ||
            !nx_graphics_span(p->registers, sizeof(*p->registers),
                              _Alignof(GPIO_TypeDef)) ||
            nx_graphics_overlap(s, sizeof(*s), p->registers,
                                sizeof(*p->registers)))
            return false;
#ifndef NEXUS_STM32_MODEL
        if ((uintptr_t)p->registers !=
            (uintptr_t)GPIOA + (uintptr_t)p->bank * UINT32_C(0x400))
            return false;
#endif
        for (size_t j = 0U; j < i; ++j)
            if ((p->bank == c->pins[j].bank && p->pin == c->pins[j].pin) ||
                (p->bank != c->pins[j].bank &&
                 p->registers == c->pins[j].registers) ||
                (p->bank == c->pins[j].bank &&
                 p->registers != c->pins[j].registers))
                return false;
    }
    return true;
}
/** \brief Platform keeps each selected GPIO bus powered through final close. */
static bool pins_powered(const nx_stm32_ltdc_config_t* c) {
    uint32_t mask = 0U;
    for (size_t i = 0U; i < c->pin_count; ++i)
        mask |= UINT32_C(1) << c->pins[i].bank;
    return (NX_GFX_READ(&c->rcc->AHB1ENR) & mask) == mask &&
           (NX_GFX_READ(&c->rcc->AHB1RSTR) & mask) == 0U;
}
/** \brief Official AF setup writes the selected MODER last, under one guard. */
static bool pins_acquire(nx_stm32_ltdc_state_t* s) {
    const nx_stm32_ltdc_config_t* c = s->config;
    if (!pins_powered(c))
        return false;
    for (size_t i = 0U; i < c->pin_count; ++i) {
        const nx_stm32_ltdc_pin_t* p = &c->pins[i];
        uint32_t shift = (uint32_t)p->pin * 2U;
        uint32_t af_shift = ((uint32_t)p->pin % 8U) * 4U;
        GPIO_InitTypeDef init = {UINT32_C(1) << p->pin, GPIO_MODE_AF_PP,
                                 GPIO_NOPULL, GPIO_SPEED_FREQ_VERY_HIGH,
                                 GPIO_AF14_LTDC};
        nx_arch_irq_state_t guard = nx_arch_irq_save();
        s->pins_owned |= UINT32_C(1) << i;
        HAL_GPIO_Init(p->registers, &init);
        nx_arch_dsb();
        bool valid =
            ((NX_GFX_READ(&p->registers->MODER) >> shift) & 3U) == 2U &&
            ((NX_GFX_READ(&p->registers->AFR[p->pin / 8U]) >> af_shift) &
             15U) == 14U &&
            ((NX_GFX_READ(&p->registers->OSPEEDR) >> shift) & 3U) == 3U &&
            ((NX_GFX_READ(&p->registers->PUPDR) >> shift) & 3U) == 0U &&
            ((NX_GFX_READ(&p->registers->OTYPER) >> p->pin) & 1U) == 0U;
        nx_arch_irq_restore(guard);
        if (!valid)
            return false;
    }
    return true;
}
/** \brief Disconnect only proved leases; Platform restores original pads once.
 */
static bool pins_release(nx_stm32_ltdc_state_t* s) {
    const nx_stm32_ltdc_config_t* c = s->config;
    if (!pins_powered(c))
        return false;
    for (size_t i = c->pin_count; i != 0U;) {
        --i;
        uint32_t lease = UINT32_C(1) << i;
        if ((s->pins_owned & lease) == 0U)
            continue;
        const nx_stm32_ltdc_pin_t* p = &c->pins[i];
        uint32_t mask = UINT32_C(3) << ((uint32_t)p->pin * 2U);
        nx_arch_irq_state_t guard = nx_arch_irq_save();
        bool result = bits(&p->registers->MODER, mask, mask);
        if (result)
            s->pins_owned &= ~lease;
        nx_arch_irq_restore(guard);
        if (!result)
            return false;
    }
    return true;
}
/** \brief Reject malformed facts and foreign identity before any hardware use.
 */
static bool config_valid(nx_stm32_ltdc_state_t* s,
                         const nx_stm32_ltdc_config_t* c) {
    if (!nx_graphics_span(s, sizeof(*s), _Alignof(nx_stm32_ltdc_state_t)) ||
        !nx_graphics_span(c, sizeof(*c), _Alignof(nx_stm32_ltdc_config_t)) ||
        nx_graphics_overlap(s, sizeof(*s), c, sizeof(*c)) ||
        c->registers == NULL || c->rcc == NULL || c->region_count == 0U ||
        c->region_count > SIZE_MAX / sizeof(*c->regions) ||
        !nx_graphics_span(c->regions, c->region_count * sizeof(*c->regions),
                          _Alignof(nx_dma_memory_region_t)) ||
        !nx_graphics_span(c->system, sizeof(*c->system),
                          _Alignof(nx_stm32_system_t)) ||
        c->layer_count == 0U || c->layer_count > 2U ||
        !nx_graphics_span(c->layers, c->layer_count * sizeof(*c->layers),
                          _Alignof(nx_stm32_ltdc_layer_t)) ||
        nx_graphics_overlap(s, sizeof(*s), c->regions,
                            c->region_count * sizeof(*c->regions)) ||
        nx_graphics_overlap(s, sizeof(*s), c->system, sizeof(*c->system)) ||
        nx_graphics_overlap(s, sizeof(*s), c->layers,
                            c->layer_count * sizeof(*c->layers)) ||
        c->priority >= 16U || c->drain_us == 0U ||
        c->drain_us == NX_DEADLINE_NEVER || c->timing.TotalWidth > 4095U ||
        c->timing.TotalHeigh > 2047U ||
        c->timing.HorizontalSync > c->timing.AccumulatedHBP ||
        c->timing.VerticalSync > c->timing.AccumulatedVBP ||
        c->timing.AccumulatedHBP >= c->timing.AccumulatedActiveW ||
        c->timing.AccumulatedVBP >= c->timing.AccumulatedActiveH ||
        c->timing.AccumulatedActiveW >= c->timing.TotalWidth ||
        c->timing.AccumulatedActiveH >= c->timing.TotalHeigh)
        return false;
    if (c->graphics_memory != NULL) {
#if defined(NX_STM32_SDRAM_GRAPHICS)
        if (!nx_graphics_span(c->graphics_memory, sizeof(*c->graphics_memory),
                              _Alignof(nx_stm32_sdram_graphics_t)) ||
            nx_graphics_overlap(c->graphics_memory, sizeof(*c->graphics_memory),
                                s, sizeof(*s)) ||
            nx_graphics_overlap(c->graphics_memory, sizeof(*c->graphics_memory),
                                c, sizeof(*c)) ||
            nx_graphics_overlap(c->graphics_memory, sizeof(*c->graphics_memory),
                                c->system, sizeof(*c->system)) ||
            nx_graphics_overlap(c->graphics_memory, sizeof(*c->graphics_memory),
                                c->regions,
                                c->region_count * sizeof(*c->regions)) ||
            nx_graphics_overlap(c->graphics_memory, sizeof(*c->graphics_memory),
                                c->layers,
                                c->layer_count * sizeof(*c->layers)) ||
            nx_graphics_overlap(c->graphics_memory, sizeof(*c->graphics_memory),
                                c->snapshots,
                                c->pin_count * sizeof(*c->snapshots)) ||
            nx_graphics_overlap(c->graphics_memory, sizeof(*c->graphics_memory),
                                c->pins, c->pin_count * sizeof(*c->pins)))
            return false;
#else
        return false;
#endif
    }
#ifndef NEXUS_STM32_MODEL
    if (c->registers != LTDC || c->rcc != RCC)
        return false;
#endif
    if (!pins_valid(s, c))
        return false;
    size_t panel_w = c->timing.AccumulatedActiveW - c->timing.AccumulatedHBP;
    size_t panel_h = c->timing.AccumulatedActiveH - c->timing.AccumulatedVBP;
    uint8_t used = 0U;
    for (size_t i = 0U; i < c->layer_count; ++i) {
        const nx_stm32_ltdc_layer_t* record = &c->layers[i];
        const nx_display_constraints_t* limits = &record->limits;
        if (limits->layer > 1U ||
            (used & (uint8_t)(1U << limits->layer)) != 0U ||
            limits->width == 0U || limits->height == 0U ||
            record->x > panel_w || limits->width > panel_w - record->x ||
            record->y > panel_h || limits->height > panel_h - record->y ||
            limits->address_alignment != 4U || limits->pitch_alignment != 4U ||
            limits->maximum_pitch_bytes == 0U ||
            limits->maximum_pitch_bytes > 8191U || limits->formats == 0U ||
            (limits->formats & ~UINT32_C(3)) != 0U)
            return false;
        for (unsigned f = 0U; f < 2U; ++f) {
            size_t pixel = f == 0U ? 2U : 4U;
            if ((limits->formats & (UINT32_C(1) << f)) != 0U &&
                (limits->width > (8191U - 3U) / pixel ||
                 limits->width * pixel % 4U != 0U))
                return false;
        }
        used |= (uint8_t)(1U << limits->layer);
    }
    nx_irq_source_t source = {(int16_t)LTDC_ER_IRQn, c->priority, 0U};
    return nx_irq_wake_validate(NULL, &c->irq_policy, &source) == NX_SUCCESS;
}
/** \brief One static controller context owns every selected layer. */
static nx_stm32_ltdc_state_t* controller(void* context) {
    nx_stm32_ltdc_state_t* s = context;
    return nx_graphics_span(s, sizeof(*s), _Alignof(nx_stm32_ltdc_state_t)) &&
                   s->owned && s->initialized
               ? s
               : NULL;
}
/** \brief Complete immutable records and the concrete System never become
 * pixels. */
static bool aliases_owner(const nx_stm32_ltdc_state_t* s, const void* p,
                          size_t bytes) {
    const nx_stm32_ltdc_config_t* c = s->config;
    for (size_t i = 0U; i < c->pin_count; ++i)
        if (nx_graphics_overlap(p, bytes, c->pins[i].registers,
                                sizeof(*c->pins[i].registers)))
            return true;
#if defined(NX_STM32_SDRAM_GRAPHICS)
    if (c->graphics_memory != NULL &&
        nx_stm32_sdram_graphics_overlaps(c->graphics_memory, p, bytes))
        return true;
#endif
    return nx_graphics_overlap(p, bytes, c->pins,
                               c->pin_count * sizeof(*c->pins)) ||
           nx_graphics_overlap(p, bytes, c->snapshots,
                               c->pin_count * sizeof(*c->snapshots)) ||
           nx_graphics_overlap(p, bytes, s, sizeof(*s)) ||
           nx_graphics_overlap(p, bytes, c, sizeof(*c)) ||
           nx_graphics_overlap(p, bytes, c->layers,
                               c->layer_count * sizeof(*c->layers)) ||
           nx_graphics_overlap(p, bytes, c->regions,
                               c->region_count * sizeof(*c->regions)) ||
           nx_graphics_overlap(p, bytes, c->system, sizeof(*c->system)) ||
           nx_graphics_overlap(p, bytes, c->registers, sizeof(*c->registers)) ||
           nx_graphics_overlap(p, bytes, c->rcc, sizeof(*c->rcc)) ||
           (c->graphics_memory != NULL &&
            nx_graphics_overlap(p, bytes, c->graphics_memory,
                                sizeof(*c->graphics_memory))) ||
           (s->wake != NULL &&
            nx_graphics_overlap(p, bytes, s->wake, sizeof(*s->wake)));
}
/** \brief Every pair of live mutable descriptors and frame spans is
 * independent. */
static bool aliases_peer(const nx_display_request_t* a,
                         const nx_display_request_t* b) {
    if (b == NULL)
        return false;
    const nx_frame_dma_view_t* x = &a->operation.frame;
    const nx_frame_dma_view_t* y = &b->operation.frame;
    return nx_graphics_overlap(a, sizeof(*a), b, sizeof(*b)) ||
           nx_graphics_overlap(x->data, x->span_bytes, b, sizeof(*b)) ||
           nx_graphics_overlap(a, sizeof(*a), y->data, y->span_bytes) ||
           nx_graphics_overlap(x->data, x->span_bytes, y->data, y->span_bytes);
}
/** \brief Readonly queries exclude all selected layers and the global pending
 * loan. */
static bool aliases_borrowed(const nx_stm32_ltdc_state_t* s, const void* p,
                             size_t bytes) {
    const nx_display_request_t* loans[3] = {s->active[0], s->active[1],
                                            s->pending};
    for (unsigned i = 0U; i < 3U; ++i)
        if (loans[i] != NULL &&
            (nx_graphics_overlap(p, bytes, loans[i], sizeof(*loans[i])) ||
             nx_graphics_overlap(p, bytes, loans[i]->operation.frame.data,
                                 loans[i]->operation.frame.span_bytes)))
            return true;
    return false;
}

/** \brief Preserve the first request cause while its metadata is borrowed. */
static void cause(nx_display_request_t* request, nx_result_t result) {
    if (request != NULL && request->base.result == NX_SUCCESS)
        request->base.result = result;
}
/** \brief A failed proof retains all still reachable frame loans. */
static nx_result_t quarantine(nx_stm32_ltdc_state_t* s, nx_result_t result) {
    if (s->first_failure == NX_SUCCESS)
        s->first_failure = result;
    for (unsigned i = 0U; i < 2U; ++i)
        if (s->active[i] != NULL)
            (void)nx_request_transition(&s->active[i]->base,
                                        NX_REQUEST_QUARANTINED);
    if (s->pending != NULL)
        (void)nx_request_transition(&s->pending->base, NX_REQUEST_QUARANTINED);
    return result;
}
/** \brief A finite retry budget never saturates into implicit infinity. */
static void begin_drain(nx_stm32_ltdc_state_t* s, nx_time_us_t now) {
    if (!s->draining) {
        s->drain_deadline = 0U;
        (void)nx_deadline_finite_after(now, s->config->drain_us,
                                       &s->drain_deadline);
        s->draining = true;
    }
}
/** \brief No request is read after final release publication. */
static void settle(nx_display_request_t* request) {
    if (request != NULL) {
        nx_result_t result = request->base.result;
        nx_request_settle(&request->base, result, 0U);
    }
}
/** \brief Remove only this controller's reload source before old publication.
 */
static bool reload_join(nx_stm32_ltdc_state_t* s) {
    __HAL_LTDC_DISABLE_IT(&s->handle, (uint32_t)(LTDC_IT_RR | LTDC_IT_LI));
    __HAL_LTDC_CLEAR_FLAG(&s->handle, LTDC_FLAG_RR | LTDC_FLAG_LI);
    nx_arch_dsb();
    NX_GFX_IRQ_CLEAR(LTDC_IRQn);
    return (NX_GFX_READ(&s->config->registers->IER) &
            (LTDC_IT_RR | LTDC_IT_LI)) == 0U &&
           (NX_GFX_READ(&s->config->registers->ISR) &
            (LTDC_ISR_RRIF | LTDC_ISR_LIF)) == 0U &&
           NX_GFX_IRQ_PENDING(LTDC_IRQn) == 0U &&
           NX_GFX_IRQ_ACTIVE(LTDC_IRQn) == 0U;
}
/** \brief A retired frame can keep its owner while its publishers were joined.
 * \note Reuse restores both error/reload vectors under the original receipt;
 *       normal live replacement avoids redundant NVIC enable writes.
 */
static bool publishers_resume(nx_stm32_ltdc_state_t* s) {
    uint32_t errors = (uint32_t)(LTDC_IT_TE | LTDC_IT_FU);
    if ((NX_GFX_READ(&s->config->registers->IER) & errors) != errors &&
        !bits(&s->config->registers->IER, errors, errors))
        return false;
    for (unsigned i = 0U; i < 2U; ++i) {
        if (!s->vectors[i] ||
            NX_GFX_IRQ_PRIORITY(irqs[i]) != s->config->priority)
            return false;
        if (NX_GFX_IRQ_ENABLED(irqs[i]) == 0U)
            NX_GFX_IRQ_ENABLE(irqs[i]);
        nx_arch_dsb();
        if (NX_GFX_IRQ_ENABLED(irqs[i]) == 0U)
            return false;
    }
    return true;
}

/** \brief Exact active values include unchanged peers, not just the target
 * address. */
static bool latched(nx_stm32_ltdc_state_t* s, bool retiring, bool blank) {
    const nx_stm32_ltdc_config_t* c = s->config;
    if ((NX_GFX_READ(&c->registers->SRCR) & LTDC_SRCR_VBR) != 0U)
        return false;
    bool live = false;
    for (unsigned id = 0U; id < 2U; ++id) {
        LTDC_Layer_TypeDef* r = active_layer(s, id);
        const nx_stm32_ltdc_layer_t* record = layer(c, id);
        const nx_display_request_t* expected = s->active[id];
        if (retiring && (s->retire_mask & (uint8_t)(1U << id)) != 0U)
            expected = NULL;
        else if (!retiring && s->pending != NULL &&
                 s->pending->operation.layer == id)
            expected = s->pending;
        if (record == NULL || expected == NULL) {
            if ((NX_GFX_READ(&r->CR) & LTDC_LxCR_LEN) != 0U ||
                NX_GFX_READ(&r->CFBAR) != 0U)
                return false;
            continue;
        }
        live = true;
        const nx_display_operation_t* op = &expected->operation;
        uint32_t pixel_format = op->format == NX_IMAGE_RGB565
                                    ? LTDC_PIXEL_FORMAT_RGB565
                                    : LTDC_PIXEL_FORMAT_ARGB8888;
        uint32_t blend = record->pixel_alpha
                             ? (uint32_t)(LTDC_BLENDING_FACTOR1_PAxCA |
                                          LTDC_BLENDING_FACTOR2_PAxCA)
                             : (uint32_t)(LTDC_BLENDING_FACTOR1_CA |
                                          LTDC_BLENDING_FACTOR2_CA);
        uint32_t x0 = record->x + c->timing.AccumulatedHBP + 1U;
        uint32_t y0 = record->y + c->timing.AccumulatedVBP + 1U;
        uint32_t x1 = record->x + c->timing.AccumulatedHBP +
                      (uint32_t)record->limits.width;
        uint32_t y1 = record->y + c->timing.AccumulatedVBP +
                      (uint32_t)record->limits.height;
        if ((NX_GFX_READ(&r->CR) & LTDC_LxCR_LEN) == 0U ||
            NX_GFX_READ(&r->CFBAR) != (uint32_t)(uintptr_t)op->frame.data ||
            NX_GFX_READ(&r->PFCR) != pixel_format ||
            NX_GFX_READ(&r->CFBLR) != (((uint32_t)op->frame.stride << 16U) |
                                       ((uint32_t)op->frame.row_bytes + 3U)) ||
            NX_GFX_READ(&r->CFBLNR) != op->frame.rows ||
            NX_GFX_READ(&r->WHPCR) != (x0 | (x1 << 16U)) ||
            NX_GFX_READ(&r->WVPCR) != (y0 | (y1 << 16U)) ||
            NX_GFX_READ(&r->CACR) != record->alpha ||
            NX_GFX_READ(&r->BFCR) != blend)
            return false;
    }
    return (!live ||
            (NX_GFX_READ(&c->registers->GCR) & LTDC_GCR_LTDCEN) != 0U) &&
           (!blank ||
            (NX_GFX_READ(&c->registers->CDSR) & LTDC_CDSR_VSYNCS) != 0U);
}
/** \brief Only this physical stage and exact withdrawal mask may reuse
 * evidence. */
static bool proof_matches(nx_stm32_ltdc_state_t* s, bool retiring) {
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    uint32_t kind = retiring ? GFX_PROOF_RETIRE : GFX_PROOF_COMMIT;
    bool result = s->stage_generation != 0U &&
                  s->proof_generation == s->stage_generation &&
                  (s->events & kind) != 0U &&
                  (!retiring || s->proof_mask == s->retire_mask);
    nx_arch_irq_restore(guard);
    return result;
}
/** \brief Clear old evidence only after reload/line sources actually join. */
static bool stage_begin(nx_stm32_ltdc_state_t* s, uint8_t phase) {
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (s->stage_generation == UINT64_MAX) {
        nx_arch_irq_restore(guard);
        return false;
    }
    ++s->stage_generation;
    s->proof_generation = 0U;
    s->proof_mask = 0U;
    s->events &= ~(GFX_PROOFS | GFX_EVENT_RELOAD);
    s->phase = phase;
    nx_arch_irq_restore(guard);
    return true;
}
/** \brief A missed reload window retries at line0, without a worker or spin. */
static bool arm_probe(nx_stm32_ltdc_state_t* s) {
    bool retiring = s->phase == GFX_RETIRE;
    return proof_matches(s, retiring) ||
           (HAL_LTDC_ProgramLineEvent(&s->handle, 0U) == HAL_OK &&
            NX_GFX_READ(&s->config->registers->LIPCR) == 0U &&
            (NX_GFX_READ(&s->config->registers->IER) & LTDC_IT_LI) != 0U);
}
/** \brief Historical IRQ proof still requires unchanged current hardware
 * values. */
static bool ready(nx_stm32_ltdc_state_t* s, bool retiring) {
    return latched(s, retiring, !proof_matches(s, retiring));
}
/** \brief One shared shadow transaction stages only the selected layer. */
static bool stage_frame(nx_stm32_ltdc_state_t* s) {
    if (s->pending == NULL)
        return false;
    const nx_display_operation_t* op = &s->pending->operation;
    const nx_stm32_ltdc_layer_t* record = layer(s->config, op->layer);
    size_t pixel = nx_graphics_pixel_bytes(op->format);
    if (record == NULL || pixel == 0U)
        return false;
    LTDC_LayerCfgTypeDef config = {0};
    config.WindowX0 = record->x;
    config.WindowX1 = record->x + (uint32_t)record->limits.width;
    config.WindowY0 = record->y;
    config.WindowY1 = record->y + (uint32_t)record->limits.height;
    config.PixelFormat = op->format == NX_IMAGE_RGB565
                             ? LTDC_PIXEL_FORMAT_RGB565
                             : LTDC_PIXEL_FORMAT_ARGB8888;
    config.Alpha = record->alpha;
    config.BlendingFactor1 = record->pixel_alpha ? LTDC_BLENDING_FACTOR1_PAxCA
                                                 : LTDC_BLENDING_FACTOR1_CA;
    config.BlendingFactor2 = record->pixel_alpha ? LTDC_BLENDING_FACTOR2_PAxCA
                                                 : LTDC_BLENDING_FACTOR2_CA;
    config.FBStartAdress = (uint32_t)(uintptr_t)op->frame.data;
    config.ImageWidth = (uint32_t)(op->frame.stride / pixel);
    config.ImageHeight = (uint32_t)op->frame.rows;
    if (!reload_join(s) || !publishers_resume(s))
        return false;
    __HAL_LTDC_ENABLE(&s->handle);
    if ((NX_GFX_READ(&s->config->registers->GCR) & LTDC_GCR_LTDCEN) == 0U)
        return false;
    if (!stage_begin(s, GFX_COMMIT))
        return false;
    return HAL_LTDC_ConfigLayer_NoReload(&s->handle, &config, op->layer) ==
               HAL_OK &&
           HAL_LTDC_Reload(&s->handle, LTDC_RELOAD_VERTICAL_BLANKING) ==
               HAL_OK &&
           arm_probe(s);
}
/** \brief Selected retirement never overwrites an unfinished global shadow. */
static bool stage_retire(nx_stm32_ltdc_state_t* s) {
    if ((NX_GFX_READ(&s->config->registers->SRCR) & LTDC_SRCR_VBR) != 0U ||
        !reload_join(s))
        return false;
    for (uint32_t id = 0U; id < 2U; ++id) {
        if ((s->retire_mask & (uint8_t)(1U << id)) != 0U) {
            if (HAL_LTDC_SetAddress_NoReload(&s->handle, 0U, id) != HAL_OK)
                return false;
            __HAL_LTDC_LAYER_DISABLE(&s->handle, id);
        }
    }
    if (!stage_begin(s, GFX_RETIRE))
        return false;
    return HAL_LTDC_Reload(&s->handle, LTDC_RELOAD_VERTICAL_BLANKING) ==
               HAL_OK &&
           arm_probe(s);
}
/** \brief Every owned publisher joins; final controller close restores
 * priority. */
static bool vectors_join(nx_stm32_ltdc_state_t* s, bool final) {
    if (!bits(&s->config->registers->IER, GFX_IRQS, 0U))
        return false;
    if ((NX_GFX_READ(&s->config->registers->ISR) & GFX_ERRORS) != 0U)
        s->events |= GFX_EVENT_ERROR;
    __HAL_LTDC_CLEAR_FLAG(&s->handle, GFX_IRQS);
    for (unsigned i = 0U; i < 2U; ++i) {
        NX_GFX_IRQ_DISABLE(irqs[i]);
        NX_GFX_IRQ_CLEAR(irqs[i]);
    }
    nx_arch_dsb();
    nx_arch_isb();
    for (unsigned i = 0U; i < 2U; ++i)
        if (NX_GFX_IRQ_ENABLED(irqs[i]) != 0U ||
            NX_GFX_IRQ_PENDING(irqs[i]) != 0U ||
            NX_GFX_IRQ_ACTIVE(irqs[i]) != 0U)
            return false;
    if ((NX_GFX_READ(&s->config->registers->ISR) & GFX_IRQS) != 0U ||
        s->publishers != 0U || (s->events & GFX_EVENT_ERROR) != 0U)
        return false;
    if (final) {
        for (unsigned i = 0U; i < 2U; ++i) {
            if (s->vectors[i]) {
                NX_GFX_IRQ_SET_PRIORITY(irqs[i], s->priorities[i]);
                nx_arch_dsb();
                if (NX_GFX_IRQ_PRIORITY(irqs[i]) != s->priorities[i])
                    return false;
                s->vectors[i] = false;
            }
        }
    }
    return true;
}
/** \brief Only requests outside the selected retirement mask survive it. */
static bool survivors(const nx_stm32_ltdc_state_t* s) {
    for (unsigned id = 0U; id < 2U; ++id)
        if (s->active[id] != NULL &&
            (s->retire_mask & (uint8_t)(1U << id)) == 0U)
            return true;
    return s->pending != NULL && (s->retire_mask & GFX_RETIRE_PENDING) == 0U;
}
/** \brief Preserve each request's first terminal cause, including both layers.
 */
static void fail_all(nx_stm32_ltdc_state_t* s, nx_result_t result) {
    for (unsigned i = 0U; i < 2U; ++i)
        cause(s->active[i], result);
    cause(s->pending, result);
}
/** \brief Return a selected bank hold only after the engine has cold joined. */
static nx_result_t memory_detach(nx_stm32_ltdc_state_t* s) {
#if defined(NX_STM32_SDRAM_GRAPHICS)
    if (s->memory_attached) {
        nx_result_t result = nx_stm32_sdram_graphics_detach(
            s->config->graphics_memory, NX_STM32_SDRAM_GRAPHICS_LTDC, s);
        if (result != NX_SUCCESS)
            return result;
        s->memory_attached = false;
    }
#else
    (void)s;
#endif
    return NX_SUCCESS;
}
/** \brief Return frame/config loans only after the final bank hold joins. */
static nx_result_t finish_join(nx_stm32_ltdc_state_t* s, bool final) {
    if (final) {
        nx_result_t result = memory_detach(s);
        if (result != NX_SUCCESS)
            return result;
    }
    nx_display_request_t* done[3] = {NULL, NULL, NULL};
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    for (unsigned id = 0U; id < 2U; ++id) {
        if ((s->retire_mask & (uint8_t)(1U << id)) != 0U) {
            done[id] = s->active[id];
            s->active[id] = NULL;
            s->handle.LayerCfg[id].FBStartAdress = 0U;
        }
    }
    if (s->pending != NULL && (s->retire_mask & GFX_RETIRE_PENDING) != 0U) {
        done[2] = s->pending;
        s->pending = NULL;
    }
    s->phase =
        s->active[0] != NULL || s->active[1] != NULL ? GFX_LIVE : GFX_IDLE;
    s->retire_mask = 0U;
    s->draining = false;
    s->events &= ~GFX_EVENT_RELOAD;
    if (final) {
        s->wake = NULL;
        s->initialized = false;
        s->owned = false;
        s->config = NULL;
    }
    nx_arch_irq_restore(guard);
    for (unsigned i = 0U; i < 3U; ++i)
        settle(done[i]);
    return s->closing && !final ? NX_ERROR_BUSY : NX_SUCCESS;
}
/** \brief One bounded owner coordinates global reload and per-layer borrowing.
 */
static nx_result_t progress(nx_stm32_ltdc_state_t* s) {
    const nx_stm32_ltdc_config_t* c = s->config;
    if (s->closing && !s->gate_owned) {
        if (s->fault || !domains(c) || c->system->time_epoch != s->epoch ||
            (NX_GFX_READ(&c->rcc->APB2ENR) & RCC_APB2ENR_LTDCEN) != 0U)
            return quarantine(s, NX_ERROR_IO);
        return finish_join(s, true);
    }
    if (s->fault || !clocks(c) || c->system->time_epoch != s->epoch ||
        (NX_GFX_READ(&c->registers->ISR) & GFX_ERRORS) != 0U ||
        (s->events & GFX_EVENT_ERROR) != 0U) {
        s->fault = true;
        fail_all(s, NX_ERROR_IO);
        return quarantine(s, NX_ERROR_IO);
    }
    nx_time_us_t now = NX_GFX_NOW();
    if (now == NX_DEADLINE_NEVER) {
        s->fault = true;
        return quarantine(s, NX_ERROR_EXHAUSTED);
    }
    for (unsigned i = 0U; i < 2U; ++i)
        if (s->active[i] != NULL &&
            nx_deadline_expired(s->active[i]->base.deadline, now))
            cause(s->active[i], NX_ERROR_TIMEOUT);
    if (s->pending != NULL &&
        nx_deadline_expired(s->pending->base.deadline, now))
        cause(s->pending, NX_ERROR_TIMEOUT);
    if (s->pending != NULL && (s->phase == GFX_IDLE || s->phase == GFX_LIVE) &&
        s->pending->base.result != NX_SUCCESS) {
        nx_display_request_t* done = s->pending;
        s->pending = NULL;
        settle(done);
        if (!s->closing)
            return NX_SUCCESS;
    }
    if (s->phase == GFX_COMMIT && s->pending != NULL && !s->closing &&
        s->pending->base.result == NX_SUCCESS) {
        if (!ready(s, false)) {
            for (unsigned i = 0U; i < 2U; ++i)
                if (s->active[i] != NULL &&
                    s->active[i]->base.result != NX_SUCCESS)
                    begin_drain(s, now);
            if (s->draining && nx_deadline_expired(s->drain_deadline, now))
                return quarantine(s, NX_ERROR_BUSY);
            return NX_ERROR_BUSY;
        }
        if (!reload_join(s))
            return quarantine(s, NX_ERROR_BUSY);
        nx_arch_irq_state_t guard = nx_arch_irq_save();
        unsigned id = s->pending->operation.layer;
        nx_display_request_t* old = s->active[id];
        s->active[id] = s->pending;
        s->pending = NULL;
        s->events &= ~GFX_EVENT_RELOAD;
        s->phase = GFX_LIVE;
        bool retire_waiting = false;
        for (unsigned i = 0U; i < 2U; ++i)
            retire_waiting |=
                s->active[i] != NULL && s->active[i]->base.result != NX_SUCCESS;
        if (!retire_waiting)
            s->draining = false;
        nx_arch_irq_restore(guard);
        settle(old);
        return NX_SUCCESS;
    }
    uint8_t mask = s->closing ? selected(c) : 0U;
    for (unsigned id = 0U; id < 2U; ++id)
        if (s->active[id] != NULL && s->active[id]->base.result != NX_SUCCESS)
            mask |= (uint8_t)(1U << id);
    if (s->pending != NULL && s->pending->base.result != NX_SUCCESS)
        mask |= (uint8_t)(1U << s->pending->operation.layer);
    if (s->phase == GFX_RETIRE || s->phase == GFX_JOIN)
        mask = s->retire_mask;
    if (mask != 0U) {
        begin_drain(s, now);
        if (s->phase != GFX_RETIRE && s->phase != GFX_JOIN) {
            s->retire_mask = mask;
            if (s->pending != NULL &&
                (mask & (uint8_t)(1U << s->pending->operation.layer)) != 0U &&
                (s->phase == GFX_COMMIT ||
                 s->pending->base.result != NX_SUCCESS || s->closing))
                s->retire_mask |= GFX_RETIRE_PENDING;
            bool empty = s->pending == NULL && s->active[0] == NULL &&
                         s->active[1] == NULL;
            if (empty &&
                (NX_GFX_READ(&c->registers->GCR) & LTDC_GCR_LTDCEN) == 0U &&
                (NX_GFX_READ(&c->registers->SRCR) & LTDC_SRCR_VBR) == 0U)
                s->phase = GFX_JOIN;
            else if (stage_retire(s))
                return NX_ERROR_BUSY;
        }
        bool live = survivors(s);
        if (s->phase == GFX_RETIRE && ready(s, true)) {
            if (!live &&
                !bits(&c->registers->GCR, (uint32_t)LTDC_GCR_LTDCEN, 0U))
                return quarantine(s, NX_ERROR_BUSY);
            s->phase = GFX_JOIN;
        }
        if (s->phase == GFX_JOIN) {
            bool final = s->closing && !live;
            if ((!live &&
                 !bits(&c->registers->GCR, (uint32_t)LTDC_GCR_LTDCEN, 0U)) ||
                !vectors_join(s, final) || (live && !publishers_resume(s)))
                return quarantine(s, NX_ERROR_BUSY);
            if (final && (!pins_release(s) || !gate(s, false)))
                return quarantine(s, NX_ERROR_BUSY);
            return finish_join(s, final);
        }
        if (nx_deadline_expired(s->drain_deadline, now))
            return quarantine(s, NX_ERROR_BUSY);
        return NX_ERROR_BUSY;
    }
    if (s->pending != NULL) {
        if (!stage_frame(s)) {
            cause(s->pending, NX_ERROR_IO);
            return quarantine(s, NX_ERROR_IO);
        }
    }
    return NX_SUCCESS;
}
/** \brief One controller executor drives every selected layer. */
static nx_result_t service(void* context) {
    if (!NX_GFX_TASK())
        return NX_ERROR_CONTEXT;
    nx_stm32_ltdc_state_t* s = controller(context);
    if (s == NULL)
        return NX_ERROR_STATE;
    if (s->executing)
        return NX_ERROR_BUSY;
    s->executing = true;
    nx_result_t result = progress(s);
    s->executing = false;
    return result;
}
/** \brief Query returns the explicitly selected immutable layer window. */
static nx_result_t query(void* context, uint8_t id,
                         nx_display_constraints_t* out) {
    if (!NX_GFX_TASK())
        return NX_ERROR_CONTEXT;
    nx_stm32_ltdc_state_t* s = controller(context);
    if (s == NULL)
        return NX_ERROR_STATE;
    if (aliases_owner(s, out, sizeof(*out)) ||
        aliases_borrowed(s, out, sizeof(*out)))
        return NX_ERROR_INVALID;
    const nx_stm32_ltdc_layer_t* record = layer(s->config, id);
    if (record == NULL)
        return NX_ERROR_UNSUPPORTED;
    *out = record->limits;
    return NX_SUCCESS;
}
/** \brief Independent complete spans precede the unique global-pending borrow.
 */
static nx_result_t submit(void* context, nx_display_request_t* request) {
    if (!NX_GFX_TASK())
        return NX_ERROR_CONTEXT;
    nx_stm32_ltdc_state_t* s = controller(context);
    if (s == NULL || s->closing || s->fault)
        return NX_ERROR_STATE;
    if (s->executing || s->pending != NULL)
        return NX_ERROR_BUSY;
#if defined(NX_STM32_SDRAM_GRAPHICS)
    if (s->memory_attached) {
        nx_result_t result = nx_stm32_sdram_graphics_check(
            s->config->graphics_memory, NX_STM32_SDRAM_GRAPHICS_LTDC, s,
            s->config->system);
        if (result != NX_SUCCESS)
            return result;
    }
#endif
    if (s->stage_generation > UINT64_MAX - 4U)
        return NX_ERROR_EXHAUSTED;
    const nx_stm32_ltdc_config_t* c = s->config;
    const nx_display_operation_t* op = &request->operation;
    const nx_frame_dma_view_t* frame = &op->frame;
    if (aliases_owner(s, request, sizeof(*request)) ||
        aliases_owner(s, frame->data, frame->span_bytes) ||
        aliases_peer(request, s->active[0]) ||
        aliases_peer(request, s->active[1]))
        return NX_ERROR_INVALID;
    size_t pixel = nx_graphics_pixel_bytes(op->format);
    const nx_stm32_ltdc_layer_t* record = layer(c, op->layer);
    if (record == NULL)
        return NX_ERROR_UNSUPPORTED;
    const nx_display_constraints_t* limits = &record->limits;
    if (pixel == 0U ||
        (limits->formats & (UINT32_C(1) << (unsigned)op->format)) == 0U)
        return NX_ERROR_UNSUPPORTED;
    if (frame->rows != limits->height ||
        frame->row_bytes != limits->width * pixel || frame->stride % 4U != 0U ||
        frame->stride > limits->maximum_pitch_bytes ||
        (uintptr_t)frame->data > UINT32_MAX ||
        frame->span_bytes > UINT32_MAX - (uintptr_t)frame->data)
        return NX_ERROR_INVALID;
    nx_result_t result = nx_dma_buffer_validate(
        c->regions, c->region_count, frame->data, frame->span_bytes,
        limits->address_alignment, NX_DMA_TO_DEVICE);
    if (result != NX_SUCCESS)
        return result;
    nx_time_us_t now = NX_GFX_NOW();
    if (now == NX_DEADLINE_NEVER)
        return NX_ERROR_EXHAUSTED;
    if (nx_deadline_expired(request->base.deadline, now))
        return NX_ERROR_TIMEOUT;
    if (!clocks(c) || c->system->time_epoch != s->epoch)
        return NX_ERROR_STATE;
    result = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result == NX_SUCCESS)
        s->pending = request;
    return result;
}
/** \brief Cancellation targets an exact descriptor of this controller. */
static nx_result_t cancel(void* context, nx_display_request_t* request) {
    if (!NX_GFX_TASK())
        return NX_ERROR_CONTEXT;
    nx_stm32_ltdc_state_t* s = controller(context);
    if (s == NULL)
        return NX_ERROR_STATE;
    if (s->executing)
        return NX_ERROR_BUSY;
    if (s->active[0] != request && s->active[1] != request &&
        s->pending != request)
        return NX_ERROR_STATE;
    cause(request, NX_ERROR_CANCELLED);
    return NX_SUCCESS;
}
/** \brief Visible describes the selected active frame, never another
 * layer/pending. */
static nx_result_t snapshot(void* context, uint8_t id,
                            nx_display_snapshot_t* out) {
    if (!NX_GFX_TASK())
        return NX_ERROR_CONTEXT;
    nx_stm32_ltdc_state_t* s = controller(context);
    if (s == NULL)
        return NX_ERROR_STATE;
    if (aliases_owner(s, out, sizeof(*out)) ||
        aliases_borrowed(s, out, sizeof(*out)))
        return NX_ERROR_INVALID;
    if (layer(s->config, id) == NULL)
        return NX_ERROR_UNSUPPORTED;
    nx_display_request_t* active = s->active[id];
    bool visible = false;
    if (active != NULL && !s->fault && clocks(s->config)) {
        LTDC_Layer_TypeDef* r = active_layer(s, id);
        visible =
            (NX_GFX_READ(&r->CR) & LTDC_LxCR_LEN) != 0U &&
            NX_GFX_READ(&r->CFBAR) ==
                (uint32_t)(uintptr_t)active->operation.frame.data &&
            (NX_GFX_READ(&s->config->registers->GCR) & LTDC_GCR_LTDCEN) != 0U;
    }
    nx_display_snapshot_t value = {
        s->first_failure,
        active != NULL,
        visible,
        s->pending != NULL && s->pending->operation.layer == id,
        s->closing,
        s->fault || (active != NULL &&
                     nx_request_state(&active->base) == NX_REQUEST_QUARANTINED),
        id};
    *out = value;
    return NX_SUCCESS;
}
/** \brief One controller-wide hint policy applies to both owned IRQ publishers.
 */
static nx_result_t attach(void* context, const nx_irq_wake_t* wake,
                          uint8_t ceiling) {
    if (!NX_GFX_TASK())
        return NX_ERROR_CONTEXT;
    nx_stm32_ltdc_state_t* s = controller(context);
    if (s == NULL || s->closing)
        return NX_ERROR_STATE;
    if (s->executing || s->active[0] != NULL || s->active[1] != NULL ||
        s->pending != NULL || s->publishers != 0U)
        return NX_ERROR_BUSY;
    if (ceiling != s->config->irq_policy.syscall_ceiling ||
        (wake != NULL && aliases_owner(s, wake, sizeof(*wake))))
        return NX_ERROR_INVALID;
    for (unsigned i = 0U; i < 2U; ++i) {
        nx_irq_source_t source = {(int16_t)irqs[i],
                                  (uint8_t)NX_GFX_IRQ_PRIORITY(irqs[i]),
                                  (uint8_t)NX_GFX_IRQ_GROUP()};
        nx_result_t result =
            nx_irq_wake_validate(wake, &s->config->irq_policy, &source);
        if (result != NX_SUCCESS)
            return result;
    }
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (s->publishers != 0U) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_BUSY;
    }
    s->wake = wake;
    nx_arch_irq_restore(guard);
    return NX_SUCCESS;
}
/** \brief Whole-controller stop joins every selected layer before config
 * return. */
nx_result_t nx_stm32_ltdc_close(nx_stm32_ltdc_state_t* s) {
    if (!NX_GFX_TASK())
        return NX_ERROR_CONTEXT;
    if (s == NULL)
        return NX_ERROR_INVALID;
    if (s->executing || s->publishers != 0U)
        return NX_ERROR_BUSY;
    if (!s->owned)
        return NX_SUCCESS;
    if (!s->controller_touched) {
        if (!domains(s->config) || !pins_release(s) || !gate(s, false))
            return NX_ERROR_IO;
        nx_result_t detached = memory_detach(s);
        if (detached != NX_SUCCESS)
            return detached;
        s->config = NULL;
        s->owned = false;
        return NX_SUCCESS;
    }
    s->executing = true;
    s->closing = true;
    fail_all(s, NX_ERROR_CANCELLED);
    nx_result_t result = progress(s);
    s->executing = false;
    return result;
}
/** \brief One static controller face reads no returned config on repeated stop.
 */
static nx_result_t stop(void* context) {
    return nx_stm32_ltdc_close(context);
}
const nx_display_ops_t nx_stm32_ltdc_ops = {query,    submit, service, cancel,
                                            snapshot, stop,   attach};
/** \brief Pre-System static validation never retains references or effects. */
nx_result_t nx_stm32_ltdc_admit(nx_stm32_ltdc_state_t* s,
                                const nx_stm32_ltdc_config_t* c) {
    if (!NX_GFX_TASK())
        return NX_ERROR_CONTEXT;
    if (!config_valid(s, c) || s->owned || s->initialized || s->gate_owned)
        return NX_ERROR_INVALID;
    if (s->executing || s->publishers != 0U)
        return NX_ERROR_BUSY;
    if ((NX_GFX_READ(&c->rcc->APB2ENR) & RCC_APB2ENR_LTDCEN) != 0U ||
        (NX_GFX_READ(&c->rcc->APB2RSTR) & RCC_APB2RSTR_LTDCRST) != 0U)
        return NX_ERROR_BUSY;
    for (unsigned i = 0U; i < 2U; ++i)
        if (NX_GFX_IRQ_ENABLED(irqs[i]) != 0U ||
            NX_GFX_IRQ_PENDING(irqs[i]) != 0U ||
            NX_GFX_IRQ_ACTIVE(irqs[i]) != 0U)
            return NX_ERROR_BUSY;
    return NX_SUCCESS;
}
/** \brief Cold admission has no implicit scheduler, peer controller or reset
 * adopt. */
nx_result_t nx_stm32_ltdc_prepare(nx_stm32_ltdc_state_t* s,
                                  const nx_stm32_ltdc_config_t* c) {
    nx_result_t admitted = nx_stm32_ltdc_admit(s, c);
    if (admitted != NX_SUCCESS)
        return admitted;
    if (!domains(c) || !pins_powered(c))
        return NX_ERROR_STATE;
    if ((NX_GFX_READ(&c->rcc->APB2ENR) & RCC_APB2ENR_LTDCEN) != 0U)
        return NX_ERROR_BUSY;
    for (unsigned i = 0U; i < 2U; ++i)
        if (NX_GFX_IRQ_ENABLED(irqs[i]) != 0U ||
            NX_GFX_IRQ_PENDING(irqs[i]) != 0U ||
            NX_GFX_IRQ_ACTIVE(irqs[i]) != 0U)
            return NX_ERROR_BUSY;
    memset(s, 0, sizeof(*s));
    s->handle.Instance = c->registers;
    s->config = c;
    s->owned = true;
    s->epoch = c->system->time_epoch;
#if defined(NX_STM32_SDRAM_GRAPHICS)
    if (c->graphics_memory != NULL) {
        nx_result_t result = nx_stm32_sdram_graphics_attach(
            c->graphics_memory, NX_STM32_SDRAM_GRAPHICS_LTDC, s, c->system);
        if (result != NX_SUCCESS)
            return result;
        s->memory_attached = true;
    }
#endif
    if (!gate(s, true))
        return NX_ERROR_IO;
    s->controller_touched = true;
    if ((NX_GFX_READ(&c->registers->GCR) & LTDC_GCR_LTDCEN) != 0U ||
        NX_GFX_READ(&c->registers->IER) != 0U ||
        NX_GFX_READ(&c->registers->SRCR) != 0U ||
        (NX_GFX_READ(&active_layer(s, 0U)->CR) & LTDC_LxCR_LEN) != 0U ||
        (NX_GFX_READ(&active_layer(s, 1U)->CR) & LTDC_LxCR_LEN) != 0U) {
        /* Trusted reset was violated: do not adopt unknown scanout loans. */
        s->fault = true;
        return NX_ERROR_IO;
    }
    if (!pins_acquire(s))
        return NX_ERROR_IO;
    s->handle.Init = c->timing;
    s->handle.State = HAL_LTDC_STATE_READY;
    for (unsigned i = 0U; i < 2U; ++i)
        s->priorities[i] = NX_GFX_IRQ_PRIORITY(irqs[i]);
    if (HAL_LTDC_Init(&s->handle) != HAL_OK ||
        !bits(&c->registers->IER, GFX_IRQS, 0U))
        return NX_ERROR_IO;
    for (uint32_t id = 0U; id < 2U; ++id) {
        if (HAL_LTDC_SetAddress_NoReload(&s->handle, 0U, id) != HAL_OK)
            return NX_ERROR_IO;
        __HAL_LTDC_LAYER_DISABLE(&s->handle, id);
    }
    for (unsigned i = 0U; i < 2U; ++i) {
        s->vectors[i] = true;
        NX_GFX_IRQ_SET_PRIORITY(irqs[i], c->priority);
        NX_GFX_IRQ_ENABLE(irqs[i]);
        nx_arch_dsb();
        if (NX_GFX_IRQ_PRIORITY(irqs[i]) != c->priority ||
            NX_GFX_IRQ_ENABLED(irqs[i]) == 0U)
            return NX_ERROR_IO;
    }
    s->initialized = true;
    return bits(&c->registers->IER, (uint32_t)(LTDC_IT_TE | LTDC_IT_FU),
                (uint32_t)(LTDC_IT_TE | LTDC_IT_FU))
               ? NX_SUCCESS
               : NX_ERROR_IO;
}

/** \brief Callback context comes from its exact embedded HAL Handle. */
static nx_stm32_ltdc_state_t* owner(LTDC_HandleTypeDef* handle) {
    return (nx_stm32_ltdc_state_t*)((uint8_t*)handle -
                                    offsetof(nx_stm32_ltdc_state_t, handle));
}
/** \brief Hint capture is independent of request storage and publication. */
static void notify(nx_stm32_ltdc_state_t* s, uint32_t event) {
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    s->events |= event;
    const nx_irq_wake_t* wake = s->wake;
    if (wake != NULL && s->publishers != UINT32_MAX) {
        ++s->publishers;
        nx_arch_irq_restore(guard);
        (void)nx_irq_wake_signal(wake);
        guard = nx_arch_irq_save();
        --s->publishers;
    }
    nx_arch_irq_restore(guard);
}
/** \brief Sample the retirement-critical memory view, with blank status last.
 * \note One owner prevents another configuration pulse during the sample.
 *       Geometry is rechecked by Task before consuming this historical fact.
 */
static bool memory_proof(nx_stm32_ltdc_state_t* s, bool retiring) {
    if ((NX_GFX_READ(&s->config->registers->SRCR) & LTDC_SRCR_VBR) != 0U)
        return false;
    bool live = false;
    for (unsigned id = 0U; id < 2U; ++id) {
        const nx_display_request_t* expected = s->active[id];
        if (retiring && (s->retire_mask & (uint8_t)(1U << id)) != 0U)
            expected = NULL;
        else if (!retiring && s->pending != NULL &&
                 s->pending->operation.layer == id)
            expected = s->pending;
        LTDC_Layer_TypeDef* r = active_layer(s, id);
        uint32_t address =
            expected != NULL
                ? (uint32_t)(uintptr_t)expected->operation.frame.data
                : 0U;
        uint32_t enabled = expected != NULL ? (uint32_t)LTDC_LxCR_LEN : 0U;
        if (NX_GFX_READ(&r->CFBAR) != address ||
            (NX_GFX_READ(&r->CR) & LTDC_LxCR_LEN) != enabled)
            return false;
        live |= expected != NULL;
    }
    return (!live || (NX_GFX_READ(&s->config->registers->GCR) &
                      LTDC_GCR_LTDCEN) != 0U) &&
           (NX_GFX_READ(&s->config->registers->CDSR) & LTDC_CDSR_VSYNCS) != 0U;
}
/** \brief Only IRQ-observed hardware facts authorize later Task retirement. */
static void observe_proof(nx_stm32_ltdc_state_t* s) {
    bool retiring = s->phase == GFX_RETIRE;
    if ((s->phase != GFX_COMMIT && !retiring) ||
        (!retiring && s->pending == NULL) || s->stage_generation == 0U) {
        notify(s, GFX_EVENT_RELOAD);
        return;
    }
    const nx_stm32_ltdc_config_t* c = s->config;
    if (s->fault || !clocks(c) || c->system->time_epoch != s->epoch ||
        (s->events & GFX_EVENT_ERROR) != 0U) {
        notify(s, GFX_EVENT_ERROR);
        return;
    }
    uint32_t event = GFX_EVENT_RELOAD;
    if (proof_matches(s, retiring)) {
        __HAL_LTDC_DISABLE_IT(&s->handle, (uint32_t)LTDC_IT_LI);
        notify(s, event);
        return;
    }
    if (memory_proof(s, retiring) && c->system->started &&
        c->system->time_epoch == s->epoch) {
        nx_arch_irq_state_t guard = nx_arch_irq_save();
        s->proof_generation = s->stage_generation;
        s->proof_mask = retiring ? s->retire_mask : 0U;
        s->events |= retiring ? GFX_PROOF_RETIRE : GFX_PROOF_COMMIT;
        nx_arch_irq_restore(guard);
        __HAL_LTDC_DISABLE_IT(&s->handle, (uint32_t)LTDC_IT_LI);
        nx_arch_dsb();
    } else if (!arm_probe(s)) {
        event |= GFX_EVENT_ERROR;
    }
    notify(s, event);
}
/** \brief IRQ completion only latches hardware commit, never frame settlement.
 */
void HAL_LTDC_ReloadEventCallback(LTDC_HandleTypeDef* handle) {
    observe_proof(owner(handle));
}
/** \brief A missed reload pulse gets one hardware line probe per frame. */
void HAL_LTDC_LineEventCallback(LTDC_HandleTypeDef* handle) {
    observe_proof(owner(handle));
}
/** \brief First actual bus/FIFO failure remains visible beside reload events.
 */
void HAL_LTDC_ErrorCallback(LTDC_HandleTypeDef* handle) {
    notify(owner(handle), GFX_EVENT_ERROR);
}
/** \brief A late handler reads no returned config after successful close. */
void nx_stm32_ltdc_irq(nx_stm32_ltdc_state_t* s) {
    if (s != NULL && s->owned && s->initialized) {
        if (!clocks(s->config) || s->config->system->time_epoch != s->epoch) {
            s->fault = true;
            for (unsigned i = 0U; i < 2U; ++i)
                NX_GFX_IRQ_DISABLE(irqs[i]);
            notify(s, GFX_EVENT_ERROR);
        } else {
            HAL_LTDC_IRQHandler(&s->handle);
        }
    }
    nx_arch_dsb();
}
