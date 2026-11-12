/**
 * \file            usb_host_transport_model.h
 * \brief           External MMIO leaves for locked HCD and LL execution
 * \author          Nexus Team
 */
#ifndef NEXUS_USB_HOST_TRANSPORT_MODEL_H
#define NEXUS_USB_HOST_TRANSPORT_MODEL_H
#include "stm32f407_usb_host.h"
#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_usb_transport_read(const volatile uint32_t* reg);
void nx_usb_transport_write(volatile uint32_t* reg, uint32_t value);
uint32_t nx_usb_transport_pop_rx(USB_OTG_GlobalTypeDef* core);
void nx_usb_transport_irq_enable(int irq, uint8_t priority);
void nx_usb_transport_irq_disable(int irq);
bool nx_usb_transport_irq_enabled(int irq);
#ifdef __cplusplus
}
#endif
#undef __HAL_HCD_CLEAR_FLAG
#undef __HAL_HCD_CLEAR_HC_INT
#define __HAL_HCD_CLEAR_FLAG(handle, flags)                                    \
    nx_usb_transport_write(&(handle)->Instance->GINTSTS, (flags))
#define __HAL_HCD_CLEAR_HC_INT(channel, flags)                                 \
    nx_usb_transport_write(&(USBx_HC(channel)->HCINT), (flags))
#define NX_USB_HOST_WRITE(reg, value) nx_usb_transport_write(&(reg), (value))
#define NX_USB_HOST_IRQ_ENABLE(irq, priority)                                  \
    nx_usb_transport_irq_enable((int)(irq), (priority))
#define NX_USB_HOST_IRQ_DISABLE(irq) nx_usb_transport_irq_disable((int)(irq))
#define NX_USB_HOST_IRQ_ENABLED(irq) nx_usb_transport_irq_enabled((int)(irq))
#define NX_USB_HOST_IRQ_ACTIVE(irq)  ((void)(irq), false)
#define NX_USB_HOST_CLOCK_VALID()    true
#define NX_USB_HOST_AHB_HZ           168000000U
#endif
