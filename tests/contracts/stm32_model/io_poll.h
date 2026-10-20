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
#include "stm32f407xx.h"
#include <stdint.h>
typedef struct {
    uint32_t ISER[3];
    uint32_t ISPR[3];
    uint32_t IABR[3];
    uint8_t priority[96];
    uint32_t pending_on_enable;
} nx_stm32_io_nvic_model_t;
extern nx_stm32_io_nvic_model_t g_nx_stm32_io_nvic;
/** \brief Read a leaf vector bit from shared deterministic NVIC storage. */
static inline uint32_t nx_stm32_model_nvic_bit(const uint32_t* registers,
                                               int irq) {
    unsigned index = (unsigned)irq;
    return (registers[index / 32U] >> (index % 32U)) & 1U;
}
/** \brief Publish the actual enable effect into the shared model storage. */
static inline void nx_stm32_model_nvic_enable(int irq) {
    unsigned index = (unsigned)irq;
    g_nx_stm32_io_nvic.ISER[index / 32U] |= 1U << (index % 32U);
    if (g_nx_stm32_io_nvic.pending_on_enable != 0U)
        g_nx_stm32_io_nvic.ISPR[index / 32U] |= 1U << (index % 32U);
}
void nx_stm32_model_io_poll(unsigned kind, void* port);
uint32_t nx_stm32_model_gpio_clock_read(const volatile uint32_t* reg);
void nx_stm32_model_nvic_disable(int irq);
void nx_stm32_model_nvic_clear(int irq);
#undef NVIC
#undef NVIC_GetEnableIRQ
#undef NVIC_GetPendingIRQ
#undef NVIC_GetActive
#undef NVIC_ClearPendingIRQ
#undef NVIC_GetPriority
#undef NVIC_SetPriority
#undef NVIC_EnableIRQ
#define NVIC                      (&g_nx_stm32_io_nvic)
#define NVIC_GetEnableIRQ(irq)    nx_stm32_model_nvic_bit(NVIC->ISER, (int)(irq))
#define NVIC_GetPendingIRQ(irq)   nx_stm32_model_nvic_bit(NVIC->ISPR, (int)(irq))
#define NVIC_GetActive(irq)       nx_stm32_model_nvic_bit(NVIC->IABR, (int)(irq))
#define NVIC_ClearPendingIRQ(irq) nx_stm32_model_nvic_clear((int)(irq))
#define NVIC_GetPriority(irq)     (NVIC->priority[(unsigned)(irq)])
#define NVIC_SetPriority(irq, value)                                           \
    (NVIC->priority[(unsigned)(irq)] = (uint8_t)(value))
#define NVIC_EnableIRQ(irq)             nx_stm32_model_nvic_enable((int)(irq))
#define NX_STM32_IO_POLL(kind, port)    nx_stm32_model_io_poll(kind, port)
#define NX_STM32_CLEAR_FLAGS(reg, mask) ((reg) &= ~(unsigned)(mask))
#define NX_STM32_NVIC_DISABLE(irq)      nx_stm32_model_nvic_disable((int)(irq))
#define NX_STM32_NVIC_CLEAR(irq)        nx_stm32_model_nvic_clear((int)(irq))
#define NX_STM32_GPIO_CLOCK_READ(reg)   nx_stm32_model_gpio_clock_read(reg)
#endif
