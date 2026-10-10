/**
 * \file            dma.h
 *
 * \brief           Explicit DMA memory domains, physical streams and drain
 *                  facts
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_DMA_H
#define NEXUS_IO_DMA_H

#include "nexus/core/status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NX_DMA_MEMORY_READ  1U
#define NX_DMA_MEMORY_WRITE 2U

/** \brief Transfer direction determines device access to caller memory. */
typedef enum { NX_DMA_TO_DEVICE, NX_DMA_FROM_DEVICE } nx_dma_direction_t;

/**
 * \brief           Reviewed DMA-visible memory interval, never CPU
 *                  accessibility.
 *
 * \note            begin plus size must not wrap. Cache maintenance remains a
 *                  provider responsibility on cached architectures.
 */
typedef struct {
    uintptr_t begin;
    size_t size;
    uint32_t permissions;
} nx_dma_memory_region_t;

/**
 * \brief           Physical stream and request selector, without runtime pools.
 *
 * \note            Two selectors on the same controller/stream conflict.
 */
typedef struct {
    uint8_t controller;
    uint8_t stream;
    uint8_t channel;
} nx_dma_resource_t;

/**
 * \brief           Independent facts required before memory borrow release.
 *
 * \note            remaining describes DMA memory transfers, not wire bytes. A
 *                  zero counter never establishes peripheral_idle.
 */
typedef struct {
    size_t length;
    size_t remaining;
    bool engine_disabled;
    bool irq_detached;
    bool peripheral_idle;
} nx_dma_drain_facts_t;

/**
 * \brief           Validate an entire buffer within one explicit memory domain.
 *
 * \param[in]       regions: Immutable SoC domains kept alive for this call.
 *
 * \param[in]       count: Positive number of domains.
 *
 * \param[in]       data: Nonempty caller buffer; never borrowed by this helper.
 *
 * \param[in]       length: Positive byte length, including all DMA accesses.
 *
 * \param[in]       alignment: Nonzero power-of-two transfer alignment.
 *
 * \param[in]       direction: Device reads or writes the supplied memory.
 *
 * \return          Success, INVALID for malformed ranges/alignment or
 *                  PERMISSION for a range outside the required DMA domain.
 *
 * \note            Bounded task/IRQ check, no MMIO, allocation or side effects.
 */
nx_result_t nx_dma_buffer_validate(const nx_dma_memory_region_t* regions,
                                   size_t count, const void* data,
                                   size_t length, size_t alignment,
                                   nx_dma_direction_t direction);

/**
 * \brief           Reject duplicate physical streams in an assembled resource
 *                  set.
 *
 * \param[in]       resources: Immutable resource list, not retained.
 *
 * \param[in]       count: Resource count; zero allows a NULL list.
 *
 * \return          Success, INVALID for a controller/stream/selector outside
 *                  the maintained two-controller eight-stream hardware, BUSY
 *                  for a duplicate physical stream.
 *
 * \note            Cold configuration check; this is not a runtime allocator.
 */
nx_result_t nx_dma_resources_validate(const nx_dma_resource_t* resources,
                                      size_t count);

/**
 * \brief           Require engine, IRQ and peripheral drain independently.
 *
 * \param[in]       facts: Provider observations; not retained.
 *
 * \return          Success only when all sources are detached and idle, BUSY
 *                  while any source remains, INVALID for impossible counters.
 *
 * \note            The provider must observe actual registers and ordering
 *                  before setting these facts. This helper does not stop
 *                  hardware.
 */
nx_result_t nx_dma_drain_check(const nx_dma_drain_facts_t* facts);

#ifdef __cplusplus
}
#endif

#endif
