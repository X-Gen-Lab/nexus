/**
 * \file            h743_w25q128_model.h
 * \brief           Host MMIO and IRQ boundaries for the H743 flash owner
 * \author          Nexus Team
 */
#ifndef NEXUS_H743_W25Q128_MODEL_H
#define NEXUS_H743_W25Q128_MODEL_H

#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
#define ARM_ARMV7M_CACHEL1_H
#endif
#include "stm32h7xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

extern QUADSPI_TypeDef g_test_qspi;
extern RCC_TypeDef g_test_rcc;
extern SCB_Type g_test_core;
extern MPU_Type g_test_mpu;
extern uint8_t g_test_aperture[16777216U];

/** \brief Read only modeled register and hardware FIFO state. */
uint32_t nx_h743_w25q128_model_read(const volatile uint32_t* reg);
/** \brief Apply modeled MMIO writes without advancing the provider. */
void nx_h743_w25q128_model_write(volatile uint32_t* reg, uint32_t value);
/** \brief Report whether the QUADSPI interrupt is disabled and inactive. */
bool nx_h743_w25q128_model_irq_idle(void);
/** \brief Read the modeled QUADSPI NVIC pending latch. */
bool nx_h743_w25q128_model_irq_pending(void);
/** \brief Clear only the modeled QUADSPI NVIC pending latch. */
void nx_h743_w25q128_model_clear_pending(void);

#ifdef __cplusplus
}
#endif

#define NX_H743_W25Q128_REGS            (&g_test_qspi)
#define NX_H743_W25Q128_RCC             (&g_test_rcc)
#define NX_H743_W25Q128_CORE            (&g_test_core)
#define NX_H743_W25Q128_MPU             (&g_test_mpu)
#define NX_H743_W25Q128_APERTURE        (g_test_aperture)
#define NX_H743_W25Q128_READ(p)         nx_h743_w25q128_model_read((p))
#define NX_H743_W25Q128_WRITE(p, v)     nx_h743_w25q128_model_write((p), (v))
#define NX_H743_W25Q128_IRQ_IDLE()      nx_h743_w25q128_model_irq_idle()
#define NX_H743_W25Q128_IRQ_PENDING()   nx_h743_w25q128_model_irq_pending()
#define NX_H743_W25Q128_CLEAR_PENDING() nx_h743_w25q128_model_clear_pending()

#endif /* NEXUS_H743_W25Q128_MODEL_H */
