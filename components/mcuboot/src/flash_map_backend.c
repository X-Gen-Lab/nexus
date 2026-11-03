/**
 * \file            flash_map_backend.c
 * \brief           Ordinary MCUboot access through existing Nexus regions.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "flash_map_backend/flash_map_backend.h"
#include <errno.h>

/** \brief Temporary view of existing facts; never a retained descriptor. */
typedef struct {
    const nx_flash_geometry_t* geometry;
    size_t first;
    uint32_t count;
} region_view_t;

/** \brief Preserve failure classes across the ordinary upstream errno ABI. */
static int result_code(nx_result_t result) {
    switch (result) {
        case NX_SUCCESS:
            return 0;
        case NX_ERROR_INVALID:
        case NX_ERROR_OVERFLOW:
            return -EINVAL;
        case NX_ERROR_BUSY:
            return -EBUSY;
        case NX_ERROR_TIMEOUT:
            return -ETIMEDOUT;
        case NX_ERROR_PERMISSION:
            return -EACCES;
        case NX_ERROR_STATE:
        case NX_ERROR_CONTEXT:
            return -EPERM;
        case NX_ERROR_UNSUPPORTED:
            return -ENOTSUP;
        default:
            return -EIO;
    }
}

/** \brief Validate the whole erase owner against the one physical geometry. */
static int region_view(const nx_flash_region_t* region, region_view_t* view) {
    nx_result_t result = nx_flash_region_validate(region);
    if (result != NX_SUCCESS) {
        return result_code(result);
    }
    const nx_flash_geometry_t* geometry = nx_flash_port_geometry(region->port);
    if (geometry->program_unit != 4U || geometry->program_flags != 0U) {
        return -ENOTSUP;
    }
    if (geometry->sectors == NULL || geometry->sector_count == 0U) {
        return -EINVAL;
    }
    const uint32_t finish = region->offset + region->size;
    uint32_t cursor = 0U;
    uint32_t count = 0U;
    size_t first = 0U;
    bool started = false;
    bool ended = false;
    for (size_t index = 0U; index < geometry->sector_count; ++index) {
        const nx_flash_sector_t* sector = &geometry->sectors[index];
        if (sector->offset != cursor || sector->size == 0U ||
            sector->size > geometry->size - cursor) {
            return -EINVAL;
        }
        if (cursor == region->offset) {
            started = true;
            first = index;
        }
        if (started && !ended) {
            ++count;
        }
        cursor += sector->size;
        if (cursor == finish) {
            ended = true;
        }
    }
    if (cursor != geometry->size || !started || !ended || count == 0U) {
        return -EINVAL;
    }
    if (view != NULL) {
        *view = (region_view_t){geometry, first, count};
    }
    return 0;
}

/** \brief Borrow the product's fixed operation without acquiring hardware. */
static int current_operation(const nx_mcuboot_flash_operation_t** selected) {
    const nx_mcuboot_flash_operation_t* operation =
        nx_mcuboot_flash_operation();
    if (operation == NULL) {
        return -EPERM;
    }
    if (operation->primary == NULL || operation->secondary == NULL ||
        operation->primary == operation->secondary ||
        operation->clock.read == NULL ||
        operation->primary->id != NX_MCUBOOT_PRIMARY_AREA_ID ||
        operation->secondary->id != NX_MCUBOOT_SECONDARY_AREA_ID ||
        operation->deadline == NX_DEADLINE_NEVER) {
        return -EINVAL;
    }
    int result = region_view(operation->primary->region, NULL);
    if (result != 0) {
        return result;
    }
    result = region_view(operation->secondary->region, NULL);
    if (result != 0) {
        return result;
    }
    const nx_flash_region_t* primary = operation->primary->region;
    const nx_flash_region_t* secondary = operation->secondary->region;
    if (primary->port != secondary->port ||
        (primary->offset < secondary->offset + secondary->size &&
         secondary->offset < primary->offset + primary->size)) {
        return -EINVAL;
    }
    *selected = operation;
    return 0;
}

/** \brief Accept only the exact objects borrowed by this operation. */
static int selected_area(const struct flash_area* area,
                         const nx_mcuboot_flash_operation_t** selected) {
    int result = current_operation(selected);
    if (result != 0) {
        return result;
    }
    return area == (*selected)->primary || area == (*selected)->secondary
               ? 0
               : -EINVAL;
}

/** \brief Expiry prevents new work and never renews the caller's deadline. */
static int check_deadline(const nx_mcuboot_flash_operation_t* operation) {
    nx_time_us_t now = operation->clock.read(operation->clock.context);
    return nx_deadline_expired(operation->deadline, now) ? -ETIMEDOUT : 0;
}

int flash_area_open(uint8_t id, const struct flash_area** area) {
    if (area == NULL) {
        return -EINVAL;
    }
    *area = NULL;
    const nx_mcuboot_flash_operation_t* operation = NULL;
    int result = current_operation(&operation);
    if (result != 0) {
        return result;
    }
    if (id == NX_MCUBOOT_PRIMARY_AREA_ID) {
        *area = operation->primary;
    } else if (id == NX_MCUBOOT_SECONDARY_AREA_ID) {
        *area = operation->secondary;
    } else {
        return -EINVAL;
    }
    return 0;
}

void flash_area_close(const struct flash_area* area) {
    /* The product still owns the entire provider epoch, including nested opens.
     */
    (void)area;
}

int flash_area_read(const struct flash_area* area, uint32_t offset, void* data,
                    uint32_t length) {
    const nx_mcuboot_flash_operation_t* operation = NULL;
    int result = selected_area(area, &operation);
    if (result == 0) {
        result = check_deadline(operation);
    }
    return result != 0 ? result
                       : result_code(nx_flash_region_read(area->region, offset,
                                                          data, length));
}

int flash_area_write(const struct flash_area* area, uint32_t offset,
                     const void* data, uint32_t length) {
    const nx_mcuboot_flash_operation_t* operation = NULL;
    int result = selected_area(area, &operation);
    if (result == 0) {
        result = check_deadline(operation);
    }
    return result != 0
               ? result
               : result_code(nx_flash_region_program(
                     area->region, offset, data, length, operation->deadline));
}

int flash_area_erase(const struct flash_area* area, uint32_t offset,
                     uint32_t length) {
    const nx_mcuboot_flash_operation_t* operation = NULL;
    int result = selected_area(area, &operation);
    if (result == 0) {
        result = check_deadline(operation);
    }
    return result != 0
               ? result
               : result_code(nx_flash_region_erase(area->region, offset, length,
                                                   operation->deadline));
}

uint32_t flash_area_align(const struct flash_area* area) {
    const nx_flash_geometry_t* geometry =
        area != NULL && area->region != NULL
            ? nx_flash_port_geometry(area->region->port)
            : NULL;
    return geometry != NULL ? geometry->program_unit : 0U;
}

uint8_t flash_area_erased_val(const struct flash_area* area) {
    (void)area;
    return UINT8_MAX;
}

uint8_t flash_area_get_id(const struct flash_area* area) {
    return area != NULL ? area->id : 0U;
}

uint8_t flash_area_get_device_id(const struct flash_area* area) {
    (void)area;
    return 0U;
}

uint32_t flash_area_get_off(const struct flash_area* area) {
    return area != NULL && area->region != NULL ? area->region->offset : 0U;
}

uint32_t flash_area_get_size(const struct flash_area* area) {
    return area != NULL && area->region != NULL ? area->region->size : 0U;
}

int flash_device_base(uint8_t device_id, uintptr_t* base) {
    if (base == NULL || device_id != 0U) {
        return -EINVAL;
    }
    const nx_mcuboot_flash_operation_t* operation = NULL;
    int result = current_operation(&operation);
    if (result != 0) {
        return result;
    }
    const nx_flash_geometry_t* geometry =
        nx_flash_port_geometry(operation->primary->region->port);
    *base = geometry->base_address;
    return 0;
}

int flash_area_get_sectors(int area_id, uint32_t* count,
                           struct flash_sector* sectors) {
    if (count == NULL || area_id < 0 || area_id > UINT8_MAX) {
        return -EINVAL;
    }
    const struct flash_area* area = NULL;
    int result = flash_area_open((uint8_t)area_id, &area);
    region_view_t view;
    if (result == 0) {
        result = region_view(area->region, &view);
    }
    if (result != 0) {
        return result;
    }
    const uint32_t capacity = *count;
    *count = view.count;
    if (capacity < view.count) {
        return -ENOMEM;
    }
    if (sectors == NULL) {
        return -EINVAL;
    }
    for (uint32_t index = 0U; index < view.count; ++index) {
        const nx_flash_sector_t* sector =
            &view.geometry->sectors[view.first + index];
        sectors[index] = (struct flash_sector){
            sector->offset - area->region->offset, sector->size};
    }
    return 0;
}

int flash_area_get_sector(const struct flash_area* area, uint32_t offset,
                          struct flash_sector* sector) {
    if (sector == NULL) {
        return -EINVAL;
    }
    const nx_mcuboot_flash_operation_t* operation = NULL;
    int result = selected_area(area, &operation);
    region_view_t view;
    if (result == 0) {
        result = region_view(area->region, &view);
    }
    if (result != 0) {
        return result;
    }
    if (offset >= area->region->size) {
        return -EINVAL;
    }
    const uint32_t physical = area->region->offset + offset;
    for (uint32_t index = 0U; index < view.count; ++index) {
        const nx_flash_sector_t* current =
            &view.geometry->sectors[view.first + index];
        if (physical >= current->offset &&
            physical - current->offset < current->size) {
            *sector = (struct flash_sector){
                current->offset - area->region->offset, current->size};
            return 0;
        }
    }
    return -EINVAL;
}

uint32_t flash_sector_get_off(const struct flash_sector* sector) {
    return sector != NULL ? sector->offset : 0U;
}

uint32_t flash_sector_get_size(const struct flash_sector* sector) {
    return sector != NULL ? sector->size : 0U;
}

int flash_area_id_from_multi_image_slot(int image_index, int slot) {
    if (image_index != 0) {
        return -1;
    }
    return slot == 0   ? (int)NX_MCUBOOT_PRIMARY_AREA_ID
           : slot == 1 ? (int)NX_MCUBOOT_SECONDARY_AREA_ID
                       : -1;
}

int flash_area_id_to_multi_image_slot(int image_index, int area_id) {
    if (image_index != 0) {
        return -1;
    }
    return area_id == (int)NX_MCUBOOT_PRIMARY_AREA_ID     ? 0
           : area_id == (int)NX_MCUBOOT_SECONDARY_AREA_ID ? 1
                                                          : -1;
}

int flash_area_id_from_image_slot(int slot) {
    return flash_area_id_from_multi_image_slot(0, slot);
}
