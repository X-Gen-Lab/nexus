/**
 * \file            nx_gpio_stm32f4.c
 * \brief           STM32F4 GPIO driver implementation
 * \author          Nexus Team
 */

#include "hal/base/nx_device.h"
#include "hal/interface/nx_gpio.h"
#include "hal/resource/nx_isr_manager.h"
#include <stddef.h>
#include <string.h>

/* Maximum number of GPIO pins per port */
#define NX_GPIO_PINS_PER_PORT 16

/* Maximum number of GPIO ports */
#define NX_GPIO_MAX_PORTS 9 /* GPIOA-GPIOI */

/**
 * \brief           GPIO pin state structure (internal)
 */
typedef struct {
    uint8_t port;                    /**< Port number (0=A, 1=B, ...) */
    uint8_t pin;                     /**< Pin number (0-15) */
    uint8_t state;                   /**< Current pin state */
    nx_gpio_config_t config;         /**< Current configuration */
    nx_gpio_exti_callback_t exti_cb; /**< EXTI callback */
    void* exti_ctx;                  /**< EXTI callback context */
    nx_isr_handle_t* isr_handle;     /**< ISR manager handle */
    bool initialized;                /**< Initialization flag */
} nx_gpio_pin_state_t;

/**
 * \brief           GPIO device implementation structure
 */
typedef struct {
    nx_gpio_t base;                 /**< Base GPIO interface */
    nx_lifecycle_t lifecycle;       /**< Lifecycle interface */
    nx_power_t power;               /**< Power interface */
    nx_gpio_pin_state_t* pin_state; /**< Pin state pointer */
    nx_device_t* device;            /**< Device descriptor */
} nx_gpio_impl_t;

/* Forward declarations - GPIO operations */
static uint8_t gpio_read(nx_gpio_t* self);
static void gpio_write(nx_gpio_t* self, uint8_t state);
static void gpio_toggle(nx_gpio_t* self);
static nx_status_t gpio_set_mode(nx_gpio_t* self, nx_gpio_mode_t mode);
static nx_status_t gpio_set_pull(nx_gpio_t* self, nx_gpio_pull_t pull);
static nx_status_t gpio_get_config(nx_gpio_t* self, nx_gpio_config_t* cfg);
static nx_status_t gpio_set_config(nx_gpio_t* self,
                                   const nx_gpio_config_t* cfg);
static nx_status_t gpio_set_exti(nx_gpio_t* self, nx_gpio_exti_trig_t trig,
                                 nx_gpio_exti_callback_t cb, void* ctx);
static nx_status_t gpio_clear_exti(nx_gpio_t* self);
static nx_lifecycle_t* gpio_get_lifecycle(nx_gpio_t* self);
static nx_power_t* gpio_get_power(nx_gpio_t* self);

/* Forward declarations - Lifecycle operations */
static nx_status_t gpio_lifecycle_init(nx_lifecycle_t* self);
static nx_status_t gpio_lifecycle_deinit(nx_lifecycle_t* self);
static nx_status_t gpio_lifecycle_suspend(nx_lifecycle_t* self);
static nx_status_t gpio_lifecycle_resume(nx_lifecycle_t* self);
static nx_device_state_t gpio_lifecycle_get_state(nx_lifecycle_t* self);

/* Forward declarations - Power operations */
static nx_status_t gpio_power_enable(nx_power_t* self);
static nx_status_t gpio_power_disable(nx_power_t* self);
static bool gpio_power_is_enabled(nx_power_t* self);

/* GPIO pin state storage */
static nx_gpio_pin_state_t g_gpio_pin_states[NX_GPIO_MAX_PORTS]
                                            [NX_GPIO_PINS_PER_PORT];

/* GPIO implementation instances */
static nx_gpio_impl_t g_gpio_instances[NX_GPIO_MAX_PORTS]
                                      [NX_GPIO_PINS_PER_PORT];

/**
 * \brief           Get GPIO implementation from base interface
 */
static nx_gpio_impl_t* gpio_get_impl(nx_gpio_t* self) {
    if (!self) {
        return NULL;
    }
    return (nx_gpio_impl_t*)self;
}

/**
 * \brief           Get GPIO implementation from lifecycle interface
 */
static nx_gpio_impl_t* gpio_get_impl_from_lifecycle(nx_lifecycle_t* self) {
    if (!self) {
        return NULL;
    }
    return (nx_gpio_impl_t*)((char*)self - offsetof(nx_gpio_impl_t, lifecycle));
}

/**
 * \brief           Get GPIO implementation from power interface
 */
static nx_gpio_impl_t* gpio_get_impl_from_power(nx_power_t* self) {
    if (!self) {
        return NULL;
    }
    return (nx_gpio_impl_t*)((char*)self - offsetof(nx_gpio_impl_t, power));
}

/**
 * \brief           Hardware-specific: Configure GPIO pin mode
 */
static void hw_gpio_set_mode(uint8_t port, uint8_t pin, nx_gpio_mode_t mode) {
    /* TODO: Implement actual STM32F4 GPIO mode configuration */
    /* This would configure MODER register */
    (void)port;
    (void)pin;
    (void)mode;
}

/**
 * \brief           Hardware-specific: Configure GPIO pull-up/pull-down
 */
static void hw_gpio_set_pull(uint8_t port, uint8_t pin, nx_gpio_pull_t pull) {
    /* TODO: Implement actual STM32F4 GPIO pull configuration */
    /* This would configure PUPDR register */
    (void)port;
    (void)pin;
    (void)pull;
}

/**
 * \brief           Hardware-specific: Configure GPIO speed
 */
static void hw_gpio_set_speed(uint8_t port, uint8_t pin,
                              nx_gpio_speed_t speed) {
    /* TODO: Implement actual STM32F4 GPIO speed configuration */
    /* This would configure OSPEEDR register */
    (void)port;
    (void)pin;
    (void)speed;
}

/**
 * \brief           Hardware-specific: Configure GPIO alternate function
 */
static void hw_gpio_set_af(uint8_t port, uint8_t pin, uint8_t af_index) {
    /* TODO: Implement actual STM32F4 GPIO AF configuration */
    /* This would configure AFR register */
    (void)port;
    (void)pin;
    (void)af_index;
}

/**
 * \brief           Hardware-specific: Read GPIO pin state
 */
static uint8_t hw_gpio_read(uint8_t port, uint8_t pin) {
    /* TODO: Implement actual STM32F4 GPIO read */
    /* This would read IDR register */
    (void)port;
    (void)pin;
    return 0;
}

/**
 * \brief           Hardware-specific: Write GPIO pin state
 */
static void hw_gpio_write(uint8_t port, uint8_t pin, uint8_t state) {
    /* TODO: Implement actual STM32F4 GPIO write */
    /* This would write to BSRR register */
    (void)port;
    (void)pin;
    (void)state;
}

/**
 * \brief           Hardware-specific: Enable GPIO port clock
 */
static void hw_gpio_enable_clock(uint8_t port) {
    /* TODO: Implement actual STM32F4 GPIO clock enable */
    /* This would enable RCC clock for the port */
    (void)port;
}

/**
 * \brief           Hardware-specific: Disable GPIO port clock
 */
static void hw_gpio_disable_clock(uint8_t port) {
    /* TODO: Implement actual STM32F4 GPIO clock disable */
    /* This would disable RCC clock for the port */
    (void)port;
}

/**
 * \brief           Hardware-specific: Configure EXTI line
 */
static nx_status_t hw_gpio_config_exti(uint8_t port, uint8_t pin,
                                       nx_gpio_exti_trig_t trig) {
    /* TODO: Implement actual STM32F4 EXTI configuration */
    /* This would configure SYSCFG_EXTICR, EXTI_IMR, EXTI_RTSR, EXTI_FTSR */
    (void)port;
    (void)pin;
    (void)trig;
    return NX_OK;
}

/**
 * \brief           Hardware-specific: Clear EXTI line
 */
static nx_status_t hw_gpio_clear_exti(uint8_t port, uint8_t pin) {
    /* TODO: Implement actual STM32F4 EXTI clear */
    /* This would clear EXTI_IMR */
    (void)port;
    (void)pin;
    return NX_OK;
}

/**
 * \brief           EXTI ISR callback wrapper
 */
static void gpio_exti_isr_callback(void* data) {
    nx_gpio_pin_state_t* pin_state = (nx_gpio_pin_state_t*)data;

    if (pin_state && pin_state->exti_cb) {
        pin_state->exti_cb(pin_state->exti_ctx);
    }
}

/**
 * \brief           Read GPIO pin state
 */
static uint8_t gpio_read(nx_gpio_t* self) {
    nx_gpio_impl_t* impl = gpio_get_impl(self);

    if (!impl || !impl->pin_state || !impl->pin_state->initialized) {
        return 0;
    }

    impl->pin_state->state =
        hw_gpio_read(impl->pin_state->port, impl->pin_state->pin);
    return impl->pin_state->state;
}

/**
 * \brief           Write GPIO pin state
 */
static void gpio_write(nx_gpio_t* self, uint8_t state) {
    nx_gpio_impl_t* impl = gpio_get_impl(self);

    if (!impl || !impl->pin_state || !impl->pin_state->initialized) {
        return;
    }

    hw_gpio_write(impl->pin_state->port, impl->pin_state->pin, state);
    impl->pin_state->state = state;
}

/**
 * \brief           Toggle GPIO pin state
 */
static void gpio_toggle(nx_gpio_t* self) {
    nx_gpio_impl_t* impl = gpio_get_impl(self);

    if (!impl || !impl->pin_state || !impl->pin_state->initialized) {
        return;
    }

    uint8_t current = hw_gpio_read(impl->pin_state->port, impl->pin_state->pin);
    uint8_t new_state = current ? 0 : 1;
    hw_gpio_write(impl->pin_state->port, impl->pin_state->pin, new_state);
    impl->pin_state->state = new_state;
}

/**
 * \brief           Set GPIO pin mode
 */
static nx_status_t gpio_set_mode(nx_gpio_t* self, nx_gpio_mode_t mode) {
    nx_gpio_impl_t* impl = gpio_get_impl(self);

    if (!impl || !impl->pin_state) {
        return NX_ERR_NULL_PTR;
    }

    if (!impl->pin_state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    hw_gpio_set_mode(impl->pin_state->port, impl->pin_state->pin, mode);
    impl->pin_state->config.mode = mode;

    return NX_OK;
}

/**
 * \brief           Set GPIO pin pull-up/pull-down
 */
static nx_status_t gpio_set_pull(nx_gpio_t* self, nx_gpio_pull_t pull) {
    nx_gpio_impl_t* impl = gpio_get_impl(self);

    if (!impl || !impl->pin_state) {
        return NX_ERR_NULL_PTR;
    }

    if (!impl->pin_state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    hw_gpio_set_pull(impl->pin_state->port, impl->pin_state->pin, pull);
    impl->pin_state->config.pull = pull;

    return NX_OK;
}

/**
 * \brief           Get GPIO pin configuration
 */
static nx_status_t gpio_get_config(nx_gpio_t* self, nx_gpio_config_t* cfg) {
    nx_gpio_impl_t* impl = gpio_get_impl(self);

    if (!impl || !impl->pin_state || !cfg) {
        return NX_ERR_NULL_PTR;
    }

    if (!impl->pin_state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    memcpy(cfg, &impl->pin_state->config, sizeof(nx_gpio_config_t));

    return NX_OK;
}

/**
 * \brief           Set GPIO pin configuration
 */
static nx_status_t gpio_set_config(nx_gpio_t* self,
                                   const nx_gpio_config_t* cfg) {
    nx_gpio_impl_t* impl = gpio_get_impl(self);

    if (!impl || !impl->pin_state || !cfg) {
        return NX_ERR_NULL_PTR;
    }

    if (!impl->pin_state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Apply configuration */
    hw_gpio_set_mode(impl->pin_state->port, impl->pin_state->pin, cfg->mode);
    hw_gpio_set_pull(impl->pin_state->port, impl->pin_state->pin, cfg->pull);
    hw_gpio_set_speed(impl->pin_state->port, impl->pin_state->pin, cfg->speed);

    if (cfg->mode == NX_GPIO_MODE_AF_PP || cfg->mode == NX_GPIO_MODE_AF_OD) {
        hw_gpio_set_af(impl->pin_state->port, impl->pin_state->pin,
                       cfg->af_index);
    }

    memcpy(&impl->pin_state->config, cfg, sizeof(nx_gpio_config_t));

    return NX_OK;
}

/**
 * \brief           Configure GPIO external interrupt
 */
static nx_status_t gpio_set_exti(nx_gpio_t* self, nx_gpio_exti_trig_t trig,
                                 nx_gpio_exti_callback_t cb, void* ctx) {
    nx_gpio_impl_t* impl = gpio_get_impl(self);

    if (!impl || !impl->pin_state) {
        return NX_ERR_NULL_PTR;
    }

    if (!impl->pin_state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (trig == NX_GPIO_EXTI_NONE) {
        return gpio_clear_exti(self);
    }

    if (!cb) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Clear existing EXTI if any */
    if (impl->pin_state->isr_handle) {
        gpio_clear_exti(self);
    }

    /* Configure hardware EXTI */
    nx_status_t status =
        hw_gpio_config_exti(impl->pin_state->port, impl->pin_state->pin, trig);
    if (status != NX_OK) {
        return status;
    }

    /* Register ISR callback */
    nx_isr_manager_t* isr_mgr = nx_isr_manager_get();
    if (!isr_mgr) {
        return NX_ERR_NO_RESOURCE;
    }

    /* Calculate EXTI IRQ number (simplified - actual mapping is more complex)
     */
    uint32_t exti_irq = impl->pin_state->pin;

    impl->pin_state->isr_handle =
        isr_mgr->connect(isr_mgr, exti_irq, gpio_exti_isr_callback,
                         impl->pin_state, NX_ISR_PRIORITY_NORMAL);

    if (!impl->pin_state->isr_handle) {
        hw_gpio_clear_exti(impl->pin_state->port, impl->pin_state->pin);
        return NX_ERR_NO_RESOURCE;
    }

    /* Enable IRQ */
    isr_mgr->enable(isr_mgr, exti_irq);

    /* Store callback */
    impl->pin_state->exti_cb = cb;
    impl->pin_state->exti_ctx = ctx;

    return NX_OK;
}

/**
 * \brief           Clear GPIO external interrupt
 */
static nx_status_t gpio_clear_exti(nx_gpio_t* self) {
    nx_gpio_impl_t* impl = gpio_get_impl(self);

    if (!impl || !impl->pin_state) {
        return NX_ERR_NULL_PTR;
    }

    if (!impl->pin_state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Disconnect ISR callback */
    if (impl->pin_state->isr_handle) {
        nx_isr_manager_t* isr_mgr = nx_isr_manager_get();
        if (isr_mgr) {
            uint32_t exti_irq = impl->pin_state->pin;
            isr_mgr->disable(isr_mgr, exti_irq);
            isr_mgr->disconnect(isr_mgr, impl->pin_state->isr_handle);
        }
        impl->pin_state->isr_handle = NULL;
    }

    /* Clear hardware EXTI */
    hw_gpio_clear_exti(impl->pin_state->port, impl->pin_state->pin);

    /* Clear callback */
    impl->pin_state->exti_cb = NULL;
    impl->pin_state->exti_ctx = NULL;

    return NX_OK;
}

/**
 * \brief           Get lifecycle interface
 */
static nx_lifecycle_t* gpio_get_lifecycle(nx_gpio_t* self) {
    nx_gpio_impl_t* impl = gpio_get_impl(self);
    return impl ? &impl->lifecycle : NULL;
}

/**
 * \brief           Get power interface
 */
static nx_power_t* gpio_get_power(nx_gpio_t* self) {
    nx_gpio_impl_t* impl = gpio_get_impl(self);
    return impl ? &impl->power : NULL;
}

/**
 * \brief           Initialize GPIO pin
 */
static nx_status_t gpio_lifecycle_init(nx_lifecycle_t* self) {
    nx_gpio_impl_t* impl = gpio_get_impl_from_lifecycle(self);

    if (!impl || !impl->pin_state) {
        return NX_ERR_NULL_PTR;
    }

    if (impl->pin_state->initialized) {
        return NX_ERR_ALREADY_INIT;
    }

    /* Enable GPIO port clock */
    hw_gpio_enable_clock(impl->pin_state->port);

    /* Apply default configuration */
    hw_gpio_set_mode(impl->pin_state->port, impl->pin_state->pin,
                     impl->pin_state->config.mode);
    hw_gpio_set_pull(impl->pin_state->port, impl->pin_state->pin,
                     impl->pin_state->config.pull);
    hw_gpio_set_speed(impl->pin_state->port, impl->pin_state->pin,
                      impl->pin_state->config.speed);

    impl->pin_state->initialized = true;

    return NX_OK;
}

/**
 * \brief           Deinitialize GPIO pin
 */
static nx_status_t gpio_lifecycle_deinit(nx_lifecycle_t* self) {
    nx_gpio_impl_t* impl = gpio_get_impl_from_lifecycle(self);

    if (!impl || !impl->pin_state) {
        return NX_ERR_NULL_PTR;
    }

    if (!impl->pin_state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Clear EXTI if configured */
    if (impl->pin_state->isr_handle) {
        gpio_clear_exti(&impl->base);
    }

    /* Reset pin to input mode */
    hw_gpio_set_mode(impl->pin_state->port, impl->pin_state->pin,
                     NX_GPIO_MODE_INPUT);
    hw_gpio_set_pull(impl->pin_state->port, impl->pin_state->pin,
                     NX_GPIO_PULL_NONE);

    impl->pin_state->initialized = false;

    return NX_OK;
}

/**
 * \brief           Suspend GPIO pin
 */
static nx_status_t gpio_lifecycle_suspend(nx_lifecycle_t* self) {
    nx_gpio_impl_t* impl = gpio_get_impl_from_lifecycle(self);

    if (!impl || !impl->pin_state) {
        return NX_ERR_NULL_PTR;
    }

    if (!impl->pin_state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Configuration is already saved in pin_state->config */
    /* Disable clock to save power */
    hw_gpio_disable_clock(impl->pin_state->port);

    return NX_OK;
}

/**
 * \brief           Resume GPIO pin
 */
static nx_status_t gpio_lifecycle_resume(nx_lifecycle_t* self) {
    nx_gpio_impl_t* impl = gpio_get_impl_from_lifecycle(self);

    if (!impl || !impl->pin_state) {
        return NX_ERR_NULL_PTR;
    }

    if (!impl->pin_state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Re-enable clock */
    hw_gpio_enable_clock(impl->pin_state->port);

    /* Restore configuration */
    hw_gpio_set_mode(impl->pin_state->port, impl->pin_state->pin,
                     impl->pin_state->config.mode);
    hw_gpio_set_pull(impl->pin_state->port, impl->pin_state->pin,
                     impl->pin_state->config.pull);
    hw_gpio_set_speed(impl->pin_state->port, impl->pin_state->pin,
                      impl->pin_state->config.speed);

    return NX_OK;
}

/**
 * \brief           Get GPIO pin state
 */
static nx_device_state_t gpio_lifecycle_get_state(nx_lifecycle_t* self) {
    nx_gpio_impl_t* impl = gpio_get_impl_from_lifecycle(self);

    if (!impl || !impl->pin_state) {
        return NX_DEV_STATE_ERROR;
    }

    if (!impl->pin_state->initialized) {
        return NX_DEV_STATE_UNINITIALIZED;
    }

    return NX_DEV_STATE_RUNNING;
}

/**
 * \brief           Enable GPIO power
 */
static nx_status_t gpio_power_enable(nx_power_t* self) {
    nx_gpio_impl_t* impl = gpio_get_impl_from_power(self);

    if (!impl || !impl->pin_state) {
        return NX_ERR_NULL_PTR;
    }

    hw_gpio_enable_clock(impl->pin_state->port);

    return NX_OK;
}

/**
 * \brief           Disable GPIO power
 */
static nx_status_t gpio_power_disable(nx_power_t* self) {
    nx_gpio_impl_t* impl = gpio_get_impl_from_power(self);

    if (!impl || !impl->pin_state) {
        return NX_ERR_NULL_PTR;
    }

    hw_gpio_disable_clock(impl->pin_state->port);

    return NX_OK;
}

/**
 * \brief           Check if GPIO power is enabled
 */
static bool gpio_power_is_enabled(nx_power_t* self) {
    nx_gpio_impl_t* impl = gpio_get_impl_from_power(self);

    if (!impl || !impl->pin_state) {
        return false;
    }

    /* TODO: Check actual clock enable status */
    return impl->pin_state->initialized;
}

/**
 * \brief           Initialize GPIO instance
 */
static void gpio_init_instance(nx_gpio_impl_t* impl, uint8_t port,
                               uint8_t pin) {
    /* Initialize base interface */
    impl->base.read = gpio_read;
    impl->base.write = gpio_write;
    impl->base.toggle = gpio_toggle;
    impl->base.set_mode = gpio_set_mode;
    impl->base.set_pull = gpio_set_pull;
    impl->base.get_config = gpio_get_config;
    impl->base.set_config = gpio_set_config;
    impl->base.set_exti = gpio_set_exti;
    impl->base.clear_exti = gpio_clear_exti;
    impl->base.get_lifecycle = gpio_get_lifecycle;
    impl->base.get_power = gpio_get_power;

    /* Initialize lifecycle interface */
    impl->lifecycle.init = gpio_lifecycle_init;
    impl->lifecycle.deinit = gpio_lifecycle_deinit;
    impl->lifecycle.suspend = gpio_lifecycle_suspend;
    impl->lifecycle.resume = gpio_lifecycle_resume;
    impl->lifecycle.get_state = gpio_lifecycle_get_state;

    /* Initialize power interface */
    impl->power.enable = gpio_power_enable;
    impl->power.disable = gpio_power_disable;
    impl->power.is_enabled = gpio_power_is_enabled;

    /* Link to pin state */
    impl->pin_state = &g_gpio_pin_states[port][pin];
    impl->pin_state->port = port;
    impl->pin_state->pin = pin;
    impl->pin_state->state = 0;
    impl->pin_state->initialized = false;
    impl->pin_state->exti_cb = NULL;
    impl->pin_state->exti_ctx = NULL;
    impl->pin_state->isr_handle = NULL;

    /* Set default configuration */
    impl->pin_state->config.mode = NX_GPIO_MODE_INPUT;
    impl->pin_state->config.pull = NX_GPIO_PULL_NONE;
    impl->pin_state->config.speed = NX_GPIO_SPEED_LOW;
    impl->pin_state->config.af_index = 0;
}

/**
 * \brief           Get GPIO instance (factory function)
 * \param[in]       port: GPIO port number (0=A, 1=B, ...)
 * \param[in]       pin: GPIO pin number (0-15)
 * \return          GPIO interface pointer, NULL on failure
 */
nx_gpio_t* nx_gpio_stm32f4_get(uint8_t port, uint8_t pin) {
    if (port >= NX_GPIO_MAX_PORTS || pin >= NX_GPIO_PINS_PER_PORT) {
        return NULL;
    }

    nx_gpio_impl_t* impl = &g_gpio_instances[port][pin];

    /* Initialize instance if not already done */
    if (!impl->pin_state) {
        gpio_init_instance(impl, port, pin);
    }

    return &impl->base;
}

/**
 * \brief           Get GPIO instance with configuration
 * \param[in]       port: GPIO port number (0=A, 1=B, ...)
 * \param[in]       pin: GPIO pin number (0-15)
 * \param[in]       cfg: GPIO configuration
 * \return          GPIO interface pointer, NULL on failure
 */
nx_gpio_t* nx_gpio_stm32f4_get_with_config(uint8_t port, uint8_t pin,
                                           const nx_gpio_config_t* cfg) {
    nx_gpio_t* gpio = nx_gpio_stm32f4_get(port, pin);

    if (!gpio || !cfg) {
        return NULL;
    }

    /* Apply configuration */
    nx_gpio_impl_t* impl = gpio_get_impl(gpio);
    if (impl && impl->pin_state) {
        memcpy(&impl->pin_state->config, cfg, sizeof(nx_gpio_config_t));
    }

    return gpio;
}

/**
 * \brief           Get GPIO device descriptor
 * \param[in]       port: GPIO port number
 * \param[in]       pin: GPIO pin number
 * \return          Device descriptor pointer, NULL on failure
 */
nx_device_t* nx_gpio_stm32f4_get_device(uint8_t port, uint8_t pin) {
    if (port >= NX_GPIO_MAX_PORTS || pin >= NX_GPIO_PINS_PER_PORT) {
        return NULL;
    }

    /* GPIO uses per-pin instances, return the device pointer from impl */
    nx_gpio_impl_t* impl = &g_gpio_instances[port][pin];
    return impl->device;
}
