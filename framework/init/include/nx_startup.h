/**
 * \file            nx_startup.h
 * \brief           Nexus startup framework
 * \author          Nexus Team
 *
 * This file provides a unified startup framework that intercepts the
 * program entry point and executes initialization in a defined order:
 * board_init -> os_init -> init_functions -> main
 *
 * The framework supports both bare-metal and RTOS configurations.
 */

#ifndef NX_STARTUP_H
#define NX_STARTUP_H

#include "hal/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Default main thread stack size (bytes)
 */
#ifndef NX_STARTUP_MAIN_STACK_SIZE
#define NX_STARTUP_MAIN_STACK_SIZE 4096
#endif

/**
 * \brief           Default main thread priority
 */
#ifndef NX_STARTUP_MAIN_PRIORITY
#define NX_STARTUP_MAIN_PRIORITY 16
#endif

/*---------------------------------------------------------------------------*/
/* Type Definitions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Startup configuration structure
 *
 * Configuration options for the startup framework.
 * Used to customize main thread parameters in RTOS mode.
 */
typedef struct nx_startup_config_s {
    uint32_t main_stack_size; /**< Main thread stack size (RTOS mode) */
    uint8_t main_priority;    /**< Main thread priority (0-31) */
    bool use_rtos;            /**< Whether to use RTOS mode */
} nx_startup_config_t;

/**
 * \brief           Startup state enumeration
 */
typedef enum {
    NX_STARTUP_STATE_NOT_STARTED = 0, /**< Startup not yet called */
    NX_STARTUP_STATE_BOARD_INIT,      /**< Board init in progress */
    NX_STARTUP_STATE_OS_INIT,         /**< OS init in progress */
    NX_STARTUP_STATE_AUTO_INIT,       /**< Auto init in progress */
    NX_STARTUP_STATE_MAIN_RUNNING,    /**< Main is running */
    NX_STARTUP_STATE_COMPLETE         /**< Startup complete */
} nx_startup_state_t;

/*---------------------------------------------------------------------------*/
/* Weak Symbol Declarations                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Board-level initialization (weak symbol)
 *
 * Override this function to perform board-specific initialization
 * such as clock configuration, power management, and GPIO setup.
 *
 * This function is called first in the startup sequence.
 *
 * \note            Default implementation does nothing.
 */
NX_WEAK void nx_board_init(void);

/**
 * \brief           OS initialization (weak symbol)
 *
 * Override this function to perform OS-specific initialization
 * such as kernel initialization and scheduler setup.
 *
 * This function is called after nx_board_init() and before
 * the automatic initialization functions.
 *
 * \note            Default implementation does nothing.
 */
NX_WEAK void nx_os_init(void);

/*---------------------------------------------------------------------------*/
/* Public API                                                                */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Execute system startup sequence
 *
 * This function executes the complete startup sequence:
 * 1. nx_board_init()  - Board-level initialization (weak, user override)
 * 2. nx_os_init()     - OS initialization (weak, user override)
 * 3. nx_init_run()    - Execute all registered init functions
 * 4. main() or main_thread - User application entry
 *
 * In bare-metal mode, main() is called directly.
 * In RTOS mode, a main thread is created and the scheduler is started.
 *
 * \note            This function does not return in normal operation.
 */
void nx_startup(void);

/**
 * \brief           Execute startup with custom configuration
 * \param[in]       config: Startup configuration (NULL for defaults)
 *
 * Same as nx_startup() but allows customization of startup parameters.
 *
 * \note            This function does not return in normal operation.
 */
void nx_startup_with_config(const nx_startup_config_t* config);

/**
 * \brief           Get current startup state
 * \return          Current startup state
 */
nx_startup_state_t nx_startup_get_state(void);

/**
 * \brief           Check if startup is complete
 * \return          true if startup sequence completed, false otherwise
 */
bool nx_startup_is_complete(void);

/**
 * \brief           Get default startup configuration
 * \param[out]      config: Configuration structure to fill
 *
 * Fills the configuration structure with default values.
 */
void nx_startup_get_default_config(nx_startup_config_t* config);

/*---------------------------------------------------------------------------*/
/* Test Support                                                              */
/*---------------------------------------------------------------------------*/

#ifdef NX_STARTUP_TEST_MODE

/**
 * \brief           Reset startup state for testing
 *
 * \note            Only available when NX_STARTUP_TEST_MODE is defined.
 */
void nx_startup_reset_for_test(void);

/**
 * \brief           Set startup state for testing
 * \param[in]       state: State to set
 *
 * \note            Only available when NX_STARTUP_TEST_MODE is defined.
 */
void nx_startup_set_state_for_test(nx_startup_state_t state);

#endif /* NX_STARTUP_TEST_MODE */

/*---------------------------------------------------------------------------*/
/* Compiler-Specific Entry Point Redirection                                 */
/*---------------------------------------------------------------------------*/

/*
 * Entry point redirection allows the startup framework to intercept
 * the program entry and execute initialization before main().
 *
 * - Arm Compiler: Uses $Sub$main / $Super$main pattern
 * - GCC: Uses entry() function with -eentry linker option
 * - IAR: Uses __low_level_init() hook
 */

#if defined(__ARMCC_VERSION)
/*---------------------------------------------------------------------------*/
/* Arm Compiler Entry Redirection                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Original main function (Arm Compiler)
 *
 * This symbol refers to the user's main() function.
 * The $Sub$main function intercepts the call and redirects to nx_startup().
 */
extern int $Super$main(void);

/**
 * \brief           Call original main (Arm Compiler)
 */
#define NX_CALL_MAIN() $Super$main()

#elif defined(__GNUC__) && !defined(_MSC_VER)
/*---------------------------------------------------------------------------*/
/* GCC Entry Redirection                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           User main function
 */
extern int main(void);

/**
 * \brief           Call original main (GCC)
 */
#define NX_CALL_MAIN() main()

#elif defined(__ICCARM__)
/*---------------------------------------------------------------------------*/
/* IAR Entry Redirection                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           User main function
 */
extern int main(void);

/**
 * \brief           Call original main (IAR)
 */
#define NX_CALL_MAIN() main()

#else
/*---------------------------------------------------------------------------*/
/* Default / Testing (MSVC, etc.)                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           User main function
 */
extern int main(void);

/**
 * \brief           Call original main (default)
 */
#define NX_CALL_MAIN() main()

#endif /* Compiler selection */

#ifdef __cplusplus
}
#endif

#endif /* NX_STARTUP_H */
