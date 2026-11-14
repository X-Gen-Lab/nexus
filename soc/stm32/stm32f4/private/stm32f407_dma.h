/**
 * \file            stm32f407_dma.h
 *
 * \brief           Private F4 finite DMA ownership and static resource records
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_DMA_H
#define NEXUS_STM32F407_DMA_H

#include "nexus/io/dma.h"
#include "stm32f407_provider.h"
#include "stm32f407_spi_cold.h"
#include "stm32f4_dma_route.h"

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

typedef struct nx_stm32_spi_dma_endpoint_state
    nx_stm32_spi_dma_endpoint_state_t;

/** \brief Readonly selected routes and domains shared by one SPI owner.
 * \note Silicon request connections are admitted by reviewed SoC route facts;
 *       the C owner checks physical stream/vector identity and full withdrawal.
 */
typedef struct {
    nx_stm32_dma_route_t transmit;
    nx_stm32_dma_route_t receive;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
} nx_stm32_spi_dma_resources_t;

/** \brief Finite DMA state retains one pointer to immutable integration. */
typedef struct {
    nx_stm32_spi_state_t spi;
    const nx_stm32_spi_dma_resources_t* resources;
    nx_spi_request_t* active;
    const nx_irq_wake_t* wake;
    nx_time_us_t drain_deadline;
    nx_result_t terminal;
    bool tx_complete;
    bool rx_complete;
    bool draining;
    bool stopping;
    bool initialized;
    uint8_t memory_flags;
} nx_stm32_spi_dma_state_t;

/** \brief Per-device CS/mode/rate identity is independent of controller DMA. */
struct nx_stm32_spi_dma_endpoint_state {
    nx_stm32_spi_endpoint_state_t endpoint;
    nx_stm32_spi_dma_state_t* dma;
};

/** \brief Immutable selected SPI assembly and DMA resources. */
typedef struct {
    nx_stm32_spi_config_t spi;
    nx_stm32_spi_dma_resources_t resources;
    uint8_t priority;
    bool kernel_bound;
} nx_stm32_spi_dma_config_t;

/** \brief One controller's retained cold effects and original IRQ priorities.
 * \note Failed preparation restores the original SPI tuple through the shared
 *       cold receipt. Normal close joins IRQs and withdraws CS before the
 *       enclosing Platform restores its original pin snapshots.
 */
typedef struct {
    nx_stm32_spi_cold_receipt_t spi;
    uint8_t priorities[2];
    bool vectors;
} nx_stm32_spi_dma_cold_t;

#ifdef __cplusplus
extern "C" {
#endif

extern const nx_uart_ops_t nx_stm32_uart_dma_ops;
extern const nx_spi_ops_t nx_stm32_spi_dma_ops;
extern const nx_spi_endpoint_ops_t nx_stm32_spi_dma_endpoint_ops;

/**
 * \brief           Initialize one exact SPI full-duplex DMA
 * controller.
 *
 * \param[in,out]   state: Exact pointers, clocks, domains and caller storage.
 *
 * \return          Success or INVALID/BUSY/UNSUPPORTED before buffer admission.
 *
 * \note            Assembly owns selected shared DMA and SPI clocks and
 * reviewed pin/CS resources. Nonoverlapping TX/RX spans are complete buffers,
 *                  or explicit caller-owned single cells through submit_memory.
 */
nx_result_t nx_stm32_spi_dma_initialize(nx_stm32_spi_dma_state_t* state);

/** \brief Prepare exact static SPI pins, endpoints and selected DMA vectors.
 * \param[in,out] state: Cold exclusive privileged task owner, no active loans.
 * \param[in] config: Static facts and the enclosing Platform pin snapshots.
 * \param[in,out] receipt: Static cold effects retained through failed cleanup.
 * \return Success or error; failed withdrawal keeps Platform responsible.
 */
nx_result_t nx_stm32_spi_dma_prepare(nx_stm32_spi_dma_state_t* state,
                                     const nx_stm32_spi_dma_config_t* config,
                                     nx_stm32_spi_dma_cold_t* receipt);
/** \brief Join DMA and wire references before returning pins and SPI clock.
 * \param[in,out] state: The same serialized owner as prepare.
 * \param[in] config: Unchanged static assembly resources.
 * \param[in,out] receipt: The same retained cold effects supplied to prepare.
 * \return Success only after complete driver and cold-resource withdrawal.
 */
nx_result_t nx_stm32_spi_dma_close(nx_stm32_spi_dma_state_t* state,
                                   const nx_stm32_spi_dma_config_t* config,
                                   nx_stm32_spi_dma_cold_t* receipt);

/**
 * \brief           Observe one selected finite DMA source without polling
 *                  waits.
 *
 * \param[in,out]   state: Live exact controller kept through successful stop.
 *
 * \param[in]       receive: True for selected RX; false for selected TX.
 *
 * \note            DMA completion latches memory facts; task service proves
 *                  wire idle before withdrawing CS and publishing settlement.
 * Fresh DMA errors survive completion W1C and are checked after actual EN/IRQ
 * withdrawal. Quarantine retains all borrowed storage.
 */
void nx_stm32_spi_dma_irq(nx_stm32_spi_dma_state_t* state, bool receive);

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
