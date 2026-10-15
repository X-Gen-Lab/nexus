/**
 * \file            memory_init.h
 * \brief           Optional GD32F470 data-only extra-memory reset
 * initialization \author          Nexus Team
 */
#ifndef NEXUS_GD32F470_MEMORY_INIT_H
#define NEXUS_GD32F470_MEMORY_INIT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** \brief Linker-derived bounds; never place this control in an output range.
 */
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
} nx_gd32_memory_region_t;

/**
 * \brief           Validate all selected regions, copy data and zero BSS.
 * \param[in]       regions: Immutable reset-owned descriptors in Flash or main
 *                  SRAM, outside every output range; one or two regions.
 * \param[in]       count: Exact selected region count, never a maximum pool.
 * \param[in]       flash_start: Actual resolved physical Flash beginning.
 * \param[in]       flash_end: Exclusive physical Flash end, word aligned.
 * \return          True after complete initialization; false without mutation
 *                  for any malformed bound, alignment, order or alias.
 * \note            Called once from reset before constructors, Platform start
 *                  or IRQs. The main MSP stays in SRAM. Selected TCM clock must
 *                  already be enabled and acquire-observed. TCM is data-only;
 *                  it is neither instruction memory nor DMA reachable. NOINIT
 *                  and padding remain untouched; callers initialize NOINIT
 *                  before reading. No heap, global state, libc or extra-memory
 *                  state is needed to initialize extra memory.
 */
bool nx_gd32_memory_initialize(const nx_gd32_memory_region_t* regions,
                               size_t count, uintptr_t flash_start,
                               uintptr_t flash_end);

#endif
