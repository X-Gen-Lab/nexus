/**
 * \file            h743_high_profile_model.h
 * \brief           External H743 peripheral clocks and nominal timer input
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_H743_HIGH_PROFILE_MODEL_H
#define NX_H743_HIGH_PROFILE_MODEL_H
/* Model-selected exact build fact, never a runtime readiness proof. */
#ifndef NEXUS_CORE_HZ
#define NEXUS_CORE_HZ 384000000U
#elif NEXUS_CORE_HZ != 384000000U
#error "The high reference target requires the exact reviewed384 build fact"
#endif
#include "h743_clock_model.h"
#undef GPIOA
#undef GPIOB
#undef GPIOD
#undef GPIOE
#undef EXTI
#define GPIOA (&g_h743_model.gpio_a)
#define GPIOB (&g_h743_model.gpio_b)
#define GPIOD (&g_h743_model.gpio_d)
#define GPIOE (&g_h743_model.gpio_e)
#define EXTI  (&g_h743_model.exti)
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    uint32_t cpu_hz;
    uint32_t hclk_hz;
    uint32_t pclk_hz[4];
    uint32_t tim2_hz;
    uint32_t tim3_hz;
    uint32_t usart3_hz;
    uint32_t i2c1_hz;
    uint32_t spi4_hz;
    uint32_t adc12_input_hz;
    uint32_t adc_hz;
} nx_h743_high_model_clocks_t;
typedef struct {
    uint32_t hsi_input_hz;
    uint32_t pll_scale_ppm;
    uint32_t adc_enables_outside_profile;
    uint32_t unsafe_flash_unlocks;
    uint32_t mux_writes_with_consumers;
    uint32_t timer_active_prescaler;
    uint32_t timer_input_remainder;
    uint32_t refused_kernel_field;
    uint32_t spi_started_divisor;
} nx_h743_high_model_behavior_t;
extern nx_h743_high_model_behavior_t g_h743_high_behavior;
nx_h743_high_model_clocks_t nx_h743_high_model_clocks(void);
void nx_h743_high_model_advance_hclk(uint32_t cycles);
void nx_h743_high_model_write(volatile uint32_t* reg, uint32_t value);
uint8_t nx_h743_high_model_spi_read8(volatile uint32_t* reg);
#ifdef __cplusplus
}
#endif
#undef NX_H743_WRITE
#define NX_H743_WRITE(reg, value) nx_h743_high_model_write((reg), (value))
#undef NX_H743_SPI_READ8
#define NX_H743_SPI_READ8(port) nx_h743_high_model_spi_read8(&(port)->RXDR)
#endif
