/**
 * \file            nx_spi_power.c
 * \brief           SPI power management interface for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements SPI power management operations. In Native
 *                  platform simulation, power is always enabled.
 */

#include "hal/base/nx_device.h"
#include "hal/nx_status.h"
#include "nx_spi_helpers.h"
#include "nx_spi_types.h"

/*---------------------------------------------------------------------------*/
/* Power Interface Implementation                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Enable power implementation
 */
static nx_status_t spi_power_enable(nx_power_t* self) {
    /* No-op in simulation */
    (void)self;
    return NX_OK;
}

/**
 * \brief           Disable power implementation
 */
static nx_status_t spi_power_disable(nx_power_t* self) {
    /* No-op in simulation */
    (void)self;
    return NX_OK;
}

/**
 * \brief           Check if power is enabled implementation
 */
static bool spi_power_is_enabled(nx_power_t* self) {
    nx_spi_impl_t* impl = NX_CONTAINER_OF(self, nx_spi_impl_t, power);

    /* Parameter validation */
    if (!impl->state) {
        return false;
    }

    /* Power is enabled if initialized and not suspended */
    return impl->state->initialized && !impl->state->suspended;
}

/**
 * \brief           Set power callback implementation
 */
static nx_status_t spi_power_set_callback(nx_power_t* self,
                                          nx_power_callback_t callback,
                                          void* user_data) {
    /* No-op in simulation */
    (void)self;
    (void)callback;
    (void)user_data;
    return NX_OK;
}

/*---------------------------------------------------------------------------*/
/* Interface Initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize power interface
 */
void spi_init_power(nx_power_t* power) {
    power->enable = spi_power_enable;
    power->disable = spi_power_disable;
    power->is_enabled = spi_power_is_enabled;
    power->set_callback = spi_power_set_callback;
}
