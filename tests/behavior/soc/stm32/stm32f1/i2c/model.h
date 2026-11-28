/**
 * \file            model.h
 * \brief           Intercept real F103 I2C and pin register accesses
 * \author          Nexus Team
 */
#ifndef NX_STM32F1_I2C_MODEL_H
#define NX_STM32F1_I2C_MODEL_H
#include "stm32f1xx.h"
#ifdef __cplusplus
extern "C" {
#endif
extern I2C_TypeDef f103_i2c;
extern RCC_TypeDef f103_i2c_rcc;
extern AFIO_TypeDef f103_i2c_afio;
extern GPIO_TypeDef f103_i2c_gpio;
uint32_t f103_i2c_read(const volatile uint32_t* field);
void f103_i2c_write(volatile uint32_t* field, uint32_t value);
#ifdef __cplusplus
}
#endif
#undef I2C1
#undef GPIOB
#undef RCC
#undef AFIO
#undef READ_REG
#undef WRITE_REG
#undef READ_BIT
#undef MODIFY_REG
#undef SET_BIT
#undef CLEAR_BIT
#define I2C1                  (&f103_i2c)
#define GPIOB                 (&f103_i2c_gpio)
#define RCC                   (&f103_i2c_rcc)
#define AFIO                  (&f103_i2c_afio)
#define READ_REG(reg)         f103_i2c_read(&(reg))
#define WRITE_REG(reg, value) f103_i2c_write(&(reg), (uint32_t)(value))
#define READ_BIT(reg, mask)   (READ_REG(reg) & (mask))
#define MODIFY_REG(reg, clear, value)                                          \
    WRITE_REG(reg, (READ_REG(reg) & ~(uint32_t)(clear)) | (value))
#define SET_BIT(reg, bits)               MODIFY_REG(reg, 0U, bits)
#define CLEAR_BIT(reg, bits)             MODIFY_REG(reg, bits, 0U)
#define NX_STM32F1_WRITE(reg, value)     f103_i2c_write((reg), (uint32_t)(value))
#define NX_STM32F1_GPIO_BSRR(reg, value) WRITE_REG((reg)->BSRR, value)
#endif
