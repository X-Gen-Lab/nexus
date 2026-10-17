/**
 * \file            spi_dma_provider.h
 * \brief           Private selected-only Native segmented wire model state
 * \author          Nexus Team
 */
#ifndef NEXUS_NATIVE_SPI_DMA_PROVIDER_H
#define NEXUS_NATIVE_SPI_DMA_PROVIDER_H
#include "spi_dma_sequence_owner.h"
#ifdef __cplusplus
extern "C" {
#endif
struct nx_native_spi_dma_endpoint_state;
/** \brief Independent model controller; existing byte-SPI state never grows. */
typedef struct {
    const nx_irq_wake_t* wake;
    nx_spi_request_t* active;
    struct nx_native_spi_dma_endpoint_state* selected;
    size_t shifted;
    size_t fail_after;
    nx_result_t terminal_result;
    uint8_t word_bits;
    uint8_t memory_flags;
    bool opened;
    bool stopping;
    bool automatic_irq;
    bool terminal;
    bool wire_idle;
    bool hold_drain;
} nx_native_spi_dma_state_t;
/** \brief Exact CS identity and copied model facts, without payload storage. */
typedef struct nx_native_spi_dma_endpoint_state {
    nx_native_spi_dma_state_t* controller;
    bool cs;
    size_t assertions;
    size_t releases;
} nx_native_spi_dma_endpoint_state_t;
/**
 * \brief           Open fresh or completely stopped model controller storage.
 * \note            Task-only, external lifecycle users joined. Rejects live
 *                  state instead of clearing any retained request or CS.
 */
nx_result_t nx_native_spi_dma_initialize(nx_native_spi_dma_state_t* state);
extern const nx_spi_dma_sequence_backend_t nx_native_spi_dma_sequence_backend;
/** \brief Check logical and actual Native endpoint controller identity. */
bool nx_native_spi_dma_owner_contains(
    const nx_spi_dma_transaction_endpoint_t* endpoint,
    const nx_spi_dma_transaction_port_t* port);
/** \brief Borrow a validated model IRQ sink for this exact live owner. */
nx_result_t
nx_native_spi_dma_owner_attach_wake(const nx_spi_dma_transaction_port_t* port,
                                    const nx_irq_wake_t* wake,
                                    uint8_t syscall_ceiling);
#ifdef __cplusplus
}
#endif
#endif
