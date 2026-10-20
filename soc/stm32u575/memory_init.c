/**
 * \file            memory_init.c
 * \brief           Selected SRAM4 pre-RAM initialization and run gate
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "private/stm32u575_memory_init.h"
#include "stm32u575xx.h"
#include <stddef.h>
#ifndef NX_U575_MEMORY_RCC
#define NX_U575_MEMORY_RCC         RCC
#define NX_U575_MEMORY_PWR         PWR
#define NX_U575_MEMORY_FLASH       FLASH
#define NX_U575_MEMORY_ICACHE      ICACHE
#define NX_U575_MEMORY_DCACHE      DCACHE1
#define NX_U575_MEMORY_FLASH_KIB() (*(volatile const uint16_t*)FLASHSIZE_BASE)
#endif
#ifndef NX_U575_MEMORY_READ
#define NX_U575_MEMORY_READ(reg)         (*(reg))
#define NX_U575_MEMORY_WRITE(reg, value) (*(reg) = (uint32_t)(value))
#endif
/** \brief Integer intervals avoid ordering unrelated C objects. */
static bool overlap(uintptr_t first, uintptr_t last, uintptr_t other,
                    uintptr_t other_last) {
    return first < other_last && other < last;
}
static bool plan_valid(const nx_stm32u575_memory_region_t* region,
                       uintptr_t flash_start, uintptr_t flash_end) {
    uintptr_t control = (uintptr_t)region;
    if (region == NULL ||
        control % _Alignof(nx_stm32u575_memory_region_t) != 0U ||
        sizeof(*region) > UINTPTR_MAX - control || flash_start >= flash_end ||
        ((flash_start | flash_end) & 3U) != 0U)
        return false;
    uintptr_t alignment = region->start | region->end | region->data_load |
                          region->data_start | region->data_end |
                          region->bss_start | region->bss_end |
                          region->noinit_start | region->noinit_end;
    if ((alignment & 3U) != 0U || region->start >= region->end ||
        region->data_start < region->start ||
        region->data_start > region->data_end ||
        region->data_end > region->bss_start ||
        region->bss_start > region->bss_end ||
        region->bss_end > region->noinit_start ||
        region->noinit_start > region->noinit_end ||
        region->noinit_end > region->end ||
        overlap(region->start, region->end, flash_start, flash_end) ||
        overlap(region->start, region->end, control, control + sizeof(*region)))
        return false;
    uintptr_t size = region->data_end - region->data_start;
    return region->data_load >= flash_start && region->data_load <= flash_end &&
           size <= flash_end - region->data_load;
}
/** \brief Reset-only selection never adopts a running DMA or clock owner. */
static bool cold_profile(void) {
    RCC_TypeDef* rcc = NX_U575_MEMORY_RCC;
    uint32_t selection = NX_U575_MEMORY_READ(&rcc->ICSCR1);
    uint32_t range = (uint32_t)((selection & RCC_ICSCR1_MSIRGSEL) == 0U
                                    ? (NX_U575_MEMORY_READ(&rcc->CSR) &
                                       RCC_CSR_MSISSRANGE) >>
                                          RCC_CSR_MSISSRANGE_Pos
                                    : (selection & RCC_ICSCR1_MSISRANGE) >>
                                          RCC_ICSCR1_MSISRANGE_Pos);
    uint32_t cr = NX_U575_MEMORY_READ(&rcc->CR);
    uint32_t allowed =
        RCC_CR_MSISON | RCC_CR_MSISRDY | RCC_CR_MSIKON | RCC_CR_MSIKRDY;
    return NX_U575_MEMORY_FLASH_KIB() == 2048U &&
           (NX_U575_MEMORY_READ(&NX_U575_MEMORY_FLASH->OPTR) &
            (FLASH_OPTR_TZEN | FLASH_OPTR_SWAP_BANK)) == 0U &&
           range == 4U && (cr & ~allowed) == 0U &&
           (cr & (RCC_CR_MSISON | RCC_CR_MSISRDY)) ==
               (RCC_CR_MSISON | RCC_CR_MSISRDY) &&
           NX_U575_MEMORY_READ(&rcc->CFGR1) == 0U &&
           NX_U575_MEMORY_READ(&rcc->CFGR2) == 0U &&
           NX_U575_MEMORY_READ(&rcc->CFGR3) == 0U &&
           (NX_U575_MEMORY_READ(&rcc->AHB1ENR) & RCC_AHB1ENR_GPDMA1EN) == 0U &&
           (NX_U575_MEMORY_READ(&rcc->AHB3ENR) & RCC_AHB3ENR_LPDMA1EN) == 0U &&
           (NX_U575_MEMORY_READ(&rcc->AHB3RSTR) & RCC_AHB3RSTR_LPDMA1RST) ==
               0U &&
           (NX_U575_MEMORY_READ(&NX_U575_MEMORY_ICACHE->CR) & ICACHE_CR_EN) ==
               0U &&
           (NX_U575_MEMORY_READ(&NX_U575_MEMORY_DCACHE->CR) & DCACHE_CR_EN) ==
               0U &&
           NX_U575_MEMORY_READ(&NX_U575_MEMORY_FLASH->ACR) == 0U &&
           (NX_U575_MEMORY_READ(&NX_U575_MEMORY_FLASH->NSSR) &
            FLASH_NSSR_BSY) == 0U;
}
static bool gate(uint32_t mask) {
    volatile uint32_t* reg = &NX_U575_MEMORY_RCC->AHB3ENR;
    uint32_t value = NX_U575_MEMORY_READ(reg) | mask;
    NX_U575_MEMORY_WRITE(reg, value);
    nx_arch_dsb();
    return NX_U575_MEMORY_READ(reg) == value;
}
bool nx_stm32u575_memory_initialize(const nx_stm32u575_memory_region_t* region,
                                    uintptr_t flash_start,
                                    uintptr_t flash_end) {
    if (!plan_valid(region, flash_start, flash_end) || !cold_profile() ||
        !gate(RCC_AHB3ENR_PWREN))
        return false;
    /* RM0456 Rev7 10.10.4: VOS00 is Range4, VOS11 is Range1. 6.3.4
     * Table47 permits zero SRAM wait states up to16MHz in Range4. No SRAM4
     * access occurs before real power/retention and its EN gate are observed.
     */
    uint32_t vos = NX_U575_MEMORY_READ(&NX_U575_MEMORY_PWR->VOSR);
    if ((vos & (PWR_VOSR_VOS | PWR_VOSR_VOSRDY | PWR_VOSR_BOOSTEN)) !=
            PWR_VOSR_VOSRDY ||
        (NX_U575_MEMORY_READ(&NX_U575_MEMORY_PWR->CR1) & PWR_CR1_SRAM4PD) !=
            0U ||
        (NX_U575_MEMORY_READ(&NX_U575_MEMORY_PWR->CR2) & PWR_CR2_SRAM4PDS) !=
            0U ||
        !gate(RCC_AHB3ENR_SRAM4EN))
        return false;
    uintptr_t source = region->data_load;
    for (uintptr_t address = region->data_start; address < region->data_end;
         address += sizeof(uint32_t)) {
        *(volatile uint32_t*)address = *(const uint32_t*)source;
        source += sizeof(uint32_t);
    }
    for (uintptr_t address = region->bss_start; address < region->bss_end;
         address += sizeof(uint32_t))
        *(volatile uint32_t*)address = 0U;
    nx_arch_dsb();
    /* The selected reset owner prepares only the LPDMA run clock after the
     * SRAM4 image is usable. No channel, SMEN, AMEN or power field is changed.
     * A refused late gate fails reset even though DATA/BSS were initialized. */
    return gate(RCC_AHB3ENR_LPDMA1EN);
}
#ifndef NEXUS_U575_MEMORY_MODEL
extern uint32_t __nx_flash_start;
extern uint32_t __nx_flash_end;
extern uint32_t __nx_sram4_region_start;
extern uint32_t __nx_sram4_region_end;
extern uint32_t __nx_sram4_data_load;
extern uint32_t __nx_sram4_data_start;
extern uint32_t __nx_sram4_data_end;
extern uint32_t __nx_sram4_bss_start;
extern uint32_t __nx_sram4_bss_end;
extern uint32_t __nx_sram4_noinit_start;
extern uint32_t __nx_sram4_noinit_end;
bool nx_stm32u575_memory_boot(void) {
    const nx_stm32u575_memory_region_t region = {
        (uintptr_t)&__nx_sram4_region_start,
        (uintptr_t)&__nx_sram4_region_end,
        (uintptr_t)&__nx_sram4_data_load,
        (uintptr_t)&__nx_sram4_data_start,
        (uintptr_t)&__nx_sram4_data_end,
        (uintptr_t)&__nx_sram4_bss_start,
        (uintptr_t)&__nx_sram4_bss_end,
        (uintptr_t)&__nx_sram4_noinit_start,
        (uintptr_t)&__nx_sram4_noinit_end};
    uintptr_t flash_start = (uintptr_t)&__nx_flash_start;
    uintptr_t flash_end = (uintptr_t)&__nx_flash_end;
    if (region.start != SRAM4_BASE_NS ||
        region.end != SRAM4_BASE_NS + SRAM4_SIZE ||
        flash_start != FLASH_BASE_NS || flash_end > FLASH_BASE_NS + 0x200000U)
        return false;
    return nx_stm32u575_memory_initialize(&region, flash_start, flash_end);
}
#endif
