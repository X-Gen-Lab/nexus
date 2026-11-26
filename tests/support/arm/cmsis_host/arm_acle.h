/**
 * \file            arm_acle.h
 * \brief           Host-only CMSIS M-profile register declarations
 * \author          Nexus Team
 *
 * Native register models do not execute Arm instructions. CMSIS retains its
 * original intrinsic implementations; this header supplies no ACLE operations.
 */

#ifndef NEXUS_TESTS_CMSIS_HOST_ARM_ACLE_H
#define NEXUS_TESTS_CMSIS_HOST_ARM_ACLE_H

#if defined(__arm__) || defined(__thumb__) || defined(__aarch64__)
#error "The CMSIS host model shim must not enter an Arm build"
#endif

#if !defined(NEXUS_CMSIS_HOST_MODEL) || NEXUS_CMSIS_HOST_MODEL != 1
#error "The CMSIS shim requires an explicit host register model"
#endif

#if defined(__ARM_ARCH_PROFILE) && __ARM_ARCH_PROFILE != 'M'
#error "The CMSIS host register model requires the M profile"
#endif

#ifndef __ARM_ARCH_PROFILE
#define __ARM_ARCH_PROFILE 'M'
#endif

#endif /* NEXUS_TESTS_CMSIS_HOST_ARM_ACLE_H */
