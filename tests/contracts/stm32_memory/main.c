/**
 * \file            main.c
 * \brief           Actual F407 CPU-only CCM placement and section-GC fixture
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/memory.h"
#include "nexus_bindings.h"

volatile uint32_t g_memory_sink;

#if defined(NEXUS_MEMORY_CCM) && NEXUS_MEMORY_CCM == 1
NX_MEMORY_CCM_DATA volatile uint32_t g_ccm_data = 0x11223344U;
NX_MEMORY_CCM_BSS volatile uint32_t g_ccm_bss[4];
NX_MEMORY_CCM_NOINIT volatile uint32_t g_ccm_noinit[4];
NX_MEMORY_CCM_DATA volatile uint32_t g_ccm_dead_data[16]
    __attribute__((used)) = {0x55667788U};
NX_MEMORY_CCM_BSS volatile uint32_t g_ccm_dead_bss[16] __attribute__((used));
NX_MEMORY_CCM_NOINIT volatile uint32_t g_ccm_dead_noinit[16]
    __attribute__((used));
#endif

/** \brief Link the actual platform; software linking does not execute MCU. */
int main(void) {
    nx_platform_start_result_t result = nx_platform_start();
    if (result.primary != NX_SUCCESS) {
        return 1;
    }
#if defined(NEXUS_MEMORY_CCM) && NEXUS_MEMORY_CCM == 1
    g_ccm_noinit[0] = 0U;
    g_memory_sink = g_ccm_data + g_ccm_bss[0] + g_ccm_noinit[0];
#endif
    for (;;) {
    }
}
