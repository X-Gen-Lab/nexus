/**
 * \file            flash.c
 * \brief           Exact full-density STM32 Flash geometry and settled pulses
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_basic_cold.h"
#include "stm32f407_provider.h"
#include <string.h>

static nx_result_t recover(void* context);
static nx_result_t end(nx_stm32_flash_state_t* port, nx_result_t result);

/** \brief Physical offsets never silently follow a boot-time bank swap. */
static nx_result_t physical_mapping(const nx_stm32_flash_state_t* port) {
#if defined(STM32F479xx)
    if ((RCC->APB2ENR & RCC_APB2ENR_SYSCFGEN) == 0U)
        return NX_ERROR_STATE;
    if ((SYSCFG->MEMRMP & SYSCFG_MEMRMP_UFB_MODE) != 0U ||
        (port->registers->OPTCR & (FLASH_OPTCR_BFB2 | FLASH_OPTCR_SPRMOD)) !=
            0U)
        return NX_ERROR_UNSUPPORTED;
#else
    (void)port;
#endif
    return NX_SUCCESS;
}

nx_result_t nx_stm32_flash_prepare(nx_stm32_flash_state_t* port,
                                   const nx_flash_geometry_t* geometry,
                                   uint32_t supply_mv) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    uint32_t sector_count = 0U;
    if (port == NULL || geometry == NULL ||
        nx_flash_geometry_sector_count(geometry, &sector_count) != NX_SUCCESS ||
        geometry->base_address != FLASH_BASE || geometry->program_unit != 4U ||
        geometry->program_flags != 0U ||
#if defined(STM32F479xx)
        !(geometry->size == 0x200000U && sector_count == 24U))
#else
        !((geometry->size == 0x80000U && sector_count == 8U) ||
          (geometry->size == 0x100000U && sector_count == 12U)))
#endif
        return NX_ERROR_INVALID;
    if (supply_mv < 2700U || supply_mv > 3600U)
        return NX_ERROR_UNSUPPORTED;
    if (port->active || port->options_owned)
        return NX_ERROR_BUSY;
    uint32_t offset = 0U;
    for (uint32_t i = 0U; i < sector_count; ++i) {
        uint32_t local = i % 12U;
        uint32_t size = local < 4U    ? 0x4000U
                        : local == 4U ? 0x10000U
                                      : 0x20000U;
        nx_flash_sector_t sector;
        if (nx_flash_geometry_sector_at(geometry, i, &sector) != NX_SUCCESS ||
            sector.offset != offset || sector.size != size)
            return NX_ERROR_INVALID;
        offset += size;
    }
    port->registers = FLASH;
    port->geometry = geometry;
    port->memory = (volatile uint8_t*)(uintptr_t)FLASH_BASE;
    port->supply_mv = supply_mv;
    return NX_SUCCESS;
}

nx_result_t nx_stm32_flash_close(nx_stm32_flash_state_t* port) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (port == NULL)
        return NX_ERROR_INVALID;
    if (port->registers == NULL)
        return NX_SUCCESS;
    return recover(port);
}

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
#ifndef NX_STM32_FLASH_STORE_BYTE
#define NX_STM32_FLASH_STORE_BYTE(address, value) (*(address) = (value))
#endif
#ifndef NX_STM32_FLASH_IRQ_IDLE
#define NX_STM32_FLASH_IRQ_IDLE()                                              \
    (NVIC_GetEnableIRQ(FLASH_IRQn) == 0U &&                                    \
     NVIC_GetPendingIRQ(FLASH_IRQn) == 0U)
#endif
#ifndef NX_STM32_FLASH_IRQ_ACTIVE
#define NX_STM32_FLASH_IRQ_ACTIVE() (NVIC_GetActive(FLASH_IRQn) != 0U)
#endif
#ifndef NX_STM32_FLASH_IRQ_ENABLED
#define NX_STM32_FLASH_IRQ_ENABLED() (NVIC_GetEnableIRQ(FLASH_IRQn) != 0U)
#endif
#ifndef NX_STM32_FLASH_HPRE
#define NX_STM32_FLASH_HPRE() (RCC->CFGR & RCC_CFGR_HPRE)
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

#define NX_FLASH_ERRORS                                                        \
    ((uint32_t)(FLASH_SR_OPERR | FLASH_SR_WRPERR | FLASH_SR_PGAERR |           \
                FLASH_SR_PGPERR | FLASH_SR_PGSERR))
#if defined(STM32F479xx)
#undef NX_FLASH_ERRORS
#define NX_FLASH_ERRORS                                                        \
    ((uint32_t)(FLASH_SR_OPERR | FLASH_SR_WRPERR | FLASH_SR_PGAERR |           \
                FLASH_SR_PGPERR | FLASH_SR_PGSERR | FLASH_SR_RDERR))
#endif
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

#if defined(STM32F479xx)
/** \brief Preserve latency while proving each cache ownership transition. */
static bool cache_write(nx_stm32_flash_state_t* port, uint32_t value) {
    NX_STM32_FLASH_WRITE(&port->registers->ACR, value);
    nx_arch_dsb();
    nx_arch_isb();
    return NX_STM32_FLASH_READ(&port->registers->ACR) == value;
}

/** \brief ES0321 2.2.12 requires DCEN0 before any cross-bank write pulse. */
static bool cache_acquire(nx_stm32_flash_state_t* port) {
    uint32_t acr = NX_STM32_FLASH_READ(&port->registers->ACR);
    port->cache_flags = (uint8_t)((acr >> 9U) & 3U);
    port->cache_owned = true;
    return cache_write(port, acr & ~(uint32_t)FLASH_ACR_DCEN);
}

/** \brief A failed cache reset/restore retains the same physical owner. */
static bool cache_release(nx_stm32_flash_state_t* port) {
    if (!port->cache_owned)
        return true;
    uint32_t acr = NX_STM32_FLASH_READ(&port->registers->ACR) &
                   ~(uint32_t)(FLASH_ACR_ICEN | FLASH_ACR_DCEN |
                               FLASH_ACR_ICRST | FLASH_ACR_DCRST);
    if (!cache_write(port, acr) ||
        !cache_write(port, acr | FLASH_ACR_ICRST | FLASH_ACR_DCRST) ||
        !cache_write(port, acr) ||
        !cache_write(port, acr | (uint32_t)port->cache_flags << 9U))
        return false;
    port->cache_owned = false;
    return true;
}
#endif

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
    nx_result_t result = physical_mapping(port);
    if (result != NX_SUCCESS)
        return result;
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
    if (port->active || port->options_owned ||
        (NX_STM32_FLASH_READ(&port->registers->SR) & FLASH_SR_BSY) != 0U ||
        (NX_STM32_FLASH_READ(&port->registers->CR) &
         ~(uint32_t)(FLASH_CR_LOCK | FLASH_CR_PSIZE)) != 0U ||
        !NX_STM32_FLASH_IRQ_IDLE() || NX_STM32_FLASH_IRQ_ACTIVE()) {
        return NX_ERROR_BUSY;
    }
#if defined(STM32F479xx)
    if (port->cache_owned || (NX_STM32_FLASH_READ(&port->registers->ACR) &
                              (FLASH_ACR_ICRST | FLASH_ACR_DCRST)) != 0U)
        return NX_ERROR_BUSY;
#endif
    if (port->supply_mv < 2700U || port->supply_mv > 3600U) {
        return NX_ERROR_UNSUPPORTED;
    }
    nx_result_t result = physical_mapping(port);
    if (result != NX_SUCCESS)
        return result;
    if ((NX_STM32_FLASH_READ(&port->registers->CR) & FLASH_CR_LOCK) != 0U) {
        NX_STM32_FLASH_WRITE(&port->registers->KEYR, 0x45670123U);
        NX_STM32_FLASH_WRITE(&port->registers->KEYR, 0xCDEF89ABU);
        NX_STM32_IO_POLL(3U, port);
        if ((NX_STM32_FLASH_READ(&port->registers->CR) & FLASH_CR_LOCK) != 0U) {
            return NX_ERROR_IO;
        }
    }
    port->active = true;
#if defined(STM32F479xx)
    if (!cache_acquire(port))
        return end(port, NX_ERROR_IO);
#endif
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
#if defined(STM32F479xx)
    withdrawn = cache_release(port) && withdrawn;
#else
    uint32_t cache = port->registers->ACR;
    port->registers->ACR = cache & ~(uint32_t)(FLASH_ACR_ICEN | FLASH_ACR_DCEN);
    port->registers->ACR |= FLASH_ACR_ICRST | FLASH_ACR_DCRST;
    port->registers->ACR = cache;
#endif
    nx_arch_dsb();
    nx_arch_isb();
    /* A failed source/lock/vector proof retains the concrete instance. The
     * generated Platform stop must not reclaim it before hardware recovery. */
    port->active = !withdrawn;
    return withdrawn ? result : NX_ERROR_IO;
}

/** \brief Retry only acquired cleanup; never acknowledge a foreign vector. */
static nx_result_t recover(void* context) {
    nx_stm32_flash_state_t* port = context;
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (!range(port, 0U, 0U))
        return NX_ERROR_STATE;
    if ((NX_STM32_FLASH_READ(&port->registers->SR) & FLASH_SR_BSY) != 0U)
        return NX_ERROR_BUSY;
    if (!port->active)
        return port->options_owned ? NX_ERROR_STATE : NX_SUCCESS;
    uint32_t allowed = NX_FLASH_OWNED_MODES | FLASH_CR_LOCK | FLASH_CR_PSIZE;
    if (NX_STM32_FLASH_IRQ_ENABLED() || NX_STM32_FLASH_IRQ_ACTIVE() ||
        (NX_STM32_FLASH_READ(&port->registers->CR) & ~allowed) != 0U)
        return NX_ERROR_BUSY;
    bool locked = true;
    if (port->options_owned) {
        uint32_t value = (NX_STM32_FLASH_READ(&port->registers->OPTCR) &
                          ~(uint32_t)FLASH_OPTCR_OPTSTRT) |
                         FLASH_OPTCR_OPTLOCK;
        NX_STM32_FLASH_WRITE(&port->registers->OPTCR, value);
        nx_arch_dsb();
        locked = NX_STM32_FLASH_READ(&port->registers->OPTCR) == value;
        if (locked)
            port->options_owned = false;
    }
    nx_result_t result = end(port, NX_SUCCESS);
    if (!locked) {
        port->active = true;
        return NX_ERROR_IO;
    }
    return result;
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
    uint32_t sector_count, first, limit;
    uint32_t finish = offset + (uint32_t)length;
    if (nx_flash_geometry_sector_count(port->geometry, &sector_count) !=
            NX_SUCCESS ||
        nx_flash_geometry_sector_index(port->geometry, offset, &first) !=
            NX_SUCCESS)
        return NX_ERROR_INVALID;
    if (finish == port->geometry->size)
        limit = sector_count;
    else if (nx_flash_geometry_sector_index(port->geometry, finish, &limit) !=
             NX_SUCCESS)
        return NX_ERROR_INVALID;
    if (first >= limit)
        return NX_ERROR_INVALID;
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    nx_result_t result = begin(port);
    if (result != NX_SUCCESS) {
        return result;
    }
    for (uint32_t sector = first; sector < limit; ++sector) {
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            result = NX_ERROR_TIMEOUT;
            break;
        }
        /* RM0386 bank two uses SNB16..27 for physical sectors12..23.
         * Keep the same encoding as the reviewed official FLASH_Erase_Sector.
         * SNB12..15 are reserved and must never be issued. */
        uint32_t encoded = sector > 11U ? sector + 4U : sector;
        if (!clear_status(port) || !NX_STM32_FLASH_IRQ_CLEAR() ||
            !control(port, (NX_STM32_FLASH_READ(&port->registers->CR) &
                            ~(uint32_t)(FLASH_CR_PSIZE | FLASH_CR_SNB)) |
                               FLASH_CR_PSIZE_1 | FLASH_CR_SER |
                               FLASH_CR_EOPIE | (encoded << 3U))) {
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
    .recover = recover,
};

#define NX_FLASH_OTP_ADDRESS  UINT32_C(0x1fff7800)
#define NX_FLASH_OTP_BYTES    UINT32_C(512)
#define NX_FLASH_OTP_BLOCK    UINT32_C(32)
#define NX_FLASH_OTP_BLOCKS   UINT32_C(16)
#define NX_FLASH_MAINTAIN_ALL UINT32_C(31)

/* The existing exact pulse owner deliberately also owns OTP and OPTCR. HAL's
 * global pFlash and hidden 50-second tick wait cannot establish this owner's
 * fresh EOP, disabled-vector join or retained failed-relock contract. RM0090
 * 3.7/3.8 require the same physical pulse ordering used above; no second HAL
 * transport, reset engine or register-script interpreter is introduced. */
static volatile uint8_t* otp_memory(void) {
    return (volatile uint8_t*)(uintptr_t)NX_FLASH_OTP_ADDRESS;
}

static nx_result_t maintenance_idle(nx_stm32_flash_state_t* port) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    uint32_t count;
    if (!range(port, 0U, 0U) ||
        nx_flash_geometry_sector_count(port->geometry, &count) != NX_SUCCESS)
        return NX_ERROR_STATE;
    /* The existing option/OTP contract has one OPTCR protection bitmap.
     * Two-bank parts require their independently reviewed OPTCR1 contract. */
    if (count != 8U && count != 12U)
        return NX_ERROR_UNSUPPORTED;
    return port->active || (NX_STM32_FLASH_READ(&port->registers->SR) &
                            FLASH_SR_BSY) != 0U
               ? NX_ERROR_BUSY
               : NX_SUCCESS;
}

static nx_flash_read_protection_t protection(uint32_t options) {
    uint32_t encoded = (uint32_t)((options & FLASH_OPTCR_RDP) >> 8U);
    return encoded == 0xaaU   ? NX_FLASH_READ_OPEN
           : encoded == 0xccU ? NX_FLASH_READ_PERMANENT
                              : NX_FLASH_READ_PROTECTED;
}

static uint32_t sector_mask(const nx_stm32_flash_state_t* port) {
    uint32_t count;
    if (nx_flash_geometry_sector_count(port->geometry, &count) != NX_SUCCESS ||
        count >= 32U)
        return 0U;
    return (UINT32_C(1) << count) - 1U;
}

static bool acknowledged(uint32_t effects, uint32_t required) {
    return (effects & ~NX_FLASH_MAINTAIN_ALL) == 0U &&
           (effects & required) == required;
}

/* Reject wrap and all provider/control/ROM destinations before any copy. The
 * host model keeps ordinary pointers outside the MCU's 32-bit apertures. */
static bool maintenance_buffer(const nx_stm32_flash_state_t* port,
                               const void* pointer, size_t size,
                               bool destination) {
    uintptr_t address = (uintptr_t)pointer;
    if (pointer == NULL || size > UINTPTR_MAX - address)
        return false;
    uintptr_t last = address + size;
    uintptr_t owner = (uintptr_t)port;
    uintptr_t registers = (uintptr_t)port->registers;
    uintptr_t geometry = (uintptr_t)port->geometry;
    uintptr_t spans = (uintptr_t)port->geometry->erase_spans;
    bool system_overlap = UINT32_C(0xe0000000) < last;
#if UINTPTR_MAX > UINT32_MAX
    system_overlap = system_overlap && address < UINT64_C(0x100000000);
#endif
    return !(address < owner + sizeof(*port) && owner < last) &&
           !(address < registers + sizeof(*port->registers) &&
             registers < last) &&
           !(address < geometry + sizeof(*port->geometry) && geometry < last) &&
           !(address < spans + port->geometry->erase_span_count *
                                   sizeof(*port->geometry->erase_spans) &&
             spans < last) &&
           !(address < UINT32_C(0x1fff7a10) && NX_FLASH_OTP_ADDRESS < last) &&
           !(address < UINT32_C(0x60000000) && UINT32_C(0x40000000) < last) &&
           !system_overlap &&
           !(destination && address < UINT32_C(0x20000000) &&
             UINT32_C(0x08000000) < last);
}

/** \brief RM0090 BOR bits implement the reviewed supply-range profile. */
static bool brownout_bits(nx_flash_brownout_t profile, uint32_t* bits) {
    switch (profile) {
        case NX_FLASH_BROWNOUT_DISABLED:
            *bits = 3U << 2U;
            return true;
        case NX_FLASH_BROWNOUT_SUPPLY_2V1:
            *bits = 2U << 2U;
            return true;
        case NX_FLASH_BROWNOUT_SUPPLY_2V4:
            *bits = 1U << 2U;
            return true;
        case NX_FLASH_BROWNOUT_SUPPLY_2V7:
            *bits = 0U;
            return true;
        default:
            return false;
    }
}

/** \brief Decode hardware independently of public semantic profile values. */
static nx_flash_brownout_t brownout_profile(uint32_t options) {
    static const nx_flash_brownout_t profiles[] = {
        NX_FLASH_BROWNOUT_SUPPLY_2V7, NX_FLASH_BROWNOUT_SUPPLY_2V4,
        NX_FLASH_BROWNOUT_SUPPLY_2V1, NX_FLASH_BROWNOUT_DISABLED};
    return profiles[(options & FLASH_OPTCR_BOR_LEV) >> 2U];
}

static nx_result_t
maintenance_snapshot(void* context, nx_flash_maintenance_snapshot_t* snapshot) {
    nx_stm32_flash_state_t* port = context;
    nx_result_t result = maintenance_idle(port);
    if (result != NX_SUCCESS)
        return result;
    if (!maintenance_buffer(port, snapshot, sizeof(*snapshot), true))
        return NX_ERROR_INVALID;
    uint32_t locked = 0U;
    for (uint32_t block = 0U; block < NX_FLASH_OTP_BLOCKS; ++block) {
        uint8_t value = otp_memory()[NX_FLASH_OTP_BYTES + block];
        if (value != 0xffU && value != 0U)
            return NX_ERROR_IO;
        if (value == 0U)
            locked |= UINT32_C(1) << block;
    }
    uint32_t sector_count;
    if (nx_flash_geometry_sector_count(port->geometry, &sector_count) !=
        NX_SUCCESS)
        return NX_ERROR_INVALID;
    uint32_t options = NX_STM32_FLASH_READ(&port->registers->OPTCR);
    *snapshot = (nx_flash_maintenance_snapshot_t){
        .options =
            {
                .read_protection = protection(options),
                .write_protected_sectors =
                    (~(options >> 16U)) & sector_mask(port),
                .brownout = brownout_profile(options),
                .hardware_watchdog = (options & FLASH_OPTCR_WDG_SW) == 0U,
                .reset_on_stop = (options & FLASH_OPTCR_nRST_STOP) == 0U,
                .reset_on_standby = (options & FLASH_OPTCR_nRST_STDBY) == 0U,
            },
        .otp_bytes = NX_FLASH_OTP_BYTES,
        .otp_block_bytes = NX_FLASH_OTP_BLOCK,
        .otp_locked_blocks = locked,
        .sector_count = sector_count,
        .power_on_reset_required = port->power_on_reset_required,
    };
    return NX_SUCCESS;
}

static nx_result_t otp_read(void* context, uint32_t offset, void* data,
                            size_t length) {
    nx_stm32_flash_state_t* port = context;
    nx_result_t result = maintenance_idle(port);
    if (result != NX_SUCCESS)
        return result;
    if (offset > NX_FLASH_OTP_BYTES || length > NX_FLASH_OTP_BYTES - offset ||
        (length != 0U && !maintenance_buffer(port, data, length, true)))
        return NX_ERROR_INVALID;
    uint8_t* output = data;
    for (size_t i = 0U; i < length; ++i)
        output[i] = otp_memory()[offset + i];
    return NX_SUCCESS;
}

static nx_result_t otp_program(void* context, uint32_t offset, const void* data,
                               size_t length, uint32_t effects,
                               nx_time_us_t deadline) {
    nx_stm32_flash_state_t* port = context;
    nx_result_t result = maintenance_idle(port);
    if (result != NX_SUCCESS)
        return result;
    if (!acknowledged(effects, NX_FLASH_MAINTAIN_OTP_PROGRAM) ||
        deadline == NX_DEADLINE_NEVER || offset >= NX_FLASH_OTP_BYTES ||
        length == 0U || length > NX_FLASH_OTP_BYTES - offset ||
        (offset & 3U) != 0U || (length & 3U) != 0U ||
        !maintenance_buffer(port, data, length, false))
        return NX_ERROR_INVALID;
    const uint8_t* input = data;
    uint32_t last_block = (offset + (uint32_t)length - 1U) / NX_FLASH_OTP_BLOCK;
    for (uint32_t block = offset / NX_FLASH_OTP_BLOCK; block <= last_block;
         ++block) {
        uint8_t lock = otp_memory()[NX_FLASH_OTP_BYTES + block];
        if (lock != 0xffU)
            return lock == 0U ? NX_ERROR_BUSY : NX_ERROR_IO;
    }
    for (size_t i = 0U; i < length; ++i)
        if ((input[i] & (uint8_t)~otp_memory()[offset + i]) != 0U)
            return NX_ERROR_IO;
    if (nx_deadline_expired(deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    result = begin(port);
    if (result != NX_SUCCESS)
        return result;
    for (size_t i = 0U; i < length; i += sizeof(uint32_t)) {
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            result = NX_ERROR_TIMEOUT;
            break;
        }
        uint32_t word;
        memcpy(&word, input + i, sizeof(word));
        volatile uint32_t* target =
            (volatile uint32_t*)(otp_memory() + offset + i);
        if (word == *target)
            continue;
        if (!clear_status(port) || !NX_STM32_FLASH_IRQ_CLEAR() ||
            !control(port, (NX_STM32_FLASH_READ(&port->registers->CR) &
                            ~(uint32_t)FLASH_CR_PSIZE) |
                               FLASH_CR_PSIZE_1 | FLASH_CR_PG |
                               FLASH_CR_EOPIE)) {
            result = NX_ERROR_IO;
            break;
        }
        NX_STM32_FLASH_STORE(target, word);
        nx_arch_dsb();
        result = pulse_result(port, deadline);
        if (!control(port, NX_STM32_FLASH_READ(&port->registers->CR) &
                               ~(uint32_t)FLASH_CR_PG) ||
            *target != word)
            result = NX_ERROR_IO;
        if (result != NX_SUCCESS)
            break;
    }
    return end(port, result);
}

static nx_result_t otp_lock(void* context, uint32_t block, uint32_t effects,
                            nx_time_us_t deadline) {
    nx_stm32_flash_state_t* port = context;
    nx_result_t result = maintenance_idle(port);
    if (result != NX_SUCCESS)
        return result;
    if (block >= NX_FLASH_OTP_BLOCKS || deadline == NX_DEADLINE_NEVER ||
        !acknowledged(effects, NX_FLASH_MAINTAIN_OTP_LOCK))
        return NX_ERROR_INVALID;
    volatile uint8_t* target = otp_memory() + NX_FLASH_OTP_BYTES + block;
    if (*target != 0xffU && *target != 0U)
        return NX_ERROR_IO;
    if (nx_deadline_expired(deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    if (*target == 0U)
        return NX_SUCCESS;
    result = begin(port);
    if (result != NX_SUCCESS)
        return result;
    if (!clear_status(port) || !NX_STM32_FLASH_IRQ_CLEAR() ||
        !control(port, (NX_STM32_FLASH_READ(&port->registers->CR) &
                        ~(uint32_t)FLASH_CR_PSIZE) |
                           FLASH_CR_PG | FLASH_CR_EOPIE))
        return end(port, NX_ERROR_IO);
    /* RM0090 table 19: the lock block is sixteen independent bytes. A word
     * store would permanently lock three neighbours, so this pulse is x8. */
    NX_STM32_FLASH_STORE_BYTE(target, 0U);
    nx_arch_dsb();
    result = pulse_result(port, deadline);
    if (*target != 0U)
        result = NX_ERROR_IO;
    return end(port, result);
}

static nx_result_t set_options(void* context, const nx_flash_options_t* desired,
                               uint32_t effects, nx_time_us_t deadline) {
    nx_stm32_flash_state_t* port = context;
    nx_result_t result = maintenance_idle(port);
    if (result != NX_SUCCESS)
        return result;
    uint32_t bor = 0U;
    if (!maintenance_buffer(port, desired, sizeof(*desired), false) ||
        deadline == NX_DEADLINE_NEVER ||
        !brownout_bits(desired->brownout, &bor) ||
        (desired->write_protected_sectors & ~sector_mask(port)) != 0U ||
        (desired->read_protection != NX_FLASH_READ_OPEN &&
         desired->read_protection != NX_FLASH_READ_PROTECTED &&
         desired->read_protection != NX_FLASH_READ_PERMANENT))
        return NX_ERROR_INVALID;
    uint32_t before = NX_STM32_FLASH_READ(&port->registers->OPTCR);
    /* ES0182 Rev19 2.2.9: avoid untracked consecutive BOR writes at HPRE>2.
     * Unchanged BOR does not restrict other option maintenance. */
    if ((before & FLASH_OPTCR_BOR_LEV) != bor &&
        NX_STM32_FLASH_HPRE() >= RCC_CFGR_HPRE_DIV4)
        return NX_ERROR_UNSUPPORTED;
    if ((before & (FLASH_OPTCR_OPTLOCK | FLASH_OPTCR_OPTSTRT)) !=
        FLASH_OPTCR_OPTLOCK)
        return NX_ERROR_BUSY;
    nx_flash_read_protection_t previous = protection(before);
    if (previous == NX_FLASH_READ_PERMANENT)
        return NX_ERROR_UNSUPPORTED;
    uint32_t required = NX_FLASH_MAINTAIN_OPTIONS;
    if (previous == NX_FLASH_READ_PROTECTED &&
        desired->read_protection == NX_FLASH_READ_OPEN)
        required |= NX_FLASH_MAINTAIN_MASS_ERASE;
    if (desired->read_protection == NX_FLASH_READ_PERMANENT)
        required |= NX_FLASH_MAINTAIN_PERMANENT;
    if (!acknowledged(effects, required))
        return NX_ERROR_INVALID;
    if (nx_deadline_expired(deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    uint32_t rdp = desired->read_protection == NX_FLASH_READ_OPEN ? 0xaaU
                   : desired->read_protection == NX_FLASH_READ_PERMANENT
                       ? 0xccU
                       : 0x55U;
    uint32_t mask = (sector_mask(port) << 16U) | FLASH_OPTCR_RDP |
                    FLASH_OPTCR_BOR_LEV | FLASH_OPTCR_WDG_SW |
                    FLASH_OPTCR_nRST_STOP | FLASH_OPTCR_nRST_STDBY |
                    FLASH_OPTCR_OPTLOCK | FLASH_OPTCR_OPTSTRT;
    uint32_t value =
        (before & ~mask) | (rdp << 8U) |
        ((~desired->write_protected_sectors & sector_mask(port)) << 16U) | bor |
        (desired->hardware_watchdog ? 0U : FLASH_OPTCR_WDG_SW) |
        (desired->reset_on_stop ? 0U : FLASH_OPTCR_nRST_STOP) |
        (desired->reset_on_standby ? 0U : FLASH_OPTCR_nRST_STDBY);
    if ((before & ~(uint32_t)FLASH_OPTCR_OPTLOCK) == value)
        return NX_SUCCESS;
    result = begin(port);
    if (result != NX_SUCCESS)
        return result;
    port->options_owned = true;
    NX_STM32_FLASH_WRITE(&port->registers->OPTKEYR, UINT32_C(0x08192a3b));
    NX_STM32_FLASH_WRITE(&port->registers->OPTKEYR, UINT32_C(0x4c5d6e7f));
    nx_arch_dsb();
    if ((NX_STM32_FLASH_READ(&port->registers->OPTCR) & FLASH_OPTCR_OPTLOCK) !=
            0U ||
        !clear_status(port) || !NX_STM32_FLASH_IRQ_CLEAR() ||
        !control(port,
                 NX_STM32_FLASH_READ(&port->registers->CR) | FLASH_CR_EOPIE)) {
        result = NX_ERROR_IO;
    } else {
        NX_STM32_FLASH_WRITE(&port->registers->OPTCR, value);
        nx_arch_dsb();
        if (NX_STM32_FLASH_READ(&port->registers->OPTCR) != value) {
            result = NX_ERROR_IO;
        } else {
            /* Once this start is issued the entire option image may have
             * changed, including an irreversible protection transition. Never
             * replay it or claim a timeout restored the prior image. */
            port->power_on_reset_required = true;
            NX_STM32_FLASH_WRITE(&port->registers->OPTCR,
                                 value | FLASH_OPTCR_OPTSTRT);
            nx_arch_dsb();
            result = pulse_result(port, deadline);
            if ((NX_STM32_FLASH_READ(&port->registers->OPTCR) &
                 ~(uint32_t)(FLASH_OPTCR_OPTSTRT | FLASH_OPTCR_OPTLOCK)) !=
                value)
                result = NX_ERROR_IO;
        }
    }
    NX_STM32_FLASH_WRITE(&port->registers->OPTCR,
                         (NX_STM32_FLASH_READ(&port->registers->OPTCR) &
                          ~(uint32_t)FLASH_OPTCR_OPTSTRT) |
                             FLASH_OPTCR_OPTLOCK);
    nx_arch_dsb();
    bool locked = (NX_STM32_FLASH_READ(&port->registers->OPTCR) &
                   FLASH_OPTCR_OPTLOCK) != 0U;
    if (locked)
        port->options_owned = false;
    result = end(port, result);
    if (!locked) {
        port->active = true;
        return NX_ERROR_IO;
    }
    return result;
}

const nx_flash_maintenance_ops_t nx_stm32_flash_maintenance_ops = {
    .snapshot = maintenance_snapshot,
    .otp_read = otp_read,
    .otp_program = otp_program,
    .otp_lock = otp_lock,
    .set_options = set_options,
};
