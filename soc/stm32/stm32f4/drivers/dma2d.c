/**
 * \file            dma2d.c
 * \brief           Official HAL image tasks with bounded memory-loan withdrawal
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "graphics_checks.h"
#include "stm32f4_dma2d.h"
#include <stddef.h>
#include <string.h>

#define IMAGE_IRQS                                                             \
    ((uint32_t)(DMA2D_CR_TEIE | DMA2D_CR_TCIE | DMA2D_CR_TWIE |                \
                DMA2D_CR_CAEIE | DMA2D_CR_CTCIE | DMA2D_CR_CEIE))
#define IMAGE_FLAGS    UINT32_C(63)
#define IMAGE_COMPLETE UINT32_C(1)
#define IMAGE_ERROR    UINT32_C(2)
#define IMAGE_ERRORS                                                           \
    ((uint32_t)(DMA2D_ISR_TEIF | DMA2D_ISR_CEIF | DMA2D_ISR_CAEIF))
#define IMAGE_CFGR                                                             \
    ((uint32_t)(RCC_CFGR_SW | RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE1 |  \
                RCC_CFGR_PPRE2))

/** \brief Read actual System/AHB ownership before peripheral MMIO. */
static bool domains(const nx_stm32_image_config_t* c) {
    if (!c->system->started || c->system->time_epoch == 0U ||
        c->system->timer_error != NX_SUCCESS || c->system->rcc != c->rcc ||
        c->system->flash == NULL || c->system->core_hz == 0U)
        return false;
    uint32_t cfgr = NX_IMG_READ(&c->rcc->CFGR);
    uint32_t required;
    switch (cfgr & RCC_CFGR_SWS) {
        case RCC_CFGR_SWS_HSI:
            required = (uint32_t)(RCC_CR_HSION | RCC_CR_HSIRDY);
            break;
        case RCC_CFGR_SWS_HSE:
            required = (uint32_t)(RCC_CR_HSEON | RCC_CR_HSERDY);
            break;
        case RCC_CFGR_SWS_PLL:
            required = (uint32_t)(RCC_CR_PLLON | RCC_CR_PLLRDY);
            break;
        default:
            return false;
    }
    return (cfgr & IMAGE_CFGR) == c->system_cfgr &&
           (NX_IMG_READ(&c->rcc->CR) & required) == required &&
           (NX_IMG_READ(&c->rcc->AHB1RSTR) & RCC_AHB1RSTR_DMA2DRST) == 0U;
}
/** \brief Register access requires both the live System and our own gate. */
static bool clocks(const nx_stm32_image_config_t* c) {
    return domains(c) &&
           (NX_IMG_READ(&c->rcc->AHB1ENR) & RCC_AHB1ENR_DMA2DEN) != 0U;
}
/** \brief Keep the attempted gate receipt until its exact readback succeeds. */
static bool gate(nx_stm32_image_state_t* s, bool enable) {
    if (!enable && !s->gate_owned)
        return true;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    volatile uint32_t* reg = &s->config->rcc->AHB1ENR;
    uint32_t mask = (uint32_t)RCC_AHB1ENR_DMA2DEN;
    if (enable)
        s->gate_owned = true;
    NX_IMG_WRITE(reg, (NX_IMG_READ(reg) & ~mask) | (enable ? mask : 0U));
    nx_arch_dsb();
    bool result = (NX_IMG_READ(reg) & mask) == (enable ? mask : 0U);
    if (result && !enable)
        s->gate_owned = false;
    nx_arch_irq_restore(guard);
    return result;
}
/** \brief Admission validates all static extents before taking responsibility.
 */
static bool config_valid(nx_stm32_image_state_t* s,
                         const nx_stm32_image_config_t* c) {
    if (!nx_graphics_span(s, sizeof(*s), _Alignof(nx_stm32_image_state_t)) ||
        !nx_graphics_span(c, sizeof(*c), _Alignof(nx_stm32_image_config_t)) ||
        nx_graphics_overlap(s, sizeof(*s), c, sizeof(*c)) ||
        c->registers == NULL || c->rcc == NULL || c->region_count == 0U ||
        c->region_count > SIZE_MAX / sizeof(*c->regions) ||
        !nx_graphics_span(c->regions, c->region_count * sizeof(*c->regions),
                          _Alignof(nx_dma_memory_region_t)) ||
        !nx_graphics_span(c->system, sizeof(*c->system),
                          _Alignof(nx_stm32_system_t)) ||
        nx_graphics_overlap(s, sizeof(*s), c->regions,
                            c->region_count * sizeof(*c->regions)) ||
        nx_graphics_overlap(s, sizeof(*s), c->system, sizeof(*c->system)) ||
        c->drain_us == 0U || c->drain_us == NX_DEADLINE_NEVER ||
        c->priority >= 16U)
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
                                c->region_count * sizeof(*c->regions)))
            return false;
#else
        return false;
#endif
    }
#ifndef NEXUS_STM32_MODEL
    if (c->registers != DMA2D || c->rcc != RCC)
        return false;
#endif
    nx_irq_source_t source = {(int16_t)DMA2D_IRQn, c->priority, 0U};
    return nx_irq_wake_validate(NULL, &c->irq_policy, &source) == NX_SUCCESS;
}
/** \brief Full metadata/MMIO/resource extents cannot become borrowed pixels. */
static bool aliases_owner(const nx_stm32_image_state_t* s, const void* p,
                          size_t n) {
    const nx_stm32_image_config_t* c = s->config;
#if defined(NX_STM32_SDRAM_GRAPHICS)
    if (c->graphics_memory != NULL &&
        nx_stm32_sdram_graphics_overlaps(c->graphics_memory, p, n))
        return true;
#endif
    return nx_graphics_overlap(p, n, s, sizeof(*s)) ||
           nx_graphics_overlap(p, n, c, sizeof(*c)) ||
           nx_graphics_overlap(p, n, c->regions,
                               c->region_count * sizeof(*c->regions)) ||
           nx_graphics_overlap(p, n, c->system, sizeof(*c->system)) ||
           nx_graphics_overlap(p, n, c->registers, sizeof(*c->registers)) ||
           nx_graphics_overlap(p, n, c->rcc, sizeof(*c->rcc)) ||
           (c->graphics_memory != NULL &&
            nx_graphics_overlap(p, n, c->graphics_memory,
                                sizeof(*c->graphics_memory))) ||
           (s->wake != NULL &&
            nx_graphics_overlap(p, n, s->wake, sizeof(*s->wake)));
}
/** \brief Queries cannot overwrite descriptors or any currently borrowed span.
 */
static bool aliases_loan(const nx_stm32_image_state_t* s, const void* p,
                         size_t n) {
    const nx_image_request_t* r = s->active;
    if (r == NULL)
        return false;
    const nx_image_operation_t* op = &r->operation;
    return nx_graphics_overlap(p, n, r, sizeof(*r)) ||
           nx_graphics_overlap(p, n, op->destination.data,
                               op->destination.capacity) ||
           (op->kind != NX_IMAGE_FILL &&
            nx_graphics_overlap(p, n, op->foreground.data,
                                op->foreground.capacity)) ||
           (op->kind == NX_IMAGE_BLEND &&
            nx_graphics_overlap(p, n, op->background.data,
                                op->background.capacity));
}
/** \brief Only direct color formats are admitted; no fallback or CLUT load. */
static bool supported(nx_image_kind_t kind, nx_image_format_t foreground,
                      nx_image_format_t background,
                      nx_image_format_t destination) {
    return nx_graphics_pixel_bytes(destination) != 0U &&
           (kind == NX_IMAGE_FILL ||
            (kind == NX_IMAGE_COPY && foreground == destination) ||
            (kind == NX_IMAGE_CONVERT &&
             nx_graphics_pixel_bytes(foreground) != 0U) ||
            (kind == NX_IMAGE_BLEND &&
             nx_graphics_pixel_bytes(foreground) != 0U &&
             nx_graphics_pixel_bytes(background) != 0U));
}
/** \brief Query never changes hardware or takes a loan. */
static nx_result_t query(void* context, nx_image_kind_t kind,
                         nx_image_format_t foreground,
                         nx_image_format_t background,
                         nx_image_format_t destination,
                         nx_image_constraints_t* out) {
    nx_stm32_image_state_t* s = context;
    if (!NX_IMG_TASK())
        return NX_ERROR_CONTEXT;
    if (!s->initialized || s->closing)
        return NX_ERROR_STATE;
    if (aliases_owner(s, out, sizeof(*out)) ||
        aliases_loan(s, out, sizeof(*out)))
        return NX_ERROR_INVALID;
    if (!supported(kind, foreground, background, destination))
        return NX_ERROR_UNSUPPORTED;
    nx_image_constraints_t value = {4U, 4U, 16383U, 65535U, 16383U};
    *out = value;
    return NX_SUCCESS;
}
/** \brief Read hardware errors before their W1C, even if no ISR ran. */
static void errors(nx_stm32_image_state_t* s) {
    if ((NX_IMG_READ(&s->config->registers->ISR) & IMAGE_ERRORS) != 0U)
        s->events |= IMAGE_ERROR;
}
/** \brief Actual sources, vector state and addresses all participate in join.
 */
static bool withdraw(nx_stm32_image_state_t* s) {
    DMA2D_TypeDef* r = s->config->registers;
    __HAL_DMA2D_DISABLE_IT(&s->handle, IMAGE_IRQS);
    NX_IMG_IRQ_DISABLE();
    /* An in-flight AHB error can retire START before its IRQ is dispatched. */
    errors(s);
    __HAL_DMA2D_CLEAR_FLAG(&s->handle, IMAGE_FLAGS);
    NX_IMG_IRQ_CLEAR();
    nx_arch_dsb();
    if ((NX_IMG_READ(&r->CR) & (IMAGE_IRQS | DMA2D_CR_START | DMA2D_CR_ABORT |
                                DMA2D_CR_SUSP)) != 0U ||
        (NX_IMG_READ(&r->FGPFCCR) & DMA2D_FGPFCCR_START) != 0U ||
        (NX_IMG_READ(&r->BGPFCCR) & DMA2D_BGPFCCR_START) != 0U ||
        NX_IMG_IRQ_ENABLED() != 0U || NX_IMG_IRQ_PENDING() != 0U ||
        NX_IMG_IRQ_ACTIVE() != 0U || s->publishers != 0U ||
        (NX_IMG_READ(&r->ISR) & IMAGE_FLAGS) != 0U)
        return false;
    NX_IMG_WRITE(&r->FGMAR, 0U);
    NX_IMG_WRITE(&r->BGMAR, 0U);
    NX_IMG_WRITE(&r->OMAR, 0U);
    NX_IMG_WRITE(&r->FGCMAR, 0U);
    NX_IMG_WRITE(&r->BGCMAR, 0U);
    nx_arch_dsb();
    return NX_IMG_READ(&r->FGMAR) == 0U && NX_IMG_READ(&r->BGMAR) == 0U &&
           NX_IMG_READ(&r->OMAR) == 0U && NX_IMG_READ(&r->FGCMAR) == 0U &&
           NX_IMG_READ(&r->BGCMAR) == 0U;
}
/** \brief Snapshot a terminal intent once; an unrepresentable grace is finite0.
 */
static void drain(nx_stm32_image_state_t* s, nx_result_t cause,
                  nx_time_us_t now) {
    if (!s->draining) {
        s->active->base.result = cause;
        s->draining = true;
        s->drain_deadline = 0U;
        (void)nx_deadline_finite_after(now, s->config->drain_us,
                                       &s->drain_deadline);
        (void)nx_request_transition(&s->active->base, NX_REQUEST_DRAINING);
    }
}
/** \brief Control intent cannot replace an already observed complete/error. */
static void cancel_intent(nx_stm32_image_state_t* s) {
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    nx_result_t result = NX_ERROR_CANCELLED;
    if (!s->clock_fault && clocks(s->config))
        errors(s);
    if ((s->events & IMAGE_ERROR) != 0U)
        result = NX_ERROR_IO;
    else if ((s->events & IMAGE_COMPLETE) != 0U && !s->clock_fault &&
             clocks(s->config) &&
             (NX_IMG_READ(&s->config->registers->CR) & DMA2D_CR_START) == 0U)
        result = NX_SUCCESS;
    drain(s, result, NX_IMG_NOW());
    nx_arch_irq_restore(guard);
}
/** \brief Failed withdrawal never changes ownership into permission to free. */
static nx_result_t retain(nx_stm32_image_state_t* s, nx_result_t result,
                          nx_time_us_t now) {
    if (s->active != NULL &&
        (result == NX_ERROR_IO ||
         (s->draining && nx_deadline_expired(s->drain_deadline, now))))
        (void)nx_request_transition(&s->active->base, NX_REQUEST_QUARANTINED);
    return result;
}
/** \brief Return a selected bank hold only after the engine has cold joined. */
static nx_result_t memory_detach(nx_stm32_image_state_t* s) {
#if defined(NX_STM32_SDRAM_GRAPHICS)
    if (s->memory_attached) {
        nx_result_t result = nx_stm32_sdram_graphics_detach(
            s->config->graphics_memory, NX_STM32_SDRAM_GRAPHICS_DMA2D, s);
        if (result != NX_SUCCESS)
            return result;
        s->memory_attached = false;
    }
#else
    (void)s;
#endif
    return NX_SUCCESS;
}
/** \brief Keep request/config storage until even the selected bank hold joins.
 */
static nx_result_t finish(nx_stm32_image_state_t* s) {
    if (s->closing) {
        nx_result_t result = memory_detach(s);
        if (result != NX_SUCCESS)
            return result;
    }
    nx_image_request_t* completed = s->active;
    nx_result_t result =
        completed != NULL ? completed->base.result : NX_SUCCESS;
    if (result == NX_SUCCESS && (s->events & IMAGE_ERROR) != 0U)
        result = NX_ERROR_IO;
    size_t bytes = completed != NULL && result == NX_SUCCESS
                       ? completed->operation.width *
                             completed->operation.height *
                             nx_graphics_pixel_bytes(
                                 completed->operation.destination.format)
                       : 0U;
    s->active = NULL;
    s->draining = false;
    s->events = 0U;
    s->handle.State = HAL_DMA2D_STATE_READY;
    s->handle.Lock = HAL_UNLOCKED;
    if (s->closing) {
        s->wake = NULL;
        s->initialized = false;
        s->owned = false;
        s->config = NULL;
    }
    if (completed != NULL)
        nx_request_settle(&completed->base, result, bytes);
    return NX_SUCCESS;
}
/** \brief One nonblocking hardware step; HAL_READY is never a join proof. */
static nx_result_t progress(nx_stm32_image_state_t* s) {
    const nx_stm32_image_config_t* c = s->config;
    nx_time_us_t now = NX_IMG_NOW();
    bool joined = s->closing && !s->gate_owned;
    if (s->clock_fault || !(joined ? domains(c) : clocks(c)) ||
        c->system->time_epoch != s->epoch || now == NX_DEADLINE_NEVER) {
        s->clock_fault = true;
        return retain(s, NX_ERROR_IO, now);
    }
    if (joined) {
        if ((NX_IMG_READ(&c->rcc->AHB1ENR) & RCC_AHB1ENR_DMA2DEN) != 0U)
            return retain(s, NX_ERROR_IO, now);
        return finish(s);
    }
    if (s->active != NULL) {
        errors(s);
        if (!s->draining) {
            uint32_t events = s->events;
            if ((events & IMAGE_ERROR) != 0U)
                drain(s, NX_ERROR_IO, now);
            else if ((events & IMAGE_COMPLETE) != 0U &&
                     (NX_IMG_READ(&c->registers->CR) & DMA2D_CR_START) == 0U)
                drain(s, NX_SUCCESS, now);
            else if (nx_deadline_expired(s->active->base.deadline, now))
                drain(s, NX_ERROR_TIMEOUT, now);
            else
                return (events & IMAGE_COMPLETE) != 0U ? NX_ERROR_BUSY
                                                       : NX_SUCCESS;
        }
        if ((NX_IMG_READ(&c->registers->CR) & DMA2D_CR_START) != 0U) {
            /* HAL_Abort's HAL-tick spin cannot honor this finite domain. */
            SET_BIT(c->registers->CR, DMA2D_CR_ABORT);
            nx_arch_dsb();
            if ((NX_IMG_READ(&c->registers->CR) & DMA2D_CR_START) != 0U)
                return retain(s, NX_ERROR_BUSY, now);
        }
    }
    if (!withdraw(s))
        return retain(
            s, (s->events & IMAGE_ERROR) != 0U ? NX_ERROR_IO : NX_ERROR_BUSY,
            now);
    if (s->closing && s->vector_owned) {
        NX_IMG_IRQ_SET_PRIORITY(s->priority);
        if (NX_IMG_IRQ_PRIORITY() != s->priority)
            return retain(s, NX_ERROR_BUSY, now);
        s->vector_owned = false;
    }
    if (s->closing && !gate(s, false))
        return retain(s, NX_ERROR_BUSY, now);
    return finish(s);
}

/** \brief Enforce the entire hardware extent and explicit DMA permissions. */
static nx_result_t span(nx_stm32_image_state_t* s, const void* p, size_t n,
                        nx_dma_direction_t direction) {
    if (aliases_owner(s, p, n) || (uintptr_t)p > UINT32_MAX ||
        n > (size_t)(UINT32_MAX - (uintptr_t)p) + 1U)
        return NX_ERROR_INVALID;
    return nx_dma_buffer_validate(s->config->regions, s->config->region_count,
                                  p, n, 4U, direction);
}
/** \brief HAL expects expanded ARGB, while the public fill is encoded565. */
static uint32_t color(const nx_image_operation_t* op) {
    uint32_t value = op->fill_value;
    if (op->destination.format == NX_IMAGE_RGB565)
        value = ((value & UINT32_C(0xf800)) << 8U) |
                ((value & UINT32_C(0x07e0)) << 5U) |
                ((value & UINT32_C(0x001f)) << 3U);
    return value;
}
/** \brief Prove positive alpha from scalar parameters without reading pixels.
 */
static bool positive_alpha(const nx_image_source_t* source,
                           nx_image_alpha_t mode, uint8_t value) {
    return value != 0U && (mode == NX_IMAGE_ALPHA_REPLACE ||
                           source->format == NX_IMAGE_RGB565);
}
/** \brief Native direct-color encodings are shared by input/output hardware. */
static uint32_t format(nx_image_format_t selected) {
    return selected == NX_IMAGE_RGB565 ? DMA2D_INPUT_RGB565
                                       : DMA2D_INPUT_ARGB8888;
}
/** \brief Configure one official HAL input, then prove exact width/alpha facts.
 */
static bool input(nx_stm32_image_state_t* s, const nx_image_operation_t* op,
                  uint32_t layer) {
    bool foreground = layer == DMA2D_FOREGROUND_LAYER;
    bool blend = op->kind == NX_IMAGE_BLEND;
    const nx_image_source_t* source =
        foreground ? &op->foreground : &op->background;
    DMA2D_LayerCfgTypeDef* config = &s->handle.LayerCfg[layer];
    bool fill = op->kind == NX_IMAGE_FILL;
    size_t pixel = nx_graphics_pixel_bytes(source->format);
    if (!fill && pixel == 0U)
        return false;
    config->InputOffset =
        fill ? 0U : (uint32_t)(source->stride_bytes / pixel - op->width);
    config->InputColorMode =
        format(fill ? op->destination.format : source->format);
    nx_image_alpha_t selected =
        foreground ? op->foreground_alpha_mode : op->background_alpha_mode;
    config->AlphaMode = !blend ? DMA2D_NO_MODIF_ALPHA
                        : selected == NX_IMAGE_ALPHA_REPLACE
                            ? DMA2D_REPLACE_ALPHA
                            : DMA2D_COMBINE_ALPHA;
    config->InputAlpha = !blend       ? 0U
                         : foreground ? op->foreground_alpha
                                      : op->background_alpha;
    if (HAL_DMA2D_ConfigLayer(&s->handle, layer) != HAL_OK)
        return false;
    DMA2D_TypeDef* r = s->config->registers;
    volatile uint32_t* control = foreground ? &r->FGPFCCR : &r->BGPFCCR;
    volatile uint32_t* offset = foreground ? &r->FGOR : &r->BGOR;
    uint32_t expected = config->InputColorMode |
                        (config->AlphaMode << DMA2D_FGPFCCR_AM_Pos) |
                        (config->InputAlpha << DMA2D_FGPFCCR_ALPHA_Pos);
    return (NX_IMG_READ(control) & (DMA2D_FGPFCCR_CM | DMA2D_FGPFCCR_AM |
                                    DMA2D_FGPFCCR_ALPHA)) == expected &&
           NX_IMG_READ(offset) == config->InputOffset;
}
/** \brief Reuse complete HAL setup while keeping admission proof before START.
 */
static bool configure(nx_stm32_image_state_t* s,
                      const nx_image_operation_t* op) {
    size_t pixel = nx_graphics_pixel_bytes(op->destination.format);
    if (pixel == 0U)
        return false;
    DMA2D_TypeDef* r = s->config->registers;
    s->handle.Init.Mode = op->kind == NX_IMAGE_FILL      ? DMA2D_R2M
                          : op->kind == NX_IMAGE_COPY    ? DMA2D_M2M
                          : op->kind == NX_IMAGE_CONVERT ? DMA2D_M2M_PFC
                                                         : DMA2D_M2M_BLEND;
    s->handle.Init.ColorMode = format(op->destination.format);
    s->handle.Init.OutputOffset =
        (uint32_t)(op->destination.stride_bytes / pixel - op->width);
    return HAL_DMA2D_Init(&s->handle) == HAL_OK &&
           (NX_IMG_READ(&r->CR) & DMA2D_CR_MODE) == s->handle.Init.Mode &&
           (NX_IMG_READ(&r->OPFCCR) & DMA2D_OPFCCR_CM) ==
               s->handle.Init.ColorMode &&
           (NX_IMG_READ(&r->OOR) & DMA2D_OOR_LO) ==
               s->handle.Init.OutputOffset &&
           input(s, op, DMA2D_FOREGROUND_LAYER) &&
           (op->kind != NX_IMAGE_BLEND ||
            input(s, op, DMA2D_BACKGROUND_LAYER)) &&
           (NX_IMG_READ(&r->ISR) & IMAGE_FLAGS) == 0U;
}
/** \brief One bounded stride check, after generic extent arithmetic validation.
 */
static bool stride(size_t bytes, size_t pixel, size_t width) {
    return bytes % 4U == 0U && bytes / pixel - width <= 16383U;
}
/** \brief Admission cannot touch hardware before the unique request borrow. */
static nx_result_t submit(void* context, nx_image_request_t* request) {
    nx_stm32_image_state_t* s = context;
    if (!NX_IMG_TASK())
        return NX_ERROR_CONTEXT;
    if (!s->initialized || s->closing || s->clock_fault)
        return NX_ERROR_STATE;
    if (s->executing || s->active != NULL)
        return NX_ERROR_BUSY;
#if defined(NX_STM32_SDRAM_GRAPHICS)
    if (s->memory_attached) {
        nx_result_t result = nx_stm32_sdram_graphics_check(
            s->config->graphics_memory, NX_STM32_SDRAM_GRAPHICS_DMA2D, s,
            s->config->system);
        if (result != NX_SUCCESS)
            return result;
    }
#endif
    const nx_image_operation_t* op = &request->operation;
    if (aliases_owner(s, request, sizeof(*request)))
        return NX_ERROR_INVALID;
    if (!supported(op->kind, op->foreground.format, op->background.format,
                   op->destination.format))
        return NX_ERROR_UNSUPPORTED;
    /* AlphaOUT0 has no defined RGB. A565 output discards alpha, so prove a
     * positive input from scalar mode/format/constant facts, never a scan. */
    if (op->kind == NX_IMAGE_BLEND &&
        op->destination.format == NX_IMAGE_RGB565 &&
        !positive_alpha(&op->foreground, op->foreground_alpha_mode,
                        op->foreground_alpha) &&
        !positive_alpha(&op->background, op->background_alpha_mode,
                        op->background_alpha))
        return NX_ERROR_UNSUPPORTED;
    if (op->width > 16383U || op->height > 65535U ||
        !stride(op->destination.stride_bytes,
                nx_graphics_pixel_bytes(op->destination.format), op->width) ||
        (op->kind != NX_IMAGE_FILL &&
         !stride(op->foreground.stride_bytes,
                 nx_graphics_pixel_bytes(op->foreground.format), op->width)) ||
        (op->kind == NX_IMAGE_BLEND &&
         !stride(op->background.stride_bytes,
                 nx_graphics_pixel_bytes(op->background.format), op->width)))
        return NX_ERROR_INVALID;
    nx_result_t result = span(s, op->destination.data, op->destination.capacity,
                              NX_DMA_FROM_DEVICE);
    if (result == NX_SUCCESS && op->kind != NX_IMAGE_FILL)
        result = span(s, op->foreground.data, op->foreground.capacity,
                      NX_DMA_TO_DEVICE);
    if (result == NX_SUCCESS && op->kind == NX_IMAGE_BLEND)
        result = span(s, op->background.data, op->background.capacity,
                      NX_DMA_TO_DEVICE);
    if (result != NX_SUCCESS)
        return result;
    nx_time_us_t now = NX_IMG_NOW();
    if (request->base.deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    if (now == NX_DEADLINE_NEVER)
        return NX_ERROR_EXHAUSTED;
    if (nx_deadline_expired(request->base.deadline, now))
        return NX_ERROR_TIMEOUT;
    if (s->config->system->time_epoch != s->epoch || !clocks(s->config))
        return NX_ERROR_STATE;
    DMA2D_TypeDef* r = s->config->registers;
    if ((NX_IMG_READ(&r->CR) & (DMA2D_CR_START | DMA2D_CR_ABORT)) != 0U ||
        NX_IMG_IRQ_PENDING() != 0U || NX_IMG_IRQ_ACTIVE() != 0U)
        return NX_ERROR_BUSY;
    result = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS)
        return result;
    s->executing = true;
    s->active = request;
    s->events = 0U;
    __HAL_DMA2D_CLEAR_FLAG(&s->handle, IMAGE_FLAGS);
    bool ready = configure(s, op);
    now = NX_IMG_NOW();
    nx_result_t start_failure = NX_SUCCESS;
    if (!ready || !clocks(s->config) ||
        s->config->system->time_epoch != s->epoch || now == NX_DEADLINE_NEVER)
        start_failure = NX_ERROR_IO;
    else if (nx_deadline_expired(request->base.deadline, now))
        start_failure = NX_ERROR_TIMEOUT;
    if (start_failure == NX_SUCCESS) {
        NX_IMG_IRQ_ENABLE();
        uint32_t source = op->kind == NX_IMAGE_FILL
                              ? color(op)
                              : (uint32_t)(uintptr_t)op->foreground.data;
        HAL_StatusTypeDef started = HAL_ERROR;
        if (NX_IMG_IRQ_ENABLED() != 0U)
            started = op->kind == NX_IMAGE_BLEND
                          ? HAL_DMA2D_BlendingStart_IT(
                                &s->handle, source,
                                (uint32_t)(uintptr_t)op->background.data,
                                (uint32_t)(uintptr_t)op->destination.data,
                                (uint32_t)op->width, (uint32_t)op->height)
                          : HAL_DMA2D_Start_IT(
                                &s->handle, source,
                                (uint32_t)(uintptr_t)op->destination.data,
                                (uint32_t)op->width, (uint32_t)op->height);
        if (started != HAL_OK)
            start_failure = NX_ERROR_IO;
    }
    if (start_failure != NX_SUCCESS)
        drain(s, start_failure, now);
    s->executing = false;
    return NX_SUCCESS;
}
/** \brief Bound one Task call; no callback reentry or hidden worker. */
static nx_result_t service(void* context) {
    nx_stm32_image_state_t* s = context;
    if (!NX_IMG_TASK())
        return NX_ERROR_CONTEXT;
    if (!s->owned)
        return NX_ERROR_STATE;
    if (s->executing)
        return NX_ERROR_BUSY;
    s->executing = true;
    nx_result_t result = progress(s);
    s->executing = false;
    return result;
}
/** \brief Cancel records intent without releasing storage or replaying pixels.
 */
static nx_result_t cancel(void* context, nx_image_request_t* request) {
    nx_stm32_image_state_t* s = context;
    if (!NX_IMG_TASK())
        return NX_ERROR_CONTEXT;
    if (!s->owned || s->active != request)
        return NX_ERROR_STATE;
    if (s->executing)
        return NX_ERROR_BUSY;
    cancel_intent(s);
    return NX_SUCCESS;
}
/** \brief Wake binding cannot weaken the configured syscall ceiling. */
static nx_result_t attach(void* context, const nx_irq_wake_t* wake,
                          uint8_t ceiling) {
    nx_stm32_image_state_t* s = context;
    if (!NX_IMG_TASK())
        return NX_ERROR_CONTEXT;
    if (!s->initialized || s->closing)
        return NX_ERROR_STATE;
    if (s->executing || s->active != NULL || s->publishers != 0U)
        return NX_ERROR_BUSY;
    if (ceiling != s->config->irq_policy.syscall_ceiling ||
        (wake != NULL && aliases_owner(s, wake, sizeof(*wake))))
        return NX_ERROR_INVALID;
    nx_irq_source_t source = {(int16_t)DMA2D_IRQn,
                              (uint8_t)NX_IMG_IRQ_PRIORITY(),
                              (uint8_t)NX_IMG_IRQ_GROUP()};
    nx_result_t result =
        nx_irq_wake_validate(wake, &s->config->irq_policy, &source);
    if (result == NX_SUCCESS)
        s->wake = wake;
    return result;
}
/** \brief Close joins every partial receipt before returning borrowed config.
 */
nx_result_t nx_stm32_image_close(nx_stm32_image_state_t* s) {
    if (!NX_IMG_TASK())
        return NX_ERROR_CONTEXT;
    if (s == NULL)
        return NX_ERROR_INVALID;
    if (s->executing || s->publishers != 0U)
        return NX_ERROR_BUSY;
    if (!s->owned)
        return NX_SUCCESS;
    if (!s->controller_touched) {
        if (!domains(s->config) || !gate(s, false))
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
    if (s->active != NULL)
        cancel_intent(s);
    nx_result_t result = progress(s);
    s->executing = false;
    return result;
}
/** \brief Face adapter keeps lifecycle ownership in the same C state. */
static nx_result_t stop(void* context) {
    return nx_stm32_image_close(context);
}
const nx_image_ops_t nx_stm32_image_ops = {query,  submit, service,
                                           cancel, stop,   attach};
/** \brief Pre-System static validation never retains references or effects. */
nx_result_t nx_stm32_image_admit(nx_stm32_image_state_t* s,
                                 const nx_stm32_image_config_t* c) {
    if (!NX_IMG_TASK())
        return NX_ERROR_CONTEXT;
    if (!config_valid(s, c) || s->owned || s->initialized || s->gate_owned)
        return NX_ERROR_INVALID;
    if (s->executing || s->publishers != 0U)
        return NX_ERROR_BUSY;
    if ((NX_IMG_READ(&c->rcc->AHB1ENR) & RCC_AHB1ENR_DMA2DEN) != 0U ||
        (NX_IMG_READ(&c->rcc->AHB1RSTR) & RCC_AHB1RSTR_DMA2DRST) != 0U)
        return NX_ERROR_BUSY;
    if (NX_IMG_IRQ_ENABLED() != 0U || NX_IMG_IRQ_PENDING() != 0U ||
        NX_IMG_IRQ_ACTIVE() != 0U)
        return NX_ERROR_BUSY;
    return NX_SUCCESS;
}
/** \brief Task initialization refuses foreign work before any effect. */
nx_result_t nx_stm32_image_prepare(nx_stm32_image_state_t* s,
                                   const nx_stm32_image_config_t* c) {
    nx_result_t admitted = nx_stm32_image_admit(s, c);
    if (admitted != NX_SUCCESS)
        return admitted;
    if (!domains(c))
        return NX_ERROR_STATE;
    if ((NX_IMG_READ(&c->rcc->AHB1ENR) & RCC_AHB1ENR_DMA2DEN) != 0U ||
        NX_IMG_IRQ_ENABLED() != 0U || NX_IMG_IRQ_PENDING() != 0U ||
        NX_IMG_IRQ_ACTIVE() != 0U)
        return NX_ERROR_BUSY;
    memset(s, 0, sizeof(*s));
    s->config = c;
    s->owned = true;
    s->epoch = c->system->time_epoch;
    s->handle.Instance = c->registers;
#if defined(NX_STM32_SDRAM_GRAPHICS)
    if (c->graphics_memory != NULL) {
        nx_result_t result = nx_stm32_sdram_graphics_attach(
            c->graphics_memory, NX_STM32_SDRAM_GRAPHICS_DMA2D, s, c->system);
        if (result != NX_SUCCESS)
            return result;
        s->memory_attached = true;
    }
#endif
    if (!gate(s, true))
        return NX_ERROR_IO;
    s->controller_touched = true;
    if ((NX_IMG_READ(&c->registers->CR) &
         (IMAGE_IRQS | DMA2D_CR_START | DMA2D_CR_ABORT)) != 0U ||
        (NX_IMG_READ(&c->registers->FGPFCCR) & DMA2D_FGPFCCR_START) != 0U ||
        (NX_IMG_READ(&c->registers->BGPFCCR) & DMA2D_BGPFCCR_START) != 0U) {
        /* Never abort a controller that violated the trusted-reset boundary. */
        s->clock_fault = true;
        return NX_ERROR_IO;
    }
    s->priority = NX_IMG_IRQ_PRIORITY();
    s->handle.State = HAL_DMA2D_STATE_READY;
    if (!withdraw(s))
        return NX_ERROR_IO;
    s->vector_owned = true;
    NX_IMG_IRQ_SET_PRIORITY(c->priority);
    NX_IMG_IRQ_ENABLE();
    nx_arch_dsb();
    if (NX_IMG_IRQ_PRIORITY() != c->priority || NX_IMG_IRQ_ENABLED() == 0U)
        return NX_ERROR_IO;
    s->initialized = true;
    return NX_SUCCESS;
}
/** \brief A callback uses its exact embedded Handle, never a global owner. */
static nx_stm32_image_state_t* owner(DMA2D_HandleTypeDef* handle) {
    return (nx_stm32_image_state_t*)((uint8_t*)handle -
                                     offsetof(nx_stm32_image_state_t, handle));
}
/** \brief IRQ publication contains no request or payload dereference. */
static void notify(nx_stm32_image_state_t* s, uint32_t event) {
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
/** \brief Hardware complete only latches intent, not SETTLED. */
static void complete_callback(DMA2D_HandleTypeDef* handle) {
    notify(owner(handle), IMAGE_COMPLETE);
}
/** \brief Preserve an actual transport/configuration error through withdrawal.
 */
static void error_callback(DMA2D_HandleTypeDef* handle) {
    notify(owner(handle), IMAGE_ERROR);
}
/** \brief No returned config/request is touched by a late vector. */
void nx_stm32_image_irq(nx_stm32_image_state_t* s) {
    if (s != NULL && s->owned && s->initialized) {
        if (s->clock_fault || !clocks(s->config) ||
            s->config->system->time_epoch != s->epoch) {
            s->clock_fault = true;
            NX_IMG_IRQ_DISABLE();
            notify(s, IMAGE_ERROR);
            nx_arch_dsb();
            return;
        }
        s->handle.XferCpltCallback = complete_callback;
        s->handle.XferErrorCallback = error_callback;
        HAL_DMA2D_IRQHandler(&s->handle);
    }
    nx_arch_dsb();
}
