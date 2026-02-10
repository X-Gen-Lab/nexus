/**
 * \file            stm32_gpio_device.c
 * \brief           STM32 GPIO device registration
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-02-08
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements GPIO device registration using Kconfig-driven
 *                  configuration. Automatically registers GPIO devices based
 *                  on enabled pins in Kconfig.
 */

#include "hal/base/nx_device.h"
#include "hal/interface/nx_gpio.h"
#include "hal/system/nx_mem.h"
#include "nexus_config.h"
#include "stm32_gpio.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define DEVICE_TYPE STM32_GPIO

/*---------------------------------------------------------------------------*/
/* Helper Macros                                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Convert port letter and pin number to device name
 * \note            Name format based on rw_mode:
 *                  - mode 0: "GPIO<port><pin>_R" (read-only)
 *                  - mode 1: "GPIO<port><pin>_W" (write-only)
 *                  - mode 2: "GPIO<port><pin>" (read-write)
 */
#define GPIO_NAME_READ(port, pin)  "GPIO" #port #pin "_R"
#define GPIO_NAME_WRITE(port, pin) "GPIO" #port #pin "_W"
#define GPIO_NAME_RW(port, pin)    "GPIO" #port #pin

/**
 * \brief           Convert port letter to GPIO_TypeDef pointer
 */
#define GPIO_PORT(port) GPIO##port

/**
 * \brief           Convert pin number to GPIO_PIN_x
 */
#define GPIO_PIN(pin) GPIO_PIN_##pin

/*---------------------------------------------------------------------------*/
/* State Initialization Helper                                               */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize GPIO state structure
 */
static stm32_gpio_state_t* gpio_alloc_state(const stm32_gpio_config_t* config) {
    stm32_gpio_state_t* state =
        (stm32_gpio_state_t*)nx_mem_alloc(sizeof(stm32_gpio_state_t));
    if (!state) {
        return NULL;
    }
    memset(state, 0, sizeof(stm32_gpio_state_t));

    state->config = config;
    state->port = config->port;
    state->pin = config->pin;
    state->initialized = false;
    state->suspended = false;

    /* Clear interrupt context */
    state->exti.callback = NULL;
    state->exti.user_data = NULL;
    state->exti.trigger = NX_GPIO_TRIGGER_RISING;
    state->exti.enabled = false;

    /* Clear statistics */
    state->stats.read_count = 0;
    state->stats.write_count = 0;
    state->stats.toggle_count = 0;

    return state;
}

/*---------------------------------------------------------------------------*/
/* Device Initialization                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Device initialization function for Kconfig registration
 * \details         Only allocates structures and initializes interfaces.
 *                  Hardware initialization is done in lifecycle init.
 */
static void* stm32_gpio_device_init(const nx_device_t* dev) {
    const stm32_gpio_config_t* config = (const stm32_gpio_config_t*)dev->config;

    /* Validate configuration */
    if (!config) {
        return NULL;
    }

    void* api = NULL;

    if (config->rw_mode == 0) {
        /* Read-only mode */
        stm32_gpio_read_impl_t* impl = (stm32_gpio_read_impl_t*)nx_mem_alloc(
            sizeof(stm32_gpio_read_impl_t));
        if (!impl) {
            return NULL;
        }
        memset(impl, 0, sizeof(stm32_gpio_read_impl_t));

        /* Initialize interfaces */
        stm32_gpio_init_read(&impl->base);
        stm32_gpio_init_lifecycle_read(&impl->lifecycle);
        stm32_gpio_init_power_read(&impl->power);

        /* Allocate and initialize state */
        impl->state = gpio_alloc_state(config);
        if (!impl->state) {
            nx_mem_free(impl);
            return NULL;
        }

        impl->device = (nx_device_t*)dev;
        api = &impl->base;

    } else if (config->rw_mode == 1) {
        /* Write-only mode */
        stm32_gpio_write_impl_t* impl = (stm32_gpio_write_impl_t*)nx_mem_alloc(
            sizeof(stm32_gpio_write_impl_t));
        if (!impl) {
            return NULL;
        }
        memset(impl, 0, sizeof(stm32_gpio_write_impl_t));

        /* Initialize interfaces */
        stm32_gpio_init_write(&impl->base);
        stm32_gpio_init_lifecycle_write(&impl->lifecycle);
        stm32_gpio_init_power_write(&impl->power);

        /* Allocate and initialize state */
        impl->state = gpio_alloc_state(config);
        if (!impl->state) {
            nx_mem_free(impl);
            return NULL;
        }

        impl->device = (nx_device_t*)dev;
        api = &impl->base;

    } else if (config->rw_mode == 2) {
        /* Read-write mode */
        stm32_gpio_read_write_impl_t* impl =
            (stm32_gpio_read_write_impl_t*)nx_mem_alloc(
                sizeof(stm32_gpio_read_write_impl_t));
        if (!impl) {
            return NULL;
        }
        memset(impl, 0, sizeof(stm32_gpio_read_write_impl_t));

        /* Initialize interfaces */
        stm32_gpio_init_read_write(&impl->base);
        stm32_gpio_init_lifecycle_read_write(&impl->lifecycle);
        stm32_gpio_init_power_read_write(&impl->power);

        /* Allocate and initialize state */
        impl->state = gpio_alloc_state(config);
        if (!impl->state) {
            nx_mem_free(impl);
            return NULL;
        }

        impl->device = (nx_device_t*)dev;
        api = &impl->base;
    }

    return api;
}

/*---------------------------------------------------------------------------*/
/* GPIO Configuration Macro                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Define GPIO configuration structure
 * \param[in]       p: Port letter (A, B, C, etc.)
 * \param[in]       n: Pin number (0-15)
 */
#define STM32_GPIO_CONFIG(p, n)                                                \
    static const stm32_gpio_config_t gpio_config_##p##n = {                    \
        .port = GPIO_PORT(p),                                                  \
        .pin = GPIO_PIN(n),                                                    \
        .mode = NX_CONFIG_GPIO_##p##n##_MODE,                                  \
        .pull = NX_CONFIG_GPIO_##p##n##_PULL,                                  \
        .speed = NX_CONFIG_GPIO_##p##n##_SPEED,                                \
        .alternate = 0,                                                        \
        .init_value = NX_CONFIG_GPIO_##p##n##_INIT_VALUE,                      \
        .rw_mode = NX_CONFIG_GPIO_##p##n##_RW_MODE,                            \
    }

/*---------------------------------------------------------------------------*/
/* GPIO Device Registration Macro                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Register a GPIO device
 * \param[in]       p: Port letter (A, B, C, etc.)
 * \param[in]       n: Pin number (0-15)
 * \note            Device name is determined by rw_mode configuration:
 *                  - mode 0: "GPIO<port><pin>_R" (read-only)
 *                  - mode 1: "GPIO<port><pin>_W" (write-only)
 *                  - mode 2: "GPIO<port><pin>" (read-write)
 */
#define STM32_GPIO_DEVICE_REGISTER(p, n)                                       \
    STM32_GPIO_CONFIG(p, n);                                                   \
    static nx_device_config_state_t gpio_state_##p##n = {                      \
        .init_res = 0,                                                         \
        .initialized = false,                                                  \
        .api = NULL,                                                           \
    };                                                                         \
    NX_DEVICE_REGISTER(                                                        \
        DEVICE_TYPE, p##n,                                                     \
        (NX_CONFIG_GPIO_##p##n##_RW_MODE == 0                                  \
             ? GPIO_NAME_READ(p, n)                                            \
             : (NX_CONFIG_GPIO_##p##n##_RW_MODE == 1 ? GPIO_NAME_WRITE(p, n)   \
                                                     : GPIO_NAME_RW(p, n))),   \
        &gpio_config_##p##n, &gpio_state_##p##n, stm32_gpio_device_init)

/*---------------------------------------------------------------------------*/
/* Instance Traversal                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Register all enabled GPIO instances
 * \details         Expands NX_DEFINE_INSTANCE_STM32_GPIO macro from
 *                  nexus_config.h. Calls STM32_GPIO_DEVICE_REGISTER for
 *                  each enabled instance.
 */
NX_TRAVERSE_EACH_INSTANCE(STM32_GPIO_DEVICE_REGISTER, DEVICE_TYPE);
