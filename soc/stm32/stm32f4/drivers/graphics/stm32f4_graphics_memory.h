/**
 * \file            stm32f4_graphics_memory.h
 * \brief           Static SDRAM hold shared by two selected graphics owners
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-13
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_STM32F4_GRAPHICS_MEMORY_H
#define NX_STM32F4_GRAPHICS_MEMORY_H
#include "nexus/io/memory_window.h"
#ifdef __cplusplus
extern "C" {
#endif
struct nx_stm32_sdram;
typedef enum {
    NX_STM32_SDRAM_GRAPHICS_LTDC = 0,
    NX_STM32_SDRAM_GRAPHICS_DMA2D = 1,
    NX_STM32_SDRAM_GRAPHICS_MASTER_COUNT = 2
} nx_stm32_sdram_graphics_master_t;
/** \brief Selected binding storage; no object exists in a CPU-only assembly.
 * \note Initialize owner statically and all other fields to zero before use.
 *       The original private lease remains held until both consumers cold join.
 *       Fields are private to these helpers; no public CPU mapping is granted.
 */
typedef struct nx_stm32_sdram_graphics {
    struct nx_stm32_sdram* owner;
    nx_memory_window_lease_t lease;
    const void* consumers[2];
} nx_stm32_sdram_graphics_t;
/** \brief Reject a graphics buffer/query span over retained bank metadata.
 * \note Pure range test; malformed or stopped bindings conservatively overlap.
 *       It neither dereferences pixels nor treats a physical bus address as a
 *       CPU pointer. The binding, bank, config and original snapshots stay
 * live.
 */
bool nx_stm32_sdram_graphics_overlaps(const nx_stm32_sdram_graphics_t* binding,
                                      const void* storage, size_t bytes);
/** \brief Attach one exact prepared-engine identity before its first effect.
 * \note One privileged unmasked task serializes this binding and bank. The
 *       stable System identity and original epoch must match the SDRAM owner.
 *       Failure retains no new consumer. Existing holds are never revoked.
 */
nx_result_t
nx_stm32_sdram_graphics_attach(nx_stm32_sdram_graphics_t* binding,
                               nx_stm32_sdram_graphics_master_t master,
                               const void* consumer, const void* system);
/** \brief Verify the retained bank, consumer and live original System domain.
 * \note Read-only request admission check; no data or device access is granted
 *       beyond the per-consumer selected memory regions.
 */
nx_result_t
nx_stm32_sdram_graphics_check(const nx_stm32_sdram_graphics_t* binding,
                              nx_stm32_sdram_graphics_master_t master,
                              const void* consumer, const void* system);
/** \brief Detach only after that engine's complete hardware/IRQ cold join.
 * \note Private driver contract, never an application release operation. The
 *       last successful detach releases the original bank lease. Failure keeps
 *       its consumer and lease, including when System evidence is lost.
 */
nx_result_t
nx_stm32_sdram_graphics_detach(nx_stm32_sdram_graphics_t* binding,
                               nx_stm32_sdram_graphics_master_t master,
                               const void* consumer);
#ifdef __cplusplus
}
#endif
#endif
