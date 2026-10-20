/**
 * \file            legacy_flash.c
 * \brief           F103RB/GD303RC half-word physical Flash pulses
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "legacy_devices.h"
#include <string.h>

#define FLASH_BUSY    1U
#define FLASH_ERRORS  (4U | 16U)
#define FLASH_END     32U
#define FLASH_PROGRAM 1U
#define FLASH_ERASE   2U
#define FLASH_START   64U
#define FLASH_LOCK    128U
#define FLASH_MODES   (FLASH_PROGRAM | FLASH_ERASE | 4U | 16U | 32U | FLASH_START)

static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool valid(const nx_legacy_flash_state_t* p) {
    return p != NULL && p->registers != NULL && p->memory != NULL &&
           (uintptr_t)p->memory % 2U == 0U && p->geometry != NULL &&
           p->geometry->program_unit == 2U && p->geometry->sectors != NULL &&
           p->geometry->sector_count != 0U && p->geometry->size != 0U;
}
static bool range(const nx_legacy_flash_state_t* p, uint32_t offset,
                  size_t length) {
    return valid(p) && offset <= p->geometry->size &&
           length <= p->geometry->size - offset;
}
static bool overlaps(const void* a, size_t as, const void* b, size_t bs) {
    return (uintptr_t)a < (uintptr_t)b + bs && (uintptr_t)b < (uintptr_t)a + as;
}
static bool write_checked(volatile uint32_t* r, uint32_t v, uint32_t mask) {
    NX_LEGACY_WRITE(r, v);
    nx_arch_dsb();
    return (NX_LEGACY_READ(r) & mask) == (v & mask);
}

/**
 * \brief Stale completion cannot prove a refused later launch completed.
 * \note PM0075 Rev2 section3.4 and GD30x Rev3.4 section2.4.4 latch EOP/ENDF
 *       after every successful pulse independently of interrupt enable.
 */
static bool clear_status(nx_legacy_flash_state_t* p) {
    NX_LEGACY_WRITE(&p->registers->status, FLASH_ERRORS | FLASH_END);
    nx_arch_dsb();
    return (NX_LEGACY_READ(&p->registers->status) &
            (FLASH_BUSY | FLASH_ERRORS | FLASH_END)) == 0U;
}

nx_result_t nx_legacy_flash_initialize(nx_legacy_flash_state_t* p) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid(p) || p->supply_mv < 2700U || p->supply_mv > 3600U)
        return NX_ERROR_INVALID;
    if (p->initialized || p->active || p->fault ||
        (NX_LEGACY_READ(&p->registers->status) & FLASH_BUSY) != 0U ||
        (NX_LEGACY_READ(&p->registers->control) & ~FLASH_LOCK) != 0U)
        return NX_ERROR_BUSY;
    uint32_t offset = 0U;
    for (size_t i = 0U; i < p->geometry->sector_count; ++i) {
        const nx_flash_sector_t* sector = &p->geometry->sectors[i];
        if (sector->offset != offset || sector->size == 0U ||
            sector->size > p->geometry->size - offset)
            return NX_ERROR_INVALID;
        offset += sector->size;
    }
    if (offset != p->geometry->size)
        return NX_ERROR_INVALID;
    p->initialized = true;
    return NX_SUCCESS;
}
nx_result_t nx_legacy_flash_close(nx_legacy_flash_state_t* p) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if (!p->initialized && !p->fault && !p->active)
        return NX_SUCCESS;
    if (p->active || (NX_LEGACY_READ(&p->registers->status) & FLASH_BUSY) != 0U)
        return NX_ERROR_BUSY;
    if (!write_checked(&p->registers->control,
                       (NX_LEGACY_READ(&p->registers->control) & ~FLASH_MODES) |
                           FLASH_LOCK,
                       FLASH_MODES | FLASH_LOCK)) {
        p->fault = true;
        return NX_ERROR_IO;
    }
    p->initialized = false;
    p->fault = false;
    return NX_SUCCESS;
}
static const nx_flash_geometry_t* geometry(const void* context) {
    const nx_legacy_flash_state_t* p = context;
    return valid(p) ? p->geometry : NULL;
}
static nx_result_t read(const void* context, uint32_t offset, void* data,
                        size_t length) {
    const nx_legacy_flash_state_t* p = context;
    if (!range(p, offset, length) || (length != 0U && data == NULL) ||
        (uintptr_t)data > UINTPTR_MAX - length ||
        (length != 0U &&
         (overlaps(data, length, p, sizeof(*p)) ||
          overlaps(data, length, p->registers, sizeof(*p->registers)) ||
          overlaps(data, length, (const void*)p->memory, p->geometry->size))))
        return NX_ERROR_INVALID;
    if (!p->initialized || p->fault)
        return NX_ERROR_STATE;
    if (p->active || (NX_LEGACY_READ(&p->registers->status) & FLASH_BUSY) != 0U)
        return NX_ERROR_BUSY;
    uint8_t* output = data;
    for (size_t i = 0U; i < length; ++i)
        output[i] = p->memory[offset + i];
    return NX_SUCCESS;
}
static nx_result_t begin(nx_legacy_flash_state_t* p, nx_time_us_t deadline) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!p->initialized || p->fault)
        return NX_ERROR_STATE;
    if (deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_UNSUPPORTED;
    if (nx_deadline_expired(deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    if (p->active || (NX_LEGACY_READ(&p->registers->status) & FLASH_BUSY) != 0U)
        return NX_ERROR_BUSY;
    if ((NX_LEGACY_READ(&p->registers->control) & ~FLASH_LOCK) != 0U)
        return NX_ERROR_STATE;
    p->active = true;
    if ((NX_LEGACY_READ(&p->registers->control) & FLASH_LOCK) != 0U) {
        NX_LEGACY_WRITE(&p->registers->key, 0x45670123U);
        NX_LEGACY_WRITE(&p->registers->key, 0xcdef89abU);
        nx_arch_dsb();
        if ((NX_LEGACY_READ(&p->registers->control) & FLASH_LOCK) != 0U) {
            p->active = false;
            return NX_ERROR_IO;
        }
    }
    return NX_SUCCESS;
}
/** \brief A deadline never preempts a live physical pulse or returns a loan. */
static nx_result_t pulse(nx_legacy_flash_state_t* p, nx_time_us_t deadline) {
    do {
        NX_LEGACY_POLL(0U, p);
    } while ((NX_LEGACY_READ(&p->registers->status) & FLASH_BUSY) != 0U);
    uint32_t status = NX_LEGACY_READ(&p->registers->status);
    if ((status & FLASH_ERRORS) != 0U || (status & FLASH_END) == 0U)
        return NX_ERROR_IO;
    return nx_deadline_expired(deadline, nx_time_now_us()) ? NX_ERROR_TIMEOUT
                                                           : NX_SUCCESS;
}
static nx_result_t end(nx_legacy_flash_state_t* p, nx_result_t result) {
    bool locked = write_checked(
        &p->registers->control,
        (NX_LEGACY_READ(&p->registers->control) & ~FLASH_MODES) | FLASH_LOCK,
        FLASH_MODES | FLASH_LOCK);
    p->active = false;
    p->fault = !locked;
    return locked ? result : NX_ERROR_IO;
}
static nx_result_t finish(nx_legacy_flash_state_t* p, nx_result_t result,
                          nx_time_us_t deadline) {
    result = end(p, result);
    return result == NX_SUCCESS &&
                   nx_deadline_expired(deadline, nx_time_now_us())
               ? NX_ERROR_TIMEOUT
               : result;
}
static nx_result_t program(void* context, uint32_t offset, const void* data,
                           size_t length, nx_time_us_t deadline) {
    nx_legacy_flash_state_t* p = context;
    if (!range(p, offset, length) || data == NULL || length == 0U ||
        ((offset | length) & 1U) != 0U ||
        (uintptr_t)data > UINTPTR_MAX - length ||
        overlaps(data, length, p, sizeof(*p)) ||
        overlaps(data, length, p->registers, sizeof(*p->registers)))
        return NX_ERROR_INVALID;
    nx_result_t result = begin(p, deadline);
    if (result != NX_SUCCESS)
        return result;
    const uint8_t* input = data;
    for (size_t i = 0U; i < length; i += 2U) {
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            result = NX_ERROR_TIMEOUT;
            break;
        }
        uint16_t value;
        memcpy(&value, input + i, sizeof(value));
        volatile uint16_t* address =
            (volatile uint16_t*)(p->memory + offset + i);
        if (value == UINT16_MAX)
            continue;
        /* Both manuals require an erased halfword, except programming zero.
         * General repeated one-to-zero writes are not a supported shortcut. */
        if (*address != UINT16_MAX && value != 0U) {
            result = NX_ERROR_IO;
            break;
        }
        if (!clear_status(p) ||
            !write_checked(&p->registers->control, FLASH_PROGRAM,
                           FLASH_MODES | FLASH_LOCK)) {
            result = NX_ERROR_IO;
            break;
        }
        NX_LEGACY_PROGRAM(address, value);
        nx_arch_dsb();
        result = pulse(p, deadline);
        if (result == NX_SUCCESS && *address != value)
            result = NX_ERROR_IO;
        if (result != NX_SUCCESS)
            break;
    }
    return finish(p, result, deadline);
}
static nx_result_t erase(void* context, uint32_t offset, size_t length,
                         nx_time_us_t deadline) {
    nx_legacy_flash_state_t* p = context;
    if (!range(p, offset, length) || length == 0U)
        return NX_ERROR_INVALID;
    size_t first = p->geometry->sector_count, last = first;
    for (size_t i = 0U; i < p->geometry->sector_count; ++i) {
        const nx_flash_sector_t* s = &p->geometry->sectors[i];
        if (s->offset == offset)
            first = i;
        if (s->offset + s->size == offset + length)
            last = i;
    }
    if (first > last || last >= p->geometry->sector_count)
        return NX_ERROR_INVALID;
    nx_result_t result = begin(p, deadline);
    if (result != NX_SUCCESS)
        return result;
    for (size_t i = first; i <= last; ++i) {
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            result = NX_ERROR_TIMEOUT;
            break;
        }
        if (!clear_status(p) ||
            !write_checked(&p->registers->control, FLASH_ERASE,
                           FLASH_MODES | FLASH_LOCK) ||
            !write_checked(&p->registers->address,
                           p->geometry->base_address +
                               p->geometry->sectors[i].offset,
                           UINT32_MAX)) {
            result = NX_ERROR_IO;
            break;
        }
        NX_LEGACY_WRITE(&p->registers->control, FLASH_ERASE | FLASH_START);
        nx_arch_dsb();
        /* ES0962.2.11: BSY rises one cycle after STRT. A separate APB control
         * read plus barrier precedes the first status observation. */
        (void)NX_LEGACY_READ(&p->registers->control);
        nx_arch_dsb();
        result = pulse(p, deadline);
        if (result != NX_SUCCESS)
            break;
        const nx_flash_sector_t* sector = &p->geometry->sectors[i];
        for (uint32_t byte = 0U; byte < sector->size; ++byte) {
            if (p->memory[sector->offset + byte] != UINT8_MAX) {
                result = NX_ERROR_IO;
                break;
            }
        }
        if (result != NX_SUCCESS)
            break;
    }
    return finish(p, result, deadline);
}
const nx_flash_ops_t nx_legacy_flash_ops = {geometry, read, program, erase};
