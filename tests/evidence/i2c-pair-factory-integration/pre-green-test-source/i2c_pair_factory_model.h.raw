/**
 * \file            i2c_pair_factory_model.h
 * \brief           External hardware leaves for two generated I2C controllers
 * \author          Nexus Team
 */
#ifndef NEXUS_I2C_PAIR_FACTORY_MODEL_H
#define NEXUS_I2C_PAIR_FACTORY_MODEL_H
#include "i2c_async_register_model.h"
#include <stdbool.h>
#ifdef NX_I2C_PAIR_STM32
#include "stm32f407xx.h"
#else
#include "gd32f4xx.h"
#endif
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Apply actual NVIC enable, disable and pending-bit semantics. */
void nx_i2c_pair_irq_enable(IRQn_Type irq);
void nx_i2c_pair_irq_disable(IRQn_Type irq);
void nx_i2c_pair_irq_clear(IRQn_Type irq);
/** \brief Store and read the actual external four-bit priority facts. */
void nx_i2c_pair_irq_priority(IRQn_Type irq, uint32_t value);
uint32_t nx_i2c_pair_irq_get_priority(IRQn_Type irq);
/** \brief Apply only the selected W1C flag word and mask. */
void nx_i2c_pair_dma_ack(volatile uint32_t* clear, uint32_t value);
/** \brief Apply or refuse an actual selected register write. */
void nx_i2c_pair_register_write(volatile uint32_t* reg, uint32_t value);
/** \brief Expose injected Thread/IRQ/mask facts only to this fixture. */
bool nx_i2c_pair_isr(void);
bool nx_i2c_pair_masked(void);
#ifdef __cplusplus
}
#endif
#define NVIC_EnableIRQ(irq)             nx_i2c_pair_irq_enable(irq)
#define NVIC_DisableIRQ(irq)            nx_i2c_pair_irq_disable(irq)
#define NVIC_ClearPendingIRQ(irq)       nx_i2c_pair_irq_clear(irq)
#define NVIC_SetPriority(irq, value)    nx_i2c_pair_irq_priority((irq), (value))
#define NVIC_GetPriority(irq)           nx_i2c_pair_irq_get_priority(irq)
#define NX_I2C_V1_DMA_ACK(clear, value) nx_i2c_pair_dma_ack((clear), (value))
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_i2c_pair_register_write(&(reg), (uint32_t)(value))
#define nx_arch_in_isr        nx_i2c_pair_isr
#define nx_arch_irq_is_masked nx_i2c_pair_masked
#endif
