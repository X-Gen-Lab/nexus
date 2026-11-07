/**
 * \file            usb_device_access.h
 * \brief           Native-only external MMIO/NVIC leaf replacements
 * \author          Nexus Team
 */
#ifndef NEXUS_NATIVE_USB_DEVICE_ACCESS_H
#define NEXUS_NATIVE_USB_DEVICE_ACCESS_H
#include <stdbool.h>
#include <stdint.h>
uint32_t nx_native_usb_read(uintptr_t, uint32_t);
void nx_native_usb_write(uintptr_t, uint32_t, uint32_t);
void nx_native_usb_irq_enable(uintptr_t, int, uint8_t, uint8_t);
void nx_native_usb_irq_disable(uintptr_t, int);
bool nx_native_usb_irq_enabled(uintptr_t, int);
uint8_t nx_native_usb_irq_priority(uintptr_t, int, uint8_t);
#define NX_USB_READ(base, offset) nx_native_usb_read((base), (offset))
#define NX_USB_WRITE(base, offset, value)                                      \
    nx_native_usb_write((base), (offset), (value))
#define NX_USB_IRQ_ENABLE(base, irq, priority, bits)                           \
    nx_native_usb_irq_enable((base), (irq), (priority), (bits))
#define NX_USB_IRQ_DISABLE(base, irq) nx_native_usb_irq_disable((base), (irq))
#define NX_USB_IRQ_ENABLED(base, irq) nx_native_usb_irq_enabled((base), (irq))
#define NX_USB_IRQ_PRIORITY(base, irq, bits)                                   \
    nx_native_usb_irq_priority((base), (irq), (bits))
#define NX_USB_IRQ_GROUP(base) ((void)(base), 3U)
#endif
