/**
 * \file            compiler.h
 * \brief           Host-only CMSIS compiler declaration selection
 * \author          Nexus Team
 *
 * Register models require the original GNU-compatible declarations, including
 * their Arm instruction bodies. This bridge supplies no instruction emulation
 * and must precede any Core header or forced model prelude. Firmware cannot use
 * it. Compiler identity, ABI and instruction feature macros remain unchanged.
 */

#ifndef NEXUS_TESTS_CMSIS_HOST_COMPILER_H
#define NEXUS_TESTS_CMSIS_HOST_COMPILER_H

#include "arm_acle.h"
#include <stdint.h>

#if defined(__CMSIS_COMPILER_H) || defined(__CMSIS_GCC_H) ||                   \
    defined(__CMSIS_CLANG_H)
#error "The CMSIS host compiler bridge must precede Core headers"
#endif

/* CMSIS 6 selects its ACLE-only Clang adapter on an x86 Clang frontend too.
 * Select the unchanged GNU syntax adapter explicitly for this private model,
 * before the vendor entry point can select an Arm compiler implementation.
 */
#define __CMSIS_COMPILER_H
#ifdef __cplusplus
extern "C" {
#endif
#include <cmsis_gcc.h>
#ifdef __cplusplus
}
#endif

#endif /* NEXUS_TESTS_CMSIS_HOST_COMPILER_H */
