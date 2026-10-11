/**
 * \file            hardware.h
 * \brief           Secure CPU boundary with exact dual-mask preservation
 * \author          Nexus Team
 */
#ifndef NEXUS_FREERTOS_SECURE_HARDWARE_H
#define NEXUS_FREERTOS_SECURE_HARDWARE_H
#include "nexus/arch/arch.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef NEXUS_FREERTOS_SECURE_MODEL
#include "secure_context_model.h"
#define NX_SECURE_GATEWAY
#define NX_SECURE_NONSECURE_CALLER() nx_secure_model_nonsecure_caller()
#define NX_SECURE_READ(name, reg)                                              \
    static inline uintptr_t nx_secure_hw_##name(void) {                        \
        return nx_secure_model_read(NX_SECURE_REG_##reg);                      \
    }
#define NX_SECURE_WRITE(name, reg)                                             \
    static inline void nx_secure_hw_##name(uintptr_t value) {                  \
        nx_secure_model_write(NX_SECURE_REG_##reg, value);                     \
    }
NX_SECURE_READ(primask, PRIMASK)
NX_SECURE_READ(primask_ns, PRIMASK_NS)
NX_SECURE_READ(psp, PSP)
NX_SECURE_READ(psplim, PSPLIM)
NX_SECURE_READ(aircr, AIRCR)
NX_SECURE_READ(nsacr, NSACR)
NX_SECURE_READ(fpccr, FPCCR)
NX_SECURE_WRITE(set_primask, PRIMASK)
NX_SECURE_WRITE(set_primask_ns, PRIMASK_NS)
NX_SECURE_WRITE(set_psp, PSP)
NX_SECURE_WRITE(set_psplim, PSPLIM)
NX_SECURE_WRITE(set_control, CONTROL)
NX_SECURE_WRITE(set_aircr, AIRCR)
NX_SECURE_WRITE(set_nsacr, NSACR)
NX_SECURE_WRITE(set_fpccr, FPCCR)
#undef NX_SECURE_READ
#undef NX_SECURE_WRITE
static inline bool nx_secure_hw_writable(void* pointer, size_t bytes) {
    return nx_secure_model_secure_writable(pointer, bytes);
}
#else
#if !defined(__ARM_FEATURE_CMSE) || (__ARM_FEATURE_CMSE & 2) == 0
#error "Secure companion mechanisms require explicit Secure CMSE compilation"
#endif
#include <arm_cmse.h>
#define NX_SECURE_GATEWAY            __attribute__((cmse_nonsecure_entry, used))
#define NX_SECURE_NONSECURE_CALLER() cmse_nonsecure_caller()
#define NX_SECURE_READ(name, reg)                                              \
    static inline uintptr_t nx_secure_hw_##name(void) {                        \
        uintptr_t value;                                                       \
        __asm__ volatile("mrs %0, " #reg : "=r"(value)::"memory");             \
        return value;                                                          \
    }
#define NX_SECURE_WRITE(name, reg)                                             \
    static inline void nx_secure_hw_##name(uintptr_t value) {                  \
        __asm__ volatile("msr " #reg ", %0" ::"r"(value) : "memory");          \
    }
NX_SECURE_READ(primask, primask)
NX_SECURE_READ(primask_ns, primask_ns)
NX_SECURE_READ(psp, psp)
NX_SECURE_READ(psplim, psplim)
NX_SECURE_WRITE(set_primask, primask)
NX_SECURE_WRITE(set_primask_ns, primask_ns)
NX_SECURE_WRITE(set_psp, psp)
NX_SECURE_WRITE(set_psplim, psplim)
NX_SECURE_WRITE(set_control, control)
#undef NX_SECURE_READ
#undef NX_SECURE_WRITE
#define NX_SECURE_MMIO_READ(name, address)                                     \
    static inline uintptr_t nx_secure_hw_##name(void) {                        \
        return *(volatile const uint32_t*)(address);                           \
    }
#define NX_SECURE_MMIO_WRITE(name, address)                                    \
    static inline void nx_secure_hw_##name(uintptr_t value) {                  \
        *(volatile uint32_t*)(address) = (uint32_t)value;                      \
    }
NX_SECURE_MMIO_READ(aircr, 0xe000ed0cu)
NX_SECURE_MMIO_READ(nsacr, 0xe000ed8cu)
NX_SECURE_MMIO_READ(fpccr, 0xe000ef34u)
NX_SECURE_MMIO_WRITE(set_aircr, 0xe000ed0cu)
NX_SECURE_MMIO_WRITE(set_nsacr, 0xe000ed8cu)
NX_SECURE_MMIO_WRITE(set_fpccr, 0xe000ef34u)
#undef NX_SECURE_MMIO_READ
#undef NX_SECURE_MMIO_WRITE
static inline bool nx_secure_hw_writable(void* pointer, size_t bytes) {
    uintptr_t last = (uintptr_t)pointer + bytes - 1u;
    if (!cmse_TT(pointer).flags.secure || !cmse_TT((void*)last).flags.secure) {
        return false;
    }
    return cmse_check_address_range(pointer, bytes, CMSE_MPU_READWRITE) != NULL;
}
#endif

/** \brief Saved incoming Secure and Nonsecure masks; never an SMP lock. */
typedef struct {
    uint32_t secure;
    uint32_t nonsecure;
} nx_secure_masks_t;

static inline nx_secure_masks_t nx_secure_masks_save(void) {
    nx_secure_masks_t saved = {(uint32_t)nx_secure_hw_primask(),
                               (uint32_t)nx_secure_hw_primask_ns()};
    nx_secure_hw_set_primask(1);
    nx_secure_hw_set_primask_ns(1);
    nx_arch_dsb();
    nx_arch_isb();
    return saved;
}

static inline void nx_secure_masks_restore(nx_secure_masks_t saved) {
    nx_arch_dsb();
    nx_secure_hw_set_primask_ns(saved.nonsecure);
    nx_secure_hw_set_primask(saved.secure);
    nx_arch_isb();
}
#endif /* NEXUS_FREERTOS_SECURE_HARDWARE_H */
