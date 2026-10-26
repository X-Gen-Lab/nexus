/**
 * \file            g474_flash_geometry.c
 * \brief           G474 standalone geometry oracle
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-26
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/flash.h"

/* Standalone models keep an independent hardware reference. Factory models
 * consume the generated selected-part geometry, never this test-only object.
 */
#define PAGE(n)                                                                \
    { (n) * 2048U, 2048U }
#define PAGES16(n)                                                             \
    PAGE(n), PAGE((n) + 1U), PAGE((n) + 2U), PAGE((n) + 3U), PAGE((n) + 4U),   \
        PAGE((n) + 5U), PAGE((n) + 6U), PAGE((n) + 7U), PAGE((n) + 8U),        \
        PAGE((n) + 9U), PAGE((n) + 10U), PAGE((n) + 11U), PAGE((n) + 12U),     \
        PAGE((n) + 13U), PAGE((n) + 14U), PAGE((n) + 15U)
static const nx_flash_sector_t s_sectors[] = {
    PAGES16(0U),   PAGES16(16U),  PAGES16(32U),  PAGES16(48U),
    PAGES16(64U),  PAGES16(80U),  PAGES16(96U),  PAGES16(112U),
    PAGES16(128U), PAGES16(144U), PAGES16(160U), PAGES16(176U),
    PAGES16(192U), PAGES16(208U), PAGES16(224U), PAGES16(240U)};
const nx_flash_geometry_t g_nx_stm32g474_flash_geometry = {
    .base_address = 0x08000000U,
    .size = 524288U,
    .program_unit = 8U,
    .sectors = s_sectors,
    .sector_count = 256U,
    .program_flags = NX_FLASH_PROGRAM_REQUIRES_FRESH_ERASE};
