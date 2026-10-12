/**
 * \file            security.c
 * \brief           Owned U575 dual-world cold bootstrap without product policy
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32u575_security.h"
#include "stm32u575_security_hw.h"

#define NX_TZ_FLASH_BASE 0x08100000U
#define NX_TZ_RAM_BASE   0x20040000U
#define NX_TZ_RAM_BYTES  0x80000U
#define NX_TZ_SUPERBLOCK 0x4000U
#define NX_TZ_VECTORS    142U
#define NX_TZ_CLOCK_SECURITY                                                   \
    (RCC_SECCFGR_MSISEC | RCC_SECCFGR_SYSCLKSEC | RCC_SECCFGR_PRESCSEC)
#define NX_TZ_NS_LOCKS (SYSCFG_CNSLCKR_LOCKNSMPU | SYSCFG_CNSLCKR_LOCKNSVTOR)
#define NX_TZ_S_LOCKS  (SYSCFG_CSLCKR_LOCKSAU | SYSCFG_CSLCKR_LOCKSVTAIRCR)

static volatile uint32_t* nx_tz_word(uintptr_t address) {
    return (volatile uint32_t*)address;
}

static bool nx_tz_span(uint32_t base, uint32_t bytes, uint32_t first,
                       uint32_t length, uint32_t alignment) {
    return bytes != 0U && base >= first && base - first < length &&
           bytes <= length - (base - first) &&
           ((base | bytes) & (alignment - 1U)) == 0U;
}

static bool nx_tz_layout_valid(const nx_stm32u575_tz_layout_t* p) {
    return p != NULL &&
           nx_tz_span(p->ns_flash_base, p->ns_flash_bytes, NX_TZ_FLASH_BASE,
                      0x100000U, 0x2000U) &&
           nx_tz_span(p->ns_ram_base, p->ns_ram_bytes, NX_TZ_RAM_BASE,
                      NX_TZ_RAM_BYTES, NX_TZ_SUPERBLOCK) &&
           p->ns_ram_bytes < NX_TZ_RAM_BYTES &&
           nx_tz_span(p->nsc_base, p->nsc_bytes, 0x0C002000U, 0xFE000U, 32U) &&
           nx_tz_span(p->secure_stack_base, p->secure_stack_bytes, 0x30000000U,
                      0x30000U, 8U);
}

static bool nx_tz_context(const nx_stm32u575_tz_layout_t* p) {
    const nx_arch_irq_masks_t masks = nx_arch_irq_masks();
    const uint32_t stack = nx_tz_msp();
    return nx_arch_is_privileged() && nx_arch_exception_number() == 0U &&
           masks.primask == 1U && masks.basepri == 0U &&
           masks.faultmask == 0U &&
           (nx_tz_control() &
            (CONTROL_SPSEL_Msk | CONTROL_FPCA_Msk | CONTROL_SFPA_Msk)) == 0U &&
           (stack & 7U) == 0U && stack > p->secure_stack_base &&
           stack - p->secure_stack_base <= p->secure_stack_bytes;
}

static bool nx_tz_write_checked(volatile uint32_t* reg, uint32_t value) {
    nx_tz_write(reg, value);
    nx_arch_dsb();
    return nx_tz_read(reg) == value;
}

static bool nx_tz_flash_valid(const nx_stm32u575_tz_layout_t* p) {
    const uint32_t options = nx_tz_read(&FLASH_S->OPTR);
    const uint32_t required = FLASH_OPTR_TZEN | FLASH_OPTR_SRAM3_ECC;
    const uint32_t watermark = nx_tz_read(&FLASH_S->SECWM2R1);
    const uint32_t start = watermark & 0x7FU;
    const uint32_t end = (watermark >> 16U) & 0x7FU;
    const uint32_t first = (p->ns_flash_base - NX_TZ_FLASH_BASE) / 0x2000U;
    const uint32_t last = first + p->ns_flash_bytes / 0x2000U - 1U;
    if ((nx_tz_read(nx_tz_word(FLASHSIZE_BASE)) & 0xFFFFU) != 2048U ||
        (options & (required | FLASH_OPTR_SWAP_BANK)) != required ||
        (nx_tz_read(&FLASH_S->SECWM1R1) & 0x007F007FU) != 0x007F0000U ||
        (nx_tz_read(&FLASH_S->SECWM1R2) & FLASH_SECWM1R2_HDP1EN) != 0U ||
        (nx_tz_read(&FLASH_S->SECWM2R2) & FLASH_SECWM2R2_HDP2EN) != 0U ||
        (nx_tz_read(&FLASH_S->SECBOOTADD0R) & FLASH_SECBOOTADD0R_SECBOOTADD0) !=
            0x0C000000U ||
        (nx_tz_read(&FLASH_S->SECCR) & (FLASH_SECCR_LOCK | FLASH_SECCR_INV)) !=
            FLASH_SECCR_LOCK ||
        (nx_tz_read(&FLASH_S->NSCR) & FLASH_NSCR_LOCK) == 0U ||
        (start <= end && first <= end && last >= start)) {
        return false;
    }
    /* RM0456 7.9.29-34: this exact 2 MiB part has four words per bank. */
    for (uint32_t page = first; page <= last; ++page) {
        const uintptr_t offset = (page / 32U) * sizeof(uint32_t);
        const uint32_t mask = 1UL << (page % 32U);
        if (((nx_tz_read(nx_tz_word((uintptr_t)&FLASH_S->SECBB2R1 + offset)) |
              nx_tz_read(nx_tz_word((uintptr_t)&FLASH_S->PRIVBB2R1 + offset))) &
             mask) != 0U) {
            return false;
        }
    }
    return true;
}

static bool nx_tz_interrupts_idle(void) {
    if ((nx_tz_read(&SysTick->CTRL) & 7U) != 0U ||
        (nx_tz_read(&SysTick_NS->CTRL) & 7U) != 0U) {
        return false;
    }
    for (uint32_t i = 0U; i < 4U; ++i) {
        if ((nx_tz_read(&NVIC->ISER[i]) | nx_tz_read(&NVIC->ISPR[i]) |
             nx_tz_read(&NVIC->IABR[i]) | nx_tz_read(&NVIC_NS->ISER[i]) |
             nx_tz_read(&NVIC_NS->ISPR[i]) | nx_tz_read(&NVIC_NS->IABR[i])) !=
            0U) {
            return false;
        }
    }
    return true;
}

static bool nx_tz_masters_idle(void) {
    return (nx_tz_read(&RCC_S->AHB1ENR) &
            (RCC_AHB1ENR_GPDMA1EN | RCC_AHB1ENR_DMA2DEN)) == 0U &&
           (nx_tz_read(&RCC_S->AHB2ENR1) &
            (RCC_AHB2ENR1_SDMMC1EN | RCC_AHB2ENR1_SDMMC2EN |
             RCC_AHB2ENR1_OTGEN)) == 0U &&
           (nx_tz_read(&RCC_S->AHB3ENR) & RCC_AHB3ENR_LPDMA1EN) == 0U;
}

nx_result_t nx_stm32u575_tz_preflight(const nx_stm32u575_tz_t* owner,
                                      const nx_stm32u575_tz_layout_t* layout) {
    if (owner == NULL || !nx_tz_layout_valid(layout)) {
        return NX_ERROR_INVALID;
    }
    if (owner->state != NX_STM32U575_TZ_COLD || owner->effects) {
        return NX_ERROR_STATE;
    }
    if (!nx_tz_context(layout)) {
        return NX_ERROR_CONTEXT;
    }
    if (!nx_tz_flash_valid(layout)) {
        return NX_ERROR_UNSUPPORTED;
    }
    const uint32_t regions =
        (nx_tz_read(&MPU_NS->TYPE) & MPU_TYPE_DREGION_Msk) >>
        MPU_TYPE_DREGION_Pos;
    if ((nx_tz_read(&SAU->TYPE) & 0xFFU) != 8U ||
        (regions != 8U && regions != 16U)) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (nx_tz_read(&SAU->CTRL) != 0U || nx_tz_read(&MPU->CTRL) != 0U ||
        nx_tz_read(&MPU_NS->CTRL) != 0U || !nx_tz_interrupts_idle() ||
        (nx_tz_read(&ICACHE_S->CR) & ICACHE_CR_EN) != 0U ||
        (nx_tz_read(&DCACHE1_S->CR) & DCACHE_CR_EN) != 0U ||
        !nx_tz_masters_idle() ||
        (nx_tz_read(&SCB->CPACR) & 0x00F00000U) != 0U ||
        (nx_tz_read(&FPU->FPCCR) & FPU_FPCCR_LSPACT_Msk) != 0U ||
        (nx_tz_read(&FLASH_S->SECSR) & (FLASH_SECSR_BSY | FLASH_SECSR_WDW)) !=
            0U ||
        (nx_tz_read(&FLASH_S->NSSR) & (FLASH_NSSR_BSY | FLASH_NSSR_WDW)) !=
            0U) {
        return NX_ERROR_BUSY;
    }
    if ((nx_tz_read(&RCC_S->AHB1ENR) & RCC_AHB1ENR_SRAM1EN) == 0U ||
        (nx_tz_read(&RCC_S->AHB2ENR1) & RCC_AHB2ENR1_SRAM3EN) == 0U ||
        nx_tz_read(&SCB->VTOR) != 0x0C000000U) {
        return NX_ERROR_UNSUPPORTED;
    }
    return NX_SUCCESS;
}

static bool nx_tz_ram_cold(void) {
    if (nx_tz_read(&GTZC_MPCBB3_S->CR) != 0U ||
        nx_tz_read(&GTZC_MPCBB3_S->CFGLOCKR1) != 0U ||
        nx_tz_read(&GTZC_MPCBB1_S->CR) != 0U ||
        nx_tz_read(&GTZC_MPCBB1_S->CFGLOCKR1) != 0U) {
        return false;
    }
    for (uint32_t i = 0U; i < 32U; ++i) {
        if (nx_tz_read(&GTZC_MPCBB3_S->SECCFGR[i]) != UINT32_MAX ||
            nx_tz_read(&GTZC_MPCBB3_S->PRIVCFGR[i]) != UINT32_MAX) {
            return false;
        }
    }
    return true;
}

static bool nx_tz_owner_ram(void) {
    for (uint32_t i = 0U; i < 12U; ++i) {
        if (nx_tz_read(&GTZC_MPCBB1_S->SECCFGR[i]) != UINT32_MAX ||
            nx_tz_read(&GTZC_MPCBB1_S->PRIVCFGR[i]) != UINT32_MAX) {
            return false;
        }
    }
    return true;
}

static bool nx_tz_ram_prepare(const nx_stm32u575_tz_layout_t* p) {
    /* RM0456 5.4/5.8: clear before removing Secure attribution; no stale data.
     */
    if (!nx_tz_clear(p->ns_ram_base + 0x10000000U, p->ns_ram_bytes)) {
        return false;
    }
    const uint32_t first = (p->ns_ram_base - NX_TZ_RAM_BASE) / NX_TZ_SUPERBLOCK;
    const uint32_t count = p->ns_ram_bytes / NX_TZ_SUPERBLOCK;
    for (uint32_t i = first; i < first + count; ++i) {
        if (!nx_tz_write_checked(&GTZC_MPCBB3_S->SECCFGR[i], 0U) ||
            !nx_tz_write_checked(&GTZC_MPCBB3_S->PRIVCFGR[i], 0U)) {
            return false;
        }
    }
    return nx_tz_write_checked(&GTZC_MPCBB1_S->CFGLOCKR1, 0xFFFU) &&
           nx_tz_write_checked(&GTZC_MPCBB1_S->CR, GTZC_MPCBB_CR_GLOCK_Msk) &&
           nx_tz_write_checked(&GTZC_MPCBB3_S->CFGLOCKR1, UINT32_MAX) &&
           nx_tz_write_checked(&GTZC_MPCBB3_S->CR, GTZC_MPCBB_CR_GLOCK_Msk);
}

static bool nx_tz_regions(const nx_stm32u575_tz_layout_t* p) {
    const uint32_t bases[] = {p->ns_flash_base, p->ns_ram_base, p->nsc_base};
    const uint32_t bytes[] = {p->ns_flash_bytes, p->ns_ram_bytes, p->nsc_bytes};
    for (uint32_t i = 0U; i < 8U; ++i) {
        const uint32_t base = i < 3U ? bases[i] : 0U;
        const uint32_t limit =
            i < 3U ? ((base + bytes[i] - 1U) & ~31U) | (i == 2U ? 3U : 1U) : 0U;
        if (!nx_tz_write_checked(&SAU->RNR, i) ||
            !nx_tz_write_checked(&SAU->RBAR, base) ||
            !nx_tz_write_checked(&SAU->RLAR, limit)) {
            return false;
        }
    }
    const uint32_t count = (nx_tz_read(&MPU_NS->TYPE) & MPU_TYPE_DREGION_Msk) >>
                           MPU_TYPE_DREGION_Pos;
    for (uint32_t i = 0U; i < count; ++i) {
        const uint32_t base = i < 2U ? bases[i] | (i == 0U ? 6U : 3U) : 0U;
        const uint32_t limit =
            i < 2U ? ((bases[i] + bytes[i] - 1U) & ~31U) | 1U : 0U;
        if (!nx_tz_write_checked(&MPU_NS->RNR, i) ||
            !nx_tz_write_checked(&MPU_NS->RBAR, base) ||
            !nx_tz_write_checked(&MPU_NS->RLAR, limit)) {
            return false;
        }
    }
    return nx_tz_write_checked(&SAU->RNR, 0U) &&
           nx_tz_write_checked(&MPU_NS->RNR, 0U) &&
           nx_tz_write_checked(&MPU_NS->MAIR0, 0x44U) &&
           nx_tz_write_checked(&MPU_NS->MAIR1, 0U);
}

static bool nx_tz_cpu_prepare(const nx_stm32u575_tz_layout_t* p) {
    if (!nx_tz_regions(p)) {
        return false;
    }
    for (uint32_t i = 0U; i < 4U; ++i) {
        if (!nx_tz_write_checked(&NVIC->ITNS[i], 0U)) {
            return false;
        }
    }
    /* Keep HardFault, BusFault and NMI Secure. VECTKEY reads back differently.
     */
    const uint32_t aircr = (nx_tz_read(&SCB->AIRCR) & SCB_AIRCR_PRIGROUP_Msk) |
                           SCB_AIRCR_PRIS_Msk | SCB_AIRCR_SYSRESETREQS_Msk;
    nx_tz_write(&SCB->AIRCR, (0x5FAUL << 16U) | aircr);
    nx_arch_dsb();
    if ((nx_tz_read(&SCB->AIRCR) &
         (SCB_AIRCR_PRIGROUP_Msk | SCB_AIRCR_PRIS_Msk |
          SCB_AIRCR_SYSRESETREQS_Msk | SCB_AIRCR_BFHFNMINS_Msk)) != aircr ||
        !nx_tz_write_checked(&SCB->NSACR, 0U) ||
        !nx_tz_write_checked(&SCB_NS->CPACR, 0U) ||
        !nx_tz_write_checked(&SCB_NS->VTOR, p->ns_flash_base) ||
        !nx_tz_write_checked(&MPU_NS->CTRL, MPU_CTRL_ENABLE_Msk) ||
        !nx_tz_write_checked(&SAU->CTRL, SAU_CTRL_ENABLE_Msk)) {
        return false;
    }
    nx_arch_dsb();
    nx_arch_isb();
    /* RM0456 15.3.4-5: seal only after every region/permission readback. */
    return nx_tz_write_checked(&SYSCFG_S->CNSLCKR, NX_TZ_NS_LOCKS) &&
           nx_tz_write_checked(&SYSCFG_S->CSLCKR, NX_TZ_S_LOCKS);
}

nx_result_t nx_stm32u575_tz_prepare(nx_stm32u575_tz_t* owner,
                                    const nx_stm32u575_tz_layout_t* layout) {
    const nx_result_t admitted = nx_stm32u575_tz_preflight(owner, layout);
    if (admitted != NX_SUCCESS) {
        return admitted;
    }
    owner->layout = *layout;
    owner->effects = true;
    owner->state = NX_STM32U575_TZ_FAILED;
    if (!nx_tz_write_checked(&RCC_S->AHB1ENR, nx_tz_read(&RCC_S->AHB1ENR) |
                                                  RCC_AHB1ENR_GTZC1EN) ||
        !nx_tz_write_checked(&RCC_S->APB3ENR, nx_tz_read(&RCC_S->APB3ENR) |
                                                  RCC_APB3ENR_SYSCFGEN)) {
        return NX_ERROR_IO;
    }
    if (!nx_tz_ram_cold() || !nx_tz_owner_ram() ||
        nx_tz_read(&SYSCFG_S->CNSLCKR) != 0U ||
        nx_tz_read(&SYSCFG_S->CSLCKR) != 0U) {
        return NX_ERROR_BUSY;
    }
    if (!nx_tz_write_checked(&RCC_S->SECCFGR, nx_tz_read(&RCC_S->SECCFGR) |
                                                  NX_TZ_CLOCK_SECURITY) ||
        !nx_tz_ram_prepare(layout) || !nx_tz_cpu_prepare(layout)) {
        return NX_ERROR_IO;
    }
    owner->state = NX_STM32U575_TZ_READY;
    return NX_SUCCESS;
}

static bool nx_tz_sealed(const nx_stm32u575_tz_layout_t* p) {
    if (nx_tz_read(&SAU->CTRL) != SAU_CTRL_ENABLE_Msk ||
        nx_tz_read(&MPU_NS->CTRL) != MPU_CTRL_ENABLE_Msk ||
        nx_tz_read(&MPU_NS->RNR) != 0U || nx_tz_read(&MPU_NS->MAIR0) != 0x44U ||
        nx_tz_read(&MPU_NS->MAIR1) != 0U ||
        nx_tz_read(&MPU_NS->RBAR) != (p->ns_flash_base | 6U) ||
        nx_tz_read(&MPU_NS->RLAR) !=
            (((p->ns_flash_base + p->ns_flash_bytes - 1U) & ~31U) | 1U) ||
        nx_tz_read(&MPU_NS->RBAR_A1) != (p->ns_ram_base | 3U) ||
        nx_tz_read(&MPU_NS->RLAR_A1) !=
            (((p->ns_ram_base + p->ns_ram_bytes - 1U) & ~31U) | 1U) ||
        nx_tz_read(&SYSCFG_S->CNSLCKR) != NX_TZ_NS_LOCKS ||
        nx_tz_read(&SYSCFG_S->CSLCKR) != NX_TZ_S_LOCKS ||
        nx_tz_read(&GTZC_MPCBB1_S->CFGLOCKR1) != 0xFFFU ||
        nx_tz_read(&GTZC_MPCBB1_S->CR) != GTZC_MPCBB_CR_GLOCK_Msk ||
        !nx_tz_owner_ram() ||
        nx_tz_read(&GTZC_MPCBB3_S->CFGLOCKR1) != UINT32_MAX ||
        nx_tz_read(&GTZC_MPCBB3_S->CR) != GTZC_MPCBB_CR_GLOCK_Msk ||
        nx_tz_read(&SCB_NS->VTOR) != p->ns_flash_base ||
        nx_tz_read(&SCB->VTOR) != 0x0C000000U ||
        (nx_tz_read(&SCB->AIRCR) & SCB_AIRCR_BFHFNMINS_Msk) != 0U ||
        !nx_tz_flash_valid(p) || !nx_tz_interrupts_idle() ||
        !nx_tz_masters_idle()) {
        return false;
    }
    const uint32_t first = (p->ns_ram_base - NX_TZ_RAM_BASE) / NX_TZ_SUPERBLOCK;
    const uint32_t count = p->ns_ram_bytes / NX_TZ_SUPERBLOCK;
    for (uint32_t i = 0U; i < 32U; ++i) {
        const uint32_t expected =
            i >= first && i - first < count ? 0U : UINT32_MAX;
        if (nx_tz_read(&GTZC_MPCBB3_S->SECCFGR[i]) != expected ||
            nx_tz_read(&GTZC_MPCBB3_S->PRIVCFGR[i]) != expected) {
            return false;
        }
    }
    for (uint32_t i = 0U; i < 4U; ++i) {
        if (nx_tz_read(&NVIC->ITNS[i]) != 0U) {
            return false;
        }
    }
    return true;
}

nx_result_t nx_stm32u575_tz_entry(const nx_stm32u575_tz_t* owner,
                                  uint32_t* stack, uint32_t* entry) {
    if (owner == NULL || stack == NULL || entry == NULL || stack == entry ||
        !nx_tz_layout_valid(&owner->layout)) {
        return NX_ERROR_INVALID;
    }
    if (owner->state != NX_STM32U575_TZ_READY || !owner->effects) {
        return NX_ERROR_STATE;
    }
    const nx_stm32u575_tz_layout_t* p = &owner->layout;
    if (!nx_tz_context(p)) {
        return NX_ERROR_CONTEXT;
    }
    if (!nx_tz_sealed(p)) {
        return NX_ERROR_PERMISSION;
    }
    const uint32_t sp = nx_tz_read(nx_tz_word(p->ns_flash_base));
    const uint32_t pc = nx_tz_read(nx_tz_word(p->ns_flash_base + 4U));
    if ((sp & 7U) != 0U || sp <= p->ns_ram_base ||
        sp - p->ns_ram_base > p->ns_ram_bytes || (pc & 1U) == 0U ||
        (pc & ~1U) < p->ns_flash_base + 1024U ||
        (pc & ~1U) - p->ns_flash_base >= p->ns_flash_bytes) {
        return NX_ERROR_PERMISSION;
    }
    /* No NS privileged handler can reconfigure the fixed memory contract. */
    for (uint32_t i = 2U; i < NX_TZ_VECTORS; ++i) {
        if (nx_tz_read(nx_tz_word(p->ns_flash_base + i * 4U)) != 0U) {
            return NX_ERROR_PERMISSION;
        }
    }
    *stack = sp;
    *entry = pc;
    return NX_SUCCESS;
}

nx_result_t nx_stm32u575_tz_launch(nx_stm32u575_tz_t* owner) {
    uint32_t stack;
    uint32_t entry;
    const nx_result_t admitted = nx_stm32u575_tz_entry(owner, &stack, &entry);
    if (admitted != NX_SUCCESS) {
        return admitted;
    }
    owner->state = NX_STM32U575_TZ_RUNNING;
    nx_arch_dsb();
    nx_tz_enter(stack, owner->layout.ns_ram_base, entry);
#ifdef NX_U575_SECURITY_MODEL
    /* A model returning from this terminal hardware leaf must fail closed. */
    owner->state = NX_STM32U575_TZ_FAILED;
    return NX_ERROR_STATE;
#endif
}
