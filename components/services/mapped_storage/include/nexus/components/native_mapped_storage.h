/**
 * \file            native_mapped_storage.h
 * \brief           Caller-owned coherent Native NOR model with bounded steps
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_NATIVE_MAPPED_STORAGE_H
#define NEXUS_COMPONENTS_NATIVE_MAPPED_STORAGE_H
#include "nexus/components/mapped_storage_owner.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief Independent Native backend state; never a physical Flash substitute
 * \note The model preserves supplied bytes, programs only 1-to-0, and erases
 * to 0xff. One service changes at most quantum bytes. No implicit file, heap,
 * thread, DMA, executable aperture or durability claim exists. fault_after
 * bounds mutated bytes; SIZE_MAX disables the fault. hold_drain models live
 * retained sources and drain_error models failure to prove source withdrawal.
 */
typedef struct {
    uint8_t* bytes;
    size_t size;
    size_t quantum;
    const uint8_t* input;
    size_t offset;
    size_t length;
    size_t completed;
    size_t fault_after;
    nx_mapped_storage_operation_t operation;
    bool active;
    bool cancelled;
    bool mapped;
    bool hold_drain;
    bool drain_error;
} nx_native_mapped_storage_t;

/**
 * \brief Construct an unused Native backend without modifying backing bytes
 * \return SUCCESS or INVALID. Only CPU_READ is supported; XIP and DMA tags
 * reject. Native coherent memory needs no cache maintenance. The portable
 * owner still excludes mappings from every modeled mutation and drain.
 */
nx_result_t nx_native_mapped_storage_init(nx_native_mapped_storage_t* model,
                                          uint8_t* bytes, size_t size,
                                          size_t program_unit,
                                          size_t erase_unit, size_t quantum,
                                          nx_mapped_storage_backend_t* backend);
#ifdef __cplusplus
}
#endif
#endif
