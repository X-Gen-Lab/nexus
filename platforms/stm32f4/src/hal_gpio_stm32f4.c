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

/**
 * \brief           IRQ callback storage for each EXTI line (0-15)
 */
static struct {
    hal_gpio_irq_callback_t callback;
    void* context;
    hal_gpio_port_t port;
} gpio_irq_handlers[16];

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

/**
 * \brief           Get NVIC IRQ number for EXTI line
 * \param[in]       pin: GPIO pin (EXTI line)
 * \return          IRQ number
 */
static IRQn_Type gpio_get_irqn(hal_gpio_pin_t pin)
{
    if (pin <= 4) {
        return (IRQn_Type)(EXTI0_IRQn + pin);
    } else if (pin <= 9) {
        return EXTI9_5_IRQn;
    } else {
        return EXTI15_10_IRQn;
    }
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

    /* Disable interrupt if configured */
    hal_gpio_irq_disable(port, pin);

    /* Reset to input mode, no pull */
    gpio->MODER &= ~(0x03UL << (pin * 2));
    gpio->PUPDR &= ~(0x03UL << (pin * 2));
    gpio->OSPEEDR &= ~(0x03UL << (pin * 2));
    gpio->OTYPER &= ~(0x01UL << pin);

    /* Clear IRQ handler */
    gpio_irq_handlers[pin].callback = NULL;
    gpio_irq_handlers[pin].context = NULL;

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
    uint32_t exticr_idx;
    uint32_t exticr_shift;

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

    /* Enable SYSCFG clock */
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    __asm volatile("dsb");

    /* Configure EXTI line to use this GPIO port */
    exticr_idx = pin / 4;
    exticr_shift = (pin % 4) * 4;
    SYSCFG->EXTICR[exticr_idx] &= ~(0x0FUL << exticr_shift);
    SYSCFG->EXTICR[exticr_idx] |= ((uint32_t)port << exticr_shift);

    /* Configure trigger edge */
    switch (mode) {
        case HAL_GPIO_IRQ_RISING:
            EXTI->RTSR |= (1UL << pin);
            EXTI->FTSR &= ~(1UL << pin);
            break;
        case HAL_GPIO_IRQ_FALLING:
            EXTI->RTSR &= ~(1UL << pin);
            EXTI->FTSR |= (1UL << pin);
            break;
        case HAL_GPIO_IRQ_BOTH:
            EXTI->RTSR |= (1UL << pin);
            EXTI->FTSR |= (1UL << pin);
            break;
        default:
            return HAL_ERR_PARAM;
    }

    /* Store callback */
    gpio_irq_handlers[pin].callback = callback;
    gpio_irq_handlers[pin].context = context;
    gpio_irq_handlers[pin].port = port;

    /* Set NVIC priority and enable */
    NVIC_SetPriority(gpio_get_irqn(pin), 5);

    return HAL_OK;
}

hal_status_t hal_gpio_irq_enable(hal_gpio_port_t port, hal_gpio_pin_t pin)
{
    (void)port;

    if (pin > 15) {
        return HAL_ERR_PARAM;
    }

    /* Clear pending flag */
    EXTI->PR = (1UL << pin);

    /* Enable EXTI interrupt mask */
    EXTI->IMR |= (1UL << pin);

    /* Enable NVIC interrupt */
    NVIC_EnableIRQ(gpio_get_irqn(pin));

    return HAL_OK;
}

hal_status_t hal_gpio_irq_disable(hal_gpio_port_t port, hal_gpio_pin_t pin)
{
    (void)port;

    if (pin > 15) {
        return HAL_ERR_PARAM;
    }

    /* Disable EXTI interrupt mask */
    EXTI->IMR &= ~(1UL << pin);

    /* Clear pending flag */
    EXTI->PR = (1UL << pin);

    return HAL_OK;
}

/*===========================================================================*/
/* IRQ Handlers                                                               */
/*===========================================================================*/

/**
 * \brief           Common EXTI handler
 * \param[in]       pin: EXTI line (pin number)
 */
static void gpio_exti_handler(hal_gpio_pin_t pin)
{
    /* Check if interrupt is pending */
    if (EXTI->PR & (1UL << pin)) {
        /* Clear pending flag */
        EXTI->PR = (1UL << pin);

        /* Call user callback */
        if (gpio_irq_handlers[pin].callback != NULL) {
            gpio_irq_handlers[pin].callback(
                gpio_irq_handlers[pin].port,
                pin,
                gpio_irq_handlers[pin].context
            );
        }
    }
}

/* EXTI0 IRQ Handler */
void EXTI0_IRQHandler(void) { gpio_exti_handler(0); }

/* EXTI1 IRQ Handler */
void EXTI1_IRQHandler(void) { gpio_exti_handler(1); }

/* EXTI2 IRQ Handler */
void EXTI2_IRQHandler(void) { gpio_exti_handler(2); }

/* EXTI3 IRQ Handler */
void EXTI3_IRQHandler(void) { gpio_exti_handler(3); }

/* EXTI4 IRQ Handler */
void EXTI4_IRQHandler(void) { gpio_exti_handler(4); }

/* EXTI9_5 IRQ Handler */
void EXTI9_5_IRQHandler(void)
{
    for (hal_gpio_pin_t pin = 5; pin <= 9; ++pin) {
        gpio_exti_handler(pin);
    }
}

/* EXTI15_10 IRQ Handler */
void EXTI15_10_IRQHandler(void)
{
    for (hal_gpio_pin_t pin = 10; pin <= 15; ++pin) {
        gpio_exti_handler(pin);
    }
}
