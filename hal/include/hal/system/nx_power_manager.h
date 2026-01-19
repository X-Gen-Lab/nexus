/**
 * \file            nx_power_manager.h
 * \brief           System power manager interface
 * \author          Nexus Team
 */

#ifndef NX_POWER_MANAGER_H
#define NX_POWER_MANAGER_H

#include "hal/nx_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           System power modes
 */
typedef enum nx_power_mode_e {
    NX_POWER_RUN = 0, /**< Normal run mode */
    NX_POWER_SLEEP,   /**< Sleep mode (CPU stopped, peripherals running) */
    NX_POWER_STOP,    /**< Stop mode (most clocks stopped) */
} nx_power_mode_t;

/**
 * \brief           Power manager interface (simplified)
 */
typedef struct nx_power_manager_s nx_power_manager_t;
struct nx_power_manager_s {
    /**
     * \brief           Enter specified power mode
     * \param[in]       self: Power manager instance
     * \param[in]       mode: Target power mode
     * \return          NX_OK on success, error code otherwise
     */
    nx_status_t (*enter_mode)(nx_power_manager_t* self, nx_power_mode_t mode);

    /**
     * \brief           Get current power mode
     * \param[in]       self: Power manager instance
     * \return          Current power mode
     */
    nx_power_mode_t (*get_mode)(nx_power_manager_t* self);
};

/**
 * \brief           Get power manager instance
 * \return          Power manager instance pointer
 */
nx_power_manager_t* nx_get_power_manager(void);

#ifdef __cplusplus
}
#endif

#endif /* NX_POWER_MANAGER_H */
