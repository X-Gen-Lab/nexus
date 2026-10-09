/**
 * \file            spi.h
 *
 * \brief           Single-executor polling SPI endpoints and complete CS
 *                  transactions.
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_SPI_H
#define NEXUS_SPI_H

#include "nexus/core/time.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct nx_spi_port nx_spi_port_t;
typedef struct nx_spi_endpoint nx_spi_endpoint_t;
/**
 * \brief           Execute one bounded full-duplex 8-bit polling transaction.
 *
 * \param[in]       endpoint: Fixed CS/mode/rate binding on one controller.
 *
 * \param[in]       tx: Transmit bytes, or NULL to send 0xff.
 *
 * \param[out]      rx: Receive bytes, or NULL to discard.
 *
 * \param[in]       length: Positive transfer length; at least one buffer
 *                  exists.
 *
 * \param[in]       deadline: Absolute deadline; includes adapter queue
 *                  residence.
 *
 * \param[out]      transferred: Complete bytes; reset to zero before
 *                  execution.
 *
 * \return          Success, BUSY/INVALID/TIMEOUT/IO. Return releases all
 *                  buffers and CS after drain; polling is CPU-active, not
 *                  IRQ/DMA async.
 *
 * \note            Task-only single executor, no allocation/OS lock. One CS
 *                  interval covers the entire transfer. Deadline cannot
 *                  preempt an in-flight hardware byte; peripheral drain may
 *                  exceed it.
 */
nx_result_t nx_spi_endpoint_transfer(const nx_spi_endpoint_t* endpoint,
                                     const uint8_t* tx, uint8_t* rx,
                                     size_t length, nx_time_us_t deadline,
                                     size_t* transferred);
#ifdef __cplusplus
}
#endif

#endif
