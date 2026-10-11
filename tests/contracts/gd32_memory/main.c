/**
 * \file            main.c
 * \brief           Real platform optional-memory domain and GC ELF fixture
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/memory.h"
#include "nexus_bindings.h"

volatile uint32_t g_memory_sink;

#if defined(NEXUS_MEMORY_ADDSRAM) && NEXUS_MEMORY_ADDSRAM == 1
NX_MEMORY_ADDSRAM_DATA volatile uint32_t g_addsram_data = 0x11223344U;
NX_MEMORY_ADDSRAM_BSS volatile uint32_t g_addsram_bss[4];
NX_MEMORY_ADDSRAM_NOINIT volatile uint32_t g_addsram_noinit[4];
NX_MEMORY_ADDSRAM_DATA volatile uint32_t g_addsram_dead_data[16]
    __attribute__((used)) = {0x55667788U};
NX_MEMORY_ADDSRAM_BSS volatile uint32_t g_addsram_dead_bss[16]
    __attribute__((used));
NX_MEMORY_ADDSRAM_NOINIT volatile uint32_t g_addsram_dead_noinit[16]
    __attribute__((used));
#endif

#if defined(NEXUS_MEMORY_TCM) && NEXUS_MEMORY_TCM == 1
NX_MEMORY_TCM_DATA volatile uint32_t g_tcm_data = 0x99AABBCCU;
NX_MEMORY_TCM_BSS volatile uint32_t g_tcm_bss[4];
NX_MEMORY_TCM_NOINIT volatile uint32_t g_tcm_noinit[4];
NX_MEMORY_TCM_DATA volatile uint32_t g_tcm_dead_data[16]
    __attribute__((used)) = {0xDDEEFF00U};
NX_MEMORY_TCM_BSS volatile uint32_t g_tcm_dead_bss[16] __attribute__((used));
NX_MEMORY_TCM_NOINIT volatile uint32_t g_tcm_dead_noinit[16]
    __attribute__((used));
#endif

/** \brief Link the actual platform without claiming execution on silicon. */
int main(void) {
    nx_platform_start_result_t result = nx_platform_start();
    if (result.primary != NX_SUCCESS) {
        return 1;
    }
#if defined(NEXUS_MEMORY_ADDSRAM) && NEXUS_MEMORY_ADDSRAM == 1
    g_addsram_noinit[0] = 0U;
    g_memory_sink = g_addsram_data + g_addsram_bss[0] + g_addsram_noinit[0];
#endif
#if defined(NEXUS_MEMORY_TCM) && NEXUS_MEMORY_TCM == 1
    g_tcm_noinit[0] = 0U;
    g_memory_sink += g_tcm_data + g_tcm_bss[0] + g_tcm_noinit[0];
#endif
    for (;;) {
    }
}
