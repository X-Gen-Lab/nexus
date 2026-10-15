/**
 * \file            memory.h
 * \brief           Variable-only placement in explicitly selected data regions
 * \author          Nexus Team
 */
#ifndef NEXUS_CORE_MEMORY_H
#define NEXUS_CORE_MEMORY_H

#include "nexus_config.h"

/**
 * \brief           Place static storage in an authored optional memory region.
 * \note            Use these attributes on variable definitions, before the
 *                  type, one variable per declaration. They are not function
 *                  or instruction attributes. Each use has an independent
 *                  input section for garbage collection of unused storage.
 *                  DATA copies its initializer from Flash at reset. BSS is
 *                  zeroed at reset. NOINIT is neither copied nor cleared;
 *                  initialize it before reading. Power-loss retention is not
 *                  promised. BSS/NOINIT reject nonzero initializers. Storage
 *                  is word aligned and subject to explicit ELF budgets.
 *                  Unselected use is a compile error. TCM is CPU-only data:
 *                  never DMA or executable storage. CCM is also CPU-only data;
 *                  placement never grants bus-master access. ADDSRAM DMA
 * requires provider address, alignment and ownership validation. Ordinary
 * storage, the main MSP and unselected images keep their existing layout.
 */
#if !defined(__GNUC__) && !defined(__clang__)
#error                                                                         \
    "Optional memory placement requires the maintained GNU-compatible compiler"
#endif

#define NX_CORE_MEMORY_STRING_DETAIL(value) #value
#define NX_CORE_MEMORY_STRING(value)        NX_CORE_MEMORY_STRING_DETAIL(value)
#define NX_CORE_MEMORY_ATTRIBUTE(name)                                         \
    __attribute__((section(name "." NX_CORE_MEMORY_STRING(__COUNTER__)),       \
                   aligned(4), nocommon))

#if defined(NEXUS_MEMORY_ADDSRAM) && NEXUS_MEMORY_ADDSRAM == 1
#define NX_MEMORY_ADDSRAM_DATA NX_CORE_MEMORY_ATTRIBUTE(".nx_addsram_data")
#define NX_MEMORY_ADDSRAM_BSS  NX_CORE_MEMORY_ATTRIBUTE(".bss.nx_addsram")
#define NX_MEMORY_ADDSRAM_NOINIT                                               \
    NX_CORE_MEMORY_ATTRIBUTE(".bss.nx_addsram_noinit")
#else
#define NX_MEMORY_ADDSRAM_DATA                                                 \
    _Pragma("GCC error \"ADDSRAM memory region is not selected\"")
#define NX_MEMORY_ADDSRAM_BSS    NX_MEMORY_ADDSRAM_DATA
#define NX_MEMORY_ADDSRAM_NOINIT NX_MEMORY_ADDSRAM_DATA
#endif

#if defined(NEXUS_MEMORY_TCM) && NEXUS_MEMORY_TCM == 1
#define NX_MEMORY_TCM_DATA   NX_CORE_MEMORY_ATTRIBUTE(".nx_tcm_data")
#define NX_MEMORY_TCM_BSS    NX_CORE_MEMORY_ATTRIBUTE(".bss.nx_tcm")
#define NX_MEMORY_TCM_NOINIT NX_CORE_MEMORY_ATTRIBUTE(".bss.nx_tcm_noinit")
#else
#define NX_MEMORY_TCM_DATA                                                     \
    _Pragma("GCC error \"TCM memory region is not selected\"")
#define NX_MEMORY_TCM_BSS    NX_MEMORY_TCM_DATA
#define NX_MEMORY_TCM_NOINIT NX_MEMORY_TCM_DATA
#endif

#if defined(NEXUS_MEMORY_CCM) && NEXUS_MEMORY_CCM == 1
#define NX_MEMORY_CCM_DATA   NX_CORE_MEMORY_ATTRIBUTE(".nx_ccm_data")
#define NX_MEMORY_CCM_BSS    NX_CORE_MEMORY_ATTRIBUTE(".bss.nx_ccm")
#define NX_MEMORY_CCM_NOINIT NX_CORE_MEMORY_ATTRIBUTE(".bss.nx_ccm_noinit")
#else
#define NX_MEMORY_CCM_DATA                                                     \
    _Pragma("GCC error \"CCM memory region is not selected\"")
#define NX_MEMORY_CCM_BSS    NX_MEMORY_CCM_DATA
#define NX_MEMORY_CCM_NOINIT NX_MEMORY_CCM_DATA
#endif

#endif
