/**
 * \file            unselected.c
 * \brief           Kept manual CCM storage cannot bypass unselected linker
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include <stdint.h>

extern volatile uint32_t g_memory_sink;
__attribute__((section(
    ".nx_ccm_data.manual"))) volatile uint32_t g_rejected_memory = 0x11223344U;

/** \brief The kept init-array reference prevents unused-section rejection. */
static void touch_unselected(void) __attribute__((constructor));
static void touch_unselected(void) {
    g_memory_sink = g_rejected_memory;
}
