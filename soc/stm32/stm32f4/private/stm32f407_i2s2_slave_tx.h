/**
 * \file            stm32f407_i2s2_slave_tx.h
 * \brief           Fixed F407 I2S2 register and instruction leaves
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_STM32F407_I2S2_SLAVE_TX_PRIVATE_H
#define NEXUS_STM32F407_I2S2_SLAVE_TX_PRIVATE_H

#include "stm32f407_system.h"

extern nx_stm32_system_t g_nx_stm32_system;

#ifndef NX_F407_I2S_SPI
#define NX_F407_I2S_SPI             SPI2
#define NX_F407_I2S_RCC             RCC
#define NX_F407_I2S_GPIO            GPIOB
#define NX_F407_I2S_DMA             DMA1
#define NX_F407_I2S_STREAM          DMA1_Stream4
#define NX_F407_I2S_READ(p)         (*(p))
#define NX_F407_I2S_WRITE(p, value) (*(p) = (value))
#define NX_F407_I2S_POLL()          ((void)0)
#define NX_F407_I2S_FLASH_KIB()     (*(volatile const uint16_t*)FLASHSIZE_BASE)
#define NX_F407_I2S_DEVICE_ID()     (DBGMCU->IDCODE)

/** \brief No ISR is installed; both physical vectors remain withdrawn. */
static inline bool nx_f407_i2s_irq_quiet(void) {
    return NVIC_GetEnableIRQ(SPI2_IRQn) == 0U &&
           NVIC_GetPendingIRQ(SPI2_IRQn) == 0U &&
           NVIC_GetActive(SPI2_IRQn) == 0U &&
           NVIC_GetEnableIRQ(DMA1_Stream4_IRQn) == 0U &&
           NVIC_GetPendingIRQ(DMA1_Stream4_IRQn) == 0U &&
           NVIC_GetActive(DMA1_Stream4_IRQn) == 0U;
}

/** \brief Withdraw only after peripheral and DMA interrupt sources are off. */
static inline bool nx_f407_i2s_irq_withdraw(void) {
    NVIC_DisableIRQ(SPI2_IRQn);
    NVIC_DisableIRQ(DMA1_Stream4_IRQn);
    NVIC_ClearPendingIRQ(SPI2_IRQn);
    NVIC_ClearPendingIRQ(DMA1_Stream4_IRQn);
    nx_arch_dsb();
    nx_arch_isb();
    return nx_f407_i2s_irq_quiet();
}
#define NX_F407_I2S_IRQ_QUIET()    nx_f407_i2s_irq_quiet()
#define NX_F407_I2S_IRQ_WITHDRAW() nx_f407_i2s_irq_withdraw()
#endif

#endif /* NEXUS_STM32F407_I2S2_SLAVE_TX_PRIVATE_H */
