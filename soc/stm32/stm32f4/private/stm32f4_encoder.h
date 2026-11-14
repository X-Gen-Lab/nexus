/**
 * \file            stm32f4_encoder.h
 * \brief           Private static F407 quadrature counter configuration
 * \author          Nexus Team
 */
#ifndef NX_STM32F4_ENCODER_H
#define NX_STM32F4_ENCODER_H
#include "nexus/io/encoder.h"
#include "stm32f407_provider.h"

/** \brief One resolved input pad; the Board owns its clock and snapshot. */
typedef struct {
    GPIO_TypeDef* registers;
    uint8_t port_index;
    uint8_t pin;
} nx_stm32_encoder_pin_t;

/** \brief Immutable mode and physical TI1/TI2 inputs in logical A/B order. */
typedef struct {
    TIM_TypeDef* registers;
    nx_encoder_mode_t mode;
    uint8_t filter_a;
    uint8_t filter_b;
    bool invert_a;
    bool invert_b;
    nx_stm32_encoder_pin_t pins[2];
} nx_stm32_encoder_config_t;

/** \brief Static descriptor borrow and retained cold effects, without buffers.
 */
typedef struct {
    const nx_stm32_encoder_config_t* config;
    uint8_t cold_effects;
    bool initialized;
} nx_stm32_encoder_state_t;

/** \brief Acquire an exclusive timer in serialized privileged Thread mode.
 * \note Platform already holds the exact GPIO pins. Config outlives close;
 *       partial acquisition retains its precise cleanup receipt.
 */
nx_result_t nx_stm32_encoder_prepare(nx_stm32_encoder_state_t* state,
                                     const nx_stm32_encoder_config_t* config);
/** \brief Prove source withdrawal before releasing the acquired clock.
 * \note Failure retains the descriptor and receipt for the same owner retry.
 */
nx_result_t nx_stm32_encoder_close(nx_stm32_encoder_state_t* state);
extern const nx_encoder_ops_t nx_stm32_encoder_ops;
#endif
