/**
 * \file            u575_stop2_clock_bridge.h
 * \brief           One external MMIO model shared by both production owners
 * \author          Nexus Team
 */
#ifndef NX_U575_STOP2_CLOCK_BRIDGE_H
#define NX_U575_STOP2_CLOCK_BRIDGE_H
#include "stm32u575_stop2_hw.h"
#include "stm32u575_system.h"
#undef NX_U575_READ
#undef NX_U575_WRITE
#define NX_U575_READ(reg) nx_u575_stop2_read((uintptr_t)(reg))
#define NX_U575_WRITE(reg, value)                                              \
    nx_u575_stop2_write((uintptr_t)(reg), (uint32_t)(value))
#define NX_U575_RTC_IRQ_QUIET()                                                \
    ((nx_u575_stop2_read((uintptr_t)&NVIC->ISER[0]) & (1U << RTC_IRQn)) ==     \
         0U &&                                                                 \
     (nx_u575_stop2_read((uintptr_t)&NVIC->ISPR[0]) & (1U << RTC_IRQn)) ==     \
         0U &&                                                                 \
     (nx_u575_stop2_read((uintptr_t)&NVIC->IABR[0]) & (1U << RTC_IRQn)) == 0U)
#endif /* NX_U575_STOP2_CLOCK_BRIDGE_H */
