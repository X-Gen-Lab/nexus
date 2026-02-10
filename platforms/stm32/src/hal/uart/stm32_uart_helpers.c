/**
 * \file            stm32_uart_helpers.c
 * \brief           STM32 UART helper functions implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-02-04
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements circular buffer operations, status code
 *                  mapping, peripheral type detection, and other utility
 *                  functions for UART driver.
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#include "stm32_uart_helpers.h"
#include "nexus_config.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Critical Section Macros                                                   */
/*---------------------------------------------------------------------------*/

#define UART_ENTER_CRITICAL() __disable_irq()
#define UART_EXIT_CRITICAL()  __enable_irq()

/*---------------------------------------------------------------------------*/
/* Circular Buffer Operations                                                */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize circular buffer
 */
void stm32_uart_buffer_init(stm32_uart_buffer_t* buf, uint8_t* data,
                            size_t size, uart_overflow_policy_t policy) {
    if (!buf)
        return;

    buf->data = data;
    buf->size = size;
    buf->head = 0;
    buf->tail = 0;
    buf->count = 0;
    buf->policy = policy;
    buf->high_watermark = 0;
    buf->low_watermark = 0;
    buf->peak_usage = 0;
    buf->overflow_count = 0;
    buf->high_water_flag = false;
    buf->low_water_flag = true;

#ifdef NX_CONFIG_STM32_UART_ENABLE_WATERMARK
    /* Set default watermarks */
    stm32_uart_buffer_set_watermarks(
        buf, NX_CONFIG_STM32_UART_HIGH_WATERMARK_PERCENT,
        NX_CONFIG_STM32_UART_LOW_WATERMARK_PERCENT);
#endif
}

/**
 * \brief           Set buffer watermarks
 */
void stm32_uart_buffer_set_watermarks(stm32_uart_buffer_t* buf,
                                      uint8_t high_percent,
                                      uint8_t low_percent) {
    if (!buf || buf->size == 0)
        return;

    buf->high_watermark = (buf->size * high_percent) / 100;
    buf->low_watermark = (buf->size * low_percent) / 100;
}

/**
 * \brief           Update watermark flags
 */
static inline void update_watermarks(stm32_uart_buffer_t* buf) {
#ifdef NX_CONFIG_STM32_UART_ENABLE_WATERMARK
    if (buf->count >= buf->high_watermark) {
        buf->high_water_flag = true;
    }
    if (buf->count <= buf->low_watermark) {
        buf->low_water_flag = true;
        buf->high_water_flag = false;
    }
#else
    (void)buf;
#endif
}

/**
 * \brief           Write data to circular buffer (optimized)
 */
size_t stm32_uart_buffer_write(stm32_uart_buffer_t* buf, const uint8_t* data,
                               size_t len) {
    if (!buf || !buf->data || !data || len == 0 || buf->size == 0) {
        return 0;
    }

    size_t free_space = buf->size - buf->count;
    size_t to_write = len;

    /* Handle overflow based on policy */
    if (to_write > free_space) {
        switch (buf->policy) {
            case UART_OVERFLOW_DROP_OLD:
                /* Overwrite old data - advance tail */
                buf->tail = (buf->tail + (to_write - free_space)) % buf->size;
                buf->count = buf->size - to_write;
                buf->overflow_count++;
                break;

            case UART_OVERFLOW_DROP_NEW:
                /* Reject new data */
                to_write = free_space;
                if (to_write < len) {
                    buf->overflow_count++;
                }
                break;

            case UART_OVERFLOW_ERROR:
                /* Return error */
                buf->overflow_count++;
                return 0;
        }
    }

    /* Optimized copy using memcpy */
    size_t chunk1 = buf->size - buf->head;
    if (to_write <= chunk1) {
        /* Single contiguous write */
        memcpy(&buf->data[buf->head], data, to_write);
    } else {
        /* Wrap-around write */
        memcpy(&buf->data[buf->head], data, chunk1);
        memcpy(buf->data, &data[chunk1], to_write - chunk1);
    }

    buf->head = (buf->head + to_write) % buf->size;
    buf->count += to_write;

    /* Update peak usage */
    if (buf->count > buf->peak_usage) {
        buf->peak_usage = buf->count;
    }

    /* Update watermark flags */
    update_watermarks(buf);

    return to_write;
}

/**
 * \brief           Write data with interrupt protection
 */
size_t stm32_uart_buffer_write_safe(stm32_uart_buffer_t* buf,
                                    const uint8_t* data, size_t len) {
    UART_ENTER_CRITICAL();
    size_t written = stm32_uart_buffer_write(buf, data, len);
    UART_EXIT_CRITICAL();
    return written;
}

/**
 * \brief           Read data from circular buffer (optimized)
 */
size_t stm32_uart_buffer_read(stm32_uart_buffer_t* buf, uint8_t* data,
                              size_t len) {
    if (!buf || !buf->data || !data || len == 0 || buf->size == 0) {
        return 0;
    }

    size_t available = buf->count;
    size_t to_read = (len < available) ? len : available;

    if (to_read == 0) {
        return 0;
    }

    /* Optimized copy using memcpy */
    size_t chunk1 = buf->size - buf->tail;
    if (to_read <= chunk1) {
        /* Single contiguous read */
        memcpy(data, &buf->data[buf->tail], to_read);
    } else {
        /* Wrap-around read */
        memcpy(data, &buf->data[buf->tail], chunk1);
        memcpy(&data[chunk1], buf->data, to_read - chunk1);
    }

    buf->tail = (buf->tail + to_read) % buf->size;
    buf->count -= to_read;

    /* Update watermark flags */
    update_watermarks(buf);

    return to_read;
}

/**
 * \brief           Read data with interrupt protection
 */
size_t stm32_uart_buffer_read_safe(stm32_uart_buffer_t* buf, uint8_t* data,
                                   size_t len) {
    UART_ENTER_CRITICAL();
    size_t read_count = stm32_uart_buffer_read(buf, data, len);
    UART_EXIT_CRITICAL();
    return read_count;
}

/**
 * \brief           Flush buffer
 */
void stm32_uart_buffer_flush(stm32_uart_buffer_t* buf) {
    if (!buf)
        return;

    UART_ENTER_CRITICAL();
    buf->head = 0;
    buf->tail = 0;
    buf->count = 0;
    buf->high_water_flag = false;
    buf->low_water_flag = true;
    UART_EXIT_CRITICAL();
}

/**
 * \brief           Get buffer statistics
 */
void stm32_uart_buffer_get_stats(const stm32_uart_buffer_t* buf,
                                 size_t* peak_usage, uint32_t* overflow_count) {
    if (!buf)
        return;

    if (peak_usage) {
        *peak_usage = buf->peak_usage;
    }
    if (overflow_count) {
        *overflow_count = buf->overflow_count;
    }
}

/*---------------------------------------------------------------------------*/
/* IRQ Number Mapping                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get peripheral type from base address
 */
uart_periph_type_t uart_get_periph_type(USART_TypeDef* instance) {
    if (!instance) {
        return UART_PERIPH_UART; /* Default to UART */
    }

    /* Check if it's a USART instance */
    if (IS_USART_INSTANCE(instance)) {
        return UART_PERIPH_USART;
    }

    /* Check if it's an LPUART instance */
    if (IS_LPUART_INSTANCE(instance)) {
        return UART_PERIPH_LPUART;
    }

    /* Default to UART */
    return UART_PERIPH_UART;
}

/**
 * \brief           Get UART IRQ number from instance index
 */
IRQn_Type stm32_uart_get_irq_number(uint8_t instance) {
#if defined(STM32F0xx)
    /* F0 series: USART1-8 (varies by model) */
    switch (instance) {
        case 1:
            return USART1_IRQn;
        case 2:
            return USART2_IRQn;
#if defined(USART3_8_IRQn)
        /* STM32F09x/F098: USART3-8 share one IRQ */
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
            return USART3_8_IRQn;
#elif defined(USART3_4_IRQn)
        /* STM32F07x/F072: USART3-4 share one IRQ */
        case 3:
        case 4:
            return USART3_4_IRQn;
#elif defined(USART3_IRQn)
        case 3:
            return USART3_IRQn;
#endif
        default:
            return (IRQn_Type)-1;
    }
#elif defined(STM32F1xx)
    /* F1 series: USART1-5 */
    switch (instance) {
        case 1:
            return USART1_IRQn;
        case 2:
            return USART2_IRQn;
#if defined(USART3)
        case 3:
            return USART3_IRQn;
#endif
#if defined(UART4)
        case 4:
            return UART4_IRQn;
#endif
#if defined(UART5)
        case 5:
            return UART5_IRQn;
#endif
        default:
            return (IRQn_Type)-1;
    }
#elif defined(STM32F2xx)
    /* F2 series: USART1-6 */
    switch (instance) {
        case 1:
            return USART1_IRQn;
        case 2:
            return USART2_IRQn;
        case 3:
            return USART3_IRQn;
        case 4:
            return UART4_IRQn;
        case 5:
            return UART5_IRQn;
        case 6:
            return USART6_IRQn;
        default:
            return (IRQn_Type)-1;
    }
#elif defined(STM32F3xx)
    /* F3 series: USART1-5 */
    switch (instance) {
        case 1:
            return USART1_IRQn;
        case 2:
            return USART2_IRQn;
#if defined(USART3)
        case 3:
            return USART3_IRQn;
#endif
#if defined(UART4)
        case 4:
            return UART4_IRQn;
#endif
#if defined(UART5)
        case 5:
            return UART5_IRQn;
#endif
        default:
            return (IRQn_Type)-1;
    }
#elif defined(STM32F407xx) || defined(STM32F429xx) || defined(STM32F446xx) ||  \
    defined(STM32F4xx)
    /* F4 series: USART1-6 */
    switch (instance) {
        case 1:
            return USART1_IRQn;
        case 2:
            return USART2_IRQn;
        case 3:
            return USART3_IRQn;
        case 4:
            return UART4_IRQn;
        case 5:
            return UART5_IRQn;
        case 6:
            return USART6_IRQn;
        default:
            return (IRQn_Type)-1;
    }
#elif defined(STM32F7xx)
    /* F7 series: USART1-6, UART7-8 */
    switch (instance) {
        case 1:
            return USART1_IRQn;
        case 2:
            return USART2_IRQn;
        case 3:
            return USART3_IRQn;
        case 4:
            return UART4_IRQn;
        case 5:
            return UART5_IRQn;
        case 6:
            return USART6_IRQn;
#if defined(UART7)
        case 7:
            return UART7_IRQn;
#endif
#if defined(UART8)
        case 8:
            return UART8_IRQn;
#endif
        default:
            return (IRQn_Type)-1;
    }
#elif defined(STM32G0xx)
    /* G0 series: USART1-6, LPUART1-2 (varies by model) */
    switch (instance) {
        case 1:
            return USART1_IRQn;
        case 2:
            return USART2_IRQn;
#if defined(USART3_4_5_6_LPUART1_IRQn)
        /* G0B/G0C: USART3-6 + LPUART1 share one IRQ */
        case 3:
        case 4:
        case 5:
        case 6:
            return USART3_4_5_6_LPUART1_IRQn;
#elif defined(USART3_4_5_6_IRQn)
        /* G0B0: USART3-6 share one IRQ */
        case 3:
        case 4:
        case 5:
        case 6:
            return USART3_4_5_6_IRQn;
#elif defined(USART3_4_LPUART1_IRQn)
        /* G071/G081: USART3-4 + LPUART1 share one IRQ */
        case 3:
        case 4:
            return USART3_4_LPUART1_IRQn;
#elif defined(USART3_4_IRQn)
        /* G070: USART3-4 share one IRQ */
        case 3:
        case 4:
            return USART3_4_IRQn;
#endif
        default:
            return (IRQn_Type)-1;
    }
#elif defined(STM32G4xx)
    /* G4 series: USART1-3, UART4-5, LPUART1 */
    switch (instance) {
        case 1:
            return USART1_IRQn;
        case 2:
            return USART2_IRQn;
        case 3:
            return USART3_IRQn;
#if defined(UART4)
        case 4:
            return UART4_IRQn;
#endif
#if defined(UART5)
        case 5:
            return UART5_IRQn;
#endif
        default:
            return (IRQn_Type)-1;
    }
#elif defined(STM32H5xx)
    /* H5 series: USART1-3, UART4-5, USART6, UART7-8, LPUART1 */
    switch (instance) {
        case 1:
            return USART1_IRQn;
        case 2:
            return USART2_IRQn;
        case 3:
            return USART3_IRQn;
#if defined(UART4)
        case 4:
            return UART4_IRQn;
#endif
#if defined(UART5)
        case 5:
            return UART5_IRQn;
#endif
#if defined(USART6)
        case 6:
            return USART6_IRQn;
#endif
#if defined(UART7)
        case 7:
            return UART7_IRQn;
#endif
#if defined(UART8)
        case 8:
            return UART8_IRQn;
#endif
        default:
            return (IRQn_Type)-1;
    }
#elif defined(STM32H743xx) || defined(STM32H750xx) || defined(STM32H7xx)
    /* H7 series: USART1-6, UART7-8 */
    switch (instance) {
        case 1:
            return USART1_IRQn;
        case 2:
            return USART2_IRQn;
        case 3:
            return USART3_IRQn;
        case 4:
            return UART4_IRQn;
        case 5:
            return UART5_IRQn;
        case 6:
            return USART6_IRQn;
        case 7:
            return UART7_IRQn;
        case 8:
            return UART8_IRQn;
        default:
            return (IRQn_Type)-1;
    }
#elif defined(STM32L476xx) || defined(STM32L432xx) || defined(STM32L4xx)
    /* L4 series: USART1-5, LPUART1 */
    switch (instance) {
        case 1:
            return USART1_IRQn;
        case 2:
            return USART2_IRQn;
#if defined(USART3)
        case 3:
            return USART3_IRQn;
#endif
#if defined(UART4)
        case 4:
            return UART4_IRQn;
#endif
#if defined(UART5)
        case 5:
            return UART5_IRQn;
#endif
        default:
            return (IRQn_Type)-1;
    }
#elif defined(STM32L0xx)
    /* L0 series: USART1-2, USART4-5, LPUART1 */
    switch (instance) {
        case 1:
            return USART1_IRQn;
        case 2:
            return USART2_IRQn;
#if defined(USART4_5_IRQn)
        /* Some L0: USART4-5 share one IRQ */
        case 4:
        case 5:
            return USART4_5_IRQn;
#elif defined(USART4)
        case 4:
            return USART4_IRQn;
#endif
#if defined(USART5) && !defined(USART4_5_IRQn)
        case 5:
            return USART5_IRQn;
#endif
        default:
            return (IRQn_Type)-1;
    }
#elif defined(STM32L1xx)
    /* L1 series: USART1-3 */
    switch (instance) {
        case 1:
            return USART1_IRQn;
        case 2:
            return USART2_IRQn;
#if defined(USART3)
        case 3:
            return USART3_IRQn;
#endif
        default:
            return (IRQn_Type)-1;
    }
#elif defined(STM32L5xx)
    /* L5 series: USART1-3, UART4-5, LPUART1 */
    switch (instance) {
        case 1:
            return USART1_IRQn;
        case 2:
            return USART2_IRQn;
        case 3:
            return USART3_IRQn;
#if defined(UART4)
        case 4:
            return UART4_IRQn;
#endif
#if defined(UART5)
        case 5:
            return UART5_IRQn;
#endif
        default:
            return (IRQn_Type)-1;
    }
#elif defined(STM32U0xx)
    /* U0 series: USART1-2 */
    switch (instance) {
        case 1:
            return USART1_IRQn;
        case 2:
            return USART2_IRQn;
        default:
            return (IRQn_Type)-1;
    }
#elif defined(STM32U3xx) || defined(STM32U5xx)
    /* U3/U5 series: USART1-3, UART4-5, LPUART1 */
    switch (instance) {
        case 1:
            return USART1_IRQn;
        case 2:
            return USART2_IRQn;
        case 3:
            return USART3_IRQn;
#if defined(UART4)
        case 4:
            return UART4_IRQn;
#endif
#if defined(UART5)
        case 5:
            return UART5_IRQn;
#endif
        default:
            return (IRQn_Type)-1;
    }
#elif defined(STM32WBxx)
    /* WB series: USART1, LPUART1 */
    switch (instance) {
        case 1:
            return USART1_IRQn;
        default:
            return (IRQn_Type)-1;
    }
#elif defined(STM32WLxx)
    /* WL series: USART1-2, LPUART1 */
    switch (instance) {
        case 1:
            return USART1_IRQn;
        case 2:
            return USART2_IRQn;
        default:
            return (IRQn_Type)-1;
    }
#else
    (void)instance;
    return (IRQn_Type)-1;
#endif
}

/*---------------------------------------------------------------------------*/
/* Status Code Conversion                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Convert HAL status to Nexus status
 * \details         Maps HAL_StatusTypeDef to nx_status_t codes.
 *                  Simple lookup for thin wrapper architecture.
 */
nx_status_t stm32_uart_hal_to_nx_status(HAL_StatusTypeDef hal_status) {
    switch (hal_status) {
        case HAL_OK:
            return NX_OK;
        case HAL_ERROR:
            return NX_ERR_HARDWARE;
        case HAL_BUSY:
            return NX_ERR_BUSY;
        case HAL_TIMEOUT:
            return NX_ERR_TIMEOUT;
        default:
            return NX_ERR_GENERIC;
    }
}

/*---------------------------------------------------------------------------*/
/* Test Support Functions                                                    */
/*---------------------------------------------------------------------------*/

#ifdef NX_CONFIG_STM32_UART_TEST_SUPPORT

/**
 * \brief           Inject data into RX buffer (for testing)
 */
nx_status_t stm32_uart_test_inject_rx(nx_uart_t* uart, const uint8_t* data,
                                      size_t len) {
    if (!uart || !data || len == 0) {
        return NX_ERR_INVALID_PARAM;
    }

    stm32_uart_impl_t* impl = NX_CONTAINER_OF(uart, stm32_uart_impl_t, base);
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Write to RX buffer */
    size_t written =
        stm32_uart_buffer_write_safe(&impl->state->rx_buf, data, len);
    if (written < len) {
        return NX_ERR_NO_MEMORY;
    }

    return NX_OK;
}

/**
 * \brief           Get buffer state (for testing)
 */
nx_status_t stm32_uart_test_get_buffer_state(nx_uart_t* uart, size_t* tx_count,
                                             size_t* rx_count) {
    if (!uart) {
        return NX_ERR_INVALID_PARAM;
    }

    stm32_uart_impl_t* impl = NX_CONTAINER_OF(uart, stm32_uart_impl_t, base);
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (tx_count) {
        *tx_count = stm32_uart_buffer_get_count(&impl->state->tx_buf);
    }
    if (rx_count) {
        *rx_count = stm32_uart_buffer_get_count(&impl->state->rx_buf);
    }

    return NX_OK;
}

/**
 * \brief           Simulate error condition (for testing)
 */
nx_status_t stm32_uart_test_simulate_error(nx_uart_t* uart,
                                           uint32_t error_code) {
    if (!uart) {
        return NX_ERR_INVALID_PARAM;
    }

    stm32_uart_impl_t* impl = NX_CONTAINER_OF(uart, stm32_uart_impl_t, base);
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Simulate error by setting HAL error code */
    impl->huart.ErrorCode = error_code;

    /* Trigger error callback if registered */
    if (impl->callbacks.error_cb) {
        impl->callbacks.error_cb(impl->callbacks.user_data, error_code);
    }

    return NX_OK;
}

#endif /* NX_CONFIG_STM32_UART_TEST_SUPPORT */
