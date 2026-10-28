/**
 * \file            stm32f407_dma.h
 *
 * \brief           Private STM32F407 finite UART DMA storage and static routes
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_DMA_H
#define NEXUS_STM32F407_DMA_H

#include "nexus/io/dma.h"
#include "stm32f407_provider.h"
#include "stm32f407_spi_cold.h"

/**
 * \brief           Exact caller-owned DMA TX instance; no stream allocator.
 *
 * \note            USART1 uses DMA2 stream7 channel4; USART2 uses DMA1 stream6
 *                  channel4. Clock and pin resources
 *                  remain owned by the platform assembly. Buffer domains
 *                  exclude CPU-only CCM. RX uses the existing independent IRQ
 *                  ring.
 */
/** \brief Immutable selected DMA domain facts, independent of TX storage. */
typedef struct {
    DMA_TypeDef* dma;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
} nx_stm32_uart_dma_resources_t;

/** \brief Only TX-DMA modes allocate these exact engine and drain facts. */
typedef struct {
    DMA_Stream_TypeDef* tx;
    IRQn_Type dma_irq;
    uint8_t stream;
    uint8_t channel;
    bool dma_complete;
    bool aborting;
} nx_stm32_uart_tx_dma_extension_t;

/** \brief One UART owner plus its selected finite TX-DMA extension. */
typedef struct {
    nx_stm32_uart_state_t uart;
    nx_stm32_uart_dma_resources_t resources;
    nx_stm32_uart_tx_dma_extension_t transmit;
} nx_stm32_uart_dma_state_t;

typedef struct nx_stm32_spi_dma_endpoint_state
    nx_stm32_spi_dma_endpoint_state_t;

/** \brief SPI1 DMA2 TX3/RX0 selector3 or SPI2 DMA1 TX4/RX3 selector0. */
typedef struct {
    nx_stm32_spi_state_t spi;
    DMA_TypeDef* dma;
    DMA_Stream_TypeDef* tx;
    DMA_Stream_TypeDef* rx;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
    nx_spi_request_t* active;
    nx_stm32_spi_dma_endpoint_state_t* endpoint;
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
    DMA_TypeDef* dma;
    DMA_Stream_TypeDef* tx;
    DMA_Stream_TypeDef* rx;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
    uint8_t priority;
    bool kernel_bound;
} nx_stm32_spi_dma_config_t;

/** \brief Exact paired-controller cold effects; pin snapshots stay shared.
 * \note Each bit names a selected pad, never a maximum-size object pool.
 *       A failed pin, vector, priority or gate withdrawal retains its marker.
 */
typedef struct {
    uint32_t clock_added;
    uint16_t pin_leases;
    uint8_t priorities[2];
    bool vectors;
    bool constructed;
} nx_stm32_spi_dma_cold_t;

#ifdef __cplusplus
extern "C" {
#endif

extern const nx_uart_ops_t nx_stm32_uart_dma_ops;
extern const nx_spi_ops_t nx_stm32_spi_dma_ops;
extern const nx_spi_endpoint_ops_t nx_stm32_spi_dma_endpoint_ops;

/**
 * \brief           Initialize one exact SPI1 or SPI2 full-duplex DMA
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
                                     nx_stm32_spi_cold_receipt_t* receipt);
/** \brief Join DMA and wire references before returning pins and SPI clock.
 * \param[in,out] state: The same serialized owner as prepare.
 * \param[in] config: Unchanged static assembly resources.
 * \param[in,out] receipt: The same retained cold effects supplied to prepare.
 * \return Success only after complete driver and cold-resource withdrawal.
 */
nx_result_t nx_stm32_spi_dma_close(nx_stm32_spi_dma_state_t* state,
                                   const nx_stm32_spi_dma_config_t* config,
                                   nx_stm32_spi_cold_receipt_t* receipt);

/** \brief Acquire one member of a selected independent SPI controller pair.
 * \param[in,out] state: Serialized cold owner, no active requests or vectors.
 * \param[in] config: Exact controller, route and shared Platform snapshots.
 * \param[in,out] cold: Zero or fully withdrawn cold effects, private per owner.
 * \return Success or error; every partial effect remains in cold until close.
 */
nx_result_t
nx_stm32_spi_dma_pair_prepare(nx_stm32_spi_dma_state_t* state,
                              const nx_stm32_spi_dma_config_t* config,
                              nx_stm32_spi_dma_cold_t* cold);
/** \brief Join publisher, IRQ, priority, pin and gate ownership in that order.
 * \param[in,out] state: Same exclusive owner as prepare.
 * \param[in] config: Unchanged selected static resources.
 * \param[in,out] cold: Actual retained markers from prepare or failed close.
 * \return Success only after every marker is withdrawn; failure allows retry.
 */
nx_result_t nx_stm32_spi_dma_pair_close(nx_stm32_spi_dma_state_t* state,
                                        const nx_stm32_spi_dma_config_t* config,
                                        nx_stm32_spi_dma_cold_t* cold);

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
