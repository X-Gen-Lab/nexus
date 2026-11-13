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
/** \brief Native diagnostic profile: identity words, revision and Flash size.
 * \note The maintained STM32 profile uses three UID words. Portable consumers
 *       use nx_chip_uid_read rather than depending on this diagnostic layout.
 */
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
/** \brief Copy opaque silicon identity bytes into caller-provided storage.
 * \param[out] output: Writable bytes, or NULL with zero capacity to query size.
 * \param[in] capacity: Complete caller-owned extent, not a platform maximum.
 * \param[out] bytes: UID length on success; disjoint aligned writable storage.
 * \return SUCCESS, INVALID, OVERFLOW when capacity is insufficient, CONTEXT or
 *         UNSUPPORTED. Both outputs remain unchanged on every failure.
 * \note Bounded privileged task/ISR read, no allocation, retained references or
 *       initialization. Byte order is stable within the provider; consumers
 *       treat it as opaque. The maintained STM32 provider emits each successive
 *       UID word least-significant byte first. Identity is public and does not
 *       authenticate the chip or prove a package/ordering code. Native models
 *       return UNSUPPORTED, including size queries, rather than inventing UID.
 */
nx_result_t nx_chip_uid_read(void* output, size_t capacity, size_t* bytes);
#ifdef __cplusplus
}
#endif
#endif
