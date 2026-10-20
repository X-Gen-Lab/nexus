/**
 * \file            memory_init.h
 * \brief           Pre-RAM H743 selected D2 SRAM1 initialization
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32H743_MEMORY_INIT_H
#define NEXUS_STM32H743_MEMORY_INIT_H

#include <stdbool.h>
#include <stdint.h>

/** \brief Reset-owned bounds outside SRAM1; no mutable global is required. */
typedef struct {
    uintptr_t start;
    uintptr_t end;
    uintptr_t data_load;
    uintptr_t data_start;
    uintptr_t data_end;
    uintptr_t bss_start;
    uintptr_t bss_end;
    uintptr_t noinit_start;
    uintptr_t noinit_end;
} nx_stm32h743_memory_region_t;

/**
 * \brief           Check all bounds before copying data or clearing BSS.
 * \param[in]       region: Immutable descriptor in Flash or current main SRAM
 *                  stack, outside every output range; never initialized RAM.
 * \param[in]       flash_start: Resolved physical Flash beginning.
 * \param[in]       flash_end: Exclusive word-aligned physical Flash end.
 * \return          True after initialization, false without mutation for an
 *                  invalid alignment, order, bound or descriptor/source alias.
 * \note            Reset-only, after SRAM1 clock enable/readback and before
 * main data/BSS, constructors, platform start or enabled IRQs. Uses no heap,
 * libc, mutable global or already initialized object. Main MSP remains in AXI
 * SRAM. SRAM1 is DMA1/2-visible data, subject to provider address/cache
 * validation; instruction execution is not maintained. NOINIT and padding are
 *                  untouched; initialize NOINIT before use. This software
 *                  exclusion does not establish physical reset/power retention.
 */
bool nx_stm32h743_memory_initialize(const nx_stm32h743_memory_region_t* region,
                                    uintptr_t flash_start, uintptr_t flash_end);

/**
 * \brief           Initialize the selected SRAM1 from actual linker bounds.
 * \return          True after copying DATA and clearing BSS; false without
 *                  destination mutation for an invalid link-derived plan.
 * \note            SystemInit calls once with the SRAM1 clock ready. Linker
 *                  symbols and local reset-stack control are the only inputs;
 *                  this call precedes main RAM initialization. The caller must
 *                  fail-stop on false before any constructor or main executes.
 */
bool nx_stm32h743_memory_boot(void);

#endif
