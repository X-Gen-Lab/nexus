/**
 * \file            stm32_spi_frames_model.h
 * \brief           External SPI register, reset and NVIC command semantics
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32_SPI_FRAMES_MODEL_H
#define NEXUS_STM32_SPI_FRAMES_MODEL_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void nx_spi_frames_model_write(volatile uint32_t* address, uint32_t value);
void nx_spi_frames_model_tx(uintptr_t registers, uint16_t value, uint8_t bits);
uint16_t nx_spi_frames_model_rx(uintptr_t registers, uint8_t bits);
uint32_t nx_spi_frames_model_irq(unsigned command, int irq, uint32_t value);
void nx_spi_frames_model_reset(uintptr_t registers);
uintptr_t nx_spi_frames_model_gpio(unsigned port);
uintptr_t nx_spi_frames_model_instance(int irq);
uintptr_t nx_spi_frames_model_rcc(void);
#ifdef __cplusplus
}
#endif
#define NX_SPI_FRAMES_WRITE(address, value)                                    \
    nx_spi_frames_model_write((address), (uint32_t)(value))
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_spi_frames_model_write(&(reg), (uint32_t)(value))
#define NX_SPI_FRAMES_GPIO(port) ((GPIO_TypeDef*)nx_spi_frames_model_gpio(port))
#define NX_SPI_FRAMES_INSTANCE(irq)                                            \
    ((SPI_TypeDef*)nx_spi_frames_model_instance((int)(irq)))
#define NX_SPI_FRAMES_RCC() ((RCC_TypeDef*)nx_spi_frames_model_rcc())
#define NX_SPI_FRAMES_TRANSMIT(registers, value, bits)                         \
    nx_spi_frames_model_tx((uintptr_t)(registers), (uint16_t)(value),          \
                           (uint8_t)(bits))
#define NX_SPI_FRAMES_RECEIVE(registers, bits)                                 \
    nx_spi_frames_model_rx((uintptr_t)(registers), (uint8_t)(bits))
#define NX_SPI_FRAMES_RESET_EVENT(registers)                                   \
    nx_spi_frames_model_reset((uintptr_t)(registers))
#define NX_SPI_FRAMES_IRQ_ENABLE(irq)                                          \
    nx_spi_frames_model_irq(0U, (int)(irq), 0U)
#define NX_SPI_FRAMES_IRQ_DISABLE(irq)                                         \
    nx_spi_frames_model_irq(1U, (int)(irq), 0U)
#define NX_SPI_FRAMES_IRQ_CLEAR(irq) nx_spi_frames_model_irq(2U, (int)(irq), 0U)
#define NX_SPI_FRAMES_IRQ_PRIORITY_SET(irq, priority)                          \
    nx_spi_frames_model_irq(3U, (int)(irq), (uint32_t)(priority))
#define NX_SPI_FRAMES_IRQ_ENABLED(irq)                                         \
    nx_spi_frames_model_irq(4U, (int)(irq), 0U)
#define NX_SPI_FRAMES_IRQ_PENDING(irq)                                         \
    nx_spi_frames_model_irq(5U, (int)(irq), 0U)
#define NX_SPI_FRAMES_IRQ_ACTIVE(irq)                                          \
    nx_spi_frames_model_irq(6U, (int)(irq), 0U)
#define NX_SPI_FRAMES_IRQ_PRIORITY(irq)                                        \
    nx_spi_frames_model_irq(7U, (int)(irq), 0U)
#endif
