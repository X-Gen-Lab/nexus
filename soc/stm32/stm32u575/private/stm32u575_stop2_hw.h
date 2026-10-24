/**
 * \file            stm32u575_stop2_hw.h
 * \brief           Private U575 MMIO and deep wait instruction boundary
 * \author          Nexus Team
 */
#ifndef NX_STM32U575_STOP2_HW_H
#define NX_STM32U575_STOP2_HW_H
#include "stm32u575xx.h"
#ifdef NX_U575_STOP2_MODEL
#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_u575_stop2_read(uintptr_t address);
void nx_u575_stop2_write(uintptr_t address, uint32_t value);
uint32_t nx_u575_stop2_control(void);
void nx_u575_stop2_wait(void);
#ifdef __cplusplus
}
#endif
#define NX_STOP_READ(reg) nx_u575_stop2_read((uintptr_t) & (reg))
#define NX_STOP_WRITE(reg, value)                                              \
    nx_u575_stop2_write((uintptr_t) & (reg), (uint32_t)(value))
#define NX_STOP_FLASH_KIB() nx_u575_stop2_read(FLASHSIZE_BASE)
#define NX_STOP_CONTROL()   nx_u575_stop2_control()
#define NX_STOP_WAIT()      nx_u575_stop2_wait()
#else
#if !defined(__ARM_ARCH_8M_MAIN__)
#error "U575 Stop2 requires the selected Cortex-M33 compiler profile"
#endif
#define NX_STOP_FLASH_KIB()       (*(volatile const uint16_t*)FLASHSIZE_BASE)
#define NX_STOP_READ(reg)         ((uint32_t)(reg))
#define NX_STOP_WRITE(reg, value) ((reg) = (uint32_t)(value))
static inline uint32_t nx_u575_stop2_control(void) {
    uint32_t value;
    __asm__ volatile("mrs %0, control" : "=r"(value)::"memory");
    return value;
}
static inline void nx_u575_stop2_wait(void) {
    __asm__ volatile("dsb\n\twfi\n\tisb" ::: "memory");
}
#define NX_STOP_CONTROL()         nx_u575_stop2_control()
#define NX_STOP_WAIT()            nx_u575_stop2_wait()
#endif
#endif /* NX_STM32U575_STOP2_HW_H */
