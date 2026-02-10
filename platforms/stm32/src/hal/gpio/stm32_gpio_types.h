/**
 * \file            stm32_gpio_types.h
 * \brief           STM32 GPIO type definitions
 * \author          Nexus Team
 */

#ifndef STM32_GPIO_TYPES_H
#define STM32_GPIO_TYPES_H

#include "hal/interface/nx_gpio.h"
#include "hal/interface/nx_lifecycle.h"
#include "hal/interface/nx_power.h"
#include "hal/nx_status.h"
#include "hal/nx_types.h"
#include <stdbool.h>
#include <stdint.h>

/* Include ST HAL headers based on chip series */
#if defined(STM32F0)
#include "stm32f0xx_hal.h"
#elif defined(STM32F1)
#include "stm32f1xx_hal.h"
#elif defined(STM32F2)
#include "stm32f2xx_hal.h"
#elif defined(STM32F3)
#include "stm32f3xx_hal.h"
#elif defined(STM32F4)
#include "stm32f4xx_hal.h"
#elif defined(STM32F7)
#include "stm32f7xx_hal.h"
#elif defined(STM32G0)
#include "stm32g0xx_hal.h"
#elif defined(STM32G4)
#include "stm32g4xx_hal.h"
#elif defined(STM32H5)
#include "stm32h5xx_hal.h"
#elif defined(STM32H7)
#include "stm32h7xx_hal.h"
#elif defined(STM32L0)
#include "stm32l0xx_hal.h"
#elif defined(STM32L1)
#include "stm32l1xx_hal.h"
#elif defined(STM32L4)
#include "stm32l4xx_hal.h"
#elif defined(STM32L5)
#include "stm32l5xx_hal.h"
#elif defined(STM32U0)
#include "stm32u0xx_hal.h"
#elif defined(STM32U5)
#include "stm32u5xx_hal.h"
#elif defined(STM32WB)
#include "stm32wbxx_hal.h"
#elif defined(STM32WL)
#include "stm32wlxx_hal.h"
#else
#error "Unsupported STM32 series"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Forward Declarations                                                      */
/*---------------------------------------------------------------------------*/

typedef struct nx_device_s nx_device_t;

/*---------------------------------------------------------------------------*/
/* Platform Configuration Structure                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           STM32 GPIO platform configuration from Kconfig
 */
typedef struct {
    GPIO_TypeDef* port; /**< GPIO port (GPIOA, GPIOB, etc.) */
    uint16_t pin;       /**< GPIO pin (GPIO_PIN_0 to GPIO_PIN_15) */
    uint32_t mode;      /**< GPIO mode */
    uint32_t pull;      /**< Pull-up/pull-down */
    uint32_t speed;     /**< GPIO speed */
    uint32_t alternate; /**< Alternate function */
    uint8_t init_value; /**< Initial output value (0 or 1) */
    uint8_t rw_mode;    /**< Read/Write mode (0=Read, 1=Write, 2=ReadWrite) */
} stm32_gpio_config_t;

/*---------------------------------------------------------------------------*/
/* GPIO Interrupt Context Structure                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           GPIO interrupt context structure
 */
typedef struct {
    nx_gpio_callback_t callback; /**< Interrupt callback function */
    void* user_data;             /**< User context pointer */
    nx_gpio_trigger_t trigger;   /**< Interrupt trigger type */
    bool enabled;                /**< Interrupt enabled flag */
} stm32_gpio_exti_ctx_t;

/*---------------------------------------------------------------------------*/
/* GPIO Statistics Structure                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           GPIO statistics structure
 */
typedef struct {
    uint32_t read_count;   /**< Number of read operations */
    uint32_t write_count;  /**< Number of write operations */
    uint32_t toggle_count; /**< Number of toggle operations */
} stm32_gpio_stats_t;

/*---------------------------------------------------------------------------*/
/* GPIO State Structure                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           GPIO state structure
 */
typedef struct {
    const stm32_gpio_config_t* config; /**< Configuration pointer */
    GPIO_TypeDef* port;                /**< GPIO port */
    uint16_t pin;                      /**< GPIO pin */
    stm32_gpio_exti_ctx_t exti;        /**< External interrupt context */
    stm32_gpio_stats_t stats;          /**< Statistics */
    bool initialized;                  /**< Initialization flag */
    bool suspended;                    /**< Suspend flag */
} stm32_gpio_state_t;

/*---------------------------------------------------------------------------*/
/* GPIO Read Implementation Structure                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           GPIO read implementation structure
 */
typedef struct {
    nx_gpio_read_t base;       /**< Base GPIO read interface */
    nx_lifecycle_t lifecycle;  /**< Lifecycle interface */
    nx_power_t power;          /**< Power interface */
    stm32_gpio_state_t* state; /**< State pointer */
    nx_device_t* device;       /**< Device descriptor */
} stm32_gpio_read_impl_t;

/*---------------------------------------------------------------------------*/
/* GPIO Write Implementation Structure                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           GPIO write implementation structure
 */
typedef struct {
    nx_gpio_write_t base;      /**< Base GPIO write interface */
    nx_lifecycle_t lifecycle;  /**< Lifecycle interface */
    nx_power_t power;          /**< Power interface */
    stm32_gpio_state_t* state; /**< State pointer */
    nx_device_t* device;       /**< Device descriptor */
} stm32_gpio_write_impl_t;

/*---------------------------------------------------------------------------*/
/* GPIO Read-Write Implementation Structure                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           GPIO read-write implementation structure
 */
typedef struct {
    nx_gpio_read_write_t base; /**< Base GPIO read-write interface */
    nx_lifecycle_t lifecycle;  /**< Lifecycle interface */
    nx_power_t power;          /**< Power interface */
    stm32_gpio_state_t* state; /**< State pointer */
    nx_device_t* device;       /**< Device descriptor */
} stm32_gpio_read_write_impl_t;

/*---------------------------------------------------------------------------*/
/* Helper Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get read implementation from base interface
 */
static inline stm32_gpio_read_impl_t*
stm32_gpio_read_get_impl(nx_gpio_read_t* self) {
    return self ? (stm32_gpio_read_impl_t*)self : NULL;
}

/**
 * \brief           Get write implementation from base interface
 */
static inline stm32_gpio_write_impl_t*
stm32_gpio_write_get_impl(nx_gpio_write_t* self) {
    return self ? (stm32_gpio_write_impl_t*)self : NULL;
}

/**
 * \brief           Get read-write implementation from base interface
 */
static inline stm32_gpio_read_write_impl_t*
stm32_gpio_read_write_get_impl(nx_gpio_read_write_t* self) {
    return self ? (stm32_gpio_read_write_impl_t*)self : NULL;
}

/*---------------------------------------------------------------------------*/
/* Interface Initialization Functions                                        */
/*---------------------------------------------------------------------------*/

void stm32_gpio_init_read(nx_gpio_read_t* read);
void stm32_gpio_init_write(nx_gpio_write_t* write);
void stm32_gpio_init_read_write(nx_gpio_read_write_t* read_write);
void stm32_gpio_init_lifecycle_read(nx_lifecycle_t* lifecycle);
void stm32_gpio_init_lifecycle_write(nx_lifecycle_t* lifecycle);
void stm32_gpio_init_lifecycle_read_write(nx_lifecycle_t* lifecycle);
void stm32_gpio_init_power_read(nx_power_t* power);
void stm32_gpio_init_power_write(nx_power_t* power);
void stm32_gpio_init_power_read_write(nx_power_t* power);

#ifdef __cplusplus
}
#endif

#endif /* STM32_GPIO_TYPES_H */
