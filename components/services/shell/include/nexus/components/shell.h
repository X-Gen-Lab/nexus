/**
 * \file            shell.h
 * \brief           Read-only letter-shell with task-owned execution
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_SHELL_H
#define NEXUS_COMPONENTS_SHELL_H
#include "nexus/components/fault_record.h"
#include "nexus/components/health.h"
#include "nexus/components/health_resources.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Exact backend layout is available only in shell_storage.h. */
typedef struct nx_shell nx_shell_t;

/**
 * \brief           Explicit synchronous byte transport; no device is
 * discovered.
 *
 * \note            read copies at most capacity bytes; EMPTY means
 * no data. write returns only after releasing its source bytes, including
 *                  failure. Both run in the calling task. Implementations must
 *                  return in their declared finite budget; no shell worker is
 *                  created. ISR/DMA publishers only fill their own RX storage.
 */
typedef struct {
    void* context;
    nx_result_t (*read)(void*, uint8_t*, size_t, size_t*);
    nx_result_t (*write)(void*, const void*, size_t, nx_time_us_t);
} nx_shell_io_t;

/**
 * \brief           Stable caller-owned diagnostics, read only during execution.
 *
 * \note            The caller serializes publication with shell execution.
 * These are value snapshots, never live request/provider pointers. Missing
 * values print unknown. Version is a valid immutable NUL-terminated string no
 * longer than 80 bytes.
 */
typedef struct {
    const char* version;
    const nx_health_snapshot_t* health;
    const nx_reset_record_t* reset;
    const nx_health_resources_t* resources;
} nx_shell_diagnostics_t;

/**
 * \brief           Initialize exact backend storage and caller line/history
 * bytes.
 *
 * \param[in,out]   shell: Unused zeroed shell_storage.h object, not
 * already live.
 *
 * \param[in]       io: Copied synchronous task transport.
 *
 * \param[out]      buffer: Exclusive line/history storage through stop.
 *
 * \param[in]       capacity: At least 64 bytes, at most UINT16_MAX.
 *
 * \param[in]       diagnostics: Stable diagnostic sources kept through stop.
 *
 * \param[in]       deadline: Absolute output deadline for the initial prompt.
 *
 * \return          SUCCESS, BUSY for concurrent/global backend entry, INVALID,
 *                  CONTEXT or transport error.
 *
 * \note            Uses the actual pinned letter-shell parser with a fixed
 *                  read-only command table: help, version, health, fault,
 *                  reset and resources. reset only reports captured causes.
 *                  No variable writes, address execution, maintenance commands,
 *                  allocation, task creation or product authentication is
 * added. The selected upstream profile supports one live shell; its transient
 * callback context is serialized without an OS lock.
 */
nx_result_t nx_shell_init(nx_shell_t* shell, nx_shell_io_t io, char* buffer,
                          size_t capacity,
                          const nx_shell_diagnostics_t* diagnostics,
                          nx_time_us_t deadline);

/**
 * \brief           Read and process at most one character in the owning task.
 *
 * \param[in,out]   shell: Live instance, exclusively owned by this task.
 *
 * \param[in]       deadline: Absolute output deadline, passed unchanged.
 *
 * \return          SUCCESS, EMPTY, BUSY, CONTEXT, STATE, or transport error.
 *
 * \note            A newline invokes a read-only command synchronously. Never
 *                  call from an ISR, trace hook or fault; no busy input loop,
 *                  implicit scheduling, kernel call or automatic retry occurs.
 */
nx_result_t nx_shell_step(nx_shell_t* shell, nx_time_us_t deadline);

/**
 * \brief           Detach the idle backend after publishers/consumers have
 * joined.
 *
 * \param[in,out]   shell: Unique task execution owner.
 *
 * \return SUCCESS,
 * BUSY during an entry, INVALID or CONTEXT.
 *
 * \note            No transport
 * shutdown or storage reclamation is performed.
 */
nx_result_t nx_shell_stop(nx_shell_t* shell);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_COMPONENTS_SHELL_H */
