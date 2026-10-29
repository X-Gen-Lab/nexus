/**
 * \file            flash.c
 * \brief           H743 exact dual-bank physical flashword and sector pulses
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32h743_cold.h"
#include "stm32h743_flash_execution.h"
#include "stm32h743_flash_ram.h"
#include "stm32h743_provider.h"
#if NEXUS_CORE_HZ == 384000000U
#include "stm32h743_clock.h"
#endif
#include <string.h>
#define NX_H743_BANK_BYTES   1048576U
#define NX_H743_SECTOR_BYTES 131072U
#define NX_H743_WORD_BYTES   32U
#define BUSY_FLAGS           ((uint32_t)(FLASH_SR_BSY | FLASH_SR_QW | FLASH_SR_WBNE))
#ifndef NX_H743_FLASH_RAM_ENTER
#define NX_H743_FLASH_RAM_ENTER(execution)                                     \
    nx_stm32h743_flash_ram_enter(execution)
#endif
#ifndef NX_H743_FLASH_IMAGE
/* The selected linker output emits these four physical endpoints as LONGs.
 * This immutable record needs no runtime registry or RAM initialization. */
extern const nx_stm32h743_flash_image_t g_nx_stm32h743_flash_image;
#define NX_H743_FLASH_IMAGE() (&g_nx_stm32h743_flash_image)
#endif
/* Geometry is emitted from the selected physical part facts. */
static bool task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
/** \brief Overflow is never evidence that two control spans are disjoint. */
static bool overlap(const void* a, size_t as, const void* b, size_t bs) {
    const uintptr_t first = (uintptr_t)a;
    const uintptr_t second = (uintptr_t)b;
    return as > UINTPTR_MAX - first || bs > UINTPTR_MAX - second ||
           (first < second + bs && second < first + as);
}
/** \brief Stopped System state and its MMIO remain owned control storage. */
static bool system_overlap(const void* value, size_t bytes) {
    const nx_stm32h743_system_t* s = &g_nx_stm32h743_system;
    return overlap(value, bytes, s, sizeof(*s)) ||
           (s->rcc != NULL && overlap(value, bytes, s->rcc, sizeof(*s->rcc))) ||
           (s->flash != NULL &&
            overlap(value, bytes, s->flash, sizeof(*s->flash))) ||
           (s->power != NULL &&
            overlap(value, bytes, s->power, sizeof(*s->power))) ||
           (s->timer != NULL &&
            overlap(value, bytes, s->timer, sizeof(*s->timer))) ||
           (s->core != NULL &&
            overlap(value, bytes, s->core, sizeof(*s->core)));
}
static bool valid(const nx_stm32h743_flash_state_t* p) {
    return p != NULL && (uintptr_t)p <= UINTPTR_MAX - sizeof(*p) &&
           !system_overlap(p, sizeof(*p)) && p->registers != NULL &&
           (uintptr_t)p->registers <= UINTPTR_MAX - sizeof(*p->registers) &&
           p->memory != NULL &&
           (uintptr_t)p->memory <=
               UINTPTR_MAX - g_nx_stm32h743_flash_geometry.size &&
           (uintptr_t)p->memory % _Alignof(uint32_t) == 0U &&
           p->poll_limit != 0U &&
           !overlap(p, sizeof(*p), p->registers, sizeof(*p->registers)) &&
           !overlap(p, sizeof(*p), (const void*)p->memory,
                    g_nx_stm32h743_flash_geometry.size);
}
static bool range(uint32_t offset, size_t length) {
    return offset <= g_nx_stm32h743_flash_geometry.size &&
           length <= g_nx_stm32h743_flash_geometry.size - offset;
}
static bool external(const nx_stm32h743_flash_state_t* p, const void* value,
                     size_t bytes) {
    return value != NULL && (uintptr_t)value <= UINTPTR_MAX - bytes &&
           !overlap(value, bytes, p, sizeof(*p)) &&
           !overlap(value, bytes, p->registers, sizeof(*p->registers)) &&
           !overlap(value, bytes, (const void*)p->memory,
                    g_nx_stm32h743_flash_geometry.size) &&
           !system_overlap(value, bytes) &&
           nx_stm32h743_flash_ram_external(p, value, bytes);
}
static bool idle(const nx_stm32h743_flash_state_t* p) {
    return !p->active && ((NX_H743_READ(&p->registers->SR1) |
                           NX_H743_READ(&p->registers->SR2)) &
                          BUSY_FLAGS) == 0U;
}
/** \brief The fixed HSI pulse profile never adopts a foreign clock or VOS.
 * \note RM0433 Rev9 Table17 and section4.3.9: read latency alone does not
 *       establish the programming delay. ACR is shared by both banks and
 *       cannot be retuned by a physical pulse submission.
 */
static bool fixed_profile_valid(const nx_stm32h743_system_t* s) {
    if (!s->started || s->rcc == NULL || s->power == NULL || s->flash == NULL ||
        s->core == NULL || (s->core_hz != 32000000U && s->core_hz != 64000000U))
        return false;
    const uint32_t cr = NX_H743_READ(&s->rcc->CR);
    const uint32_t divider =
        s->core_hz == 32000000U ? RCC_CR_HSIDIV_2 : RCC_CR_HSIDIV_1;
    if ((cr & (RCC_CR_HSION | RCC_CR_HSIRDY | RCC_CR_HSIDIV)) !=
            (RCC_CR_HSION | RCC_CR_HSIRDY | divider) ||
        (divider != RCC_CR_HSIDIV_1 && (cr & RCC_CR_HSIDIVF) == 0U) ||
        (cr & (RCC_CR_HSEON | RCC_CR_HSEBYP | RCC_CR_PLL1ON | RCC_CR_PLL2ON |
               RCC_CR_PLL3ON)) != 0U ||
        (NX_H743_READ(&s->rcc->CFGR) & (RCC_CFGR_SW | RCC_CFGR_SWS)) !=
            (RCC_CFGR_SW_HSI | RCC_CFGR_SWS_HSI) ||
        (NX_H743_READ(&s->rcc->D1CFGR) &
         (RCC_D1CFGR_D1CPRE | RCC_D1CFGR_HPRE | RCC_D1CFGR_D1PPRE)) != 0U ||
        (NX_H743_READ(&s->rcc->D2CFGR) &
         (RCC_D2CFGR_D2PPRE1 | RCC_D2CFGR_D2PPRE2)) != 0U ||
        (NX_H743_READ(&s->rcc->D3CFGR) & RCC_D3CFGR_D3PPRE) != 0U ||
        (NX_H743_READ(&s->power->CR3) & (PWR_CR3_LDOEN | PWR_CR3_BYPASS)) !=
            PWR_CR3_LDOEN ||
        (NX_H743_READ(&s->power->D3CR) & (PWR_D3CR_VOS | PWR_D3CR_VOSRDY)) !=
            (PWR_D3CR_VOS_0 | PWR_D3CR_VOSRDY) ||
        (NX_H743_READ(&s->power->CSR1) &
         (PWR_CSR1_ACTVOS | PWR_CSR1_ACTVOSRDY)) !=
            (PWR_CSR1_ACTVOS_0 | PWR_CSR1_ACTVOSRDY))
        return false;
    return true;
}
static bool pulse_timing_valid(const nx_stm32h743_flash_state_t* p) {
    const nx_stm32h743_system_t* s = &g_nx_stm32h743_system;
    if (p->registers != s->flash)
        return false;
#if NEXUS_CORE_HZ == 384000000U
    /* RM0433 Table17 and4.3.9: WR2 covers the upper198.9MHz envelope;
     * a longer programming delay only reduces speed at its lower end.
     * The joint cold owner establishes timing before any physical pulse. */
    return s->started && nx_stm32h743_pll384_clocks_ready();
#else
    if (!fixed_profile_valid(s))
        return false;
    const uint32_t delay =
        s->core_hz == 64000000U ? FLASH_ACR_WRHIGHFREQ_0 : 0U;
    return (NX_H743_READ(&p->registers->ACR) &
            (FLASH_ACR_LATENCY | FLASH_ACR_WRHIGHFREQ)) ==
           (FLASH_ACR_LATENCY_7WS | delay);
#endif
}
/** \brief Idle status alone cannot exclude a bank whose pulse is armed. */
static bool timing_idle(FLASH_TypeDef* flash) {
    const uint32_t permitted = FLASH_CR_LOCK | FLASH_CR_PSIZE;
    return ((NX_H743_READ(&flash->SR1) | NX_H743_READ(&flash->SR2)) &
            BUSY_FLAGS) == 0U &&
           ((NX_H743_READ(&flash->CR1) | NX_H743_READ(&flash->CR2)) &
            ~permitted) == 0U;
}
_Static_assert(sizeof(nx_stm32h743_flash_timing_state_t) == 8U,
               "Selected H743 Flash timing owner exceeds its static budget");
nx_result_t
nx_stm32h743_flash_timing_initialize(nx_stm32h743_flash_timing_state_t* state) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (state == NULL)
        return NX_ERROR_INVALID;
    if (state->retained)
        return NX_ERROR_BUSY;
    nx_stm32h743_system_t* s = &g_nx_stm32h743_system;
    if (!fixed_profile_valid(s) ||
        (NX_H743_READ(&s->core->CCR) & (SCB_CCR_DC_Msk | SCB_CCR_IC_Msk)) != 0U)
        return NX_ERROR_STATE;
    if (!timing_idle(s->flash))
        return NX_ERROR_BUSY;
    const uint32_t acr = NX_H743_READ(&s->flash->ACR);
    if ((acr & (FLASH_ACR_LATENCY | FLASH_ACR_WRHIGHFREQ)) !=
        FLASH_ACR_LATENCY_7WS)
        return NX_ERROR_STATE;
    if (s->core_hz == 32000000U)
        return NX_SUCCESS;
    /* Shared ACR changes only before either bank can have a consumer. Set
     * responsibility before the write: a refused readback is not release. */
    state->saved_acr = acr;
    state->retained = true;
    const uint32_t value = acr | FLASH_ACR_WRHIGHFREQ_0;
    NX_H743_WRITE(&s->flash->ACR, value);
    nx_arch_dsb();
    nx_arch_isb();
    return NX_H743_READ(&s->flash->ACR) == value ? NX_SUCCESS : NX_ERROR_IO;
}
nx_result_t
nx_stm32h743_flash_timing_stop(nx_stm32h743_flash_timing_state_t* state) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (state == NULL)
        return NX_ERROR_INVALID;
    if (!state->retained)
        return NX_SUCCESS;
    nx_stm32h743_system_t* s = &g_nx_stm32h743_system;
    if (!fixed_profile_valid(s) || s->core_hz != 64000000U ||
        (NX_H743_READ(&s->core->CCR) & (SCB_CCR_DC_Msk | SCB_CCR_IC_Msk)) !=
            0U ||
        (state->saved_acr & (FLASH_ACR_LATENCY | FLASH_ACR_WRHIGHFREQ)) !=
            FLASH_ACR_LATENCY_7WS)
        return NX_ERROR_STATE;
    if (!timing_idle(s->flash))
        return NX_ERROR_BUSY;
    const uint32_t acr = NX_H743_READ(&s->flash->ACR);
    const uint32_t delay = acr & FLASH_ACR_WRHIGHFREQ;
    if ((acr & FLASH_ACR_LATENCY) != FLASH_ACR_LATENCY_7WS ||
        (delay != 0U && delay != FLASH_ACR_WRHIGHFREQ_0))
        return NX_ERROR_STATE;
    /* Preserve the conservative CPU read latency and every unowned bit.
     * Restored delay0 forbids further HSI64 pulses through admission(). */
    const uint32_t restored = (acr & ~(uint32_t)FLASH_ACR_WRHIGHFREQ) |
                              (state->saved_acr & FLASH_ACR_WRHIGHFREQ);
    if (acr != restored) {
        NX_H743_WRITE(&s->flash->ACR, restored);
        nx_arch_dsb();
        nx_arch_isb();
        if (NX_H743_READ(&s->flash->ACR) != restored)
            return NX_ERROR_IO;
    }
    state->retained = false;
    return NX_SUCCESS;
}
static const nx_flash_geometry_t* geometry(const void* context) {
    return valid(context) ? &g_nx_stm32h743_flash_geometry : NULL;
}
static nx_result_t read_status(const nx_stm32h743_flash_state_t* p) {
    return ((NX_H743_READ(&p->registers->SR1) |
             NX_H743_READ(&p->registers->SR2)) &
            (FLASH_SR_SNECCERR | FLASH_SR_DBECCERR)) == 0U
               ? NX_SUCCESS
               : NX_ERROR_IO;
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
        out[i] = NX_H743_FLASH_READ8(p->memory + offset + i);
    return read_status(p);
}
static nx_result_t admission(nx_stm32h743_flash_state_t* p,
                             nx_time_us_t deadline) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    nx_result_t ready = nx_stm32h743_flash_ram_admission(p);
    if (ready != NX_SUCCESS)
        return ready;
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
    if (!pulse_timing_valid(p))
        return NX_ERROR_STATE;
    uint32_t permitted = FLASH_CR_LOCK | FLASH_CR_PSIZE;
    if (((NX_H743_READ(&p->registers->CR1) | NX_H743_READ(&p->registers->CR2)) &
         ~permitted) != 0U)
        return NX_ERROR_BUSY;
    if (deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_UNSUPPORTED;
    return nx_deadline_expired(deadline, nx_time_now_us()) ? NX_ERROR_TIMEOUT
                                                           : NX_SUCCESS;
}
/** \brief Stage only after final live admission, then enter the real RAM pod.
 */
static nx_result_t command(nx_stm32h743_flash_state_t* p, unsigned bank,
                           uint32_t configuration,
                           volatile uint32_t* destination,
                           const uint32_t* words, nx_time_us_t deadline) {
    nx_result_t result = nx_stm32h743_flash_ram_admission(p);
    if (result != NX_SUCCESS)
        return result;
    nx_stm32h743_flash_ram_workspace_t* execution = p->execution;
    volatile uint32_t* control =
        bank == 0U ? &p->registers->CR1 : &p->registers->CR2;
    execution->bank = bank;
    execution->configuration = configuration;
    execution->destination = destination;
    execution->saved_control = NX_H743_READ(control) & FLASH_CR_PSIZE;
    execution->deadline = deadline;
    execution->program = words != NULL;
    if (words != NULL)
        for (unsigned i = 0U; i < 8U; ++i)
            execution->words[i] = words[i];
    execution->active = true;
    p->active = true;
    result = NX_H743_FLASH_RAM_ENTER(execution);
    /* This continuation is legal only because the RAM transaction proved
     * physical drain, LOCK and MPU/VTOR restoration before bridge return. */
    execution->active = false;
    p->active = false;
    execution->destination = NULL;
    return result;
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
    result =
        nx_stm32h743_flash_image_check(NX_H743_FLASH_IMAGE(), offset, length);
    if (result != NX_SUCCESS)
        return result;
    /* The caller proves fresh erase provenance; visible all-FF data cannot
     * prove virgin ECC cells. Reject visible prior writes before any pulse. */
    for (size_t i = 0U; i < length; ++i)
        if (NX_H743_FLASH_READ8(p->memory + offset + i) != 0xffU)
            return NX_ERROR_STATE;
    /* Corrected bytes may still compare equal. Retain the read diagnostic
     * before an all-FF no-op or a later pulse can hide it. */
    result = read_status(p);
    if (result != NX_SUCCESS)
        return result;
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
        result = command(p, bank, FLASH_CR_PG,
                         (volatile uint32_t*)(p->memory + offset + i), words,
                         deadline);
        if (result != NX_SUCCESS)
            return result;
        for (unsigned byte = 0U; byte < NX_H743_WORD_BYTES; ++byte)
            if (NX_H743_FLASH_READ8(p->memory + offset + i + byte) !=
                input[i + byte])
                return NX_ERROR_IO;
        result = read_status(p);
        if (result != NX_SUCCESS)
            return result;
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
    result =
        nx_stm32h743_flash_image_check(NX_H743_FLASH_IMAGE(), offset, length);
    if (result != NX_SUCCESS)
        return result;
    for (size_t i = 0U; i < length; i += NX_H743_SECTOR_BYTES) {
        if (nx_deadline_expired(deadline, nx_time_now_us()))
            return NX_ERROR_TIMEOUT;
        unsigned bank = (offset + (uint32_t)i) / NX_H743_BANK_BYTES;
        unsigned sector = ((offset + (uint32_t)i) % NX_H743_BANK_BYTES) /
                          NX_H743_SECTOR_BYTES;
        const uint32_t config =
            FLASH_CR_SER | FLASH_CR_PSIZE_1 | (sector << FLASH_CR_SNB_Pos);
        result = command(p, bank, config, NULL, NULL, deadline);
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

nx_result_t
nx_stm32h743_flash_prepare(nx_stm32h743_flash_state_t* port,
                           nx_stm32h743_flash_ram_workspace_t* workspace,
                           uint32_t supply_mv) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (port == NULL || workspace == NULL)
        return NX_ERROR_INVALID;
    if (port->active)
        return NX_ERROR_BUSY;
    port->registers = FLASH;
    port->memory = (volatile uint8_t*)FLASH_BASE;
    port->supply_mv = supply_mv;
    port->poll_limit = 500000000U;
    return nx_stm32h743_flash_ram_initialize(port, workspace);
}

nx_result_t nx_stm32h743_flash_preflight(void) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    return ((NX_H743_READ(&FLASH->SR1) | NX_H743_READ(&FLASH->SR2)) &
            (FLASH_SR_BSY | FLASH_SR_QW | FLASH_SR_WBNE)) != 0U ||
                   (NX_H743_READ(&FLASH->OPTSR_CUR) &
                    FLASH_OPTSR_SWAP_BANK_OPT) != 0U
               ? NX_ERROR_BUSY
               : NX_SUCCESS;
}
