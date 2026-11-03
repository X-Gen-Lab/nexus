/**
 * \file            mcuboot_assert.h
 * \brief           Product-owned fatal assertion hook; no libc diagnostics.
 * \author          Nexus Team
 */
#ifndef NEXUS_MCUBOOT_REFERENCE_ASSERT_H
#define NEXUS_MCUBOOT_REFERENCE_ASSERT_H
#include "mcuboot_config/mcuboot_config.h"
#define ASSERT(condition)                                                      \
    do {                                                                       \
        if (!(condition)) {                                                    \
            nx_mcuboot_assert_fail();                                          \
        }                                                                      \
    } while (0)
#define assert(condition) ASSERT(condition)
#endif
