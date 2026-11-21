/**
 * \file            sfud.c
 * \brief           Real selected SFUD algorithms without reset/discovery policy
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/sfud.h"
#include "sfud_port.h"
#include <string.h>

/** \brief Exact model metadata is admitted by read-only probe, not discovery.
 */
typedef struct {
    sfud_flash flash;
} sfud_state_t;

/** \brief Settle only this caller's issued array pulse within its original
 * budget.
 */
static sfud_err wait_array(nx_sfud_t* device, nx_time_us_t interval);

/** \brief Gate every vendor exchange on one finite monotonic operation budget.
 */
static nx_result_t budget(nx_sfud_t* device) {
    if (device->error != NX_SUCCESS) {
        return device->error;
    }
    nx_time_us_t now = device->clock.read(device->clock.context);
    if (now == NX_DEADLINE_NEVER || now < device->last_time) {
        device->error = NX_ERROR_STATE;
    } else if (nx_deadline_expired(device->deadline, now)) {
        device->error = NX_ERROR_TIMEOUT;
    }
    device->last_time = now;
    return device->error;
}

/** \brief Frame one settled CS interval, rejecting short transfers completely.
 */
static sfud_err transfer_once(const sfud_spi* spi, const uint8_t* write,
                              size_t write_bytes, uint8_t* read,
                              size_t read_bytes) {
    nx_sfud_t* device = spi->user_data;
    if (device == NULL || write == NULL || write_bytes == 0U ||
        write_bytes > 256U || read_bytes > 256U - write_bytes ||
        (read == NULL && read_bytes != 0U) || !device->entered ||
        budget(device) != NX_SUCCESS) {
        return SFUD_ERR_TIMEOUT;
    }
    uint8_t command = write[0];
    if (command != 0x03U && command != 0x02U && command != 0x20U &&
        command != 0x04U && command != 0x05U && command != 0x06U &&
        command != 0x35U && command != 0x15U && command != 0x9fU) {
        device->error = NX_ERROR_UNSUPPORTED;
        return SFUD_ERR_WRITE;
    }
    uint8_t transmit[256];
    uint8_t receive[256];
    size_t bytes = write_bytes + read_bytes;
    memset(transmit, 0xff, bytes);
    memcpy(transmit, write, write_bytes);
    if (command == 0x06U || command == 0x02U || command == 0x20U) {
        device->uncertain = true;
    }
    size_t transferred = 0U;
    nx_result_t result = nx_spi_endpoint_transfer(
        device->endpoint, transmit, read_bytes != 0U ? receive : NULL, bytes,
        device->deadline, &transferred);
    if (result != NX_SUCCESS || transferred != bytes) {
        device->error = result != NX_SUCCESS ? result : NX_ERROR_IO;
        return read_bytes != 0U ? SFUD_ERR_READ : SFUD_ERR_WRITE;
    }
    if (budget(device) != NX_SUCCESS) {
        return SFUD_ERR_TIMEOUT;
    }
    if (read_bytes != 0U) {
        memcpy(read, receive + write_bytes, read_bytes);
    }
    return SFUD_SUCCESS;
}

/** \brief Finish a mutating pulse before upstream's fixed-count busy loop.
 * \note Framing storage and CS are released before entering status polling.
 */
static sfud_err exchange(const sfud_spi* spi, const uint8_t* write,
                         size_t write_bytes, uint8_t* read, size_t read_bytes) {
    sfud_err result = transfer_once(spi, write, write_bytes, read, read_bytes);
    if (result == SFUD_SUCCESS && (write[0] == 0x02U || write[0] == 0x20U)) {
        return wait_array(spi->user_data, write[0] == 0x02U ? 100U : 1000U);
    }
    return result;
}

/** \brief Unselected upstream discovery cannot initialize any hardware here. */
sfud_err sfud_spi_port_init(sfud_flash* flash) {
    (void)flash;
    return SFUD_ERR_NOT_FOUND;
}

/** \brief Report actual caller-owned private vendor metadata bytes. */
size_t nx_sfud_workspace_bytes(void) {
    return sizeof(sfud_state_t);
}

/** \brief No public vendor structure is required for caller allocation. */
size_t nx_sfud_workspace_alignment(void) {
    return _Alignof(sfud_state_t);
}

/** \brief Read one status register through the same bounded SPI callback. */
static nx_result_t status(nx_sfud_t* device, uint8_t command, uint8_t* value) {
    sfud_state_t* state = device->state;
    sfud_err result = transfer_once(&state->flash.spi, &command, 1U, value, 1U);
    return result == SFUD_SUCCESS ? NX_SUCCESS : device->error;
}

/** \brief Space finite status probes without holding CS or inventing OS state.
 * \note The selected microsecond clock must advance; a stalled source has a
 *       finite consecutive-read guard, independent of the operation deadline.
 */
static sfud_err wait_array(nx_sfud_t* device, nx_time_us_t interval) {
    for (;;) {
        uint8_t value = 0U;
        if (status(device, 0x05U, &value) != NX_SUCCESS) {
            return SFUD_ERR_TIMEOUT;
        }
        if ((value & 1U) == 0U) {
            return SFUD_SUCCESS;
        }
        nx_time_us_t remaining = device->deadline - device->last_time;
        nx_time_us_t next =
            device->last_time + (remaining < interval ? remaining : interval);
        nx_time_us_t previous = device->last_time;
        uint32_t stagnant = 0U;
        while (device->last_time < next) {
            if (budget(device) != NX_SUCCESS) {
                return SFUD_ERR_TIMEOUT;
            }
            stagnant = device->last_time == previous ? stagnant + 1U : 0U;
            if (stagnant == UINT32_C(1048576)) {
                device->error = NX_ERROR_STATE;
                return SFUD_ERR_TIMEOUT;
            }
            previous = device->last_time;
        }
    }
}

/** \brief Busy, suspended or protected chips never receive a mutating command.
 */
static nx_result_t writable(nx_sfud_t* device) {
    uint8_t first = 0U;
    uint8_t second = 0U;
    uint8_t third = 0U;
    nx_result_t result = status(device, 0x05U, &first);
    if (result == NX_SUCCESS) {
        result = status(device, 0x35U, &second);
    }
    if (result == NX_SUCCESS) {
        result = status(device, 0x15U, &third);
    }
    if (result != NX_SUCCESS) {
        return result;
    }
    if ((first & 3U) != 0U || (second & 0x80U) != 0U) {
        device->uncertain = true;
        return NX_ERROR_BUSY;
    }
    return (first & 0x1cU) != 0U || (second & 0x40U) != 0U ||
                   (third & 0x04U) != 0U
               ? NX_ERROR_PERMISSION
               : NX_SUCCESS;
}

/** \brief One execution scope excludes reentrancy and uncertain array duties.
 */
static nx_result_t begin(nx_sfud_t* device, nx_time_us_t deadline) {
    if (device == NULL || !device->initialized || device->state == NULL ||
        deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (device->entered) {
        return NX_ERROR_BUSY;
    }
    if (device->faulted || device->uncertain) {
        return NX_ERROR_STATE;
    }
    nx_time_us_t now = device->clock.read(device->clock.context);
    if (now == NX_DEADLINE_NEVER || now < device->last_time) {
        return NX_ERROR_STATE;
    }
    if (nx_deadline_expired(deadline, now)) {
        return NX_ERROR_TIMEOUT;
    }
    device->deadline = deadline;
    device->last_time = now;
    device->error = NX_SUCCESS;
    device->entered = true;
    return NX_SUCCESS;
}

/** \brief Translate upstream errors without forgetting possibly active pulses.
 */
static nx_result_t finish(nx_sfud_t* device, sfud_err upstream) {
    nx_result_t result = device->error;
    if (result == NX_SUCCESS && upstream != SFUD_SUCCESS) {
        result = upstream == SFUD_ERR_TIMEOUT ? NX_ERROR_TIMEOUT : NX_ERROR_IO;
    }
    if (result == NX_SUCCESS) {
        result = budget(device);
    }
    device->entered = false;
    if (result == NX_SUCCESS) {
        device->uncertain = false;
    } else if (device->uncertain) {
        device->faulted = true;
    }
    return result;
}

/** \brief Preserve the complete physical array geometry, never a product slot.
 */
static const nx_flash_geometry_t* describe(const void* context) {
    const nx_sfud_t* device = context;
    return device->initialized ? &device->geometry : NULL;
}

/** \brief Read chunks stay inside the endpoint's admitted 256-byte CS extent.
 */
static nx_result_t read_bytes(const void* context, uint32_t offset, void* data,
                              size_t bytes) {
    nx_sfud_t* device = (nx_sfud_t*)context;
    if (!device->initialized || offset > device->geometry.size ||
        bytes > device->geometry.size - offset ||
        (data == NULL && bytes != 0U)) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = begin(device, device->deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    sfud_state_t* state = device->state;
    sfud_err upstream = SFUD_SUCCESS;
    for (size_t completed = 0U; completed < bytes;) {
        size_t chunk = bytes - completed;
        if (chunk > 252U) {
            chunk = 252U;
        }
        upstream = sfud_read(&state->flash, offset + (uint32_t)completed, chunk,
                             (uint8_t*)data + completed);
        if (upstream != SFUD_SUCCESS) {
            break;
        }
        completed += chunk;
    }
    return finish(device, upstream);
}

/** \brief Real SFUD page splitting runs only after read-only protection checks.
 */
static nx_result_t program_bytes(void* context, uint32_t offset,
                                 const void* data, size_t bytes,
                                 nx_time_us_t deadline) {
    nx_sfud_t* device = context;
    if (!device->initialized || data == NULL || bytes == 0U ||
        offset > device->geometry.size ||
        bytes > device->geometry.size - offset) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = begin(device, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    result = writable(device);
    if (result != NX_SUCCESS) {
        device->entered = false;
        return result;
    }
    sfud_state_t* state = device->state;
    sfud_err upstream = SFUD_SUCCESS;
    const uint8_t* input = data;
    for (size_t completed = 0U; completed < bytes;) {
        if (input[completed] == UINT8_C(0xff)) {
            ++completed;
            continue;
        }
        size_t chunk = 1U;
        while (chunk < 252U && chunk < bytes - completed &&
               input[completed + chunk] != UINT8_C(0xff)) {
            ++chunk;
        }
        upstream = sfud_write(&state->flash, offset + (uint32_t)completed,
                              chunk, (const uint8_t*)data + completed);
        if (upstream != SFUD_SUCCESS) {
            break;
        }
        completed += chunk;
    }
    return finish(device, upstream);
}

/** \brief Provider entry rejects partial sectors before issuing any command. */
static nx_result_t erase_bytes(void* context, uint32_t offset, size_t bytes,
                               nx_time_us_t deadline) {
    nx_sfud_t* device = context;
    if (!device->initialized || bytes == 0U || offset % 4096U != 0U ||
        bytes % 4096U != 0U || offset > device->geometry.size ||
        bytes > device->geometry.size - offset) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = begin(device, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    result = writable(device);
    if (result != NX_SUCCESS) {
        device->entered = false;
        return result;
    }
    sfud_state_t* state = device->state;
    sfud_err upstream = SFUD_SUCCESS;
    for (size_t completed = 0U; completed < bytes; completed += 4096U) {
        upstream =
            sfud_erase(&state->flash, offset + (uint32_t)completed, 4096U);
        if (upstream != SFUD_SUCCESS) {
            break;
        }
    }
    return finish(device, upstream);
}

/** \brief This face never invents a cleanup deadline or performs hidden I/O. */
static nx_result_t recover(void* context) {
    const nx_sfud_t* device = context;
    return device->uncertain || device->entered ? NX_ERROR_BUSY : NX_SUCCESS;
}

static const nx_flash_ops_t flash_ops = {describe, read_bytes, program_bytes,
                                         erase_bytes, recover};

/** \brief Explicit model admission never calls upstream reset-capable init. */
nx_result_t nx_sfud_init(nx_sfud_t* device, const nx_spi_endpoint_t* endpoint,
                         nx_clock_t clock, nx_sfud_profile_t profile,
                         nx_time_us_t ready_at, void* workspace, size_t bytes,
                         nx_time_us_t deadline) {
    if (device == NULL || endpoint == NULL || endpoint->ops == NULL ||
        endpoint->ops->transfer == NULL || clock.read == NULL ||
        workspace == NULL || bytes < sizeof(sfud_state_t) ||
        (uintptr_t)workspace % _Alignof(sfud_state_t) != 0U ||
        deadline == NX_DEADLINE_NEVER || ready_at == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (profile != NX_SFUD_W25Q128JV) {
        return NX_ERROR_UNSUPPORTED;
    }
    nx_time_us_t now = clock.read(clock.context);
    if (now == NX_DEADLINE_NEVER) {
        return NX_ERROR_STATE;
    }
    if (now < ready_at) {
        return NX_ERROR_BUSY;
    }
    if (nx_deadline_expired(deadline, now)) {
        return NX_ERROR_TIMEOUT;
    }
    nx_sfud_t initialized = {0};
    initialized.endpoint = endpoint;
    initialized.clock = clock;
    initialized.state = workspace;
    initialized.deadline = deadline;
    initialized.last_time = now;
    initialized.ready_at = ready_at;
    initialized.entered = true;
    *device = initialized;
    sfud_state_t* state = workspace;
    memset(state, 0, sizeof(*state));
    state->flash.spi.wr = exchange;
    state->flash.spi.user_data = device;
    state->flash.retry.times = 32U;
    state->flash.chip.mf_id = 0xefU;
    state->flash.chip.type_id = 0x40U;
    state->flash.chip.capacity_id = 0x18U;
    state->flash.chip.capacity = UINT32_C(16777216);
    state->flash.chip.write_mode = SFUD_WM_PAGE_256B;
    state->flash.chip.erase_gran = 4096U;
    state->flash.chip.erase_gran_cmd = 0x20U;
    uint8_t command = 0x9fU;
    uint8_t identity[3];
    sfud_err probed = exchange(&state->flash.spi, &command, 1U, identity, 3U);
    nx_result_t result = probed == SFUD_SUCCESS ? NX_SUCCESS : device->error;
    if (result == NX_SUCCESS && (identity[0] != 0xefU || identity[1] != 0x40U ||
                                 identity[2] != 0x18U)) {
        result = NX_ERROR_UNSUPPORTED;
    }
    if (result == NX_SUCCESS) {
        result = writable(device);
    }
    if (result != NX_SUCCESS) {
        device->entered = false;
        return result;
    }
    device->erase_span = (nx_flash_erase_span_t){0U, 4096U, 4096U};
    device->geometry = (nx_flash_geometry_t){
        0U, UINT32_C(16777216), 1U, &device->erase_span, 1U, 0U};
    device->port = (nx_flash_port_t){&flash_ops, device};
    /* Do not publish a face using only the final exchange's earlier snapshot.
     */
    result = budget(device);
    device->entered = false;
    if (result != NX_SUCCESS) {
        return result;
    }
    state->flash.init_ok = true;
    device->initialized = true;
    return NX_SUCCESS;
}

/** \brief Lookup neither starts hardware nor acquires a second physical owner.
 */
const nx_flash_port_t* nx_sfud_flash(const nx_sfud_t* device) {
    return device != NULL && device->initialized ? &device->port : NULL;
}

/** \brief Products set a read budget once, before their complete consumer call.
 */
nx_result_t nx_sfud_set_read_deadline(nx_sfud_t* device,
                                      nx_time_us_t deadline) {
    nx_result_t result = begin(device, deadline);
    if (result == NX_SUCCESS) {
        device->entered = false;
    }
    return result;
}

/** \brief At most four probes prove idle without reset, replay or pulse wait.
 */
nx_result_t nx_sfud_service(nx_sfud_t* device, nx_time_us_t cleanup_deadline) {
    if (device == NULL || device->state == NULL || device->clock.read == NULL ||
        cleanup_deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (device->entered) {
        return NX_ERROR_BUSY;
    }
    nx_time_us_t now = device->clock.read(device->clock.context);
    if (now == NX_DEADLINE_NEVER || now < device->last_time) {
        return NX_ERROR_STATE;
    }
    if (nx_deadline_expired(cleanup_deadline, now)) {
        return NX_ERROR_TIMEOUT;
    }
    device->entered = true;
    device->error = NX_SUCCESS;
    device->deadline = cleanup_deadline;
    device->last_time = now;
    uint8_t first = 0U;
    uint8_t second = 0U;
    nx_result_t result = status(device, 0x05U, &first);
    if (result == NX_SUCCESS) {
        result = status(device, 0x35U, &second);
    }
    if (result == NX_SUCCESS &&
        ((first & 1U) != 0U || (second & 0x80U) != 0U)) {
        result = NX_ERROR_BUSY;
    }
    if (result == NX_SUCCESS && (first & 2U) != 0U) {
        sfud_state_t* state = device->state;
        uint8_t command = 0x04U;
        if (exchange(&state->flash.spi, &command, 1U, NULL, 0U) !=
            SFUD_SUCCESS) {
            result = device->error;
        } else {
            result = status(device, 0x05U, &first);
            if (result == NX_SUCCESS && (first & 3U) != 0U) {
                result = NX_ERROR_BUSY;
            }
        }
    }
    if (result == NX_SUCCESS) {
        result = budget(device);
    }
    device->entered = false;
    if (result == NX_SUCCESS) {
        device->uncertain = false;
        device->faulted = false;
    }
    return result;
}

/** \brief Failed or busy array duties keep caller metadata and endpoint alive.
 */
nx_result_t nx_sfud_stop(nx_sfud_t* device) {
    if (device == NULL) {
        return NX_ERROR_INVALID;
    }
    if (device->entered || device->uncertain) {
        return NX_ERROR_BUSY;
    }
    device->initialized = false;
    return NX_SUCCESS;
}
