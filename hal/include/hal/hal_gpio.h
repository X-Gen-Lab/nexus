/**
 * \file            hal_gpio.h
 * \brief           HAL GPIO Interface
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include "hal_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup        HAL_GPIO GPIO Hardware Abstraction
 * \brief           GPIO interface for hardware abstraction
 * \{
 */

/**
 * \brief           GPIO port enumeration
 */
typedef enum {
    HAL_GPIO_PORT_A = 0, /**< GPIO Port A */
    HAL_GPIO_PORT_B,     /**< GPIO Port B */
    HAL_GPIO_PORT_C,     /**< GPIO Port C */
    HAL_GPIO_PORT_D,     /**< GPIO Port D */
    HAL_GPIO_PORT_E,     /**< GPIO Port E */
    HAL_GPIO_PORT_F,     /**< GPIO Port F */
    HAL_GPIO_PORT_G,     /**< GPIO Port G */
    HAL_GPIO_PORT_H,     /**< GPIO Port H */
    HAL_GPIO_PORT_MAX    /**< Maximum port count */
} hal_gpio_port_t;

/**
 * \brief           GPIO pin type
 */
typedef uint8_t hal_gpio_pin_t;

/**
 * \brief           GPIO direction enumeration
 */
typedef enum {
    HAL_GPIO_DIR_INPUT = 0, /**< Input mode */
    HAL_GPIO_DIR_OUTPUT = 1 /**< Output mode */
} hal_gpio_dir_t;

/**
 * \brief           GPIO pull configuration
 */
typedef enum {
    HAL_GPIO_PULL_NONE = 0, /**< No pull-up/pull-down */
    HAL_GPIO_PULL_UP,       /**< Pull-up enabled */
    HAL_GPIO_PULL_DOWN      /**< Pull-down enabled */
} hal_gpio_pull_t;

/**
 * \brief           GPIO output mode
 */
typedef enum {
    HAL_GPIO_OUTPUT_PP = 0, /**< Push-pull output */
    HAL_GPIO_OUTPUT_OD      /**< Open-drain output */
} hal_gpio_output_mode_t;

/**
 * \brief           GPIO output speed
 */
typedef enum {
    HAL_GPIO_SPEED_LOW = 0,  /**< Low speed */
    HAL_GPIO_SPEED_MEDIUM,   /**< Medium speed */
    HAL_GPIO_SPEED_HIGH,     /**< High speed */
    HAL_GPIO_SPEED_VERY_HIGH /**< Very high speed */
} hal_gpio_speed_t;

/**
 * \brief           GPIO level enumeration
 */
typedef enum {
    HAL_GPIO_LEVEL_LOW = 0, /**< Low level (0) */
    HAL_GPIO_LEVEL_HIGH = 1 /**< High level (1) */
} hal_gpio_level_t;

/**
 * \brief           GPIO interrupt trigger mode
 */
typedef enum {
    HAL_GPIO_IRQ_NONE = 0, /**< No interrupt */
    HAL_GPIO_IRQ_RISING,   /**< Rising edge trigger */
    HAL_GPIO_IRQ_FALLING,  /**< Falling edge trigger */
    HAL_GPIO_IRQ_BOTH      /**< Both edges trigger */
} hal_gpio_irq_mode_t;

/**
 * \brief           GPIO configuration structure
 */
typedef struct {
    hal_gpio_dir_t direction;           /**< Pin direction */
    hal_gpio_pull_t pull;               /**< Pull configuration */
    hal_gpio_output_mode_t output_mode; /**< Output mode (if output) */
    hal_gpio_speed_t speed;             /**< Output speed */
    hal_gpio_level_t init_level;        /**< Initial level (if output) */
} hal_gpio_config_t;

/**
 * \brief           GPIO interrupt callback function type
 * \param[in]       port: GPIO port that triggered interrupt
 * \param[in]       pin: GPIO pin that triggered interrupt
 * \param[in]       context: User context pointer
 */
typedef void (*hal_gpio_irq_callback_t)(hal_gpio_port_t port,
                                        hal_gpio_pin_t pin, void* context);

/**
 * \brief           Initialize GPIO pin
 * \param[in]       port: GPIO port
 * \param[in]       pin: GPIO pin number (0-15)
 * \param[in]       config: Pointer to configuration structure
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_gpio_init(hal_gpio_port_t port, hal_gpio_pin_t pin,
                           const hal_gpio_config_t* config);

/**
 * \brief           Deinitialize GPIO pin
 * \param[in]       port: GPIO port
 * \param[in]       pin: GPIO pin number
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_gpio_deinit(hal_gpio_port_t port, hal_gpio_pin_t pin);

/**
 * \brief           Write GPIO pin level
 * \param[in]       port: GPIO port
 * \param[in]       pin: GPIO pin number
 * \param[in]       level: Level to write
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_gpio_write(hal_gpio_port_t port, hal_gpio_pin_t pin,
                            hal_gpio_level_t level);

/**
 * \brief           Read GPIO pin level
 * \param[in]       port: GPIO port
 * \param[in]       pin: GPIO pin number
 * \param[out]      level: Pointer to store read level
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_gpio_read(hal_gpio_port_t port, hal_gpio_pin_t pin,
                           hal_gpio_level_t* level);

/**
 * \brief           Toggle GPIO pin level
 * \param[in]       port: GPIO port
 * \param[in]       pin: GPIO pin number
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_gpio_toggle(hal_gpio_port_t port, hal_gpio_pin_t pin);

/**
 * \brief           Configure GPIO interrupt
 * \param[in]       port: GPIO port
 * \param[in]       pin: GPIO pin number
 * \param[in]       mode: Interrupt trigger mode
 * \param[in]       callback: Interrupt callback function
 * \param[in]       context: User context for callback
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_gpio_irq_config(hal_gpio_port_t port, hal_gpio_pin_t pin,
                                 hal_gpio_irq_mode_t mode,
                                 hal_gpio_irq_callback_t callback,
                                 void* context);

/**
 * \brief           Enable GPIO interrupt
 * \param[in]       port: GPIO port
 * \param[in]       pin: GPIO pin number
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_gpio_irq_enable(hal_gpio_port_t port, hal_gpio_pin_t pin);

/**
 * \brief           Disable GPIO interrupt
 * \param[in]       port: GPIO port
 * \param[in]       pin: GPIO pin number
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_gpio_irq_disable(hal_gpio_port_t port, hal_gpio_pin_t pin);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* HAL_GPIO_H */
