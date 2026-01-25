/**
 * \file            nx_types.h
 * \brief           Nexus platform basic type definitions
 * \author          Nexus Team
 *
 * This file provides portable type definitions for the Nexus HAL.
 * It uses standard C headers when available, or provides fallback definitions.
 */

#ifndef NX_TYPES_H
#define NX_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Include standard headers if available                                     */
/*---------------------------------------------------------------------------*/
#if defined(__cplusplus)
/* C++ mode - use standard headers */
#include <cstddef>
#include <cstdint>
/* C++ has built-in bool type, no need for stdbool.h */
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
/* C99 or later - use standard headers */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#else
/* Pre-C99 fallback definitions */

/* Fixed-width integer types */
#ifndef _STDINT_H
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
#if defined(__GNUC__) || defined(_MSC_VER)
typedef signed long long int64_t;
typedef unsigned long long uint64_t;
#endif
#endif /* _STDINT_H */

/* Size types */
#ifndef _STDDEF_H
#ifndef _SIZE_T_DEFINED
typedef unsigned int size_t;
#define _SIZE_T_DEFINED
#endif
#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void*)0)
#endif
#endif
#endif /* _STDDEF_H */

/* Boolean type */
#ifndef _STDBOOL_H
#ifndef __cplusplus
typedef unsigned char bool;
#define true  1
#define false 0
#endif
#define __bool_true_false_are_defined 1
#endif /* _STDBOOL_H */

#endif /* __STDC_VERSION__ */

/*---------------------------------------------------------------------------*/
/* Additional type definitions for embedded systems                          */
/*---------------------------------------------------------------------------*/

/** Pointer-sized unsigned integer */
#if defined(__cplusplus) ||                                                    \
    (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
/* C99/C++ - uintptr_t is available from stdint.h */
typedef uintptr_t nx_uintptr_t;
#else
/* Pre-C99 fallback */
typedef unsigned long nx_uintptr_t;
#endif

/** Register-sized type for hardware access */
typedef volatile uint32_t nx_reg32_t;
typedef volatile uint16_t nx_reg16_t;
typedef volatile uint8_t nx_reg8_t;

/*---------------------------------------------------------------------------*/
/* Compiler-specific attributes                                              */
/*---------------------------------------------------------------------------*/

/* Arm Compiler 4/5 (armcc) */
#if defined(__CC_ARM)
#define NX_ASM        __asm
#define NX_INLINE     static __inline
#define NX_NORETURN   __declspec(noreturn)
#define NX_PACKED     __packed
#define NX_ALIGNED(x) __align(x)
#define NX_UNUSED     __attribute__((unused))
#define NX_WEAK       __weak
#define NX_SECTION(x) __attribute__((section(x)))
#define NX_USED       __attribute__((used))

/* Arm Compiler 6 (armclang) */
#elif defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
#define NX_ASM        __asm
#define NX_INLINE     static inline
#define NX_NORETURN   __attribute__((noreturn))
#define NX_PACKED     __attribute__((packed))
#define NX_ALIGNED(x) __attribute__((aligned(x)))
#define NX_UNUSED     __attribute__((unused))
#define NX_WEAK       __attribute__((weak))
#define NX_SECTION(x) __attribute__((section(x)))
#define NX_USED       __attribute__((used))

/* GCC / Clang */
#elif defined(__GNUC__)
#define NX_ASM        __asm__
#define NX_INLINE     static inline
#define NX_NORETURN   __attribute__((noreturn))
#define NX_PACKED     __attribute__((packed))
#define NX_ALIGNED(x) __attribute__((aligned(x)))
#define NX_UNUSED     __attribute__((unused))
#define NX_WEAK       __attribute__((weak))
#define NX_SECTION(x) __attribute__((section(x)))
#define NX_USED       __attribute__((used))
#define NX_DEPRECATED __attribute__((deprecated))

/* IAR Compiler */
#elif defined(__ICCARM__)
#define NX_ASM        __asm
#define NX_INLINE     static inline
#define NX_NORETURN   __noreturn
#define NX_PACKED     __packed
#define NX_ALIGNED(x) _Pragma("data_alignment=" #x)
#define NX_UNUSED
#define NX_WEAK       __weak
#define NX_SECTION(x) _Pragma("location=" #x)
#define NX_USED       __root

/* TI Arm Compiler */
#elif defined(__TI_ARM__)
#define NX_ASM        __asm
#define NX_INLINE     static inline
#define NX_NORETURN   __attribute__((noreturn))
#define NX_PACKED     __attribute__((packed))
#define NX_ALIGNED(x) __attribute__((aligned(x)))
#define NX_UNUSED     __attribute__((unused))
#define NX_WEAK       __attribute__((weak))
#define NX_SECTION(x) __attribute__((section(x)))
#define NX_USED       __attribute__((used))

/* TASKING Compiler */
#elif defined(__TASKING__)
#define NX_ASM        __asm
#define NX_INLINE     static inline
#define NX_NORETURN   __attribute__((noreturn))
#define NX_PACKED     __packed__
#define NX_ALIGNED(x) __align(x)
#define NX_UNUSED     __attribute__((unused))
#define NX_WEAK       __attribute__((weak))
#define NX_SECTION(x) __attribute__((section(x)))
#define NX_USED       __attribute__((used))

/* Microsoft Visual C++ */
#elif defined(_MSC_VER)
#define NX_ASM      __asm
#define NX_INLINE   static __inline
#define NX_NORETURN __declspec(noreturn)
#define NX_PACKED
#define NX_ALIGNED(x) __declspec(align(x))
#define NX_UNUSED
#define NX_WEAK
#define NX_SECTION(x) __declspec(allocate(x))
#define NX_USED
#define NX_DEPRECATED __declspec(deprecated)

/* Unknown compiler - fallback */
#else
#define NX_ASM    __asm__
#define NX_INLINE static inline
#define NX_NORETURN
#define NX_PACKED
#define NX_ALIGNED(x)
#define NX_UNUSED
#define NX_WEAK
#define NX_SECTION(x)
#define NX_USED
#define NX_DEPRECATED
#endif

/*---------------------------------------------------------------------------*/
/* Utility macros                                                            */
/*---------------------------------------------------------------------------*/

/** Get array element count */
#define NX_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/** Get minimum of two values */
#define NX_MIN(a, b) (((a) < (b)) ? (a) : (b))

/** Get maximum of two values */
#define NX_MAX(a, b) (((a) > (b)) ? (a) : (b))

/** Clamp value to range */
#define NX_CLAMP(val, min, max) (NX_MIN(NX_MAX((val), (min)), (max)))

/** Check if value is power of 2 */
#define NX_IS_POWER_OF_2(x) (((x) != 0) && (((x) & ((x) - 1)) == 0))

/** Align value up to alignment boundary */
#define NX_ALIGN_UP(x, align) (((x) + ((align) - 1)) & ~((align) - 1))

/** Align value down to alignment boundary */
#define NX_ALIGN_DOWN(x, align) ((x) & ~((align) - 1))

/** Get offset of member in structure */
#ifndef offsetof
#define offsetof(type, member) ((size_t) & ((type*)0)->member)
#endif

/** Get container structure from member pointer */
#define NX_CONTAINER_OF(ptr, type, member)                                     \
    ((type*)((char*)(ptr) - offsetof(type, member)))

/** Bit manipulation macros */
#define NX_BIT(n)           (1UL << (n))
#define NX_BIT_SET(x, n)    ((x) |= NX_BIT(n))
#define NX_BIT_CLEAR(x, n)  ((x) &= ~NX_BIT(n))
#define NX_BIT_TOGGLE(x, n) ((x) ^= NX_BIT(n))
#define NX_BIT_CHECK(x, n)  (((x) & NX_BIT(n)) != 0)

/*---------------------------------------------------------------------------*/
/* Assertion macros                                                          */
/*---------------------------------------------------------------------------*/

#include "nexus_config.h"

#if NX_CONFIG_HAL_ASSERT_ENABLE

/**
 * \brief           Assert handler function type
 */
typedef void (*nx_assert_handler_t)(const char* file, int line,
                                    const char* expr);

/**
 * \brief           Set custom assert handler
 * \param[in]       handler: Assert handler function
 */
void nx_set_assert_handler(nx_assert_handler_t handler);

/**
 * \brief           Default assert handler (weak, can be overridden)
 * \param[in]       file: Source file name
 * \param[in]       line: Line number
 * \param[in]       expr: Failed expression string
 */
void nx_assert_failed(const char* file, int line, const char* expr);

/**
 * \brief           Assert macro with expression
 * \param[in]       expr: Expression to evaluate
 */
#define NX_ASSERT(expr)                                                        \
    do {                                                                       \
        if (!(expr)) {                                                         \
            nx_assert_failed(__FILE__, __LINE__, #expr);                       \
        }                                                                      \
    } while (0)

/**
 * \brief           Assert macro with message
 * \param[in]       expr: Expression to evaluate
 * \param[in]       msg: Message to display on failure
 */
#define NX_ASSERT_MSG(expr, msg)                                               \
    do {                                                                       \
        if (!(expr)) {                                                         \
            nx_assert_failed(__FILE__, __LINE__, msg);                         \
        }                                                                      \
    } while (0)

#else /* NX_CONFIG_HAL_ASSERT_ENABLE */

#define NX_ASSERT(expr)          ((void)0)
#define NX_ASSERT_MSG(expr, msg) ((void)0)

#endif /* NX_CONFIG_HAL_ASSERT_ENABLE */

/**
 * \brief           Static assert (compile-time)
 * \param[in]       expr: Expression to evaluate
 * \param[in]       msg: Message identifier (no spaces)
 */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define NX_STATIC_ASSERT(expr, msg) _Static_assert(expr, #msg)
#elif defined(__cplusplus) && __cplusplus >= 201103L
#define NX_STATIC_ASSERT(expr, msg) static_assert(expr, #msg)
#else
#define NX_STATIC_ASSERT(expr, msg)                                            \
    typedef char NX_STATIC_ASSERT_##msg[(expr) ? 1 : -1] NX_UNUSED
#endif

#ifdef __cplusplus
}
#endif

#endif /* NX_TYPES_H */
