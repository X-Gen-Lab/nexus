/**
 * \file            stm32_ltdc_model.h
 * \brief           External LTDC MMIO shadow/active and CPU model leaves
 * \author          Nexus Team
 */
#ifndef NX_GRAPHICS_MODEL_H
#define NX_GRAPHICS_MODEL_H
#include "nexus/core/time.h"
#include "stm32_official_model.h"
#define USE_HAL_LTDC_REGISTER_CALLBACKS 0U
#include "stm32f4xx_hal_ltdc.h"
#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_gfx_model_read(volatile uint32_t* reg);
void nx_gfx_model_write(volatile uint32_t* reg, uint32_t value);
LTDC_Layer_TypeDef* nx_gfx_model_shadow(LTDC_HandleTypeDef*, uint32_t);
LTDC_Layer_TypeDef* nx_gfx_model_active_layer(LTDC_HandleTypeDef*, uint32_t);
LTDC_TypeDef* nx_gfx_model_registers(void);
nx_time_us_t nx_gfx_model_now(void);
bool nx_gfx_model_task(void);
uint32_t nx_gfx_model_irq_enabled(IRQn_Type irq);
uint32_t nx_gfx_model_irq_pending(IRQn_Type irq);
uint32_t nx_gfx_model_irq_active(IRQn_Type irq);
void nx_gfx_model_irq_enable(IRQn_Type irq);
void nx_gfx_model_irq_disable(IRQn_Type irq);
void nx_gfx_model_irq_clear(IRQn_Type irq);
uint32_t nx_gfx_model_irq_priority(IRQn_Type irq);
void nx_gfx_model_irq_set_priority(IRQn_Type irq, uint32_t priority);
#ifdef __cplusplus
}
#endif
/* CMSIS LTDC_LAYER is the address boundary. The two host views model the
 * documented shadow-write/active-read alias without replacing HAL methods. */
#undef LTDC_LAYER
#define LTDC_LAYER(handle, layer) nx_gfx_model_shadow((handle), (layer))
#define NX_GFX_ACTIVE_LAYER(handle, layer)                                     \
    nx_gfx_model_active_layer((handle), (layer))
#undef READ_REG
#undef WRITE_REG
#undef SET_BIT
#undef CLEAR_BIT
#undef MODIFY_REG
#define READ_REG(reg)         nx_gfx_model_read(&(reg))
#define WRITE_REG(reg, value) nx_gfx_model_write(&(reg), (uint32_t)(value))
#define SET_BIT(reg, mask)    WRITE_REG((reg), READ_REG(reg) | (mask))
#define CLEAR_BIT(reg, mask)  WRITE_REG((reg), READ_REG(reg) & ~(mask))
#define MODIFY_REG(reg, mask, value)                                           \
    WRITE_REG((reg), (READ_REG(reg) & ~(mask)) | (value))
#undef __HAL_LTDC_CLEAR_FLAG
#define __HAL_LTDC_CLEAR_FLAG(handle, flags)                                   \
    nx_gfx_model_write(&(handle)->Instance->ICR, (uint32_t)(flags))
#undef __HAL_LTDC_ENABLE
#define __HAL_LTDC_ENABLE(handle)                                              \
    nx_gfx_model_write(&(handle)->Instance->GCR,                               \
                       nx_gfx_model_read(&(handle)->Instance->GCR) |           \
                           (uint32_t)LTDC_GCR_LTDCEN)
#define NX_GFX_READ(reg)         nx_gfx_model_read(reg)
#define NX_GFX_WRITE(reg, value) nx_gfx_model_write((reg), (value))
#define NX_GFX_NOW()             nx_gfx_model_now()
#define NX_GFX_TASK()            nx_gfx_model_task()
#define NX_GFX_IRQ_ENABLED(irq)  nx_gfx_model_irq_enabled(irq)
#define NX_GFX_IRQ_PENDING(irq)  nx_gfx_model_irq_pending(irq)
#define NX_GFX_IRQ_ACTIVE(irq)   nx_gfx_model_irq_active(irq)
#define NX_GFX_IRQ_ENABLE(irq)   nx_gfx_model_irq_enable(irq)
#define NX_GFX_IRQ_DISABLE(irq)  nx_gfx_model_irq_disable(irq)
#define NX_GFX_IRQ_CLEAR(irq)    nx_gfx_model_irq_clear(irq)
#define NX_GFX_IRQ_PRIORITY(irq) nx_gfx_model_irq_priority(irq)
#define NX_GFX_IRQ_SET_PRIORITY(irq, value)                                    \
    nx_gfx_model_irq_set_priority((irq), (value))
#define NX_GFX_IRQ_GROUP() 0U
#undef LTDC
#define LTDC nx_gfx_model_registers()
#endif /* NX_GRAPHICS_MODEL_H */
