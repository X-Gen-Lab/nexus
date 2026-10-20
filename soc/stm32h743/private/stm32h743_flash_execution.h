/**
 * \file            stm32h743_flash_execution.h
 * \brief           Private physical image boundary for H743 Flash execution
 * \author          Nexus Team
 */
#ifndef NX_STM32H743_FLASH_EXECUTION_H
#define NX_STM32H743_FLASH_EXECUTION_H
#include "nexus/core/status.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief           Immutable physical bounds derived from the linked image.
 * \note            Half-open addresses, not host backing pointers or authored
 *                  product partitions. The live interval contains Flash code,
 *                  vectors and read-only dependencies; the load interval also
 *                  contains initialized RAM and executable RAM load bytes.
 *                  Both must belong to the exact unswapped 2MiB image. This
 *                  descriptor alone does not establish RAM execution closure.
 */
typedef struct {
    uint32_t live_start;
    uint32_t live_end;
    uint32_t load_start;
    uint32_t load_end;
} nx_stm32h743_flash_image_t;
/**
 * \brief           Reject a physical pulse range overlapping the live image.
 * \param[in]       image: Exact immutable linker-derived bounds, not a caller
 *                  assertion that an arbitrary RAM address is executable.
 * \param[in]       offset: Physical offset from 0x08000000, before bank swap.
 * \param[in]       length: Positive whole operation span checked for overflow.
 * \return          SUCCESS for a disjoint in-range span, STATE on image
 *                  overlap, or INVALID for malformed image/range. No MMIO.
 * \note            Includes load-only bytes needed after reset. This helper
 *                  does not start hardware, authorize self-update, select a
 *                  product partition or prove pulse/vector/stack RAM closure.
 */
nx_result_t
nx_stm32h743_flash_image_check(const nx_stm32h743_flash_image_t* image,
                               uint32_t offset, size_t length);
#ifdef __cplusplus
}
#endif
#endif
