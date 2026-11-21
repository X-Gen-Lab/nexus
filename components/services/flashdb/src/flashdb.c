/**
 * \file            flashdb.c
 * \brief           Real FlashDB algorithms with instance-stable FAL ABI views
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/flashdb.h"
#include <flashdb.h>
#include <stddef.h>
#include <string.h>

/** \brief Vendor state stays private and is allocated only by the caller. */
typedef struct {
    nx_flashdb_t* owner;
    struct fal_partition partition;
    struct fal_flash_dev device;
    union {
        struct fdb_kvdb kv;
        struct fdb_tsdb ts;
    } database;
} database_state_t;

/** \brief Check known storage extents before publishing a workspace binding. */
static bool overlaps(const void* first, size_t first_bytes, const void* second,
                     size_t second_bytes) {
    uintptr_t left = (uintptr_t)first;
    uintptr_t right = (uintptr_t)second;
    return first_bytes > UINTPTR_MAX - left ||
           second_bytes > UINTPTR_MAX - right ||
           (left < right + second_bytes && right < left + first_bytes);
}

/** \brief The upstream FAL view is stable per database, not a global table. */
static database_state_t* state_for_part(const struct fal_partition* partition) {
    return (database_state_t*)((uint8_t*)partition -
                               offsetof(database_state_t, partition));
}

/** \brief Initialization lookup uses only an explicitly borrowed product scope.
 */
static database_state_t* selected_state(void) {
    const nx_flashdb_operation_t* operation = nx_flashdb_operation_get();
    return operation != NULL && operation->database != NULL &&
                   operation->database->entered
               ? operation->database->state
               : NULL;
}

/** \brief No hardware initialization or global FAL table exists in this
 * profile. */
int fal_init(void) {
    return selected_state() != NULL ? 1 : -1;
}

/** \brief Resolve only this profile's private device spelling. */
const struct fal_flash_dev* fal_flash_device_find(const char* name) {
    database_state_t* state = selected_state();
    return state != NULL && name != NULL &&
                   strcmp(name, state->device.name) == 0
               ? &state->device
               : NULL;
}

/** \brief Return the exact instance partition retained by the upstream DB. */
const struct fal_partition* fal_partition_find(const char* name) {
    database_state_t* state = selected_state();
    return state != NULL && name != NULL &&
                   strcmp(name, state->partition.name) == 0
               ? &state->partition
               : NULL;
}

/** \brief Stable partition identity maps directly to its original region owner.
 */
int fal_partition_read(const struct fal_partition* partition, uint32_t address,
                       uint8_t* data, size_t bytes) {
    database_state_t* state = state_for_part(partition);
    return nx_fal_read(state->owner->flash, address, data, bytes);
}

/** \brief Program exact bytes without creating another hardware owner. */
int fal_partition_write(const struct fal_partition* partition, uint32_t address,
                        const uint8_t* data, size_t bytes) {
    database_state_t* state = state_for_part(partition);
    return nx_fal_program(state->owner->flash, address, data, bytes);
}

/** \brief Preserve whole-sector bounds and the transaction's original deadline.
 */
int fal_partition_erase(const struct fal_partition* partition, uint32_t address,
                        size_t bytes) {
    database_state_t* state = state_for_part(partition);
    return nx_fal_erase(state->owner->flash, address, bytes);
}

/** \brief Report exact selected-profile allocation requirements. */
size_t nx_flashdb_workspace_bytes(void) {
    return sizeof(database_state_t);
}

/** \brief Vendor types are never required in public consumer headers. */
size_t nx_flashdb_workspace_alignment(void) {
    return _Alignof(database_state_t);
}

/** \brief Bind only the fixed non-ECC, 32-bit compatible storage profile. */
nx_result_t nx_flashdb_init(nx_flashdb_t* database, nx_fal_t* flash,
                            nx_flashdb_kind_t kind, void* workspace,
                            size_t workspace_bytes, size_t max_record_bytes) {
    if (database == NULL || flash == NULL || workspace == NULL ||
        workspace_bytes < sizeof(database_state_t) ||
        (uintptr_t)workspace % _Alignof(database_state_t) != 0U ||
        (kind != NX_FLASHDB_KV && kind != NX_FLASHDB_TS) ||
        overlaps(database, sizeof(*database), workspace, workspace_bytes) ||
        overlaps(flash, sizeof(*flash), workspace, workspace_bytes) ||
        overlaps(database, sizeof(*database), flash, sizeof(*flash))) {
        return NX_ERROR_INVALID;
    }
    if (flash->region.port == NULL || flash->program_bytes == 0U ||
        4U % flash->program_bytes != 0U || flash->erase_bytes == 0U ||
        (flash->erase_bytes & (flash->erase_bytes - 1U)) != 0U ||
        flash->region.size / flash->erase_bytes < 2U ||
        (kind == NX_FLASHDB_TS &&
         (max_record_bytes == 0U ||
          max_record_bytes > flash->erase_bytes / 2U))) {
        return NX_ERROR_UNSUPPORTED;
    }
    database_state_t* state = workspace;
    memset(state, 0, sizeof(*state));
    nx_flashdb_t initialized = {
        flash, workspace, workspace_bytes, max_record_bytes, kind,
        false, false};
    *database = initialized;
    state->owner = database;
    state->partition.magic_word = UINT32_C(0x45503130);
    memcpy(state->partition.name, "nexus", sizeof("nexus"));
    memcpy(state->partition.flash_name, "nexus", sizeof("nexus"));
    state->partition.len = flash->region.size;
    memcpy(state->device.name, "nexus", sizeof("nexus"));
    state->device.len = flash->region.size;
    state->device.blk_size = flash->erase_bytes;
    state->device.write_gran = (size_t)flash->program_bytes * 8U;
    return NX_SUCCESS;
}

/** \brief Validate product scope before making the first upstream callback. */
static nx_result_t begin(const nx_flashdb_operation_t* operation,
                         bool require_open) {
    if (operation == NULL || nx_flashdb_operation_get() != operation ||
        operation->database == NULL || operation->database->state == NULL) {
        return NX_ERROR_STATE;
    }
    nx_flashdb_t* database = operation->database;
    if (database->entered) {
        return NX_ERROR_BUSY;
    }
    if (require_open && !database->opened) {
        return NX_ERROR_STATE;
    }
    nx_result_t result = nx_fal_begin(database->flash, operation->deadline);
    if (result == NX_SUCCESS) {
        database->entered = true;
    }
    return result;
}

/** \brief Translate only the upstream outcomes with a defined public meaning.
 */
static nx_result_t translated(fdb_err_t error) {
    if (error == FDB_NO_ERR) {
        return NX_SUCCESS;
    }
    if (error == FDB_SAVED_FULL) {
        return NX_ERROR_EXHAUSTED;
    }
    return error == FDB_KV_NAME_ERR ? NX_ERROR_INVALID : NX_ERROR_IO;
}

/** \brief Withdraw logical admission on any observed storage error. */
static nx_result_t finish(nx_flashdb_t* database, nx_result_t result) {
    nx_result_t io = nx_fal_end(database->flash);
    database->entered = false;
    if (io != NX_SUCCESS) {
        database->opened = false;
        database_state_t* state = database->state;
        state->database.kv.parent.init_ok = false;
        return io;
    }
    return result;
}

/** \brief Check the entire selected region before permission to initialize it.
 */
static nx_result_t erased(nx_flashdb_t* database, bool* empty) {
    uint8_t bytes[64];
    *empty = true;
    for (uint32_t offset = 0U; offset < database->flash->region.size;) {
        size_t length = database->flash->region.size - offset;
        if (length > sizeof(bytes)) {
            length = sizeof(bytes);
        }
        if (nx_fal_read(database->flash, offset, bytes, length) !=
            (int)length) {
            return NX_ERROR_IO;
        }
        for (size_t index = 0U; index < length; ++index) {
            if (bytes[index] != UINT8_C(0xff)) {
                *empty = false;
                return NX_SUCCESS;
            }
        }
        offset += (uint32_t)length;
    }
    return NX_SUCCESS;
}

/** \brief Required upstream callback; explicit append timestamps are used. */
static fdb_time_t unused_time(void) {
    return 0;
}

/** \brief Never turn an unreadable or unknown existing image into an empty DB.
 */
nx_result_t nx_flashdb_open(const nx_flashdb_operation_t* operation,
                            nx_flashdb_open_policy_t policy) {
    if (policy != NX_FLASHDB_EXISTING_ONLY &&
        policy != NX_FLASHDB_CREATE_IF_ERASED) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = begin(operation, false);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_flashdb_t* database = operation->database;
    if (database->opened) {
        return finish(database, NX_ERROR_STATE);
    }
    bool empty;
    result = erased(database, &empty);
    if (result != NX_SUCCESS || (empty && policy == NX_FLASHDB_EXISTING_ONLY)) {
        return finish(database, result != NX_SUCCESS ? result : NX_ERROR_EMPTY);
    }
    database_state_t* state = database->state;
    memset(&state->database, 0, sizeof(state->database));
    bool not_format = !empty;
    fdb_err_t upstream;
    if (database->kind == NX_FLASHDB_KV) {
        fdb_kvdb_control(&state->database.kv, FDB_KVDB_CTRL_SET_NOT_FORMAT,
                         &not_format);
        upstream = fdb_kvdb_init(&state->database.kv, "nexus", "nexus", NULL,
                                 database);
    } else {
        fdb_tsdb_control(&state->database.ts, FDB_TSDB_CTRL_SET_NOT_FORMAT,
                         &not_format);
        upstream =
            fdb_tsdb_init(&state->database.ts, "nexus", "nexus", unused_time,
                          database->max_record_bytes, database);
        if (upstream == FDB_NO_ERR) {
            bool rollover = false;
            fdb_tsdb_control(&state->database.ts, FDB_TSDB_CTRL_SET_ROLLOVER,
                             &rollover);
        }
    }
    result = finish(database, translated(upstream));
    database->opened = result == NX_SUCCESS;
    return result;
}

/** \brief Explicit destructive erase is never an open-error fallback. */
nx_result_t nx_flashdb_format(const nx_flashdb_operation_t* operation) {
    nx_result_t result = begin(operation, false);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_flashdb_t* database = operation->database;
    if (database->opened) {
        return finish(database, NX_ERROR_STATE);
    }
    for (uint32_t offset = 0U; offset < database->flash->region.size;
         offset += database->flash->erase_bytes) {
        if (nx_fal_erase(database->flash, offset,
                         database->flash->erase_bytes) < 0) {
            return finish(database, NX_ERROR_IO);
        }
    }
    return finish(database, NX_SUCCESS);
}

/** \brief Logical close never clears physical failed ownership. */
nx_result_t nx_flashdb_close(nx_flashdb_t* database) {
    if (database == NULL) {
        return NX_ERROR_INVALID;
    }
    if (database->entered) {
        return NX_ERROR_BUSY;
    }
    if (database->state != NULL) {
        database_state_t* state = database->state;
        if (database->kind == NX_FLASHDB_KV) {
            (void)fdb_kvdb_deinit(&state->database.kv);
        } else {
            (void)fdb_tsdb_deinit(&state->database.ts);
        }
    }
    database->opened = false;
    return NX_SUCCESS;
}

/** \brief A bounded key check prevents upstream strlen from becoming unbounded.
 */
static bool valid_key(const char* key) {
    if (key == NULL || key[0] == '\0') {
        return false;
    }
    for (size_t length = 1U; length < 64U; ++length) {
        if (key[length] == '\0') {
            return true;
        }
    }
    return false;
}

/** \brief Persist one key without changing Settings snapshot semantics. */
nx_result_t nx_flashdb_kv_set(const nx_flashdb_operation_t* operation,
                              const char* key, const void* value,
                              size_t bytes) {
    if (!valid_key(key) || (value == NULL && bytes != 0U)) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = begin(operation, true);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_flashdb_t* database = operation->database;
    if (database->kind != NX_FLASHDB_KV ||
        bytes > database->flash->region.size) {
        return finish(database, NX_ERROR_INVALID);
    }
    database_state_t* state = database->state;
    uint8_t empty_value = 0U;
    struct fdb_blob blob;
    fdb_blob_make(&blob, value != NULL ? value : &empty_value, bytes);
    return finish(database,
                  translated(fdb_kv_set_blob(&state->database.kv, key, &blob)));
}

/** \brief Check complete required size before copying any value prefix. */
nx_result_t nx_flashdb_kv_get(const nx_flashdb_operation_t* operation,
                              const char* key, void* value, size_t* bytes) {
    if (!valid_key(key) || bytes == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = begin(operation, true);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_flashdb_t* database = operation->database;
    if (database->kind != NX_FLASHDB_KV) {
        return finish(database, NX_ERROR_INVALID);
    }
    database_state_t* state = database->state;
    struct fdb_kv object;
    if (fdb_kv_get_obj(&state->database.kv, key, &object) == NULL) {
        return finish(database, NX_ERROR_EMPTY);
    }
    size_t capacity = *bytes;
    *bytes = object.value_len;
    if (value == NULL || object.value_len == 0U) {
        return finish(database, NX_SUCCESS);
    }
    if (capacity < object.value_len) {
        return finish(database, NX_ERROR_EXHAUSTED);
    }
    struct fdb_blob blob;
    fdb_blob_make(&blob, value, object.value_len);
    size_t copied = fdb_kv_get_blob(&state->database.kv, key, &blob);
    return finish(database,
                  copied == object.value_len ? NX_SUCCESS : NX_ERROR_IO);
}

/** \brief Deletion is independent per key and never silently creates a value.
 */
nx_result_t nx_flashdb_kv_remove(const nx_flashdb_operation_t* operation,
                                 const char* key) {
    if (!valid_key(key)) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = begin(operation, true);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_flashdb_t* database = operation->database;
    if (database->kind != NX_FLASHDB_KV) {
        return finish(database, NX_ERROR_INVALID);
    }
    database_state_t* state = database->state;
    struct fdb_kv object;
    if (fdb_kv_get_obj(&state->database.kv, key, &object) == NULL) {
        return finish(database, NX_ERROR_EMPTY);
    }
    return finish(database, translated(fdb_kv_del(&state->database.kv, key)));
}

/** \brief Explicit timestamps avoid hidden global wall-clock callbacks. */
nx_result_t nx_flashdb_ts_append(const nx_flashdb_operation_t* operation,
                                 int64_t timestamp, const void* data,
                                 size_t bytes) {
    nx_result_t result = begin(operation, true);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_flashdb_t* database = operation->database;
    database_state_t* state = database->state;
    if (database->kind != NX_FLASHDB_TS || data == NULL || bytes == 0U ||
        bytes > database->max_record_bytes || timestamp < 0 ||
        timestamp <= state->database.ts.last_time) {
        return finish(database, NX_ERROR_INVALID);
    }
    struct fdb_blob blob;
    fdb_blob_make(&blob, data, bytes);
    return finish(database, translated(fdb_tsl_append_with_ts(
                                &state->database.ts, &blob, timestamp)));
}

/** \brief Traversal scratch and user context are borrowed only for this call.
 */
typedef struct {
    nx_flashdb_t* database;
    void* scratch;
    nx_flashdb_visit_t visit;
    void* context;
    nx_result_t result;
} visit_context_t;

/** \brief Publish only a completely copied committed record to the visitor. */
static bool visit_record(fdb_tsl_t log, void* context) {
    visit_context_t* traversal = context;
    if (log->status < FDB_TSL_WRITE) {
        return false;
    }
    if (log->log_len > traversal->database->max_record_bytes) {
        traversal->result = NX_ERROR_IO;
        return true;
    }
    database_state_t* state = traversal->database->state;
    struct fdb_blob blob;
    fdb_blob_make(&blob, traversal->scratch, log->log_len);
    fdb_tsl_to_blob(log, &blob);
    if (fdb_blob_read((fdb_db_t)&state->database.ts, &blob) != log->log_len) {
        traversal->result = NX_ERROR_IO;
        return true;
    }
    nx_flashdb_record_t record = {log->addr.index, log->time,
                                  (nx_flashdb_record_status_t)log->status,
                                  traversal->scratch, log->log_len};
    return traversal->visit(traversal->context, &record);
}

/** \brief Hold same-owner admission through visitor callbacks and withdrawal.
 */
nx_result_t nx_flashdb_ts_visit(const nx_flashdb_operation_t* operation,
                                void* scratch, size_t scratch_bytes,
                                nx_flashdb_visit_t visit, void* context) {
    nx_result_t result = begin(operation, true);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_flashdb_t* database = operation->database;
    if (database->kind != NX_FLASHDB_TS || scratch == NULL || visit == NULL ||
        scratch_bytes < database->max_record_bytes ||
        overlaps(database->state, database->state_bytes, scratch,
                 scratch_bytes)) {
        return finish(database, NX_ERROR_INVALID);
    }
    database_state_t* state = database->state;
    visit_context_t traversal = {database, scratch, visit, context, NX_SUCCESS};
    fdb_tsl_iter(&state->database.ts, visit_record, &traversal);
    return finish(database, traversal.result);
}

/** \brief Resolve a fresh upstream record before advancing its status. */
typedef struct {
    uint32_t index;
    struct fdb_tsl record;
    bool found;
} find_context_t;

/** \brief Do not trust an old caller-provided address as an upstream object. */
static bool find_record(fdb_tsl_t log, void* context) {
    find_context_t* search = context;
    if (log->addr.index == search->index && log->status >= FDB_TSL_WRITE) {
        search->record = *log;
        search->found = true;
        return true;
    }
    return false;
}

/** \brief Duplicate acknowledged states avoid repeating a physical program. */
nx_result_t nx_flashdb_ts_mark(const nx_flashdb_operation_t* operation,
                               uint32_t index,
                               nx_flashdb_record_status_t status) {
    nx_result_t result = begin(operation, true);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_flashdb_t* database = operation->database;
    if (database->kind != NX_FLASHDB_TS || status < NX_FLASHDB_RECORD_WRITTEN ||
        status > NX_FLASHDB_RECORD_USER2) {
        return finish(database, NX_ERROR_INVALID);
    }
    database_state_t* state = database->state;
    find_context_t search = {0};
    search.index = index;
    fdb_tsl_iter(&state->database.ts, find_record, &search);
    if (!search.found) {
        return finish(database, NX_ERROR_EMPTY);
    }
    if ((int)status < (int)search.record.status) {
        return finish(database, NX_ERROR_INVALID);
    }
    if ((int)status == (int)search.record.status) {
        return finish(database, NX_SUCCESS);
    }
    return finish(database, translated(fdb_tsl_set_status(
                                &state->database.ts, &search.record,
                                (fdb_tsl_status_t)status)));
}
