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

#include "hal/provider/nx_device_provider.h"
#include "hal/base/nx_device.h"
#include "hal/interface/nx_gpio.h"
#include "nexus_config.h"
#include "nx_gpio_helpers.h"
#include "nx_gpio_types.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define DEVICE_TYPE NX_GPIO

/*---------------------------------------------------------------------------*/
/* Forward Declarations                                                      */
/*---------------------------------------------------------------------------*/

/* Interface implementations (defined in separate files) */
extern void gpio_init_read_write(nx_gpio_read_write_t* gpio);
extern void gpio_init_lifecycle(nx_lifecycle_t* lifecycle);
extern void gpio_init_power(nx_power_t* power);

/** Stable provider storage belongs to the descriptor, including reopen. */
typedef struct {
    nx_device_config_state_t core;
    nx_gpio_read_write_impl_t impl;
    nx_gpio_state_t state;
} native_gpio_storage_t;

static nx_status_t nx_gpio_construct(const nx_device_t* dev, void** out) {
    if (!out) return NX_ERR_NULL_PTR;
    *out = NULL;
    if (!dev || !dev->state || !dev->config) return NX_ERR_INVALID_PARAM;
    const nx_gpio_platform_config_t* cfg = dev->config;
    if (cfg->port > 7 || cfg->pin > 15 || cfg->mode > NX_GPIO_MODE_ANALOG ||
        cfg->pull > NX_GPIO_PULL_DOWN || cfg->speed > NX_GPIO_SPEED_VERY_HIGH ||
        cfg->af > 15) return NX_ERR_INVALID_PARAM;
    native_gpio_storage_t* storage = NX_CONTAINER_OF(dev->state, native_gpio_storage_t, core);
    nx_gpio_read_write_impl_t* impl = &storage->impl;
    memset(impl, 0, sizeof(*impl));
    impl->state = &storage->state;
    memset(impl->state, 0, sizeof(*impl->state));
    impl->state->port = cfg->port;
    impl->state->pin = cfg->pin;
    impl->state->config = (nx_gpio_config_t){cfg->port, cfg->pin, cfg->mode,
        cfg->pull, cfg->speed, cfg->af};
    impl->state->exti.trigger = NX_GPIO_TRIGGER_RISING;
    impl->device = (nx_device_t*)dev;
    gpio_init_read_write(&impl->base);
    gpio_init_lifecycle(&impl->lifecycle);
    gpio_init_power(&impl->power);
    *out = &impl->base;
    return NX_OK;
}

/**
 * \brief           Helper macro to convert port token to number
 * \note            Uses token concatenation to create port-specific macros
 * \note            Maps port letters (A-H) to numbers (0-7)
 */
#define NX_GPIO_PORT_A 0
#define NX_GPIO_PORT_B 1
#define NX_GPIO_PORT_C 2
#define NX_GPIO_PORT_D 3
#define NX_GPIO_PORT_E 4
#define NX_GPIO_PORT_F 5
#define NX_GPIO_PORT_G 6
#define NX_GPIO_PORT_H 7

/**
 * \brief           Convert port token to port number
 * \param[in]       port: Port letter token (A, B, C, etc.)
 * \return          Port number (0-7)
 * \note            Uses token concatenation: NX_GPIO_PORT_##port
 */
#define NX_GPIO_PORT_TO_NUM(port) NX_GPIO_PORT_##port

/**
 * \brief           Configuration macro - reads from Kconfig
 * \param[in]       _P: Port letter (A, B, C, etc.)
 * \param[in]       _N: Pin number (0-15)
 * \note            Creates static platform configuration structure
 * \note            Reads mode, pull, and speed values from Kconfig symbols
 */
#define NX_GPIO_CONFIG(_P, _N)                                                 \
    static const nx_gpio_platform_config_t gpio_config_##_P##_N = {            \
        .port = NX_GPIO_PORT_TO_NUM(_P),                                       \
        .pin = _N,                                                             \
        .mode = (nx_gpio_mode_t)NX_CONFIG_GPIO##_P##_PIN##_N##_MODE,           \
        .pull = (nx_gpio_pull_t)NX_CONFIG_GPIO##_P##_PIN##_N##_PULL_VALUE,     \
        .speed = (nx_gpio_speed_t)NX_CONFIG_GPIO##_P##_PIN##_N##_SPEED_VALUE,  \
        .af = 0,                                                               \
    }

/**
 * \brief           Device registration macro
 * \param[in]       _P: Port letter (A, B, C, etc.)
 * \param[in]       _N: Pin number (0-15)
 * \note            Generates configuration, state, and registration code
 * \note            Device name format: "GPIO<PORT><PIN>" (e.g., "GPIOA0")
 * \note            Device ID concatenates port and pin without underscore
 */
#define NX_GPIO_DEVICE_REGISTER(_P, _N)                                        \
    NX_GPIO_CONFIG(_P, _N);                                                    \
    static native_gpio_storage_t gpio_storage_##_P##_N;                       \
    NX_DEVICE_REGISTER_TYPED(DEVICE_TYPE, _P##_N, "GPIO" #_P #_N,              \
        &gpio_config_##_P##_N, &gpio_storage_##_P##_N.core,                     \
        NX_DEVICE_CLASS_GPIO, 0, nx_gpio_construct, NULL);

/**
 * \brief           Register all enabled GPIO instances
 * \note            Expands NX_DEFINE_INSTANCE_NX_GPIO macro from nexus_config.h
 * \note            Calls NX_GPIO_DEVICE_REGISTER for each enabled instance
 */
NX_TRAVERSE_EACH_INSTANCE(NX_GPIO_DEVICE_REGISTER, DEVICE_TYPE)
