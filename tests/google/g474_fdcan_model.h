/**
 * \file            g474_fdcan_model.h
 * \brief           Test-only external G474 SDK MMIO and IRQ boundary
 * \author          Nexus Team
 */
#ifndef NX_G474_FDCAN_MODEL_H
#define NX_G474_FDCAN_MODEL_H
#include "nexus/arch/arch.h"
#include "nexus/core/status.h"
#include "stm32g474xx.h"
#ifdef __cplusplus
extern "C" {
#endif
extern RCC_TypeDef g_io33_rcc;
extern FDCAN_Config_TypeDef g_io33_configuration;
extern FDCAN_GlobalTypeDef g_io33_fdcan[3];
extern volatile uint32_t g_io33_ram[3U * 212U];
uint32_t nx_io33_read(volatile const uint32_t*);
void nx_io33_write(volatile uint32_t*, uint32_t);
bool nx_io33_irq_available(int32_t);
nx_result_t nx_io33_irq_control(int32_t, bool, uint8_t);
uint8_t nx_io33_irq_priority(int32_t);
#ifdef __cplusplus
}
#endif
#undef RCC
#undef FDCAN1
#undef FDCAN2
#undef FDCAN3
#undef FDCAN_CONFIG
#undef SRAMCAN_BASE
#define RCC                              (&g_io33_rcc)
#define FDCAN1                           (&g_io33_fdcan[0])
#define FDCAN2                           (&g_io33_fdcan[1])
#define FDCAN3                           (&g_io33_fdcan[2])
#define FDCAN_CONFIG                     (&g_io33_configuration)
#define SRAMCAN_BASE                     ((uintptr_t)g_io33_ram)
#define NX_G474_FDCAN_READ(reg)          nx_io33_read(reg)
#define NX_G474_FDCAN_WRITE(reg, value)  nx_io33_write((reg), (value))
#define NX_G474_FDCAN_IRQ_AVAILABLE(irq) nx_io33_irq_available(irq)
#define NX_G474_FDCAN_IRQ_CONTROL(irq, on, priority)                           \
    nx_io33_irq_control((irq), (on), (priority))
#define NX_G474_FDCAN_IRQ_PRIORITY(irq) nx_io33_irq_priority(irq)
#define NX_G474_FDCAN_IRQ_GROUP()       3U
#endif
