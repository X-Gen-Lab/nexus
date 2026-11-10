/**
 * \file            usb_host_hal_model.h
 * \brief           External NVIC and W1C leaves for the actual Host provider
 * \author          Nexus Team
 */
#ifndef NEXUS_USB_HOST_HAL_MODEL_H
#define NEXUS_USB_HOST_HAL_MODEL_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void nx_usb_host_model_write(volatile uint32_t* reg, uint32_t value);
void nx_usb_host_model_irq_enable(int irq, uint8_t priority);
void nx_usb_host_model_irq_disable(int irq);
bool nx_usb_host_model_irq_enabled(int irq);
bool nx_usb_host_model_irq_active(int irq);
extern uint32_t g_nx_usb_host_hclk_hz;
#ifdef __cplusplus
}
#endif
#define NX_USB_HOST_WRITE(reg, value) nx_usb_host_model_write(&(reg), (value))
#define NX_USB_HOST_IRQ_ENABLE(irq, priority)                                  \
    nx_usb_host_model_irq_enable((int)(irq), (priority))
#define NX_USB_HOST_IRQ_DISABLE(irq) nx_usb_host_model_irq_disable((int)(irq))
#define NX_USB_HOST_IRQ_ENABLED(irq) nx_usb_host_model_irq_enabled((int)(irq))
#define NX_USB_HOST_IRQ_ACTIVE(irq)  nx_usb_host_model_irq_active((int)(irq))
#define NX_USB_HOST_CLOCK_VALID()    true
#define NX_USB_HOST_AHB_HZ           g_nx_usb_host_hclk_hz
#endif
