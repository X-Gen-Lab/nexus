/**
 * \file            nx_option_bytes_types.h
 * \brief           Native Option Bytes internal types
 * \author          Nexus Team
 */

#ifndef NX_OPTION_BYTES_TYPES_H
#define NX_OPTION_BYTES_TYPES_H

#include "hal/base/nx_device.h"
#include "hal/interface/nx_lifecycle.h"
#include "hal/interface/nx_option_bytes.h"
#include "hal/interface/nx_power.h"
#include "hal/nx_status.h"
#include "hal/nx_types.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Option Bytes Configuration                                                */
/*---------------------------------------------------------------------------*/

#define NX_OPTION_BYTES_USER_DATA_SIZE 16

/*---------------------------------------------------------------------------*/
/* Option Bytes Data                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Option bytes data structure
 */
typedef struct {
    uint8_t read_protection; /**< Read protection level */
    uint8_t user_data[NX_OPTION_BYTES_USER_DATA_SIZE]; /**< User data bytes */
    bool write_protected; /**< Write protection status */
    bool pending_changes; /**< Pending changes flag */
} nx_option_bytes_data_t;

/*---------------------------------------------------------------------------*/
/* Option Bytes State                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Option bytes state structure
 */
typedef struct {
    uint8_t index;                  /**< Option bytes instance index */
    bool initialized;               /**< Initialization status */
    bool suspended;                 /**< Suspend status */
    nx_option_bytes_data_t data;    /**< Option bytes data */
    nx_option_bytes_data_t pending; /**< Pending changes */
} nx_option_bytes_state_t;

/*---------------------------------------------------------------------------*/
/* Option Bytes Implementation                                               */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Option bytes implementation structure
 */
typedef struct {
    nx_option_bytes_t base;         /**< Base option bytes interface */
    nx_lifecycle_t lifecycle;       /**< Lifecycle interface */
    nx_power_t power;               /**< Power interface */
    nx_option_bytes_state_t* state; /**< Option bytes state */
    nx_device_t* device;            /**< Device handle */
} nx_option_bytes_impl_t;

#ifdef __cplusplus
}
#endif

#endif /* NX_OPTION_BYTES_TYPES_H */
