/**
 * \file            nx_power_manager.c
 * \brief           System power manager implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-17
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "hal/system/nx_power_manager.h"
#include "hal/nx_status.h"
#include "hal/nx_types.h"

/*---------------------------------------------------------------------------*/
/* Private Types                                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Power manager implementation structure
 */
typedef struct {
    nx_power_manager_t base;      /**< Base interface (must be first) */
    nx_power_mode_t current_mode; /**< Current power mode */
} nx_power_manager_impl_t;

/*---------------------------------------------------------------------------*/
/* Private Variables                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Global power manager instance
 */
static nx_power_manager_impl_t g_power_manager = {
    .current_mode = NX_POWER_RUN,
};

/*---------------------------------------------------------------------------*/
/* Private Function Prototypes                                               */
/*---------------------------------------------------------------------------*/

static nx_status_t power_manager_enter_mode(nx_power_manager_t* self,
                                            nx_power_mode_t mode);
static nx_power_mode_t power_manager_get_mode(nx_power_manager_t* self);

/*---------------------------------------------------------------------------*/
/* Public Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get power manager instance
 */
nx_power_manager_t* nx_get_power_manager(void) {
    /* Initialize function pointers if not already done */
    if (g_power_manager.base.enter_mode == NULL) {
        g_power_manager.base.enter_mode = power_manager_enter_mode;
        g_power_manager.base.get_mode = power_manager_get_mode;
    }
    return &g_power_manager.base;
}

/*---------------------------------------------------------------------------*/
/* Private Functions                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Enter specified power mode
 */
static nx_status_t power_manager_enter_mode(nx_power_manager_t* self,
                                            nx_power_mode_t mode) {
    NX_ASSERT(self != NULL);

    nx_power_manager_impl_t* impl =
        NX_CONTAINER_OF(self, nx_power_manager_impl_t, base);

    /* Validate power mode */
    if (mode > NX_POWER_STOP) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Update current mode */
    impl->current_mode = mode;

    /* Platform-specific power mode transition would go here */
    /* For now, this is a simplified implementation that just tracks the mode */

    return NX_OK;
}

/**
 * \brief           Get current power mode
 */
static nx_power_mode_t power_manager_get_mode(nx_power_manager_t* self) {
    NX_ASSERT(self != NULL);

    nx_power_manager_impl_t* impl =
        NX_CONTAINER_OF(self, nx_power_manager_impl_t, base);
    return impl->current_mode;
}
