/**
 * \file            f407_i2s2_slave_tx_model.h
 * \brief           External register leaves for the finite F407 I2S2 owner
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_F407_I2S2_SLAVE_TX_MODEL_H
#define NEXUS_F407_I2S2_SLAVE_TX_MODEL_H

#include "nexus_config.h"
#include "stm32f407xx.h"
#include <stdbool.h>
#include <stdint.h>

/* The forced-include model first consumes the generated host configuration,
 * then selects this exact reviewed hardware role for the real System source.
 * This overrides no production leaf and avoids command-line redefinitions. */
#undef NEXUS_F407_I2S2_SLAVE_TX_SELECTED
#define NEXUS_F407_I2S2_SLAVE_TX_SELECTED 1u

#ifdef __cplusplus
extern "C" {
#endif
extern SPI_TypeDef g_f407_i2s_spi;
extern RCC_TypeDef g_f407_i2s_rcc;
extern GPIO_TypeDef g_f407_i2s_gpio;
extern DMA_TypeDef g_f407_i2s_dma;
extern DMA_Stream_TypeDef g_f407_i2s_stream;
extern TIM_TypeDef g_f407_i2s_timer;
uint32_t nx_f407_i2s_model_read(volatile uint32_t* reg);
void nx_f407_i2s_model_write(volatile uint32_t* reg, uint32_t value);
void nx_f407_i2s_model_poll(void);
void nx_f407_i2s_model_system_poll(void* system);
bool nx_f407_i2s_model_irq_quiet(void);
bool nx_f407_i2s_model_irq_withdraw(void);
uint16_t nx_f407_i2s_model_flash_kib(void);
uint32_t nx_f407_i2s_model_device_id(void);
#ifdef __cplusplus
}
#endif

#define NX_F407_I2S_SPI            (&g_f407_i2s_spi)
#define NX_F407_I2S_RCC            (&g_f407_i2s_rcc)
#define NX_F407_I2S_GPIO           (&g_f407_i2s_gpio)
#define NX_F407_I2S_DMA            (&g_f407_i2s_dma)
#define NX_F407_I2S_STREAM         (&g_f407_i2s_stream)
#define NX_F407_I2S_READ(p)        nx_f407_i2s_model_read(p)
#define NX_F407_I2S_WRITE(p, v)    nx_f407_i2s_model_write(p, v)
#define NX_F407_I2S_POLL()         nx_f407_i2s_model_poll()
#define NX_F407_I2S_IRQ_QUIET()    nx_f407_i2s_model_irq_quiet()
#define NX_F407_I2S_IRQ_WITHDRAW() nx_f407_i2s_model_irq_withdraw()
#define NX_F407_I2S_FLASH_KIB()    nx_f407_i2s_model_flash_kib()
#define NX_F407_I2S_DEVICE_ID()    nx_f407_i2s_model_device_id()

#define NX_STM32_WRITE(p, v) nx_f407_i2s_model_write(p, (uint32_t)(v))
#define NX_STM32_POLL(s)     nx_f407_i2s_model_system_poll(s)

#endif /* NEXUS_F407_I2S2_SLAVE_TX_MODEL_H */
