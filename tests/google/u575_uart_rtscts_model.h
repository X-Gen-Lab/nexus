/**
 * \file            u575_uart_rtscts_model.h
 * \brief           hardware FIFO bus ports, never provider algorithms
 * \author          Nexus Team
 */
#ifndef NEXUS_U575_UART_RTSCTS_MODEL_H
#define NEXUS_U575_UART_RTSCTS_MODEL_H
#include "stm32u5xx.h"
#include "u575_model.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t rx[9];
    uint32_t rx_error[9];
    uint8_t tx[8];
    uint8_t received;
    uint8_t queued;
    uint32_t rdr_reads;
    uint32_t tdr_writes;
    uint32_t isr_reads;
    uint32_t uart_reads;
    uint32_t uart_writes;
    uint32_t tx_flush_writes;
    uint32_t empty_reads;
    uint32_t full_writes;
    uint32_t forbidden_config;
    uint32_t lost_frames;
    uint32_t wire_frames;
    uint32_t saved_flags;
    uint32_t reject_clear;
    uint32_t reject_set;
    uint32_t icr_writes;
    uint32_t flush_writes;
    uint32_t irq_enables;
    bool hold_priority;
    bool hold_pending;
    volatile uint32_t* reject_register;
    volatile uint32_t* reject_bits_register;
    bool shift_busy;
    bool cts_high;
    uint32_t blocked_shifts;
    uint32_t peer_blocked_frames;
    void (*before_read)(volatile uint32_t* reg);
    void (*before_write)(volatile uint32_t* reg, uint32_t value);
} nx_u575_uart_rtscts_model_t;
extern nx_u575_uart_rtscts_model_t g_u575_uart_rtscts;
void nx_u575_uart_rtscts_model_reset(void);
uint32_t nx_u575_uart_rtscts_model_read(volatile uint32_t* reg);
void nx_u575_uart_rtscts_model_write(volatile uint32_t* reg, uint32_t value);
/** \brief Hardware-only arrivals/wire events, never request-state access. */
void nx_u575_uart_rtscts_model_receive(uint8_t value, uint32_t errors);
void nx_u575_uart_rtscts_model_idle(void);
nx_result_t nx_u575_uart_rtscts_model_irq_control(int irq, bool enabled,
                                                  uint8_t priority);
void nx_u575_uart_rtscts_model_shift(void);
void nx_u575_uart_rtscts_model_finish_frame(void);
/** \brief Physical input level; ISR.CTS is the inverted copy, not a policy. */
void nx_u575_uart_rtscts_model_cts(bool high);
/** \brief Query live RTSE+UE+RE only; no reset/AF electrical guarantee. */
bool nx_u575_uart_rtscts_model_rts_high(void);
/** \brief Start a new peer frame only if live hardware RTS currently permits.
 */
bool nx_u575_uart_rtscts_model_peer_receive(uint8_t value, uint32_t errors);

#define NX_U575_UART_FIFO_IRQ_ENABLED(irq) (g_u575_model.irq_enabled[(irq)])
#define NX_U575_UART_FIFO_IRQ_PENDING(irq) (g_u575_model.irq_pending[(irq)])
#define NX_U575_UART_FIFO_IRQ_ACTIVE(irq)  (g_u575_model.irq_active[(irq)])
#undef NX_U575_IRQ_CONTROL
#define NX_U575_IRQ_CONTROL(irq, enabled, priority)                            \
    nx_u575_uart_rtscts_model_irq_control((irq), (enabled), (priority))
#undef NX_U575_READ
#undef NX_U575_WRITE
#define NX_U575_READ(reg) nx_u575_uart_rtscts_model_read(reg)
#define NX_U575_WRITE(reg, value)                                              \
    nx_u575_uart_rtscts_model_write((reg), (value))
/* Cold HW-flow setters use the pinned LL function too. Only register RMW
 * is external, never the provider admission/pause/resume algorithm. */
#undef MODIFY_REG
#define MODIFY_REG(reg, clear, set)                                            \
    NX_U575_WRITE(&(reg), (NX_U575_READ(&(reg)) & ~(uint32_t)(clear)) |        \
                              (uint32_t)(set))
/* Selected production defaults use pinned LL_USART inline leaves. These
 * overrides preserve one-register bus effects while keeping hardware queues
 * external; they never replace the provider FIFO loop or request state. */
#define NX_U575_UART_FIFO_ENABLE(uart)                                         \
    NX_U575_WRITE(&(uart)->CR1, NX_U575_READ(&(uart)->CR1) | USART_CR1_FIFOEN)
/* Use the pinned LL threshold routines, including their unshifted argument
 * contract. Only their cold, exclusively owned register RMW is external. */
#undef ATOMIC_SET_BIT
#define ATOMIC_SET_BIT(reg, bits)                                              \
    nx_u575_uart_rtscts_model_write(                                           \
        &(reg), nx_u575_uart_rtscts_model_read(&(reg)) | (uint32_t)(bits))
#undef ATOMIC_CLEAR_BIT
#define ATOMIC_CLEAR_BIT(reg, bits)                                            \
    nx_u575_uart_rtscts_model_write(                                           \
        &(reg), nx_u575_uart_rtscts_model_read(&(reg)) & ~(uint32_t)(bits))
#undef ATOMIC_MODIFY_REG
#define ATOMIC_MODIFY_REG(reg, clear, set)                                     \
    nx_u575_uart_rtscts_model_write(                                           \
        &(reg),                                                                \
        (nx_u575_uart_rtscts_model_read(&(reg)) & ~(uint32_t)(clear)) |        \
            (uint32_t)(set))
#define NX_U575_UART_FIFO_RECEIVE(uart) ((uint8_t)NX_U575_READ(&(uart)->RDR))
#define NX_U575_UART_FIFO_TRANSMIT(uart, value)                                \
    NX_U575_WRITE(&(uart)->TDR, (uint32_t)(uint8_t)(value))
#ifdef __cplusplus
}
#endif
#endif
