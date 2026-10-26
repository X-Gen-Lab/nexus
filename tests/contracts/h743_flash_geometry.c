/**
 * \file            h743_flash_geometry.c
 * \brief           H743 standalone geometry oracle
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-26
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/flash.h"

/* Standalone models keep an independent hardware reference. Factory models
 * consume the generated selected-part geometry, never this test-only object.
 */
static const nx_flash_sector_t s_sectors[16] = {
    {0U, 131072U},        {0x20000U, 131072U},  {0x40000U, 131072U},
    {0x60000U, 131072U},  {0x80000U, 131072U},  {0xa0000U, 131072U},
    {0xc0000U, 131072U},  {0xe0000U, 131072U},  {0x100000U, 131072U},
    {0x120000U, 131072U}, {0x140000U, 131072U}, {0x160000U, 131072U},
    {0x180000U, 131072U}, {0x1a0000U, 131072U}, {0x1c0000U, 131072U},
    {0x1e0000U, 131072U}};
const nx_flash_geometry_t g_nx_stm32h743_flash_geometry = {
    .base_address = 0x08000000U,
    .size = 2097152U,
    .program_unit = 32U,
    .sectors = s_sectors,
    .sector_count = 16U,
    .program_flags = NX_FLASH_PROGRAM_REQUIRES_FRESH_ERASE};
