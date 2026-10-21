/**
 * \file            h743_frame_dma2d.c
 * \brief           Exact H743 framed RGB565 copy/PFC and final access
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/components/frame_dma2d.h"
#include "stm32h743xx.h"
#include <string.h>

#ifndef NX_H743_FRAME_REGS
#define NX_H743_FRAME_REGS        DMA2D
#define NX_H743_FRAME_RCC         RCC
#define NX_H743_FRAME_CORE        SCB
#define NX_H743_FRAME_DEBUG       DBGMCU
#define NX_H743_FRAME_READ(p)     (*(p))
#define NX_H743_FRAME_WRITE(p, v) (*(p) = (v))
#define NX_H743_FRAME_IRQ_QUIET()                                              \
    (NVIC_GetEnableIRQ(DMA2D_IRQn) == 0U &&                                    \
     NVIC_GetPendingIRQ(DMA2D_IRQn) == 0U && NVIC_GetActive(DMA2D_IRQn) == 0U)
#endif

#define FRAME_MAGIC 0x46324431U
#define FRAME_IRQS                                                             \
    (DMA2D_CR_TEIE | DMA2D_CR_TCIE | DMA2D_CR_TWIE | DMA2D_CR_CAEIE |          \
     DMA2D_CR_CTCIE | DMA2D_CR_CEIE)
#define FRAME_FLAGS  0x3FU
#define FRAME_ERRORS (DMA2D_ISR_TEIF | DMA2D_ISR_CAEIF | DMA2D_ISR_CEIF)

/** \brief Private state is copied to/from the opaque exact storage object. */
typedef struct {
    uint32_t magic;
    uint32_t abort_limit;
    uint32_t remaining;
    const nx_frame_blitter_t* face;
    nx_frame_blit_request_t* active;
    nx_frame_dma_view_t source;
    size_t destination_span;
    nx_time_us_t last_time;
    uint8_t result;
    bool clock_owned;
    bool ready;
    bool stopping;
    bool stopped;
    bool draining;
    bool guard_lost;
} owner_t;
_Static_assert(NX_ERROR_CONTEXT <= UINT8_MAX,
               "private result byte must contain the Nexus status vocabulary");
_Static_assert(sizeof(owner_t) == sizeof(nx_h743_frame_dma2d_storage_t),
               "public storage must exactly match one private owner ABI");
_Static_assert(_Alignof(owner_t) == _Alignof(nx_h743_frame_dma2d_storage_t),
               "public storage must exactly match private owner alignment");
_Static_assert(DMA2D_BASE == 0x52001000U && RCC_BASE == 0x58024400U,
               "this provider selects only the H743 physical DMA2D");

static bool task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

static bool span(const void* data, size_t bytes) {
    return data != NULL && bytes != 0U &&
           bytes <= UINTPTR_MAX - (uintptr_t)data;
}

static bool overlap(const void* a, size_t as, const void* b, size_t bs) {
    return !span(a, as) || !span(b, bs) ||
           ((uintptr_t)a < (uintptr_t)b + bs &&
            (uintptr_t)b < (uintptr_t)a + as);
}

static void save(void* storage, const owner_t* owner) {
    /* memcpy avoids effective-type assumptions about caller opaque bytes. */
    memcpy(storage, owner, sizeof(*owner));
}

static bool load(void* storage, owner_t* owner) {
    if (storage == NULL ||
        (uintptr_t)storage % _Alignof(nx_h743_frame_dma2d_storage_t) != 0U) {
        return false;
    }
    memcpy(owner, storage, sizeof(*owner));
    return owner->magic == FRAME_MAGIC && owner->face != NULL &&
           owner->face->ops == &nx_h743_frame_dma2d_ops &&
           owner->face->context == storage;
}

/** \brief Resource-history loss cannot be repaired by zeroed DMA registers. */
static bool live(owner_t* owner) {
    /* Reuse readiness as a permanent latch while an accepted loan exists.
     * No further MMIO or API can infer final master access after this loss. */
    if (owner->active != NULL && !owner->ready) {
        return false;
    }
    if ((NX_H743_FRAME_READ(&NX_H743_FRAME_RCC->AHB3ENR) &
         RCC_AHB3ENR_DMA2DEN) == 0U ||
        (NX_H743_FRAME_READ(&NX_H743_FRAME_RCC->AHB3RSTR) &
         RCC_AHB3RSTR_DMA2DRST) != 0U) {
        if (owner->active != NULL) {
            owner->ready = false;
        }
        return false;
    }
    return (NX_H743_FRAME_READ(&NX_H743_FRAME_CORE->CCR) & SCB_CCR_DC_Msk) ==
               0U &&
           NX_H743_FRAME_IRQ_QUIET();
}

static bool checked(volatile uint32_t* reg, uint32_t value, uint32_t mask) {
    NX_H743_FRAME_WRITE(reg, value);
    nx_arch_dsb();
    return (NX_H743_FRAME_READ(reg) & mask) == (value & mask);
}

/** \brief Only the selected RCC bit is changed; other gates retain their value.
 */
static bool clock_set(bool enabled) {
    uint32_t value = NX_H743_FRAME_READ(&NX_H743_FRAME_RCC->AHB3ENR);
    value = enabled ? value | RCC_AHB3ENR_DMA2DEN
                    : value & ~(uint32_t)RCC_AHB3ENR_DMA2DEN;
    return checked(&NX_H743_FRAME_RCC->AHB3ENR, value, RCC_AHB3ENR_DMA2DEN);
}

static bool reset(void) {
    uint32_t previous = NX_H743_FRAME_READ(&NX_H743_FRAME_RCC->AHB3RSTR);
    return checked(&NX_H743_FRAME_RCC->AHB3RSTR,
                   previous | RCC_AHB3RSTR_DMA2DRST, RCC_AHB3RSTR_DMA2DRST) &&
           checked(&NX_H743_FRAME_RCC->AHB3RSTR,
                   previous & ~(uint32_t)RCC_AHB3RSTR_DMA2DRST,
                   RCC_AHB3RSTR_DMA2DRST);
}

/** \brief Admission rejects aliases with all known source ownership controls.
 */
static bool controls_disjoint(const owner_t* owner, const void* storage,
                              const nx_frame_blit_request_t* request,
                              size_t destination_span) {
    nx_frame_dma_loan_t* loan = request->source;
    nx_frame_stream_t* framed = loan->owner;
    nx_stream_t* stream = framed->stream;
    if (stream->count > SIZE_MAX / sizeof(*stream->slots)) {
        return false;
    }
    const void* controls[] = {storage, owner->face, request,      loan,
                              framed,  stream,      stream->slots};
    const size_t sizes[] = {sizeof(nx_h743_frame_dma2d_storage_t),
                            sizeof(*owner->face),
                            sizeof(*request),
                            sizeof(*loan),
                            sizeof(*framed),
                            sizeof(*stream),
                            stream->count * sizeof(*stream->slots)};
    for (size_t i = 0U; i < sizeof(controls) / sizeof(controls[0]); ++i) {
        if (overlap(loan->block.data, loan->block.length, controls[i],
                    sizes[i]) ||
            overlap(request->destination, destination_span, controls[i],
                    sizes[i])) {
            return false;
        }
    }
    /* Every source-pool slot may be reused once its independent loan ends.
     * Destination borrowing must not silently depend on any such pool loan. */
    for (size_t i = 0U; i < stream->count; ++i) {
        if (overlap(request->destination, destination_span,
                    stream->slots[i].data, stream->slots[i].capacity)) {
            return false;
        }
        /* A free slot may be reused independently. No control can borrow
         * bytes from any pool capacity, including immutable frame tails. */
        for (size_t j = 0U; j < sizeof(controls) / sizeof(controls[0]); ++j) {
            if (overlap(controls[j], sizes[j], stream->slots[i].data,
                        stream->slots[i].capacity)) {
                return false;
            }
        }
    }
    return !overlap(storage, sizeof(nx_h743_frame_dma2d_storage_t), request,
                    sizeof(*request)) &&
           !overlap(storage, sizeof(nx_h743_frame_dma2d_storage_t), loan,
                    sizeof(*loan)) &&
           !overlap(request, sizeof(*request), loan, sizeof(*loan));
}

/** \brief A fixed format selects its source bytes; output is always RGB565. */
static size_t source_pixel_bytes(const nx_frame_blit_request_t* request) {
    return request->format == NX_FRAME_BLIT_ARGB8888_TO_RGB565 ? 4U : 2U;
}

static uint32_t mode(const nx_frame_blit_request_t* request) {
    return request->format == NX_FRAME_BLIT_ARGB8888_TO_RGB565 ? DMA2D_CR_MODE_0
                                                               : 0U;
}

/** \brief Validate both exact pixel geometries and complete AXI intervals. */
static nx_result_t validate(const owner_t* owner, const void* storage,
                            const nx_frame_blit_request_t* request,
                            nx_frame_dma_view_t* source,
                            size_t* destination_span) {
    if (request == NULL || request->source == NULL) {
        return NX_ERROR_INVALID;
    }
    if (request->format != NX_FRAME_BLIT_RGB565 &&
        request->format != NX_FRAME_BLIT_ARGB8888_TO_RGB565) {
        return NX_ERROR_UNSUPPORTED;
    }
    size_t input_pixel = source_pixel_bytes(request);
    size_t pixels = request->width_bytes / input_pixel;
    size_t output_row = pixels * 2U;
    if (request->width_bytes == 0U ||
        request->width_bytes % input_pixel != 0U ||
        request->x % input_pixel != 0U || pixels > 0x3FFFU ||
        request->rows == 0U || request->rows > 0xFFFFU ||
        request->destination == NULL ||
        (uintptr_t)request->destination % 2U != 0U ||
        request->destination_stride < output_row ||
        request->destination_stride % 2U != 0U ||
        (request->destination_stride - output_row) / 2U > 0x3FFFU ||
        request->rows - 1U > SIZE_MAX / request->destination_stride) {
        return NX_ERROR_INVALID;
    }
    size_t tail = (request->rows - 1U) * request->destination_stride;
    if (output_row > SIZE_MAX - tail ||
        tail + output_row > request->destination_capacity) {
        return NX_ERROR_INVALID;
    }
    *destination_span = tail + output_row;
    nx_result_t result = nx_frame_dma_view(
        request->source, request->plane, request->x, request->y,
        request->width_bytes, request->rows, source);
    if (result != NX_SUCCESS) {
        return result;
    }
    if ((uintptr_t)source->data % input_pixel != 0U ||
        source->stride % input_pixel != 0U ||
        (source->stride - source->row_bytes) / input_pixel > 0x3FFFU ||
        overlap(request->source->block.data, request->source->block.length,
                request->destination, *destination_span) ||
        !controls_disjoint(owner, storage, request, *destination_span)) {
        return NX_ERROR_INVALID;
    }
    const nx_dma_memory_region_t domain = {.begin = 0x24000000U,
                                           .size = 0x80000U,
                                           .permissions = NX_DMA_MEMORY_READ |
                                                          NX_DMA_MEMORY_WRITE};
    result =
        nx_dma_buffer_validate(&domain, 1U, source->data, source->span_bytes,
                               input_pixel, NX_DMA_TO_DEVICE);
    return result != NX_SUCCESS
               ? result
               : nx_dma_buffer_validate(&domain, 1U, request->destination,
                                        *destination_span, 2U,
                                        NX_DMA_FROM_DEVICE);
}

/** \brief Preserve the first software-observed failure across guard changes. */
static void record_failure(owner_t* owner, nx_result_t result) {
    if (owner->result == NX_SUCCESS) {
        owner->result = (uint8_t)result;
    }
}

/** \brief START is read/set; its zero write neither cancels nor retriggers. */
static void begin_drain(owner_t* owner, nx_result_t result) {
    record_failure(owner, result);
    if (!owner->draining) {
        owner->draining = true;
        owner->remaining = owner->abort_limit;
        (void)nx_request_transition(&owner->active->base, NX_REQUEST_DRAINING);
        NX_H743_FRAME_WRITE(&NX_H743_FRAME_REGS->CR,
                            mode(owner->active) | DMA2D_CR_ABORT);
        nx_arch_dsb();
    }
}

static void quarantine(owner_t* owner) {
    (void)nx_request_transition(&owner->active->base, NX_REQUEST_QUARANTINED);
}

/** \brief Detach stale register addresses before returning the stream block. */
static bool detach(void) {
    DMA2D_TypeDef* dma = NX_H743_FRAME_REGS;
    if (!checked(&dma->CR, 0U, DMA2D_CR_START | DMA2D_CR_ABORT | FRAME_IRQS) ||
        !checked(&dma->FGMAR, 0U, UINT32_MAX) ||
        !checked(&dma->OMAR, 0U, UINT32_MAX) ||
        !checked(&dma->NLR, 0U, DMA2D_NLR_PL | DMA2D_NLR_NL)) {
        return false;
    }
    NX_H743_FRAME_WRITE(&dma->IFCR, FRAME_FLAGS);
    nx_arch_dsb();
    return (NX_H743_FRAME_READ(&dma->ISR) & FRAME_FLAGS) == 0U &&
           (NX_H743_FRAME_READ(&dma->CR) & (DMA2D_CR_START | FRAME_IRQS)) ==
               0U &&
           (NX_H743_FRAME_READ(&dma->FGPFCCR) & DMA2D_FGPFCCR_START) == 0U &&
           (NX_H743_FRAME_READ(&dma->BGPFCCR) & DMA2D_BGPFCCR_START) == 0U &&
           NX_H743_FRAME_IRQ_QUIET();
}

/** \brief Final publication occurs after hardware and local references detach.
 */
static nx_result_t settle(void* storage, owner_t* owner) {
    nx_frame_blit_request_t* request = owner->active;
    if (!detach()) {
        quarantine(owner);
        save(storage, owner);
        return NX_ERROR_BUSY;
    }
    const nx_dma_drain_facts_t proof = {.length = owner->source.span_bytes,
                                        .engine_disabled = true,
                                        .irq_detached = true,
                                        .peripheral_idle = true};
    nx_result_t released =
        nx_frame_dma_release(request->source->owner, request->source, &proof);
    if (released != NX_SUCCESS) {
        quarantine(owner);
        save(storage, owner);
        return NX_ERROR_BUSY;
    }
    nx_result_t result = (nx_result_t)owner->result;
    size_t bytes = result == NX_SUCCESS
                       ? owner->source.row_bytes / source_pixel_bytes(request) *
                             2U * owner->source.rows
                       : 0U;
    owner->active = NULL;
    owner->source = (nx_frame_dma_view_t){0};
    owner->destination_span = 0U;
    owner->draining = false;
    save(storage, owner);
    nx_request_settle(&request->base, result, bytes);
    return NX_SUCCESS;
}

static nx_result_t service(void* storage, nx_time_us_t now) {
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    owner_t owner;
    if (!load(storage, &owner)) {
        return NX_ERROR_STATE;
    }
    if (owner.stopped || owner.active == NULL) {
        return NX_SUCCESS;
    }
    if (!live(&owner)) {
        record_failure(&owner, NX_ERROR_IO);
        owner.guard_lost = true;
        quarantine(&owner);
        save(storage, &owner);
        return NX_ERROR_BUSY;
    }
    if (owner.guard_lost) {
        begin_drain(&owner, NX_ERROR_IO);
        owner.guard_lost = false;
    }
    uint32_t flags = NX_H743_FRAME_READ(&NX_H743_FRAME_REGS->ISR);
    uint32_t control = NX_H743_FRAME_READ(&NX_H743_FRAME_REGS->CR);
    if (!owner.draining) {
        if ((flags & FRAME_ERRORS) != 0U || now < owner.last_time ||
            now == NX_DEADLINE_NEVER) {
            begin_drain(&owner, NX_ERROR_IO);
        } else if (owner.active->base.deadline != NX_DEADLINE_NEVER &&
                   now >= owner.active->base.deadline) {
            begin_drain(&owner, NX_ERROR_TIMEOUT);
        } else if ((flags & DMA2D_ISR_TCIF) != 0U &&
                   (control & DMA2D_CR_START) == 0U) {
            owner.result = NX_SUCCESS;
            return settle(storage, &owner);
        } else {
            owner.last_time = now;
            save(storage, &owner);
            return NX_ERROR_BUSY;
        }
    }
    control = NX_H743_FRAME_READ(&NX_H743_FRAME_REGS->CR);
    if ((control & DMA2D_CR_START) != 0U ||
        (NX_H743_FRAME_READ(&NX_H743_FRAME_REGS->FGPFCCR) &
         DMA2D_FGPFCCR_START) != 0U ||
        (NX_H743_FRAME_READ(&NX_H743_FRAME_REGS->BGPFCCR) &
         DMA2D_BGPFCCR_START) != 0U) {
        if (owner.remaining != 0U) {
            --owner.remaining;
        }
        if (owner.remaining == 0U) {
            quarantine(&owner);
        }
        save(storage, &owner);
        return NX_ERROR_BUSY;
    }
    return settle(storage, &owner);
}

/** \brief LOM=0: foreground/output offsets count their own format pixels. */
static bool configure(const nx_frame_blit_request_t* request,
                      const nx_frame_dma_view_t* source) {
    DMA2D_TypeDef* dma = NX_H743_FRAME_REGS;
    size_t input_pixel = source_pixel_bytes(request);
    size_t pixels = source->row_bytes / input_pixel;
    return checked(&dma->CR, mode(request), UINT32_MAX) &&
           checked(&dma->FGMAR, (uint32_t)(uintptr_t)source->data,
                   UINT32_MAX) &&
           checked(
               &dma->FGOR,
               (uint32_t)((source->stride - source->row_bytes) / input_pixel),
               DMA2D_FGOR_LO) &&
           checked(&dma->FGPFCCR, input_pixel == 4U ? 0U : 2U, UINT32_MAX) &&
           checked(&dma->BGPFCCR, 0U, UINT32_MAX) &&
           checked(&dma->OPFCCR, 2U, UINT32_MAX) &&
           checked(&dma->OMAR, (uint32_t)(uintptr_t)request->destination,
                   UINT32_MAX) &&
           checked(&dma->OOR,
                   (uint32_t)((request->destination_stride - pixels * 2U) / 2U),
                   DMA2D_OOR_LO) &&
           checked(&dma->NLR,
                   (uint32_t)(pixels << 16U) | (uint32_t)request->rows,
                   DMA2D_NLR_PL | DMA2D_NLR_NL);
}

static nx_result_t submit(void* storage, nx_frame_blit_request_t* request,
                          nx_time_us_t now) {
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    owner_t owner;
    if (!load(storage, &owner) || !owner.ready || owner.stopping ||
        owner.stopped) {
        return NX_ERROR_STATE;
    }
    if (owner.active != NULL) {
        return NX_ERROR_BUSY;
    }
    if (!live(&owner) || (NX_H743_FRAME_READ(&NX_H743_FRAME_REGS->CR) &
                          (DMA2D_CR_START | FRAME_IRQS)) != 0U) {
        return NX_ERROR_STATE;
    }
    if (request == NULL ||
        nx_request_state(&request->base) != NX_REQUEST_READY) {
        return NX_ERROR_INVALID;
    }
    if (now == NX_DEADLINE_NEVER ||
        (request->base.deadline != NX_DEADLINE_NEVER &&
         now >= request->base.deadline)) {
        return NX_ERROR_TIMEOUT;
    }
    nx_frame_dma_view_t source;
    size_t destination_span;
    nx_result_t result =
        validate(&owner, storage, request, &source, &destination_span);
    if (result != NX_SUCCESS) {
        return result;
    }
    result = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS) {
        return result;
    }
    owner.active = request;
    owner.source = source;
    owner.destination_span = destination_span;
    owner.last_time = now;
    owner.result = NX_SUCCESS;
    save(storage, &owner);
    /* Every hardware effect follows admission. Failure still owns all loans. */
    if (!configure(request, &source)) {
        begin_drain(&owner, NX_ERROR_IO);
        quarantine(&owner);
    } else {
        NX_H743_FRAME_WRITE(&NX_H743_FRAME_REGS->IFCR, FRAME_FLAGS);
        nx_arch_dsb();
        if ((NX_H743_FRAME_READ(&NX_H743_FRAME_REGS->ISR) & FRAME_FLAGS) !=
            0U) {
            begin_drain(&owner, NX_ERROR_IO);
            quarantine(&owner);
        } else {
            NX_H743_FRAME_WRITE(&NX_H743_FRAME_REGS->CR,
                                mode(request) | DMA2D_CR_START);
            nx_arch_dsb();
            if ((NX_H743_FRAME_READ(&NX_H743_FRAME_REGS->CR) &
                 DMA2D_CR_START) == 0U &&
                (NX_H743_FRAME_READ(&NX_H743_FRAME_REGS->ISR) &
                 DMA2D_ISR_TCIF) == 0U) {
                begin_drain(&owner, NX_ERROR_IO);
            }
        }
    }
    save(storage, &owner);
    return NX_SUCCESS;
}

static nx_result_t cancel(void* storage, nx_frame_blit_request_t* request) {
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    owner_t owner;
    if (!load(storage, &owner) || request == NULL || owner.active != request) {
        return NX_ERROR_STATE;
    }
    if (!live(&owner)) {
        record_failure(&owner, NX_ERROR_IO);
        owner.guard_lost = true;
        quarantine(&owner);
    } else {
        begin_drain(&owner, NX_ERROR_CANCELLED);
    }
    save(storage, &owner);
    return NX_SUCCESS;
}

static nx_result_t stop(void* storage) {
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    owner_t owner;
    if (!load(storage, &owner)) {
        return NX_ERROR_STATE;
    }
    if (owner.stopped) {
        return NX_SUCCESS;
    }
    owner.stopping = true;
    if (owner.active != NULL) {
        if (live(&owner)) {
            begin_drain(&owner, NX_ERROR_CANCELLED);
        } else {
            record_failure(&owner, NX_ERROR_IO);
            owner.guard_lost = true;
            quarantine(&owner);
        }
        save(storage, &owner);
        return NX_ERROR_BUSY;
    }
    save(storage, &owner);
    if (owner.clock_owned) {
        /* No request is borrowed here. Reset withdraws configuration, not an
         * unproved active DMA transfer. A failed reset/gate keeps ownership. */
        if (!NX_H743_FRAME_IRQ_QUIET() || !reset() || !clock_set(false)) {
            return NX_ERROR_BUSY;
        }
    }
    owner.clock_owned = false;
    owner.ready = false;
    owner.stopped = true;
    save(storage, &owner);
    return NX_SUCCESS;
}

const nx_frame_blitter_ops_t nx_h743_frame_dma2d_ops = {submit, service, cancel,
                                                        stop};

nx_result_t
nx_h743_frame_dma2d_initialize(nx_h743_frame_dma2d_storage_t* storage,
                               const nx_frame_blitter_t* face,
                               const nx_h743_frame_dma2d_config_t* config) {
    if (storage == NULL || face == NULL || config == NULL ||
        (uintptr_t)storage % _Alignof(nx_h743_frame_dma2d_storage_t) != 0U ||
        face->ops != &nx_h743_frame_dma2d_ops || face->context != storage ||
        config->resource != NX_H743_FRAME_DMA2D_CONTROLLER ||
        config->abort_observations == 0U ||
        overlap(storage, sizeof(*storage), face, sizeof(*face)) ||
        overlap(storage, sizeof(*storage), config, sizeof(*config))) {
        return NX_ERROR_INVALID;
    }
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    uint32_t prior_magic;
    memcpy(&prior_magic, storage, sizeof(prior_magic));
    if (prior_magic != 0U) {
        return NX_ERROR_STATE;
    }
    if ((NX_H743_FRAME_READ(&NX_H743_FRAME_DEBUG->IDCODE) & 0xFFFF0FFFU) !=
            0x20030450U ||
        (NX_H743_FRAME_READ(&NX_H743_FRAME_CORE->CCR) & SCB_CCR_DC_Msk) != 0U) {
        return NX_ERROR_UNSUPPORTED;
    }
    if ((NX_H743_FRAME_READ(&NX_H743_FRAME_RCC->AHB3ENR) &
         RCC_AHB3ENR_DMA2DEN) != 0U ||
        (NX_H743_FRAME_READ(&NX_H743_FRAME_RCC->AHB3RSTR) &
         RCC_AHB3RSTR_DMA2DRST) != 0U ||
        !NX_H743_FRAME_IRQ_QUIET()) {
        return NX_ERROR_BUSY;
    }
    owner_t owner = {.magic = FRAME_MAGIC,
                     .abort_limit = config->abort_observations,
                     .face = face,
                     .clock_owned = true};
    save(storage, &owner);
    if (!clock_set(true) || !reset()) {
        return NX_ERROR_IO;
    }
    if (!live(&owner) || (NX_H743_FRAME_READ(&NX_H743_FRAME_REGS->CR) &
                          (DMA2D_CR_START | FRAME_IRQS)) != 0U) {
        return NX_ERROR_IO;
    }
    owner.ready = true;
    save(storage, &owner);
    return NX_SUCCESS;
}
