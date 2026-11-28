/**
 * \file            flash.c
 * \brief           STM32F103xB FPEC halfword program and page erase
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/flash/instance.h"
#include "nexus/arch/arch.h"
#include "stm32f1xx.h"

#if !defined(STM32F103xB)
#error "The selected Flash controller requires the STM32F103xB geometry"
#endif

#ifndef NX_F1_FLASH_REGISTERS
#define NX_F1_FLASH_REGISTERS             FLASH
#define NX_F1_FLASH_MEMORY                ((volatile uint8_t*)(uintptr_t)FLASH_BASE)
#define NX_F1_FLASH_READ(reg)             (*(reg))
#define NX_F1_FLASH_WRITE(reg, value)     (*(reg) = (value))
#define NX_F1_FLASH_STORE(address, value) (*(address) = (value))
#define NX_F1_FLASH_IRQ_IDLE()                                                 \
    (NVIC_GetEnableIRQ(FLASH_IRQn) == 0U &&                                    \
     NVIC_GetActive(FLASH_IRQn) == 0U && NVIC_GetPendingIRQ(FLASH_IRQn) == 0U)
#define NX_F1_FLASH_CLOCK() (RCC->CR)
#endif

#define NX_F1_FLASH_PAGE_BYTES    1024U
#define NX_F1_FLASH_FLAGS         (FLASH_SR_PGERR | FLASH_SR_WRPRTERR | FLASH_SR_EOP)
#define NX_F1_FLASH_OWNED_CONTROL (FLASH_CR_PG | FLASH_CR_PER | FLASH_CR_LOCK)

static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

static bool busy(void) {
    return (NX_F1_FLASH_READ(&NX_F1_FLASH_REGISTERS->SR) & FLASH_SR_BSY) != 0U;
}

static bool locked_and_quiet(void) {
    return !busy() && NX_F1_FLASH_IRQ_IDLE() &&
           NX_F1_FLASH_READ(&NX_F1_FLASH_REGISTERS->CR) == FLASH_CR_LOCK;
}

nx_result_t nx_stm32f1_flash_prepare(nx_stm32f1_flash_state_t* port,
                                     const nx_flash_geometry_t* geometry,
                                     uint32_t supply_mv) {
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    uint32_t pages;
    if (port == NULL ||
        nx_flash_geometry_sector_count(geometry, &pages) != NX_SUCCESS ||
        geometry->base_address != FLASH_BASE ||
        (geometry->size != 64U * NX_F1_FLASH_PAGE_BYTES &&
         geometry->size != 128U * NX_F1_FLASH_PAGE_BYTES) ||
        geometry->program_unit != 2U || geometry->program_flags != 0U ||
        geometry->erase_span_count != 1U ||
        geometry->erase_spans[0].erase_bytes != NX_F1_FLASH_PAGE_BYTES) {
        return NX_ERROR_INVALID;
    }
    /* DS5319 Rev 20, Table 28 specifies an inclusive 2.0–3.6 V range. */
    if (supply_mv < 2000U || supply_mv > 3600U) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (port->geometry != NULL || port->phase != NX_STM32F1_FLASH_IDLE ||
        !locked_and_quiet()) {
        return NX_ERROR_BUSY;
    }
    port->geometry = geometry;
    return NX_SUCCESS;
}

static bool control(uint32_t value) {
    NX_F1_FLASH_WRITE(&NX_F1_FLASH_REGISTERS->CR, value);
    nx_arch_dsb();
    return NX_F1_FLASH_READ(&NX_F1_FLASH_REGISTERS->CR) == value;
}

/** \brief Prove a new completion epoch and keep IRQ sources disabled. */
static bool clear_status(void) {
    NX_F1_FLASH_WRITE(&NX_F1_FLASH_REGISTERS->SR, NX_F1_FLASH_FLAGS);
    nx_arch_dsb();
    return (NX_F1_FLASH_READ(&NX_F1_FLASH_REGISTERS->SR) &
            (NX_F1_FLASH_FLAGS | FLASH_SR_BSY)) == 0U;
}

/** \brief Withdraw only this polling owner's bits after actual idle. */
static nx_result_t withdraw(nx_stm32f1_flash_state_t* port) {
    if (busy() || !NX_F1_FLASH_IRQ_IDLE() ||
        (NX_F1_FLASH_READ(&NX_F1_FLASH_REGISTERS->CR) &
         ~(uint32_t)NX_F1_FLASH_OWNED_CONTROL) != 0U) {
        return NX_ERROR_BUSY;
    }
    if (!control(FLASH_CR_LOCK) || !clear_status() || !locked_and_quiet()) {
        return NX_ERROR_IO;
    }
    nx_arch_isb();
    port->phase = NX_STM32F1_FLASH_IDLE;
    return NX_SUCCESS;
}

static nx_result_t recover(void* context) {
    nx_stm32f1_flash_state_t* port = context;
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (port == NULL || port->geometry == NULL) {
        return NX_ERROR_STATE;
    }
    if (port->phase == NX_STM32F1_FLASH_RETAINED) {
        return withdraw(port);
    }
    return port->phase == NX_STM32F1_FLASH_IDLE && locked_and_quiet()
               ? NX_SUCCESS
               : NX_ERROR_BUSY;
}

nx_result_t nx_stm32f1_flash_close(nx_stm32f1_flash_state_t* port) {
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->geometry == NULL) {
        return NX_SUCCESS;
    }
    nx_result_t result = recover(port);
    if (result == NX_SUCCESS) {
        port->geometry = NULL;
    }
    return result;
}

static const nx_flash_geometry_t* geometry(const void* context) {
    const nx_stm32f1_flash_state_t* port = context;
    return port != NULL ? port->geometry : NULL;
}

static bool range(const nx_stm32f1_flash_state_t* port, uint32_t offset,
                  size_t length) {
    const nx_flash_geometry_t* layout = geometry(port);
    return layout != NULL && offset <= layout->size &&
           length <= layout->size - offset;
}

static bool overlaps(const void* data, size_t length, const volatile void* ref,
                     size_t ref_length) {
    uintptr_t start = (uintptr_t)data;
    uintptr_t other = (uintptr_t)ref;
    return length != 0U && start < other + ref_length && other < start + length;
}

static bool buffer(const nx_stm32f1_flash_state_t* port, const void* data,
                   size_t length) {
    return length == 0U ||
           (data != NULL && length <= UINTPTR_MAX - (uintptr_t)data &&
            !overlaps(data, length, port, sizeof(*port)) &&
            !overlaps(data, length, port->geometry, sizeof(*port->geometry)) &&
            !overlaps(data, length, port->geometry->erase_spans,
                      sizeof(*port->geometry->erase_spans)) &&
            !overlaps(data, length, NX_F1_FLASH_REGISTERS,
                      sizeof(*NX_F1_FLASH_REGISTERS)));
}

static nx_result_t read(const void* context, uint32_t offset, void* data,
                        size_t length) {
    const nx_stm32f1_flash_state_t* port = context;
    if (!range(port, offset, length) || !buffer(port, data, length) ||
        overlaps(data, length, NX_F1_FLASH_MEMORY, port->geometry->size)) {
        return NX_ERROR_INVALID;
    }
    if (port->phase == NX_STM32F1_FLASH_RETAINED) {
        return NX_ERROR_STATE;
    }
    if (port->phase != NX_STM32F1_FLASH_IDLE || busy()) {
        return NX_ERROR_BUSY;
    }
    uint8_t* output = data;
    for (size_t index = 0U; index < length; ++index) {
        output[index] = NX_F1_FLASH_MEMORY[offset + index];
    }
    return NX_SUCCESS;
}

static bool expired(nx_time_us_t deadline) {
    return nx_deadline_expired(deadline, nx_time_now_us());
}

/** \brief An accepted pulse is non-preemptible, including after its deadline.
 */
static nx_result_t pulse(nx_time_us_t deadline) {
    uint32_t status;
    do {
        status = NX_F1_FLASH_READ(&NX_F1_FLASH_REGISTERS->SR);
    } while ((status & FLASH_SR_BSY) != 0U);
    if ((status & NX_F1_FLASH_FLAGS) != FLASH_SR_EOP) {
        return NX_ERROR_IO;
    }
    return expired(deadline) ? NX_ERROR_TIMEOUT : NX_SUCCESS;
}

static nx_result_t finish(nx_stm32f1_flash_state_t* port, nx_result_t result,
                          nx_time_us_t deadline) {
    port->phase = NX_STM32F1_FLASH_RETAINED;
    if (withdraw(port) != NX_SUCCESS) {
        return NX_ERROR_IO;
    }
    return result == NX_SUCCESS && expired(deadline) ? NX_ERROR_TIMEOUT
                                                     : result;
}

static bool clock_ready(void) {
    const uint32_t needed = RCC_CR_HSION | RCC_CR_HSIRDY;
    return (NX_F1_FLASH_CLOCK() & needed) == needed;
}

static nx_result_t begin(nx_stm32f1_flash_state_t* port,
                         nx_time_us_t deadline) {
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->phase == NX_STM32F1_FLASH_RETAINED || !clock_ready()) {
        return NX_ERROR_STATE;
    }
    if (deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (expired(deadline)) {
        return NX_ERROR_TIMEOUT;
    }
    if (port->phase != NX_STM32F1_FLASH_IDLE || !locked_and_quiet()) {
        return NX_ERROR_BUSY;
    }
    port->phase = NX_STM32F1_FLASH_ACTIVE;
    NX_F1_FLASH_WRITE(&NX_F1_FLASH_REGISTERS->KEYR, 0x45670123U);
    NX_F1_FLASH_WRITE(&NX_F1_FLASH_REGISTERS->KEYR, 0xcdef89abU);
    nx_arch_dsb();
    if (NX_F1_FLASH_READ(&NX_F1_FLASH_REGISTERS->CR) != 0U) {
        return finish(port, NX_ERROR_IO, deadline);
    }
    return NX_SUCCESS;
}

static nx_result_t program(void* context, uint32_t offset, const void* data,
                           size_t length, nx_time_us_t deadline) {
    nx_stm32f1_flash_state_t* port = context;
    if (!range(port, offset, length) || length == 0U ||
        ((offset | length) & 1U) != 0U || !buffer(port, data, length) ||
        overlaps(data, length, NX_F1_FLASH_MEMORY + offset, length)) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = begin(port, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    const uint8_t* input = data;
    for (size_t index = 0U; index < length; index += 2U) {
        if (expired(deadline)) {
            result = NX_ERROR_TIMEOUT;
            break;
        }
        uint16_t value = (uint16_t)((uint16_t)input[index] |
                                    (uint16_t)input[index + 1U] << 8U);
        if (value == UINT16_MAX) {
            continue;
        }
        volatile uint16_t* address =
            (volatile uint16_t*)(NX_F1_FLASH_MEMORY + offset + index);
        /* PM0075 §2.3.3 permits a zero pulse on a non-erased halfword. */
        if ((*address != UINT16_MAX && value != 0U) || !clock_ready() ||
            !clear_status() || !control(FLASH_CR_PG)) {
            result = NX_ERROR_IO;
            break;
        }
        if (expired(deadline)) {
            result = NX_ERROR_TIMEOUT;
            break;
        }
        NX_F1_FLASH_STORE(address, value);
        nx_arch_dsb();
        result = pulse(deadline);
        if (result == NX_SUCCESS && *address != value) {
            result = NX_ERROR_IO;
        }
        if (result != NX_SUCCESS) {
            break;
        }
    }
    return finish(port, result, deadline);
}

static nx_result_t erase(void* context, uint32_t offset, size_t length,
                         nx_time_us_t deadline) {
    nx_stm32f1_flash_state_t* port = context;
    if (!range(port, offset, length) || length == 0U ||
        offset % NX_F1_FLASH_PAGE_BYTES != 0U ||
        length % NX_F1_FLASH_PAGE_BYTES != 0U) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = begin(port, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    for (uint32_t page = offset; page < offset + length;
         page += NX_F1_FLASH_PAGE_BYTES) {
        if (!clock_ready() || !clear_status() || !control(FLASH_CR_PER)) {
            result = NX_ERROR_IO;
            break;
        }
        uint32_t address = port->geometry->base_address + page;
        NX_F1_FLASH_WRITE(&NX_F1_FLASH_REGISTERS->AR, address);
        nx_arch_dsb();
        if (NX_F1_FLASH_READ(&NX_F1_FLASH_REGISTERS->AR) != address) {
            result = NX_ERROR_IO;
            break;
        }
        if (expired(deadline)) {
            result = NX_ERROR_TIMEOUT;
            break;
        }
        NX_F1_FLASH_WRITE(&NX_F1_FLASH_REGISTERS->CR,
                          FLASH_CR_PER | FLASH_CR_STRT);
        nx_arch_dsb();
        /* ES096 §2.2.11: BSY can be low on the first bus cycle after STRT. */
        (void)NX_F1_FLASH_READ(&NX_F1_FLASH_REGISTERS->CR);
        nx_arch_dsb();
        result = pulse(deadline);
        if (result != NX_SUCCESS) {
            break;
        }
        for (uint32_t index = 0U; index < NX_F1_FLASH_PAGE_BYTES; ++index) {
            if (NX_F1_FLASH_MEMORY[page + index] != UINT8_MAX) {
                result = NX_ERROR_IO;
                break;
            }
        }
        if (result != NX_SUCCESS || expired(deadline)) {
            break;
        }
    }
    return finish(port, result, deadline);
}

const nx_flash_ops_t nx_stm32f1_flash_ops = {geometry, read, program, erase,
                                             recover};
