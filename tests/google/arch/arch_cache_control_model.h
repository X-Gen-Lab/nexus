/**
 * \file            arch_cache_control_model.h
 * \brief           Host-only boundary for the stack-free M7 transition leaf
 * \author          Nexus Team
 */
#ifndef NEXUS_ARCH_CACHE_CONTROL_MODEL_H
#define NEXUS_ARCH_CACHE_CONTROL_MODEL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Model one indivisible clean-all/CCR transition instruction leaf. */
uint32_t nx_arch_cache_control_model_clean_disable(uint32_t last_set,
                                                   uint32_t ccr);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_ARCH_CACHE_CONTROL_MODEL_H */
