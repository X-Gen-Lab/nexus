/**
 * \file            stm32f4_qspi_mapped.h
 * \brief           Exact F479 CPU-read loans over the sole QSPI transport
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_QSPI_MAPPED_H
#define NEXUS_STM32F4_QSPI_MAPPED_H
#include "nexus/components/mapped_storage_owner.h"
#include "stm32f4_qspi.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Selected immutable slots; construction never allocates a pool. */
struct nx_stm32_qspi_mapped_config {
    nx_mapped_storage_owner_t* owner;
    nx_mapped_storage_slot_t* slots;
    size_t count;
};
/** \brief One composite backend span, containing the only HAL/QSPI owner.
 * \note Generic owner and slots are separate. All storage survives failed
 * map, writer drain and cold stop. Old controls never authorize new loans.
 */
typedef struct {
    nx_stm32_qspi_t controller;
    const nx_stm32_qspi_mapped_config_t* binding;
    nx_flash_async_request_t inner;
    nx_result_t inner_result;
    size_t inner_completed;
    bool constructed;
    bool inner_live;
    bool mapping_active;
} nx_stm32_qspi_mapped_t;
/** \brief Shared task-only physical mapping face; no raw Flash face is emitted.
 */
extern const nx_mapped_storage_ops_t nx_stm32_qspi_mapped_ops;
/** \brief Validate static mapped storage before Platform changes registers.
 * \param[in] owner: First member of the original mapped composite.
 * \param[in] config: Immutable exact slots and portable owner binding.
 * \return SUCCESS or INVALID without effects or mutation.
 */
nx_result_t nx_stm32_qspi_mapped_admit(const nx_stm32_qspi_t* owner,
                                       const nx_stm32_qspi_config_t* config);
/** \brief Construct once or reopen after a completely proved cold restart.
 * \param[in,out] owner: Prepared sole hardware transport, never a second one.
 * \param[in] config: Same lifetime owner and exact slots.
 * \return SUCCESS or retained cold failure; epochs are never reset on restart.
 */
nx_result_t
nx_stm32_qspi_mapped_prepare(nx_stm32_qspi_t* owner,
                             const nx_stm32_qspi_mapped_config_t* config);
/** \brief Seal and join readers and writer before cold register withdrawal.
 * \param[in,out] owner: Original controller/composite storage.
 * \return SUCCESS or BUSY/error retaining all storage and hardware resources.
 */
nx_result_t nx_stm32_qspi_mapped_stop(nx_stm32_qspi_t* owner);
/** \brief Check task context and the original prepared System epoch.
 * \param[in] owner: Original hardware owner.
 * \return SUCCESS or INVALID/CONTEXT/STATE; no hardware effects.
 */
nx_result_t nx_stm32_qspi_mapping_ready(const nx_stm32_qspi_t* owner);
/** \brief Protect all hardware metadata before outer request admission.
 * \param[in] owner: Original owner with live immutable configuration.
 * \param[in] object: Proposed request, output or payload span.
 * \param[in] bytes: Positive exact span length.
 * \param[in] write: Whether the span must be writable.
 * \return True only for disjoint non-MMIO storage.
 */
bool nx_stm32_qspi_mapping_external(const nx_stm32_qspi_t* owner,
                                    const void* object, size_t bytes,
                                    bool write);
/** \brief Apply reviewed AR/ABORT and TCEN0 before publishing CPU access.
 * \param[in,out] owner: Sole prepared transport; all foreign masters excluded.
 * \return Readable mapping or explicit retained cleanup, never a reader loan.
 */
nx_mapped_storage_map_result_t
nx_stm32_qspi_mapping_enter(nx_stm32_qspi_t* owner);
/** \brief After all CPU readers join, DSB and prove actual mapped-mode exit.
 * \param[in,out] owner: Original controller; storage remains live on failure.
 * \return SUCCESS only after mode/FIFO/BUSY/source withdrawal; otherwise retry.
 */
nx_result_t nx_stm32_qspi_mapping_leave(nx_stm32_qspi_t* owner);
#ifdef __cplusplus
}
#endif
#endif
