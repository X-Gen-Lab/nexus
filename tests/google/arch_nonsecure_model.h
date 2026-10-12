/**
 * \file            arch_nonsecure_model.h
 * \brief           Hardware CMSE instruction boundary for host contracts
 * \author          Nexus Team
 */
#ifndef NEXUS_TESTS_ARCH_NONSECURE_MODEL_H
#define NEXUS_TESTS_ARCH_NONSECURE_MODEL_H
#include "nexus/arch/nonsecure.h"
#include <stddef.h>
/* Match the locked Arm GNU arm_cmse.h constants, not an emulated MPU policy. */
#define CMSE_NONSECURE     18
#define CMSE_MPU_UNPRIV    4
#define CMSE_MPU_READ      8
#define CMSE_MPU_READWRITE 1
#ifdef __cplusplus
extern "C" {
#endif
void* nx_arch_model_nonsecure_range(void* address, size_t bytes, int flags);
/** \brief Pass malformed integer inputs through C rather than C++ enum casts.
 */
nx_arch_result_t nx_arch_nonsecure_test_raw(const void* address, size_t bytes,
                                            unsigned access);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_TESTS_ARCH_NONSECURE_MODEL_H */
