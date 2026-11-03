/**
 * \file            flash_map_backend.h
 * \brief           MCUboot's ordinary Flash API over explicit Nexus regions.
 * \author          Nexus Team
 *
 * Backend-private integration contract, not a new generic image writer.
 * Only the reviewed F407 non-ECC 4-byte-program profile is in scope. The
 * product owns region storage, platform lifetime, clock and watchdog policy.
 */
#ifndef NEXUS_MCUBOOT_FLASH_MAP_BACKEND_H
#define NEXUS_MCUBOOT_FLASH_MAP_BACKEND_H

#include "nexus/io/flash.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NX_MCUBOOT_PRIMARY_AREA_ID   1U
#define NX_MCUBOOT_SECONDARY_AREA_ID 2U

/** \brief Immutable identity referring to the whole physical slot. */
struct flash_area {
    uint8_t id;
    const nx_flash_region_t* region;
};

/** \brief A returned physical sector, with an area-relative byte offset. */
struct flash_sector {
    uint32_t offset;
    uint32_t size;
};

/**
 * \brief One immutable maintenance call, explicit clock and finite deadline.
 * \note Keep this object, its areas, regions and provider alive and unchanged
 * for the complete upstream call, including nested opens and retries. The
 * clock callback/context outlive the call. The deadline belongs to this clock
 * and the Flash port's same live monotonic epoch, never renewing on retry.
 * There is no clock-domain translation or implicit global clock dependency.
 * The read callback is bounded; a missing callback rejects without access.
 * The caller serializes calls, joins them before platform stop and owns
 * Flash-stall, power and watchdog policy. No adapter task or manager exists.
 */
typedef struct {
    const struct flash_area* primary;
    const struct flash_area* secondary;
    nx_clock_t clock;
    nx_time_us_t deadline;
} nx_mcuboot_flash_operation_t;

/**
 * \brief Product hook returning the current explicitly owned immutable call.
 * \return Stable borrowed context, or NULL outside an authorized operation.
 * \note Required product definition; no default or Nexus-owned active state.
 */
const nx_mcuboot_flash_operation_t* nx_mcuboot_flash_operation(void);

/**
 * \brief Product-owned fatal assertion path; never returns to upstream code.
 * \note Required by the selected control and boot profiles. This hook owns
 * the product's fatal/reset policy; no default logger or allocator is linked.
 */
void nx_mcuboot_assert_fail(void);

/**
 * \brief Product-owned watchdog progress during a full boot copy operation.
 * \note Required only by the full boot profile. It obeys the same operation
 * lifetime and finite budget; it cannot renew the Flash deadline implicitly.
 */
void nx_mcuboot_watchdog_progress(void);

/** \brief Pure lookup and structural admission; never starts hardware. */
int flash_area_open(uint8_t id, const struct flash_area** area);
/** \brief No hardware withdrawal; the product owns the entire call lifetime. */
void flash_area_close(const struct flash_area* area);
/** \brief Region-relative read; destination is not retained after return. */
int flash_area_read(const struct flash_area* area, uint32_t offset, void* data,
                    uint32_t length);
/**
 * \brief Delegate aligned programming using the call's fixed finite deadline.
 * \note Return settles the provider's current pulse; TIMEOUT is not abort.
 * The input is not retained after return. No image prefix is added to offset.
 */
int flash_area_write(const struct flash_area* area, uint32_t offset,
                     const void* data, uint32_t length);
/**
 * \brief Delegate whole-sector erase with the same pulse-settlement contract.
 * \note These APIs return zero on success or a negative errno on failure.
 * Partial progress on an underlying error remains possible.
 */
int flash_area_erase(const struct flash_area* area, uint32_t offset,
                     uint32_t length);
/** \brief Read the exact provider program unit without hardware effects. */
uint32_t flash_area_align(const struct flash_area* area);
/** \brief First non-ECC F407 profile reads erased bytes as all ones. */
uint8_t flash_area_erased_val(const struct flash_area* area);
/** \brief Return the static area's identity without lookup side effects. */
uint8_t flash_area_get_id(const struct flash_area* area);
/** \brief The first profile uses one explicitly supplied physical device. */
uint8_t flash_area_get_device_id(const struct flash_area* area);
/** \brief Derive offset from the existing region, relative to full Flash. */
uint32_t flash_area_get_off(const struct flash_area* area);
/** \brief Derive whole-slot size; prefix/trailer bytes remain part of it. */
uint32_t flash_area_get_size(const struct flash_area* area);
/** \brief Query the full physical device base, never the slot's origin. */
int flash_device_base(uint8_t device_id, uintptr_t* base);
/**
 * \brief Enumerate exact sectors with area-relative offsets, without heap.
 * \note count supplies capacity and receives required count. Insufficient
 * capacity returns -ENOMEM and does not partially modify sector output.
 */
int flash_area_get_sectors(int area_id, uint32_t* count,
                           struct flash_sector* sectors);
/** \brief Query the exact sector containing a region-relative byte offset. */
int flash_area_get_sector(const struct flash_area* area, uint32_t offset,
                          struct flash_sector* sector);
/** \brief Return a sector's area-relative offset. */
uint32_t flash_sector_get_off(const struct flash_sector* sector);
/** \brief Return a sector's actual physical size. */
uint32_t flash_sector_get_size(const struct flash_sector* sector);
/** \brief Map only image zero and the primary/secondary roles; else -1. */
int flash_area_id_from_multi_image_slot(int image_index, int slot);
/** \brief Map image zero's two known area IDs to slots; else -1. */
int flash_area_id_to_multi_image_slot(int image_index, int area_id);
/** \brief Single-image shorthand, preserving invalid-slot rejection. */
int flash_area_id_from_image_slot(int slot);

#ifdef __cplusplus
}
#endif
#endif
