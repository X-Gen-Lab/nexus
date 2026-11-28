/**
 * \file            standby.h
 * \brief           Private F103RB PA0 WKUP Standby boundary.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_STANDBY_H
#define NEXUS_STM32F1_STANDBY_H

#include "nexus/soc/stm32f103_standby.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief         Attempt Standby after full Platform stop and caller drain.
 * \note          Exact HSI8 baremetal, PA0 as WKUP, pre-owned PWR and GPIOA.
 *                True Standby exits through reset. WFI returning is an error.
 *                Flag snapshots do not identify a specific reset cause.
 *                A false entry leaves the Platform stopped for explicit start.
 */
nx_stm32f1_standby_receipt_t nx_stm32f1_standby_pa0(nx_clock_barrier_t barrier,
                                                    bool discard_wuf);

#ifdef __cplusplus
}
#endif
#endif
