/**
 * \file            instance.h
 * \brief           Private STM32F103 TIM3 quadrature bindings.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_ENCODER_INSTANCE_H
#define NEXUS_STM32F1_ENCODER_INSTANCE_H

#include "drivers/gpio/instance.h"
#include "nexus/io/encoder.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Immutable filters and independently selected phase polarities. */
typedef struct {
    uint8_t filter_a;
    uint8_t filter_b;
    bool invert_a;
    bool invert_b;
} nx_stm32f1_encoder_config_t;

/** \brief One static owner and recoverable GPIO and clock receipts. */
typedef struct {
    const nx_stm32f1_encoder_config_t* config;
    nx_stm32f1_pads_t pads;
    uint8_t effects;
} nx_stm32f1_encoder_t;

nx_result_t
nx_stm32f1_encoder_prepare(nx_stm32f1_encoder_t* state,
                           const nx_stm32f1_encoder_config_t* config);
nx_result_t nx_stm32f1_encoder_close(nx_stm32f1_encoder_t* state);
extern const nx_encoder_ops_t nx_stm32f1_encoder_ops;

#ifdef __cplusplus
}
#endif
#endif
