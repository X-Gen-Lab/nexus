/**
 * \file            unselected.c
 * \brief           Live hand-authored storage must not bypass unselected LD
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include <stdint.h>

extern volatile uint32_t g_memory_sink;

#if defined(NX_MEMORY_REJECT_ADDSRAM)
__attribute__((
    section(".nx_addsram_data.manual"))) volatile uint32_t g_rejected_memory =
    0x11223344U;
#elif defined(NX_MEMORY_REJECT_TCM)
__attribute__((
    section(".bss.nx_tcm_noinit.manual"))) volatile uint32_t g_rejected_memory;
#else
#error "A deliberate negative fixture must select one injection"
#endif

/** \brief An actual kept init-array reference defeats unused-section GC. */
static void touch_unselected(void) __attribute__((constructor));
static void touch_unselected(void) {
    g_memory_sink = g_rejected_memory;
}
