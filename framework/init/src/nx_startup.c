/**
 * \file            nx_startup.c
 * \brief           Nexus startup framework implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-16
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         This file implements the startup framework that provides
 *                  a unified entry point for system initialization.
 *
 *                  The startup sequence is:
 *                  1. nx_board_init()  - Board-level initialization
 *                  2. nx_os_init()     - OS initialization
 *                  3. nx_init_run()    - Auto initialization functions
 *                  4. main()           - User application entry
 */

#include "nx_startup.h"
#include "nx_init.h"

/*---------------------------------------------------------------------------*/
/* Private Variables                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Current startup state
 */
static nx_startup_state_t g_startup_state = NX_STARTUP_STATE_NOT_STARTED;

/**
 * \brief           Startup configuration
 */
static nx_startup_config_t g_startup_config = {
    .main_stack_size = NX_STARTUP_MAIN_STACK_SIZE,
    .main_priority = NX_STARTUP_MAIN_PRIORITY,
    .use_rtos = false};

/*---------------------------------------------------------------------------*/
/* Weak Symbol Default Implementations                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Default board initialization (weak)
 *
 * \details         This is the default implementation that does nothing.
 *                  Users should override this function to perform
 *                  board-specific initialization.
 */
NX_WEAK void nx_board_init(void) {
    /* Default implementation - do nothing */
}

/**
 * \brief           Default OS initialization (weak)
 *
 * \details         This is the default implementation that does nothing.
 *                  Users should override this function to perform
 *                  OS-specific initialization.
 */
NX_WEAK void nx_os_init(void) {
    /* Default implementation - do nothing */
}

/*---------------------------------------------------------------------------*/
/* RTOS Support                                                              */
/*---------------------------------------------------------------------------*/

#ifdef NX_USE_RTOS
#include "osal/osal_task.h"

/**
 * \brief           Main thread handle
 */
static osal_task_handle_t g_main_thread_handle = NULL;

/**
 * \brief           Main thread function
 */
static void nx_main_thread(void* arg) {
    (void)arg; /* Unused */

    /* Update state */
    g_startup_state = NX_STARTUP_STATE_MAIN_RUNNING;

    /* Call user main */
    NX_CALL_MAIN();

    /* Mark startup complete */
    g_startup_state = NX_STARTUP_STATE_COMPLETE;

    /* Delete self if main returns */
    osal_task_delete(NULL);
}

/**
 * \brief           Create main thread for RTOS mode
 */
static void nx_create_main_thread(void) {
    osal_task_config_t config = {.name = "main",
                                 .func = nx_main_thread,
                                 .arg = NULL,
                                 .priority = g_startup_config.main_priority,
                                 .stack_size =
                                     g_startup_config.main_stack_size};

    osal_task_create(&config, &g_main_thread_handle);
}

#endif /* NX_USE_RTOS */

/*---------------------------------------------------------------------------*/
/* Private Functions                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Execute startup sequence
 */
static void nx_startup_execute(void) {
    /* Step 1: Board initialization */
    g_startup_state = NX_STARTUP_STATE_BOARD_INIT;
    nx_board_init();

    /* Step 2: OS initialization */
    g_startup_state = NX_STARTUP_STATE_OS_INIT;
    nx_os_init();

    /* Step 3: Auto initialization */
    g_startup_state = NX_STARTUP_STATE_AUTO_INIT;
    nx_init_run();

#ifdef NX_USE_RTOS
    if (g_startup_config.use_rtos) {
        /* RTOS mode: Create main thread and start scheduler */
        nx_create_main_thread();

        /* Note: Scheduler start is typically done in nx_os_init() */
        /* The scheduler should be started after this function returns */
        return;
    }
#endif /* NX_USE_RTOS */

    /* Bare-metal mode: Call main directly */
    g_startup_state = NX_STARTUP_STATE_MAIN_RUNNING;
    NX_CALL_MAIN();

    /* Mark startup complete */
    g_startup_state = NX_STARTUP_STATE_COMPLETE;
}

/*---------------------------------------------------------------------------*/
/* Public API Implementation                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Execute system startup sequence
 */
void nx_startup(void) {
    nx_startup_execute();
}

/**
 * \brief           Execute startup with custom configuration
 */
void nx_startup_with_config(const nx_startup_config_t* config) {
    if (config != NULL) {
        g_startup_config = *config;
    }
    nx_startup_execute();
}

/**
 * \brief           Get current startup state
 */
nx_startup_state_t nx_startup_get_state(void) {
    return g_startup_state;
}

/**
 * \brief           Check if startup is complete
 */
bool nx_startup_is_complete(void) {
    return (g_startup_state == NX_STARTUP_STATE_COMPLETE) ||
           (g_startup_state == NX_STARTUP_STATE_MAIN_RUNNING);
}

/**
 * \brief           Get default startup configuration
 */
void nx_startup_get_default_config(nx_startup_config_t* config) {
    if (config != NULL) {
        config->main_stack_size = NX_STARTUP_MAIN_STACK_SIZE;
        config->main_priority = NX_STARTUP_MAIN_PRIORITY;
        config->use_rtos = false;
    }
}

/*---------------------------------------------------------------------------*/
/* Compiler-Specific Entry Points                                            */
/*---------------------------------------------------------------------------*/

#if defined(__ARMCC_VERSION)
/*---------------------------------------------------------------------------*/
/* Arm Compiler Entry Point                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Arm Compiler $Sub$main entry point
 *
 * \details         This function intercepts the call to main() and
 *                  redirects to nx_startup() for initialization.
 */
int $Sub$main(void) {
    nx_startup();
    return 0;
}

#elif defined(__GNUC__) && !defined(_MSC_VER) && defined(NX_USE_ENTRY_POINT)
/*---------------------------------------------------------------------------*/
/* GCC Entry Point                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           GCC entry() function
 *
 * \details         Use -eentry linker option to specify this as entry point.
 *                  This function initializes the C runtime and calls
 * nx_startup().
 */
int entry(void) {
    /* Initialize C runtime (constructors, etc.) */
    extern void __libc_init_array(void);
    __libc_init_array();

    /* Execute startup sequence */
    nx_startup();

    return 0;
}

#elif defined(__ICCARM__) && defined(NX_USE_ENTRY_POINT)
/*---------------------------------------------------------------------------*/
/* IAR Entry Point                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           IAR __low_level_init hook
 *
 * \details         This function is called before main() by the IAR runtime.
 *                  Return 1 to initialize data segments, 0 to skip.
 */
int __low_level_init(void) {
    /* Board init can be done here before data initialization */
    /* Return 1 to initialize data segments */
    return 1;
}

#endif /* Compiler-specific entry points */

/*---------------------------------------------------------------------------*/
/* Test Support                                                              */
/*---------------------------------------------------------------------------*/

#ifdef NX_STARTUP_TEST_MODE

/**
 * \brief           Reset startup state for testing
 *
 * \note            Only available when NX_STARTUP_TEST_MODE is defined.
 */
void nx_startup_reset_for_test(void) {
    g_startup_state = NX_STARTUP_STATE_NOT_STARTED;
    g_startup_config.main_stack_size = NX_STARTUP_MAIN_STACK_SIZE;
    g_startup_config.main_priority = NX_STARTUP_MAIN_PRIORITY;
    g_startup_config.use_rtos = false;
}

/**
 * \brief           Set startup state for testing
 *
 * \note            Only available when NX_STARTUP_TEST_MODE is defined.
 */
void nx_startup_set_state_for_test(nx_startup_state_t state) {
    g_startup_state = state;
}

#endif /* NX_STARTUP_TEST_MODE */
