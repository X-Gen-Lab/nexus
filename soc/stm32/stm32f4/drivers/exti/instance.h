/**
 * \file            instance.h
 * \brief           Private STM32F4 EXTI line and bounded event-ring methods
 * \author          Nexus Team
 */
#ifndef NX_STM32F4_EXTI_INSTANCE_H
#define NX_STM32F4_EXTI_INSTANCE_H

#include "nexus/io/exti.h"
#include "nexus/io/wake.h"
#include "private/model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    EXTI_TypeDef* registers;
    const nx_irq_wake_t* wake;
    GPIO_TypeDef* gpio;
    nx_exti_event_t* storage;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
    uint32_t loss;
    uint8_t line;
    nx_exti_edge_t edge;
    bool initialized;
    bool cold_owned;
} nx_stm32_exti_state_t;

/** \brief Initialize one statically allocated EXTI line and event ring. */
nx_result_t nx_stm32_exti_initialize(nx_stm32_exti_state_t* port,
                                     SYSCFG_TypeDef* mux, uint8_t gpio_index);
/** \brief Bounded IRQ delivery for a statically assembled line array. */
void nx_stm32_exti_dispatch(nx_stm32_exti_state_t* const* ports, size_t count,
                            uint32_t vector_mask);
extern const nx_exti_ops_t nx_stm32_exti_ops;
nx_result_t nx_stm32_exti_read(void* context, nx_exti_event_t* events,
                               size_t capacity, size_t* count);
nx_result_t nx_stm32_exti_stop(void* context);
nx_result_t nx_stm32_exti_attach_wake(void* context, const nx_irq_wake_t* wake,
                                      uint8_t syscall_ceiling);

#ifdef __cplusplus
}
#endif
#endif /* NX_STM32F4_EXTI_INSTANCE_H */
