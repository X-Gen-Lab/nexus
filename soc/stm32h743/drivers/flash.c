/**
 * \file            flash.c
 * \brief           H743 exact dual-bank physical flashword and sector pulses
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/fault.h"
#include "stm32h743_provider.h"
#include <string.h>
#define NX_H743_FLASH_BYTES  2097152U
#define NX_H743_BANK_BYTES   1048576U
#define NX_H743_SECTOR_BYTES 131072U
#define NX_H743_WORD_BYTES   32U
#define BUSY_FLAGS           ((uint32_t)(FLASH_SR_BSY | FLASH_SR_QW | FLASH_SR_WBNE))
#define ERROR_FLAGS                                                            \
    ((uint32_t)(FLASH_SR_WRPERR | FLASH_SR_PGSERR | FLASH_SR_STRBERR |         \
                FLASH_SR_INCERR | FLASH_SR_OPERR | FLASH_SR_RDPERR |           \
                FLASH_SR_RDSERR | FLASH_SR_SNECCERR | FLASH_SR_DBECCERR))
static const nx_flash_sector_t s_sectors[16] = {
    {0U, NX_H743_SECTOR_BYTES},        {0x20000U, NX_H743_SECTOR_BYTES},
    {0x40000U, NX_H743_SECTOR_BYTES},  {0x60000U, NX_H743_SECTOR_BYTES},
    {0x80000U, NX_H743_SECTOR_BYTES},  {0xa0000U, NX_H743_SECTOR_BYTES},
    {0xc0000U, NX_H743_SECTOR_BYTES},  {0xe0000U, NX_H743_SECTOR_BYTES},
    {0x100000U, NX_H743_SECTOR_BYTES}, {0x120000U, NX_H743_SECTOR_BYTES},
    {0x140000U, NX_H743_SECTOR_BYTES}, {0x160000U, NX_H743_SECTOR_BYTES},
    {0x180000U, NX_H743_SECTOR_BYTES}, {0x1a0000U, NX_H743_SECTOR_BYTES},
    {0x1c0000U, NX_H743_SECTOR_BYTES}, {0x1e0000U, NX_H743_SECTOR_BYTES}};
const nx_flash_geometry_t g_nx_stm32h743_flash_geometry = {
    .base_address = 0x08000000U,
    .size = NX_H743_FLASH_BYTES,
    .program_unit = NX_H743_WORD_BYTES,
    .sectors = s_sectors,
    .sector_count = 16U,
    .program_flags = NX_FLASH_PROGRAM_REQUIRES_FRESH_ERASE};
static bool task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool valid(const nx_stm32h743_flash_state_t* p) {
    return p != NULL && p->registers != NULL && p->memory != NULL &&
           (uintptr_t)p->memory <= UINTPTR_MAX - NX_H743_FLASH_BYTES &&
           (uintptr_t)p->memory % _Alignof(uint32_t) == 0U &&
           p->poll_limit != 0U;
}
static bool range(uint32_t offset, size_t length) {
    return offset <= NX_H743_FLASH_BYTES &&
           length <= NX_H743_FLASH_BYTES - offset;
}
static bool overlap(const void* a, size_t as, const void* b, size_t bs) {
    return (uintptr_t)a < (uintptr_t)b + bs && (uintptr_t)b < (uintptr_t)a + as;
}
static bool external(const nx_stm32h743_flash_state_t* p, const void* value,
                     size_t bytes) {
    return value != NULL && (uintptr_t)value <= UINTPTR_MAX - bytes &&
           !overlap(value, bytes, p, sizeof(*p)) &&
           !overlap(value, bytes, p->registers, sizeof(*p->registers)) &&
           !overlap(value, bytes, (const void*)p->memory, NX_H743_FLASH_BYTES);
}
static bool idle(const nx_stm32h743_flash_state_t* p) {
    return !p->active && ((NX_H743_READ(&p->registers->SR1) |
                           NX_H743_READ(&p->registers->SR2)) &
                          BUSY_FLAGS) == 0U;
}
static const nx_flash_geometry_t* geometry(const void* context) {
    return valid(context) ? &g_nx_stm32h743_flash_geometry : NULL;
}
static nx_result_t read(const void* context, uint32_t offset, void* data,
                        size_t length) {
    const nx_stm32h743_flash_state_t* p = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(p) || !range(offset, length) ||
        (length != 0U && !external(p, data, length)))
        return NX_ERROR_INVALID;
    if (!idle(p))
        return NX_ERROR_BUSY;
    uint8_t* out = data;
    for (size_t i = 0U; i < length; ++i)
        out[i] = p->memory[offset + i];
    return ((NX_H743_READ(&p->registers->SR1) |
             NX_H743_READ(&p->registers->SR2)) &
            (FLASH_SR_SNECCERR | FLASH_SR_DBECCERR)) == 0U
               ? NX_SUCCESS
               : NX_ERROR_IO;
}
/** \brief A failed physical pulse or control drain cannot release its caller.
 */
static _Noreturn void retained_fault(nx_stm32h743_flash_state_t* p) {
    p->active = true;
    (void)nx_arch_irq_save();
    nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_IO, __FILE__, __LINE__);
    for (;;) {
    }
}
static nx_result_t admission(nx_stm32h743_flash_state_t* p,
                             nx_time_us_t deadline) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if (!g_nx_stm32h743_system.started || g_nx_stm32h743_system.core == NULL)
        return NX_ERROR_STATE;
    if (!idle(p))
        return NX_ERROR_BUSY;
    if (p->supply_mv < 2700U || p->supply_mv > 3600U ||
        (NX_H743_READ(&g_nx_stm32h743_system.core->CCR) &
         (SCB_CCR_DC_Msk | SCB_CCR_IC_Msk)) != 0U ||
        (NX_H743_READ(&p->registers->OPTSR_CUR) & FLASH_OPTSR_SWAP_BANK_OPT) !=
            0U)
        return NX_ERROR_UNSUPPORTED;
    uint32_t permitted = FLASH_CR_LOCK | FLASH_CR_PSIZE;
    if (((NX_H743_READ(&p->registers->CR1) | NX_H743_READ(&p->registers->CR2)) &
         ~permitted) != 0U)
        return NX_ERROR_BUSY;
    if (deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_UNSUPPORTED;
    return nx_deadline_expired(deadline, nx_time_now_us()) ? NX_ERROR_TIMEOUT
                                                           : NX_SUCCESS;
}
static volatile uint32_t* control(nx_stm32h743_flash_state_t* p,
                                  unsigned bank) {
    return bank == 0U ? &p->registers->CR1 : &p->registers->CR2;
}
static volatile uint32_t* status(nx_stm32h743_flash_state_t* p, unsigned bank) {
    return bank == 0U ? &p->registers->SR1 : &p->registers->SR2;
}
static nx_result_t begin(nx_stm32h743_flash_state_t* p, unsigned bank,
                         uint32_t* saved) {
    volatile uint32_t* cr = control(p, bank);
    volatile uint32_t* key =
        bank == 0U ? &p->registers->KEYR1 : &p->registers->KEYR2;
    *saved = NX_H743_READ(cr) & FLASH_CR_PSIZE;
    if ((NX_H743_READ(cr) & FLASH_CR_LOCK) != 0U) {
        NX_H743_WRITE(key, 0x45670123U);
        NX_H743_WRITE(key, 0xcdef89abU);
        nx_arch_dsb();
        if ((NX_H743_READ(cr) & FLASH_CR_LOCK) != 0U)
            return NX_ERROR_IO;
    }
    p->active = true;
    volatile uint32_t* clear =
        bank == 0U ? &p->registers->CCR1 : &p->registers->CCR2;
    NX_H743_WRITE(clear, ERROR_FLAGS | FLASH_SR_EOP);
    nx_arch_dsb();
    if ((NX_H743_READ(status(p, bank)) & (ERROR_FLAGS | FLASH_SR_EOP)) != 0U)
        return NX_ERROR_IO;
    return NX_SUCCESS;
}
/** \brief Timeout never aborts a pulse. A bounded stuck controller parks. */
static nx_result_t pulse(nx_stm32h743_flash_state_t* p, unsigned bank,
                         nx_time_us_t deadline) {
    uint32_t remaining = p->poll_limit;
    do {
        NX_H743_FLASH_POLL(p);
        if ((NX_H743_READ(status(p, bank)) & BUSY_FLAGS) == 0U)
            break;
    } while (--remaining != 0U);
    if ((NX_H743_READ(status(p, bank)) & BUSY_FLAGS) != 0U)
        retained_fault(p);
    if ((NX_H743_READ(status(p, bank)) & ERROR_FLAGS) != 0U ||
        (NX_H743_READ(status(p, bank)) & FLASH_SR_EOP) == 0U)
        return NX_ERROR_IO;
    return nx_deadline_expired(deadline, nx_time_now_us()) ? NX_ERROR_TIMEOUT
                                                           : NX_SUCCESS;
}
static void end(nx_stm32h743_flash_state_t* p, unsigned bank, uint32_t saved) {
    volatile uint32_t* cr = control(p, bank);
    NX_H743_WRITE(cr, saved | FLASH_CR_LOCK);
    nx_arch_dsb();
    nx_arch_isb();
    if (NX_H743_READ(cr) != (saved | FLASH_CR_LOCK))
        retained_fault(p);
    p->active = false;
}
static nx_result_t program(void* context, uint32_t offset, const void* data,
                           size_t length, nx_time_us_t deadline) {
    nx_stm32h743_flash_state_t* p = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(p) || !range(offset, length) || length == 0U ||
        offset % NX_H743_WORD_BYTES != 0U ||
        length % NX_H743_WORD_BYTES != 0U || !external(p, data, length))
        return NX_ERROR_INVALID;
    nx_result_t result = admission(p, deadline);
    if (result != NX_SUCCESS)
        return result;
    /* The caller proves fresh erase provenance; visible all-FF data cannot
     * prove virgin ECC cells. Reject visible prior writes before any pulse. */
    for (size_t i = 0U; i < length; ++i)
        if (p->memory[offset + i] != 0xffU)
            return NX_ERROR_STATE;
    const uint8_t* input = data;
    for (size_t i = 0U; i < length; i += NX_H743_WORD_BYTES) {
        if (nx_deadline_expired(deadline, nx_time_now_us()))
            return NX_ERROR_TIMEOUT;
        uint32_t words[8];
        memcpy(words, input + i, sizeof(words));
        bool erased_input = true;
        for (unsigned word = 0U; word < 8U; ++word)
            erased_input = erased_input && words[word] == UINT32_MAX;
        if (erased_input)
            continue;
        unsigned bank = (offset + (uint32_t)i) / NX_H743_BANK_BYTES;
        uint32_t saved = 0U;
        result = begin(p, bank, &saved);
        if (result != NX_SUCCESS) {
            if (p->active)
                end(p, bank, saved);
            return result;
        }
        NX_H743_WRITE(control(p, bank), FLASH_CR_PG);
        nx_arch_dsb();
        if (NX_H743_READ(control(p, bank)) != FLASH_CR_PG) {
            end(p, bank, saved);
            return NX_ERROR_IO;
        }
        volatile uint32_t* destination =
            (volatile uint32_t*)(p->memory + offset + i);
        nx_arch_irq_state_t mask = nx_arch_irq_save();
        for (unsigned word = 0U; word < 8U; ++word)
            NX_H743_FLASH_WRITE32(destination + word, words[word]);
        nx_arch_dsb();
        nx_arch_isb();
        nx_arch_irq_restore(mask);
        result = pulse(p, bank, deadline);
        end(p, bank, saved);
        if (result != NX_SUCCESS)
            return result;
        for (unsigned byte = 0U; byte < NX_H743_WORD_BYTES; ++byte)
            if (p->memory[offset + i + byte] != input[i + byte])
                return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}
static nx_result_t erase(void* context, uint32_t offset, size_t length,
                         nx_time_us_t deadline) {
    nx_stm32h743_flash_state_t* p = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(p) || !range(offset, length) || length == 0U ||
        offset % NX_H743_SECTOR_BYTES != 0U ||
        length % NX_H743_SECTOR_BYTES != 0U)
        return NX_ERROR_INVALID;
    nx_result_t result = admission(p, deadline);
    if (result != NX_SUCCESS)
        return result;
    for (size_t i = 0U; i < length; i += NX_H743_SECTOR_BYTES) {
        if (nx_deadline_expired(deadline, nx_time_now_us()))
            return NX_ERROR_TIMEOUT;
        unsigned bank = (offset + (uint32_t)i) / NX_H743_BANK_BYTES;
        unsigned sector = ((offset + (uint32_t)i) % NX_H743_BANK_BYTES) /
                          NX_H743_SECTOR_BYTES;
        uint32_t saved = 0U;
        result = begin(p, bank, &saved);
        if (result != NX_SUCCESS) {
            if (p->active)
                end(p, bank, saved);
            return result;
        }
        uint32_t config =
            FLASH_CR_SER | FLASH_CR_PSIZE_1 | (sector << FLASH_CR_SNB_Pos);
        NX_H743_WRITE(control(p, bank), config);
        nx_arch_dsb();
        if (NX_H743_READ(control(p, bank)) != config) {
            end(p, bank, saved);
            return NX_ERROR_IO;
        }
        NX_H743_WRITE(control(p, bank), config | FLASH_CR_START);
        nx_arch_dsb();
        result = pulse(p, bank, deadline);
        end(p, bank, saved);
        if (result != NX_SUCCESS)
            return result;
    }
    return NX_SUCCESS;
}
nx_result_t nx_stm32h743_flash_stop(nx_stm32h743_flash_state_t* p) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    return idle(p) ? NX_SUCCESS : NX_ERROR_BUSY;
}
const nx_flash_ops_t nx_stm32h743_flash_ops = {geometry, read, program, erase};
