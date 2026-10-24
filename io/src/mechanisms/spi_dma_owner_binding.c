/**
 * \file            spi_dma_owner_binding.c
 * \brief           Checked optional controller identity and wake dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/spi_dma_owner_binding.h"
#include "nexus/arch/arch.h"
#include "spi_dma_sequence_owner.h"

/** \brief A capability does not convert an absent or malformed IO face. */
static bool valid(const nx_spi_dma_owner_binding_t* binding) {
    return binding != NULL && binding->port != NULL &&
           binding->port->ops != NULL && binding->port->context != NULL;
}
bool nx_spi_dma_owner_contains(
    const nx_spi_dma_owner_binding_t* binding,
    const nx_spi_dma_transaction_endpoint_t* endpoint) {
    return valid(binding) && endpoint != NULL && endpoint->ops != NULL &&
           endpoint->context != NULL && binding->contains != NULL &&
           binding->contains(endpoint, binding->port);
}
nx_result_t
nx_spi_dma_owner_attach_wake(const nx_spi_dma_owner_binding_t* binding,
                             const nx_irq_wake_t* wake,
                             uint8_t syscall_ceiling) {
    if (!valid(binding)) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    return binding->attach_wake == NULL
               ? NX_ERROR_UNSUPPORTED
               : binding->attach_wake(binding->port, wake, syscall_ceiling);
}

/** \brief Identity remains separate from admission and lifecycle state. */
bool nx_spi_dma_sequence_contains(
    const nx_spi_dma_transaction_endpoint_t* endpoint,
    const nx_spi_dma_transaction_port_t* port) {
    if (endpoint == NULL || port == NULL || endpoint->context == NULL ||
        port->context == NULL ||
        endpoint->ops != &nx_spi_dma_sequence_endpoint_ops ||
        port->ops != &nx_spi_dma_sequence_port_ops) {
        return false;
    }
    const nx_spi_dma_sequence_endpoint_t* child = endpoint->context;
    return child->endpoint != NULL && child->controller == port->context;
}
