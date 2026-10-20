/**
 * \file            frame_dma2d.h
 * \brief           Explicit optional H743 RGB565 framed DMA2D owner
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_FRAME_DMA2D_H
#define NEXUS_COMPONENTS_FRAME_DMA2D_H
#include "nexus/core/request.h"
#include "nexus/io/frame_stream.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           One caller-owned request; rectangle coordinates are bytes.
 *
 * \note            Initialize/prepare base using the ordinary request helpers.
 *                  ACCEPTED borrows this object, the exact source loan control
 *                  and destination span until acquire-observed SETTLED. Source
 *                  loan ownership transfers to this owner on acceptance; it is
 *                  released only after final DMA access. Rejection leaves the
 *                  original loan with its caller. Do not separately release,
 *                  mutate or reuse accepted controls, source or destination.
 *                  Width, offsets and strides obey RGB565 alignment.
 *                  Destination cannot overlap a full-capacity source-pool slot.
 *                  Task-only admission checks known pool extents in O(n).
 */
typedef struct {
    nx_request_t base;
    nx_frame_dma_loan_t* source;
    size_t plane;
    size_t x;
    size_t y;
    size_t width_bytes;
    size_t rows;
    uint8_t* destination;
    size_t destination_stride;
    size_t destination_capacity;
} nx_frame_blit_request_t;

/** \brief One-executor task-only operations; each service call is bounded. */
typedef struct {
    nx_result_t (*submit)(void* context, nx_frame_blit_request_t* request,
                          nx_time_us_t now);
    nx_result_t (*service)(void* context, nx_time_us_t now);
    nx_result_t (*cancel)(void* context, nx_frame_blit_request_t* request);
    nx_result_t (*stop)(void* context);
} nx_frame_blitter_ops_t;
/** \brief Immutable face; state is separate caller-owned private storage. */
typedef struct {
    const nx_frame_blitter_ops_t* ops;
    void* context;
} nx_frame_blitter_t;

/** \brief Explicit exclusive physical resource; no runtime resource registry.
 */
typedef enum { NX_H743_FRAME_DMA2D_CONTROLLER = 1 } nx_h743_frame_resource_t;
/**
 * \brief           Cold configuration for exact H743 V with D-Cache disabled.
 *
 * \note            abort_observations bounds failed service observations before
 *                  quarantine. It is a software progress budget, not a measured
 *                  bus/DMA timing bound. The same executor continues service
 *                  after quarantine; actual drain can still settle the request.
 */
typedef struct {
    nx_h743_frame_resource_t resource;
    uint32_t abort_observations;
} nx_h743_frame_dma2d_config_t;
/**
 * \brief           Exact opaque storage reservation for one optional owner.
 *
 * \note            Zero-initialize once; do not copy or mutate. This is one
 *                  object's ABI storage, not a pool or allocator. Keep it, face
 *                  and all accepted requests live until successful stop.
 */
#if UINTPTR_MAX == UINT32_MAX
#define NX_H743_FRAME_DMA2D_STORAGE_BYTES 64U
#elif UINTPTR_MAX == UINT64_MAX
#define NX_H743_FRAME_DMA2D_STORAGE_BYTES 96U
#else
#error "H743 frame DMA2D maintains 32-bit firmware and 64-bit host-model ABIs"
#endif
typedef union {
    uint64_t alignment;
    uintptr_t pointer_alignment;
    unsigned char private_bytes[NX_H743_FRAME_DMA2D_STORAGE_BYTES];
} nx_h743_frame_dma2d_storage_t;
extern const nx_frame_blitter_ops_t nx_h743_frame_dma2d_ops;
/**
 * \brief           Acquire cold DMA2D and its dedicated RCC gate/reset bit.
 * \param[in,out]   storage: Fresh zero storage exclusively owned by caller.
 * \param[in]       face: Stable readonly {&nx_h743_frame_dma2d_ops, storage}.
 * \param[in]       config: Exact resource plus positive abort observation
 * bound.
 *
 * \return          SUCCESS, INVALID, BUSY for foreign gate/reset/IRQ
 * ownership, UNSUPPORTED for non-V/cache-on, CONTEXT or IO.
 *
 * \note Privileged
 * unmasked task, one serialized executor. Caller explicitly excludes every
 * other DMA2D owner, including HAL, IRQ handlers and resource selections
 * outside this component. Never change Cache/RCC/reset/MPU or clock domain
 * before stop. All payloads must be wholly in AXI SRAM [0x24000000,
 *                  0x24080000); source/destination overlap is rejected. Only
 *                  M2M RGB565, pixel offsets, no CLUT/PFC/blending/IRQ is
 * supported. No pins, Board route, scheduler, queue or worker is created.
 *                  Initialization IO errors retain cleanup ownership; call
 * stop. A successful stop detaches every source then withdraws only its RCC
 * gate. Repeated stop performs no hardware accesses.
 */
nx_result_t
nx_h743_frame_dma2d_initialize(nx_h743_frame_dma2d_storage_t* storage,
                               const nx_frame_blitter_t* face,
                               const nx_h743_frame_dma2d_config_t* config);
#ifdef __cplusplus
}
#endif
#endif
