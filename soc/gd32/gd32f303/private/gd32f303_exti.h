/**
 * \file            gd32f303_exti.h
 * \brief           Exact independent PA0/PA1 EXTI private ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_GD32F303_EXTI_H
#define NX_GD32F303_EXTI_H
#include "nexus/io/exti.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Legacy private layouts verified against exact official registers. */
typedef struct {
    volatile uint32_t IMR, EMR, RTSR, FTSR, SWIER, PR;
} nx_gd32f303_exti_registers_t;
typedef struct {
    volatile uint32_t EVCR, MAPR, EXTICR[4];
} nx_gd32f303_exti_mux_t;
typedef struct {
    volatile uint32_t CRL, CRH, IDR, ODR, BSRR, BRR, LCKR;
} nx_gd32f303_exti_gpio_t;
/** \brief One exact line and borrowed event ring; no hidden owner or task. */
typedef struct {
    nx_gd32f303_exti_registers_t* registers;
    nx_gd32f303_exti_mux_t* mux;
    nx_gd32f303_exti_gpio_t* gpio;
    nx_exti_event_t* storage;
    const nx_irq_wake_t* wake;
    size_t capacity, head, tail, count;
    nx_irq_policy_t irq_policy;
    nx_exti_edge_t edge;
    uint16_t wake_publishers;
    uint8_t line, priority, saved_mux;
    bool initialized, retained, closing, loss;
} nx_gd32f303_exti_state_t;
#define NX_GD303_EXTI      ((nx_gd32f303_exti_registers_t*)(uintptr_t)EXTI)
#define NX_GD303_EXTI_MUX  ((nx_gd32f303_exti_mux_t*)(uintptr_t)AFIO)
#define NX_GD303_EXTI_GPIO ((nx_gd32f303_exti_gpio_t*)(uintptr_t)GPIOA)
extern const nx_exti_ops_t nx_gd32f303_exti_ops;
/** \brief Acquire one source under Platform's GPIO/AFIO lease.
 * \param[in,out] port: Privileged unmasked task, stable bounded event ring.
 * \return SUCCESS or error; retained requires successful ordinary stop.
 * \note Only exact PA0/PA1 raw events; bias and debounce are external.
 */
nx_result_t nx_gd32f303_exti_initialize(nx_gd32f303_exti_state_t* port);
/** \brief Ack once and latch at most one coalesced timestamped fact.
 * \param[in,out] port: The live static selected vector instance.
 * \note Pending can reassert; no polling and no user owner callbacks.
 */
void nx_gd32f303_exti_irq(nx_gd32f303_exti_state_t* port);
#ifdef __cplusplus
}
#endif
#endif
