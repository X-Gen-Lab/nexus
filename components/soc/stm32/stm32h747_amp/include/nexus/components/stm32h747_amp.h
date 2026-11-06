/**
 * \file            stm32h747_amp.h
 * \brief           Exact noncacheable H747 paired-boot AMP attachment
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_STM32H747_AMP_H
#define NEXUS_COMPONENTS_STM32H747_AMP_H
#include "nexus/components/amp_ipc_storage.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief External persistent boot authority, bound to this exact lane/role.
 * \note Reject reused/decreasing epochs, including after whole-device reset.
 *       SUCCESS commits the claim identity; rejection has no effects. The
 *       authority/configuration storage outlives final endpoint join.
 */
typedef nx_result_t (*nx_h747_amp_epoch_admit_t)(void* context, uint32_t epoch);
/** \brief Explicit reserved shared window; no physical boot policy is inferred.
 * \note The caller supplies an established paired boot, HSEM clock and one
 *       noncacheable shared MPU region. Both images reserve this same physical
 *       window outside startup clearing. Only one serialized privileged Thread
 *       per core calls attachment/AMP methods. Semaphore IDs are pair-owned;
 *       each role uses its unique claim ID and a separate outgoing doorbell.
 */
typedef struct {
    void* shared;
    size_t shared_bytes;
    const nx_amp_wire_slot_t* wire;
    size_t slots;
    nx_h747_amp_epoch_admit_t admit_epoch;
    void* authority;
    uint32_t boot_epoch;
    nx_amp_role_t role;
    uint8_t claim_semaphore;
    uint8_t notify_semaphore;
    uint8_t process_id;
    uint8_t mpu_region;
} nx_h747_amp_config_t;
/** \brief Caller-owned port; zero initialize and never mutate live fields.
 * \note Config, port and all copied AMP callback contexts survive final join.
 *       A successful endpoint join releases its HSEM claim, never boot history,
 *       peer clocks, shared SRAM, or the externally owned boot runtime.
 */
typedef struct {
    const nx_h747_amp_config_t* config;
    const void* owner;
    bool initialized;
    bool notify_pending;
    bool constructing;
} nx_h747_amp_t;
/** \brief Attach to a verified external paired boot without global mutations.
 * \param[out] port: Fresh zeroed unique storage, separate from shared memory.
 * \param[in] config: Immutable exact reservation/role and persistent authority.
 * \return SUCCESS or INVALID/CONTEXT/STATE/BUSY; rejection retains nothing.
 * \note Does not launch/reset a core or configure system clocks/MPU. Unknown
 *       boot/cache/memory ownership must be rejected by the external
 * integrator.
 */
nx_result_t nx_h747_amp_attach(nx_h747_amp_t* port,
                               const nx_h747_amp_config_t* config);
/** \brief Relinquish local port only after endpoint join released its claim.
 * \param[in,out] port: Same stable unique Thread-owned port.
 * \return SUCCESS or INVALID/CONTEXT/BUSY; BUSY retains all storage.
 */
nx_result_t nx_h747_amp_detach(nx_h747_amp_t* port);
/** \brief Validate the complete shared layout before any control-word read.
 * \param[out] endpoint: Fresh unique local endpoint storage.
 * \param[in,out] port: Same attached single Thread owner.
 * \param[in] config: Unbound common layout with NULL ops/context, granule32,
 *                  exact reserved wire/count/role/local epoch, shared boot
 * words and payload; local slots remain outside shared memory. \return SUCCESS
 * or INVALID/STATE/BUSY/CONTEXT; rejection retains nothing. \note Private
 * methods/context are bound only after whole-layout admission. Endpoint, port,
 * local slots, boot words and payload survive final join.
 */
nx_result_t nx_h747_amp_construct(nx_amp_ipc_t* endpoint, nx_h747_amp_t* port,
                                  const nx_amp_config_t* config);
/** \brief Try one HSEM doorbell pulse; delivery is a hint, never an ACK.
 * \param[in] port: Attached port with an active endpoint owner.
 * \return SUCCESS or CONTEXT/STATE/BUSY/IO; never revokes a payload borrow.
 * \note The peer owns its notification enable/IRQ route. Polling the lane
 *       remains authoritative when a pulse coalesces or the peer is absent.
 */
nx_result_t nx_h747_amp_notify(nx_h747_amp_t* port);
#ifdef __cplusplus
}
#endif
#endif
