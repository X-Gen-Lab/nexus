/**
 * \file            w25.c
 * \brief           Single-line W25Q128JV bounded device protocol
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/w25.h"
#include <string.h>

#define W25_BUSY UINT8_C(0x01)
#define W25_WEL  UINT8_C(0x02)
#define W25_BP   UINT8_C(0x1c)
#define W25_SUS  UINT8_C(0x80)
#define W25_CMP  UINT8_C(0x40)
#define W25_WPS  UINT8_C(0x04)

/** \brief Validate numeric spans before reading known typed objects. */
static bool span(const void* pointer, size_t bytes, size_t alignment) {
    uintptr_t address = (uintptr_t)pointer;
    return pointer != NULL && address % alignment == 0U &&
           bytes <= UINTPTR_MAX - address;
}

/** \brief Opaque bus/clock context extents remain the caller's responsibility.
 */
static bool overlaps(const void* first, size_t first_bytes, const void* second,
                     size_t second_bytes) {
    uintptr_t a = (uintptr_t)first;
    uintptr_t b = (uintptr_t)second;
    return first_bytes > UINTPTR_MAX - a || second_bytes > UINTPTR_MAX - b ||
           (a < b + second_bytes && b < a + first_bytes);
}

/** \brief Known control and immutable dispatch storage cannot be payloads. */
static bool payload_valid(const nx_w25_t* device, const void* pointer,
                          size_t bytes) {
    if (!span(pointer, bytes, 1U) ||
        overlaps(pointer, bytes, device, sizeof(*device)))
        return false;
    if (device->endpoint != NULL &&
        (overlaps(pointer, bytes, device->endpoint,
                  sizeof(*device->endpoint)) ||
         overlaps(pointer, bytes, device->endpoint->ops,
                  sizeof(*device->endpoint->ops)) ||
         (device->endpoint->context != NULL &&
          overlaps(pointer, bytes, device->endpoint->context, 1U))))
        return false;
    if (device->clock.context != NULL &&
        overlaps(pointer, bytes, device->clock.context, 1U))
        return false;
    return true;
}

/** \brief A region is an explicit caller lease, never an inferred partition. */
static bool config_valid(const nx_w25_config_t* config) {
    return span(config, sizeof(*config), _Alignof(nx_w25_config_t)) &&
           config->profile == NX_W25_PROFILE_Q128JV &&
           config->ready_at != NX_DEADLINE_NEVER &&
           config->region_bytes != 0U &&
           config->region_bytes <= NX_W25_Q128JV_BYTES &&
           config->region_offset <=
               NX_W25_Q128JV_BYTES - config->region_bytes &&
           config->region_offset % NX_W25_SECTOR_BYTES == 0U &&
           config->region_bytes % NX_W25_SECTOR_BYTES == 0U;
}

/** \brief Clock observation never converts a cleanup budget into operation
 * time. */
static nx_result_t observe(nx_w25_t* device, nx_time_us_t deadline) {
    nx_time_us_t now = device->clock.read(device->clock.context);
    if (now == NX_DEADLINE_NEVER || now < device->last_clock) {
        device->clock_fault = true;
        if (device->pending && device->operation_result == NX_ERROR_BUSY)
            device->operation_result = NX_ERROR_IO;
        return NX_ERROR_IO;
    }
    device->last_clock = now;
    if (device->pending && device->operation_result == NX_ERROR_BUSY &&
        now >= device->operation_deadline)
        device->operation_result = NX_ERROR_TIMEOUT;
    return now >= deadline ? NX_ERROR_TIMEOUT : NX_SUCCESS;
}

/** \brief The polling face ends CPU access even when CS recovery is retained.
 */
static nx_result_t wire(nx_w25_t* device, const uint8_t* tx, uint8_t* rx,
                        size_t bytes, nx_time_us_t deadline, bool* attempted) {
    if (attempted != NULL)
        *attempted = false;
    nx_result_t result = observe(device, deadline);
    if (result != NX_SUCCESS)
        return result;
    size_t transferred = 0U;
    if (attempted != NULL)
        *attempted = true;
    result = nx_spi_endpoint_transfer(device->endpoint, tx, rx, bytes, deadline,
                                      &transferred);
    nx_result_t timing = observe(device, deadline);
    if (result != NX_SUCCESS)
        return result;
    if (transferred != bytes)
        return NX_ERROR_IO;
    return timing;
}

/** \brief Read one status register without status or protection writes. */
static nx_result_t read_status(nx_w25_t* device, uint8_t opcode, uint8_t* value,
                               nx_time_us_t deadline) {
    const uint8_t tx[2] = {opcode, 0xffU};
    uint8_t rx[2] = {0U, 0U};
    nx_result_t result = wire(device, tx, rx, sizeof(tx), deadline, NULL);
    if (result == NX_SUCCESS)
        *value = rx[1];
    return result;
}

/** \brief Serialize callbacks before observing any live peripheral state. */
static nx_result_t enter(nx_w25_t* device, bool allow_pending) {
    if (!span(device, sizeof(*device), _Alignof(nx_w25_t)))
        return NX_ERROR_INVALID;
    if (device->entered)
        return NX_ERROR_BUSY;
    if (!device->initialized)
        return NX_ERROR_STATE;
    if (!allow_pending && (device->pending || device->uncertain))
        return NX_ERROR_BUSY;
    if (device->clock_fault)
        return NX_ERROR_IO;
    device->entered = true;
    return NX_SUCCESS;
}

/** \brief Check region-relative spans before forming the 24-bit address. */
static bool range_valid(const nx_w25_t* device, uint32_t offset, size_t bytes) {
    return bytes != 0U && bytes <= device->config.region_bytes &&
           offset <= device->config.region_bytes - bytes;
}

/** \brief Observed foreign activity invalidates the previous idle assumption.
 */
static void retain_unknown(nx_w25_t* device, nx_time_us_t deadline) {
    device->operation = NX_W25_OPERATION_NONE;
    device->operation_offset = 0U;
    device->operation_bytes = 0U;
    device->operation_deadline = deadline;
    device->operation_result = NX_ERROR_STATE;
    device->pending = true;
    device->uncertain = true;
    device->may_have_modified = false;
}

/** \brief Reject protected or suspended operation instead of repairing the
 * chip. */
static nx_result_t writable(nx_w25_t* device, nx_time_us_t deadline) {
    uint8_t first, second, third;
    nx_result_t result = read_status(device, 0x05U, &first, deadline);
    if (result != NX_SUCCESS)
        return result;
    if ((first & (W25_BUSY | W25_WEL)) != 0U) {
        retain_unknown(device, deadline);
        return NX_ERROR_BUSY;
    }
    result = read_status(device, 0x35U, &second, deadline);
    if (result != NX_SUCCESS)
        return result;
    if ((second & W25_SUS) != 0U) {
        retain_unknown(device, deadline);
        return NX_ERROR_BUSY;
    }
    result = read_status(device, 0x15U, &third, deadline);
    if (result != NX_SUCCESS)
        return result;
    return ((first & W25_BP) | (second & W25_CMP) | (third & W25_WPS)) == 0U
               ? NX_SUCCESS
               : NX_ERROR_PERMISSION;
}

/** \brief WREN uncertainty retains the binding even before an array command. */
static nx_result_t begin(nx_w25_t* device, nx_w25_operation_t operation,
                         uint32_t offset, uint32_t bytes,
                         nx_time_us_t deadline) {
    nx_result_t result = writable(device, deadline);
    if (result != NX_SUCCESS)
        return result;
    device->operation = operation;
    device->operation_offset = offset;
    device->operation_bytes = bytes;
    device->operation_deadline = deadline;
    device->operation_result = NX_ERROR_BUSY;
    device->pending = true;
    device->uncertain = false;
    device->may_have_modified = false;
    const uint8_t enable = 0x06U;
    bool attempted;
    result = wire(device, &enable, NULL, 1U, deadline, &attempted);
    if (result != NX_SUCCESS) {
        device->pending = attempted;
        device->uncertain = attempted;
        if (device->operation_result == NX_ERROR_BUSY)
            device->operation_result = result;
        return result;
    }
    uint8_t status;
    result = read_status(device, 0x05U, &status, deadline);
    if (result != NX_SUCCESS) {
        device->uncertain = true;
        if (device->operation_result == NX_ERROR_BUSY)
            device->operation_result = result;
        return result;
    }
    if ((status & W25_BUSY) != 0U) {
        device->uncertain = true;
        device->operation_result = NX_ERROR_STATE;
        return NX_ERROR_BUSY;
    }
    if ((status & W25_WEL) == 0U) {
        device->pending = false;
        device->operation_result = NX_ERROR_PERMISSION;
        return NX_ERROR_PERMISSION;
    }
    return NX_SUCCESS;
}

/** \brief Address formation follows checked fixed geometry, never truncation.
 */
static void address_command(uint8_t* tx, uint8_t opcode, uint32_t address) {
    tx[0] = opcode;
    tx[1] = (uint8_t)(address >> 16U);
    tx[2] = (uint8_t)(address >> 8U);
    tx[3] = (uint8_t)address;
}

/** \brief A failed array transfer may have executed after CS withdrawal. */
static nx_result_t array_command(nx_w25_t* device, const uint8_t* tx,
                                 size_t bytes, nx_time_us_t deadline) {
    bool attempted;
    nx_result_t result = wire(device, tx, NULL, bytes, deadline, &attempted);
    device->may_have_modified = attempted;
    if (result != NX_SUCCESS) {
        device->uncertain = true;
        if (device->operation_result == NX_ERROR_BUSY)
            device->operation_result = result;
    }
    return result;
}

nx_result_t nx_w25_init(nx_w25_t* device, const nx_spi_endpoint_t* endpoint,
                        nx_clock_t clock, const nx_w25_config_t* config,
                        nx_time_us_t deadline) {
    if (!span(device, sizeof(*device), _Alignof(nx_w25_t)) ||
        !span(config, sizeof(*config), _Alignof(nx_w25_config_t)) ||
        !span(endpoint, sizeof(*endpoint), _Alignof(nx_spi_endpoint_t)) ||
        overlaps(device, sizeof(*device), config, sizeof(*config)) ||
        overlaps(device, sizeof(*device), endpoint, sizeof(*endpoint)) ||
        overlaps(config, sizeof(*config), endpoint, sizeof(*endpoint)) ||
        clock.read == NULL || deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    /* Reject known control aliases before interpreting either dispatch or
     * model fields through their different effective types. */
    if (!span(endpoint->ops, sizeof(*endpoint->ops),
              _Alignof(nx_spi_endpoint_ops_t)) ||
        overlaps(device, sizeof(*device), endpoint->ops,
                 sizeof(*endpoint->ops)) ||
        overlaps(config, sizeof(*config), endpoint->ops,
                 sizeof(*endpoint->ops)) ||
        overlaps(endpoint, sizeof(*endpoint), endpoint->ops,
                 sizeof(*endpoint->ops)) ||
        (endpoint->context != NULL &&
         overlaps(device, sizeof(*device), endpoint->context, 1U)) ||
        (clock.context != NULL &&
         overlaps(device, sizeof(*device), clock.context, 1U)))
        return NX_ERROR_INVALID;
    if (!config_valid(config) || endpoint->context == NULL ||
        endpoint->ops->transfer == NULL)
        return NX_ERROR_INVALID;
    if (device->entered || device->initialized || device->pending ||
        device->uncertain)
        return NX_ERROR_BUSY;
    device->entered = true;
    nx_w25_t candidate = {0};
    candidate.endpoint = endpoint;
    candidate.clock = clock;
    candidate.config = *config;
    nx_result_t result = observe(&candidate, deadline);
    if (result == NX_SUCCESS && candidate.last_clock < config->ready_at)
        result = NX_ERROR_BUSY;
    uint8_t status;
    if (result == NX_SUCCESS) {
        result = read_status(&candidate, 0x05U, &status, deadline);
        if (result == NX_SUCCESS && (status & (W25_BUSY | W25_WEL)) != 0U)
            result = NX_ERROR_BUSY;
    }
    if (result == NX_SUCCESS) {
        result = read_status(&candidate, 0x35U, &status, deadline);
        if (result == NX_SUCCESS && (status & W25_SUS) != 0U)
            result = NX_ERROR_BUSY;
    }
    if (result == NX_SUCCESS) {
        const uint8_t tx[4] = {0x9fU, 0xffU, 0xffU, 0xffU};
        uint8_t rx[4] = {0U};
        result = wire(&candidate, tx, rx, sizeof(tx), deadline, NULL);
        uint32_t id =
            ((uint32_t)rx[1] << 16U) | ((uint32_t)rx[2] << 8U) | rx[3];
        if (result == NX_SUCCESS && id != NX_W25_Q128JV_ID)
            result = NX_ERROR_UNSUPPORTED;
    }
    if (result == NX_SUCCESS)
        result = read_status(&candidate, 0x15U, &status, deadline);
    if (result == NX_SUCCESS) {
        candidate.initialized = true;
        *device = candidate;
    } else {
        device->entered = false;
    }
    return result;
}

nx_result_t nx_w25_read(nx_w25_t* device, uint32_t offset, uint8_t* data,
                        size_t bytes, nx_time_us_t deadline) {
    nx_result_t result = enter(device, false);
    if (result != NX_SUCCESS)
        return result;
    if (deadline == NX_DEADLINE_NEVER || bytes > NX_W25_MAX_CHUNK_BYTES ||
        !range_valid(device, offset, bytes) ||
        !payload_valid(device, data, bytes)) {
        device->entered = false;
        return NX_ERROR_INVALID;
    }
    uint8_t tx[NX_W25_PAGE_BYTES];
    uint8_t rx[NX_W25_PAGE_BYTES];
    memset(tx, 0xff, sizeof(tx));
    address_command(tx, 0x03U, device->config.region_offset + offset);
    result = wire(device, tx, rx, bytes + 4U, deadline, NULL);
    if (result == NX_SUCCESS)
        memcpy(data, &rx[4], bytes);
    device->entered = false;
    return result;
}

nx_result_t nx_w25_program_start(nx_w25_t* device, uint32_t offset,
                                 const uint8_t* data, size_t bytes,
                                 nx_time_us_t deadline) {
    nx_result_t result = enter(device, false);
    if (result != NX_SUCCESS)
        return result;
    if (deadline == NX_DEADLINE_NEVER || bytes > NX_W25_MAX_CHUNK_BYTES ||
        !range_valid(device, offset, bytes) ||
        !payload_valid(device, data, bytes) ||
        bytes > NX_W25_PAGE_BYTES - (offset % NX_W25_PAGE_BYTES)) {
        device->entered = false;
        return NX_ERROR_INVALID;
    }
    result = begin(device, NX_W25_OPERATION_PROGRAM, offset, (uint32_t)bytes,
                   deadline);
    if (result == NX_SUCCESS) {
        uint8_t tx[NX_W25_PAGE_BYTES];
        address_command(tx, 0x02U, device->config.region_offset + offset);
        memcpy(&tx[4], data, bytes);
        result = array_command(device, tx, bytes + 4U, deadline);
    }
    device->entered = false;
    return result;
}

nx_result_t nx_w25_erase_4k_start(nx_w25_t* device, uint32_t offset,
                                  nx_time_us_t deadline) {
    nx_result_t result = enter(device, false);
    if (result != NX_SUCCESS)
        return result;
    if (deadline == NX_DEADLINE_NEVER || offset % NX_W25_SECTOR_BYTES != 0U ||
        !range_valid(device, offset, NX_W25_SECTOR_BYTES)) {
        device->entered = false;
        return NX_ERROR_INVALID;
    }
    result = begin(device, NX_W25_OPERATION_ERASE_4K, offset,
                   NX_W25_SECTOR_BYTES, deadline);
    if (result == NX_SUCCESS) {
        uint8_t tx[4];
        address_command(tx, 0x20U, device->config.region_offset + offset);
        result = array_command(device, tx, sizeof(tx), deadline);
    }
    device->entered = false;
    return result;
}

nx_result_t nx_w25_service(nx_w25_t* device, nx_time_us_t cleanup_deadline) {
    nx_result_t result = enter(device, true);
    if (result != NX_SUCCESS)
        return result;
    if (cleanup_deadline == NX_DEADLINE_NEVER) {
        device->entered = false;
        return NX_ERROR_INVALID;
    }
    if (!device->pending && !device->uncertain) {
        device->entered = false;
        return NX_SUCCESS;
    }
    uint8_t status;
    result = read_status(device, 0x05U, &status, cleanup_deadline);
    if (result != NX_SUCCESS)
        goto unfinished;
    if ((status & W25_BUSY) != 0U) {
        result = NX_ERROR_BUSY;
        goto done;
    }
    uint8_t second;
    result = read_status(device, 0x35U, &second, cleanup_deadline);
    if (result != NX_SUCCESS)
        goto unfinished;
    if ((second & W25_SUS) != 0U) {
        if (device->operation_result == NX_ERROR_BUSY)
            device->operation_result = NX_ERROR_STATE;
        result = NX_ERROR_STATE;
        goto unfinished;
    }
    if ((status & W25_WEL) != 0U) {
        if (device->operation_result == NX_ERROR_BUSY)
            device->operation_result = NX_ERROR_IO;
        const uint8_t disable = 0x04U;
        result = wire(device, &disable, NULL, 1U, cleanup_deadline, NULL);
        if (result != NX_SUCCESS)
            goto unfinished;
        result = read_status(device, 0x05U, &status, cleanup_deadline);
        if (result != NX_SUCCESS)
            goto unfinished;
        if ((status & (W25_BUSY | W25_WEL)) != 0U) {
            result = NX_ERROR_IO;
            goto unfinished;
        }
    }
    device->pending = false;
    device->uncertain = false;
    if (device->operation_result == NX_ERROR_BUSY)
        device->operation_result = NX_SUCCESS;
    result = device->operation_result;
    goto done;
unfinished:
    device->uncertain = true;
done:
    device->entered = false;
    return result;
}

nx_result_t nx_w25_get_status(const nx_w25_t* device, nx_w25_status_t* status) {
    if (!span(device, sizeof(*device), _Alignof(nx_w25_t)) ||
        !span(status, sizeof(*status), _Alignof(nx_w25_status_t)) ||
        !payload_valid(device, status, sizeof(*status)))
        return NX_ERROR_INVALID;
    if (device->entered)
        return NX_ERROR_BUSY;
    *status =
        (nx_w25_status_t){device->operation,        device->operation_result,
                          device->operation_offset, device->operation_bytes,
                          device->initialized,      device->pending,
                          device->uncertain,        device->may_have_modified,
                          device->clock_fault};
    return NX_SUCCESS;
}

nx_result_t nx_w25_stop(nx_w25_t* device) {
    if (!span(device, sizeof(*device), _Alignof(nx_w25_t)))
        return NX_ERROR_INVALID;
    if (device->entered || device->pending || device->uncertain)
        return NX_ERROR_BUSY;
    memset(device, 0, sizeof(*device));
    return NX_SUCCESS;
}
