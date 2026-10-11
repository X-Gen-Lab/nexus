/**
 * \file            flash_storage.h
 * \brief           Explicit physical Flash-region adapter without global
 *                  registry
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_FLASH_STORAGE_H
#define NEXUS_COMPONENTS_FLASH_STORAGE_H
#include "nexus/components/storage.h"
#include "nexus/io/flash.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Caller-owned adapter, no dynamic binding or global token lookup. */
typedef struct {
    nx_flash_region_t region;
    nx_time_us_t deadline;
} nx_flash_storage_adapter_t;

/**
 * \brief           Bind an explicit writable uniform-erase physical region
 * \param[out]      adapter: Unused caller storage kept through all store calls
 * \param[in]       region: Externally selected writable physical region
 * \param[in]       deadline: Absolute whole-transaction deadline for mutations
 * \param[out]      port: Narrow synchronous persistent port, valid on SUCCESS
 * \return          SUCCESS or INVALID/PERMISSION/UNSUPPORTED geometry error
 * \note            Task-only, serialized caller. No region is reserved by the
 *                  platform. Mixed sector geometries require another explicit
 *                  storage adapter; this one rejects them. IO pulse settlement
 *                  guarantees sync durability; no caller buffer survives a port
 *                  call. Physical power-loss remains unqualified.
 */
nx_result_t nx_flash_storage_bind(nx_flash_storage_adapter_t* adapter,
                                  nx_flash_region_t region,
                                  nx_time_us_t deadline,
                                  nx_storage_port_t* port);
/**
 * \brief           Set the next transaction's one absolute mutation deadline
 * \param[in,out]   adapter: Bound adapter with no active store operation
 * \param[in]       deadline: Absolute deadline in nx_time_now_us domain
 * \note            Caller serializes and sets this before open/save/recovery.
 *                  Per-pulse timeouts cannot preempt hardware pulses; IO
 *                  settles before return.
 */
void nx_flash_storage_set_deadline(nx_flash_storage_adapter_t* adapter,
                                   nx_time_us_t deadline);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_COMPONENTS_FLASH_STORAGE_H */
