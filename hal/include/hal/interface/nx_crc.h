/**
 * \file            nx_crc.h
 * \brief           CRC hardware acceleration interface
 * \author          Nexus Team
 */

#ifndef NX_CRC_H
#define NX_CRC_H

#include "hal/interface/nx_lifecycle.h"
#include "hal/nx_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           CRC interface
 */
typedef struct nx_crc_s nx_crc_t;
struct nx_crc_s {
    /**
     * \brief           Reset CRC calculation
     * \param[in]       self: Interface pointer
     */
    void (*reset)(nx_crc_t* self);

    /**
     * \brief           Update CRC with data
     * \param[in]       self: Interface pointer
     * \param[in]       data: Data buffer
     * \param[in]       len: Data length
     */
    void (*update)(nx_crc_t* self, const uint8_t* data, size_t len);

    /**
     * \brief           Get CRC result
     * \param[in]       self: Interface pointer
     * \return          CRC value
     */
    uint32_t (*get_result)(nx_crc_t* self);

    /**
     * \brief           Calculate CRC in one shot
     * \param[in]       self: Interface pointer
     * \param[in]       data: Data buffer
     * \param[in]       len: Data length
     * \return          CRC value
     */
    uint32_t (*calculate)(nx_crc_t* self, const uint8_t* data, size_t len);

    /**
     * \brief           Set CRC polynomial
     * \param[in]       self: Interface pointer
     * \param[in]       polynomial: CRC polynomial
     * \return          NX_OK on success, error code otherwise
     */
    nx_status_t (*set_polynomial)(nx_crc_t* self, uint32_t polynomial);

    /**
     * \brief           Get lifecycle interface
     * \param[in]       self: Interface pointer
     * \return          Lifecycle interface pointer
     */
    nx_lifecycle_t* (*get_lifecycle)(nx_crc_t* self);
};

#ifdef __cplusplus
}
#endif

#endif /* NX_CRC_H */
