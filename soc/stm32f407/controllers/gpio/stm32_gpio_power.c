/**
 * \file            stm32_gpio_power.c
 * \brief           STM32 GPIO power management
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-02-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements GPIO power management (suspend/resume)
 */

#include "hal/nx_types.h"
#include "stm32_gpio.h"

/* Logical suspend retains the pin's electrical state. No shared port clock
 * is disabled, and applications cannot treat this as a measured power saving. */
#define DEFINE_POWER(kind, type)                                               \
    static type* kind##_impl(nx_power_t* self) {                               \
        return self ? NX_CONTAINER_OF(self, type, power) : NULL;              \
    }                                                                         \
    static nx_status_t kind##_enable(nx_power_t* self) {                       \
        type* impl = kind##_impl(self);                                       \
        if (!impl || !impl->state) return NX_ERR_INVALID_PARAM;               \
        bool changed = impl->state->suspended;                                \
        nx_status_t r = impl->lifecycle.resume(&impl->lifecycle);             \
        if (r == NX_OK && changed && impl->state->power_callback)              \
            impl->state->power_callback(impl->state->power_context, true);    \
        return r;                                                             \
    }                                                                         \
    static nx_status_t kind##_disable(nx_power_t* self) {                      \
        type* impl = kind##_impl(self);                                       \
        if (!impl || !impl->state) return NX_ERR_INVALID_PARAM;               \
        bool changed = !impl->state->suspended;                               \
        nx_status_t r = impl->lifecycle.suspend(&impl->lifecycle);            \
        if (r == NX_OK && changed && impl->state->power_callback)              \
            impl->state->power_callback(impl->state->power_context, false);   \
        return r;                                                             \
    }                                                                         \
    static bool kind##_enabled(nx_power_t* self) {                            \
        type* impl = kind##_impl(self);                                       \
        return impl && impl->state && impl->state->initialized &&             \
               !impl->state->suspended;                                      \
    }                                                                         \
    static nx_status_t kind##_callback(nx_power_t* self,                       \
                                       nx_power_callback_t fn, void* ctx) {   \
        type* impl = kind##_impl(self);                                       \
        if (!impl || !impl->state) return NX_ERR_INVALID_PARAM;               \
        if (__get_IPSR()) return NX_ERR_INVALID_STATE;                         \
        impl->state->power_callback = fn; impl->state->power_context = ctx;   \
        return NX_OK;                                                         \
    }                                                                         \
    void stm32_gpio_init_power_##kind(nx_power_t* iface) {                     \
        iface->enable = kind##_enable; iface->disable = kind##_disable;       \
        iface->is_enabled = kind##_enabled; iface->set_callback = kind##_callback; \
    }
DEFINE_POWER(read, stm32_gpio_read_impl_t)
DEFINE_POWER(write, stm32_gpio_write_impl_t)
DEFINE_POWER(read_write, stm32_gpio_read_write_impl_t)
