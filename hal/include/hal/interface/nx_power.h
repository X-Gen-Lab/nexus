/**
 * \file            nx_power.h
 * \brief           Power management interface
 * \author          Nexus Team
 *
 * This file defines the power management interface for controlling
 * device power states and clock gating.
 */

#ifndef NX_POWER_H
#define NX_POWER_H

#include "hal/nx_status.h"
#include "hal/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Power state change callback type
 * \param[in]       user_data: User data pointer
 * \param[in]       enabled: New power state (true = enabled)
 */
typedef void (*nx_power_callback_t)(void* user_data, bool enabled);

/**
 * \brief           Power management interface structure
 *
 * This interface provides methods for controlling device power state,
 * including enabling/disabling peripheral clocks and power domains.
 */
typedef struct nx_power_s nx_power_t;
struct nx_power_s {
    /**
     * \brief       Enable device power/clock
     * \param[in]   self: Pointer to power interface
     * \return      NX_OK on success, error code otherwise
     *
     * This function enables the peripheral clock and/or power domain
     * for the device. The device configuration is restored if it was
     * previously disabled.
     */
    nx_status_t (*enable)(nx_power_t* self);

    /**
     * \brief       Disable device power/clock
     * \param[in]   self: Pointer to power interface
     * \return      NX_OK on success, error code otherwise
     *
     * This function disables the peripheral clock and/or power domain
     * for the device to reduce power consumption. The device configuration
     * is preserved and will be restored when enable() is called.
     */
    nx_status_t (*disable)(nx_power_t* self);

    /**
     * \brief       Check if device power is enabled
     * \param[in]   self: Pointer to power interface
     * \return      true if enabled, false if disabled
     */
    bool (*is_enabled)(nx_power_t* self);

    /**
     * \brief       Set power state change callback
     * \param[in]   self: Pointer to power interface
     * \param[in]   callback: Callback function (NULL to disable)
     * \param[in]   user_data: User data passed to callback
     * \return      NX_OK on success, error code otherwise
     *
     * The callback is invoked when the power state changes.
     */
    nx_status_t (*set_callback)(nx_power_t* self, nx_power_callback_t callback,
                                void* user_data);
};

#ifdef __cplusplus
}
#endif

#endif /* NX_POWER_H */
