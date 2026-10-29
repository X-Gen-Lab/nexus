/**
 * \file            flash.c
 * \brief           Exact U575 physical pages and settled128bit Flash pulses
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32u575_cold.h"
#include "stm32u575_provider.h"
#include <string.h>
#define PAGE_BYTES 8192U
#define FLASH_ERRORS                                                           \
    ((uint32_t)(FLASH_NSSR_OPERR | FLASH_NSSR_PROGERR | FLASH_NSSR_WRPERR |    \
                FLASH_NSSR_PGAERR | FLASH_NSSR_SIZERR | FLASH_NSSR_PGSERR |    \
                FLASH_NSSR_OPTWERR))
#define PULSE_BUSY      ((uint32_t)(FLASH_NSSR_BSY | FLASH_NSSR_WDW))
#define CONTROL_ALLOWED ((uint32_t)(FLASH_NSCR_LOCK | FLASH_NSCR_OPTLOCK))
/* Geometry is emitted from the selected physical part facts. */
static bool task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool valid(const nx_stm32u575_flash_state_t* p) {
    return p != NULL && p->registers != NULL && p->memory != NULL &&
           (uintptr_t)p->memory % 16U == 0U;
}
static bool range(const nx_stm32u575_flash_state_t* p, uint32_t offset,
                  size_t length) {
    return valid(p) && offset <= g_nx_stm32u575_flash_geometry.size &&
           length <= g_nx_stm32u575_flash_geometry.size - offset;
}
static bool span(const void* pointer, size_t bytes) {
    return pointer != NULL && (uintptr_t)pointer <= UINTPTR_MAX - bytes;
}
static bool overlap(const void* a, size_t as, const void* b, size_t bs) {
    return (uintptr_t)a < (uintptr_t)b + bs && (uintptr_t)b < (uintptr_t)a + as;
}
static bool external(const nx_stm32u575_flash_state_t* p, const void* data,
                     size_t bytes) {
    return span(data, bytes) && !overlap(data, bytes, p, sizeof(*p)) &&
           !overlap(data, bytes, p->registers, sizeof(*p->registers));
}
static nx_result_t admission(const nx_stm32u575_flash_state_t* p) {
    if (!g_nx_stm32u575_system.started ||
        g_nx_stm32u575_system.core_hz != 4000000U)
        return NX_ERROR_STATE;
    nx_result_t identity = nx_stm32u575_validate_identity();
    if (identity != NX_SUCCESS)
        return identity;
    if ((NX_U575_READ(&p->registers->NSSR) & PULSE_BUSY) != 0U ||
        (NX_U575_READ(&p->registers->NSCR) & ~CONTROL_ALLOWED) != 0U ||
        (NX_U575_READ(&p->registers->NSCR) & FLASH_NSCR_LOCK) == 0U ||
        !NX_U575_IRQ_AVAILABLE(FLASH_IRQn))
        return NX_ERROR_BUSY;
    return NX_SUCCESS;
}
nx_result_t nx_stm32u575_flash_initialize(nx_stm32u575_flash_state_t* p) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if (p->initialized || p->retained || p->active)
        return NX_ERROR_BUSY;
    nx_result_t result = admission(p);
    if (result == NX_SUCCESS)
        p->initialized = p->retained = true;
    return result;
}
static const nx_flash_geometry_t* geometry(const void* context) {
    return context != NULL ? &g_nx_stm32u575_flash_geometry : NULL;
}
static nx_result_t read(const void* context, uint32_t offset, void* data,
                        size_t length) {
    const nx_stm32u575_flash_state_t* p = context;
    if (!range(p, offset, length) ||
        (length != 0U &&
         (!external(p, data, length) ||
          overlap(data, length, (const void*)(p->memory + offset), length))))
        return NX_ERROR_INVALID;
    if (!p->initialized)
        return NX_ERROR_STATE;
    if (p->active || (NX_U575_READ(&p->registers->NSSR) & PULSE_BUSY) != 0U)
        return NX_ERROR_BUSY;
    uint8_t* output = data;
    for (size_t index = 0U; index < length; ++index)
        output[index] = p->memory[offset + index];
    return NX_SUCCESS;
}
static nx_result_t begin(nx_stm32u575_flash_state_t* p, nx_time_us_t deadline) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!p->initialized)
        return NX_ERROR_STATE;
    if (p->active)
        return NX_ERROR_BUSY;
    if (deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_UNSUPPORTED;
    if (nx_deadline_expired(deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    nx_result_t result = admission(p);
    if (result != NX_SUCCESS)
        return result;
    NX_U575_WRITE(&p->registers->NSSR, FLASH_ERRORS | FLASH_NSSR_EOP);
    nx_arch_dsb();
    if ((NX_U575_READ(&p->registers->NSSR) & (FLASH_ERRORS | FLASH_NSSR_EOP)) !=
        0U)
        return NX_ERROR_IO;
    NX_U575_WRITE(&p->registers->NSKEYR, 0x45670123U);
    NX_U575_WRITE(&p->registers->NSKEYR, 0xcdef89abU);
    nx_arch_dsb();
    if ((NX_U575_READ(&p->registers->NSCR) & FLASH_NSCR_LOCK) != 0U)
        return NX_ERROR_IO;
    p->active = true;
    return NX_SUCCESS;
}
/** \brief A deadline never interrupts a physical program/erase pulse. */
static nx_result_t pulse(nx_stm32u575_flash_state_t* p, nx_time_us_t deadline) {
    do {
        NX_U575_FLASH_POLL(p);
    } while ((NX_U575_READ(&p->registers->NSSR) & PULSE_BUSY) != 0U);
    uint32_t status = NX_U575_READ(&p->registers->NSSR);
    if ((status & FLASH_ERRORS) != 0U || (status & FLASH_NSSR_EOP) == 0U)
        return NX_ERROR_IO;
    return nx_deadline_expired(deadline, nx_time_now_us()) ? NX_ERROR_TIMEOUT
                                                           : NX_SUCCESS;
}
static nx_result_t end(nx_stm32u575_flash_state_t* p) {
    uint32_t control = NX_U575_READ(&p->registers->NSCR);
    control &= CONTROL_ALLOWED;
    control |= FLASH_NSCR_LOCK;
    NX_U575_WRITE(&p->registers->NSCR, control);
    nx_arch_dsb();
    p->active = false;
    if (NX_U575_READ(&p->registers->NSCR) != control) {
        p->initialized = false;
        return NX_ERROR_IO;
    }
    /* RM0456 7.9.7 gates EOP itself with EOPIE. The exclusive FLASH IRQ
     * remains disabled throughout polling; withdraw its source and lock
     * before clearing the pending fact, preserving its original priority. */
    nx_result_t result = NX_U575_IRQ_CONTROL(FLASH_IRQn, false, 0U);
    if (result != NX_SUCCESS || !NX_U575_IRQ_AVAILABLE(FLASH_IRQn)) {
        p->initialized = false;
        return result == NX_SUCCESS ? NX_ERROR_IO : result;
    }
    return NX_SUCCESS;
}
static nx_result_t program(void* context, uint32_t offset, const void* data,
                           size_t length, nx_time_us_t deadline) {
    nx_stm32u575_flash_state_t* p = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!range(p, offset, length) || length == 0U || offset % 16U != 0U ||
        length % 16U != 0U || !external(p, data, length) ||
        overlap(data, length, (const void*)(p->memory + offset), length))
        return NX_ERROR_INVALID;
    /* Reject known programmed cells before unlocking. All-FF reads cannot
     * prove hidden ECC history: the caller must own a fresh sector erase
     * and must erase again after every failed or interrupted pulse. */
    for (size_t index = 0U; index < length; ++index)
        if (p->memory[offset + index] != 0xffU)
            return NX_ERROR_STATE;
    nx_result_t result = begin(p, deadline);
    if (result != NX_SUCCESS)
        return result;
    const uint8_t* input = data;
    for (size_t index = 0U; index < length; index += 16U) {
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            result = NX_ERROR_TIMEOUT;
            break;
        }
        uint32_t words[4];
        memcpy(words, input + index, sizeof(words));
        /* Writing all ones still programs ECC; preserve the erased physical
         * unit by issuing no programming pulse for an all-FF input unit. */
        if (words[0] == UINT32_MAX && words[1] == UINT32_MAX &&
            words[2] == UINT32_MAX && words[3] == UINT32_MAX)
            continue;
        NX_U575_WRITE(&p->registers->NSSR, FLASH_ERRORS | FLASH_NSSR_EOP);
        nx_arch_dsb();
        if ((NX_U575_READ(&p->registers->NSSR) &
             (FLASH_ERRORS | FLASH_NSSR_EOP)) != 0U) {
            result = NX_ERROR_IO;
            break;
        }
        uint32_t control = NX_U575_READ(&p->registers->NSCR) | FLASH_NSCR_PG |
                           FLASH_NSCR_EOPIE;
        NX_U575_WRITE(&p->registers->NSCR, control);
        nx_arch_dsb();
        if (NX_U575_READ(&p->registers->NSCR) != control) {
            result = NX_ERROR_IO;
            break;
        }
        /* RM0456 7.3.7 requires four successive32bit writes in one aligned
         * quad-word. Input data is copied before the first write; the brief
         * incoming-mask-preserving exclusion cannot interleave another ISR. */
        nx_arch_irq_state_t saved = nx_arch_irq_save();
        for (size_t word = 0U; word < 4U; ++word)
            NX_U575_FLASH_PROGRAM32(p->memory + offset + index + word * 4U,
                                    words[word]);
        nx_arch_dsb();
        nx_arch_irq_restore(saved);
        result = pulse(p, deadline);
        if (result != NX_SUCCESS)
            break;
        for (unsigned byte = 0U; byte < 16U; ++byte)
            if (p->memory[offset + index + byte] != input[index + byte])
                result = NX_ERROR_IO;
        if (result != NX_SUCCESS)
            break;
    }
    nx_result_t cleanup = end(p);
    return cleanup == NX_SUCCESS ? result : cleanup;
}
static nx_result_t erase(void* context, uint32_t offset, size_t length,
                         nx_time_us_t deadline) {
    nx_stm32u575_flash_state_t* p = context;
    if (!range(p, offset, length) || length == 0U ||
        offset % PAGE_BYTES != 0U || length % PAGE_BYTES != 0U)
        return NX_ERROR_INVALID;
    nx_result_t result = begin(p, deadline);
    if (result != NX_SUCCESS)
        return result;
    for (size_t index = 0U; index < length; index += PAGE_BYTES) {
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            result = NX_ERROR_TIMEOUT;
            break;
        }
        uint32_t page = (offset + (uint32_t)index) / PAGE_BYTES;
        uint32_t control =
            NX_U575_READ(&p->registers->NSCR) & FLASH_NSCR_OPTLOCK;
        control |= FLASH_NSCR_PER | FLASH_NSCR_EOPIE |
                   ((page % 128U) << FLASH_NSCR_PNB_Pos);
        if (page >= 128U)
            control |= FLASH_NSCR_BKER;
        NX_U575_WRITE(&p->registers->NSSR, FLASH_ERRORS | FLASH_NSSR_EOP);
        nx_arch_dsb();
        if ((NX_U575_READ(&p->registers->NSSR) &
             (FLASH_ERRORS | FLASH_NSSR_EOP)) != 0U) {
            result = NX_ERROR_IO;
            break;
        }
        NX_U575_WRITE(&p->registers->NSCR, control);
        nx_arch_dsb();
        if (NX_U575_READ(&p->registers->NSCR) != control) {
            result = NX_ERROR_IO;
            break;
        }
        NX_U575_WRITE(&p->registers->NSCR, control | FLASH_NSCR_STRT);
        result = pulse(p, deadline);
        if (result != NX_SUCCESS)
            break;
    }
    nx_result_t cleanup = end(p);
    return cleanup == NX_SUCCESS ? result : cleanup;
}
nx_result_t nx_stm32u575_flash_close(nx_stm32u575_flash_state_t* p) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if (p->active || (NX_U575_READ(&p->registers->NSSR) & PULSE_BUSY) != 0U)
        return NX_ERROR_BUSY;
    if (!p->retained)
        return NX_SUCCESS;
    nx_result_t result = end(p);
    if (result == NX_SUCCESS)
        p->initialized = p->retained = false;
    return result;
}
const nx_flash_ops_t nx_stm32u575_flash_ops = {
    .geometry = geometry, .read = read, .program = program, .erase = erase};

nx_result_t nx_stm32u575_flash_prepare(nx_stm32u575_flash_state_t* port) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (port == NULL)
        return NX_ERROR_INVALID;
    if (port->retained || port->initialized || port->active)
        return NX_ERROR_BUSY;
    port->registers = FLASH;
    port->memory = (volatile uint8_t*)0x08000000U;
    return nx_stm32u575_flash_initialize(port);
}
