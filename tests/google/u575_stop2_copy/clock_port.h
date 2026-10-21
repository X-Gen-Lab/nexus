/** \file clock_port.h
 * \brief Route real retained calendar reads through the shared external model.
 * \author Nexus Team
 * \version 1.0.0
 * \date 2026-10-20
 * \copyright Copyright (c) 2026 Nexus Team
 */
#ifndef NX_U575_STOP2_COPY_CLOCK_PORT_H
#define NX_U575_STOP2_COPY_CLOCK_PORT_H
#include "stm32u575_rtc.h"
#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_u575_stop2_read(uintptr_t address);
#ifdef __cplusplus
}
#endif
#undef NX_U575_READ
#define NX_U575_READ(pointer) nx_u575_stop2_read((uintptr_t)(pointer))
#define NX_U575_RTC_IRQ_QUIET()                                                \
    (nx_u575_stop2_read((uintptr_t)&NVIC->ISER[0]) == 0U &&                    \
     nx_u575_stop2_read((uintptr_t)&NVIC->ISPR[0]) == 0U &&                    \
     nx_u575_stop2_read((uintptr_t)&NVIC->IABR[0]) == 0U)
#endif
