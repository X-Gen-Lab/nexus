/**
 * \file            nx_gpio_device.c
 * \brief           GPIO device registration for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements GPIO device registration using Kconfig-driven
 *                  configuration. Provides factory functions for test access
 *                  and manages GPIO instance lifecycle.
 */

#include "hal/base/nx_device.h"
#include "hal/interface/nx_gpio.h"
#include "nexus_config.h"
#include "nx_gpio_helpers.h"
#include "nx_gpio_types.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define NX_GPIO_MAX_PORTS     8
#define NX_GPIO_MAX_PINS      16
#define NX_GPIO_MAX_INSTANCES (NX_GPIO_MAX_PORTS * NX_GPIO_MAX_PINS)
#define DEVICE_TYPE           NX_GPIO

/*---------------------------------------------------------------------------*/
/* Static Storage                                                            */
/*---------------------------------------------------------------------------*/

static nx_gpio_state_t g_gpio_states[NX_GPIO_MAX_INSTANCES];
static nx_gpio_read_write_impl_t g_gpio_instances[NX_GPIO_MAX_INSTANCES];
static uint8_t g_gpio_instance_count = 0;

/*---------------------------------------------------------------------------*/
/* Forward Declarations                                                      */
/*---------------------------------------------------------------------------*/

/* Interface implementations (defined in separate files) */
extern void gpio_init_read(nx_gpio_read_t* read);
extern void gpio_init_write(nx_gpio_write_t* write);
extern void gpio_init_lifecycle(nx_lifecycle_t* lifecycle);
extern void gpio_init_power(nx_power_t* power);

/*---------------------------------------------------------------------------*/
/* Instance Initialization                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize GPIO instance with platform configuration
 */
static void gpio_init_instance(nx_gpio_read_write_impl_t* impl, uint8_t index,
                               const nx_gpio_platform_config_t* platform_cfg) {
    /* Initialize interfaces (implemented in separate files) */
    gpio_init_read(&impl->base.read);
    gpio_init_write(&impl->base.write);
    gpio_init_lifecycle(&impl->lifecycle);
    gpio_init_power(&impl->power);

    /* Link to state */
    impl->state = &g_gpio_states[index];
    impl->state->port = platform_cfg->port;
    impl->state->pin = platform_cfg->pin;
    impl->state->initialized = false;
    impl->state->suspended = false;
    impl->state->pin_state = 0;

    /* Set configuration from Kconfig */
    if (platform_cfg != NULL) {
        impl->state->config.port = platform_cfg->port;
        impl->state->config.pin = platform_cfg->pin;
        impl->state->config.mode = platform_cfg->mode;
        impl->state->config.pull = platform_cfg->pull;
        impl->state->config.speed = platform_cfg->speed;
        impl->state->config.af = platform_cfg->af;
    }

    /* Clear statistics */
    memset(&impl->state->stats, 0, sizeof(nx_gpio_stats_t));

    /* Clear interrupt context */
    impl->state->exti.callback = NULL;
    impl->state->exti.user_data = NULL;
    impl->state->exti.trigger = NX_GPIO_TRIGGER_RISING;
    impl->state->exti.enabled = false;
}

/*---------------------------------------------------------------------------*/
/* Device Registration                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Device initialization function for Kconfig registration
 */
static void* nx_gpio_device_init(const nx_device_t* dev) {
    const nx_gpio_platform_config_t* config =
        (const nx_gpio_platform_config_t*)dev->config;

    if (config == NULL || g_gpio_instance_count >= NX_GPIO_MAX_INSTANCES) {
        return NULL;
    }

    uint8_t index = g_gpio_instance_count++;
    nx_gpio_read_write_impl_t* impl = &g_gpio_instances[index];

    /* Initialize instance with platform configuration */
    gpio_init_instance(impl, index, config);

    /* Initialize lifecycle */
    nx_status_t status = impl->lifecycle.init(&impl->lifecycle);
    if (status != NX_OK) {
        return NULL;
    }

    return &impl->base;
}

/**
 * \brief           Configuration macro - reads from Kconfig
 */
#define NX_GPIO_CONFIG(port, pin)                                              \
    static const nx_gpio_platform_config_t gpio_config_##port##pin = {         \
        .port = NX_GPIO_PORT_NUM(#port[0]),                                    \
        .pin = pin,                                                            \
        .mode = (nx_gpio_mode_t)CONFIG_GPIO##port##_PIN##pin##_MODE,           \
        .pull = (nx_gpio_pull_t)CONFIG_GPIO##port##_PIN##pin##_PULL_VALUE,     \
        .speed = (nx_gpio_speed_t)CONFIG_GPIO##port##_PIN##pin##_SPEED_VALUE,  \
        .af = 0,                                                               \
    }

/**
 * \brief           Device registration macro
 */
#define NX_GPIO_DEVICE_REGISTER(port, pin)                                     \
    NX_GPIO_CONFIG(port, pin);                                                 \
    static nx_device_config_state_t gpio_kconfig_state_##port##pin = {         \
        .init_res = 0,                                                         \
        .initialized = false,                                                  \
    };                                                                         \
    NX_DEVICE_REGISTER(DEVICE_TYPE, (NX_GPIO_PORT_NUM(#port[0]) * 16 + pin),   \
                       "GPIO" #port #pin, &gpio_config_##port##pin,            \
                       &gpio_kconfig_state_##port##pin, nx_gpio_device_init)

/* Register all enabled GPIO instances */
NX_DEFINE_INSTANCE_NX_GPIO(NX_GPIO_DEVICE_REGISTER);

/*---------------------------------------------------------------------------*/
/* Legacy Factory Functions (for backward compatibility)                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get GPIO instance by port and pin (legacy)
 */
nx_gpio_read_write_t* nx_gpio_native_get(uint8_t port, uint8_t pin) {
    if (port >= NX_GPIO_MAX_PORTS || pin >= NX_GPIO_MAX_PINS) {
        return NULL;
    }

    /* Use device registration mechanism */
    char name[16];
    const char* port_names[] = {"A", "B", "C", "D", "E", "F", "G", "H"};
    snprintf(name, sizeof(name), "GPIO%s%d", port_names[port], pin);
    return (nx_gpio_read_write_t*)nx_device_get(name);
}

/**
 * \brief           Get GPIO read instance (legacy)
 */
nx_gpio_read_t* nx_gpio_read_native_get(uint8_t port, uint8_t pin) {
    nx_gpio_read_write_t* gpio = nx_gpio_native_get(port, pin);
    return gpio ? &gpio->read : NULL;
}

/**
 * \brief           Get GPIO write instance (legacy)
 */
nx_gpio_write_t* nx_gpio_write_native_get(uint8_t port, uint8_t pin) {
    nx_gpio_read_write_t* gpio = nx_gpio_native_get(port, pin);
    return gpio ? &gpio->write : NULL;
}

/**
 * \brief           Reset all GPIO instances (for testing)
 */
void nx_gpio_native_reset_all(void) {
    for (uint8_t i = 0; i < g_gpio_instance_count; i++) {
        nx_gpio_read_write_impl_t* impl = &g_gpio_instances[i];
        if (impl->state && impl->state->initialized) {
            impl->lifecycle.deinit(&impl->lifecycle);
        }
        memset(&g_gpio_states[i], 0, sizeof(nx_gpio_state_t));
    }
    g_gpio_instance_count = 0;
}

/**
 * \brief           Trigger external interrupt (for testing)
 */
nx_status_t nx_gpio_native_trigger_exti(uint8_t port, uint8_t pin,
                                        uint8_t pin_state) {
    if (port >= NX_GPIO_MAX_PORTS || pin >= NX_GPIO_MAX_PINS) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Find the GPIO instance */
    for (uint8_t i = 0; i < g_gpio_instance_count; i++) {
        nx_gpio_read_write_impl_t* impl = &g_gpio_instances[i];
        if (impl->state && impl->state->port == port &&
            impl->state->pin == pin) {
            gpio_trigger_exti(impl->state, pin_state);
            return NX_OK;
        }
    }

    return NX_ERR_NOT_FOUND;
}

/**
 * \brief           Get GPIO pin state (for testing)
 */
uint8_t nx_gpio_native_get_pin_state(uint8_t port, uint8_t pin) {
    if (port >= NX_GPIO_MAX_PORTS || pin >= NX_GPIO_MAX_PINS) {
        return 0;
    }

    /* Find the GPIO instance */
    for (uint8_t i = 0; i < g_gpio_instance_count; i++) {
        nx_gpio_read_write_impl_t* impl = &g_gpio_instances[i];
        if (impl->state && impl->state->port == port &&
            impl->state->pin == pin) {
            return gpio_get_pin_state(impl->state);
        }
    }

    return 0;
}

/**
 * \brief           Get GPIO device descriptor (for testing)
 */
nx_device_t* nx_gpio_native_get_device(uint8_t port, uint8_t pin) {
    if (port >= NX_GPIO_MAX_PORTS || pin >= NX_GPIO_MAX_PINS) {
        return NULL;
    }

    /* Find the GPIO instance */
    for (uint8_t i = 0; i < g_gpio_instance_count; i++) {
        nx_gpio_read_write_impl_t* impl = &g_gpio_instances[i];
        if (impl->state && impl->state->port == port &&
            impl->state->pin == pin) {
            return impl->device;
        }
    }

    return NULL;
}

/*---------------------------------------------------------------------------*/
/* Test Support Functions                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get GPIO state (for testing)
 */
nx_status_t nx_gpio_native_get_state(uint8_t port, uint8_t pin,
                                     bool* initialized, bool* suspended) {
    if (port >= NX_GPIO_MAX_PORTS || pin >= NX_GPIO_MAX_PINS) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Find the GPIO instance */
    for (uint8_t i = 0; i < g_gpio_instance_count; i++) {
        nx_gpio_read_write_impl_t* impl = &g_gpio_instances[i];
        if (impl->state && impl->state->port == port &&
            impl->state->pin == pin) {
            if (initialized) {
                *initialized = impl->state->initialized;
            }
            if (suspended) {
                *suspended = impl->state->suspended;
            }
            return NX_OK;
        }
    }

    return NX_ERR_NOT_FOUND;
}

/**
 * \brief           Reset GPIO instance (for testing)
 */
nx_status_t nx_gpio_native_reset(uint8_t port, uint8_t pin) {
    if (port >= NX_GPIO_MAX_PORTS || pin >= NX_GPIO_MAX_PINS) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Find the GPIO instance */
    for (uint8_t i = 0; i < g_gpio_instance_count; i++) {
        nx_gpio_read_write_impl_t* impl = &g_gpio_instances[i];
        if (impl->state && impl->state->port == port &&
            impl->state->pin == pin) {
            /* Clear statistics */
            memset(&impl->state->stats, 0, sizeof(nx_gpio_stats_t));

            /* Clear interrupt context */
            memset(&impl->state->exti, 0, sizeof(nx_gpio_exti_ctx_t));

            /* Reset state flags */
            impl->state->pin_state = 0;
            impl->state->initialized = false;
            impl->state->suspended = false;

            return NX_OK;
        }
    }

    return NX_ERR_NOT_FOUND;
}
