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

/* Static per-descriptor storage. Opening a GPIO never consumes a heap block. */
typedef struct {
    nx_device_config_state_t core; /* First: descriptor state owns this slot. */
    stm32_gpio_state_t runtime;
    union {
        stm32_gpio_read_impl_t read;
        stm32_gpio_write_impl_t write;
        stm32_gpio_read_write_impl_t read_write;
    } implementation;
} stm32_gpio_slot_t;

static nx_status_t stm32_gpio_construct(const nx_device_t* dev, void** api) {
    if (!dev || !dev->state || !dev->config || !api) return NX_ERR_INVALID_PARAM;
    *api = NULL;
    const stm32_gpio_config_t* config = dev->config;
    if (config->rw_mode > 2) return NX_ERR_INVALID_PARAM;
    nx_device_class_t expected = config->rw_mode == 0 ? NX_DEVICE_CLASS_GPIO_READ :
        config->rw_mode == 1 ? NX_DEVICE_CLASS_GPIO_WRITE : NX_DEVICE_CLASS_GPIO;
    if (dev->device_class != expected) return NX_ERR_TYPE_MISMATCH;
    stm32_gpio_slot_t* slot = (stm32_gpio_slot_t*)dev->state;
    memset(&slot->runtime, 0, sizeof(slot->runtime));
    memset(&slot->implementation, 0, sizeof(slot->implementation));
    slot->runtime.config = config;
    slot->runtime.port = config->port;
    slot->runtime.pin = config->pin;
    slot->runtime.exti.trigger = NX_GPIO_TRIGGER_RISING;
    if (config->rw_mode == 0) {
        stm32_gpio_read_impl_t* impl = &slot->implementation.read;
        stm32_gpio_init_read(&impl->base);
        stm32_gpio_init_lifecycle_read(&impl->lifecycle);
        stm32_gpio_init_power_read(&impl->power);
        impl->state = &slot->runtime;
        impl->device = (nx_device_t*)dev;
        *api = &impl->base;
    } else if (config->rw_mode == 1) {
        stm32_gpio_write_impl_t* impl = &slot->implementation.write;
        stm32_gpio_init_write(&impl->base);
        stm32_gpio_init_lifecycle_write(&impl->lifecycle);
        stm32_gpio_init_power_write(&impl->power);
        impl->state = &slot->runtime;
        impl->device = (nx_device_t*)dev;
        *api = &impl->base;
    } else {
        stm32_gpio_read_write_impl_t* impl = &slot->implementation.read_write;
        stm32_gpio_init_read_write(&impl->base);
        stm32_gpio_init_lifecycle_read_write(&impl->lifecycle);
        stm32_gpio_init_power_read_write(&impl->power);
        impl->state = &slot->runtime;
        impl->device = (nx_device_t*)dev;
        *api = &impl->base;
    }
    return NX_OK;
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
    static stm32_gpio_slot_t gpio_slot_##p##n;                                 \
    NX_DEVICE_REGISTER_TYPED(                                                \
        DEVICE_TYPE, p##n,                                                     \
        (NX_CONFIG_GPIO_##p##n##_RW_MODE == 0                                  \
             ? GPIO_NAME_READ(p, n)                                            \
             : (NX_CONFIG_GPIO_##p##n##_RW_MODE == 1 ? GPIO_NAME_WRITE(p, n)   \
                                                     : GPIO_NAME_RW(p, n))),   \
        &gpio_config_##p##n, &gpio_slot_##p##n.core,                            \
        (NX_CONFIG_GPIO_##p##n##_RW_MODE == 0 ? NX_DEVICE_CLASS_GPIO_READ :     \
         NX_CONFIG_GPIO_##p##n##_RW_MODE == 1 ? NX_DEVICE_CLASS_GPIO_WRITE :    \
         NX_DEVICE_CLASS_GPIO), 0, stm32_gpio_construct, NULL)

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
