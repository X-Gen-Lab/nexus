/**
 * \file            nx_assert.c
 * \brief           Nexus HAL assertion implementation
 * \author          Nexus Team
 */

#include "hal/nx_types.h"

#if NX_CONFIG_HAL_ASSERT_ENABLE

/*---------------------------------------------------------------------------*/
/* Private variables                                                         */
/*---------------------------------------------------------------------------*/

static nx_assert_handler_t g_assert_handler = NULL;

/*---------------------------------------------------------------------------*/
/* Public functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Set custom assert handler
 */
void nx_set_assert_handler(nx_assert_handler_t handler) {
    g_assert_handler = handler;
}

/**
 * \brief           Default assert handler
 */
void nx_assert_failed(const char* file, int line, const char* expr) {
    if (g_assert_handler != NULL) {
        g_assert_handler(file, line, expr);
    }

    /* Default behavior: infinite loop */
    (void)file;
    (void)line;
    (void)expr;

    while (1) {
        /* Halt execution */
    }
}

#endif /* NX_CONFIG_HAL_ASSERT_ENABLE */
