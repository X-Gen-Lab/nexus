/**
 * \file            stm32f407_dac.h
 * \brief           Private F407 DAC selection and resource responsibility
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_DAC_H
#define NEXUS_STM32F407_DAC_H
#include "nexus/io/dac.h"
#include "stm32f407_system.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Complete selected behavior, never runtime provider discovery. */
typedef enum {
    NX_STM32_DAC_DIRECT,
    NX_STM32_DAC_TRIGGERED,
    NX_STM32_DAC_NOISE,
    NX_STM32_DAC_TRIANGLE
} nx_stm32_dac_mode_t;
/** \brief Only explicitly leased software or basic-timer sources are allowed.
 */
typedef enum {
    NX_STM32_DAC_TRIGGER_NONE,
    NX_STM32_DAC_TRIGGER_SOFTWARE,
    NX_STM32_DAC_TRIGGER_TIM6,
    NX_STM32_DAC_TRIGGER_TIM7
} nx_stm32_dac_trigger_t;
/** \brief ROM settings; codes are raw input units, not pre-shifted storage. */
typedef struct {
    DAC_TypeDef* registers;
    nx_dac_format_t format;
    nx_stm32_dac_mode_t mode;
    nx_stm32_dac_trigger_t trigger;
    uint16_t idle_codes[2];
    uint16_t amplitude;
    uint8_t channels;
    bool output_buffer;
} nx_stm32_dac_config_t;
/** \brief CPU/wave modes keep no DMA storage, request, worker or queue. */
typedef struct {
    const nx_stm32_dac_config_t* config;
    uint32_t clocks_added;
    uint32_t trigger_divisor;
    bool initialized;
    bool running;
    bool owned;
    bool faulted;
} nx_stm32_dac_state_t;
/** \brief Platform has already acquired and recorded the selected GPIO pads. */
nx_result_t nx_stm32_dac_prepare(nx_stm32_dac_state_t* state,
                                 const nx_stm32_dac_config_t* config);
/** \brief Reverse only owned effects; refusal retains the same live receipt. */
nx_result_t nx_stm32_dac_close(nx_stm32_dac_state_t* state);
extern const nx_dac_ops_t nx_stm32_dac_ops;
#ifdef __cplusplus
}
#endif
#endif
