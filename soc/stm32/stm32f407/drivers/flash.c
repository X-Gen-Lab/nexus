/**
 * \file            flash.c
 * \brief           Exact full-density STM32 Flash geometry and settled pulses
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_provider.h"
#include <string.h>

#ifndef NX_STM32_IO_POLL
#define NX_STM32_IO_POLL(kind, port) ((void)(kind), (void)(port))
#endif
#ifndef NX_STM32_CLEAR_FLAGS
#define NX_STM32_CLEAR_FLAGS(reg, mask) ((reg) = (mask))
#endif
#ifndef NX_STM32_FLASH_READ
#define NX_STM32_FLASH_READ(reg) (*(reg))
#endif
#ifndef NX_STM32_FLASH_WRITE
#define NX_STM32_FLASH_WRITE(reg, value) (*(reg) = (value))
#endif
#ifndef NX_STM32_FLASH_STORE
#define NX_STM32_FLASH_STORE(address, value) (*(address) = (value))
#endif
#ifndef NX_STM32_FLASH_IRQ_IDLE
#define NX_STM32_FLASH_IRQ_IDLE()                                              \
    (NVIC_GetEnableIRQ(FLASH_IRQn) == 0U &&                                    \
     NVIC_GetPendingIRQ(FLASH_IRQn) == 0U)
#endif
#ifndef NX_STM32_FLASH_IRQ_ACTIVE
#define NX_STM32_FLASH_IRQ_ACTIVE() (NVIC_GetActive(FLASH_IRQn) != 0U)
#endif
#ifndef NX_STM32_FLASH_IRQ_CLEAR
/** \brief Withdraw only the disabled vector acquired by the polling owner. */
static bool clear_irq(void) {
    NVIC_ClearPendingIRQ(FLASH_IRQn);
    nx_arch_dsb();
    return NX_STM32_FLASH_IRQ_IDLE() && !NX_STM32_FLASH_IRQ_ACTIVE();
}
#define NX_STM32_FLASH_IRQ_CLEAR() clear_irq()
#endif

static const nx_flash_sector_t s_sectors[] = {
    {0x00000U, 0x04000U}, {0x04000U, 0x04000U}, {0x08000U, 0x04000U},
    {0x0C000U, 0x04000U}, {0x10000U, 0x10000U}, {0x20000U, 0x20000U},
    {0x40000U, 0x20000U}, {0x60000U, 0x20000U}, {0x80000U, 0x20000U},
    {0xA0000U, 0x20000U}, {0xC0000U, 0x20000U}, {0xE0000U, 0x20000U}};
const nx_flash_geometry_t g_nx_stm32_flash_ve = {.base_address = 0x08000000U,
                                                 .size = 524288U,
                                                 .program_unit = 4U,
                                                 .sectors = s_sectors,
                                                 .sector_count = 8U,
                                                 .program_flags = 0U};
const nx_flash_geometry_t g_nx_stm32_flash_zg = {.base_address = 0x08000000U,
                                                 .size = 1048576U,
                                                 .program_unit = 4U,
                                                 .sectors = s_sectors,
                                                 .sector_count = 12U,
                                                 .program_flags = 0U};

#define NX_FLASH_ERRORS                                                        \
    ((uint32_t)(FLASH_SR_OPERR | FLASH_SR_WRPERR | FLASH_SR_PGAERR |           \
                FLASH_SR_PGPERR | FLASH_SR_PGSERR))
#define NX_FLASH_OWNED_MODES                                                   \
    ((uint32_t)(FLASH_CR_PG | FLASH_CR_SER | FLASH_CR_SNB | FLASH_CR_STRT |    \
                FLASH_CR_EOPIE))

/** \brief Prove a fresh completion epoch before issuing each physical pulse. */
static bool clear_status(nx_stm32_flash_state_t* port) {
    NX_STM32_CLEAR_FLAGS(port->registers->SR, NX_FLASH_ERRORS | FLASH_SR_EOP);
    nx_arch_dsb();
    return (NX_STM32_FLASH_READ(&port->registers->SR) &
            (NX_FLASH_ERRORS | FLASH_SR_EOP | FLASH_SR_BSY)) == 0U;
}
static bool control(nx_stm32_flash_state_t* port, uint32_t value) {
    NX_STM32_FLASH_WRITE(&port->registers->CR, value);
    nx_arch_dsb();
    return NX_STM32_FLASH_READ(&port->registers->CR) == value;
}

/** \brief Validate physical subtraction bounds before any read or write. */
static bool range(const nx_stm32_flash_state_t* port, uint32_t offset,
                  size_t length) {
    return port != NULL && port->registers != NULL && port->geometry != NULL &&
           port->memory != NULL && offset <= port->geometry->size &&
           length <= port->geometry->size - offset;
}

/** \brief Expose immutable density geometry with no product reservation. */
const nx_flash_geometry_t* nx_stm32_flash_geometry(const void* context) {
    const nx_stm32_flash_state_t* port = context;
    return port != NULL ? port->geometry : NULL;
}

/** \brief Read physical nonvolatile bytes without a retained destination. */
nx_result_t nx_stm32_flash_read(const void* context, uint32_t offset,
                                void* data, size_t length) {
    const nx_stm32_flash_state_t* port = context;
    if (!range(port, offset, length) || (data == NULL && length != 0U)) {
        return NX_ERROR_INVALID;
    }
    if (port->active || (port->registers->SR & FLASH_SR_BSY) != 0U) {
        return NX_ERROR_BUSY;
    }
    uint8_t* output = data;
    for (size_t i = 0U; i < length; ++i) {
        output[i] = port->memory[offset + i];
    }
    return NX_SUCCESS;
}

/** \brief Unlock only when an operation passed all structural validation. */
static nx_result_t begin(nx_stm32_flash_state_t* port) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() ||
        nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->active ||
        (NX_STM32_FLASH_READ(&port->registers->SR) & FLASH_SR_BSY) != 0U ||
        (NX_STM32_FLASH_READ(&port->registers->CR) &
         ~(uint32_t)(FLASH_CR_LOCK | FLASH_CR_PSIZE)) != 0U ||
        !NX_STM32_FLASH_IRQ_IDLE() || NX_STM32_FLASH_IRQ_ACTIVE()) {
        return NX_ERROR_BUSY;
    }
    if (port->supply_mv < 2700U || port->supply_mv > 3600U) {
        return NX_ERROR_UNSUPPORTED;
    }
    if ((NX_STM32_FLASH_READ(&port->registers->CR) & FLASH_CR_LOCK) != 0U) {
        NX_STM32_FLASH_WRITE(&port->registers->KEYR, 0x45670123U);
        NX_STM32_FLASH_WRITE(&port->registers->KEYR, 0xCDEF89ABU);
        NX_STM32_IO_POLL(3U, port);
        if ((NX_STM32_FLASH_READ(&port->registers->CR) & FLASH_CR_LOCK) != 0U) {
            return NX_ERROR_IO;
        }
    }
    port->active = true;
    return NX_SUCCESS;
}

/** \brief Wait for the uninterruptible physical pulse; timeout is not abort. */
static nx_result_t pulse_result(nx_stm32_flash_state_t* port,
                                nx_time_us_t deadline) {
    do {
        NX_STM32_IO_POLL(3U, port);
    } while ((NX_STM32_FLASH_READ(&port->registers->SR) & FLASH_SR_BSY) != 0U);
    uint32_t status = NX_STM32_FLASH_READ(&port->registers->SR);
    if ((status & NX_FLASH_ERRORS) != 0U || (status & FLASH_SR_EOP) == 0U) {
        return NX_ERROR_IO;
    }
    return nx_deadline_expired(deadline, nx_time_now_us()) ? NX_ERROR_TIMEOUT
                                                           : NX_SUCCESS;
}

/** \brief Clear programming modes and relock only after hardware has settled.
 */
static nx_result_t end(nx_stm32_flash_state_t* port, nx_result_t result) {
    bool withdrawn = control(port, (NX_STM32_FLASH_READ(&port->registers->CR) &
                                    ~NX_FLASH_OWNED_MODES) |
                                       FLASH_CR_LOCK);
    withdrawn = clear_status(port) && withdrawn;
    withdrawn = NX_STM32_FLASH_IRQ_CLEAR() && withdrawn;
    withdrawn = !NX_STM32_FLASH_IRQ_ACTIVE() && withdrawn;
    uint32_t cache = port->registers->ACR;
    port->registers->ACR = cache & ~(uint32_t)(FLASH_ACR_ICEN | FLASH_ACR_DCEN);
    port->registers->ACR |= FLASH_ACR_ICRST | FLASH_ACR_DCRST;
    port->registers->ACR = cache;
    nx_arch_dsb();
    nx_arch_isb();
    /* A failed source/lock/vector proof retains the concrete instance. The
     * generated Platform stop must not reclaim it before hardware recovery. */
    port->active = !withdrawn;
    return withdrawn ? result : NX_ERROR_IO;
}

/** \brief Program x32 words with caller-declared valid programming voltage. */
nx_result_t nx_stm32_flash_program(void* context, uint32_t offset,
                                   const void* data, size_t length,
                                   nx_time_us_t deadline) {
    nx_stm32_flash_state_t* port = context;
    if (!range(port, offset, length) || data == NULL || length == 0U ||
        (offset & 3U) != 0U || (length & 3U) != 0U || port->registers == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    nx_result_t result = begin(port);
    if (result != NX_SUCCESS) {
        return result;
    }
    const uint8_t* input = data;
    for (size_t index = 0U; index < length; index += 4U) {
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            result = NX_ERROR_TIMEOUT;
            break;
        }
        uint32_t word;
        memcpy(&word, input + index, sizeof(word));
        if (word == UINT32_MAX) {
            continue;
        }
        uint32_t current = *(volatile uint32_t*)(port->memory + offset + index);
        if ((word & ~current) != 0U) {
            result = NX_ERROR_IO;
            break;
        }
        /* RM0090 Rev22 3.9.5: EOP only latches with EOPIE=1. NVIC remains
         * disabled throughout the polling lease; end joins its pending bit. */
        if (!clear_status(port) || !NX_STM32_FLASH_IRQ_CLEAR() ||
            !control(port, (NX_STM32_FLASH_READ(&port->registers->CR) &
                            ~(uint32_t)FLASH_CR_PSIZE) |
                               FLASH_CR_PSIZE_1 | FLASH_CR_PG |
                               FLASH_CR_EOPIE)) {
            result = NX_ERROR_IO;
            break;
        }
        NX_STM32_FLASH_STORE(
            (volatile uint32_t*)(port->memory + offset + index), word);
        nx_arch_dsb();
        result = pulse_result(port, deadline);
        if (!control(port, NX_STM32_FLASH_READ(&port->registers->CR) &
                               ~(uint32_t)FLASH_CR_PG))
            result = NX_ERROR_IO;
        if (result != NX_SUCCESS) {
            break;
        }
    }
    result = end(port, result);
    if (result == NX_SUCCESS &&
        nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    return result;
}

/** \brief Erase exact sectors without rounding into an adjacent consumer
 * region. */
nx_result_t nx_stm32_flash_erase(void* context, uint32_t offset, size_t length,
                                 nx_time_us_t deadline) {
    nx_stm32_flash_state_t* port = context;
    if (!range(port, offset, length) || length == 0U ||
        port->registers == NULL) {
        return NX_ERROR_INVALID;
    }
    size_t first = port->geometry->sector_count;
    size_t last = first;
    uint32_t finish = offset + (uint32_t)length;
    for (size_t i = 0U; i < port->geometry->sector_count; ++i) {
        if (port->geometry->sectors[i].offset == offset) {
            first = i;
        }
        if (port->geometry->sectors[i].offset +
                port->geometry->sectors[i].size ==
            finish) {
            last = i;
        }
    }
    if (first > last || last >= port->geometry->sector_count) {
        return NX_ERROR_INVALID;
    }
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    nx_result_t result = begin(port);
    if (result != NX_SUCCESS) {
        return result;
    }
    for (size_t sector = first; sector <= last; ++sector) {
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            result = NX_ERROR_TIMEOUT;
            break;
        }
        if (!clear_status(port) || !NX_STM32_FLASH_IRQ_CLEAR() ||
            !control(port, (NX_STM32_FLASH_READ(&port->registers->CR) &
                            ~(uint32_t)(FLASH_CR_PSIZE | FLASH_CR_SNB)) |
                               FLASH_CR_PSIZE_1 | FLASH_CR_SER |
                               FLASH_CR_EOPIE | ((uint32_t)sector << 3U))) {
            result = NX_ERROR_IO;
            break;
        }
        NX_STM32_FLASH_WRITE(&port->registers->CR,
                             NX_STM32_FLASH_READ(&port->registers->CR) |
                                 FLASH_CR_STRT);
        nx_arch_dsb();
        result = pulse_result(port, deadline);
        if (!control(port, NX_STM32_FLASH_READ(&port->registers->CR) &
                               ~(uint32_t)FLASH_CR_SER))
            result = NX_ERROR_IO;
        if (result != NX_SUCCESS) {
            break;
        }
    }
    result = end(port, result);
    if (result == NX_SUCCESS &&
        nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    return result;
}

/** \brief One shared immutable method table for this execution mode. */
const nx_flash_ops_t nx_stm32_flash_ops = {
    .geometry = nx_stm32_flash_geometry,
    .read = nx_stm32_flash_read,
    .program = nx_stm32_flash_program,
    .erase = nx_stm32_flash_erase,
};
