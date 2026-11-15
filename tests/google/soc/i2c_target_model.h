/**
 * \file            i2c_target_model.h
 * \brief           External I2C, GPIO, reset and NVIC command semantics
 * \author          Nexus Team
 */
#ifndef NX_I2C_TARGET_MODEL_H
#define NX_I2C_TARGET_MODEL_H
#include "stm32_official_model.h"
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
extern RCC_TypeDef nx_target_rcc;
extern I2C_TypeDef nx_target_i2c[3];
extern GPIO_TypeDef nx_target_gpio[9];
uint32_t nx_target_read(volatile uint32_t* address);
void nx_target_write(volatile uint32_t* address, uint32_t value);
uint32_t nx_target_irq(unsigned command, IRQn_Type irq, uint32_t value);
bool nx_target_privileged(void);
bool nx_target_in_isr(void);
bool nx_target_masked(void);
void nx_target_dsb(void);
#ifdef __cplusplus
}
#endif
#undef RCC
#undef I2C1
#undef I2C2
#undef I2C3
#undef GPIOA
#undef GPIOB
#undef GPIOC
#undef GPIOD
#undef GPIOE
#undef GPIOF
#undef GPIOG
#undef GPIOH
#undef GPIOI
#define RCC   (&nx_target_rcc)
#define I2C1  (&nx_target_i2c[0])
#define I2C2  (&nx_target_i2c[1])
#define I2C3  (&nx_target_i2c[2])
#define GPIOA (&nx_target_gpio[0])
#define GPIOB (&nx_target_gpio[1])
#define GPIOC (&nx_target_gpio[2])
#define GPIOD (&nx_target_gpio[3])
#define GPIOE (&nx_target_gpio[4])
#define GPIOF (&nx_target_gpio[5])
#define GPIOG (&nx_target_gpio[6])
#define GPIOH (&nx_target_gpio[7])
#define GPIOI (&nx_target_gpio[8])
#undef READ_REG
#undef WRITE_REG
#undef READ_BIT
#undef SET_BIT
#undef CLEAR_BIT
#undef MODIFY_REG
#define READ_REG(reg)         nx_target_read(&(reg))
#define WRITE_REG(reg, value) nx_target_write(&(reg), (uint32_t)(value))
#define READ_BIT(reg, mask)   (READ_REG(reg) & (uint32_t)(mask))
#define MODIFY_REG(reg, clear, set)                                            \
    WRITE_REG(reg, (READ_REG(reg) & ~(uint32_t)(clear)) | (uint32_t)(set))
#define SET_BIT(reg, mask)                   MODIFY_REG(reg, 0U, mask)
#define CLEAR_BIT(reg, mask)                 MODIFY_REG(reg, mask, 0U)
#define NX_I2C_TARGET_WRITE(p, v)            nx_target_write((p), (v))
#define NX_I2C_TARGET_READ(p)                nx_target_read(p)
#define NX_PLATFORM_REGISTER_WRITE(reg, v)   WRITE_REG(reg, v)
#define NX_I2C_TARGET_IRQ_ENABLE(i)          nx_target_irq(0U, i, 0U)
#define NX_I2C_TARGET_IRQ_DISABLE(i)         nx_target_irq(1U, i, 0U)
#define NX_I2C_TARGET_IRQ_CLEAR(i)           nx_target_irq(2U, i, 0U)
#define NX_I2C_TARGET_IRQ_PRIORITY_SET(i, p) nx_target_irq(3U, i, p)
#define NX_I2C_TARGET_IRQ_ENABLED(i)         nx_target_irq(4U, i, 0U)
#define NX_I2C_TARGET_IRQ_PENDING(i)         nx_target_irq(5U, i, 0U)
#define NX_I2C_TARGET_IRQ_ACTIVE(i)          nx_target_irq(6U, i, 0U)
#define NX_I2C_TARGET_IRQ_PRIORITY(i)        nx_target_irq(7U, i, 0U)
#endif /* NX_I2C_TARGET_MODEL_H */
