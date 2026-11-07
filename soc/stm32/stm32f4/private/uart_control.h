/**
 * \file            uart_control.h
 * \brief           Compile-time leaf UART MMIO and IRQ test boundaries.
 * \author          Nexus Team
 */
#ifndef NEXUS_PRIVATE_UART_CONTROL_H
#define NEXUS_PRIVATE_UART_CONTROL_H

#ifndef NX_UART_REG_WRITE
#define NX_UART_REG_WRITE(address, value) (*(address) = (uint32_t)(value))
#endif
#ifndef NX_UART_IRQ_ENABLE
#if defined(NEXUS_STM32_MODEL)
#define NX_UART_IRQ_ENABLE(irq)              ((void)(irq))
#define NX_UART_IRQ_DISABLE(irq)             ((void)(irq))
#define NX_UART_IRQ_CLEAR_PENDING(irq)       ((void)(irq))
#define NX_UART_IRQ_SET_PRIORITY(irq, value) ((void)(irq), (void)(value))
#define NX_UART_IRQ_ENABLED(irq)             (0U)
#define NX_UART_IRQ_PENDING(irq)             (0U)
#define NX_UART_IRQ_ACTIVE(irq)              (0U)
#define NX_UART_IRQ_PRIORITY(irq)            (5U)
#else
#define NX_UART_IRQ_ENABLE(irq)        NVIC_EnableIRQ((IRQn_Type)(irq))
#define NX_UART_IRQ_DISABLE(irq)       NVIC_DisableIRQ((IRQn_Type)(irq))
#define NX_UART_IRQ_CLEAR_PENDING(irq) NVIC_ClearPendingIRQ((IRQn_Type)(irq))
#define NX_UART_IRQ_SET_PRIORITY(irq, value)                                   \
    NVIC_SetPriority((IRQn_Type)(irq), (uint32_t)(value))
/* The locked GD32 CMSIS predates NVIC_GetEnableIRQ. ISER is the architectural
 * enable readback, independent of pending or active status. Callers validate
 * the external IRQ before reaching this leaf. */
#define NX_UART_IRQ_ENABLED(irq)                                               \
    ((NVIC->ISER[(uint32_t)(irq) / 32U] >> ((uint32_t)(irq) % 32U)) & 1U)
#define NX_UART_IRQ_PENDING(irq)  NVIC_GetPendingIRQ((IRQn_Type)(irq))
#define NX_UART_IRQ_ACTIVE(irq)   NVIC_GetActive((IRQn_Type)(irq))
#define NX_UART_IRQ_PRIORITY(irq) NVIC_GetPriority((IRQn_Type)(irq))
#endif
#endif
#if defined(NEXUS_STM32_MODEL) && !defined(NX_UART_MODEL_IRQ)
/* Legacy register fixtures do not model NVIC; physical checks stay in firmware.
 * The focused UART external-port fixture supplies actual modeled IRQ facts.
 */
#define NX_UART_IRQ_QUIET(irq)                (true)
#define NX_UART_IRQ_CONFIGURED(irq, priority) (true)
#else
#define NX_UART_IRQ_QUIET(irq)                                                 \
    (NX_UART_IRQ_ENABLED(irq) == 0U && NX_UART_IRQ_PENDING(irq) == 0U &&       \
     NX_UART_IRQ_ACTIVE(irq) == 0U)
#define NX_UART_IRQ_CONFIGURED(irq, priority)                                  \
    (NX_UART_IRQ_ENABLED(irq) != 0U && NX_UART_IRQ_PENDING(irq) == 0U &&       \
     NX_UART_IRQ_PRIORITY(irq) == (uint32_t)(priority))
#endif
#ifndef NX_UART_CLOCK_ON
#define NX_UART_CLOCK_ON(clock)                                                \
    rcu_periph_clock_enable((rcu_periph_enum)(clock))
#define NX_UART_CLOCK_OFF(clock)                                               \
    rcu_periph_clock_disable((rcu_periph_enum)(clock))
#endif
#endif
