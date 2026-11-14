/**
 * \file            model.h
 * \brief           External NVIC observation boundary for real GD32 SPL tests
 * \author          Nexus Team
 */
#ifndef NX_GD32_REFERENCE_MODEL_H
#define NX_GD32_REFERENCE_MODEL_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
bool nx_gd32_model_irq_enabled(void);
bool nx_gd32_model_irq_pending(void);
bool nx_gd32_model_irq_active(void);
uint32_t nx_gd32_model_irq_priority(void);
void nx_gd32_model_irq_set_priority(uint32_t value);
void nx_gd32_model_irq_enable(void);
void nx_gd32_model_irq_disable(void);
void nx_gd32_model_irq_clear(void);
#ifdef __cplusplus
}
#endif
#define NX_GD32_IRQ_ENABLED()           nx_gd32_model_irq_enabled()
#define NX_GD32_IRQ_PENDING()           nx_gd32_model_irq_pending()
#define NX_GD32_IRQ_ACTIVE()            nx_gd32_model_irq_active()
#define NX_GD32_IRQ_PRIORITY()          nx_gd32_model_irq_priority()
#define NX_GD32_IRQ_SET_PRIORITY(value) nx_gd32_model_irq_set_priority(value)
#define NX_GD32_IRQ_ENABLE()            nx_gd32_model_irq_enable()
#define NX_GD32_IRQ_DISABLE()           nx_gd32_model_irq_disable()
#define NX_GD32_IRQ_CLEAR()             nx_gd32_model_irq_clear()
#endif /* NX_GD32_REFERENCE_MODEL_H */
