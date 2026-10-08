/**
 * \file            stm32_gpio_lifecycle.c
 * \brief           STM32 GPIO lifecycle management
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-02-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements GPIO lifecycle management (init/deinit)
 */

#include "hal/nx_types.h"
#include "stm32_gpio.h"

/* Port clocks are shared by many pins; deinit must not gate the entire port. */
static nx_status_t enable_clock(GPIO_TypeDef* port) {
#if defined(GPIOA) && defined(__HAL_RCC_GPIOA_CLK_ENABLE)
    if (port == GPIOA) { __HAL_RCC_GPIOA_CLK_ENABLE(); return NX_OK; }
#endif
#if defined(GPIOB) && defined(__HAL_RCC_GPIOB_CLK_ENABLE)
    if (port == GPIOB) { __HAL_RCC_GPIOB_CLK_ENABLE(); return NX_OK; }
#endif
#if defined(GPIOC) && defined(__HAL_RCC_GPIOC_CLK_ENABLE)
    if (port == GPIOC) { __HAL_RCC_GPIOC_CLK_ENABLE(); return NX_OK; }
#endif
#if defined(GPIOD) && defined(__HAL_RCC_GPIOD_CLK_ENABLE)
    if (port == GPIOD) { __HAL_RCC_GPIOD_CLK_ENABLE(); return NX_OK; }
#endif
#if defined(GPIOE) && defined(__HAL_RCC_GPIOE_CLK_ENABLE)
    if (port == GPIOE) { __HAL_RCC_GPIOE_CLK_ENABLE(); return NX_OK; }
#endif
#if defined(GPIOF) && defined(__HAL_RCC_GPIOF_CLK_ENABLE)
    if (port == GPIOF) { __HAL_RCC_GPIOF_CLK_ENABLE(); return NX_OK; }
#endif
#if defined(GPIOG) && defined(__HAL_RCC_GPIOG_CLK_ENABLE)
    if (port == GPIOG) { __HAL_RCC_GPIOG_CLK_ENABLE(); return NX_OK; }
#endif
#if defined(GPIOH) && defined(__HAL_RCC_GPIOH_CLK_ENABLE)
    if (port == GPIOH) { __HAL_RCC_GPIOH_CLK_ENABLE(); return NX_OK; }
#endif
#if defined(GPIOI) && defined(__HAL_RCC_GPIOI_CLK_ENABLE)
    if (port == GPIOI) { __HAL_RCC_GPIOI_CLK_ENABLE(); return NX_OK; }
#endif
#if defined(GPIOJ) && defined(__HAL_RCC_GPIOJ_CLK_ENABLE)
    if (port == GPIOJ) { __HAL_RCC_GPIOJ_CLK_ENABLE(); return NX_OK; }
#endif
#if defined(GPIOK) && defined(__HAL_RCC_GPIOK_CLK_ENABLE)
    if (port == GPIOK) { __HAL_RCC_GPIOK_CLK_ENABLE(); return NX_OK; }
#endif
    return NX_ERR_NOT_SUPPORTED;
}

nx_status_t stm32_gpio_hw_init(stm32_gpio_state_t* state) {
    if (!state || !state->config) return NX_ERR_INVALID_PARAM;
    if (__get_IPSR()) return NX_ERR_INVALID_STATE;
    if (state->initialized) return NX_ERR_ALREADY_INIT;
    nx_status_t result = enable_clock(state->config->port);
    if (result != NX_OK) return result;
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = state->config->pin;
    gpio.Mode = state->config->mode;
    gpio.Pull = state->config->pull;
    gpio.Speed = state->config->speed;
    gpio.Alternate = state->config->alternate;
    /* Set inactive/output latch before output mode to avoid a startup pulse. */
    if (gpio.Mode == GPIO_MODE_OUTPUT_PP || gpio.Mode == GPIO_MODE_OUTPUT_OD)
        HAL_GPIO_WritePin(state->config->port, gpio.Pin,
                           state->config->init_value ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_Init(state->config->port, &gpio);
    state->initialized = true;
    state->suspended = false;
    return NX_OK;
}
void stm32_gpio_hw_deinit(stm32_gpio_state_t* state) {
    if (!state || !state->config || !state->initialized) return;
    HAL_GPIO_DeInit(state->config->port, state->config->pin);
    state->initialized = false;
    state->suspended = false;
}

static nx_status_t suspend_state(stm32_gpio_state_t* state) {
    if (!state) return NX_ERR_INVALID_PARAM;
    if (__get_IPSR()) return NX_ERR_INVALID_STATE;
    if (!state->initialized) return NX_ERR_NOT_INIT;
    state->suspended = true; /* Retain electrical output; suppress operations. */
    return NX_OK;
}
static nx_status_t resume_state(stm32_gpio_state_t* state) {
    if (!state) return NX_ERR_INVALID_PARAM;
    if (__get_IPSR()) return NX_ERR_INVALID_STATE;
    if (!state->initialized) return NX_ERR_NOT_INIT;
    state->suspended = false;
    return NX_OK;
}
static nx_device_state_t get_state(stm32_gpio_state_t* state) {
    if (!state) return NX_DEV_STATE_ERROR;
    return !state->initialized ? NX_DEV_STATE_UNINITIALIZED :
           state->suspended ? NX_DEV_STATE_SUSPENDED : NX_DEV_STATE_RUNNING;
}
#define DEFINE_LIFECYCLE(kind, type)                                           \
    static stm32_gpio_state_t* kind##_state(nx_lifecycle_t* self) {             \
        return self ? NX_CONTAINER_OF(self, type, lifecycle)->state : NULL;   \
    }                                                                         \
    static nx_status_t kind##_init(nx_lifecycle_t* self) {                     \
        return stm32_gpio_hw_init(kind##_state(self));                         \
    }                                                                         \
    static nx_status_t kind##_deinit(nx_lifecycle_t* self) {                   \
        if (!kind##_state(self)) return NX_ERR_INVALID_PARAM;                 \
        if (__get_IPSR()) return NX_ERR_INVALID_STATE;                         \
        stm32_gpio_hw_deinit(kind##_state(self));                              \
        return NX_OK;                                                         \
    }                                                                         \
    static nx_status_t kind##_suspend(nx_lifecycle_t* self) {                  \
        return suspend_state(kind##_state(self));                              \
    }                                                                         \
    static nx_status_t kind##_resume(nx_lifecycle_t* self) {                   \
        return resume_state(kind##_state(self));                               \
    }                                                                         \
    static nx_device_state_t kind##_get_state(nx_lifecycle_t* self) {         \
        return get_state(kind##_state(self));                                  \
    }                                                                         \
    void stm32_gpio_init_lifecycle_##kind(nx_lifecycle_t* iface) {             \
        iface->init = kind##_init; iface->deinit = kind##_deinit;              \
        iface->suspend = kind##_suspend; iface->resume = kind##_resume;        \
        iface->get_state = kind##_get_state;                                   \
    }
DEFINE_LIFECYCLE(read, stm32_gpio_read_impl_t)
DEFINE_LIFECYCLE(write, stm32_gpio_write_impl_t)
DEFINE_LIFECYCLE(read_write, stm32_gpio_read_write_impl_t)
