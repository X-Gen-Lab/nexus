/**
 * \file            mcuboot_swap_port.h
 * \brief           Explicit scratch area for the independent MCUboot profile
 * \author          Nexus Team
 */
#ifndef NEXUS_MCUBOOT_SWAP_PORT_H
#define NEXUS_MCUBOOT_SWAP_PORT_H
#include "flash_map_backend/flash_map_backend.h"
#ifdef __cplusplus
extern "C" {
#endif
#define NX_MCUBOOT_SCRATCH_AREA_ID 3U
/**
 * \brief Immutable three-area call; old two-area operation ABI is unchanged.
 * \note All areas share one non-ECC four-byte-program provider and retain
 *       disjoint erase-aligned regions. Primary/secondary have matching sector
 *       geometry and size, at most three sectors. Scratch is one whole sector,
 *       at least as large as any image sector. Clock, deadline and all storage
 *       obey the same joined lifetime as the ordinary operation. Scratch is
 *       destructive maintenance storage, never an application data partition.
 */
typedef struct {
    const struct flash_area* primary;
    const struct flash_area* secondary;
    const struct flash_area* scratch;
    nx_clock_t clock;
    nx_time_us_t deadline;
} nx_mcuboot_swap_flash_operation_t;
/**
 * \brief Product hook for the whole authorized boot/control operation.
 * \return Stable borrowed three-area scope, or NULL outside that scope.
 * \note One serialized boot/task owner; no library active-operation global,
 *       default partition, hidden worker or allocation. This profile must not
 *       share a final executable with the overwrite-only backend.
 */
const nx_mcuboot_swap_flash_operation_t* nx_mcuboot_swap_flash_operation(void);
#ifdef __cplusplus
}
#endif
#endif
