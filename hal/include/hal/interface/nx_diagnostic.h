/**
 * \file            nx_diagnostic.h
 * \brief           Diagnostic interface
 * \author          Nexus Team
 *
 * This file defines the diagnostic interface for querying device
 * status and statistics.
 */

#ifndef NX_DIAGNOSTIC_H
#define NX_DIAGNOSTIC_H

#include "hal/nx_status.h"
#include "hal/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Diagnostic interface structure
 *
 * This interface provides methods for querying device status,
 * retrieving performance statistics, and clearing accumulated data.
 */
typedef struct nx_diagnostic_s nx_diagnostic_t;
struct nx_diagnostic_s {
    /**
     * \brief       Get current device status
     * \param[in]   self: Pointer to diagnostic interface
     * \param[out]  status: Buffer to store status information
     * \param[in]   size: Size of status buffer
     * \return      NX_OK on success, error code otherwise
     *
     * The status structure is device-specific and contains information
     * about the current operational state of the device.
     */
    nx_status_t (*get_status)(nx_diagnostic_t* self, void* status, size_t size);

    /**
     * \brief       Get device statistics
     * \param[in]   self: Pointer to diagnostic interface
     * \param[out]  stats: Buffer to store statistics
     * \param[in]   size: Size of statistics buffer
     * \return      NX_OK on success, error code otherwise
     *
     * The statistics structure is device-specific and contains
     * accumulated performance data such as transfer counts,
     * error counts, etc.
     */
    nx_status_t (*get_statistics)(nx_diagnostic_t* self, void* stats,
                                  size_t size);

    /**
     * \brief       Clear accumulated statistics
     * \param[in]   self: Pointer to diagnostic interface
     * \return      NX_OK on success, error code otherwise
     *
     * This function resets all accumulated statistics to zero.
     */
    nx_status_t (*clear_statistics)(nx_diagnostic_t* self);
};

#ifdef __cplusplus
}
#endif

#endif /* NX_DIAGNOSTIC_H */
