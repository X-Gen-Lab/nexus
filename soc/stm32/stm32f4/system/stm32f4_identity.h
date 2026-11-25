/**
 * \file            stm32f4_identity.h
 * \brief           Read-only selected device and Flash density admission
 * \author          Nexus Team
 */
#ifndef NX_STM32F4_IDENTITY_H
#define NX_STM32F4_IDENTITY_H

#include "nexus/core/status.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Check observed device group, reviewed cut and Flash density.
 * \param[in] expected_flash_bytes: Selected reviewed density in bytes.
 * \return SUCCESS, INVALID for unknown expected density, IO for mismatched
 *         observed die/density, or UNSUPPORTED for an unreviewed cut.
 * \note Serialized cold startup only; no register writes or retained state.
 *       Read the actual IDCODE once; configured revisions never substitute
 *       for observation. Device IDs do not establish package or ordering part.
 */
nx_result_t nx_stm32_validate_identity(uint32_t expected_flash_bytes);

#ifdef __cplusplus
}
#endif

#endif /* NX_STM32F4_IDENTITY_H */
