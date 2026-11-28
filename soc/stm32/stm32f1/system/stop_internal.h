/**
 * \file            stop_internal.h
 * \brief           Masked handoff between F1 STOP and the TIM2 owner.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_STOP_INTERNAL_H
#define NEXUS_STM32F1_STOP_INTERNAL_H

#include "drivers/exti/instance.h"
#include "nexus/soc/stm32f103_stop.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Exact masked hardware transition for the generated wake owner. */
nx_stm32f1_stop_receipt_t nx_stm32f1_stop_hsi8(nx_clock_barrier_t barrier,
                                               const nx_stm32f1_exti_t* wake);

/** \note All helpers execute only under the one private STOP caller's PRIMASK.
 */
bool nx_stm32f1_stop_time_ready(void);
nx_result_t nx_stm32f1_stop_time_suspend(nx_stm32f1_stop_receipt_t* receipt);
nx_result_t nx_stm32f1_stop_time_restore(nx_stm32f1_stop_receipt_t* receipt);

#ifdef __cplusplus
}
#endif

#endif
