/**
 * \file            g474_fdcan_factory_model.h
 * \brief           Shared clock and FDCAN MMIO for actual generated assembly
 * \author          Nexus Team
 */
#ifndef NX_G474_FDCAN_FACTORY_MODEL_H
#define NX_G474_FDCAN_FACTORY_MODEL_H
#include "g474_model.h"
#ifdef __cplusplus
extern "C" {
#endif
extern FDCAN_GlobalTypeDef g_g474_factory_fdcan[3];
extern FDCAN_Config_TypeDef g_g474_factory_fdcan_config;
extern volatile uint32_t g_g474_factory_fdcan_ram[3U * 212U];
uint32_t nx_g474_fdcan_factory_read(volatile const uint32_t* address);
void nx_g474_fdcan_factory_write(volatile uint32_t* address, uint32_t value);
void nx_g474_fdcan_factory_priority_write(int irq, uint32_t priority);
#ifdef __cplusplus
}
#endif
#undef RCC
#undef FLASH
#undef PWR
#undef TIM2
#undef GPIOA
#undef FDCAN1
#undef FDCAN2
#undef FDCAN3
#undef FDCAN_CONFIG
#undef SRAMCAN_BASE
#define RCC                         (&g_g474_model.rcc)
#define FLASH                       (&g_g474_model.flash)
#define PWR                         (&g_g474_model.power)
#define TIM2                        (&g_g474_model.timer)
#define GPIOA                       (&g_g474_model.gpio)
#define FDCAN1                      (&g_g474_factory_fdcan[0])
#define FDCAN2                      (&g_g474_factory_fdcan[1])
#define FDCAN3                      (&g_g474_factory_fdcan[2])
#define FDCAN_CONFIG                (&g_g474_factory_fdcan_config)
#define SRAMCAN_BASE                ((uintptr_t)g_g474_factory_fdcan_ram)
#define NX_G474_FDCAN_READ(address) nx_g474_fdcan_factory_read(address)
#define NX_G474_FDCAN_WRITE(address, value)                                    \
    nx_g474_fdcan_factory_write((address), (uint32_t)(value))
#define NX_G474_FDCAN_IRQ_AVAILABLE(irq) NX_G474_IRQ_AVAILABLE(irq)
#define NX_G474_FDCAN_IRQ_CONTROL(irq, on, priority)                           \
    nx_g474_model_irq_control((irq), (on), (priority))
#define NX_G474_FDCAN_IRQ_PRIORITY(irq) g_g474_model.priority[(irq)]
#define NX_G474_FDCAN_IRQ_GROUP()       g_g474_model.group
#define NX_G474_IRQ_PRIORITY_WRITE(irq, priority)                              \
    nx_g474_fdcan_factory_priority_write((irq), (priority))
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_g474_fdcan_factory_write(&(reg), (uint32_t)(value))
#endif
