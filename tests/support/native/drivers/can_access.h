/**
 * \file            can_access.h
 * \brief           Opt-in Native MMIO boundaries for the production CAN engine
 * \author          Nexus Team
 */
#ifndef NEXUS_NATIVE_CAN_ACCESS_H
#define NEXUS_NATIVE_CAN_ACCESS_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_native_can_read(uintptr_t base, unsigned offset);
void nx_native_can_write(uintptr_t base, unsigned offset, uint32_t value);
#ifdef __cplusplus
}
#endif
#define NX_CAN_READ(base, offset) nx_native_can_read((base), (offset))
#define NX_CAN_WRITE(base, offset, value)                                      \
    nx_native_can_write((base), (offset), (value))
#define NX_CAN_IRQ_PRIORITY(controller, vector)                                \
    ((controller)->sources[(vector)].priority)
#define NX_CAN_IRQ_GROUP(controller) ((controller)->sources[0].priority_group)
#define NX_CAN_IRQ_CONFIGURE(controller, vector)                               \
    ((void)(controller), (void)(vector))
#define NX_CAN_IRQ_SET(controller, vector, enabled)                            \
    nx_native_can_irq_set((controller), (vector), (enabled))
#define NX_CAN_IRQ_ENABLED(controller, vector)                                 \
    nx_native_can_irq_enabled((controller), (vector))
#include "can.h"
#endif
