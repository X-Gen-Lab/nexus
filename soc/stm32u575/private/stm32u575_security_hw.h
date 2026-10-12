/**
 * \file            stm32u575_security_hw.h
 * \brief           Private U575 MMIO and cross-world instruction leaves
 * \author          Nexus Team
 */
#ifndef NX_STM32U575_SECURITY_HW_H
#define NX_STM32U575_SECURITY_HW_H
#include "nexus/arch/arch.h"
#include "nexus/core/fault.h"
#include "stm32u575xx.h"
#if !defined(__ARM_FEATURE_CMSE) || (__ARM_FEATURE_CMSE & 2) == 0
#error "U575 dual-world bootstrap requires an explicit Secure CMSE image"
#endif
#ifdef NX_U575_SECURITY_MODEL
#include "u575_security_model.h"
#define nx_tz_read(reg) nx_u575_tz_model_read((uintptr_t)(reg))
#define nx_tz_write(reg, value)                                                \
    nx_u575_tz_model_write((uintptr_t)(reg), (uint32_t)(value))
#define nx_tz_msp()                 nx_u575_tz_model_msp()
#define nx_tz_control()             nx_u575_tz_model_control()
#define nx_tz_clear(address, bytes) nx_u575_tz_model_clear(address, bytes)
#define nx_tz_enter(stack, limit, entry)                                       \
    nx_u575_tz_model_enter(stack, limit, entry)
#else
#include <arm_cmse.h>
static inline uint32_t nx_tz_read(volatile const uint32_t* reg) {
    return *reg;
}
static inline void nx_tz_write(volatile uint32_t* reg, uint32_t value) {
    *reg = value;
}
static inline uint32_t nx_tz_msp(void) {
    return __get_MSP();
}
static inline uint32_t nx_tz_control(void) {
    return __get_CONTROL();
}
/** \brief Clear future shared storage while its physical blocks remain Secure.
 */
static inline bool nx_tz_clear(uint32_t address, uint32_t bytes) {
    volatile uint32_t* words = (volatile uint32_t*)(uintptr_t)address;
    for (uint32_t i = 0U; i < bytes / sizeof(*words); ++i) {
        words[i] = 0U;
    }
    nx_arch_dsb();
    for (uint32_t i = 0U; i < bytes / sizeof(*words); ++i) {
        if (words[i] != 0U) {
            return false;
        }
    }
    return true;
}
/** \brief Compiler-owned CMSE transition; no arbitrary NonSecure callback API.
 */
static inline _Noreturn void nx_tz_enter(uint32_t stack, uint32_t limit,
                                         uint32_t entry) {
    typedef void (*nonsecure_entry_t)(void)
        __attribute__((cmse_nonsecure_call));
    __TZ_set_MSP_NS(stack);
    __TZ_set_MSPLIM_NS(limit);
    __TZ_set_PSP_NS(0U);
    __TZ_set_PSPLIM_NS(0U);
    __TZ_set_BASEPRI_NS(0U);
    __TZ_set_FAULTMASK_NS(0U);
    __TZ_set_PRIMASK_NS(0U);
    __TZ_set_CONTROL_NS(1U);
    nx_arch_dsb();
    nx_arch_isb();
    __set_PRIMASK(0U);
    nonsecure_entry_t next = (nonsecure_entry_t)(uintptr_t)entry;
    next = cmse_nsfptr_create(next);
    next();
    (void)nx_arch_irq_save();
    nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_STATE, __FILE__, __LINE__);
    for (;;) {
    }
}
#endif
#endif /* NX_STM32U575_SECURITY_HW_H */
