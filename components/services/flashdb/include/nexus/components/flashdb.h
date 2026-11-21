/**
 * \file            flashdb.h
 * \brief           Caller-owned KVDB and TSDB over explicit Flash regions
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_FLASHDB_H
#define NEXUS_COMPONENTS_FLASHDB_H

#include "nexus/components/fal.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Explicit algorithm selection; Settings snapshot semantics differ. */
typedef enum { NX_FLASHDB_KV, NX_FLASHDB_TS } nx_flashdb_kind_t;
/** \brief Unknown or damaged existing media is never implicitly reformatted. */
typedef enum {
    NX_FLASHDB_EXISTING_ONLY,
    NX_FLASHDB_CREATE_IF_ERASED
} nx_flashdb_open_policy_t;
/** \brief TS status advances only; local commit never implies remote ACK. */
typedef enum {
    NX_FLASHDB_RECORD_WRITTEN = 2,
    NX_FLASHDB_RECORD_USER1,
    NX_FLASHDB_RECORD_DELETED,
    NX_FLASHDB_RECORD_USER2
} nx_flashdb_record_status_t;

/** \brief Caller metadata; workspace and Flash binding outlive every call. */
typedef struct {
    nx_fal_t* flash;
    void* state;
    size_t state_bytes;
    size_t max_record_bytes;
    nx_flashdb_kind_t kind;
    bool opened;
    bool entered;
} nx_flashdb_t;

/** \brief Product-selected immutable scope for one complete upstream call.
 * \note Getter and scope belong to the product's same-owner serialization.
 *       Keep the exact pointer active through callbacks and all error returns.
 */
typedef struct {
    nx_flashdb_t* database;
    nx_time_us_t deadline;
} nx_flashdb_operation_t;

/**
 * \brief Supply the product's explicitly active database operation.
 * \return Exact operation passed to the current API, or NULL outside a call.
 * \note Implemented by the consumer, without a library global device registry.
 *       No ISR use. A getter must not acquire hardware or recurse into this
 * API.
 */
const nx_flashdb_operation_t* nx_flashdb_operation_get(void);

/**
 * \brief Query exact private workspace size for this fixed build profile.
 * \return Nonzero bytes; alignment is reported separately.
 * \note No allocation or initialization. The profile uses 32-bit program
 *       granularity and disabled dynamic read/KV caches, not every ECC device.
 */
size_t nx_flashdb_workspace_bytes(void);
/** \brief Return required workspace alignment without accessing hardware. */
size_t nx_flashdb_workspace_alignment(void);

/**
 * \brief Initialize metadata without reading, writing or formatting media.
 * \param[out] database: Unused caller state, disjoint from workspace.
 * \param[in,out] flash: Bound same-owner non-ECC region, retained until close.
 * \param[in] kind: KVDB or TSDB algorithm selection.
 * \param[in,out] workspace: Aligned caller bytes, retained until close.
 * \param[in] workspace_bytes: At least the exact reported requirement.
 * \param[in] max_record_bytes: Positive TS record bound; ignored for KVDB.
 * \return SUCCESS, INVALID or UNSUPPORTED; rejection changes no state.
 */
nx_result_t nx_flashdb_init(nx_flashdb_t* database, nx_fal_t* flash,
                            nx_flashdb_kind_t kind, void* workspace,
                            size_t workspace_bytes, size_t max_record_bytes);

/**
 * \brief Open existing data or explicitly initialize a wholly erased region.
 * \param[in] operation: Exact active product scope, finite whole-call deadline.
 * \param[in] policy: Existing-only or permission to create on erased media.
 * \return SUCCESS, EMPTY, IO or underlying bridge error; no implicit repair.
 * \note Existing corruption is never treated as empty. Firmware/library format
 *       migrations, authentication, defaults and power-loss policy are
 * external. First opens across instances are serialized before publishing
 * users; upstream initialization includes a shared logging flag.
 */
nx_result_t nx_flashdb_open(const nx_flashdb_operation_t* operation,
                            nx_flashdb_open_policy_t policy);

/**
 * \brief Explicitly erase an entire closed database region.
 * \param[in] operation: Active scope for a closed initialized database.
 * \return SUCCESS or a retained mutation error; no automatic reopen.
 * \note Destructive product decision. No ordinary open or failed read calls it.
 */
nx_result_t nx_flashdb_format(const nx_flashdb_operation_t* operation);

/**
 * \brief Release only quiesced logical database state, without committing.
 * \param[in,out] database: Serialized initialized state, or NULL.
 * \return SUCCESS, BUSY or INVALID; physical failed ownership stays retained.
 * \note Close does not stop hardware or clear the bridge's recovery duty.
 */
nx_result_t nx_flashdb_close(nx_flashdb_t* database);

/**
 * \brief Persist one copied KV value with upstream GC and recovery semantics.
 * \param[in] operation: Active open KVDB scope.
 * \param[in] key: Nonempty NUL-terminated key shorter than 64 bytes.
 * \param[in] value: Immutable bytes borrowed until return; NULL only if empty.
 * \param[in] bytes: Value length within the selected database capacity.
 * \return SUCCESS, EXHAUSTED, INVALID or error; failure may already have
 * effects. \note This writes immediately. It is not Settings set-plus-commit
 * staging.
 */
nx_result_t nx_flashdb_kv_set(const nx_flashdb_operation_t* operation,
                              const char* key, const void* value, size_t bytes);

/**
 * \brief Copy one KV value without exposing retained vendor pointers.
 * \param[in] operation: Active open KVDB scope.
 * \param[in] key: Exact bounded key.
 * \param[out] value: Disjoint output or NULL to query required bytes.
 * \param[in,out] bytes: Capacity on input, required bytes on success/EXHAUSTED.
 * \return SUCCESS, EMPTY, EXHAUSTED or error; no read truncation success.
 */
nx_result_t nx_flashdb_kv_get(const nx_flashdb_operation_t* operation,
                              const char* key, void* value, size_t* bytes);

/**
 * \brief Persist deletion of one key, independently of any other key.
 * \param[in] operation: Active open KVDB scope.
 * \param[in] key: Exact bounded key.
 * \return SUCCESS, EMPTY or error; no multi-key snapshot promise.
 */
nx_result_t nx_flashdb_kv_remove(const nx_flashdb_operation_t* operation,
                                 const char* key);

/** \brief Borrowed record description valid only during its visitor callback.
 */
typedef struct {
    uint32_t index;
    int64_t timestamp;
    nx_flashdb_record_status_t status;
    const void* data;
    size_t bytes;
} nx_flashdb_record_t;
/** \brief Return true to stop traversal; no recursive database operations. */
typedef bool (*nx_flashdb_visit_t)(void* context,
                                   const nx_flashdb_record_t* record);

/**
 * \brief Append one timestamped TS record without implying business delivery.
 * \param[in] operation: Active open TSDB scope.
 * \param[in] timestamp: Nonnegative strictly increasing selected 64-bit time.
 * \param[in] data: Borrowed immutable record bytes.
 * \param[in] bytes: Positive bytes at most the configured maximum.
 * \return SUCCESS, EXHAUSTED, INVALID or error; never silently rolls over.
 * \note Rollover is disabled by this profile. Full media requires an explicit
 *       product retention/reclamation decision; no reliable outbox is implied.
 */
nx_result_t nx_flashdb_ts_append(const nx_flashdb_operation_t* operation,
                                 int64_t timestamp, const void* data,
                                 size_t bytes);

/**
 * \brief Visit committed TS records using caller-provided scratch bytes.
 * \param[in] operation: Active open TSDB scope, retained through all callbacks.
 * \param[in,out] scratch: Disjoint caller buffer valid during this call only.
 * \param[in] scratch_bytes: At least the configured maximum record length.
 * \param[in] visit: Bounded callback; cannot retain data or reenter database.
 * \param[in] context: Caller callback context.
 * \return SUCCESS or error; callback observes only complete copied records.
 */
nx_result_t nx_flashdb_ts_visit(const nx_flashdb_operation_t* operation,
                                void* scratch, size_t scratch_bytes,
                                nx_flashdb_visit_t visit, void* context);

/**
 * \brief Advance a still-existing record status without repeating its pulse.
 * \param[in] operation: Active TSDB scope, serialized with all traversal.
 * \param[in] index: Record index obtained from this database's visitor.
 * \param[in] status: Same or later user status; backward transitions reject.
 * \return SUCCESS, EMPTY, INVALID or retained error; same status causes no
 *         program pulse.
 * \note An uncertain write closes admission; no retry infers fresh erase from
 *       0xff. Status is local product metadata, not remote acknowledgement.
 */
nx_result_t nx_flashdb_ts_mark(const nx_flashdb_operation_t* operation,
                               uint32_t index,
                               nx_flashdb_record_status_t status);

#ifdef __cplusplus
}
#endif
#endif
