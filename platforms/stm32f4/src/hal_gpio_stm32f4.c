/**
 * \file            hal_gpio_stm32f4.c
 * \brief           STM32F4 GPIO HAL Implementation (ST HAL Wrapper)
 * \author          Nexus Team
 * \version         2.0.0
 * \date            2026-01-15
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * This implementation wraps ST HAL GPIO functions to provide the Nexus HAL
 * interface. It uses HAL_GPIO_Init(), HAL_GPIO_WritePin(), HAL_GPIO_ReadPin(),
 * HAL_GPIO_TogglePin(), and HAL_GPIO_EXTI_IRQHandler() from the ST HAL library.
 */

#include "hal/hal_gpio.h"
#include "stm32f4xx.h"
#include "stm32f4xx_hal_conf.h"

/*===========================================================================*/
/* Local definitions                                                          */
/*===========================================================================*/

/**
 * \brief           GPIO port base addresses
 */
static GPIO_TypeDef* const gpio_ports[HAL_GPIO_PORT_MAX] = {
    GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, GPIOF, GPIOG, GPIOH};

/**
 * \brief           GPIO clock enable macros (using ST HAL RCC macros)
 */
static void gpio_enable_clock(hal_gpio_port_t port) {
    switch (port) {
        case HAL_GPIO_PORT_A:
            __HAL_RCC_GPIOA_CLK_ENABLE();
            break;
        case HAL_GPIO_PORT_B:
            __HAL_RCC_GPIOB_CLK_ENABLE();
            break;
        case HAL_GPIO_PORT_C:
            __HAL_RCC_GPIOC_CLK_ENABLE();
            break;
        case HAL_GPIO_PORT_D:
            __HAL_RCC_GPIOD_CLK_ENABLE();
            break;
        case HAL_GPIO_PORT_E:
            __HAL_RCC_GPIOE_CLK_ENABLE();
            break;
        case HAL_GPIO_PORT_F:
            __HAL_RCC_GPIOF_CLK_ENABLE();
            break;
        case HAL_GPIO_PORT_G:
            __HAL_RCC_GPIOG_CLK_ENABLE();
            break;
        case HAL_GPIO_PORT_H:
            __HAL_RCC_GPIOH_CLK_ENABLE();
            break;
        default:
            break;
    }
}

/**
 * \brief           Convert pin number to ST HAL pin mask
 */
static uint16_t pin_to_mask(hal_gpio_pin_t pin) {
    return (uint16_t)(1U << pin);
}

/**
 * \brief           IRQ callback storage for each EXTI line (0-15)
 */
static struct {
    hal_gpio_irq_callback_t callback;
    void* context;
    hal_gpio_port_t port;
    bool configured;
} gpio_irq_handlers[16];

/**
 * \brief           Pin initialization state tracking
 */
static struct {
    bool initialized;
    bool is_output;
} gpio_pin_state[HAL_GPIO_PORT_MAX][16];

/*===========================================================================*/
/* Local functions                                                            */
/*===========================================================================*/

/**
 * \brief           Get NVIC IRQ number for EXTI line
 * \param[in]       pin: GPIO pin (EXTI line)
 * \return          IRQ number
 */
static IRQn_Type gpio_get_irqn(hal_gpio_pin_t pin) {
    if (pin <= 4) {
        return (IRQn_Type)(EXTI0_IRQn + pin);
    } else if (pin <= 9) {
        return EXTI9_5_IRQn;
    } else {
        return EXTI15_10_IRQn;
    }
}

/**
 * \brief           Map Nexus HAL speed to ST HAL speed
 */
static uint32_t map_speed(hal_gpio_speed_t speed) {
    switch (speed) {
        case HAL_GPIO_SPEED_LOW:
            return GPIO_SPEED_FREQ_LOW;
        case HAL_GPIO_SPEED_MEDIUM:
            return GPIO_SPEED_FREQ_MEDIUM;
        case HAL_GPIO_SPEED_HIGH:
            return GPIO_SPEED_FREQ_HIGH;
        case HAL_GPIO_SPEED_VERY_HIGH:
            return GPIO_SPEED_FREQ_VERY_HIGH;
        default:
            return GPIO_SPEED_FREQ_LOW;
    }
}

/**
 * \brief           Map Nexus HAL pull to ST HAL pull
 */
static uint32_t map_pull(hal_gpio_pull_t pull) {
    switch (pull) {
        case HAL_GPIO_PULL_NONE:
            return GPIO_NOPULL;
        case HAL_GPIO_PULL_UP:
            return GPIO_PULLUP;
        case HAL_GPIO_PULL_DOWN:
            return GPIO_PULLDOWN;
        default:
            return GPIO_NOPULL;
    }
}

/*===========================================================================*/
/* Public functions                                                           */
/*===========================================================================*/

hal_status_t hal_gpio_init(hal_gpio_port_t port, hal_gpio_pin_t pin,
                           const hal_gpio_config_t* config) {
    GPIO_TypeDef* gpio;
    GPIO_InitTypeDef gpio_init = {0};

    /* Parameter validation */
    if (port >= HAL_GPIO_PORT_MAX || pin > 15) {
        return HAL_ERR_PARAM;
    }

    if (config == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }

    /* Enable clock using ST HAL macro */
    gpio_enable_clock(port);
    gpio = gpio_ports[port];

    /* Configure GPIO_InitTypeDef structure */
    gpio_init.Pin = pin_to_mask(pin);
    gpio_init.Pull = map_pull(config->pull);
    gpio_init.Speed = map_speed(config->speed);

    /* Set mode based on direction and output type */
    if (config->direction == HAL_GPIO_DIR_OUTPUT) {
        if (config->output_mode == HAL_GPIO_OUTPUT_OD) {
            gpio_init.Mode = GPIO_MODE_OUTPUT_OD;
        } else {
            gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
        }
        gpio_pin_state[port][pin].is_output = true;
    } else {
        gpio_init.Mode = GPIO_MODE_INPUT;
        gpio_pin_state[port][pin].is_output = false;
    }

    /* Call ST HAL GPIO Init */
    HAL_GPIO_Init(gpio, &gpio_init);

    /* Set initial level for output using ST HAL */
    if (config->direction == HAL_GPIO_DIR_OUTPUT) {
        GPIO_PinState state = (config->init_level == HAL_GPIO_LEVEL_HIGH)
                                  ? GPIO_PIN_SET
                                  : GPIO_PIN_RESET;
        HAL_GPIO_WritePin(gpio, pin_to_mask(pin), state);
    }

    /* Mark pin as initialized */
    gpio_pin_state[port][pin].initialized = true;

    return HAL_OK;
}

hal_status_t hal_gpio_deinit(hal_gpio_port_t port, hal_gpio_pin_t pin) {
    GPIO_TypeDef* gpio;

    if (port >= HAL_GPIO_PORT_MAX || pin > 15) {
        return HAL_ERR_PARAM;
    }

    gpio = gpio_ports[port];

    /* Disable interrupt if configured */
    hal_gpio_irq_disable(port, pin);

    /* Call ST HAL GPIO DeInit */
    HAL_GPIO_DeInit(gpio, pin_to_mask(pin));

    /* Clear IRQ handler */
    gpio_irq_handlers[pin].callback = NULL;
    gpio_irq_handlers[pin].context = NULL;
    gpio_irq_handlers[pin].configured = false;

    /* Mark pin as uninitialized */
    gpio_pin_state[port][pin].initialized = false;
    gpio_pin_state[port][pin].is_output = false;

    return HAL_OK;
}

hal_status_t hal_gpio_write(hal_gpio_port_t port, hal_gpio_pin_t pin,
                            hal_gpio_level_t level) {
    GPIO_TypeDef* gpio;

    if (port >= HAL_GPIO_PORT_MAX || pin > 15) {
        return HAL_ERR_PARAM;
    }

    /* Check if pin is initialized */
    if (!gpio_pin_state[port][pin].initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Check if pin is configured as output */
    if (!gpio_pin_state[port][pin].is_output) {
        return HAL_ERROR_INVALID_STATE;
    }

    gpio = gpio_ports[port];

    /* Use ST HAL WritePin function */
    GPIO_PinState state =
        (level == HAL_GPIO_LEVEL_HIGH) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(gpio, pin_to_mask(pin), state);

    return HAL_OK;
}

hal_status_t hal_gpio_read(hal_gpio_port_t port, hal_gpio_pin_t pin,
                           hal_gpio_level_t* level) {
    GPIO_TypeDef* gpio;

    if (port >= HAL_GPIO_PORT_MAX || pin > 15) {
        return HAL_ERR_PARAM;
    }

    if (level == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }

    /* Check if pin is initialized */
    if (!gpio_pin_state[port][pin].initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    gpio = gpio_ports[port];

    /* Use ST HAL ReadPin function */
    GPIO_PinState state = HAL_GPIO_ReadPin(gpio, pin_to_mask(pin));
    *level = (state == GPIO_PIN_SET) ? HAL_GPIO_LEVEL_HIGH : HAL_GPIO_LEVEL_LOW;

    return HAL_OK;
}

hal_status_t hal_gpio_toggle(hal_gpio_port_t port, hal_gpio_pin_t pin) {
    GPIO_TypeDef* gpio;

    if (port >= HAL_GPIO_PORT_MAX || pin > 15) {
        return HAL_ERR_PARAM;
    }

    /* Check if pin is initialized */
    if (!gpio_pin_state[port][pin].initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Check if pin is configured as output */
    if (!gpio_pin_state[port][pin].is_output) {
        return HAL_ERROR_INVALID_STATE;
    }

    gpio = gpio_ports[port];

    /* Use ST HAL TogglePin function */
    HAL_GPIO_TogglePin(gpio, pin_to_mask(pin));

    return HAL_OK;
}

hal_status_t hal_gpio_irq_config(hal_gpio_port_t port, hal_gpio_pin_t pin,
                                 hal_gpio_irq_mode_t mode,
                                 hal_gpio_irq_callback_t callback,
                                 void* context) {
    GPIO_TypeDef* gpio;
    GPIO_InitTypeDef gpio_init = {0};

    if (port >= HAL_GPIO_PORT_MAX || pin > 15) {
        return HAL_ERR_PARAM;
    }

    if (mode == HAL_GPIO_IRQ_NONE) {
        /* Disable interrupt */
        return hal_gpio_irq_disable(port, pin);
    }

    if (callback == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }

    /* Enable clock */
    gpio_enable_clock(port);
    gpio = gpio_ports[port];

    /* Enable SYSCFG clock for EXTI configuration */
    __HAL_RCC_SYSCFG_CLK_ENABLE();

    /* Configure GPIO with interrupt mode using ST HAL */
    gpio_init.Pin = pin_to_mask(pin);
    gpio_init.Pull = GPIO_NOPULL;

    /* Map interrupt mode to ST HAL mode */
    switch (mode) {
        case HAL_GPIO_IRQ_RISING:
            gpio_init.Mode = GPIO_MODE_IT_RISING;
            break;
        case HAL_GPIO_IRQ_FALLING:
            gpio_init.Mode = GPIO_MODE_IT_FALLING;
            break;
        case HAL_GPIO_IRQ_BOTH:
            gpio_init.Mode = GPIO_MODE_IT_RISING_FALLING;
            break;
        default:
            return HAL_ERR_PARAM;
    }

    /* Initialize GPIO with interrupt configuration */
    HAL_GPIO_Init(gpio, &gpio_init);

    /* Store callback */
    gpio_irq_handlers[pin].callback = callback;
    gpio_irq_handlers[pin].context = context;
    gpio_irq_handlers[pin].port = port;
    gpio_irq_handlers[pin].configured = true;

    /* Set NVIC priority */
    HAL_NVIC_SetPriority(gpio_get_irqn(pin), 5, 0);

    return HAL_OK;
}

hal_status_t hal_gpio_irq_enable(hal_gpio_port_t port, hal_gpio_pin_t pin) {
    (void)port;

    if (pin > 15) {
        return HAL_ERR_PARAM;
    }

    /* Clear pending flag using ST HAL macro */
    __HAL_GPIO_EXTI_CLEAR_FLAG(pin_to_mask(pin));

    /* Enable NVIC interrupt using ST HAL */
    HAL_NVIC_EnableIRQ(gpio_get_irqn(pin));

    return HAL_OK;
}

hal_status_t hal_gpio_irq_disable(hal_gpio_port_t port, hal_gpio_pin_t pin) {
    (void)port;

    if (pin > 15) {
        return HAL_ERR_PARAM;
    }

    /* Disable EXTI interrupt mask */
    EXTI->IMR &= ~(1UL << pin);

    /* Clear pending flag using ST HAL macro */
    __HAL_GPIO_EXTI_CLEAR_FLAG(pin_to_mask(pin));

    return HAL_OK;
}

/*===========================================================================*/
/* ST HAL Callback Implementation                                             */
/*===========================================================================*/

/**
 * \brief           ST HAL GPIO EXTI Callback
 * \note            This function is called by HAL_GPIO_EXTI_IRQHandler()
 *                  when an EXTI interrupt occurs.
 * \param[in]       GPIO_Pin: Pin mask that triggered the interrupt
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    /* Find which pin triggered the interrupt */
    for (hal_gpio_pin_t pin = 0; pin <= 15; ++pin) {
        if ((GPIO_Pin & pin_to_mask(pin)) != 0) {
            /* Call user callback if configured */
            if (gpio_irq_handlers[pin].callback != NULL &&
                gpio_irq_handlers[pin].configured) {
                gpio_irq_handlers[pin].callback(gpio_irq_handlers[pin].port,
                                                pin,
                                                gpio_irq_handlers[pin].context);
            }
            break;
        }
    }
}

/*===========================================================================*/
/* IRQ Handlers - Using ST HAL EXTI Handler                                   */
/*===========================================================================*/

/* EXTI0 IRQ Handler */
void EXTI0_IRQHandler(void) {
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}

/* EXTI1 IRQ Handler */
void EXTI1_IRQHandler(void) {
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_1);
}

/* EXTI2 IRQ Handler */
void EXTI2_IRQHandler(void) {
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_2);
}

/* EXTI3 IRQ Handler */
void EXTI3_IRQHandler(void) {
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_3);
}

/* EXTI4 IRQ Handler */
void EXTI4_IRQHandler(void) {
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_4);
}

/* EXTI9_5 IRQ Handler */
void EXTI9_5_IRQHandler(void) {
    for (uint16_t pin = 5; pin <= 9; ++pin) {
        HAL_GPIO_EXTI_IRQHandler((uint16_t)(1U << pin));
    }
}

/* EXTI15_10 IRQ Handler */
void EXTI15_10_IRQHandler(void) {
    for (uint16_t pin = 10; pin <= 15; ++pin) {
        HAL_GPIO_EXTI_IRQHandler((uint16_t)(1U << pin));
    }
}

/*===========================================================================*/
/* Alternate Function Support                                                 */
/*===========================================================================*/

hal_status_t hal_gpio_init_af(hal_gpio_port_t port, hal_gpio_pin_t pin,
                              const hal_gpio_af_config_t* config) {
    GPIO_TypeDef* gpio;
    GPIO_InitTypeDef gpio_init = {0};

    /* Parameter validation */
    if (port >= HAL_GPIO_PORT_MAX || pin > 15) {
        return HAL_ERR_PARAM;
    }

    if (config == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }

    /* Validate alternate function number (0-15) */
    if (config->alternate > 15) {
        return HAL_ERR_PARAM;
    }

    /* Enable clock using ST HAL macro */
    gpio_enable_clock(port);
    gpio = gpio_ports[port];

    /* Configure GPIO_InitTypeDef structure for alternate function */
    gpio_init.Pin = pin_to_mask(pin);
    gpio_init.Pull = map_pull(config->pull);
    gpio_init.Speed = map_speed(config->speed);
    gpio_init.Alternate = config->alternate;

    /* Set mode based on output type */
    if (config->output_mode == HAL_GPIO_OUTPUT_OD) {
        gpio_init.Mode = GPIO_MODE_AF_OD;
    } else {
        gpio_init.Mode = GPIO_MODE_AF_PP;
    }

    /* Call ST HAL GPIO Init */
    HAL_GPIO_Init(gpio, &gpio_init);

    /* Mark pin as initialized (not a regular output) */
    gpio_pin_state[port][pin].initialized = true;
    gpio_pin_state[port][pin].is_output = false;

    return HAL_OK;
}
