/**
 * \file            stm32f407_fsmc_io.h
 * \brief           Private bounded external-memory CPU and storage boundaries
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_FSMC_IO_H
#define NEXUS_STM32F407_FSMC_IO_H
#include "stm32f407_fsmc.h"
#ifndef NX_STM32_FSMC_READ8
#define NX_STM32_FSMC_READ8(address) (*(address))
#endif
#ifndef NX_STM32_FSMC_WRITE8
#define NX_STM32_FSMC_WRITE8(address, value) (*(address) = (value))
#endif
#ifndef NX_STM32_FSMC_READ16
#define NX_STM32_FSMC_READ16(address) (*(address))
#endif
#ifndef NX_STM32_FSMC_WRITE16
#define NX_STM32_FSMC_WRITE16(address, value) (*(address) = (value))
#endif
/** \brief Peripheral/external spans never back command control or payload. */
static inline bool nx_fsmc_span(const void* p, size_t bytes, size_t align,
                                bool writable) {
    uintptr_t a = (uintptr_t)p;
    if (p == NULL || bytes == 0U || a % align != 0U || bytes > UINTPTR_MAX - a)
        return false;
    bool low_address = true;
#if UINTPTR_MAX > UINT32_MAX
    low_address = (uint64_t)a < UINT64_C(0x100000000);
#endif
    if (low_address && UINT64_C(0x40000000) < (uint64_t)a + bytes)
        return false;
    return !writable || (uint64_t)a >= UINT64_C(0x10000000) ||
           (uint64_t)a + bytes <= UINT64_C(0x08000000);
}
/** \brief Complete representable spans have already been admitted. */
static inline bool nx_fsmc_overlap(const void* a, size_t an, const void* b,
                                   size_t bn) {
    return (uintptr_t)a < (uintptr_t)b + bn && (uintptr_t)b < (uintptr_t)a + an;
}
/** \brief Keep caller interrupt masks unchanged; no ISR command sequences. */
static inline bool nx_fsmc_task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
/** \brief One width-selected volatile bus cycle followed by actual ordering. */
static inline void nx_fsmc_write(uintptr_t address, uint16_t value,
                                 uint8_t width) {
    if (width == 8U)
        NX_STM32_FSMC_WRITE8((volatile uint8_t*)address, (uint8_t)value);
    else
        NX_STM32_FSMC_WRITE16((volatile uint16_t*)address, value);
    nx_arch_dsb();
}
/** \brief One width-selected external read, never a native RAM surrogate. */
static inline uint16_t nx_fsmc_read(uintptr_t address, uint8_t width) {
    return width == 8U
               ? NX_STM32_FSMC_READ8((const volatile uint8_t*)address)
               : NX_STM32_FSMC_READ16((const volatile uint16_t*)address);
}
#endif
