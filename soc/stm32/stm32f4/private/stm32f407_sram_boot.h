/**
 * \file            stm32f407_sram_boot.h
 * \brief           Reset-only checked external SRAM placement bootstrap
 * \author          Nexus Team
 */
#ifndef NX_STM32F407_SRAM_BOOT_H
#define NX_STM32F407_SRAM_BOOT_H
#include "memory_init.h"
#ifdef __cplusplus
extern "C" {
#endif
struct nx_stm32_fsmc_bank_config;
/** \brief Exact software-declared SRAM identity, not an inferred speed grade.
 */
enum {
    NX_STM32_SRAM_BOOT_VERSION = 1U,
    NX_STM32_SRAM_IS62WV51216 = 1U,
    NX_STM32_SRAM_BOOT_MIN_STACK = 512U
};
/** \brief One immutable internal-Flash plan, selected explicitly by assembly.
 * \note The tagged bank descriptor is the same Flash object referenced by
 *       the post-C FSMC owner. region_count is 2 (primary, ADDSRAM) or 3
 *       (primary, CCM, ADDSRAM); each uses the existing linker copy/zero ABI.
 *       Primary RAM and MSP remain internal, with at least 512 reset-stack
 * bytes. The maintained compiler call-graph bound must be regenerated when
 *       compiler/options/source change; this is not a measured stack peak.
 *       ADDSRAM is NE3 68000000h/1MiB;
 *       no external code, vectors, stack, heap or DMA reachability is granted.
 *       Timing fields are authored assumptions for HCLK up to 168MHz, not
 *       measured electrical timing or an invented device speed grade.
 *       Initial source/control bytes and every selected plan/config object
 *       reside in internal physical Flash. NOINIT and padding are untouched.
 */
typedef struct {
    uint32_t version;
    uint32_t part;
    uint32_t maximum_hclk_hz;
    uint32_t region_count;
    uintptr_t flash_start;
    uintptr_t flash_end;
    uintptr_t vector_address;
    uintptr_t msp_start;
    uintptr_t msp_end;
    const struct nx_stm32_fsmc_bank_config* bank;
    nx_stm32_memory_region_t regions[3];
} nx_stm32_sram_boot_plan_t;
/** \brief Validate complete Flash/control/copy/zero/MSP facts without effects.
 * \param[in] plan: Exact immutable Flash descriptor, naturally aligned.
 * \return True only for the selected F407ZG internal Flash/NE3 layout.
 * \note Pure validation does not establish reset provenance or SRAM fitting.
 */
bool nx_stm32_sram_boot_plan_valid(const nx_stm32_sram_boot_plan_t* plan);
/** \brief Prepare exact NE3 and initialize only its selected DATA/BSS sections.
 * \param[in] plan: Same immutable complete Flash plan; all hardware is reset.
 * \return True after profile readback and section initialization, else false.
 * \note Called once at the first SystemInit statement, using internal MSP,
 *       before any initialized RAM, global writes, libc or constructors.
 *       Cold HSI16/reset-like clocks and quiet IRQs only; no warm handoff or
 *       offset image. False is terminal: caller parks before entering C main.
 *       GPIO/FSMC/NE3 ownership is persistent; no rollback or clean global
 *       shutdown while placed SRAM objects can exist. No hidden SRAM probe.
 */
bool nx_stm32_sram_bootstrap(const nx_stm32_sram_boot_plan_t* plan);
/** \brief Prove the exact persistent NE3/pad/timing profile after C startup.
 * \param[in] plan: The same original immutable Flash plan.
 * \return True only when plan and actual boot-owned hardware still match.
 * \note Read-only; no assumption of reset HSI after the system raises clocks.
 *       This proves register identity, not hardware fitting, speed or HIL.
 */
bool nx_stm32_sram_boot_matches(const nx_stm32_sram_boot_plan_t* plan);
#ifdef __cplusplus
}
#endif
#endif
