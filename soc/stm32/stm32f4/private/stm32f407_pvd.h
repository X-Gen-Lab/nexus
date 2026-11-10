/**
 * \file            stm32f407_pvd.h
 * \brief           Exclusive PVD signal ownership without voltage policy
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_PVD_H
#define NEXUS_STM32F407_PVD_H
#include "stm32f407_provider.h"

/** \brief PWR clock belongs to System; only PLS/PVDE belong to this receipt. */
typedef struct {
    uint8_t saved_level;
    uint8_t selected_level;
    uint8_t effects;
} nx_stm32_pvd_state_t;
/** \brief Prefix permits the existing bounded EXTI reader and dispatcher. */
typedef struct {
    nx_stm32_exti_state_t events;
    nx_stm32_pvd_state_t pvd;
    uint8_t saved_priority;
} nx_stm32_pvd_events_state_t;
/** \brief Caller-owned ring and exact comparator encoding, never millivolts. */
typedef struct {
    nx_exti_event_t* storage;
    size_t capacity;
    nx_exti_edge_t edge;
    uint8_t threshold_code;
    uint8_t priority;
} nx_stm32_pvd_events_config_t;

#ifdef __cplusplus
extern "C" {
#endif
extern const nx_exti_ops_t nx_stm32_pvd_ops;
extern const nx_exti_ops_t nx_stm32_pvd_events_ops;
/** \brief Read-only admission precedes System's PWR-clock acquisition. */
nx_result_t nx_stm32_pvd_admit(const nx_stm32_pvd_state_t* port,
                               uint8_t threshold_code);
/** \brief Enable only the admitted comparator after System starts. */
nx_result_t nx_stm32_pvd_prepare(nx_stm32_pvd_state_t* port,
                                 uint8_t threshold_code);
/** \brief Disable and restore comparator fields, retaining failed cleanup. */
nx_result_t nx_stm32_pvd_stop(nx_stm32_pvd_state_t* port);
/** \brief Validate the complete IRQ/ring selection without acquiring it. */
nx_result_t
nx_stm32_pvd_events_admit(const nx_stm32_pvd_events_state_t* port,
                          const nx_stm32_pvd_events_config_t* config);
/** \brief Arm EXTI16 after comparator configuration and exact pending clear. */
nx_result_t
nx_stm32_pvd_events_prepare(nx_stm32_pvd_events_state_t* port,
                            const nx_stm32_pvd_events_config_t* config);
/** \brief Drain EXTI/IRQ before disabling the comparator or releasing borrows.
 */
nx_result_t nx_stm32_pvd_events_stop(nx_stm32_pvd_events_state_t* port);
#ifdef __cplusplus
}
#endif
#endif
