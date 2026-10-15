/**
 * \file            multi_controller_factory_model.h
 * \brief           External hardware leaves for five generated DMA controllers
 * \author          Nexus Team
 */
#ifndef NEXUS_MULTI_CONTROLLER_FACTORY_MODEL_H
#define NEXUS_MULTI_CONTROLLER_FACTORY_MODEL_H
#include <stdbool.h>
#include <stdint.h>
#ifdef NX_MULTI_FACTORY_STM32
#include "stm32f407xx.h"
#else
#include "gd32f4xx.h"
#endif
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Model atomic GPIO/RCC/NVIC command register writes and refusals. */
void nx_multi_factory_write(volatile uint32_t* reg, uint32_t value);
/** \brief Model exact byte-wide priority writes. */
void nx_multi_factory_priority_write(volatile uint8_t* reg, uint8_t value);
/** \brief Apply W1C to exactly the selected DMA interrupt bank. */
void nx_multi_factory_dma_ack(volatile uint32_t* clear, uint32_t value);
/** \brief Withdraw only the selected DMA engine, with EN refusal injection. */
void nx_multi_factory_dma_disable(volatile uint32_t* control, uint32_t value);
/** \brief Preserve exact NVIC enable, pending, active and priority facts. */
void nx_multi_factory_irq_enable(int irq);
void nx_multi_factory_irq_disable(int irq);
void nx_multi_factory_irq_pending_clear(int irq);
void nx_multi_factory_irq_priority(int irq, uint32_t priority);
uint32_t nx_multi_factory_irq_get_priority(int irq);
/** \brief Model the external CPU privilege boundary independently of Arch. */
bool nx_multi_factory_is_privileged(void);
/** \brief Model only the selected STM32 SPI reset pulse. */
void nx_multi_factory_spi_reset(volatile uint32_t* reg, uint32_t mask,
                                bool active);
/** \brief Model I2C DATA and address-read side effects. */
uint8_t nx_multi_factory_i2c_read(void* state);
void nx_multi_factory_i2c_write(void* state, uint32_t value);
void nx_multi_factory_i2c_address(void* state);
#ifdef __cplusplus
}
#endif
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_multi_factory_write(&(reg), (uint32_t)(value))
#define NX_PLATFORM_PRIORITY_WRITE(reg, value)                                 \
    nx_multi_factory_priority_write(&(reg), (uint8_t)(value))
#define NVIC_EnableIRQ(irq)       nx_multi_factory_irq_enable((int)(irq))
#define NVIC_DisableIRQ(irq)      nx_multi_factory_irq_disable((int)(irq))
#define NVIC_ClearPendingIRQ(irq) nx_multi_factory_irq_pending_clear((int)(irq))
#define NVIC_SetPriority(irq, priority)                                        \
    nx_multi_factory_irq_priority((int)(irq), (uint32_t)(priority))
#define NVIC_GetPriority(irq) nx_multi_factory_irq_get_priority((int)(irq))
#define nx_arch_is_privileged nx_multi_factory_is_privileged
#define NX_STM32_DMA_DISABLE(stream, mask)                                     \
    nx_multi_factory_dma_disable(&(stream)->CR, (uint32_t)(mask))
#define NX_STM32_UART_DMA_ACK(clear, mask)                                     \
    nx_multi_factory_dma_ack((clear), (uint32_t)(mask))
#define NX_STM32_SPI_DMA_ACK(clear, mask)                                      \
    nx_multi_factory_dma_ack((clear), (uint32_t)(mask))
#define NX_GD32_UART_DMA_DISABLE(control, mask)                                \
    nx_multi_factory_dma_disable((control), (uint32_t)(mask))
#define NX_GD32_UART_DMA_ACK(clear, mask)                                      \
    nx_multi_factory_dma_ack((clear), (uint32_t)(mask))
#define NX_GD32_SPI_DMA_CONTROL_DISABLE(control, mask)                         \
    nx_multi_factory_dma_disable((control), (uint32_t)(mask))
#define NX_GD32_SPI_DMA_FLAGS_ACK(clear, mask)                                 \
    nx_multi_factory_dma_ack((clear), (uint32_t)(mask))
#define NX_STM32_SPI_RESET(reg, mask, active)                                  \
    nx_multi_factory_spi_reset((reg), (uint32_t)(mask), (active))
#define NX_I2C_V1_DMA_DISABLE(control, mask)                                   \
    nx_multi_factory_dma_disable((control), (uint32_t)(mask))
#define NX_I2C_V1_DMA_ACK(clear, mask)                                         \
    nx_multi_factory_dma_ack((clear), (uint32_t)(mask))
#define NX_I2C_V1_READ_DATA(state) nx_multi_factory_i2c_read(state)
#define NX_I2C_V1_WRITE_DATA(state, value)                                     \
    nx_multi_factory_i2c_write((state), (uint32_t)(value))
#define NX_I2C_V1_ADDRESS_CLEARED(state) nx_multi_factory_i2c_address(state)
#endif
