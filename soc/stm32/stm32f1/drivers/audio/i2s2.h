/**
 * \file            i2s2.h
 * \brief           Private F103ZE finite I2S2 master transmitter
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_I2S2_H
#define NEXUS_STM32F1_I2S2_H
#include "drivers/gpio/instance.h"
#include "nexus/io/audio.h"
#include "nexus/io/dma.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const nx_dma_memory_region_t* regions;
    size_t region_count;
    nx_audio_rate_t rate;
    uint8_t divider;
    bool odd;
    uint8_t priority;
} nx_stm32f1_i2s2_config_t;

typedef struct {
    const nx_stm32f1_i2s2_config_t* config;
    nx_stm32f1_pads_t pads;
    nx_audio_request_t* active;
    nx_time_us_t drain_deadline;
    nx_result_t terminal;
    uint16_t count;
    uint16_t remaining;
    uint8_t previous_priority;
    bool gate_added;
    bool spi_gate_added;
    bool spi_owned;
    bool lane_owned;
    bool irq_owned;
    bool ready;
    bool closing;
    bool draining;
    bool enabled_attempted;
    bool wire_quiet;
} nx_stm32f1_i2s2_t;

nx_result_t nx_stm32f1_i2s2_admit(const nx_stm32f1_i2s2_t* state,
                                  const nx_stm32f1_i2s2_config_t* config);
nx_result_t nx_stm32f1_i2s2_prepare(nx_stm32f1_i2s2_t* state,
                                    const nx_stm32f1_i2s2_config_t* config);
nx_result_t nx_stm32f1_i2s2_close(nx_stm32f1_i2s2_t* state);
void nx_stm32f1_i2s2_irq(nx_stm32f1_i2s2_t* state);
extern const nx_audio_ops_t nx_stm32f1_i2s2_ops;
#ifdef __cplusplus
}
#endif
#endif
