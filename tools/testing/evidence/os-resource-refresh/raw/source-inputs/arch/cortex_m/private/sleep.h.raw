/**
 * \file            sleep.h
 * \brief           Private WFI and sleep-control hardware boundary
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_ARCH_SLEEP_PRIVATE_H
#define NEXUS_ARCH_SLEEP_PRIVATE_H
#include <stdint.h>
#if defined(NEXUS_ARCH_SLEEP_MODEL)
#include "arch_sleep_model.h"
static inline uint32_t nx_arch_sleep_control(void) {
    return nx_arch_sleep_model_control();
}
static inline void nx_arch_sleep_instruction(void) {
    nx_arch_sleep_model_wfi();
}
#elif defined(__arm__) || defined(__thumb__)
static inline uint32_t nx_arch_sleep_control(void) {
    return *(volatile const uint32_t*)0xe000ed10u;
}
static inline void nx_arch_sleep_instruction(void) {
    __asm__ volatile("wfi" ::: "memory");
}
#endif
#endif /* NEXUS_ARCH_SLEEP_PRIVATE_H */
