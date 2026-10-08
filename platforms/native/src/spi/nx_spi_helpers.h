/**
 * \file            nx_spi_helpers.h
 * \brief           SPI helper functions for Native platform
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef NX_SPI_HELPERS_H
#define NX_SPI_HELPERS_H

#include "hal/nx_status.h"
#include "nx_spi_types.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A short process-wide metadata guard. Never held across OSAL waits/callbacks. */
void native_spi_lock(void);
void native_spi_unlock(void);
uint32_t native_spi_now(void);
nx_status_t native_spi_reset_impl(nx_spi_impl_t* bus);
void spi_init_lifecycle(nx_lifecycle_t* lifecycle);
void spi_init_power(nx_power_t* power);

/*---------------------------------------------------------------------------*/
/* Helper Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get implementation from base interface
 * \param[in]       self: SPI bus interface pointer
 * \return          Implementation pointer or NULL
 * \note            Optimized as inline for performance
 */
static inline nx_spi_impl_t* spi_get_impl(nx_spi_bus_t* self) {
    if (!self) {
        return NULL;
    }
    return NX_CONTAINER_OF(self, nx_spi_impl_t, base);
}

/**
 * \brief           Initialize circular buffer
 * \param[in]       buf: Buffer structure pointer
 * \param[in]       data: Buffer data pointer
 * \param[in]       size: Buffer size
 * \note            Optimized as inline for performance
 */
static inline void spi_buffer_init(nx_spi_buffer_t* buf, uint8_t* data,
                                   size_t size) {
    if (!buf || !data) {
        return;
    }
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
static inline size_t spi_buffer_get_count(const nx_spi_buffer_t* buf) {
    if (!buf) {
        return 0;
    }
    return buf->count;
}

/**
 * \brief           Clear circular buffer
 * \param[in]       buf: Buffer structure pointer
 * \note            Optimized as inline for performance
 */
static inline void spi_buffer_clear(nx_spi_buffer_t* buf) {
    if (!buf) {
        return;
    }
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
size_t spi_buffer_write(nx_spi_buffer_t* buf, const uint8_t* data, size_t len);

/**
 * \brief           Read data from circular buffer
 * \param[in]       buf: Buffer structure pointer
 * \param[out]      data: Data buffer
 * \param[in]       len: Maximum bytes to read
 * \return          Number of bytes read
 */
size_t spi_buffer_read(nx_spi_buffer_t* buf, uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* NX_SPI_HELPERS_H */
