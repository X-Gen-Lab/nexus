/**
 * \file            instance.h
 * \brief           Private STM32F1 DAC direct owner and immutable operations.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_DAC_INSTANCE_H
#define NEXUS_STM32F1_DAC_INSTANCE_H

#include "drivers/gpio/instance.h"
#include "nexus/io/dac.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nx_stm32f1_pads_t pads;
    uint16_t idle;
    nx_dac_format_t format;
    uint8_t effects;
    bool buffered;
} nx_stm32f1_dac_t;

nx_result_t nx_stm32f1_dac_prepare(nx_stm32f1_dac_t* state,
                                   nx_dac_format_t format, uint16_t idle,
                                   bool buffered);
nx_result_t nx_stm32f1_dac_close(nx_stm32f1_dac_t* state);
extern const nx_dac_ops_t nx_stm32f1_dac_ops;

#ifdef __cplusplus
}
#endif
#endif
