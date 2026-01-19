/**
 * \file            nx_sdio.h
 * \brief           SDIO interface
 * \author          Nexus Team
 */

#ifndef NX_SDIO_H
#define NX_SDIO_H

#include "hal/interface/nx_lifecycle.h"
#include "hal/interface/nx_power.h"
#include "hal/nx_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           SDIO interface
 */
typedef struct nx_sdio_s nx_sdio_t;
struct nx_sdio_s {
    /**
     * \brief           Read blocks from SD card
     * \param[in]       self: Interface pointer
     * \param[in]       block: Starting block number
     * \param[out]      data: Buffer to store read data
     * \param[in]       block_count: Number of blocks to read
     * \return          NX_OK on success, error code otherwise
     */
    nx_status_t (*read)(nx_sdio_t* self, uint32_t block, uint8_t* data,
                        size_t block_count);

    /**
     * \brief           Write blocks to SD card
     * \param[in]       self: Interface pointer
     * \param[in]       block: Starting block number
     * \param[in]       data: Data buffer to write
     * \param[in]       block_count: Number of blocks to write
     * \return          NX_OK on success, error code otherwise
     */
    nx_status_t (*write)(nx_sdio_t* self, uint32_t block, const uint8_t* data,
                         size_t block_count);

    /**
     * \brief           Erase blocks on SD card
     * \param[in]       self: Interface pointer
     * \param[in]       start_block: Starting block number
     * \param[in]       block_count: Number of blocks to erase
     * \return          NX_OK on success, error code otherwise
     */
    nx_status_t (*erase)(nx_sdio_t* self, uint32_t start_block,
                         size_t block_count);

    /**
     * \brief           Get block size
     * \param[in]       self: Interface pointer
     * \return          Block size in bytes
     */
    size_t (*get_block_size)(nx_sdio_t* self);

    /**
     * \brief           Get total capacity
     * \param[in]       self: Interface pointer
     * \return          Total capacity in bytes
     */
    uint64_t (*get_capacity)(nx_sdio_t* self);

    /**
     * \brief           Check if SD card is present
     * \param[in]       self: Interface pointer
     * \return          true if card is present, false otherwise
     */
    bool (*is_present)(nx_sdio_t* self);

    /**
     * \brief           Get lifecycle interface
     * \param[in]       self: Interface pointer
     * \return          Lifecycle interface pointer
     */
    nx_lifecycle_t* (*get_lifecycle)(nx_sdio_t* self);

    /**
     * \brief           Get power management interface
     * \param[in]       self: Interface pointer
     * \return          Power management interface pointer
     */
    nx_power_t* (*get_power)(nx_sdio_t* self);
};

#ifdef __cplusplus
}
#endif

#endif /* NX_SDIO_H */
