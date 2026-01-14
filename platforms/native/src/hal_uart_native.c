/**
 * \file            hal_uart_native.c
 * \brief           Native Platform UART HAL Implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * This implementation uses ring buffers to simulate UART communication
 * for testing purposes on the native platform.
 */

#include "hal/hal_uart.h"
#include "native_platform.h"
#include <stdio.h>
#include <string.h>

/*===========================================================================*/
/* Local definitions                                                          */
/*===========================================================================*/

#define MAX_UART_INSTANCES 4
#define UART_BUFFER_SIZE   256

/**
 * \brief           Ring buffer structure
 */
typedef struct {
    uint8_t buffer[UART_BUFFER_SIZE];
    size_t head;
    size_t tail;
    size_t count;
} ring_buffer_t;

/**
 * \brief           UART state structure
 */
typedef struct {
    bool initialized;
    hal_uart_config_t config;
    ring_buffer_t rx_buffer;
    ring_buffer_t tx_buffer;
    hal_uart_rx_callback_t rx_callback;
    hal_uart_tx_callback_t tx_callback;
    void* rx_context;
    void* tx_context;
    uint32_t actual_baudrate;
} uart_state_t;

static uart_state_t uart_state[MAX_UART_INSTANCES];

/*===========================================================================*/
/* Local functions                                                            */
/*===========================================================================*/

/**
 * \brief           Initialize ring buffer
 * \param[in]       rb: Pointer to ring buffer
 */
static void ring_buffer_init(ring_buffer_t* rb) {
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    memset(rb->buffer, 0, UART_BUFFER_SIZE);
}

/**
 * \brief           Check if ring buffer is empty
 * \param[in]       rb: Pointer to ring buffer
 * \return          true if empty, false otherwise
 */
static bool ring_buffer_is_empty(const ring_buffer_t* rb) {
    return rb->count == 0;
}

/**
 * \brief           Check if ring buffer is full
 * \param[in]       rb: Pointer to ring buffer
 * \return          true if full, false otherwise
 */
static bool ring_buffer_is_full(const ring_buffer_t* rb) {
    return rb->count >= UART_BUFFER_SIZE;
}

/**
 * \brief           Put byte into ring buffer
 * \param[in]       rb: Pointer to ring buffer
 * \param[in]       byte: Byte to put
 * \return          true on success, false if buffer full
 */
static bool ring_buffer_put(ring_buffer_t* rb, uint8_t byte) {
    if (ring_buffer_is_full(rb)) {
        return false;
    }
    rb->buffer[rb->head] = byte;
    rb->head = (rb->head + 1) % UART_BUFFER_SIZE;
    rb->count++;
    return true;
}

/**
 * \brief           Get byte from ring buffer
 * \param[in]       rb: Pointer to ring buffer
 * \param[out]      byte: Pointer to store byte
 * \return          true on success, false if buffer empty
 */
static bool ring_buffer_get(ring_buffer_t* rb, uint8_t* byte) {
    if (ring_buffer_is_empty(rb)) {
        return false;
    }
    *byte = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % UART_BUFFER_SIZE;
    rb->count--;
    return true;
}

/**
 * \brief           Calculate actual baudrate (simulated)
 * \param[in]       requested: Requested baudrate
 * \return          Actual baudrate (with simulated error < 2%)
 *
 * This simulates the baudrate calculation that would happen on real hardware.
 * The actual baudrate is calculated to be within 2% of the requested value.
 */
static uint32_t calculate_actual_baudrate(uint32_t requested) {
    /* Simulate a typical MCU clock divider calculation */
    /* For simulation, we return the exact requested baudrate */
    /* In real hardware, there would be some error due to clock division */
    return requested;
}

/*===========================================================================*/
/* Public functions - Test helpers                                            */
/*===========================================================================*/

void native_uart_reset_all(void) {
    memset(uart_state, 0, sizeof(uart_state));
}

native_uart_state_t* native_uart_get_state(int instance) {
    if (instance < 0 || instance >= MAX_UART_INSTANCES) {
        return NULL;
    }
    return (native_uart_state_t*)&uart_state[instance];
}

bool native_uart_inject_rx_data(int instance, const uint8_t* data, size_t len) {
    if (instance < 0 || instance >= MAX_UART_INSTANCES || data == NULL) {
        return false;
    }
    if (!uart_state[instance].initialized) {
        return false;
    }

    for (size_t i = 0; i < len; i++) {
        if (!ring_buffer_put(&uart_state[instance].rx_buffer, data[i])) {
            return false; /* Buffer full */
        }
        /* Invoke callback if registered */
        if (uart_state[instance].rx_callback != NULL) {
            uart_state[instance].rx_callback((hal_uart_instance_t)instance,
                                             data[i],
                                             uart_state[instance].rx_context);
        }
    }
    return true;
}

size_t native_uart_get_tx_data(int instance, uint8_t* data, size_t max_len) {
    if (instance < 0 || instance >= MAX_UART_INSTANCES || data == NULL) {
        return 0;
    }
    if (!uart_state[instance].initialized) {
        return 0;
    }

    size_t count = 0;
    while (count < max_len) {
        uint8_t byte;
        if (!ring_buffer_get(&uart_state[instance].tx_buffer, &byte)) {
            break;
        }
        data[count++] = byte;
    }
    return count;
}

uint32_t native_uart_get_actual_baudrate(int instance) {
    if (instance < 0 || instance >= MAX_UART_INSTANCES) {
        return 0;
    }
    if (!uart_state[instance].initialized) {
        return 0;
    }
    return uart_state[instance].actual_baudrate;
}

/*===========================================================================*/
/* Public functions - HAL API                                                 */
/*===========================================================================*/

hal_status_t hal_uart_init(hal_uart_instance_t instance,
                           const hal_uart_config_t* config) {
    if (instance >= MAX_UART_INSTANCES) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (config == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }

    /* Validate baudrate range (9600 - 921600) */
    if (config->baudrate < 9600 || config->baudrate > 921600) {
        return HAL_ERROR_INVALID_PARAM;
    }

    uart_state_t* state = &uart_state[instance];

    state->config = *config;
    state->actual_baudrate = calculate_actual_baudrate(config->baudrate);
    ring_buffer_init(&state->rx_buffer);
    ring_buffer_init(&state->tx_buffer);
    state->rx_callback = NULL;
    state->tx_callback = NULL;
    state->rx_context = NULL;
    state->tx_context = NULL;
    state->initialized = true;

    return HAL_OK;
}

hal_status_t hal_uart_deinit(hal_uart_instance_t instance) {
    if (instance >= MAX_UART_INSTANCES) {
        return HAL_ERROR_INVALID_PARAM;
    }

    uart_state_t* state = &uart_state[instance];
    state->initialized = false;
    ring_buffer_init(&state->rx_buffer);
    ring_buffer_init(&state->tx_buffer);
    state->rx_callback = NULL;
    state->tx_callback = NULL;

    return HAL_OK;
}

hal_status_t hal_uart_transmit(hal_uart_instance_t instance,
                               const uint8_t* data, size_t len,
                               uint32_t timeout_ms) {
    (void)timeout_ms; /* Timeout not used in native simulation */

    if (instance >= MAX_UART_INSTANCES) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (data == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }

    uart_state_t* state = &uart_state[instance];
    if (!state->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Put data into TX buffer */
    for (size_t i = 0; i < len; i++) {
        if (!ring_buffer_put(&state->tx_buffer, data[i])) {
            return HAL_ERROR_OVERRUN; /* Buffer full */
        }
    }

    /* Invoke TX complete callback if registered */
    if (state->tx_callback != NULL) {
        state->tx_callback(instance, state->tx_context);
    }

    return HAL_OK;
}

hal_status_t hal_uart_receive(hal_uart_instance_t instance, uint8_t* data,
                              size_t len, uint32_t timeout_ms) {
    (void)timeout_ms; /* Timeout not used in native simulation */

    if (instance >= MAX_UART_INSTANCES) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (data == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }

    uart_state_t* state = &uart_state[instance];
    if (!state->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Get data from RX buffer */
    for (size_t i = 0; i < len; i++) {
        if (!ring_buffer_get(&state->rx_buffer, &data[i])) {
            return HAL_ERROR_TIMEOUT; /* No data available */
        }
    }

    return HAL_OK;
}

hal_status_t hal_uart_putc(hal_uart_instance_t instance, uint8_t byte) {
    return hal_uart_transmit(instance, &byte, 1, HAL_WAIT_FOREVER);
}

hal_status_t hal_uart_getc(hal_uart_instance_t instance, uint8_t* byte,
                           uint32_t timeout_ms) {
    return hal_uart_receive(instance, byte, 1, timeout_ms);
}

hal_status_t hal_uart_set_rx_callback(hal_uart_instance_t instance,
                                      hal_uart_rx_callback_t callback,
                                      void* context) {
    if (instance >= MAX_UART_INSTANCES) {
        return HAL_ERROR_INVALID_PARAM;
    }

    uart_state_t* state = &uart_state[instance];
    if (!state->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    state->rx_callback = callback;
    state->rx_context = context;

    return HAL_OK;
}

hal_status_t hal_uart_set_tx_callback(hal_uart_instance_t instance,
                                      hal_uart_tx_callback_t callback,
                                      void* context) {
    if (instance >= MAX_UART_INSTANCES) {
        return HAL_ERROR_INVALID_PARAM;
    }

    uart_state_t* state = &uart_state[instance];
    if (!state->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    state->tx_callback = callback;
    state->tx_context = context;

    return HAL_OK;
}
