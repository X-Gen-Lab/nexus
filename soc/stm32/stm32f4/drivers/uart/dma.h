/**
 * \file            dma.h
 * \brief           Selected UART DMA resources and TX extension lifecycle
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_DRIVERS_UART_DMA_H
#define NEXUS_STM32F4_DRIVERS_UART_DMA_H

#include "drivers/uart/instance.h"
#include "nexus/io/dma.h"
#include "private/stm32f4_dma_route.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Exact caller-owned DMA TX instance; no stream allocator.
 *
 * \note            The selected readonly resources supply reviewed
 *                  stream/channel facts. Clock and pin resources
 *                  remain owned by the platform assembly. Buffer domains
 *                  exclude CPU-only CCM. RX uses the existing independent IRQ
 *                  ring.
 */
/** \brief Immutable selected DMA domain facts, independent of TX storage.
 * \note The cold configuration retains these facts through successful stop;
 *       selected runtime owners retain only a pointer and never modify them.
 */
typedef struct {
    nx_stm32_dma_route_t transmit;
    nx_stm32_dma_route_t receive;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
} nx_stm32_uart_dma_resources_t;

/** \brief Reject mutable storage over retained UART DMA configuration facts.
 * \note Task admission only; malformed or wrapped metadata is also rejected.
 *       These checks precede descriptor writes and producer/DMA borrowing.
 */
static inline bool nx_stm32_uart_dma_resources_overlap(
    const nx_stm32_uart_dma_resources_t* resources, const void* storage,
    size_t length) {
    if (resources == NULL || resources->regions == NULL ||
        resources->region_count == 0U ||
        resources->region_count > SIZE_MAX / sizeof(*resources->regions))
        return true;
    uintptr_t first = (uintptr_t)storage;
    uintptr_t config = (uintptr_t)resources;
    uintptr_t regions = (uintptr_t)resources->regions;
    size_t bytes = resources->region_count * sizeof(*resources->regions);
    return length > UINTPTR_MAX - first ||
           sizeof(*resources) > UINTPTR_MAX - config ||
           bytes > UINTPTR_MAX - regions ||
           (first < config + sizeof(*resources) && config < first + length) ||
           (first < regions + bytes && regions < first + length);
}

/** \brief Only TX-DMA modes retain these completion and drain flags. */
typedef struct {
    bool dma_complete;
    bool aborting;
} nx_stm32_uart_tx_dma_extension_t;

/** \brief One UART owner plus its selected finite TX-DMA extension. */
typedef struct {
    nx_stm32_uart_state_t uart;
    const nx_stm32_uart_dma_resources_t* resources;
    nx_stm32_uart_tx_dma_extension_t transmit;
} nx_stm32_uart_dma_state_t;

extern const nx_uart_ops_t nx_stm32_uart_dma_ops;

/**
 * \brief           Initialize exact USART1/USART2 TX DMA and IRQ RX storage.
 *
 * \param[in,out]   state: Fresh instance with reviewed pointers and fixed
 *                  route.
 *
 * \param[in]       clock_hz: Exact USART clock used by the ordinary UART init.
 *
 * \return          Success or INVALID/BUSY/UNSUPPORTED before TX admission.
 *
 * \note            Task-only; assembly enables selected shared DMA and UART
 * clocks before calling. No DMA operation starts until a valid request admits.
 */
nx_result_t nx_stm32_uart_dma_initialize(nx_stm32_uart_dma_state_t* state,
                                         uint32_t clock_hz);
/**
 * \brief           Initialize TX DMA for an explicit separate RX producer.
 * \param[in,out]   state: Same selected USART/stream facts, BLOCKS profile.
 * \param[in]       clock_hz: Exact USART peripheral clock.
 * \return          Same cold rejection contract as ordinary DMA init.
 * \note            This initializer provides no RX method or block loan itself.
 */
nx_result_t nx_stm32_uart_dma_block_initialize(nx_stm32_uart_dma_state_t* state,
                                               uint32_t clock_hz);

/**
 * \brief           Observe bounded DMA terminal flags and detach memory source.
 *
 * \param[in,out]   state: Live fixed state kept through successful stop.
 *
 * \note            Generated selected DMA vector calls this exact instance; DMA
 *                  TC only enables independent UART TC drain observation.
 */
void nx_stm32_uart_dma_irq(nx_stm32_uart_dma_state_t* state);

/**
 * \brief           Observe IRQ RX and independent TX wire-idle completion.
 *
 * \param[in,out]   state: Same live state as the DMA vector.
 *
 * \note            Generated selected USART vector uses this mode-specific
 * thunk.
 */
void nx_stm32_uart_dma_uart_irq(nx_stm32_uart_dma_state_t* state);

/**
 * \brief           Side-effect-free checks shared with this exact TX start.
 * \param[in]       context: Exact immutable face's selected provider state.
 * \param[in]       request: Live descriptor, not admitted or retained here.
 * \return          Same payload/domain/context/state/deadline rejection.
 * \note            Task-only; reads facts but performs no MMIO store or borrow.
 *                  RS485 binding must use this provider's matching validator.
 */
nx_result_t nx_stm32_uart_dma_tx_validate(void* context,
                                          const nx_uart_tx_request_t* request);

/** \brief Direct selected TX-DMA method; same request borrow contract. */
nx_result_t nx_stm32_uart_dma_submit(void* context,
                                     nx_uart_tx_request_t* request);
/** \brief Direct selected TX-DMA method; same request borrow contract. */
nx_result_t nx_stm32_uart_dma_start_admitted(void* context,
                                             nx_uart_tx_request_t* request);
/** \brief Direct selected TX-DMA method; same request borrow contract. */
nx_result_t nx_stm32_uart_dma_cancel(void* context,
                                     nx_uart_tx_request_t* request);
/** \brief Owner-only bounded service; failed drain retains quarantine. */
void nx_stm32_uart_dma_service(void* context);
/** \brief Close TX admission and prove both memory and wire drain. */
nx_result_t nx_stm32_uart_dma_stop(void* context);

/** \brief Cold component constructor; initializes the shared UART once. */
nx_result_t nx_stm32_uart_dma_initialize_parts(
    nx_stm32_uart_state_t* uart, const nx_stm32_uart_dma_resources_t* resources,
    nx_stm32_uart_tx_dma_extension_t* transmit, uint32_t clock_hz, bool blocks);
/** \brief Typed TX extension operation; no outer-owner context cast. */
nx_result_t nx_stm32_uart_dma_submit_parts(
    nx_stm32_uart_state_t* uart, const nx_stm32_uart_dma_resources_t* resources,
    nx_stm32_uart_tx_dma_extension_t* transmit, nx_uart_tx_request_t* request);
/** \brief Typed TX extension operation; no outer-owner context cast. */
nx_result_t nx_stm32_uart_dma_start_admitted_parts(
    nx_stm32_uart_state_t* uart, const nx_stm32_uart_dma_resources_t* resources,
    nx_stm32_uart_tx_dma_extension_t* transmit, nx_uart_tx_request_t* request);
/** \brief Typed TX extension operation; no outer-owner context cast. */
nx_result_t nx_stm32_uart_dma_cancel_parts(
    nx_stm32_uart_state_t* uart, const nx_stm32_uart_dma_resources_t* resources,
    nx_stm32_uart_tx_dma_extension_t* transmit, nx_uart_tx_request_t* request);
/** \brief Typed TX extension operation; no outer-owner context cast. */
nx_result_t nx_stm32_uart_dma_tx_validate_parts(
    nx_stm32_uart_state_t* uart, const nx_stm32_uart_dma_resources_t* resources,
    nx_stm32_uart_tx_dma_extension_t* transmit,
    const nx_uart_tx_request_t* request);
/** \brief Progress or quiesce TX without releasing the shared UART. */
void nx_stm32_uart_dma_irq_parts(nx_stm32_uart_state_t* uart,
                                 const nx_stm32_uart_dma_resources_t* resources,
                                 nx_stm32_uart_tx_dma_extension_t* transmit);
/** \brief Progress or quiesce TX without releasing the shared UART. */
void nx_stm32_uart_dma_service_parts(
    nx_stm32_uart_state_t* uart, const nx_stm32_uart_dma_resources_t* resources,
    nx_stm32_uart_tx_dma_extension_t* transmit);
/** \brief Progress or quiesce TX without releasing the shared UART. */
nx_result_t
nx_stm32_uart_dma_stop_parts(nx_stm32_uart_state_t* uart,
                             const nx_stm32_uart_dma_resources_t* resources,
                             nx_stm32_uart_tx_dma_extension_t* transmit);

#ifdef __cplusplus
}
#endif
#endif
