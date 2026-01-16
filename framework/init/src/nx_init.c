/**
 * \file            nx_init.c
 * \brief           Nexus automatic initialization mechanism implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-16
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         This file implements the automatic initialization system
 *                  that executes registered initialization functions in
 *                  level order at system startup.
 */

#include "nx_init.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Private Variables                                                         */
/*---------------------------------------------------------------------------*/

/** Global initialization statistics */
static nx_init_stats_t g_init_stats = {0};

/** Flag to track if initialization has been run */
static bool g_init_complete = false;

#ifdef _MSC_VER
/* MSVC: Provide empty array for testing */
const nx_init_fn_t __nx_init_fn_array[] = {NULL};
const size_t __nx_init_fn_count = 0;
#endif

/*---------------------------------------------------------------------------*/
/* Boundary Marker Functions                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Start boundary marker function
 *
 * This function is automatically registered at level 0 to mark the
 * beginning of the initialization function table. It is transparent
 * to user code and is skipped during iteration.
 */
static int _nx_init_boundary_start(void) {
    return 0;
}

/**
 * \brief           End boundary marker function
 *
 * This function is automatically registered at level 6 to mark the
 * end of the initialization function table. It is transparent to
 * user code and is skipped during iteration.
 */
static int _nx_init_boundary_end(void) {
    return 0;
}

#ifndef _MSC_VER
/*
 * Register boundary markers at level 0 (start) and level 7 (end).
 * These markers define the valid range for init function iteration.
 * User levels are 1-6, avoiding overlap with boundary markers.
 */
_NX_INIT_EXPORT(_nx_init_boundary_start, 0);
_NX_INIT_EXPORT(_nx_init_boundary_end, 7);
#endif

/*---------------------------------------------------------------------------*/
/* Private Functions                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Execute a single initialization function
 * \param[in]       fn: Function pointer to execute
 * \return          0 on success, error code on failure
 */
static int execute_init_fn(nx_init_fn_t fn) {
    int result;

    if (fn == NULL) {
        return 0; /* Skip NULL functions */
    }

#ifdef NX_INIT_DEBUG
    /* Debug output before execution */
    extern void nx_init_debug_print(const char* msg);
    nx_init_debug_print("Executing init function...");
#endif

    result = fn();

#ifdef NX_INIT_DEBUG
    /* Debug output after execution */
    if (result != 0) {
        nx_init_debug_print("Init function failed");
    }
#endif

    return result;
}

/**
 * \brief           Update statistics after function execution
 * \param[in]       result: Result code from function execution
 */
static void update_stats(int result) {
    g_init_stats.total_count++;

    if (result == 0) {
        g_init_stats.success_count++;
    } else {
        g_init_stats.fail_count++;
        g_init_stats.last_error = result;
    }
}

/*---------------------------------------------------------------------------*/
/* Public Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Execute all registered initialization functions
 */
nx_status_t nx_init_run(void) {
    const nx_init_fn_t* fn_ptr;
    int result;
    bool has_error = false;

    /* If already run, return immediately (idempotent) */
    if (g_init_complete) {
        return NX_OK;
    }

    /* Reset statistics */
    memset(&g_init_stats, 0, sizeof(g_init_stats));

    /* Iterate through all registered init functions */
    for (fn_ptr = NX_INIT_FN_START; fn_ptr < NX_INIT_FN_END; fn_ptr++) {
#ifndef _MSC_VER
        /* Skip boundary markers (not used in MSVC) */
        if (*fn_ptr == _nx_init_boundary_start ||
            *fn_ptr == _nx_init_boundary_end) {
            continue;
        }
#endif

        /* Execute function */
        result = execute_init_fn(*fn_ptr);

        /* Update statistics */
        update_stats(result);

        /* Record error but continue execution */
        if (result != 0) {
            has_error = true;
        }
    }

    /* Mark initialization as complete */
    g_init_complete = true;

    return has_error ? NX_ERR_GENERIC : NX_OK;
}

/**
 * \brief           Get initialization statistics
 */
nx_status_t nx_init_get_stats(nx_init_stats_t* stats) {
    if (stats == NULL) {
        return NX_ERR_NULL_PTR;
    }

    /* Copy statistics to output */
    memcpy(stats, &g_init_stats, sizeof(nx_init_stats_t));

    return NX_OK;
}

/**
 * \brief           Check if all initializations completed successfully
 */
bool nx_init_is_complete(void) {
    return g_init_complete && (g_init_stats.fail_count == 0);
}
