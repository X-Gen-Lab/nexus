/**
 * \file            composition.h
 * \brief           Vendor-free interface for the private ARM link composition
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-06
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_H743_W25Q128_REFERENCE_COMPOSITION_H
#define NX_H743_W25Q128_REFERENCE_COMPOSITION_H
#include "nexus/components/mapped_storage.h"
/** \brief Link-only reference; external pin/clock/MPU leases are prerequisites.
 */
nx_result_t nx_h743_w25q128_reference_prepare(void);
/** \brief Advance one finite discovery/request step. */
void nx_h743_w25q128_reference_service(void);
/** \brief Return the generic face only after successful private construction.
 */
const nx_mapped_storage_port_t* nx_h743_w25q128_reference_storage(void);
/** \brief Stop the owner before releasing its private controller state. */
nx_result_t nx_h743_w25q128_reference_stop(void);
/** \brief Compilation rejects vendor/private headers reaching public code. */
bool nx_h743_w25q128_public_headers_hidden(void);
#endif
