/**
 * \file            flash.c
 * \brief           G474RE DBANK1 full physical pages and double-word pulses
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32g474_acquisition.h"
#include <string.h>
#define PAGE(n)                                                                \
    { (n) * 2048U, 2048U }
#define PAGES16(n)                                                             \
    PAGE(n), PAGE((n) + 1U), PAGE((n) + 2U), PAGE((n) + 3U), PAGE((n) + 4U),   \
        PAGE((n) + 5U), PAGE((n) + 6U), PAGE((n) + 7U), PAGE((n) + 8U),        \
        PAGE((n) + 9U), PAGE((n) + 10U), PAGE((n) + 11U), PAGE((n) + 12U),     \
        PAGE((n) + 13U), PAGE((n) + 14U), PAGE((n) + 15U)
static const nx_flash_sector_t s_sectors[] = {
    PAGES16(0U),   PAGES16(16U),  PAGES16(32U),  PAGES16(48U),
    PAGES16(64U),  PAGES16(80U),  PAGES16(96U),  PAGES16(112U),
    PAGES16(128U), PAGES16(144U), PAGES16(160U), PAGES16(176U),
    PAGES16(192U), PAGES16(208U), PAGES16(224U), PAGES16(240U)};
const nx_flash_geometry_t g_nx_stm32g474_flash_geometry = {
    .base_address = 0x08000000U,
    .size = 524288U,
    .program_unit = 8U,
    .sectors = s_sectors,
    .sector_count = 256U,
    .program_flags = NX_FLASH_PROGRAM_REQUIRES_FRESH_ERASE};
#define PULSE_EVENTS (FLASH_CR_EOPIE | FLASH_CR_ERRIE)
#define ERRORS                                                                 \
    ((uint32_t)(FLASH_SR_OPERR | FLASH_SR_PROGERR | FLASH_SR_WRPERR |          \
                FLASH_SR_PGAERR | FLASH_SR_SIZERR | FLASH_SR_PGSERR |          \
                FLASH_SR_MISERR | FLASH_SR_FASTERR | FLASH_SR_RDERR |          \
                FLASH_SR_OPTVERR))
static bool range(const nx_stm32g474_flash_state_t* p, uint32_t offset,
                  size_t length) {
    return p != NULL && p->registers != NULL && p->memory != NULL &&
           offset <= 524288U && length <= 524288U - offset;
}
static bool overlap(const void* a, size_t as, const void* b, size_t bs) {
    return (uintptr_t)a < (uintptr_t)b + bs && (uintptr_t)b < (uintptr_t)a + as;
}
static bool protected(const nx_stm32g474_flash_state_t* p, const void* data,
                      size_t length) {
    return (uintptr_t)data > UINTPTR_MAX - length ||
           overlap(data, length, p, sizeof(*p)) ||
           overlap(data, length, p->registers, sizeof(*p->registers)) ||
           overlap(data, length, (const void*)p->memory, 524288U);
}
/** \brief Observe bank mapping only through the owned, live SYSCFG gate. */
static nx_result_t mapping(const nx_stm32g474_flash_state_t* p) {
    if (g_nx_stm32g474_system.rcc == NULL ||
        (NX_G474_READ(&g_nx_stm32g474_system.rcc->APB2ENR) &
         RCC_APB2ENR_SYSCFGEN) == 0U)
        return NX_ERROR_STATE;
    if ((NX_G474_FLASH_REMAP() & SYSCFG_MEMRMP_FB_MODE) != 0U ||
        (NX_G474_READ(&p->registers->OPTR) & FLASH_OPTR_BFB2) != 0U)
        return NX_ERROR_UNSUPPORTED;
    return NX_SUCCESS;
}
static bool foreign_mode(const nx_stm32g474_flash_state_t* p) {
    return (NX_G474_READ(&p->registers->CR) &
            ~(uint32_t)(FLASH_CR_LOCK | FLASH_CR_OPTLOCK)) != 0U;
}
static const nx_flash_geometry_t* geometry(const void* context) {
    return context != NULL ? &g_nx_stm32g474_flash_geometry : NULL;
}
static nx_result_t read(const void* context, uint32_t offset, void* data,
                        size_t length) {
    const nx_stm32g474_flash_state_t* p = context;
    if (!range(p, offset, length) || (data == NULL && length != 0U) ||
        (length != 0U && protected(p, data, length)))
        return NX_ERROR_INVALID;
    if (p->active || (NX_G474_READ(&p->registers->SR) & FLASH_SR_BSY) != 0U)
        return NX_ERROR_BUSY;
    if (p->restore_pending)
        return NX_ERROR_STATE;
    nx_result_t mapped = mapping(p);
    if (mapped != NX_SUCCESS)
        return mapped;
    uint8_t* out = data;
    for (size_t i = 0U; i < length; ++i)
        out[i] = p->memory[offset + i];
    return NX_SUCCESS;
}
static bool end(nx_stm32g474_flash_state_t* p);
static nx_result_t begin(nx_stm32g474_flash_state_t* p) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (p->active || (NX_G474_READ(&p->registers->SR) & FLASH_SR_BSY) != 0U)
        return NX_ERROR_BUSY;
    if (p->restore_pending)
        return NX_ERROR_STATE;
    if (foreign_mode(p) || !NX_G474_IRQ_AVAILABLE(FLASH_IRQn) ||
        (NX_G474_READ(&p->registers->ECCR) & FLASH_ECCR_ECCIE) != 0U)
        return NX_ERROR_BUSY;
    nx_result_t mapped = mapping(p);
    if (mapped != NX_SUCCESS)
        return mapped;
    if ((NX_G474_READ(&p->registers->OPTR) & FLASH_OPTR_DBANK) == 0U ||
        p->supply_mv < 2700U || p->supply_mv > 3600U)
        return NX_ERROR_UNSUPPORTED;
    if ((NX_G474_READ(&p->registers->CR) & FLASH_CR_LOCK) != 0U) {
        NX_G474_WRITE(&p->registers->KEYR, 0x45670123U);
        NX_G474_WRITE(&p->registers->KEYR, 0xcdef89abU);
        NX_G474_FLASH_POLL(p);
        if ((NX_G474_READ(&p->registers->CR) & FLASH_CR_LOCK) != 0U)
            return NX_ERROR_IO;
    }
    p->saved_cache = NX_G474_READ(&p->registers->ACR);
    p->active = true;
    p->restore_pending = true;
    /* ES0430 2.2.2: dual-bank read-while-write must not update live data cache.
     * Disable before the first pulse, then reset before restoring DCEN.
     */
    NX_G474_WRITE(&p->registers->ACR,
                  p->saved_cache & ~(uint32_t)FLASH_ACR_DCEN);
    nx_arch_dsb();
    if ((NX_G474_READ(&p->registers->ACR) & FLASH_ACR_DCEN) != 0U) {
        (void)end(p);
        return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}
/** \brief Every physical unit needs a freshly observable completion flag. */
static nx_result_t prepare_pulse(nx_stm32g474_flash_state_t* p) {
    NX_G474_WRITE(&p->registers->SR, ERRORS | FLASH_SR_EOP);
    nx_arch_dsb();
    return (NX_G474_READ(&p->registers->SR) &
            (ERRORS | FLASH_SR_EOP | FLASH_SR_BSY)) == 0U
               ? NX_SUCCESS
               : NX_ERROR_IO;
}
static nx_result_t pulse(nx_stm32g474_flash_state_t* p, nx_time_us_t deadline) {
    /* A physical Flash pulse has no cancellation. An elapsed deadline cannot
     * permit return while the Flash state machine is still accessing memory.
     */
    do {
        NX_G474_FLASH_POLL(p);
    } while ((NX_G474_READ(&p->registers->SR) & FLASH_SR_BSY) != 0U);
    uint32_t status = NX_G474_READ(&p->registers->SR);
    if ((status & ERRORS) != 0U || (status & FLASH_SR_EOP) == 0U)
        return NX_ERROR_IO;
    return nx_deadline_expired(deadline, nx_time_now_us()) ? NX_ERROR_TIMEOUT
                                                           : NX_SUCCESS;
}
static bool end(nx_stm32g474_flash_state_t* p) {
    NX_G474_WRITE(&p->registers->CR,
                  (NX_G474_READ(&p->registers->CR) &
                   ~(uint32_t)(FLASH_CR_PG | FLASH_CR_PER | FLASH_CR_PNB |
                               FLASH_CR_BKER | PULSE_EVENTS)) |
                      FLASH_CR_LOCK);
    /* Source withdrawal precedes flag acknowledgement and NVIC pending join.
     * FLASH_IRQn is exclusively claimed but is never enabled by this provider.
     */
    nx_arch_dsb();
    bool source_withdrawn =
        (NX_G474_READ(&p->registers->CR) & PULSE_EVENTS) == 0U;
    NX_G474_WRITE(&p->registers->SR, ERRORS | FLASH_SR_EOP);
    nx_arch_dsb();
    bool irq_joined =
        source_withdrawn &&
        NX_G474_IRQ_CONTROL(FLASH_IRQn, false, 0U) == NX_SUCCESS &&
        NX_G474_IRQ_AVAILABLE(FLASH_IRQn);
    uint32_t cache = p->saved_cache;
    NX_G474_WRITE(&p->registers->ACR,
                  cache & ~(uint32_t)(FLASH_ACR_ICEN | FLASH_ACR_DCEN));
    NX_G474_WRITE(&p->registers->ACR,
                  (cache & ~(uint32_t)(FLASH_ACR_ICEN | FLASH_ACR_DCEN)) |
                      FLASH_ACR_ICRST | FLASH_ACR_DCRST);
    NX_G474_WRITE(&p->registers->ACR, cache);
    nx_arch_dsb();
    nx_arch_isb();
    p->active = false;
    uint32_t control = NX_G474_READ(&p->registers->CR);
    bool restored = (control & FLASH_CR_LOCK) != 0U &&
                    (control & (FLASH_CR_PG | FLASH_CR_PER | FLASH_CR_PNB |
                                FLASH_CR_BKER | PULSE_EVENTS)) == 0U &&
                    (NX_G474_READ(&p->registers->SR) &
                     (ERRORS | FLASH_SR_EOP | FLASH_SR_BSY)) == 0U &&
                    irq_joined && NX_G474_READ(&p->registers->ACR) == cache;
    p->restore_pending = !restored;
    return restored;
}
nx_result_t nx_stm32g474_flash_close(nx_stm32g474_flash_state_t* p) {
    if (p == NULL)
        return NX_ERROR_INVALID;
    if (!p->restore_pending && !p->active)
        return NX_SUCCESS;
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (!range(p, 0U, 0U))
        return NX_ERROR_INVALID;
    if (p->active || (NX_G474_READ(&p->registers->SR) & FLASH_SR_BSY) != 0U)
        return NX_ERROR_BUSY;
    return end(p) ? NX_SUCCESS : NX_ERROR_IO;
}
static nx_result_t program(void* context, uint32_t offset, const void* data,
                           size_t length, nx_time_us_t deadline) {
    nx_stm32g474_flash_state_t* p = context;
    if (!range(p, offset, length) || data == NULL || length == 0U ||
        (offset & 7U) != 0U || (length & 7U) != 0U ||
        protected(p, data, length))
        return NX_ERROR_INVALID;
    if (deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_UNSUPPORTED;
    if (nx_deadline_expired(deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    nx_result_t result = begin(p);
    if (result != NX_SUCCESS)
        return result;
    const uint8_t* input = data;
    for (size_t i = 0U; i < length; i += 8U) {
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            result = NX_ERROR_TIMEOUT;
            break;
        }
        volatile uint32_t* address =
            (volatile uint32_t*)(p->memory + offset + i);
        /* G4 ECC double words are single-program units, including the all-ones
         * data pattern. Do not promise safe rewriting of previously used ECC.
         */
        if (address[0] != UINT32_MAX || address[1] != UINT32_MAX) {
            result = NX_ERROR_IO;
            break;
        }
        uint32_t words[2];
        memcpy(words, input + i, sizeof(words));
        /* An all-ones input must not consume the erased ECC line. Visible
         * FF is only a rejection check; callers own actual erase provenance.
         */
        if (words[0] == UINT32_MAX && words[1] == UINT32_MAX)
            continue;
        result = prepare_pulse(p);
        if (result != NX_SUCCESS)
            break;
        NX_G474_WRITE(&p->registers->CR, FLASH_CR_PG | PULSE_EVENTS);
        nx_arch_dsb();
        if ((NX_G474_READ(&p->registers->CR) & ~(uint32_t)FLASH_CR_OPTLOCK) !=
            (FLASH_CR_PG | PULSE_EVENTS)) {
            result = NX_ERROR_IO;
            break;
        }
        nx_arch_irq_state_t saved = nx_arch_irq_save();
        NX_G474_FLASH_WRITE_WORD(address, words[0]);
        nx_arch_isb();
        NX_G474_FLASH_WRITE_WORD(address + 1, words[1]);
        nx_arch_irq_restore(saved);
        result = pulse(p, deadline);
        NX_G474_WRITE(&p->registers->CR, 0U);
        if (result != NX_SUCCESS)
            break;
    }
    if (!end(p))
        return NX_ERROR_IO;
    return result;
}
static nx_result_t erase(void* context, uint32_t offset, size_t length,
                         nx_time_us_t deadline) {
    nx_stm32g474_flash_state_t* p = context;
    if (!range(p, offset, length) || length == 0U || (offset & 2047U) != 0U ||
        (length & 2047U) != 0U)
        return NX_ERROR_INVALID;
    if (deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_UNSUPPORTED;
    if (nx_deadline_expired(deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    nx_result_t result = begin(p);
    if (result != NX_SUCCESS)
        return result;
    for (size_t i = 0U; i < length; i += 2048U) {
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            result = NX_ERROR_TIMEOUT;
            break;
        }
        uint32_t page = (offset + (uint32_t)i) / 2048U;
        uint32_t command = FLASH_CR_PER | PULSE_EVENTS |
                           ((page % 128U) << FLASH_CR_PNB_Pos) |
                           (page >= 128U ? FLASH_CR_BKER : 0U);
        result = prepare_pulse(p);
        if (result != NX_SUCCESS)
            break;
        NX_G474_WRITE(&p->registers->CR, command);
        nx_arch_dsb();
        if ((NX_G474_READ(&p->registers->CR) & ~(uint32_t)FLASH_CR_OPTLOCK) !=
            command) {
            result = NX_ERROR_IO;
            break;
        }
        NX_G474_WRITE(&p->registers->CR, command | FLASH_CR_STRT);
        result = pulse(p, deadline);
        NX_G474_WRITE(&p->registers->CR, 0U);
        if (result != NX_SUCCESS)
            break;
    }
    if (!end(p))
        return NX_ERROR_IO;
    return result;
}
const nx_flash_ops_t nx_stm32g474_flash_ops = {
    .geometry = geometry, .read = read, .program = program, .erase = erase};
