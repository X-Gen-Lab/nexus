/**
 * \file            nx_uart_helpers.h
 * \brief           UART helper functions for Native platform
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef NX_UART_HELPERS_H
#define NX_UART_HELPERS_H

#include "hal/nx_status.h"
#include "nx_uart_types.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Helper Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get implementation from base interface
 * \param[in]       self: UART interface pointer
 * \return          Implementation pointer or NULL
 * \note            Optimized as inline for performance
 */
static inline nx_uart_impl_t* uart_get_impl(nx_uart_t* self) {
    return self ? (nx_uart_impl_t*)self : NULL;
}

/**
 * \brief           Initialize circular buffer
 * \param[in]       buf: Buffer structure pointer
 * \param[in]       data: Buffer data pointer
 * \param[in]       size: Buffer size
 * \note            Optimized as inline for performance
 */
static inline void buffer_init(nx_uart_buffer_t* buf, uint8_t* data,
                               size_t size) {
    buf->data = data;
    buf->size = size;
    buf->head = 0;
    buf->tail = 0;
    buf->count = 0;
}

/**
 * \brief           Get number of bytes in buffer
 * \param[in]       buf: Buffer structure pointer
 * \return          Number of bytes available
 * \note            Optimized as inline for performance
 */
static inline size_t buffer_get_count(const nx_uart_buffer_t* buf) {
    return buf->count;
}

/**
 * \brief           Write data to circular buffer
 * \param[in]       buf: Buffer structure pointer
 * \param[in]       data: Data to write
 * \param[in]       len: Data length
 * \return          Number of bytes written
 */
size_t buffer_write(nx_uart_buffer_t* buf, const uint8_t* data, size_t len);

/**
 * \brief           Read data from circular buffer
 * \param[in]       buf: Buffer structure pointer
 * \param[out]      data: Data buffer
 * \param[in]       len: Maximum bytes to read
 * \return          Number of bytes read
 */
size_t buffer_read(nx_uart_buffer_t* buf, uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* NX_UART_HELPERS_H */
