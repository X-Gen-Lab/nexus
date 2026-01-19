/**
 * \file            nx_option_bytes.h
 * \brief           Option Bytes interface
 * \author          Nexus Team
 */

#ifndef NX_OPTION_BYTES_H
#define NX_OPTION_BYTES_H

#include "hal/nx_status.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Option Bytes interface
 */
typedef struct nx_option_bytes_s nx_option_bytes_t;
struct nx_option_bytes_s {
    /**
     * \brief           Get user data bytes
     * \param[in]       self: Interface pointer
     * \param[out]      data: Buffer to store user data
     * \param[in]       len: Length of data to read
     * \return          NX_OK on success, error code otherwise
     */
    nx_status_t (*get_user_data)(nx_option_bytes_t* self, uint8_t* data,
                                 size_t len);

    /**
     * \brief           Set user data bytes
     * \param[in]       self: Interface pointer
     * \param[in]       data: User data to write
     * \param[in]       len: Length of data to write
     * \return          NX_OK on success, error code otherwise
     */
    nx_status_t (*set_user_data)(nx_option_bytes_t* self, const uint8_t* data,
                                 size_t len);

    /**
     * \brief           Get read protection level
     * \param[in]       self: Interface pointer
     * \return          Read protection level (0=none, 1=level1, 2=level2)
     */
    uint8_t (*get_read_protection)(nx_option_bytes_t* self);

    /**
     * \brief           Set read protection level
     * \param[in]       self: Interface pointer
     * \param[in]       level: Read protection level to set
     * \return          NX_OK on success, error code otherwise
     */
    nx_status_t (*set_read_protection)(nx_option_bytes_t* self, uint8_t level);

    /**
     * \brief           Apply pending changes
     * \param[in]       self: Interface pointer
     * \return          NX_OK on success, error code otherwise
     * \note            This commits all pending option bytes changes
     */
    nx_status_t (*apply)(nx_option_bytes_t* self);
};

#ifdef __cplusplus
}
#endif

#endif /* NX_OPTION_BYTES_H */
