/**
 * \file            chip.h
 * \brief           Read-only observed silicon identity without runtime storage
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_CHIP_H
#define NEXUS_IO_CHIP_H
#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Raw identity words, revision and actual factory Flash capacity. */
typedef struct {
    uint32_t uid[3];
    uint16_t device_id;
    uint16_t revision_id;
    uint16_t flash_size_kib;
} nx_chip_info_t;
/**
 * \brief Read the selected SoC's physical identity into independent storage.
 * \param[out] info: Aligned caller-owned output; changed only on success.
 * \return SUCCESS, INVALID, CONTEXT or UNSUPPORTED; no cached fallback values.
 * \note Bounded privileged task/ISR read, including masked context. No clocks,
 *       allocation, preparation or retained references. The selected Platform
 *       supplies this symbol; CPU-only Runtime does not supply a SoC backend.
 *       UID is public silicon identity, not authentication. The device/revision
 *       fields do not prove an exact package or ordering code. Explicit Native
 *       test support reports UNSUPPORTED without inventing physical identity.
 */
nx_result_t nx_chip_info_read(nx_chip_info_t* info);
#ifdef __cplusplus
}
#endif
#endif
