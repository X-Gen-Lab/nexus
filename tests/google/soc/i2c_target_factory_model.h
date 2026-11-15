/**
 * \file            i2c_target_factory_model.h
 * \brief           Actual generated owner with external MMIO and CPU leaves
 * \author          Nexus Team
 */
#ifndef NX_I2C_TARGET_FACTORY_MODEL_H
#define NX_I2C_TARGET_FACTORY_MODEL_H
#include "stm32_official_model.h"
#include "stm32_platform_cold_model.h"
#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_i2c_target_factory_irq(unsigned command, IRQn_Type irq,
                                   uint32_t value);
uint32_t nx_i2c_target_factory_read(volatile uint32_t* address);
#ifdef __cplusplus
}
#endif
#define NX_I2C_TARGET_WRITE(p, v)            nx_platform_cold_model_write(p, v)
#define NX_I2C_TARGET_READ(p)                nx_i2c_target_factory_read(p)
#define NX_I2C_TARGET_IRQ_ENABLE(i)          nx_i2c_target_factory_irq(0U, i, 0U)
#define NX_I2C_TARGET_IRQ_DISABLE(i)         nx_i2c_target_factory_irq(1U, i, 0U)
#define NX_I2C_TARGET_IRQ_CLEAR(i)           nx_i2c_target_factory_irq(2U, i, 0U)
#define NX_I2C_TARGET_IRQ_PRIORITY_SET(i, p) nx_i2c_target_factory_irq(3U, i, p)
#define NX_I2C_TARGET_IRQ_ENABLED(i)         nx_i2c_target_factory_irq(4U, i, 0U)
#define NX_I2C_TARGET_IRQ_PENDING(i)         nx_i2c_target_factory_irq(5U, i, 0U)
#define NX_I2C_TARGET_IRQ_ACTIVE(i)          nx_i2c_target_factory_irq(6U, i, 0U)
#define NX_I2C_TARGET_IRQ_PRIORITY(i)        nx_i2c_target_factory_irq(7U, i, 0U)
#endif /* NX_I2C_TARGET_FACTORY_MODEL_H */
