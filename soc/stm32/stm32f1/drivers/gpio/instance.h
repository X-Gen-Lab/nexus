/**
 * \file            instance.h
 * \brief           Private F1 GPIO bindings and cold pad receipts.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_GPIO_INSTANCE_H
#define NEXUS_STM32F1_GPIO_INSTANCE_H

#include "nexus/io/gpio.h"
#include "stm32f1xx.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Saved fields belonging to one authorized hardware pad group. */
typedef struct {
    GPIO_TypeDef* registers;
    uint32_t low;
    uint32_t high;
    uint32_t latch;
    uint32_t gate;
    uint16_t mask;
    bool captured;
    bool gate_added;
} nx_stm32f1_pads_t;

/** \brief Generated immutable selection and board-approved electrical mode. */
typedef struct {
    GPIO_TypeDef* registers;
    uint32_t mask;
    uint32_t initial;
    bool output;
    uint8_t pull;
    uint8_t output_type;
    uint8_t speed;
} nx_stm32f1_gpio_config_t;

/** \brief One static GPIO face's mutable pad ownership. */
typedef struct {
    const nx_stm32f1_gpio_config_t* config;
    nx_stm32f1_pads_t pads;
    bool live;
} nx_stm32f1_gpio_t;

nx_result_t nx_stm32f1_pads_capture(nx_stm32f1_pads_t* pads,
                                    GPIO_TypeDef* registers, uint16_t mask);
nx_result_t nx_stm32f1_pads_apply(const nx_stm32f1_pads_t* pads, uint32_t low,
                                  uint32_t high, uint32_t latch);
nx_result_t nx_stm32f1_pads_restore(nx_stm32f1_pads_t* pads);
nx_result_t nx_stm32f1_gpio_admit(const nx_stm32f1_gpio_t* state,
                                  const nx_stm32f1_gpio_config_t* config);
nx_result_t nx_stm32f1_gpio_prepare(nx_stm32f1_gpio_t* state,
                                    const nx_stm32f1_gpio_config_t* config);
nx_result_t nx_stm32f1_gpio_close(nx_stm32f1_gpio_t* state);
extern const nx_gpio_ops_t nx_stm32f1_gpio_ops;

#ifdef __cplusplus
}
#endif
#endif
