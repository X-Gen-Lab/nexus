/**
 * \file            gd32f470_uart_cold.h
 * \brief           Typed UART construction and retained cold resources
 * \author          Nexus Team
 */
#ifndef NEXUS_GD32F470_UART_COLD_H
#define NEXUS_GD32F470_UART_COLD_H
#include "gd32f470_cold_pins.h"
#include "gd32f470_uart_receive.h"
#include "gd32f470_uart_stream.h"
#include "gd32f470_uart_word.h"
#include "nexus/arch/arch.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif
/** \brief Immutable exact pads, snapshot slots, publishers and ring facts.
 * \note Priority storage has exactly vector_count bytes. Platform and this
 *       constructor share the same two original pin snapshots; no second pin
 *       copy, hidden ring or runtime registry is allocated.
 */
typedef struct {
    const nx_gd32_uart_controller_t* controller;
    const nx_gd32_cold_pin_config_t* pins;
    uint16_t* snapshots;
    const IRQn_Type* vectors;
    uint8_t* priorities;
    void* receive;
    const nx_uart_format_t* format;
    size_t capacity;
    uint32_t baud;
    nx_uart_rx_profile_t profile;
    uint8_t vector_count;
    uint8_t priority;
    bool kernel_bound;
    bool preload_transmit;
} nx_gd32_uart_cold_config_t;
/** \brief Only selected DMA modes retain immutable DMA visibility facts. */
typedef struct {
    nx_gd32_uart_cold_config_t uart;
    nx_gd32_uart_dma_resources_t resources;
    bool continuous;
} nx_gd32_uart_dma_cold_config_t;
/** \brief One-byte acquired effects; concrete state owns initialized/drain.
 * \note Failed close retains this exact receipt and original priority bytes.
 */
typedef struct {
    uint8_t effects;
} nx_gd32_uart_cold_t;
/** \brief Pure observation of privileged, unmasked Thread startup context. */
bool nx_gd32_uart_cold_context(void);
/** \brief Validate exact DMA lanes and visibility before cold acquisition. */
bool nx_gd32_uart_cold_dma_valid(const nx_gd32_uart_dma_cold_config_t* config,
                                 bool receive, bool transmit);
/** \brief Acquire exact pads and priority observations for a stopped UART.
 * \param[in] config: Static resources; Platform owns GPIO/DMA clock leases.
 * \param[in,out] cold: Initially zero receipt, retained after any failure.
 * \return Success or error; acquired effects always require checked close.
 * \note Task-only, serialized and unmasked. No transfer is admitted here.
 */
nx_result_t nx_gd32_uart_cold_acquire(const nx_gd32_uart_cold_config_t* config,
                                      nx_gd32_uart_cold_t* cold);
/** \brief Observe selected gate, UART enable and each publisher after init. */
nx_result_t nx_gd32_uart_cold_verify(const nx_gd32_uart_cold_config_t* config,
                                     const nx_gd32_uart_cold_t* cold);
/** \brief Restore joined vectors, original pads and the acquired UART gate.
 * \param[in] config: The unchanged selected configuration and original arrays.
 * \param[in,out] cold: Retained receipt; cleared only after all readbacks.
 * \return Success or error retaining every unproved effect for retry.
 * \note Concrete driver stop and all request/block borrowers must join first.
 */
nx_result_t nx_gd32_uart_cold_release(const nx_gd32_uart_cold_config_t* config,
                                      nx_gd32_uart_cold_t* cold);

/** \brief Join the concrete byte owner before returning its cold resources. */
static inline nx_result_t
nx_gd32_uart_byte_cold_close(nx_gd32_uart_state_t* state,
                             const nx_gd32_uart_cold_config_t* config,
                             nx_gd32_uart_cold_t* cold) {
    if (!nx_gd32_uart_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || config == NULL || cold == NULL)
        return NX_ERROR_INVALID;
    /* Rejected admission never grants authority over an existing owner. */
    if (cold->effects == 0U)
        return state->initialized || state->active != NULL ? NX_ERROR_BUSY
                                                           : NX_SUCCESS;
    nx_result_t result =
        state->initialized ? nx_gd32_uart_stop(state) : NX_SUCCESS;
    if (result == NX_SUCCESS)
        result = nx_gd32_uart_cold_release(config, cold);
    if (result == NX_SUCCESS)
        memset(state, 0, sizeof(*state));
    return result;
}
/** \brief Prepare exact byte storage; every failed effect keeps the same close.
 * \note Privileged unmasked serialized startup, with no request or block loan.
 */
static inline nx_result_t
nx_gd32_uart_byte_cold_prepare(nx_gd32_uart_state_t* state,
                               const nx_gd32_uart_cold_config_t* config,
                               nx_gd32_uart_cold_t* cold) {
    if (!nx_gd32_uart_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || config == NULL || cold == NULL)
        return NX_ERROR_INVALID;
    if (state->initialized || state->active != NULL)
        return NX_ERROR_BUSY;
    if (config->receive == NULL || config->capacity == 0U ||
        (config->profile != NX_UART_RX_BYTES &&
         config->profile != NX_UART_RX_EVENTS))
        return NX_ERROR_INVALID;
    if (config->format != NULL) {
        nx_result_t valid = nx_uart_format_validate(*config->format);
        if (valid != NX_SUCCESS || config->format->data_bits == 9U)
            return valid == NX_SUCCESS ? NX_ERROR_UNSUPPORTED : valid;
    }
    nx_result_t result = nx_gd32_uart_cold_acquire(config, cold);
    if (result != NX_SUCCESS)
        return result;
    result = config->format == NULL
                 ? nx_gd32_uart_initialize_at(
                       state, config->controller, config->baud, config->profile,
                       config->receive, config->capacity, config->priority)
                 : nx_gd32_uart_initialize_format_at(
                       state, config->controller, config->baud, config->profile,
                       config->receive, config->capacity, config->priority,
                       *config->format);
    if (result == NX_SUCCESS)
        result = nx_gd32_uart_cold_verify(config, cold);
    if (result != NX_SUCCESS)
        (void)nx_gd32_uart_byte_cold_close(state, config, cold);
    return result;
}

/** \brief Join the concrete word owner before returning its cold resources. */
static inline nx_result_t
nx_gd32_uart_word_cold_close(nx_gd32_uart_word_state_t* state,
                             const nx_gd32_uart_cold_config_t* config,
                             nx_gd32_uart_cold_t* cold) {
    if (!nx_gd32_uart_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || config == NULL || cold == NULL)
        return NX_ERROR_INVALID;
    /* Rejected admission never grants authority over an existing owner. */
    if (cold->effects == 0U)
        return state->owned || state->initialized || state->active != NULL
                   ? NX_ERROR_BUSY
                   : NX_SUCCESS;
    nx_result_t result = state->owned || state->initialized
                             ? nx_gd32_uart_word_ops.stop(state)
                             : NX_SUCCESS;
    if (result == NX_SUCCESS)
        result = nx_gd32_uart_cold_release(config, cold);
    if (result == NX_SUCCESS)
        memset(state, 0, sizeof(*state));
    return result;
}
/** \brief Prepare exact word storage; every failed effect keeps the same close.
 * \note Privileged unmasked serialized startup, with no request or block loan.
 */
static inline nx_result_t
nx_gd32_uart_word_cold_prepare(nx_gd32_uart_word_state_t* state,
                               const nx_gd32_uart_cold_config_t* config,
                               nx_gd32_uart_cold_t* cold) {
    if (!nx_gd32_uart_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || config == NULL || cold == NULL)
        return NX_ERROR_INVALID;
    if (state->owned || state->initialized || state->active != NULL)
        return NX_ERROR_BUSY;
    if (config->receive == NULL || config->capacity == 0U)
        return NX_ERROR_INVALID;
    nx_result_t result = nx_gd32_uart_cold_acquire(config, cold);
    if (result != NX_SUCCESS)
        return result;
    result = nx_gd32_uart_word_initialize(
        state, config->controller, config->baud, (uint16_t*)config->receive,
        config->capacity, config->priority);
    if (result == NX_SUCCESS)
        result = nx_gd32_uart_cold_verify(config, cold);
    if (result != NX_SUCCESS)
        (void)nx_gd32_uart_word_cold_close(state, config, cold);
    return result;
}

/** \brief Join the concrete stream owner before returning its cold resources.
 */
static inline nx_result_t
nx_gd32_uart_stream_cold_close(nx_gd32_uart_stream_state_t* state,
                               const nx_gd32_uart_cold_config_t* config,
                               nx_gd32_uart_cold_t* cold) {
    if (!nx_gd32_uart_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || config == NULL || cold == NULL)
        return NX_ERROR_INVALID;
    /* Rejected admission never grants authority over an existing owner. */
    if (cold->effects == 0U)
        return state->uart.initialized || state->uart.active != NULL ||
                       state->stream != NULL
                   ? NX_ERROR_BUSY
                   : NX_SUCCESS;
    nx_result_t result = state->uart.initialized
                             ? nx_gd32_uart_stream_ops.stop(state)
                             : NX_SUCCESS;
    if (result == NX_SUCCESS)
        result = nx_gd32_uart_cold_release(config, cold);
    if (result == NX_SUCCESS)
        memset(state, 0, sizeof(*state));
    return result;
}
/** \brief Prepare exact stream storage; every failed effect keeps the same
 * close. \note Privileged unmasked serialized startup, with no request or block
 * loan.
 */
static inline nx_result_t
nx_gd32_uart_stream_cold_prepare(nx_gd32_uart_stream_state_t* state,
                                 const nx_gd32_uart_cold_config_t* config,
                                 nx_gd32_uart_cold_t* cold) {
    if (!nx_gd32_uart_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || config == NULL || cold == NULL)
        return NX_ERROR_INVALID;
    if (state->uart.initialized || state->uart.active != NULL ||
        state->stream != NULL)
        return NX_ERROR_BUSY;
    nx_result_t result = nx_gd32_uart_cold_acquire(config, cold);
    if (result != NX_SUCCESS)
        return result;
    result = nx_gd32_uart_stream_initialize_at(state, config->controller,
                                               config->baud, config->priority);
    if (result == NX_SUCCESS)
        result = nx_gd32_uart_cold_verify(config, cold);
    if (result != NX_SUCCESS)
        (void)nx_gd32_uart_stream_cold_close(state, config, cold);
    return result;
}

/** \brief Join the concrete dma owner before returning its cold resources. */
static inline nx_result_t
nx_gd32_uart_dma_cold_close(nx_gd32_uart_dma_state_t* state,
                            const nx_gd32_uart_dma_cold_config_t* config,
                            nx_gd32_uart_cold_t* cold) {
    if (!nx_gd32_uart_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || config == NULL || cold == NULL)
        return NX_ERROR_INVALID;
    /* Rejected admission never grants authority over an existing owner. */
    if (cold->effects == 0U)
        return state->uart.initialized || state->uart.active != NULL
                   ? NX_ERROR_BUSY
                   : NX_SUCCESS;
    nx_result_t result =
        state->uart.initialized ? nx_gd32_uart_dma_ops.stop(state) : NX_SUCCESS;
    if (result == NX_SUCCESS)
        result = nx_gd32_uart_cold_release(&config->uart, cold);
    if (result == NX_SUCCESS)
        memset(state, 0, sizeof(*state));
    return result;
}
/** \brief Prepare exact dma storage; every failed effect keeps the same close.
 * \note Privileged unmasked serialized startup, with no request or block loan.
 */
static inline nx_result_t
nx_gd32_uart_dma_cold_prepare(nx_gd32_uart_dma_state_t* state,
                              const nx_gd32_uart_dma_cold_config_t* config,
                              nx_gd32_uart_cold_t* cold) {
    if (!nx_gd32_uart_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || config == NULL || cold == NULL)
        return NX_ERROR_INVALID;
    if (!nx_gd32_uart_cold_dma_valid(config, false, true))
        return NX_ERROR_INVALID;
    if (state->uart.initialized || state->uart.active != NULL)
        return NX_ERROR_BUSY;
    nx_result_t result = nx_gd32_uart_cold_acquire(&config->uart, cold);
    if (result != NX_SUCCESS)
        return result;
    state->resources = config->resources;
    result = nx_gd32_uart_dma_initialize_at(
        state, config->uart.controller, config->uart.baud, config->uart.profile,
        config->uart.receive, config->uart.capacity, config->uart.priority);
    if (result == NX_SUCCESS)
        result = nx_gd32_uart_cold_verify(&config->uart, cold);
    if (result != NX_SUCCESS)
        (void)nx_gd32_uart_dma_cold_close(state, config, cold);
    return result;
}

/** \brief Join the concrete receive owner before returning its cold resources.
 */
static inline nx_result_t
nx_gd32_uart_receive_cold_close(nx_gd32_uart_receive_state_t* state,
                                const nx_gd32_uart_dma_cold_config_t* config,
                                nx_gd32_uart_cold_t* cold) {
    if (!nx_gd32_uart_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || config == NULL || cold == NULL)
        return NX_ERROR_INVALID;
    /* Rejected admission never grants authority over an existing owner. */
    if (cold->effects == 0U)
        return state->uart.initialized || state->uart.active != NULL ||
                       state->stream != NULL
                   ? NX_ERROR_BUSY
                   : NX_SUCCESS;
    nx_result_t result = state->uart.initialized
                             ? nx_gd32_uart_receive_ops.stop(state)
                             : NX_SUCCESS;
    if (result == NX_SUCCESS)
        result = nx_gd32_uart_cold_release(&config->uart, cold);
    if (result == NX_SUCCESS)
        memset(state, 0, sizeof(*state));
    return result;
}
/** \brief Prepare exact receive storage; every failed effect keeps the same
 * close. \note Privileged unmasked serialized startup, with no request or block
 * loan.
 */
static inline nx_result_t
nx_gd32_uart_receive_cold_prepare(nx_gd32_uart_receive_state_t* state,
                                  const nx_gd32_uart_dma_cold_config_t* config,
                                  nx_gd32_uart_cold_t* cold) {
    if (!nx_gd32_uart_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || config == NULL || cold == NULL)
        return NX_ERROR_INVALID;
    if (!nx_gd32_uart_cold_dma_valid(config, true, false))
        return NX_ERROR_INVALID;
    if (state->uart.initialized || state->uart.active != NULL ||
        state->stream != NULL)
        return NX_ERROR_BUSY;
    nx_result_t result = nx_gd32_uart_cold_acquire(&config->uart, cold);
    if (result != NX_SUCCESS)
        return result;
    state->resources = config->resources;
    state->continuous = config->continuous;
    result = nx_gd32_uart_receive_initialize(state, config->uart.baud,
                                             config->uart.priority);
    if (result == NX_SUCCESS)
        result = nx_gd32_uart_cold_verify(&config->uart, cold);
    if (result != NX_SUCCESS)
        (void)nx_gd32_uart_receive_cold_close(state, config, cold);
    return result;
}

/** \brief Join the concrete duplex owner before returning its cold resources.
 */
static inline nx_result_t
nx_gd32_uart_duplex_cold_close(nx_gd32_uart_duplex_state_t* state,
                               const nx_gd32_uart_dma_cold_config_t* config,
                               nx_gd32_uart_cold_t* cold) {
    if (!nx_gd32_uart_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || config == NULL || cold == NULL)
        return NX_ERROR_INVALID;
    /* Rejected admission never grants authority over an existing owner. */
    if (cold->effects == 0U)
        return state->receive.uart.initialized ||
                       state->receive.uart.active != NULL ||
                       state->receive.stream != NULL
                   ? NX_ERROR_BUSY
                   : NX_SUCCESS;
    nx_result_t result = state->receive.uart.initialized
                             ? nx_gd32_uart_duplex_ops.stop(state)
                             : NX_SUCCESS;
    if (result == NX_SUCCESS)
        result = nx_gd32_uart_cold_release(&config->uart, cold);
    if (result == NX_SUCCESS)
        memset(state, 0, sizeof(*state));
    return result;
}
/** \brief Prepare exact duplex storage; every failed effect keeps the same
 * close. \note Privileged unmasked serialized startup, with no request or block
 * loan.
 */
static inline nx_result_t
nx_gd32_uart_duplex_cold_prepare(nx_gd32_uart_duplex_state_t* state,
                                 const nx_gd32_uart_dma_cold_config_t* config,
                                 nx_gd32_uart_cold_t* cold) {
    if (!nx_gd32_uart_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || config == NULL || cold == NULL)
        return NX_ERROR_INVALID;
    if (!nx_gd32_uart_cold_dma_valid(config, true, true))
        return NX_ERROR_INVALID;
    if (state->receive.uart.initialized || state->receive.uart.active != NULL ||
        state->receive.stream != NULL)
        return NX_ERROR_BUSY;
    nx_result_t result = nx_gd32_uart_cold_acquire(&config->uart, cold);
    if (result != NX_SUCCESS)
        return result;
    state->receive.resources = config->resources;
    state->receive.continuous = config->continuous;
    result = nx_gd32_uart_duplex_initialize(state, config->uart.baud,
                                            config->uart.priority);
    if (result == NX_SUCCESS)
        result = nx_gd32_uart_cold_verify(&config->uart, cold);
    if (result != NX_SUCCESS)
        (void)nx_gd32_uart_duplex_cold_close(state, config, cold);
    return result;
}

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_GD32F470_UART_COLD_H */
