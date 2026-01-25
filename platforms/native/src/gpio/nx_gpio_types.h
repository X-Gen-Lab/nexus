/**
 * \file            nx_gpio_types.h
 * \brief           GPIO type definitions for Native platform
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef NX_GPIO_TYPES_H
#define NX_GPIO_TYPES_H

#include "hal/interface/nx_diagnostic.h"
#include "hal/interface/nx_gpio.h"
#include "hal/interface/nx_lifecycle.h"
#include "hal/interface/nx_power.h"
#include "hal/nx_status.h"
#include "hal/nx_types.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Forward Declarations                                                      */
/*---------------------------------------------------------------------------*/

/* Forward declare device type */
typedef struct nx_device_s nx_device_t;

/*---------------------------------------------------------------------------*/
/* Platform Configuration Structure                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           GPIO platform configuration structure
 *
 * Contains compile-time configuration from Kconfig.
 */
typedef struct nx_gpio_platform_config_s {
    uint8_t port;  /**< GPIO port (A=0, B=1, etc.) */
    uint8_t pin;   /**< GPIO pin number (0-15) */
    uint8_t mode;  /**< GPIO mode */
    uint8_t pull;  /**< Pull-up/pull-down configuration */
    uint8_t speed; /**< GPIO speed */
    uint8_t af;    /**< Alternate function number */
} nx_gpio_platform_config_t;

/*---------------------------------------------------------------------------*/
/* GPIO Configuration Structure                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           GPIO runtime configuration structure
 */
typedef struct nx_gpio_config_s {
    uint8_t port;  /**< GPIO port (A=0, B=1, etc.) */
    uint8_t pin;   /**< GPIO pin number (0-15) */
    uint8_t mode;  /**< GPIO mode */
    uint8_t pull;  /**< Pull-up/pull-down configuration */
    uint8_t speed; /**< GPIO speed */
    uint8_t af;    /**< Alternate function number */
} nx_gpio_config_t;

/*---------------------------------------------------------------------------*/
/* GPIO Statistics Structure                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           GPIO statistics structure
 */
typedef struct nx_gpio_stats_s {
    uint32_t read_count;   /**< Number of read operations */
    uint32_t write_count;  /**< Number of write operations */
    uint32_t toggle_count; /**< Number of toggle operations */
    uint32_t exti_count;   /**< Number of external interrupts */
} nx_gpio_stats_t;

/*---------------------------------------------------------------------------*/
/* GPIO Interrupt Context Structure                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           GPIO interrupt context structure
 */
typedef struct nx_gpio_exti_ctx_s {
    nx_gpio_callback_t callback; /**< Interrupt callback function */
    void* user_data;             /**< User context pointer */
    nx_gpio_trigger_t trigger;   /**< Interrupt trigger type */
    bool enabled;                /**< Interrupt enabled flag */
} nx_gpio_exti_ctx_t;

/*---------------------------------------------------------------------------*/
/* GPIO State Structure                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           GPIO state structure
 *
 * Contains runtime state and statistics.
 */
typedef struct nx_gpio_state_s {
    uint8_t port;            /**< GPIO port */
    uint8_t pin;             /**< GPIO pin number */
    nx_gpio_config_t config; /**< Configuration */
    nx_gpio_stats_t stats;   /**< Statistics */
    nx_gpio_exti_ctx_t exti; /**< External interrupt context */
    uint8_t pin_state;       /**< Current pin state (0 or 1) */
    bool initialized;        /**< Initialization flag */
    bool suspended;          /**< Suspend flag */
} nx_gpio_state_t;

/*---------------------------------------------------------------------------*/
/* GPIO Read Implementation Structure                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           GPIO read implementation structure
 *
 * Contains read interface and state pointer.
 */
typedef struct nx_gpio_read_impl_s {
    nx_gpio_read_t base;      /**< Base GPIO read interface */
    nx_lifecycle_t lifecycle; /**< Lifecycle interface */
    nx_power_t power;         /**< Power interface */
    nx_gpio_state_t* state;   /**< State pointer */
    nx_device_t* device;      /**< Device descriptor */
} nx_gpio_read_impl_t;

/*---------------------------------------------------------------------------*/
/* GPIO Write Implementation Structure                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           GPIO write implementation structure
 *
 * Contains write interface and state pointer.
 */
typedef struct nx_gpio_write_impl_s {
    nx_gpio_write_t base;     /**< Base GPIO write interface */
    nx_lifecycle_t lifecycle; /**< Lifecycle interface */
    nx_power_t power;         /**< Power interface */
    nx_gpio_state_t* state;   /**< State pointer */
    nx_device_t* device;      /**< Device descriptor */
} nx_gpio_write_impl_t;

/*---------------------------------------------------------------------------*/
/* GPIO Read-Write Implementation Structure                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           GPIO read-write implementation structure
 *
 * Contains both read and write interfaces and state pointer.
 */
typedef struct nx_gpio_read_write_impl_s {
    nx_gpio_read_write_t base; /**< Base GPIO read-write interface */
    nx_lifecycle_t lifecycle;  /**< Lifecycle interface */
    nx_power_t power;          /**< Power interface */
    nx_gpio_state_t* state;    /**< State pointer */
    nx_device_t* device;       /**< Device descriptor */
} nx_gpio_read_write_impl_t;

#ifdef __cplusplus
}
#endif

#endif /* NX_GPIO_TYPES_H */
