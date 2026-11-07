/**
 * \file            memory_window.h
 * \brief           Exact caller-owned CPU memory-window leases
 * \author          Nexus Team
 */
#ifndef NEXUS_MEMORY_WINDOW_H
#define NEXUS_MEMORY_WINDOW_H

#include "nexus/core/status.h"
#include "nexus/io/fwd.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Stable exclusive direct-access loan; zero before acquire
 * \note            Do not copy a live lease or alter fields. owner is opaque
 *                  provider identity, not a public device pointer. CPU accesses
 *                  use address directly without per-load dispatch. This first
 *                  profile permits CPU data read/write only; it does not grant
 *                  DMA, executable, allocator or C-runtime startup backing.
 *                  Join all derived pointer users before release. A timeout or
 *                  lost pointer never proves that those users have stopped.
 */
typedef struct {
    void* owner;
    uintptr_t address;
    size_t bytes;
    uint64_t epoch;
} nx_memory_window_lease_t;

/**
 * \brief           Readonly physical geometry, never permission to use bytes
 * \note            startup_backing marks memory with live startup objects.
 *                  A whole-bank runtime loan is unsupported for that profile.
 *                  Geometry does not authorize DMA, probing or allocation.
 */
typedef struct {
    uintptr_t address;
    size_t bytes;
    bool startup_backing;
} nx_memory_window_geometry_t;

/** \brief Immutable methods for one exact selected memory bank. */
typedef struct {
    nx_result_t (*acquire)(void* context, nx_memory_window_lease_t* lease);
    nx_result_t (*release)(void* context, nx_memory_window_lease_t* lease,
                           uint64_t epoch);
    nx_result_t (*geometry)(const void* context,
                            nx_memory_window_geometry_t* geometry);
} nx_memory_window_ops_t;

/** \brief Immutable face; lookup does not acquire, clear or test memory. */
struct nx_memory_window_port {
    const nx_memory_window_ops_t* ops;
    void* context;
};

/**
 * \brief           Borrow one complete admitted CPU memory window
 * \param[in]       port: Live selected bank under one serialized task owner
 * \param[in,out]   lease: Fresh zero stable disjoint caller storage
 * \return          SUCCESS with exact address/bytes/epoch, or rejection with
 *                  lease and hardware unchanged
 * \note            CPU accessibility is not physical retention or timing
 *                  qualification. No heap pool or automatic SRAM test exists.
 */
nx_result_t nx_memory_window_port_acquire(const nx_memory_window_port_t* port,
                                          nx_memory_window_lease_t* lease);

/**
 * \brief           Release the original loan after all direct users joined
 * \param[in]       port: Exact same bank identity that granted the loan
 * \param[in,out]   lease: Original stable lease storage, never a copied token
 * \param[in]       epoch: Generation captured on the original acquisition
 * \return          SUCCESS or exact stale/foreign/release rejection; failed
 *                  release retains the original responsibility
 * \note            The provider enforces identity and orders prior CPU access
 *                  before later bank withdrawal. It cannot detect a caller
 *                  violating the join precondition by reusing a raw pointer.
 */
nx_result_t nx_memory_window_port_release(const nx_memory_window_port_t* port,
                                          nx_memory_window_lease_t* lease,
                                          uint64_t epoch);

/**
 * \brief           Read selected bank geometry without acquiring a loan
 * \param[in]       port: Stable selected face under its serialized owner.
 * \param[out]      geometry: Distinct aligned caller output, preserved on
 *                  input rejection; provider errors may leave it unchanged.
 * \return          SUCCESS with readonly facts, STATE before facts exist,
 *                  UNSUPPORTED for a missing method or exact provider failure.
 * \note            This performs no memory test or implied initialization.
 *                  startup_backing never grants an exclusive whole-bank loan.
 */
nx_result_t
nx_memory_window_port_geometry(const nx_memory_window_port_t* port,
                               nx_memory_window_geometry_t* geometry);

#ifdef __cplusplus
}
#endif
#endif
