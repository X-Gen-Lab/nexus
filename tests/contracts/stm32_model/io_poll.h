/**
 * \file            io_poll.h
 * \brief           Host register model event and W1C hooks
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_STM32_IO_MODEL_H
#define NX_STM32_IO_MODEL_H
#include <stdint.h>
void nx_stm32_model_io_poll(unsigned kind, void* port);
uint32_t nx_stm32_model_gpio_clock_read(const volatile uint32_t* reg);
void nx_stm32_model_nvic_disable(int irq);
void nx_stm32_model_nvic_clear(int irq);
#define NX_STM32_IO_POLL(kind, port)    nx_stm32_model_io_poll(kind, port)
#define NX_STM32_CLEAR_FLAGS(reg, mask) ((reg) &= ~(unsigned)(mask))
#define NX_STM32_NVIC_DISABLE(irq)      nx_stm32_model_nvic_disable((int)(irq))
#define NX_STM32_NVIC_CLEAR(irq)        nx_stm32_model_nvic_clear((int)(irq))
#define NX_STM32_GPIO_CLOCK_READ(reg)   nx_stm32_model_gpio_clock_read(reg)
#endif
