/**
 * \file            fault_record.c
 * \brief           Versioned raw crash record with last-word publication
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/fault_record.h"
#include "nexus/arch/arch.h"
#include <string.h>

#define RECORD_MAGIC     UINT32_C(0x5246584e)
#define RECORD_COMMITTED UINT32_C(0x4346584e)
#define RECORD_WRITING   UINT32_C(0x5746584e)

_Static_assert(sizeof(nx_fault_record_t) == NX_FAULT_RECORD_BYTES,
               "Fault record wire size must remain exact");

/** \brief Validate spans before touching even the retained publication word. */
static bool overlaps(const volatile void* first, size_t bytes,
                     const void* second, size_t other_bytes) {
    uintptr_t a = (uintptr_t)first;
    uintptr_t b = (uintptr_t)second;
    return bytes > UINTPTR_MAX - a || other_bytes > UINTPTR_MAX - b ||
           (a < b + other_bytes && b < a + bytes);
}

/** \brief Write the wire value without depending on ABI layout or endianness.
 */
static void put32(uint8_t* output, uint32_t value) {
    for (unsigned i = 0U; i < 4U; ++i) {
        output[i] = (uint8_t)(value >> (8U * i));
    }
}

/** \brief Decode unaligned wire bytes without casting them to host words. */
static uint32_t get32(const uint8_t* input) {
    uint32_t value = 0U;
    for (unsigned i = 0U; i < 4U; ++i) {
        value |= (uint32_t)input[i] << (8U * i);
    }
    return value;
}

/** \brief Fixed 152-byte IEEE CRC guards this record, not arbitrary storage. */
static uint32_t checksum(const uint8_t* bytes) {
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0U; i < 152U; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0U; bit < 8U; ++bit) {
            crc = (crc >> 1U) ^ (UINT32_C(0xedb88320) & (0U - (crc & 1U)));
        }
    }
    return ~crc;
}

/** \brief Encode only declared numeric facts; all reserved bytes remain zero.
 */
static void encode(uint8_t* bytes, const nx_fault_capture_t* facts) {
    memset(bytes, 0, 156U);
    put32(bytes, RECORD_MAGIC);
    put32(bytes + 4U, 1U);
    put32(bytes + 8U, NX_FAULT_RECORD_BYTES);
    put32(bytes + 12U, facts->sequence);
    put32(bytes + 16U, facts->domain);
    put32(bytes + 20U, facts->reason);
    put32(bytes + 24U, facts->flags);
    put32(bytes + 28U, facts->exception);
    put32(bytes + 32U, facts->exception_return);
    put32(bytes + 36U, facts->msp);
    put32(bytes + 40U, facts->psp);
    memcpy(bytes + 44U, facts->build_id, NX_FAULT_BUILD_ID_BYTES);
    for (size_t i = 0U; i < 8U; ++i) {
        put32(bytes + 60U + 4U * i, facts->core[i]);
        put32(bytes + 92U + 4U * i, facts->status[i]);
    }
    put32(bytes + 124U, facts->stack_begin);
    put32(bytes + 128U, facts->stack_end);
    put32(bytes + 132U, facts->source_line);
    put32(bytes + 152U, checksum(bytes));
}

/** \brief One weak exclusive attempt avoids an unbounded retry on nested fault.
 */
nx_result_t nx_fault_record_capture(volatile nx_fault_record_t* record,
                                    const nx_fault_capture_t* facts) {
    if (record == NULL || facts == NULL ||
        (uintptr_t)record % _Alignof(nx_fault_record_t) != 0U ||
        overlaps(record, sizeof(*record), facts, sizeof(*facts))) {
        return NX_ERROR_INVALID;
    }
#if defined(__ARM_ARCH_6M__)
    return NX_ERROR_UNSUPPORTED;
#else
    uint32_t expected = __atomic_load_n(&record->committed, __ATOMIC_ACQUIRE);
    if (expected == RECORD_COMMITTED || expected == RECORD_WRITING ||
        !__atomic_compare_exchange_n(&record->committed, &expected,
                                     RECORD_WRITING, true, __ATOMIC_ACQ_REL,
                                     __ATOMIC_ACQUIRE)) {
        return NX_ERROR_BUSY;
    }
    uint8_t bytes[156];
    encode(bytes, facts);
    for (size_t i = 0U; i < sizeof(bytes); ++i) {
        record->bytes[i] = bytes[i];
    }
    __atomic_store_n(&record->committed, RECORD_COMMITTED, __ATOMIC_RELEASE);
    nx_arch_dsb();
    return NX_SUCCESS;
#endif
}

/** \brief Validate the copied publication before exposing decoded facts. */
nx_result_t nx_fault_record_read(const volatile nx_fault_record_t* record,
                                 nx_fault_capture_t* facts) {
    if (record == NULL || facts == NULL ||
        (uintptr_t)record % _Alignof(nx_fault_record_t) != 0U ||
        overlaps(record, sizeof(*record), facts, sizeof(*facts))) {
        return NX_ERROR_INVALID;
    }
    if (__atomic_load_n(&record->committed, __ATOMIC_ACQUIRE) !=
        RECORD_COMMITTED) {
        return NX_ERROR_EMPTY;
    }
    uint8_t bytes[156];
    for (size_t i = 0U; i < sizeof(bytes); ++i) {
        bytes[i] = record->bytes[i];
    }
    if (get32(bytes) != RECORD_MAGIC ||
        get32(bytes + 8U) != NX_FAULT_RECORD_BYTES ||
        get32(bytes + 152U) != checksum(bytes)) {
        return NX_ERROR_IO;
    }
    if (get32(bytes + 4U) != 1U) {
        return NX_ERROR_UNSUPPORTED;
    }
    nx_fault_capture_t result = {0};
    result.sequence = get32(bytes + 12U);
    result.domain = get32(bytes + 16U);
    result.reason = get32(bytes + 20U);
    result.flags = get32(bytes + 24U);
    result.exception = get32(bytes + 28U);
    result.exception_return = get32(bytes + 32U);
    result.msp = get32(bytes + 36U);
    result.psp = get32(bytes + 40U);
    memcpy(result.build_id, bytes + 44U, NX_FAULT_BUILD_ID_BYTES);
    for (size_t i = 0U; i < 8U; ++i) {
        result.core[i] = get32(bytes + 60U + 4U * i);
        result.status[i] = get32(bytes + 92U + 4U * i);
    }
    result.stack_begin = get32(bytes + 124U);
    result.stack_end = get32(bytes + 128U);
    result.source_line = get32(bytes + 132U);
    *facts = result;
    return NX_SUCCESS;
}

/** \brief Normal-context acknowledgement clears only the expected publication.
 */
nx_result_t nx_fault_record_ack(volatile nx_fault_record_t* record,
                                uint32_t sequence) {
    nx_fault_capture_t facts;
    nx_result_t result = nx_fault_record_read(record, &facts);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (facts.sequence != sequence) {
        return NX_ERROR_STATE;
    }
    __atomic_store_n(&record->committed, 0U, __ATOMIC_RELEASE);
    return NX_SUCCESS;
}

/** \brief Reset classification never turns a damaged crash into an empty boot.
 */
nx_result_t nx_reset_record_read(const volatile nx_fault_record_t* record,
                                 uint32_t reset_causes,
                                 nx_reset_record_t* output) {
    if (output == NULL || record == NULL ||
        overlaps(record, sizeof(*record), output, sizeof(*output))) {
        return NX_ERROR_INVALID;
    }
    nx_reset_record_t result = {0};
    result.reset_causes = reset_causes;
    nx_result_t status = nx_fault_record_read(record, &result.fault);
    if (status != NX_SUCCESS && status != NX_ERROR_EMPTY) {
        return status;
    }
    result.has_fault = status == NX_SUCCESS;
    *output = result;
    return NX_SUCCESS;
}
