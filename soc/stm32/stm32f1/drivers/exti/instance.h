/**
 * \file            instance.h
 * \brief           F103 PC13 EXTI input and retained source ownership.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_EXTI_INSTANCE_H
#define NEXUS_STM32F1_EXTI_INSTANCE_H

#include "drivers/gpio/instance.h"
#include "nexus/io/exti.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Immutable pad identity, edge selection and explicit event buffer. */
typedef struct {
    GPIO_TypeDef* gpio;
    nx_exti_event_t* storage;
    size_t capacity;
    nx_exti_edge_t edge;
    uint8_t gpio_index;
    uint8_t line;
    uint8_t priority;
    uint8_t pull;
} nx_stm32f1_exti_config_t;

/** \brief One line's IRQ producer, consumer cursor and cold receipts. */
typedef struct {
    const nx_stm32f1_exti_config_t* config;
    nx_stm32f1_pads_t pads;
    uint32_t previous_route;
    uint16_t head;
    uint16_t count;
    uint8_t previous_priority;
    bool loss;
    bool live;
    bool mux_owned;
    bool line_owned;
    bool vector_owned;
    bool afio_added;
} nx_stm32f1_exti_t;

nx_result_t nx_stm32f1_exti_admit(const nx_stm32f1_exti_t* state,
                                  const nx_stm32f1_exti_config_t* config);
nx_result_t nx_stm32f1_exti_prepare(nx_stm32f1_exti_t* state,
                                    const nx_stm32f1_exti_config_t* config);
nx_result_t nx_stm32f1_exti_close(nx_stm32f1_exti_t* state);
void nx_stm32f1_exti_irq(nx_stm32f1_exti_t* state);
extern const nx_exti_ops_t nx_stm32f1_exti_ops;

#ifdef __cplusplus
}
#endif
#endif
