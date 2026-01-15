/**
 * \file            nx_gpio.h
 * \brief           GPIO device interface definition
 * \author          Nexus Team
 */

#ifndef NX_GPIO_H
#define NX_GPIO_H

#include "hal/interface/nx_lifecycle.h"
#include "hal/interface/nx_power.h"
#include "hal/nx_status.h"
#include "hal/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           GPIO mode enumeration
 */
typedef enum nx_gpio_mode_e {
    NX_GPIO_MODE_INPUT = 0, /**< Input mode */
    NX_GPIO_MODE_OUTPUT_PP, /**< Output push-pull */
    NX_GPIO_MODE_OUTPUT_OD, /**< Output open-drain */
    NX_GPIO_MODE_AF_PP,     /**< Alternate function push-pull */
    NX_GPIO_MODE_AF_OD,     /**< Alternate function open-drain */
    NX_GPIO_MODE_ANALOG,    /**< Analog mode */
} nx_gpio_mode_t;

/**
 * \brief           GPIO pull-up/pull-down enumeration
 */
typedef enum nx_gpio_pull_e {
    NX_GPIO_PULL_NONE = 0, /**< No pull-up/pull-down */
    NX_GPIO_PULL_UP,       /**< Pull-up */
    NX_GPIO_PULL_DOWN,     /**< Pull-down */
} nx_gpio_pull_t;

/**
 * \brief           GPIO speed enumeration
 */
typedef enum nx_gpio_speed_e {
    NX_GPIO_SPEED_LOW = 0,   /**< Low speed */
    NX_GPIO_SPEED_MEDIUM,    /**< Medium speed */
    NX_GPIO_SPEED_HIGH,      /**< High speed */
    NX_GPIO_SPEED_VERY_HIGH, /**< Very high speed */
} nx_gpio_speed_t;

/**
 * \brief           GPIO external interrupt trigger enumeration
 */
typedef enum nx_gpio_exti_trig_e {
    NX_GPIO_EXTI_NONE = 0, /**< No interrupt */
    NX_GPIO_EXTI_RISING,   /**< Rising edge trigger */
    NX_GPIO_EXTI_FALLING,  /**< Falling edge trigger */
    NX_GPIO_EXTI_BOTH,     /**< Both edges trigger */
} nx_gpio_exti_trig_t;

/**
 * \brief           GPIO configuration structure
 */
typedef struct nx_gpio_config_s {
    nx_gpio_mode_t mode;   /**< GPIO mode */
    nx_gpio_pull_t pull;   /**< Pull-up/pull-down configuration */
    nx_gpio_speed_t speed; /**< GPIO speed */
    uint8_t af_index;      /**< Alternate function index */
} nx_gpio_config_t;

/**
 * \brief           GPIO external interrupt callback type
 * \param[in]       context: User context pointer
 */
typedef void (*nx_gpio_exti_callback_t)(void* context);

/**
 * \brief           GPIO device interface
 */
typedef struct nx_gpio_s nx_gpio_t;
struct nx_gpio_s {
    /* Basic operations */
    uint8_t (*read)(nx_gpio_t* self);
    void (*write)(nx_gpio_t* self, uint8_t state);
    void (*toggle)(nx_gpio_t* self);

    /* Runtime configuration */
    nx_status_t (*set_mode)(nx_gpio_t* self, nx_gpio_mode_t mode);
    nx_status_t (*set_pull)(nx_gpio_t* self, nx_gpio_pull_t pull);
    nx_status_t (*get_config)(nx_gpio_t* self, nx_gpio_config_t* cfg);
    nx_status_t (*set_config)(nx_gpio_t* self, const nx_gpio_config_t* cfg);

    /* Interrupt configuration */
    nx_status_t (*set_exti)(nx_gpio_t* self, nx_gpio_exti_trig_t trig,
                            nx_gpio_exti_callback_t cb, void* ctx);
    nx_status_t (*clear_exti)(nx_gpio_t* self);

    /* Base interfaces */
    nx_lifecycle_t* (*get_lifecycle)(nx_gpio_t* self);
    nx_power_t* (*get_power)(nx_gpio_t* self);
};

#ifdef __cplusplus
}
#endif

#endif /* NX_GPIO_H */
