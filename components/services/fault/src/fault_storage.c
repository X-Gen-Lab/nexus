/**
 * \file            fault_storage.c
 * \brief           Existing durable Storage owns crash replay commit
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/fault_storage.h"
#include "nexus/arch/arch.h"

/** \brief Persist first; failed or ambiguous mutation keeps retained recovery.
 */
nx_storage_status_t nx_fault_record_persist(volatile nx_fault_record_t* record,
                                            nx_storage_t* storage) {
    if (record == NULL || storage == NULL || nx_arch_in_isr()) {
        return NX_STORAGE_INVALID;
    }
    nx_fault_capture_t facts;
    nx_result_t read = nx_fault_record_read(record, &facts);
    if (read == NX_ERROR_EMPTY) {
        return NX_STORAGE_NOT_FOUND;
    }
    if (read != NX_SUCCESS) {
        return read == NX_ERROR_INVALID ? NX_STORAGE_INVALID
                                        : NX_STORAGE_CORRUPT;
    }
    nx_fault_record_t copy;
    for (size_t i = 0U; i < sizeof(copy.bytes); ++i) {
        copy.bytes[i] = record->bytes[i];
    }
    copy.committed = record->committed;
    nx_storage_status_t result = nx_storage_save(storage, &copy, sizeof(copy));
    if (result != NX_STORAGE_OK) {
        return result;
    }
    return nx_fault_record_ack(record, facts.sequence) == NX_SUCCESS
               ? NX_STORAGE_OK
               : NX_STORAGE_CORRUPT;
}
