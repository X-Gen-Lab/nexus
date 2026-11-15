/**
 * \file            spi_dma_model.h
 * \brief           Explicit Native segmented wire and drain fault controls
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_NATIVE_SPI_DMA_MODEL_H
#define NEXUS_IO_NATIVE_SPI_DMA_MODEL_H
#include "nexus/io/spi_dma_transaction.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief           Move one modeled wire word or observe its final idle event.
 * \param[in]       port: Exact live Native enhanced controller.
 * \note            Caller-scheduled task/model IRQ; no hidden worker. Facts
 *                  latch before a bounded hint. Never invoke after publisher
 *                  join. A stopped owner cannot drive a successor's model.
 */
void nx_native_spi_dma_model_step(const nx_spi_dma_transaction_port_t* port);
/**
 * \brief           Select optional single-step progress during task service.
 * \param[in]       port: Exact live Native enhanced controller, quiescent.
 * \param[in]       automatic: True for one modeled step per service.
 * \return          SUCCESS, INVALID, STATE, CONTEXT or BUSY while borrowed.
 * \note            Task-only. Explicit stepping remains available. This model
 *                  does not establish physical IRQ/DMA timing or HIL.
 */
nx_result_t
nx_native_spi_dma_model_automatic(const nx_spi_dma_transaction_port_t* port,
                                  bool automatic);
/**
 * \brief           Inject or remove inability to prove physical model drain.
 * \param[in]       port: Exact live Native enhanced controller.
 * \param[in]       hold: Keep all request/CS loans while true.
 * \return          SUCCESS, INVALID, STATE or CONTEXT.
 * \note            Task-only, allowed while borrowed. Removing the fault does
 *                  not settle; continue model stepping and unique service.
 */
nx_result_t
nx_native_spi_dma_model_hold(const nx_spi_dma_transaction_port_t* port,
                             bool hold);
/**
 * \brief           Fail a phase before the selected zero-based wire word.
 * \param[in]       port: Exact live Native enhanced controller.
 * \param[in]       fail_after: SIZE_MAX disables the injected fault.
 * \return          SUCCESS, INVALID, STATE or CONTEXT.
 * \note            Task-only model input; no replay or early loan release.
 */
nx_result_t
nx_native_spi_dma_model_fault(const nx_spi_dma_transaction_port_t* port,
                              size_t fail_after);
#ifdef __cplusplus
}
#endif
#endif
