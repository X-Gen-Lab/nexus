/**
 * \file            compiler_abstraction.h
 * \brief           Compiler Abstraction Layer for Multi-Compiler Support
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-15
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \note            This header provides compiler-independent macros for:
 *                  - Compiler detection (GCC, Clang, IAR)
 *                  - Function attributes (inline, weak, align, section)
 *                  - Memory barriers (DSB, ISB, DMB)
 *                  - Interrupt control
 *                  - PRIMASK access
 *
 * \par             Requirements: 12.4, 12.5, 12.6, 13.6, 13.7
 */

#ifndef COMPILER_ABSTRACTION_H
#define COMPILER_ABSTRACTION_H

#include "core_config.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================*/
/* Compiler Detection                                                        */
/*===========================================================================*/

/**
 * \brief           Compiler type detection macros
 * \note            Detects GCC, Clang, and IAR compilers
 */
#if defined(__GNUC__) && !defined(__clang__)
#define COMPILER_GCC   1
#define COMPILER_CLANG 0
#define COMPILER_IAR   0
#define COMPILER_MSVC  0
#define COMPILER_NAME  "GCC"
#elif defined(__clang__)
#define COMPILER_GCC   0
#define COMPILER_CLANG 1
#define COMPILER_IAR   0
#define COMPILER_MSVC  0
#define COMPILER_NAME  "Clang"
#elif defined(__ICCARM__)
#define COMPILER_GCC   0
#define COMPILER_CLANG 0
#define COMPILER_IAR   1
#define COMPILER_MSVC  0
#define COMPILER_NAME  "IAR"
#elif defined(_MSC_VER)
/* Microsoft Visual C++ compiler */
#define COMPILER_GCC   0
#define COMPILER_CLANG 0
#define COMPILER_IAR   0
#define COMPILER_MSVC  1
#define COMPILER_NAME  "MSVC"
#else
/* Unknown compiler - provide defaults that may work */
#define COMPILER_GCC   0
#define COMPILER_CLANG 0
#define COMPILER_IAR   0
#define COMPILER_MSVC  0
#define COMPILER_NAME  "Unknown"
/* Note: Unknown compiler - using default implementations */
#endif

/**
 * \brief           Compiler version detection
 */
#if COMPILER_GCC
#define COMPILER_VERSION                                                       \
    (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#elif COMPILER_CLANG
#define COMPILER_VERSION                                                       \
    (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
#elif COMPILER_IAR
#define COMPILER_VERSION __VER__
#else
#define COMPILER_VERSION 0
#endif

/*===========================================================================*/
/* Function Attributes                                                       */
/*===========================================================================*/

/**
 * \brief           Force inline function attribute
 */
#if COMPILER_GCC || COMPILER_CLANG
#define HAL_INLINE static inline __attribute__((always_inline))
#elif COMPILER_IAR
#define HAL_INLINE static inline
#else
#define HAL_INLINE static inline
#endif

/**
 * \brief           Weak symbol attribute
 */
#if COMPILER_GCC || COMPILER_CLANG
#define HAL_WEAK __attribute__((weak))
#elif COMPILER_IAR
#define HAL_WEAK __weak
#else
#define HAL_WEAK
#endif

/**
 * \brief           Memory alignment attribute
 * \param[in]       n: Alignment in bytes (must be power of 2)
 */
#if COMPILER_GCC || COMPILER_CLANG
#define HAL_ALIGN(n) __attribute__((aligned(n)))
#elif COMPILER_IAR
/* IAR uses pragma for alignment */
#define HAL_ALIGN(n) /* Use #pragma data_alignment=n before variable */
#else
#define HAL_ALIGN(n)
#endif

/**
 * \brief           Section placement attribute
 * \param[in]       name: Section name string
 */
#if COMPILER_GCC || COMPILER_CLANG
#define HAL_SECTION(name) __attribute__((section(name)))
#elif COMPILER_IAR
#define HAL_SECTION(name) @name
#else
#define HAL_SECTION(name)
#endif

/**
 * \brief           No return function attribute
 */
#if COMPILER_GCC || COMPILER_CLANG
#define HAL_NORETURN __attribute__((noreturn))
#elif COMPILER_IAR
#define HAL_NORETURN __noreturn
#else
#define HAL_NORETURN
#endif

/**
 * \brief           Used attribute (prevent optimization removal)
 */
#if COMPILER_GCC || COMPILER_CLANG
#define HAL_USED __attribute__((used))
#elif COMPILER_IAR
#define HAL_USED __root
#else
#define HAL_USED
#endif

/**
 * \brief           Packed structure attribute
 */
#if COMPILER_GCC || COMPILER_CLANG
#define HAL_PACKED __attribute__((packed))
#elif COMPILER_IAR
#define HAL_PACKED __packed
#else
#define HAL_PACKED
#endif

/*===========================================================================*/
/* Memory Barriers                                                           */
/*===========================================================================*/

/**
 * \brief           Data Synchronization Barrier
 * \note            Ensures all explicit memory accesses complete before
 *                  continuing. CM0/CM0+ use NOP sequence as fallback.
 */
#if COMPILER_GCC || COMPILER_CLANG
#if (CORE_TYPE >= CORE_CM3)
#define HAL_DSB() __asm volatile("dsb 0xF" ::: "memory")
#else
/* CM0/CM0+ don't have DSB instruction, use NOP sequence */
#define HAL_DSB() __asm volatile("nop\nnop\nnop\nnop" ::: "memory")
#endif
#elif COMPILER_IAR
#include <intrinsics.h>
#define HAL_DSB() __DSB()
#else
#define HAL_DSB()                                                              \
    do {                                                                       \
    } while (0)
#endif

/**
 * \brief           Instruction Synchronization Barrier
 * \note            Flushes the pipeline and ensures all instructions
 *                  are fetched from cache or memory.
 */
#if COMPILER_GCC || COMPILER_CLANG
#if (CORE_TYPE >= CORE_CM3)
#define HAL_ISB() __asm volatile("isb 0xF" ::: "memory")
#else
/* CM0/CM0+ don't have ISB instruction, use NOP sequence */
#define HAL_ISB() __asm volatile("nop\nnop\nnop\nnop" ::: "memory")
#endif
#elif COMPILER_IAR
#define HAL_ISB() __ISB()
#else
#define HAL_ISB()                                                              \
    do {                                                                       \
    } while (0)
#endif

/**
 * \brief           Data Memory Barrier
 * \note            Ensures all explicit memory accesses that appear
 *                  before the DMB complete before any explicit memory
 *                  accesses that appear after the DMB.
 */
#if COMPILER_GCC || COMPILER_CLANG
#if (CORE_TYPE >= CORE_CM3)
#define HAL_DMB() __asm volatile("dmb 0xF" ::: "memory")
#else
/* CM0/CM0+ don't have DMB instruction, use NOP sequence */
#define HAL_DMB() __asm volatile("nop\nnop\nnop\nnop" ::: "memory")
#endif
#elif COMPILER_IAR
#define HAL_DMB() __DMB()
#else
#define HAL_DMB()                                                              \
    do {                                                                       \
    } while (0)
#endif

/**
 * \brief           Compiler memory barrier (no hardware barrier)
 * \note            Prevents compiler from reordering memory accesses
 */
#if COMPILER_GCC || COMPILER_CLANG
#define HAL_COMPILER_BARRIER() __asm volatile("" ::: "memory")
#elif COMPILER_IAR
#define HAL_COMPILER_BARRIER() __memory_barrier()
#else
#define HAL_COMPILER_BARRIER()                                                 \
    do {                                                                       \
    } while (0)
#endif

/*===========================================================================*/
/* Interrupt Control                                                         */
/*===========================================================================*/

/**
 * \brief           Disable all interrupts (set PRIMASK)
 */
#if COMPILER_GCC || COMPILER_CLANG
#define HAL_DISABLE_IRQ() __asm volatile("cpsid i" ::: "memory")
#elif COMPILER_IAR
#define HAL_DISABLE_IRQ() __disable_irq()
#else
#define HAL_DISABLE_IRQ()                                                      \
    do {                                                                       \
    } while (0)
#endif

/**
 * \brief           Enable all interrupts (clear PRIMASK)
 */
#if COMPILER_GCC || COMPILER_CLANG
#define HAL_ENABLE_IRQ() __asm volatile("cpsie i" ::: "memory")
#elif COMPILER_IAR
#define HAL_ENABLE_IRQ() __enable_irq()
#else
#define HAL_ENABLE_IRQ()                                                       \
    do {                                                                       \
    } while (0)
#endif

/**
 * \brief           No Operation instruction
 */
#if COMPILER_GCC || COMPILER_CLANG
#define HAL_NOP() __asm volatile("nop")
#elif COMPILER_IAR
#define HAL_NOP() __no_operation()
#else
#define HAL_NOP()                                                              \
    do {                                                                       \
    } while (0)
#endif

/**
 * \brief           Wait For Interrupt (low power wait)
 */
#if COMPILER_GCC || COMPILER_CLANG
#define HAL_WFI() __asm volatile("wfi")
#elif COMPILER_IAR
#define HAL_WFI() __WFI()
#else
#define HAL_WFI()                                                              \
    do {                                                                       \
    } while (0)
#endif

/**
 * \brief           Wait For Event
 */
#if COMPILER_GCC || COMPILER_CLANG
#define HAL_WFE() __asm volatile("wfe")
#elif COMPILER_IAR
#define HAL_WFE() __WFE()
#else
#define HAL_WFE()                                                              \
    do {                                                                       \
    } while (0)
#endif

/**
 * \brief           Send Event
 */
#if COMPILER_GCC || COMPILER_CLANG
#define HAL_SEV() __asm volatile("sev")
#elif COMPILER_IAR
#define HAL_SEV() __SEV()
#else
#define HAL_SEV()                                                              \
    do {                                                                       \
    } while (0)
#endif

/*===========================================================================*/
/* PRIMASK Access Functions                                                  */
/*===========================================================================*/

/**
 * \brief           Get current PRIMASK value
 * \return          Current PRIMASK register value
 */
#if COMPILER_GCC || COMPILER_CLANG
HAL_INLINE uint32_t hal_get_primask(void) {
    uint32_t result;
    __asm volatile("mrs %0, primask" : "=r"(result));
    return result;
}
#elif COMPILER_IAR
#define hal_get_primask() __get_PRIMASK()
#else
HAL_INLINE uint32_t hal_get_primask(void) {
    return 0;
}
#endif

/**
 * \brief           Set PRIMASK value
 * \param[in]       primask: Value to set in PRIMASK register
 */
#if COMPILER_GCC || COMPILER_CLANG
HAL_INLINE void hal_set_primask(uint32_t primask) {
    __asm volatile("msr primask, %0" ::"r"(primask) : "memory");
}
#elif COMPILER_IAR
#define hal_set_primask(x) __set_PRIMASK(x)
#else
HAL_INLINE void hal_set_primask(uint32_t primask) {
    (void)primask;
}
#endif

/**
 * \brief           Get current BASEPRI value (CM3+ only)
 * \return          Current BASEPRI register value
 */
#if CORE_TYPE >= CORE_CM3
#if COMPILER_GCC || COMPILER_CLANG
HAL_INLINE uint32_t hal_get_basepri(void) {
    uint32_t result;
    __asm volatile("mrs %0, basepri" : "=r"(result));
    return result;
}
#elif COMPILER_IAR
#define hal_get_basepri() __get_BASEPRI()
#else
HAL_INLINE uint32_t hal_get_basepri(void) {
    return 0;
}
#endif
#endif

/**
 * \brief           Set BASEPRI value (CM3+ only)
 * \param[in]       basepri: Value to set in BASEPRI register
 */
#if CORE_TYPE >= CORE_CM3
#if COMPILER_GCC || COMPILER_CLANG
HAL_INLINE void hal_set_basepri(uint32_t basepri) {
    __asm volatile("msr basepri, %0" ::"r"(basepri) : "memory");
}
#elif COMPILER_IAR
#define hal_set_basepri(x) __set_BASEPRI(x)
#else
HAL_INLINE void hal_set_basepri(uint32_t basepri) {
    (void)basepri;
}
#endif
#endif

/**
 * \brief           Get current FAULTMASK value (CM3+ only)
 * \return          Current FAULTMASK register value
 */
#if CORE_TYPE >= CORE_CM3
#if COMPILER_GCC || COMPILER_CLANG
HAL_INLINE uint32_t hal_get_faultmask(void) {
    uint32_t result;
    __asm volatile("mrs %0, faultmask" : "=r"(result));
    return result;
}
#elif COMPILER_IAR
#define hal_get_faultmask() __get_FAULTMASK()
#else
HAL_INLINE uint32_t hal_get_faultmask(void) {
    return 0;
}
#endif
#endif

/**
 * \brief           Set FAULTMASK value (CM3+ only)
 * \param[in]       faultmask: Value to set in FAULTMASK register
 */
#if CORE_TYPE >= CORE_CM3
#if COMPILER_GCC || COMPILER_CLANG
HAL_INLINE void hal_set_faultmask(uint32_t faultmask) {
    __asm volatile("msr faultmask, %0" ::"r"(faultmask) : "memory");
}
#elif COMPILER_IAR
#define hal_set_faultmask(x) __set_FAULTMASK(x)
#else
HAL_INLINE void hal_set_faultmask(uint32_t faultmask) {
    (void)faultmask;
}
#endif
#endif

/*===========================================================================*/
/* Critical Section Helpers                                                  */
/*===========================================================================*/

/**
 * \brief           Enter critical section (save and disable interrupts)
 * \return          Previous interrupt state (PRIMASK value)
 */
HAL_INLINE uint32_t hal_enter_critical(void) {
    uint32_t primask = hal_get_primask();
    HAL_DISABLE_IRQ();
    return primask;
}

/**
 * \brief           Exit critical section (restore interrupt state)
 * \param[in]       state: Previous interrupt state from hal_enter_critical()
 */
HAL_INLINE void hal_exit_critical(uint32_t state) {
    hal_set_primask(state);
}

/*===========================================================================*/
/* Bit Manipulation Intrinsics                                               */
/*===========================================================================*/

/**
 * \brief           Count leading zeros
 * \param[in]       value: Input value
 * \return          Number of leading zero bits
 */
#if COMPILER_GCC || COMPILER_CLANG
HAL_INLINE uint32_t hal_clz(uint32_t value) {
    if (value == 0)
        return 32;
    return (uint32_t)__builtin_clz(value);
}
#elif COMPILER_IAR
#define hal_clz(x) __CLZ(x)
#else
HAL_INLINE uint32_t hal_clz(uint32_t value) {
    uint32_t count = 0;
    if (value == 0)
        return 32;
    while ((value & 0x80000000UL) == 0) {
        count++;
        value <<= 1;
    }
    return count;
}
#endif

/**
 * \brief           Reverse byte order (32-bit)
 * \param[in]       value: Input value
 * \return          Byte-reversed value
 */
#if COMPILER_GCC || COMPILER_CLANG
HAL_INLINE uint32_t hal_rev(uint32_t value) {
    return __builtin_bswap32(value);
}
#elif COMPILER_IAR
#define hal_rev(x) __REV(x)
#else
HAL_INLINE uint32_t hal_rev(uint32_t value) {
    return ((value & 0xFF000000UL) >> 24) | ((value & 0x00FF0000UL) >> 8) |
           ((value & 0x0000FF00UL) << 8) | ((value & 0x000000FFUL) << 24);
}
#endif

/**
 * \brief           Reverse byte order (16-bit)
 * \param[in]       value: Input value
 * \return          Byte-reversed value
 */
#if COMPILER_GCC || COMPILER_CLANG
HAL_INLINE uint16_t hal_rev16(uint16_t value) {
    return __builtin_bswap16(value);
}
#elif COMPILER_IAR
#define hal_rev16(x) __REV16(x)
#else
HAL_INLINE uint16_t hal_rev16(uint16_t value) {
    return (uint16_t)(((value & 0xFF00U) >> 8) | ((value & 0x00FFU) << 8));
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* COMPILER_ABSTRACTION_H */
