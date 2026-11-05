/**
 * \file            cortex_m_port.h
 * \brief           Private classic CAN Cortex-M interrupt register port
 * \author          Nexus Team
 * \note            Native models override each macro before this include.
 *                  Preserve byte priority accesses and pending-before-enable
 *                  ordering; this port adds no runtime operations table.
 */
#ifndef NEXUS_CLASSIC_CAN_CORTEX_M_PORT_H
#define NEXUS_CLASSIC_CAN_CORTEX_M_PORT_H
#include "can_controller.h"

#ifndef NX_CAN_IRQ_PRIORITY
#define NX_CAN_IRQ_PRIORITY(controller, vector)                                \
    ((uint8_t)(*(volatile uint8_t*)(UINT32_C(0xE000E400) +                     \
                                    (uint16_t)(controller)                     \
                                        ->sources[(vector)]                    \
                                        .irq_number) >>                        \
               (8U - (controller)->irq_policy.priority_bits)))
#endif
#ifndef NX_CAN_IRQ_GROUP
#define NX_CAN_IRQ_GROUP(controller)                                           \
    ((uint8_t)((*(volatile uint32_t*)UINT32_C(0xE000ED0C) >> 8U) & 7U))
#endif
#ifndef NX_CAN_IRQ_CONFIGURE
#define NX_CAN_IRQ_CONFIGURE(controller, vector)                               \
    (*(volatile uint8_t*)(UINT32_C(0xE000E400) + (uint16_t)(controller)        \
                                                     ->sources[(vector)]       \
                                                     .irq_number) =            \
         (uint8_t)((controller)->sources[(vector)].priority                    \
                   << (8U - (controller)->irq_policy.priority_bits)))
#endif
#ifndef NX_CAN_IRQ_SET
/** \brief Write the exact selected NVIC bit and clear its pending fact. */
static void irq_set(nx_can_controller_t* controller, unsigned vector,
                    bool enabled) {
    unsigned irq = (unsigned)controller->sources[vector].irq_number;
    uintptr_t offset = (uintptr_t)(irq / 32U) * 4U;
    *(volatile uint32_t*)(UINT32_C(0xE000E280) + offset) = 1U << (irq % 32U);
    *(volatile uint32_t*)((enabled ? UINT32_C(0xE000E100)
                                   : UINT32_C(0xE000E180)) +
                          offset) = 1U << (irq % 32U);
}
#define NX_CAN_IRQ_SET(controller, vector, enabled)                            \
    irq_set((controller), (vector), (enabled))
#endif
#ifndef NX_CAN_IRQ_ENABLED
#define NX_CAN_IRQ_ENABLED(controller, vector)                                 \
    ((*(volatile uint32_t*)(UINT32_C(0xE000E100) +                             \
                            (uintptr_t)((uint16_t)(controller)                 \
                                            ->sources[(vector)]                \
                                            .irq_number /                      \
                                        32U) *                                 \
                                4U) &                                          \
      (1U << ((uint16_t)(controller)->sources[(vector)].irq_number % 32U))) != \
     0U)
#endif

#endif
