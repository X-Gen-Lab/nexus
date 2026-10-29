/**
 * \file            stm32f407_uart_cold.h
 * \brief           Typed UART cold assembly and retained resource ownership
 * \author          Nexus Team
 */
#ifndef NX_STM32F407_UART_COLD_H
#define NX_STM32F407_UART_COLD_H

#include "stm32f407_platform_cold.h"
#include "stm32f407_stream.h"
#include "stm32f407_uart_receive.h"
#include "stm32f407_uart_word.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Exact readonly pins, vector facts, ring and frame selection. */
typedef struct {
    USART_TypeDef* registers;
    const nx_stm32_platform_pin_config_t* pins;
    const IRQn_Type* vectors;
    void* rx_storage;
    const nx_uart_format_t* format;
    size_t rx_capacity;
    uint32_t clock_hz;
    uint32_t baud;
    nx_uart_rx_profile_t profile;
    uint8_t vector_count;
    uint8_t priority;
    bool kernel_bound;
    bool preload_tx;
} nx_stm32_uart_cold_config_t;

/** \brief Static DMA transmit route; completion facts remain in the owner. */
typedef struct {
    DMA_Stream_TypeDef* registers;
    IRQn_Type irq;
    uint8_t stream;
    uint8_t channel;
} nx_stm32_uart_tx_cold_config_t;

/** \brief Selected DMA data, independent of request and producer storage. */
typedef struct {
    nx_stm32_uart_cold_config_t uart;
    nx_stm32_uart_dma_resources_t resources;
    nx_stm32_uart_tx_cold_config_t transmit;
    DMA_Stream_TypeDef* receive;
    bool continuous;
} nx_stm32_uart_dma_cold_config_t;

/** \brief Unique resource effects; driver lifecycle stays in its own state.
 * \note Pad values use the same two Platform snapshot slots. No GPIO clock,
 *       initialized flag or complete state copy is retained here.
 */
typedef struct {
    uint32_t clock_added;
    uint8_t priorities[3];
    uint8_t effects;
} nx_stm32_uart_cold_receipt_t;

/** \brief Observe privileged, unmasked Thread context without effects. */
bool nx_stm32_uart_cold_context(void);
/** \brief Acquire selected pins, gate and vector-priority observations.
 * \param[in] config: Exact static resources; Platform owns shared GPIO clocks.
 * \param[in,out] receipt: Zero owner retained through failed cleanup.
 * \return Success or refusal; every acquired effect still requires release.
 * \note Thread-only. No caller, IRQ, request or stream can already own state.
 */
nx_result_t
nx_stm32_uart_cold_acquire(const nx_stm32_uart_cold_config_t* config,
                           nx_stm32_uart_cold_receipt_t* receipt);
/** \brief Bind selected static facts before the concrete initializer. */
void nx_stm32_uart_cold_bind(nx_stm32_uart_state_t* state,
                             const nx_stm32_uart_cold_config_t* config);
/** \brief Validate only exact selected DMA routes without hardware effects. */
bool nx_stm32_uart_cold_dma_valid(const nx_stm32_uart_dma_cold_config_t* config,
                                  bool receive, bool transmit);
/** \brief Apply and read back selected priority and enabled publisher facts.
 * \param[in] config: The unchanged resources supplied to acquisition.
 * \param[in] receipt: Same retained resource observations.
 * \return Success or IO; failure leaves cleanup responsibility intact.
 */
nx_result_t
nx_stm32_uart_cold_verify(const nx_stm32_uart_cold_config_t* config,
                          const nx_stm32_uart_cold_receipt_t* receipt);
/** \brief Join vectors, restore original pins and release only added gate.
 * \param[in] config: The same static resources used by prepare.
 * \param[in,out] receipt: Retained until every selected readback proves return.
 * \return Success or error; failure allows the same close to retry.
 * \note All concrete driver calls, loans, DMA and wire publishers must drain
 *       first. A deadline or failed stop never authorizes early release.
 */
nx_result_t
nx_stm32_uart_cold_release(const nx_stm32_uart_cold_config_t* config,
                           nx_stm32_uart_cold_receipt_t* receipt);

/** \brief Close the byte/event owner after its callers have joined. */
static inline nx_result_t
nx_stm32_uart_byte_cold_close(nx_stm32_uart_state_t* state,
                              const nx_stm32_uart_cold_config_t* config,
                              nx_stm32_uart_cold_receipt_t* receipt) {
    if (!nx_stm32_uart_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || config == NULL || receipt == NULL)
        return NX_ERROR_INVALID;
    if (receipt->effects == 0U && receipt->clock_added == 0U)
        return state->initialized || state->active != NULL ? NX_ERROR_BUSY
                                                           : NX_SUCCESS;
    nx_result_t result =
        state->initialized ? nx_stm32_uart_stop(state) : NX_SUCCESS;
    if (result == NX_SUCCESS)
        result = nx_stm32_uart_cold_release(config, receipt);
    if (result == NX_SUCCESS)
        memset(state, 0, sizeof(*state));
    return result;
}
/** \brief Prepare exact byte/events; failure retains the same close owner.
 * \note Privileged unmasked Thread startup, fixed static configuration. */
static inline nx_result_t
nx_stm32_uart_byte_cold_prepare(nx_stm32_uart_state_t* state,
                                const nx_stm32_uart_cold_config_t* config,
                                nx_stm32_uart_cold_receipt_t* receipt) {
    if (!nx_stm32_uart_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || config == NULL || receipt == NULL)
        return NX_ERROR_INVALID;
    if (state->initialized || state->active != NULL)
        return NX_ERROR_BUSY;
    nx_result_t result = nx_stm32_uart_cold_acquire(config, receipt);
    if (result != NX_SUCCESS)
        return result;
    nx_stm32_uart_cold_bind(state, config);
    result = config->format == NULL
                 ? nx_stm32_uart_initialize(state, config->clock_hz)
                 : nx_stm32_uart_initialize_format(state, config->clock_hz,
                                                   *config->format);
    if (result == NX_SUCCESS)
        result = nx_stm32_uart_cold_verify(config, receipt);
    if (result != NX_SUCCESS)
        (void)nx_stm32_uart_byte_cold_close(state, config, receipt);
    return result;
}

/** \brief Close word ownership before releasing the same static resources. */
static inline nx_result_t
nx_stm32_uart_word_cold_close(nx_stm32_uart_word_state_t* state,
                              const nx_stm32_uart_cold_config_t* config,
                              nx_stm32_uart_cold_receipt_t* receipt) {
    if (!nx_stm32_uart_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || config == NULL || receipt == NULL)
        return NX_ERROR_INVALID;
    if (receipt->effects == 0U && receipt->clock_added == 0U)
        return state->owned || state->initialized || state->active != NULL
                   ? NX_ERROR_BUSY
                   : NX_SUCCESS;
    nx_result_t result = state->owned || state->initialized
                             ? nx_stm32_uart_word_ops.stop(state)
                             : NX_SUCCESS;
    if (result == NX_SUCCESS)
        result = nx_stm32_uart_cold_release(config, receipt);
    if (result == NX_SUCCESS)
        memset(state, 0, sizeof(*state));
    return result;
}
/** \brief Prepare nine-bit storage without introducing byte/DMA state. */
static inline nx_result_t
nx_stm32_uart_word_cold_prepare(nx_stm32_uart_word_state_t* state,
                                const nx_stm32_uart_cold_config_t* config,
                                nx_stm32_uart_cold_receipt_t* receipt) {
    if (!nx_stm32_uart_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || config == NULL || receipt == NULL)
        return NX_ERROR_INVALID;
    if (state->owned || state->initialized || state->active != NULL)
        return NX_ERROR_BUSY;
    nx_result_t result = nx_stm32_uart_cold_acquire(config, receipt);
    if (result != NX_SUCCESS)
        return result;
    state->registers = config->registers;
    state->irq = config->vectors[0];
    result = nx_stm32_uart_word_initialize(
        state, config->clock_hz, config->baud, (uint16_t*)config->rx_storage,
        config->rx_capacity);
    if (result == NX_SUCCESS)
        result = nx_stm32_uart_cold_verify(config, receipt);
    if (result != NX_SUCCESS)
        (void)nx_stm32_uart_word_cold_close(state, config, receipt);
    return result;
}

/** \brief Stop block and consumer loans before relinquishing cold resources. */
static inline nx_result_t
nx_stm32_uart_stream_cold_close(nx_stm32_uart_stream_state_t* state,
                                const nx_stm32_uart_cold_config_t* config,
                                nx_stm32_uart_cold_receipt_t* receipt) {
    if (!nx_stm32_uart_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || config == NULL || receipt == NULL)
        return NX_ERROR_INVALID;
    if (receipt->effects == 0U && receipt->clock_added == 0U)
        return state->uart.initialized || state->uart.active != NULL ||
                       state->stream != NULL
                   ? NX_ERROR_BUSY
                   : NX_SUCCESS;
    nx_result_t result = state->uart.initialized
                             ? nx_stm32_uart_stream_ops.stop(state)
                             : NX_SUCCESS;
    if (result == NX_SUCCESS)
        result = nx_stm32_uart_cold_release(config, receipt);
    if (result == NX_SUCCESS)
        memset(state, 0, sizeof(*state));
    return result;
}
/** \brief Prepare selected IRQ block mode with no block borrowed at startup. */
static inline nx_result_t
nx_stm32_uart_stream_cold_prepare(nx_stm32_uart_stream_state_t* state,
                                  const nx_stm32_uart_cold_config_t* config,
                                  nx_stm32_uart_cold_receipt_t* receipt) {
    if (!nx_stm32_uart_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || config == NULL || receipt == NULL)
        return NX_ERROR_INVALID;
    if (state->uart.initialized || state->uart.active != NULL ||
        state->stream != NULL)
        return NX_ERROR_BUSY;
    nx_result_t result = nx_stm32_uart_cold_acquire(config, receipt);
    if (result != NX_SUCCESS)
        return result;
    nx_stm32_uart_cold_bind(&state->uart, config);
    result = nx_stm32_uart_stream_initialize(state, config->clock_hz);
    if (result == NX_SUCCESS)
        result = nx_stm32_uart_cold_verify(config, receipt);
    if (result != NX_SUCCESS)
        (void)nx_stm32_uart_stream_cold_close(state, config, receipt);
    return result;
}

/** \brief Join finite TX DMA and wire references before cold withdrawal. */
static inline nx_result_t
nx_stm32_uart_dma_cold_close(nx_stm32_uart_dma_state_t* state,
                             const nx_stm32_uart_dma_cold_config_t* config,
                             nx_stm32_uart_cold_receipt_t* receipt) {
    if (!nx_stm32_uart_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || config == NULL || receipt == NULL)
        return NX_ERROR_INVALID;
    if (receipt->effects == 0U && receipt->clock_added == 0U)
        return state->uart.initialized || state->uart.active != NULL
                   ? NX_ERROR_BUSY
                   : NX_SUCCESS;
    nx_result_t result = state->uart.initialized
                             ? nx_stm32_uart_dma_ops.stop(state)
                             : NX_SUCCESS;
    if (result == NX_SUCCESS)
        result = nx_stm32_uart_cold_release(&config->uart, receipt);
    if (result == NX_SUCCESS)
        memset(state, 0, sizeof(*state));
    return result;
}
/** \brief Prepare exact UART1/UART2 finite TX and its selected IRQ ring. */
static inline nx_result_t
nx_stm32_uart_dma_cold_prepare(nx_stm32_uart_dma_state_t* state,
                               const nx_stm32_uart_dma_cold_config_t* config,
                               nx_stm32_uart_cold_receipt_t* receipt) {
    if (!nx_stm32_uart_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || receipt == NULL ||
        !nx_stm32_uart_cold_dma_valid(config, false, true))
        return NX_ERROR_INVALID;
    if (state->uart.initialized || state->uart.active != NULL)
        return NX_ERROR_BUSY;
    if ((config->transmit.registers->CR & DMA_SxCR_EN) != 0U)
        return NX_ERROR_BUSY;
    nx_result_t result = nx_stm32_uart_cold_acquire(&config->uart, receipt);
    if (result != NX_SUCCESS)
        return result;
    nx_stm32_uart_cold_bind(&state->uart, &config->uart);
    state->resources = config->resources;
    state->transmit.tx = config->transmit.registers;
    state->transmit.dma_irq = config->transmit.irq;
    state->transmit.stream = config->transmit.stream;
    state->transmit.channel = config->transmit.channel;
    result = nx_stm32_uart_dma_initialize(state, config->uart.clock_hz);
    if (result == NX_SUCCESS)
        result = nx_stm32_uart_cold_verify(&config->uart, receipt);
    if (result != NX_SUCCESS)
        (void)nx_stm32_uart_dma_cold_close(state, config, receipt);
    return result;
}

/** \brief Drain DMA producers and consumers before restoring cold resources. */
static inline nx_result_t
nx_stm32_uart_receive_cold_close(nx_stm32_uart_receive_state_t* state,
                                 const nx_stm32_uart_dma_cold_config_t* config,
                                 nx_stm32_uart_cold_receipt_t* receipt) {
    if (!nx_stm32_uart_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || config == NULL || receipt == NULL)
        return NX_ERROR_INVALID;
    if (receipt->effects == 0U && receipt->clock_added == 0U)
        return state->uart.initialized || state->uart.active != NULL ||
                       state->stream != NULL
                   ? NX_ERROR_BUSY
                   : NX_SUCCESS;
    nx_result_t result = state->uart.initialized
                             ? nx_stm32_uart_receive_ops.stop(state)
                             : NX_SUCCESS;
    if (result == NX_SUCCESS)
        result = nx_stm32_uart_cold_release(&config->uart, receipt);
    if (result == NX_SUCCESS)
        memset(state, 0, sizeof(*state));
    return result;
}
/** \brief Prepare finite/continuous RX with an explicit IRQ TX owner. */
static inline nx_result_t nx_stm32_uart_receive_cold_prepare(
    nx_stm32_uart_receive_state_t* state,
    const nx_stm32_uart_dma_cold_config_t* config,
    nx_stm32_uart_cold_receipt_t* receipt) {
    if (!nx_stm32_uart_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || receipt == NULL ||
        !nx_stm32_uart_cold_dma_valid(config, true, false))
        return NX_ERROR_INVALID;
    if (state->uart.initialized || state->uart.active != NULL ||
        state->stream != NULL)
        return NX_ERROR_BUSY;
    if ((config->receive->CR & DMA_SxCR_EN) != 0U)
        return NX_ERROR_BUSY;
    nx_result_t result = nx_stm32_uart_cold_acquire(&config->uart, receipt);
    if (result != NX_SUCCESS)
        return result;
    nx_stm32_uart_cold_bind(&state->uart, &config->uart);
    state->resources = config->resources;
    state->receive = config->receive;
    state->continuous = config->continuous;
    result = nx_stm32_uart_receive_initialize(state, config->uart.clock_hz);
    if (result == NX_SUCCESS)
        result = nx_stm32_uart_cold_verify(&config->uart, receipt);
    if (result != NX_SUCCESS)
        (void)nx_stm32_uart_receive_cold_close(state, config, receipt);
    return result;
}

/** \brief Join independent RX loans and TX DMA/wire before cold release. */
static inline nx_result_t
nx_stm32_uart_duplex_cold_close(nx_stm32_uart_duplex_state_t* state,
                                const nx_stm32_uart_dma_cold_config_t* config,
                                nx_stm32_uart_cold_receipt_t* receipt) {
    if (!nx_stm32_uart_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || config == NULL || receipt == NULL)
        return NX_ERROR_INVALID;
    if (receipt->effects == 0U && receipt->clock_added == 0U)
        return state->receive.uart.initialized ||
                       state->receive.uart.active != NULL ||
                       state->receive.stream != NULL
                   ? NX_ERROR_BUSY
                   : NX_SUCCESS;
    nx_result_t result = state->receive.uart.initialized
                             ? nx_stm32_uart_duplex_ops.stop(state)
                             : NX_SUCCESS;
    if (result == NX_SUCCESS)
        result = nx_stm32_uart_cold_release(&config->uart, receipt);
    if (result == NX_SUCCESS)
        memset(state, 0, sizeof(*state));
    return result;
}
/** \brief Prepare exact RX plus finite TX without enlarging RX-only state. */
static inline nx_result_t
nx_stm32_uart_duplex_cold_prepare(nx_stm32_uart_duplex_state_t* state,
                                  const nx_stm32_uart_dma_cold_config_t* config,
                                  nx_stm32_uart_cold_receipt_t* receipt) {
    if (!nx_stm32_uart_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || receipt == NULL ||
        !nx_stm32_uart_cold_dma_valid(config, true, true))
        return NX_ERROR_INVALID;
    if (state->receive.uart.initialized || state->receive.uart.active != NULL ||
        state->receive.stream != NULL)
        return NX_ERROR_BUSY;
    if ((config->receive->CR & DMA_SxCR_EN) != 0U ||
        (config->transmit.registers->CR & DMA_SxCR_EN) != 0U)
        return NX_ERROR_BUSY;
    nx_result_t result = nx_stm32_uart_cold_acquire(&config->uart, receipt);
    if (result != NX_SUCCESS)
        return result;
    nx_stm32_uart_cold_bind(&state->receive.uart, &config->uart);
    state->receive.resources = config->resources;
    state->receive.receive = config->receive;
    state->receive.continuous = config->continuous;
    state->transmit.tx = config->transmit.registers;
    state->transmit.dma_irq = config->transmit.irq;
    state->transmit.stream = config->transmit.stream;
    state->transmit.channel = config->transmit.channel;
    result = nx_stm32_uart_duplex_initialize(state, config->uart.clock_hz);
    if (result == NX_SUCCESS)
        result = nx_stm32_uart_cold_verify(&config->uart, receipt);
    if (result != NX_SUCCESS)
        (void)nx_stm32_uart_duplex_cold_close(state, config, receipt);
    return result;
}

#ifdef __cplusplus
}
#endif
#endif
