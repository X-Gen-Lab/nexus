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
#include "hal/system/nx_mem.h"
#include "nexus_config.h"
#include "nx_gpio_helpers.h"
#include "nx_gpio_types.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define DEVICE_TYPE NX_GPIO

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
 * \param[in]       impl: GPIO implementation structure pointer
 * \param[in]       index: Device index calculated from port and pin
 * \param[in]       platform_cfg: Platform configuration from Kconfig
 * \note            Allocates state memory and initializes all interfaces
 */
NX_UNUSED static void
gpio_init_instance(nx_gpio_read_write_impl_t* impl, uint8_t index,
                   const nx_gpio_platform_config_t* platform_cfg) {
    /* Initialize interfaces (implemented in separate files) */
    gpio_init_read(&impl->base.read);
    gpio_init_write(&impl->base.write);
    gpio_init_lifecycle(&impl->lifecycle);
    gpio_init_power(&impl->power);

    /* Allocate and initialize state */
    impl->state = (nx_gpio_state_t*)nx_mem_alloc(sizeof(nx_gpio_state_t));
    if (!impl->state) {
        return;
    }
    memset(impl->state, 0, sizeof(nx_gpio_state_t));

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
 * \details         Allocates and initializes GPIO device with error handling.
 *                  Returns NULL on any failure (memory allocation, invalid
 *                  config, or initialization failure).
 * \note            Ensures proper cleanup of allocated resources on failure.
 */
NX_UNUSED static void* nx_gpio_device_init(const nx_device_t* dev) {
    const nx_gpio_platform_config_t* config =
        (const nx_gpio_platform_config_t*)dev->config;

    /* Validate configuration */
    if (config == NULL) {
        return NULL;
    }

    /* Allocate implementation structure */
    nx_gpio_read_write_impl_t* impl = (nx_gpio_read_write_impl_t*)nx_mem_alloc(
        sizeof(nx_gpio_read_write_impl_t));
    if (!impl) {
        return NULL;
    }
    memset(impl, 0, sizeof(nx_gpio_read_write_impl_t));

    /* Initialize instance with platform configuration */
    uint8_t index = (config->port * 16 + config->pin);
    gpio_init_instance(impl, index, config);

    /* Check if state allocation succeeded */
    if (!impl->state) {
        nx_mem_free(impl);
        return NULL;
    }

    /* Device is created but not initialized - tests will call init() */
    return &impl->base;
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
    static nx_device_config_state_t gpio_kconfig_state_##_P##_N = {            \
        .init_res = 0,                                                         \
        .initialized = false,                                                  \
    };                                                                         \
    NX_DEVICE_REGISTER(DEVICE_TYPE, _P##_N, "GPIO" #_P #_N,                    \
                       &gpio_config_##_P##_N, &gpio_kconfig_state_##_P##_N,    \
                       nx_gpio_device_init);

/**
 * \brief           Register all enabled GPIO instances
 * \note            Expands NX_DEFINE_INSTANCE_NX_GPIO macro from nexus_config.h
 * \note            Calls NX_GPIO_DEVICE_REGISTER for each enabled instance
 */
NX_TRAVERSE_EACH_INSTANCE(NX_GPIO_DEVICE_REGISTER, DEVICE_TYPE);
