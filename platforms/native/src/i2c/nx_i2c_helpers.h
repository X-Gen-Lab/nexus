/**
 * \file            nx_i2c_helpers.h
 * \brief           I2C helper functions for Native platform
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef NX_I2C_HELPERS_H
#define NX_I2C_HELPERS_H

#include "hal/nx_status.h"
#include "nx_i2c_types.h"
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
 * \param[in]       self: I2C interface pointer
 * \return          Implementation pointer or NULL
 * \note            Optimized as inline for performance
 */
static inline nx_i2c_impl_t* i2c_get_impl(nx_i2c_bus_t* self) {
    return self ? (nx_i2c_impl_t*)self : NULL;
}

/**
 * \brief           Initialize circular buffer
 * \param[in]       buf: Buffer structure pointer
 * \param[in]       data: Buffer data pointer
 * \param[in]       size: Buffer size
 * \note            Optimized as inline for performance
 */
static inline void i2c_buffer_init(nx_i2c_buffer_t* buf, uint8_t* data,
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
static inline size_t i2c_buffer_get_count(const nx_i2c_buffer_t* buf) {
    return buf->count;
}

/**
 * \brief           Clear circular buffer
 * \param[in]       buf: Buffer structure pointer
 * \note            Optimized as inline for performance
 */
static inline void i2c_buffer_clear(nx_i2c_buffer_t* buf) {
    buf->head = 0;
    buf->tail = 0;
    buf->count = 0;
}

/**
 * \brief           Write data to circular buffer
 * \param[in]       buf: Buffer structure pointer
 * \param[in]       data: Data to write
 * \param[in]       len: Data length
 * \return          Number of bytes written
 */
size_t i2c_buffer_write(nx_i2c_buffer_t* buf, const uint8_t* data, size_t len);

/**
 * \brief           Read data from circular buffer
 * \param[in]       buf: Buffer structure pointer
 * \param[out]      data: Data buffer
 * \param[in]       len: Maximum bytes to read
 * \return          Number of bytes read
 */
size_t i2c_buffer_read(nx_i2c_buffer_t* buf, uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* NX_I2C_HELPERS_H */
