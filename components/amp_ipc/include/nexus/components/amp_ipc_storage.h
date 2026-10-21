/**
 * \file            amp_ipc_storage.h
 * \brief           Caller-owned endpoint configuration and retained loan state
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_AMP_IPC_STORAGE_H
#define NEXUS_AMP_IPC_STORAGE_H
#include "nexus/components/amp_ipc.h"
/** \brief Zero-initialized local state; no sharing with another endpoint. */
typedef struct {
    uint32_t sequence;
    uint32_t offset;
    uint32_t length;
    bool active;
    bool returned;
} nx_amp_local_slot_t;
/**
 * \brief           One unidirectional lane and exact caller-owned storage
 * \note            Both boot words are externally published, monotonic nonzero
 *                  epochs. Never republish an old boot identity, including
 *                  after a whole-device reset. The boot owner must fail closed
 *                  on exhaustion. Initial shared records/words are zeroed or
 *                  retained from older epochs, never edited while live.
 *                  Wire and boot words occupy separate complete 64-byte units.
 *                  All shared spans align to granule (power of two, 1..64).
 *                  Payload size is a positive multiple of granule.
 * Configuration, callbacks and all supplied storage outlive final join.
 */
typedef struct {
    const nx_amp_port_ops_t* ops;
    void* context;
    nx_amp_wire_slot_t* wire;
    nx_amp_local_slot_t* slots;
    uint32_t capacity;
    void* payload;
    uint32_t payload_bytes;
    nx_amp_word_t* local_boot;
    nx_amp_word_t* peer_boot;
    uint32_t local_epoch;
    uint32_t peer_epoch;
    uint32_t granule;
    nx_amp_role_t role;
} nx_amp_config_t;
struct nx_amp_ipc {
    nx_amp_config_t config;
    bool initialized;
    bool closed;
    bool faulted;
    bool joined;
};
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief           Claim and bind unused zero-initialized endpoint storage
 * \note            Serialized privileged owner Thread, before publication.
 *                  Validates disjoint spans and exact boot epochs before claim.
 *                  Rejected construction has no references or shared effects.
 *                  The port must independently enforce unique hardware/role
 *                  ownership; this component contains no global registry.
 */
nx_result_t nx_amp_ipc_construct(nx_amp_ipc_t* ipc,
                                 const nx_amp_config_t* config);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_AMP_IPC_STORAGE_H */
