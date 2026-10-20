/**
 * \file            legacy_transport_model.h
 * \brief           External mapped peripheral and NVIC transport boundary
 * \author          Nexus Team
 */
#ifndef NX_LEGACY_TRANSPORT_MODEL_H
#define NX_LEGACY_TRANSPORT_MODEL_H
#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_legacy_model_read(const volatile uint32_t* reg);
void nx_legacy_model_write(volatile uint32_t* reg, uint32_t value);
nx_result_t nx_legacy_model_irq(int irq, bool enable, uint8_t priority);
bool nx_legacy_model_irq_available(int irq);
uint32_t nx_legacy_model_priority(int irq);
#ifdef __cplusplus
}
#endif
#define NX_F103_READ(reg)         nx_legacy_model_read(reg)
#define NX_F103_WRITE(reg, value) nx_legacy_model_write((reg), (value))
#define NX_F103_IRQ_CONTROL(irq, enable, priority)                             \
    nx_legacy_model_irq((irq), (enable), (priority))
#define NX_F103_IRQ_AVAILABLE(irq) nx_legacy_model_irq_available(irq)
#define NX_F103_IRQ_PRIORITY(irq)  nx_legacy_model_priority(irq)
#define NX_F103_IRQ_GROUP()        3U
#define NX_GD303_READ(reg)         nx_legacy_model_read(reg)
#define NX_GD303_WRITE(reg, value) nx_legacy_model_write((reg), (value))
#define NX_GD303_IRQ_CONTROL(irq, enable, priority)                            \
    nx_legacy_model_irq((irq), (enable), (priority))
#define NX_GD303_IRQ_AVAILABLE(irq)  nx_legacy_model_irq_available(irq)
#define NX_GD303_IRQ_PRIORITY(irq)   nx_legacy_model_priority(irq)
#define NX_GD303_IRQ_GROUP()         3U
#define NX_UART_V1_READ(reg)         nx_legacy_model_read(reg)
#define NX_UART_V1_WRITE(reg, value) nx_legacy_model_write((reg), (value))
#endif
