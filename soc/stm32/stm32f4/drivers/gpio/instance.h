/**
 * \file            instance.h
 * \brief           Private STM32F4 GPIO state and interface methods
 * \author          Nexus Team
 */
#ifndef NX_STM32F4_GPIO_INSTANCE_H
#define NX_STM32F4_GPIO_INSTANCE_H

#include "nexus/io/gpio.h"
#include "private/model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    GPIO_TypeDef* registers;
    uint32_t mask;
    bool output;
    bool initialized;
} nx_stm32_gpio_state_t;

/** \brief Invalidate a quiescent GPIO binding at its inactive electrical level.
 */
nx_result_t nx_stm32_gpio_stop(nx_stm32_gpio_state_t* port, uint32_t inactive);
/** \brief Initialize authorized pins, preloading BSRR before output mode. */
nx_result_t nx_stm32_gpio_initialize(nx_stm32_gpio_state_t* port,
                                     uint32_t initial);

extern const nx_gpio_ops_t nx_stm32_gpio_ops;
nx_result_t nx_stm32_gpio_write(void* context, uint32_t set_mask,
                                uint32_t reset_mask);
nx_result_t nx_stm32_gpio_read(const void* context, uint32_t* value);
nx_result_t nx_stm32_gpio_toggle(void* context, uint32_t mask);

#ifdef __cplusplus
}
#endif
#endif /* NX_STM32F4_GPIO_INSTANCE_H */
