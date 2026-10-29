/**
 * \file            spi_dma_sequence.h
 * \brief           Optional finite SPI sequence owner and hardware boundaries
 * \author          Nexus Team
 */
#ifndef NEXUS_PRIVATE_SPI_DMA_SEQUENCE_H
#define NEXUS_PRIVATE_SPI_DMA_SEQUENCE_H
#include "nexus/io/spi_dma_transaction.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Backend owns hardware; sequence owns all outer request loans. */
typedef struct {
    nx_result_t (*validate)(void* context, void* endpoint,
                            const nx_spi_dma_transaction_request_t* request);
    nx_result_t (*begin)(void* context, void* endpoint, nx_spi_request_t* phase,
                         uint8_t word_bits, uint8_t memory_flags,
                         bool retained_cs);
    void (*advance)(void* context);
    nx_result_t (*cancel)(void* context, nx_spi_request_t* phase);
    nx_result_t (*release)(void* context, void* endpoint);
    nx_result_t (*stop)(void* context);
} nx_spi_dma_sequence_backend_t;
/** \brief Selected mode alone reserves this state; legacy states never grow. */
typedef struct {
    const nx_spi_dma_sequence_backend_t* backend;
    void* context;
    nx_spi_dma_transaction_request_t* active;
    void* endpoint;
    /* Private phase length is hardware words, interpreted with word_bits by
     * backend begin; it is never submitted through the legacy byte API. */
    nx_spi_request_t phase_request;
    size_t phase;
    size_t completed;
    nx_result_t terminal;
    bool cancelled;
    bool cs_held;
    bool pending_release;
    bool stopping;
    bool stopped;
} nx_spi_dma_sequence_state_t;
/** \brief Endpoint identity and controller state remain distinct. */
typedef struct {
    nx_spi_dma_sequence_state_t* controller;
    void* endpoint;
} nx_spi_dma_sequence_endpoint_t;
/**
 * \brief           Admit cold initialization without binding or hardware.
 * \param[in]       state: Same static sequence, with all submitters joined.
 * \param[in]       backend: Complete stable immutable hardware methods.
 * \param[in]       context: Stable separately owned provider hardware state.
 * \return          SUCCESS, INVALID, CONTEXT or BUSY; state remains unchanged.
 * \note            Task-only. Call before provider preparation or acquiring a
 *                  cleanup responsibility. Rejection preserves active phases,
 *                  failed stop duty, outer loans and existing binding identity.
 *                  No backend method, allocation or source acquisition occurs.
 */
nx_result_t
nx_spi_dma_sequence_preflight(const nx_spi_dma_sequence_state_t* state,
                              const nx_spi_dma_sequence_backend_t* backend,
                              const void* context);
/**
 * \brief           Initialize cold or fully quiescent mode-only owner storage.
 * \param[in,out]   state: Cold, quiescent same-binding or stopped state.
 * \param[in]       backend: Complete stable immutable hardware methods.
 * \param[in]       context: Stable separately owned provider hardware state.
 * \return          SUCCESS, INVALID, CONTEXT or BUSY with live/retained loans.
 * \note            Task-only; caller has joined all users/observers. No
 *                  hardware operation, source acquisition or loan revocation.
 *                  Existing non-NULL backend/context identities never change.
 *                  Failed stop responsibility cannot be cleared by initialize.
 */
nx_result_t
nx_spi_dma_sequence_initialize(nx_spi_dma_sequence_state_t* state,
                               const nx_spi_dma_sequence_backend_t* backend,
                               void* context);
/**
 * \brief           Reject any caller span aliasing protected provider storage.
 * \note            Request fields have already passed whole-chain validation.
 *                  This cold bounded check never reads protected storage.
 */
nx_result_t nx_spi_dma_sequence_storage_validate(
    const nx_spi_dma_transaction_request_t* request, const void* storage,
    size_t bytes);
extern const nx_spi_dma_transaction_endpoint_ops_t
    nx_spi_dma_sequence_endpoint_ops;
extern const nx_spi_dma_transaction_port_ops_t nx_spi_dma_sequence_port_ops;
#ifdef __cplusplus
}
#endif
#endif
