/**
 * \file            arch_sleep_model.h
 * \brief           Test-only shallow-sleep hardware boundary
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_ARCH_SLEEP_MODEL_H
#define NEXUS_ARCH_SLEEP_MODEL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_arch_sleep_model_control(void);
void nx_arch_sleep_model_wfi(void);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_ARCH_SLEEP_MODEL_H */
