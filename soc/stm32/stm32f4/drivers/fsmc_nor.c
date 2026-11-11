/**
 * \file            fsmc_nor.c
 * \brief           Finite NOR command pulses retaining the unique FSMC lease
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_fsmc_io.h"
#include "stm32f407_fsmc_nor.h"

/** \brief Use official LL for bus timing; HAL NOR's process-global width and
 *         HAL_GetTick waits cannot provide independent retained Core borrows.
 *         This explicit command state is that SDK gap, not a second bus owner.
 */
static void command(nx_stm32_fsmc_nor_t* n, uint32_t offset, uint16_t value) {
    nx_fsmc_write(n->lease.address + offset, value, n->width);
}
static uint16_t read_word(const nx_stm32_fsmc_nor_t* n, uint32_t offset) {
    return nx_fsmc_read(n->lease.address + offset, n->width);
}
static void read_array(nx_stm32_fsmc_nor_t* n) {
    command(n, 0U, n->config->command_set == 1U ? 0xffU : 0xf0U);
}
static bool state_valid(const nx_stm32_fsmc_nor_t* n) {
    return nx_fsmc_span(n, sizeof(*n), _Alignof(nx_stm32_fsmc_nor_t), true);
}
/** \brief Payloads cannot overwrite any owner, immutable fact or receipt. */
static bool storage(const nx_stm32_fsmc_nor_t* n, const void* p, size_t bytes,
                    bool writable) {
    return nx_fsmc_span(p, bytes, 1U, writable) &&
           !nx_fsmc_overlap(p, bytes, n, sizeof(*n)) &&
           !nx_fsmc_overlap(p, bytes, n->config, sizeof(*n->config)) &&
           !nx_fsmc_overlap(p, bytes, n->config->geometry,
                            sizeof(*n->config->geometry)) &&
           !nx_fsmc_overlap(p, bytes, n->config->geometry->sectors,
                            n->config->geometry->sector_count *
                                sizeof(nx_flash_sector_t)) &&
           nx_stm32_fsmc_storage(n->lease.owner, p, bytes, writable);
}
static nx_result_t clock_now(nx_stm32_fsmc_nor_t* n, nx_time_us_t* now) {
    const nx_stm32_fsmc_t* bus = n->lease.owner;
    *now = bus->config->clock.read(bus->config->clock.context);
    if (*now == NX_DEADLINE_NEVER || *now < n->last_clock)
        return NX_ERROR_STATE;
    n->last_clock = *now;
    return NX_SUCCESS;
}
static bool geometry_valid(const nx_flash_geometry_t* g, uint8_t width,
                           uintptr_t address, size_t bytes) {
    if (!nx_fsmc_span(g, sizeof(*g), _Alignof(nx_flash_geometry_t), false) ||
        g->base_address != address || g->size == 0U || g->size > bytes ||
        g->program_unit != width / 8U || g->sector_count == 0U ||
        g->sector_count > 1024U ||
        !nx_fsmc_span(g->sectors, g->sector_count * sizeof(*g->sectors),
                      _Alignof(nx_flash_sector_t), false) ||
        (g->program_flags & ~(uint32_t)NX_FLASH_PROGRAM_REQUIRES_FRESH_ERASE) !=
            0U)
        return false;
    uint32_t end = 0U;
    for (size_t i = 0U; i < g->sector_count; ++i) {
        const nx_flash_sector_t* sector = &g->sectors[i];
        if (sector->offset != end || sector->size == 0U ||
            sector->size > g->size - end ||
            sector->size % g->program_unit != 0U)
            return false;
        end += sector->size;
    }
    return end == g->size;
}
/** \brief CFI indices are device words; caller separately supplied query byte
 * address. */
static uint8_t cfi_byte(const nx_stm32_fsmc_nor_t* n, uint32_t word) {
    return (uint8_t)read_word(n, word * (n->width / 8U));
}
static uint16_t cfi_half(const nx_stm32_fsmc_nor_t* n, uint32_t word) {
    return (uint16_t)((uint16_t)cfi_byte(n, word) |
                      (uint16_t)((uint16_t)cfi_byte(n, word + 1U) << 8U));
}
static bool cfi_matches(nx_stm32_fsmc_nor_t* n) {
    command(n, n->config->cfi_address, 0x98U);
    bool valid = cfi_byte(n, 0x10U) == 'Q' && cfi_byte(n, 0x11U) == 'R' &&
                 cfi_byte(n, 0x12U) == 'Y' &&
                 cfi_half(n, 0x13U) == n->config->command_set;
    uint8_t exponent = cfi_byte(n, 0x27U);
    uint8_t regions = cfi_byte(n, 0x2cU);
    valid = valid && exponent < 32U &&
            (UINT32_C(1) << exponent) == n->config->geometry->size &&
            regions != 0U && regions <= 32U;
    size_t sector = 0U;
    for (uint32_t region = 0U; valid && region < regions; ++region) {
        uint32_t count = (uint32_t)cfi_half(n, 0x2dU + region * 4U) + 1U;
        uint32_t size = (uint32_t)cfi_half(n, 0x2fU + region * 4U) * 256U;
        if (size == 0U)
            size = 128U;
        if (count > n->config->geometry->sector_count - sector) {
            valid = false;
            break;
        }
        for (uint32_t block = 0U; block < count; ++block)
            valid &= n->config->geometry->sectors[sector++].size == size;
    }
    valid &= sector == n->config->geometry->sector_count;
    read_array(n);
    return valid;
}
/** \brief Poll once; never reset or abort a busy pulse to claim settlement. */
static bool pulse_idle(nx_stm32_fsmc_nor_t* n, bool* failed) {
    *failed = false;
    if (n->config->command_set == 1U) {
        command(n, n->pulse_offset, 0x70U);
        uint16_t status = read_word(n, n->pulse_offset);
        if ((status & 0x80U) == 0U)
            return false;
        *failed = (status & 0x3aU) != 0U;
        if (*failed)
            command(n, 0U, 0x50U);
        read_array(n);
        return true;
    }
    uint16_t first = read_word(n, n->pulse_offset);
    uint16_t second = read_word(n, n->pulse_offset);
    if (((first ^ second) & 0x40U) != 0U) {
        if ((second & 0x20U) != 0U) {
            first = read_word(n, n->pulse_offset);
            second = read_word(n, n->pulse_offset);
            if (((first ^ second) & 0x40U) != 0U)
                *failed = true;
            else {
                if (n->request != NULL)
                    *failed = second !=
                              (n->request->operation == NX_FLASH_ASYNC_PROGRAM
                                   ? n->pulse_value
                               : n->width == 16U ? 0xffffU
                                                 : 0xffU);
                return true;
            }
        }
        return false;
    }
    if (n->request != NULL)
        *failed = second != (n->request->operation == NX_FLASH_ASYNC_PROGRAM
                                 ? n->pulse_value
                             : n->width == 16U ? 0xffffU
                                               : 0xffU);
    return true;
}
nx_result_t
nx_stm32_fsmc_nor_prepare(nx_stm32_fsmc_nor_t* n, nx_stm32_fsmc_t* bus,
                          size_t index,
                          const nx_stm32_fsmc_nor_config_t* config) {
    if (!nx_fsmc_task())
        return NX_ERROR_CONTEXT;
    if (!state_valid(n) ||
        !nx_fsmc_span(bus, sizeof(*bus), _Alignof(nx_stm32_fsmc_t), true) ||
        !nx_fsmc_span(config, sizeof(*config),
                      _Alignof(nx_stm32_fsmc_nor_config_t), false))
        return NX_ERROR_INVALID;
    if (n->lease.owner != NULL || n->config != NULL || n->request != NULL ||
        n->in_call)
        return NX_ERROR_BUSY;
    if (!bus->prepared || bus->config == NULL ||
        index >= bus->config->bank_count)
        return NX_ERROR_STATE;
    if (bus->config->banks[index].kind != NX_STM32_FSMC_NOR ||
        bus->config->parameters == NULL)
        return NX_ERROR_UNSUPPORTED;
    nx_stm32_fsmc_parameters_t p = bus->config->parameters[index];
    uintptr_t address =
        UINT32_C(0x60000000) + (uint32_t)(p.bank - 1U) * UINT32_C(0x4000000);
    if ((config->command_set != 1U && config->command_set != 2U) ||
        !geometry_valid(config->geometry, p.width, address, p.bytes) ||
        config->ready_delay_us > 1000000U || p.burst_read || p.burst_write ||
        config->unlock1 >= p.bytes || config->unlock2 >= p.bytes ||
        config->cfi_address >= p.bytes ||
        config->unlock1 % (p.width / 8U) != 0U ||
        config->unlock2 % (p.width / 8U) != 0U ||
        config->cfi_address % (p.width / 8U) != 0U ||
        !nx_stm32_fsmc_storage(bus, n, sizeof(*n), true) ||
        nx_fsmc_overlap(n, sizeof(*n), config, sizeof(*config)) ||
        nx_fsmc_overlap(n, sizeof(*n), config->geometry,
                        sizeof(*config->geometry)) ||
        nx_fsmc_overlap(n, sizeof(*n), config->geometry->sectors,
                        config->geometry->sector_count *
                            sizeof(nx_flash_sector_t)))
        return NX_ERROR_INVALID;
    nx_time_us_t now = bus->config->clock.read(bus->config->clock.context);
    if (now == NX_DEADLINE_NEVER)
        return NX_ERROR_STATE;
    nx_result_t result = nx_stm32_fsmc_acquire(bus, index, &n->lease);
    if (result != NX_SUCCESS)
        return result;
    n->config = config;
    n->width = p.width;
    n->last_clock = now;
    n->closed = true;
    /* Initial status observation precedes CFI/read-array writes. A retained
     * power-domain operation from before MCU reset is not silently aborted. */
    bool failed;
    n->pulse = true;
    if (!pulse_idle(n, &failed))
        return NX_ERROR_BUSY;
    n->pulse = false;
    if (config->cfi && !cfi_matches(n))
        return NX_ERROR_IO;
    n->closed = false;
    return NX_SUCCESS;
}
/** \brief First terminal observation survives cancel, timeout and recovery. */
static void terminal(nx_stm32_fsmc_nor_t* n, nx_result_t result) {
    if (n->terminal == NX_SUCCESS)
        n->terminal = result;
}
static void settle(nx_stm32_fsmc_nor_t* n) {
    nx_flash_async_request_t* request = n->request;
    nx_result_t result = n->terminal;
    size_t done = n->done;
    n->request = NULL;
    nx_arch_dsb();
    nx_request_settle(&request->base, result, done);
}
static const nx_flash_sector_t* sector_at(const nx_stm32_fsmc_nor_t* n,
                                          uint32_t offset) {
    const nx_flash_geometry_t* g = n->config->geometry;
    for (size_t i = 0U; i < g->sector_count; ++i)
        if (g->sectors[i].offset == offset)
            return &g->sectors[i];
    return NULL;
}
static void unlock(nx_stm32_fsmc_nor_t* n) {
    command(n, n->config->unlock1, 0xaaU);
    command(n, n->config->unlock2, 0x55U);
}
/** \brief A service call issues at most one physical pulse. */
static nx_result_t service_inner(nx_stm32_fsmc_nor_t* n) {
    nx_result_t check = nx_stm32_fsmc_check(&n->lease);
    if (check != NX_SUCCESS) {
        if (n->request != NULL) {
            terminal(n, check);
            (void)nx_request_transition(&n->request->base,
                                        NX_REQUEST_QUARANTINED);
        }
        return check;
    }
    nx_time_us_t now;
    nx_result_t clock = clock_now(n, &now);
    if (n->request != NULL) {
        if (clock != NX_SUCCESS)
            terminal(n, clock);
        else if (now >= n->request->base.deadline)
            terminal(n, NX_ERROR_TIMEOUT);
    } else if (clock != NX_SUCCESS)
        return clock;
    if (n->pulse) {
        if (n->config->ready_delay_us != 0U &&
            (clock != NX_SUCCESS || now < n->poll_after)) {
            if (n->request != NULL && n->terminal != NX_SUCCESS)
                (void)nx_request_transition(&n->request->base,
                                            NX_REQUEST_QUARANTINED);
            return clock != NX_SUCCESS ? clock : NX_ERROR_BUSY;
        }
        bool failed;
        bool idle = pulse_idle(n, &failed);
        if (failed && n->request != NULL)
            terminal(n, NX_ERROR_IO);
        if (!idle) {
            if (n->request != NULL && n->terminal != NX_SUCCESS)
                (void)nx_request_transition(&n->request->base,
                                            NX_REQUEST_QUARANTINED);
            return NX_ERROR_BUSY;
        }
        /* Only actual idle authorizes the AMD reset/read-array command.
         * It clears a terminal status mode without replaying the pulse. */
        if (n->config->command_set == 2U &&
            (failed || n->terminal == NX_ERROR_IO))
            read_array(n);
        n->pulse = false;
        if (n->request != NULL && !failed)
            n->done += n->pulse_bytes;
        /* Post-poll time decides late completion, before releasing storage. */
        if (n->request != NULL) {
            clock = clock_now(n, &now);
            if (clock != NX_SUCCESS)
                terminal(n, clock);
            else if (now >= n->request->base.deadline)
                terminal(n, NX_ERROR_TIMEOUT);
        }
    }
    if (n->request == NULL)
        return NX_SUCCESS;
    if (n->terminal != NX_SUCCESS || n->done == n->request->length) {
        settle(n);
        return NX_SUCCESS;
    }
    nx_flash_async_request_t* r = n->request;
    if (n->last_clock >
        NX_DEADLINE_NEVER - 1U - n->config->ready_delay_us - 1U) {
        terminal(n, NX_ERROR_OVERFLOW);
        settle(n);
        return NX_SUCCESS;
    }
    n->pulse_offset = r->offset + (uint32_t)n->done;
    if (r->operation == NX_FLASH_ASYNC_PROGRAM) {
        const uint8_t* data = r->data;
        n->pulse_value = data[n->done];
        if (n->width == 16U)
            n->pulse_value =
                (uint16_t)(n->pulse_value |
                           (uint16_t)((uint16_t)data[n->done + 1U] << 8U));
        n->pulse_bytes = n->width / 8U;
        uint16_t old = read_word(n, n->pulse_offset);
        if ((old & n->pulse_value) != n->pulse_value) {
            terminal(n, NX_ERROR_IO);
            settle(n);
            return NX_SUCCESS;
        }
        /* Even an all-FF no-op must describe the actual old contents. An
         * external read can consume the remaining deadline before a pulse. */
        clock = clock_now(n, &now);
        if (clock != NX_SUCCESS || now >= r->base.deadline) {
            terminal(n, clock != NX_SUCCESS ? clock : NX_ERROR_TIMEOUT);
            settle(n);
            return NX_SUCCESS;
        }
        if (n->pulse_value == (n->width == 16U ? 0xffffU : 0xffU)) {
            n->done += n->pulse_bytes;
            if (n->done == r->length)
                settle(n);
            return NX_SUCCESS;
        }
        if (n->config->command_set == 2U) {
            unlock(n);
            command(n, n->config->unlock1, 0xa0U);
        } else
            command(n, n->pulse_offset, 0x40U);
        command(n, n->pulse_offset, n->pulse_value);
    } else {
        n->pulse_bytes = sector_at(n, n->pulse_offset)->size;
        if (n->config->command_set == 2U) {
            unlock(n);
            command(n, n->config->unlock1, 0x80U);
            unlock(n);
            command(n, n->pulse_offset, 0x30U);
        } else {
            command(n, n->pulse_offset, 0x20U);
            command(n, n->pulse_offset, 0xd0U);
        }
    }
    n->pulse = true;
    nx_time_us_t started;
    nx_result_t clock_result = clock_now(n, &started);
    if (clock_result != NX_SUCCESS)
        terminal(n, clock_result);
    uint64_t delay = n->config->ready_delay_us;
    if (delay != 0U)
        ++delay;
    n->poll_after = n->last_clock > NX_DEADLINE_NEVER - 1U - delay
                        ? NX_DEADLINE_NEVER
                        : n->last_clock + delay;
    return NX_SUCCESS;
}
static nx_result_t service(void* context) {
    nx_stm32_fsmc_nor_t* n = context;
    if (!nx_fsmc_task())
        return NX_ERROR_CONTEXT;
    if (!state_valid(n))
        return NX_ERROR_INVALID;
    if (n->config == NULL || n->lease.owner == NULL)
        return NX_ERROR_STATE;
    if (n->in_call)
        return NX_ERROR_BUSY;
    n->in_call = true;
    nx_result_t result = service_inner(n);
    n->in_call = false;
    return result;
}
static nx_result_t submit(void* context, nx_flash_async_request_t* r) {
    nx_stm32_fsmc_nor_t* n = context;
    if (!nx_fsmc_task())
        return NX_ERROR_CONTEXT;
    if (!state_valid(n) ||
        !nx_fsmc_span(r, sizeof(*r), _Alignof(nx_flash_async_request_t), true))
        return NX_ERROR_INVALID;
    if (n->config == NULL || n->closed)
        return NX_ERROR_STATE;
    if (n->request != NULL || n->pulse || n->in_call)
        return NX_ERROR_BUSY;
    if (!storage(n, r, sizeof(*r), true))
        return NX_ERROR_INVALID;
    if (nx_request_state(&r->base) != NX_REQUEST_READY)
        return NX_ERROR_STATE;
    const nx_flash_geometry_t* g = n->config->geometry;
    if (r->length == 0U || r->offset >= g->size ||
        r->length > g->size - r->offset ||
        r->base.deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    if (r->operation == NX_FLASH_ASYNC_PROGRAM) {
        if (r->offset % g->program_unit != 0U ||
            r->length % g->program_unit != 0U ||
            !storage(n, r->data, r->length, false) ||
            nx_fsmc_overlap(r, sizeof(*r), r->data, r->length))
            return NX_ERROR_INVALID;
    } else if (r->operation == NX_FLASH_ASYNC_ERASE) {
        size_t end = 0U;
        while (end < r->length) {
            const nx_flash_sector_t* sector =
                sector_at(n, r->offset + (uint32_t)end);
            if (sector == NULL || sector->size > r->length - end)
                return NX_ERROR_INVALID;
            end += sector->size;
        }
        if (r->data != NULL)
            return NX_ERROR_INVALID;
    } else
        return NX_ERROR_INVALID;
    nx_result_t result = nx_stm32_fsmc_check(&n->lease);
    if (result != NX_SUCCESS)
        return result;
    nx_time_us_t now;
    result = clock_now(n, &now);
    if (result != NX_SUCCESS)
        return result;
    result = nx_request_admit(&r->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS)
        return result;
    n->request = r;
    n->done = 0U;
    n->terminal = NX_SUCCESS;
    (void)service(n);
    return NX_SUCCESS;
}
static nx_result_t cancel(void* context, nx_flash_async_request_t* r) {
    nx_stm32_fsmc_nor_t* n = context;
    if (!nx_fsmc_task())
        return NX_ERROR_CONTEXT;
    if (!state_valid(n) || r == NULL)
        return NX_ERROR_INVALID;
    if (n->in_call)
        return NX_ERROR_BUSY;
    if (n->request != r)
        return NX_ERROR_STATE;
    terminal(n, NX_ERROR_CANCELLED);
    if (nx_request_state(&r->base) != NX_REQUEST_QUARANTINED)
        (void)nx_request_transition(&r->base, NX_REQUEST_DRAINING);
    return NX_SUCCESS;
}
static nx_result_t stop(void* context) {
    nx_stm32_fsmc_nor_t* n = context;
    if (!nx_fsmc_task())
        return NX_ERROR_CONTEXT;
    if (!state_valid(n))
        return NX_ERROR_INVALID;
    if (n->in_call)
        return NX_ERROR_BUSY;
    n->closed = true;
    if (n->request != NULL)
        terminal(n, NX_ERROR_CANCELLED);
    return n->config == NULL ? NX_SUCCESS : service(n);
}
nx_result_t nx_stm32_fsmc_nor_close(nx_stm32_fsmc_nor_t* n) {
    if (!nx_fsmc_task())
        return NX_ERROR_CONTEXT;
    if (!state_valid(n))
        return NX_ERROR_INVALID;
    if (n->in_call || n->request != NULL)
        return NX_ERROR_BUSY;
    n->closed = true;
    if (n->lease.owner == NULL)
        return NX_SUCCESS;
    if (n->pulse) {
        nx_result_t result = service(n);
        if (result != NX_SUCCESS)
            return result;
    }
    nx_result_t result = nx_stm32_fsmc_check(&n->lease);
    if (result != NX_SUCCESS)
        return result;
    read_array(n);
    result = nx_stm32_fsmc_release(&n->lease, n->lease.epoch);
    if (result == NX_SUCCESS)
        n->config = NULL;
    return result;
}
static const nx_flash_geometry_t* geometry(const void* context) {
    const nx_stm32_fsmc_nor_t* n = context;
    return state_valid(n) && n->config != NULL && !n->closed
               ? n->config->geometry
               : NULL;
}
static nx_result_t read_data(const void* context, uint32_t offset, void* data,
                             size_t bytes) {
    nx_stm32_fsmc_nor_t* n = (nx_stm32_fsmc_nor_t*)context;
    if (!nx_fsmc_task())
        return NX_ERROR_CONTEXT;
    if (!state_valid(n))
        return NX_ERROR_INVALID;
    if (n->config == NULL || n->closed)
        return NX_ERROR_STATE;
    if (n->in_call || n->request != NULL || n->pulse)
        return NX_ERROR_BUSY;
    if (bytes == 0U)
        return offset <= n->config->geometry->size ? NX_SUCCESS
                                                   : NX_ERROR_INVALID;
    if (offset >= n->config->geometry->size ||
        bytes > n->config->geometry->size - offset ||
        !storage(n, data, bytes, true))
        return NX_ERROR_INVALID;
    nx_result_t result = nx_stm32_fsmc_check(&n->lease);
    if (result != NX_SUCCESS)
        return result;
    n->in_call = true;
    uint8_t* out = data;
    for (size_t i = 0U; i < bytes; ++i) {
        uint32_t byte = offset + (uint32_t)i;
        uint16_t value = read_word(n, byte & ~(uint32_t)(n->width / 8U - 1U));
        out[i] = (uint8_t)(value >>
                           ((n->width == 16U && (byte & 1U) != 0U) ? 8U : 0U));
    }
    nx_arch_dsb();
    n->in_call = false;
    return NX_SUCCESS;
}
const nx_flash_ops_t nx_stm32_fsmc_nor_flash_ops = {geometry, read_data, NULL,
                                                    NULL, NULL};
const nx_flash_async_ops_t nx_stm32_fsmc_nor_async_ops = {submit, cancel,
                                                          service, stop};
