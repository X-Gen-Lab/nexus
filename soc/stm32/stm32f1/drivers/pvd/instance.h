/**
 * \file            instance.h
 * \brief           Private F103xB polling comparator instance.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_PVD_INSTANCE_H
#define NEXUS_STM32F1_PVD_INSTANCE_H

#include "nexus/io/exti.h"

/** \brief Resources retained until comparator and gate are proven restored. */
typedef struct {
    uint8_t effects;
    uint8_t saved_code;
    uint8_t selected_code;
} nx_stm32f1_pvd_t;

extern const nx_exti_ops_t nx_stm32f1_pvd_ops;

/** \brief Reject foreign or unavailable ownership without modifying hardware.
 */
nx_result_t nx_stm32f1_pvd_admit(const nx_stm32f1_pvd_t* port, uint8_t code);
/** \brief Enable the comparator with an exact raw PLS code 0 through 7. */
nx_result_t nx_stm32f1_pvd_prepare(nx_stm32f1_pvd_t* port, uint8_t code);
/** \brief Restore inactive PLS and release the acquired PWR clock. */
nx_result_t nx_stm32f1_pvd_close(nx_stm32f1_pvd_t* port);
#endif
