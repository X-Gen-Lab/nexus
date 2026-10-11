/**
 * \file            clock_sleep_soc_model.h
 * \brief           CPU-boundary injection preserving real Native exclusion
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_CLOCK_SLEEP_SOC_MODEL_H
#define NEXUS_CLOCK_SLEEP_SOC_MODEL_H
#include "nexus/arch/arch.h"
#ifdef __cplusplus
extern "C" {
#endif
bool nx_clock_test_privileged(void);
uint32_t nx_clock_test_exception(void);
nx_arch_irq_masks_t nx_sleep_test_masks(void);
nx_arch_irq_state_t nx_sleep_test_irq_save(void);
#ifdef __cplusplus
}
#endif
#ifndef __cplusplus
#define nx_arch_is_privileged    nx_clock_test_privileged
#define nx_arch_exception_number nx_clock_test_exception
#define nx_arch_irq_masks        nx_sleep_test_masks
#define nx_arch_irq_save         nx_sleep_test_irq_save
#endif
#endif /* NEXUS_CLOCK_SLEEP_SOC_MODEL_H */
