/**
 * \file            nx_flash_types.h
 * \brief           Native Flash internal types
 * \author          Nexus Team
 */

#ifndef NX_FLASH_TYPES_H
#define NX_FLASH_TYPES_H

#include "hal/base/nx_device.h"
#include "hal/interface/nx_flash.h"
#include "hal/interface/nx_lifecycle.h"
#include "hal/nx_status.h"
#include "hal/nx_types.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Flash Configuration                                                       */
/*---------------------------------------------------------------------------*/

#define NX_FLASH_SECTOR_SIZE 4096
#define NX_FLASH_NUM_SECTORS 128
#define NX_FLASH_TOTAL_SIZE  (NX_FLASH_SECTOR_SIZE * NX_FLASH_NUM_SECTORS)
#define NX_FLASH_WRITE_UNIT  4
#define NX_FLASH_ERASED_BYTE 0xFF

/*---------------------------------------------------------------------------*/
/* Flash Sector                                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Flash sector structure
 */
typedef struct {
    uint8_t data[NX_FLASH_SECTOR_SIZE]; /**< Sector data */
    bool erased;                        /**< Erase status */
} nx_flash_sector_t;

/*---------------------------------------------------------------------------*/
/* Flash State                                                               */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Flash state structure
 */
typedef struct {
    uint8_t index;    /**< Flash instance index */
    bool initialized; /**< Initialization status */
    bool suspended;   /**< Suspend status */
    bool locked;      /**< Lock status */
    nx_flash_sector_t sectors[NX_FLASH_NUM_SECTORS]; /**< Flash sectors */
    char backing_file[256];                          /**< Backing file path */
} nx_flash_state_t;

/*---------------------------------------------------------------------------*/
/* Flash Implementation                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Flash implementation structure
 */
typedef struct {
    nx_internal_flash_t base; /**< Base flash interface */
    nx_lifecycle_t lifecycle; /**< Lifecycle interface */
    nx_flash_state_t* state;  /**< Flash state */
    nx_device_t* device;      /**< Device handle */
} nx_flash_impl_t;

#ifdef __cplusplus
}
#endif

#endif /* NX_FLASH_TYPES_H */
