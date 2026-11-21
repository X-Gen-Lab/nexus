/**
 * \file            sfud.h
 * \brief           Selected SFUD NOR algorithms behind the existing SPI owner
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_SFUD_H
#define NEXUS_COMPONENTS_SFUD_H
#include "nexus/io/flash.h"
#include "nexus/io/spi.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Explicit suffix selection; JEDEC alone does not prove this model. */
typedef enum { NX_SFUD_W25Q128JV = 1 } nx_sfud_profile_t;
/** \brief Caller metadata; workspace contains all private upstream state. */
typedef struct {
    const nx_spi_endpoint_t* endpoint;
    nx_clock_t clock;
    void* state;
    nx_time_us_t ready_at;
    nx_time_us_t deadline;
    nx_time_us_t last_time;
    nx_result_t error;
    nx_flash_erase_span_t erase_span;
    nx_flash_geometry_t geometry;
    nx_flash_port_t port;
    bool initialized;
    bool entered;
    bool uncertain;
    bool faulted;
} nx_sfud_t;
/** \brief Exact workspace bytes for the selected static chip profile. */
size_t nx_sfud_workspace_bytes(void);
/** \brief Required workspace alignment, without allocation. */
size_t nx_sfud_workspace_alignment(void);
/**
 * \brief Admit one explicitly selected awake W25Q128JV using only read
 * commands. \param[out] device: Unused caller metadata; retained through proven
 * stop. \param[in] endpoint: Existing single-line mode 0/3 SPI owner, at most
 * 50 MHz. \param[in] clock: Ready monotonic clock in the same deadline domain.
 * \param[in] profile: Exact product-selected model, not autodetection policy.
 * \param[in] ready_at: Actual power-up/write readiness established externally.
 * \param[in,out] workspace: Disjoint aligned caller vendor-state storage.
 * \param[in] bytes: At least the exact reported requirement.
 * \param[in] deadline: Finite absolute probe deadline.
 * \return SUCCESS or error; no reset, status write, protection change or erase.
 * \note Calls selected upstream read/program/erase algorithms after explicit
 *       metadata admission, not SFUD's reset-capable device_init/discovery.
 *       No concurrent W25/SFUD owner may command this same physical chip.
 */
nx_result_t nx_sfud_init(nx_sfud_t* device, const nx_spi_endpoint_t* endpoint,
                         nx_clock_t clock, nx_sfud_profile_t profile,
                         nx_time_us_t ready_at, void* workspace, size_t bytes,
                         nx_time_us_t deadline);
/**
 * \brief Query the full physical 16 MiB array; products supply their own
 * region. \param[in] device: Initialized serialized owner, stable through all
 * users. \return Existing immutable face or NULL; lookup does not initialize
 * hardware. \note The current profile is non-ECC NOR, one-byte program and 4
 * KiB erase.
 */
const nx_flash_port_t* nx_sfud_flash(const nx_sfud_t* device);
/**
 * \brief Set the next explicit synchronous read budget without any bus access.
 * \param[in,out] device: Initialized idle instance, externally serialized.
 * \param[in] deadline: Finite provider-domain budget, shared with its consumer.
 * \return SUCCESS, BUSY, STATE, INVALID or TIMEOUT without hardware effects.
 * \note Set once before the whole FAL/DB/filesystem operation, never per read.
 *       No timeout renewal or hidden current-deadline variable is created.
 */
nx_result_t nx_sfud_set_read_deadline(nx_sfud_t* device, nx_time_us_t deadline);

/**
 * \brief Perform bounded cleanup without resetting or replaying failed writes.
 * \param[in,out] device: Same owner retained after timeout or uncertain
 * command. \param[in] cleanup_deadline: Fresh finite budget for at most four
 * probes. \return SUCCESS after WIP0/SUS0/WEL0, BUSY or a retained error
 * otherwise. \note Cleanup never upgrades a previous write result. Call before
 * the Flash face's non-I/O recover observation; preserve storage while BUSY.
 */
nx_result_t nx_sfud_service(nx_sfud_t* device, nx_time_us_t cleanup_deadline);
/**
 * \brief Unbind only after every user joined and the chip proved quiescent.
 * \param[in,out] device: Stable serialized state, live through refused stop.
 * \return SUCCESS or BUSY; does not stop/reset the existing SPI bus owner.
 */
nx_result_t nx_sfud_stop(nx_sfud_t* device);
#ifdef __cplusplus
}
#endif
#endif
