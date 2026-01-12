/**
 * \file            hal_gpio_stm32f4.c
 * \brief           STM32F4 GPIO HAL Implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "hal/hal_gpio.h"
#include "stm32f4xx.h"

/*===========================================================================*/
/* Local definitions                                                          */
/*===========================================================================*/

/**
 * \brief           GPIO port base addresses
 */
static GPIO_TypeDef* const gpio_ports[HAL_GPIO_PORT_MAX] = {
    GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, GPIOF, GPIOG, GPIOH
};

/**
 * \brief           GPIO clock enable bits
 */
static const uint32_t gpio_clk_bits[HAL_GPIO_PORT_MAX] = {
    RCC_AHB1ENR_GPIOAEN, RCC_AHB1ENR_GPIOBEN, RCC_AHB1ENR_GPIOCEN,
    RCC_AHB1ENR_GPIODEN, RCC_AHB1ENR_GPIOEEN, RCC_AHB1ENR_GPIOFEN,
    RCC_AHB1ENR_GPIOGEN, RCC_AHB1ENR_GPIOHEN
};

/*===========================================================================*/
/* Local functions                                                            */
/*===========================================================================*/

/**
 * \brief           Enable GPIO port clock
 * \param[in]       port: GPIO port
 */
static void gpio_enable_clock(hal_gpio_port_t port)
{
    RCC->AHB1ENR |= gpio_clk_bits[port];
    /* Delay after enabling clock */
    __asm volatile("dsb");
}

/*===========================================================================*/
/* Public functions                                                           */
/*===========================================================================*/

hal_status_t hal_gpio_init(hal_gpio_port_t port,
                           hal_gpio_pin_t pin,
                           const hal_gpio_config_t* config)
{
    GPIO_TypeDef* gpio;
    uint32_t temp;

    /* Parameter validation */
    if (port >= HAL_GPIO_PORT_MAX || pin > 15 || config == NULL) {
        return HAL_ERR_PARAM;
    }

    /* Enable clock */
    gpio_enable_clock(port);
    gpio = gpio_ports[port];

    /* Configure mode (MODER) */
    temp = gpio->MODER;
    temp &= ~(0x03UL << (pin * 2));
    temp |= ((uint32_t)config->direction << (pin * 2));
    gpio->MODER = temp;

    /* Configure output type (OTYPER) */
    if (config->direction == HAL_GPIO_DIR_OUTPUT) {
        temp = gpio->OTYPER;
        temp &= ~(0x01UL << pin);
        temp |= ((uint32_t)config->output_mode << pin);
        gpio->OTYPER = temp;
    }

    /* Configure speed (OSPEEDR) */
    temp = gpio->OSPEEDR;
    temp &= ~(0x03UL << (pin * 2));
    temp |= ((uint32_t)config->speed << (pin * 2));
    gpio->OSPEEDR = temp;

    /* Configure pull-up/pull-down (PUPDR) */
    temp = gpio->PUPDR;
    temp &= ~(0x03UL << (pin * 2));
    temp |= ((uint32_t)config->pull << (pin * 2));
    gpio->PUPDR = temp;

    /* Set initial level for output */
    if (config->direction == HAL_GPIO_DIR_OUTPUT) {
        if (config->init_level == HAL_GPIO_LEVEL_HIGH) {
            gpio->BSRR = (1UL << pin);
        } else {
            gpio->BSRR = (1UL << (pin + 16));
        }
    }

    return HAL_OK;
}

hal_status_t hal_gpio_deinit(hal_gpio_port_t port, hal_gpio_pin_t pin)
{
    GPIO_TypeDef* gpio;

    if (port >= HAL_GPIO_PORT_MAX || pin > 15) {
        return HAL_ERR_PARAM;
    }

    gpio = gpio_ports[port];

    /* Reset to input mode, no pull */
    gpio->MODER &= ~(0x03UL << (pin * 2));
    gpio->PUPDR &= ~(0x03UL << (pin * 2));
    gpio->OSPEEDR &= ~(0x03UL << (pin * 2));
    gpio->OTYPER &= ~(0x01UL << pin);

    return HAL_OK;
}

hal_status_t hal_gpio_write(hal_gpio_port_t port,
                            hal_gpio_pin_t pin,
                            hal_gpio_level_t level)
{
    GPIO_TypeDef* gpio;

    if (port >= HAL_GPIO_PORT_MAX || pin > 15) {
        return HAL_ERR_PARAM;
    }

    gpio = gpio_ports[port];

    if (level == HAL_GPIO_LEVEL_HIGH) {
        gpio->BSRR = (1UL << pin);
    } else {
        gpio->BSRR = (1UL << (pin + 16));
    }

    return HAL_OK;
}

hal_status_t hal_gpio_read(hal_gpio_port_t port,
                           hal_gpio_pin_t pin,
                           hal_gpio_level_t* level)
{
    GPIO_TypeDef* gpio;

    if (port >= HAL_GPIO_PORT_MAX || pin > 15 || level == NULL) {
        return HAL_ERR_PARAM;
    }

    gpio = gpio_ports[port];
    *level = (gpio->IDR & (1UL << pin)) ? HAL_GPIO_LEVEL_HIGH : HAL_GPIO_LEVEL_LOW;

    return HAL_OK;
}

hal_status_t hal_gpio_toggle(hal_gpio_port_t port, hal_gpio_pin_t pin)
{
    GPIO_TypeDef* gpio;

    if (port >= HAL_GPIO_PORT_MAX || pin > 15) {
        return HAL_ERR_PARAM;
    }

    gpio = gpio_ports[port];
    gpio->ODR ^= (1UL << pin);

    return HAL_OK;
}

hal_status_t hal_gpio_irq_config(hal_gpio_port_t port,
                                 hal_gpio_pin_t pin,
                                 hal_gpio_irq_mode_t mode,
                                 hal_gpio_irq_callback_t callback,
                                 void* context)
{
    /* TODO: Implement EXTI configuration */
    (void)port;
    (void)pin;
    (void)mode;
    (void)callback;
    (void)context;
    return HAL_ERR_NOT_SUPPORTED;
}

hal_status_t hal_gpio_irq_enable(hal_gpio_port_t port, hal_gpio_pin_t pin)
{
    /* TODO: Implement EXTI enable */
    (void)port;
    (void)pin;
    return HAL_ERR_NOT_SUPPORTED;
}

hal_status_t hal_gpio_irq_disable(hal_gpio_port_t port, hal_gpio_pin_t pin)
{
    /* TODO: Implement EXTI disable */
    (void)port;
    (void)pin;
    return HAL_ERR_NOT_SUPPORTED;
}
