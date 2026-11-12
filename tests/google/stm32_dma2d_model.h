/**
 * \file            stm32_dma2d_model.h
 * \brief           DMA2D MMIO and CPU leaves for the locked official HAL
 * \author          Nexus Team
 */
#ifndef NX_IMAGE_MODEL_H
#define NX_IMAGE_MODEL_H
#include "nexus/core/time.h"
#include "stm32_official_model.h"
#define USE_HAL_DMA2D_REGISTER_CALLBACKS 0U
#include "stm32f4xx_hal_dma2d.h"
#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_img_model_read(volatile uint32_t* reg);
void nx_img_model_write(volatile uint32_t* reg, uint32_t value);
nx_time_us_t nx_img_model_now(void);
bool nx_img_model_task(void);
uint32_t nx_img_model_irq_enabled(void);
uint32_t nx_img_model_irq_pending(void);
uint32_t nx_img_model_irq_active(void);
void nx_img_model_irq_enable(void);
void nx_img_model_irq_disable(void);
void nx_img_model_irq_clear(void);
uint32_t nx_img_model_irq_priority(void);
void nx_img_model_irq_set_priority(uint32_t priority);
#ifdef __cplusplus
}
#endif
#undef READ_REG
#undef WRITE_REG
#undef SET_BIT
#undef CLEAR_BIT
#undef MODIFY_REG
#define READ_REG(reg)         nx_img_model_read(&(reg))
#define WRITE_REG(reg, value) nx_img_model_write(&(reg), (uint32_t)(value))
#define SET_BIT(reg, mask)    WRITE_REG((reg), READ_REG(reg) | (mask))
#define CLEAR_BIT(reg, mask)  WRITE_REG((reg), READ_REG(reg) & ~(mask))
#define MODIFY_REG(reg, mask, value)                                           \
    WRITE_REG((reg), (READ_REG(reg) & ~(mask)) | (value))
#undef __HAL_DMA2D_CLEAR_FLAG
#define __HAL_DMA2D_CLEAR_FLAG(handle, flags)                                  \
    nx_img_model_write(&(handle)->Instance->IFCR, (uint32_t)(flags))
#define NX_IMG_READ(reg)               nx_img_model_read(reg)
#define NX_IMG_WRITE(reg, value)       nx_img_model_write((reg), (value))
#define NX_IMG_NOW()                   nx_img_model_now()
#define NX_IMG_TASK()                  nx_img_model_task()
#define NX_IMG_IRQ_ENABLED()           nx_img_model_irq_enabled()
#define NX_IMG_IRQ_PENDING()           nx_img_model_irq_pending()
#define NX_IMG_IRQ_ACTIVE()            nx_img_model_irq_active()
#define NX_IMG_IRQ_ENABLE()            nx_img_model_irq_enable()
#define NX_IMG_IRQ_DISABLE()           nx_img_model_irq_disable()
#define NX_IMG_IRQ_CLEAR()             nx_img_model_irq_clear()
#define NX_IMG_IRQ_PRIORITY()          nx_img_model_irq_priority()
#define NX_IMG_IRQ_SET_PRIORITY(value) nx_img_model_irq_set_priority(value)
#define NX_IMG_IRQ_GROUP()             0U
#endif /* NX_IMAGE_MODEL_H */
