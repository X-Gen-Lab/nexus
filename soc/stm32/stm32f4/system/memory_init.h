/**
 * \file            memory_init.h
 * \brief           Pre-RAM STM32F407 selected memory initialization
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_MEMORY_INIT_H
#define NEXUS_STM32F407_MEMORY_INIT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** \brief Reset-owned bounds outside CCM; no mutable global is required. */
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
} nx_stm32_memory_region_t;

/**
 * \brief           Validate the whole selected reset plan without mutations.
 * \param[in]       regions: Complete immutable contiguous region descriptors.
 * \param[in]       count: One to three physical regions, not overlapping.
 * \param[in]       flash_start: Physical internal Flash lower bound.
 * \param[in]       flash_end: Exclusive word-aligned physical Flash bound.
 * \param[in]       protected_start: First byte of internal reset stack, or 0.
 * \param[in]       protected_end: Exclusive stack bound, or 0 with start 0.
 * \return          True only when every source, destination and control span
 *                  is valid before any clock/pad/destination write.
 * \note            Reset-safe pure integer validation; no globals, libc, heap
 *                  or already initialized RAM. The descriptor table cannot
 *                  alias any selected physical output region. DATA, BSS and
 *                  NOINIT must exclude the protected internal-MSP reservation.
 */
bool nx_stm32_memory_plan_valid(const nx_stm32_memory_region_t* regions,
                                size_t count, uintptr_t flash_start,
                                uintptr_t flash_end, uintptr_t protected_start,
                                uintptr_t protected_end);

/**
 * \brief           Check all bounds before copying data or clearing BSS.
 * \param[in]       region: Immutable descriptor in Flash or current main SRAM
 *                  stack, outside every output range; never initialized RAM.
 * \param[in]       flash_start: Resolved physical Flash beginning.
 * \param[in]       flash_end: Exclusive word-aligned physical Flash end.
 * \return          True after initialization, false without mutation for an
 *                  invalid alignment, order, bound or descriptor/source alias.
 * \note            Reset-only, after selected bus/clock readiness and before
 *                  main data/BSS, constructors, platform start or enabled IRQs.
 *                  Uses no heap, libc, mutable global or initialized object.
 *                  Main MSP remains internal. Placement never grants execution
 *                  or DMA access; physical domain restrictions still apply.
 *                  NOINIT and padding are untouched; initialize before use.
 *                  This does not establish physical reset/power retention.
 */
bool nx_stm32_memory_initialize(const nx_stm32_memory_region_t* region,
                                uintptr_t flash_start, uintptr_t flash_end);

/**
 * \brief           Initialize the selected CCM from actual linker bounds.
 * \return          True after copying DATA and clearing BSS; false without
 *                  destination mutation for an invalid link-derived plan.
 * \note            SystemInit calls once with the CCM clock ready. Linker
 *                  symbols and local reset-stack control are the only inputs;
 *                  this call precedes main RAM initialization. The caller must
 *                  fail-stop on false before any constructor or main executes.
 */
bool nx_stm32_memory_boot(void);

#endif
