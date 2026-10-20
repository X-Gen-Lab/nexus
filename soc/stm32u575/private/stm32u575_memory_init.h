/**
 * \file            stm32u575_memory_init.h
 * \brief           Pre-RAM initialization of explicitly selected U575 SRAM4
 * \author          Nexus Team
 */
#ifndef NX_STM32U575_MEMORY_INIT_H
#define NX_STM32U575_MEMORY_INIT_H
#include <stdbool.h>
#include <stdint.h>
/** \brief Reset-stack plan outside the complete SRAM4 destination domain. */
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
} nx_stm32u575_memory_region_t;
/**
 * \brief           Validate all bounds and hardware before SRAM4 writes.
 * \param[in]       region: Trusted immutable Flash/reset-stack plan, outside
 *                  the complete output domain. No initialized C state exists.
 * \param[in]       flash_start: Word-aligned beginning of the source Flash.
 * \param[in]       flash_end: Exclusive word-aligned source Flash end.
 * \return          True after DATA copy/BSS clear and prepared LPDMA run gate.
 *                  Invalid plans/admission leave destination unchanged. A
 *                  refused final DMA gate may leave DATA/BSS initialized;
 *                  every false return requires reset fail-stop.
 * \note            Exact TZEN0/MSIS4/equal-bus/cache-off/Range4 reset only.
 *                  Verify the selected EN gates and SRAM4 power/retention
 *                  before the first SRAM4 load/store. Never change power,
 *                  retention, clock source, cache, security or option bytes.
 *                  Initial DMA gates must be off. After copying and clearing,
 *                  enable/observe only LPDMA1EN. PWR/SRAM4/LPDMA run gates
 *                  remain boot owned for the image lifetime; other masters
 *                  and LPDMA channels remain excluded. No heap, libc,
 *                  initialized global or observer is used.
 *                  External reset ownership excludes debugger, NMI and other
 *                  bus masters; gate observations cannot drain a warm handoff.
 *                  NOINIT and padding remain untouched; initialize NOINIT
 *                  before use. This does not prove physical retention.
 */
bool nx_stm32u575_memory_initialize(const nx_stm32u575_memory_region_t* region,
                                    uintptr_t flash_start, uintptr_t flash_end);
/**
 * \brief           Apply exact linker bounds before primary DATA/BSS.
 * \return          True after initialization; false requires reset fail-stop.
 * \note            Called only by selected SystemInit. Flash constants and
 *                  current SRAM1 MSP are the only control storage. PWR,
 *                  SRAM4 and LPDMA EN gates remain enabled for the image
 *                  lifetime, including Stop2 and all request joins.
 */
bool nx_stm32u575_memory_boot(void);
#endif
