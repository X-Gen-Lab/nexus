/**
 * \file            connectivity_host_model.h
 * \brief           Model only the selected F1 Host hardware transactions.
 * \author          Nexus Team
 */
#ifndef NEXUS_TESTS_F1_CONNECTIVITY_HOST_MODEL_H
#define NEXUS_TESTS_F1_CONNECTIVITY_HOST_MODEL_H
#include "stm32f1xx.h"
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
extern RCC_TypeDef nx_f1_host_rcc;
extern USB_OTG_GlobalTypeDef* nx_f1_host_core;
void nx_f1_host_write(volatile uint32_t* reg, uint32_t value);
void nx_f1_host_irq_enable(IRQn_Type irq, uint8_t priority);
void nx_f1_host_irq_disable(IRQn_Type irq);
bool nx_f1_host_irq_enabled(IRQn_Type irq);
bool nx_f1_host_irq_active(IRQn_Type irq);
bool nx_f1_host_irq_pending(IRQn_Type irq);
#ifdef __cplusplus
}
#endif
#undef RCC
#define RCC (&nx_f1_host_rcc)
#undef USB_OTG_FS
#define USB_OTG_FS                    nx_f1_host_core
#define NX_USB_HOST_WRITE(reg, value) nx_f1_host_write(&(reg), (value))
#define NX_USB_HOST_IRQ_ENABLE(irq, priority)                                  \
    nx_f1_host_irq_enable(irq, priority)
#define NX_USB_HOST_IRQ_DISABLE(irq) nx_f1_host_irq_disable(irq)
#define NX_USB_HOST_IRQ_ENABLED(irq) nx_f1_host_irq_enabled(irq)
#define NX_USB_HOST_IRQ_ACTIVE(irq)  nx_f1_host_irq_active(irq)
#define NX_USB_HOST_IRQ_PENDING(irq) nx_f1_host_irq_pending(irq)
#endif
