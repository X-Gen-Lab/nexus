/**
 * \file            nx_configurable.h
 * \brief           Generic configuration interface
 * \author          Nexus Team
 *
 * This file defines a generic configuration interface that allows
 * runtime configuration of device parameters.
 */

#ifndef NX_CONFIGURABLE_H
#define NX_CONFIGURABLE_H

#include "hal/nx_status.h"
#include "hal/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Generic configuration interface structure
 *
 * This interface provides a generic way to get, set, and reset
 * device configuration. The actual configuration structure is
 * device-specific.
 */
typedef struct nx_configurable_s nx_configurable_t;
struct nx_configurable_s {
    /**
     * \brief       Get current configuration
     * \param[in]   self: Pointer to configurable interface
     * \param[out]  cfg: Buffer to store configuration
     * \param[in]   size: Size of configuration buffer
     * \return      NX_OK on success, error code otherwise
     *
     * The caller must provide a buffer of appropriate size for the
     * device-specific configuration structure.
     */
    nx_status_t (*get_config)(nx_configurable_t* self, void* cfg, size_t size);

    /**
     * \brief       Set new configuration
     * \param[in]   self: Pointer to configurable interface
     * \param[in]   cfg: New configuration to apply
     * \param[in]   size: Size of configuration structure
     * \return      NX_OK on success, error code otherwise
     *
     * This function applies a new configuration to the device.
     * The configuration may be applied immediately or may require
     * a device restart depending on the implementation.
     */
    nx_status_t (*set_config)(nx_configurable_t* self, const void* cfg,
                              size_t size);

    /**
     * \brief       Reset configuration to defaults
     * \param[in]   self: Pointer to configurable interface
     * \return      NX_OK on success, error code otherwise
     *
     * This function resets the device configuration to its default values.
     */
    nx_status_t (*reset_config)(nx_configurable_t* self);
};

#ifdef __cplusplus
}
#endif

#endif /* NX_CONFIGURABLE_H */
