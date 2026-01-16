/**
 * \file            nx_init.h
 * \brief           Nexus automatic initialization mechanism
 * \author          Nexus Team
 *
 * This file provides a compile-time automatic initialization system
 * similar to Linux initcall. Initialization functions are registered
 * at compile time using linker sections and executed in level order
 * at system startup.
 */

#ifndef NX_INIT_H
#define NX_INIT_H

#include "hal/nx_status.h"
#include "hal/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Type Definitions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialization function type
 * \return          0 on success, non-zero error code on failure
 */
typedef int (*nx_init_fn_t)(void);

/**
 * \brief           Initialization level enumeration
 *
 * Defines the order in which initialization functions are executed.
 * Lower levels execute before higher levels.
 * Level 0 and 7 are reserved for internal boundary markers.
 */
typedef enum {
    NX_INIT_LEVEL_BOARD = 1,     /**< Board-level init (clock, power) */
    NX_INIT_LEVEL_PREV = 2,      /**< Pre-initialization (memory, debug) */
    NX_INIT_LEVEL_BSP = 3,       /**< BSP initialization (peripheral config) */
    NX_INIT_LEVEL_DRIVER = 4,    /**< Driver initialization */
    NX_INIT_LEVEL_COMPONENT = 5, /**< Component init (middleware) */
    NX_INIT_LEVEL_APP = 6,       /**< Application initialization */
    NX_INIT_LEVEL_MAX = 7        /**< Maximum level (boundary marker) */
} nx_init_level_t;

/*---------------------------------------------------------------------------*/
/* Boundary Marker Level Definitions                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Boundary marker level for start of init table
 *
 * This level is used internally to mark the beginning of the
 * initialization function table. User code should not use this level.
 */
#define NX_INIT_LEVEL_BOUNDARY_START "0"

/**
 * \brief           Boundary marker level for end of init table
 *
 * This level is used internally to mark the end of the
 * initialization function table. User code should not use this level.
 */
#define NX_INIT_LEVEL_BOUNDARY_END "7"

/**
 * \brief           Initialization statistics structure
 */
typedef struct nx_init_stats_s {
    uint16_t total_count;   /**< Total number of init functions */
    uint16_t success_count; /**< Number of successful initializations */
    uint16_t fail_count;    /**< Number of failed initializations */
    int last_error;         /**< Last error code returned */
#ifdef NX_INIT_DEBUG
    const char* last_failed; /**< Name of last failed function (debug mode) */
#endif
} nx_init_stats_t;

/*---------------------------------------------------------------------------*/
/* Linker Section Boundary Symbols                                           */
/*---------------------------------------------------------------------------*/

/* MSVC - For testing purposes, use array-based approach */
#if defined(_MSC_VER)
/* MSVC doesn't support linker sections in the same way.
 * For testing, we'll use a simpler array-based approach.
 */
extern const nx_init_fn_t __nx_init_fn_array[];
extern const size_t __nx_init_fn_count;

#define NX_INIT_FN_START __nx_init_fn_array
#define NX_INIT_FN_END   (__nx_init_fn_array + __nx_init_fn_count)

/* GCC / Clang */
#elif defined(__GNUC__) && !defined(__ARMCC_VERSION)
extern const nx_init_fn_t __nx_init_fn_start[];
extern const nx_init_fn_t __nx_init_fn_end[];

#define NX_INIT_FN_START __nx_init_fn_start
#define NX_INIT_FN_END   __nx_init_fn_end

/* Arm Compiler 5/6 */
#elif defined(__ARMCC_VERSION)
extern const nx_init_fn_t Image$$nx_init_fn$$Base[];
extern const nx_init_fn_t Image$$nx_init_fn$$Limit[];

#define NX_INIT_FN_START Image$$nx_init_fn$$Base
#define NX_INIT_FN_END   Image$$nx_init_fn$$Limit

/* IAR */
#elif defined(__ICCARM__)
#pragma section = "nx_init_fn"

#define NX_INIT_FN_START ((const nx_init_fn_t*)__section_begin("nx_init_fn"))
#define NX_INIT_FN_END   ((const nx_init_fn_t*)__section_end("nx_init_fn"))

#else
#error "Unsupported compiler for static initialization"
#endif

/*---------------------------------------------------------------------------*/
/* Initialization Export Macros                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Internal macro to export initialization function
 * \param[in]       fn: Function pointer
 * \param[in]       level: Initialization level (0-5)
 *
 * This macro places the function pointer in a linker section named
 * ".nx_init_fn.X" where X is the level. The linker sorts sections
 * by name, ensuring correct execution order.
 */
#if defined(_MSC_VER)
/* MSVC: For testing, just define empty macros */
#define _NX_INIT_EXPORT(fn, level) /* No-op for MSVC testing */
#else
#define _NX_INIT_EXPORT(fn, level)                                             \
    NX_USED const nx_init_fn_t _nx_init_##fn NX_SECTION(                       \
        ".nx_init_fn." #level) = (fn)
#endif

/**
 * \brief           Export board-level initialization function
 * \param[in]       fn: Function pointer
 *
 * Board-level initialization runs first (level 1).
 * Use for clock configuration, power management, etc.
 */
#define NX_INIT_BOARD_EXPORT(fn) _NX_INIT_EXPORT(fn, 1)

/**
 * \brief           Export pre-initialization function
 * \param[in]       fn: Function pointer
 *
 * Pre-initialization runs at level 2.
 * Use for memory initialization, debug setup, etc.
 */
#define NX_INIT_PREV_EXPORT(fn) _NX_INIT_EXPORT(fn, 2)

/**
 * \brief           Export BSP initialization function
 * \param[in]       fn: Function pointer
 *
 * BSP initialization runs at level 3.
 * Use for peripheral configuration, pin muxing, etc.
 */
#define NX_INIT_BSP_EXPORT(fn) _NX_INIT_EXPORT(fn, 3)

/**
 * \brief           Export driver initialization function
 * \param[in]       fn: Function pointer
 *
 * Driver initialization runs at level 4.
 * Use for device driver initialization.
 */
#define NX_INIT_DRIVER_EXPORT(fn) _NX_INIT_EXPORT(fn, 4)

/**
 * \brief           Export component initialization function
 * \param[in]       fn: Function pointer
 *
 * Component initialization runs at level 5.
 * Use for middleware and component initialization.
 */
#define NX_INIT_COMPONENT_EXPORT(fn) _NX_INIT_EXPORT(fn, 5)

/**
 * \brief           Export application initialization function
 * \param[in]       fn: Function pointer
 *
 * Application initialization runs last (level 6).
 * Use for application-specific initialization.
 */
#define NX_INIT_APP_EXPORT(fn) _NX_INIT_EXPORT(fn, 6)

/*---------------------------------------------------------------------------*/
/* Public API                                                                */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Execute all registered initialization functions
 * \return          NX_OK if all succeeded, NX_ERR_GENERIC if any failed
 *
 * This function iterates through all registered initialization functions
 * in level order and executes them. If a function returns non-zero,
 * the error is recorded but execution continues with remaining functions.
 */
nx_status_t nx_init_run(void);

/**
 * \brief           Get initialization statistics
 * \param[out]      stats: Pointer to statistics structure
 * \return          NX_OK on success, NX_ERR_NULL_PTR if stats is NULL
 *
 * Returns statistics about initialization execution including
 * total count, success count, failure count, and last error.
 */
nx_status_t nx_init_get_stats(nx_init_stats_t* stats);

/**
 * \brief           Check if all initializations completed successfully
 * \return          true if all succeeded, false if any failed
 *
 * This function checks if all initialization functions executed
 * successfully (no failures recorded).
 */
bool nx_init_is_complete(void);

#ifdef __cplusplus
}
#endif

#endif /* NX_INIT_H */
