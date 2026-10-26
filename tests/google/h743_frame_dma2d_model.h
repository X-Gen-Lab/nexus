/**
 * \file            h743_frame_dma2d_model.h
 * \brief           External MMIO boundaries for the real DMA2D owner
 * \author          Nexus Team
 */
#ifndef NEXUS_H743_FRAME_DMA2D_MODEL_H
#define NEXUS_H743_FRAME_DMA2D_MODEL_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
#define ARM_ARMV7M_CACHEL1_H
#endif
#include "stm32h743xx.h"
#ifdef __cplusplus
extern "C" {
#endif
extern DMA2D_TypeDef g_frame_dma2d;
extern RCC_TypeDef g_frame_rcc;
extern SCB_Type g_frame_core;
extern DBGMCU_TypeDef g_frame_debug;
uint32_t nx_frame_model_read(volatile uint32_t* reg);
void nx_frame_model_write(volatile uint32_t* reg, uint32_t value);
bool nx_frame_model_irq_quiet(void);
#ifdef __cplusplus
}
#endif
#define NX_H743_FRAME_REGS        (&g_frame_dma2d)
#define NX_H743_FRAME_RCC         (&g_frame_rcc)
#define NX_H743_FRAME_CORE        (&g_frame_core)
#define NX_H743_FRAME_DEBUG       (&g_frame_debug)
#define NX_H743_FRAME_READ(p)     nx_frame_model_read(p)
#define NX_H743_FRAME_WRITE(p, v) nx_frame_model_write(p, v)
#define NX_H743_FRAME_IRQ_QUIET() nx_frame_model_irq_quiet()
#endif
