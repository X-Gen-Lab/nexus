/**
 * \file            fault_storage.h
 * \brief           Normal-context persistence of one raw crash
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_FAULT_STORAGE_H
#define NEXUS_COMPONENTS_FAULT_STORAGE_H
#include "nexus/components/fault_record.h"
#include "nexus/components/storage.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Persist a validated raw record before acknowledging
 * retention.
 *
 * \param[in,out]   record: Retained source, exclusively owned
 * through this call.
 *
 * \param[in,out]   storage: Already-open exclusive
 * product-selected Storage.
 *
 * \return          OK after durable save and exact
 * acknowledgement, NOT_FOUND for absent retained record, CORRUPT for damaged
 * capture, or original Storage failure. Failure keeps the source committed.
 *
 * \note            Privileged task/startup after storage initialization only;
 *                  never call from an exception. Uses existing whole-blob
 *                  Storage, without reserving Flash, formatting, retrying,
 *                  changing schema or opening a database. An ambiguous failed
 *                  commit may already exist durably; consumers use sequence and
 *                  build ID for duplicate handling. Reopen Storage as required
 *                  by its mutation-failure contract before another attempt.
 */
nx_storage_status_t nx_fault_record_persist(volatile nx_fault_record_t* record,
                                            nx_storage_t* storage);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_COMPONENTS_FAULT_STORAGE_H */
