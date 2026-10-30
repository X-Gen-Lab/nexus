/**
 * \file            h743_peripheral_factory_model.h
 * \brief           External exact selected H743 peripheral MMIO fixtures
 * \author          Nexus Team
 */
#ifndef NX_H743_PERIPHERAL_FACTORY_MODEL_H
#define NX_H743_PERIPHERAL_FACTORY_MODEL_H
#include "h743_factory_model.h"
#include "h743_peripheral_model.h"
#undef GPIOE
#undef TIM3
#undef SPI4
#undef I2C1
#undef ADC1
#undef ADC2
#undef ADC12_COMMON
#undef IWDG1
#undef DBGMCU
#define GPIOE                       (&g_h743_model.gpio_e)
#define TIM3                        (&g_h743_model.pwm_registers)
#define SPI4                        (&g_h743_model.spi_registers)
#define I2C1                        (&g_h743_model.i2c_registers)
#define ADC1                        (&g_h743_model.adc_registers)
#define ADC2                        (&g_h743_model.adc_peer)
#define ADC12_COMMON                (&g_h743_model.adc_common)
#define IWDG1                       (&g_h743_model.watchdog_registers)
#define DBGMCU                      (&g_h743_model.debug)
#define NX_I2C_V2_WRITE(reg, value) NX_H743_WRITE(reg, value)
#define NX_I2C_V2_READ(reg)         NX_H743_READ(reg)
/* Keep external data-register effects when the SoC selects official leaves. */
#define NX_I2C_V2_RECEIVE_DATA(registers)                                      \
    ((uint8_t)NX_I2C_V2_READ(&(registers)->rxdr))
#define NX_I2C_V2_TRANSMIT_DATA(registers, value)                              \
    NX_I2C_V2_WRITE(&(registers)->txdr, (value))
#endif
