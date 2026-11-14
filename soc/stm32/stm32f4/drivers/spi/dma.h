/**
 * \file            dma.h
 * \brief           Selected SPI DMA resources and retained owner lifecycle
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_DRIVERS_SPI_DMA_H
#define NEXUS_STM32F4_DRIVERS_SPI_DMA_H

#include "drivers/spi/instance.h"
#include "drivers/spi/stm32f407_spi_cold.h"
#include "nexus/io/dma.h"
#include "nexus/io/spi_dma_owner_binding.h"
#include "private/stm32f4_dma_route.h"
#include "private/stm32f4_irq_wake.h"

#ifdef __cplusplus
extern "C" {
#endif

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

/** \brief Exact enhanced sequence and underlying controller membership. */
bool nx_stm32_spi_dma_owner_contains(
    const nx_spi_dma_transaction_endpoint_t* endpoint,
    const nx_spi_dma_transaction_port_t* port);
/** \brief Bind one borrowed wake sink to the exact enhanced controller. */
nx_result_t
nx_stm32_spi_dma_owner_attach_wake(const nx_spi_dma_transaction_port_t* port,
                                   const nx_irq_wake_t* wake,
                                   uint8_t syscall_ceiling);

#ifdef __cplusplus
}
#endif
#endif
