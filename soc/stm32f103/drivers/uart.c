/**
 * \file            uart.c
 * \brief           F103 USART2 IRQ bytes,real TC and explicit caller ring
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f103_provider.h"
#include "stm32f1xx_ll_usart.h"
#include <limits.h>
#ifndef NX_F103_UART_TRANSMIT
#define NX_F103_UART_TRANSMIT(regs, byte) LL_USART_TransmitData8(regs, byte)
#define NX_F103_UART_RECEIVE(regs)        LL_USART_ReceiveData8(regs)
#endif
#ifndef NX_F103_IRQ_AVAILABLE
#define NX_F103_IRQ_AVAILABLE(irq)                                             \
    (NVIC_GetEnableIRQ((IRQn_Type)(irq)) == 0U &&                              \
     NVIC_GetPendingIRQ((IRQn_Type)(irq)) == 0U &&                             \
     NVIC_GetActive((IRQn_Type)(irq)) == 0U)
#endif
#define TX_SOURCES ((uint32_t)(USART_CR1_TXEIE | USART_CR1_TCIE))
#define RX_ERRORS                                                              \
    ((uint32_t)(USART_SR_PE | USART_SR_FE | USART_SR_NE | USART_SR_ORE))
static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool span(const void* p, size_t size) {
    return p != NULL && size != 0U && (uintptr_t)p <= UINTPTR_MAX - size;
}
static bool overlap(const void* a, size_t as, const void* b, size_t bs) {
    return (uintptr_t)a < (uintptr_t)b + bs && (uintptr_t)b < (uintptr_t)a + as;
}
static bool protected_span(const nx_stm32f103_uart_state_t* p, const void* data,
                           size_t size) {
    return overlap(data, size, p, sizeof(*p)) ||
           overlap(data, size, p->registers, sizeof(*p->registers)) ||
           overlap(data, size, p->rx_storage, p->rx_capacity) ||
           (p->wake != NULL && overlap(data, size, p->wake, sizeof(*p->wake)));
}
static bool output_alias(const nx_stm32f103_uart_state_t* p, const void* data,
                         size_t size) {
    return !span(data, size) || protected_span(p, data, size) ||
           (p->active != NULL &&
            (overlap(data, size, p->active, sizeof(*p->active)) ||
             overlap(data, size, p->active->data, p->active->length)));
}
static bool write_checked(volatile uint32_t* reg, uint32_t value,
                          uint32_t mask) {
    NX_F103_WRITE(reg, value);
    nx_arch_dsb();
    return (NX_F103_READ(reg) & mask) == (value & mask);
}
static void loss(nx_stm32f103_uart_state_t* p) {
    if (p->rx_loss != UINT32_MAX)
        p->rx_loss++;
}
static void begin_drain(nx_stm32f103_uart_state_t* p, nx_result_t terminal,
                        nx_time_us_t now) {
    if (p->tx_complete)
        return;
    if (p->terminal == NX_SUCCESS) {
        p->terminal = terminal;
        p->drain_deadline = nx_deadline_after(now, 20000000U / p->baud + 100U);
        (void)nx_request_transition(&p->active->base, NX_REQUEST_DRAINING);
    }
    NX_F103_WRITE(&p->registers->CR1, (NX_F103_READ(&p->registers->CR1) &
                                       ~(uint32_t)USART_CR1_TXEIE) |
                                          USART_CR1_TCIE);
    if (p->tx_position == 0U ||
        (NX_F103_READ(&p->registers->SR) & USART_SR_TC) != 0U)
        p->tx_complete = true;
}
static void observe_tc(nx_stm32f103_uart_state_t* p, nx_time_us_t now) {
    if (p->tx_complete || p->active == NULL)
        return;
    if ((p->tx_position == p->active->length || p->terminal != NX_SUCCESS) &&
        (NX_F103_READ(&p->registers->SR) & USART_SR_TC) != 0U) {
        if (p->terminal == NX_SUCCESS &&
            nx_deadline_expired(p->active->base.deadline, now))
            p->terminal = NX_ERROR_TIMEOUT;
        p->tx_complete = true;
        NX_F103_WRITE(&p->registers->CR1,
                      NX_F103_READ(&p->registers->CR1) & ~TX_SOURCES);
    }
}
static void service(void* context) {
    nx_stm32f103_uart_state_t* p = context;
    if (!task_context() || p == NULL)
        return;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    nx_uart_tx_request_t* request = p->active;
    if (request == NULL) {
        nx_arch_irq_restore(guard);
        return;
    }
    nx_time_us_t now = nx_time_now_us();
    observe_tc(p, now);
    if (!p->tx_complete && p->terminal == NX_SUCCESS) {
        if (nx_deadline_expired(request->base.deadline, now))
            begin_drain(p, NX_ERROR_TIMEOUT, now);
    }
    if (p->tx_complete) {
        if (!write_checked(&p->registers->CR1,
                           NX_F103_READ(&p->registers->CR1) & ~TX_SOURCES,
                           TX_SOURCES)) {
            (void)nx_request_transition(&request->base, NX_REQUEST_QUARANTINED);
            nx_arch_irq_restore(guard);
            return;
        }
        nx_result_t terminal = p->terminal;
        size_t transferred = p->tx_position;
        p->active = NULL;
        nx_request_settle(&request->base, terminal, transferred);
    } else if (p->terminal != NX_SUCCESS &&
               nx_deadline_expired(p->drain_deadline, now)) {
        (void)nx_request_transition(&request->base, NX_REQUEST_QUARANTINED);
    }
    nx_arch_irq_restore(guard);
}
static nx_result_t start(void* context, nx_uart_tx_request_t* request,
                         bool admitted) {
    nx_stm32f103_uart_state_t* p = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (p == NULL || request == NULL || !span(request, sizeof(*request)) ||
        !span(request->data, request->length))
        return NX_ERROR_INVALID;
    if (protected_span(p, request, sizeof(*request)) ||
        protected_span(p, request->data, request->length) ||
        overlap(request, sizeof(*request), request->data, request->length))
        return NX_ERROR_INVALID;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!p->initialized || p->closing) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_STATE;
    }
    if (p->active != NULL) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_BUSY;
    }
    if (nx_deadline_expired(request->base.deadline, nx_time_now_us())) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_TIMEOUT;
    }
    nx_result_t result =
        admitted ? nx_request_transition(&request->base, NX_REQUEST_ACTIVE)
                 : nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result == NX_SUCCESS) {
        p->active = request;
        p->tx_position = 0U;
        p->tx_complete = false;
        p->terminal = NX_SUCCESS;
        p->drain_deadline = NX_DEADLINE_NEVER;
        NX_F103_WRITE(&p->registers->SR, ~(uint32_t)USART_SR_TC);
        NX_F103_WRITE(&p->registers->CR1,
                      NX_F103_READ(&p->registers->CR1) | USART_CR1_TXEIE);
        nx_arch_dsb();
        if ((NX_F103_READ(&p->registers->CR1) & USART_CR1_TXEIE) == 0U)
            begin_drain(p, NX_ERROR_IO, nx_time_now_us());
    }
    nx_arch_irq_restore(guard);
    return result;
}
static nx_result_t submit(void* p, nx_uart_tx_request_t* request) {
    return start(p, request, false);
}
static nx_result_t start_admitted(void* p, nx_uart_tx_request_t* request) {
    if (request == NULL ||
        nx_request_state(&request->base) != NX_REQUEST_QUEUED)
        return NX_ERROR_STATE;
    return start(p, request, true);
}
static nx_result_t cancel(void* context, nx_uart_tx_request_t* request) {
    nx_stm32f103_uart_state_t* p = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (p == NULL || request == NULL)
        return NX_ERROR_INVALID;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (p->active != request) {
        nx_result_t result =
            nx_request_state(&request->base) == NX_REQUEST_SETTLED
                ? NX_SUCCESS
                : NX_ERROR_STATE;
        nx_arch_irq_restore(guard);
        return result;
    }
    nx_time_us_t now = nx_time_now_us();
    observe_tc(p, now);
    if (!p->tx_complete) {
        begin_drain(p, NX_ERROR_CANCELLED, now);
    }
    nx_arch_irq_restore(guard);
    return NX_SUCCESS;
}
void nx_stm32f103_uart_irq(nx_stm32f103_uart_state_t* p) {
    if (p == NULL)
        return;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!p->initialized) {
        nx_arch_irq_restore(guard);
        return;
    }
    nx_time_us_t now = nx_time_now_us();
    uint32_t status = NX_F103_READ(&p->registers->SR);
    bool loaded = false;
    if (p->active != NULL && !p->tx_complete) {
        if (p->terminal == NX_SUCCESS &&
            nx_deadline_expired(p->active->base.deadline, now))
            begin_drain(p, NX_ERROR_TIMEOUT, now);
        if (p->terminal == NX_SUCCESS && (status & USART_SR_TXE) != 0U &&
            (NX_F103_READ(&p->registers->CR1) & USART_CR1_TXEIE) != 0U &&
            p->tx_position < p->active->length) {
            NX_F103_UART_TRANSMIT(p->registers,
                                  p->active->data[p->tx_position++]);
            loaded = true;
            if (p->tx_position == p->active->length)
                NX_F103_WRITE(&p->registers->CR1,
                              (NX_F103_READ(&p->registers->CR1) &
                               ~(uint32_t)USART_CR1_TXEIE) |
                                  USART_CR1_TCIE);
        }
        if (!loaded)
            observe_tc(p, now);
    }
    uint32_t errors = status & RX_ERRORS;
    if ((status & USART_SR_RXNE) != 0U) {
        uint8_t byte = NX_F103_UART_RECEIVE(p->registers);
        if (p->closing || errors != 0U || p->rx_count == p->rx_capacity)
            loss(p);
        else {
            ((uint8_t*)p->rx_storage)[p->rx_head] = byte;
            if (++p->rx_head == p->rx_capacity)
                p->rx_head = 0U;
            p->rx_count++;
        }
    } else if (errors != 0U)
        loss(p);
    if ((status & USART_SR_RXNE) == 0U && errors != 0U)
        (void)NX_F103_UART_RECEIVE(p->registers);
    const nx_irq_wake_t* wake = p->wake;
    if (wake != NULL && p->wake_publishers != UINT16_MAX)
        p->wake_publishers++;
    else
        wake = NULL;
    nx_arch_irq_restore(guard);
    (void)nx_irq_wake_signal(wake);
    if (wake != NULL) {
        guard = nx_arch_irq_save();
        p->wake_publishers--;
        nx_arch_irq_restore(guard);
    }
}
static nx_result_t read_bytes(void* context, uint8_t* output, size_t capacity,
                              size_t* count) {
    nx_stm32f103_uart_state_t* p = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (p == NULL || capacity == 0U || output_alias(p, output, capacity) ||
        output_alias(p, count, sizeof(*count)) ||
        overlap(output, capacity, count, sizeof(*count)))
        return NX_ERROR_INVALID;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!p->initialized) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_STATE;
    }
    nx_arch_irq_restore(guard);
    size_t copied = 0U;
    while (copied < capacity) {
        guard = nx_arch_irq_save();
        if (p->rx_count == 0U) {
            nx_arch_irq_restore(guard);
            break;
        }
        /* One ring byte and its cursors per critical section. Output memory
         * is caller-owned; copying it does not need to delay hardware IRQs. */
        uint8_t byte = ((uint8_t*)p->rx_storage)[p->rx_tail];
        if (++p->rx_tail == p->rx_capacity)
            p->rx_tail = 0U;
        p->rx_count--;
        nx_arch_irq_restore(guard);
        output[copied++] = byte;
    }
    guard = nx_arch_irq_save();
    bool lost = p->rx_loss != 0U;
    p->rx_loss = 0U;
    nx_arch_irq_restore(guard);
    *count = copied;
    return lost ? NX_ERROR_OVERFLOW
                : (copied != 0U ? NX_SUCCESS : NX_ERROR_EMPTY);
}
static nx_result_t stop(void* context) {
    nx_stm32f103_uart_state_t* p = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (p == NULL || p->registers == NULL)
        return NX_ERROR_INVALID;
    if (!p->retained)
        return NX_SUCCESS;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    p->closing = true;
    if (p->active != NULL && !p->tx_complete)
        begin_drain(p, NX_ERROR_CANCELLED, nx_time_now_us());
    nx_arch_irq_restore(guard);
    service(p);
    guard = nx_arch_irq_save();
    bool busy = p->active != NULL || p->wake_publishers != 0U;
    nx_arch_irq_restore(guard);
    if (busy)
        return NX_ERROR_BUSY;
    if (!write_checked(&p->registers->CR1, 0U, UINT32_MAX) ||
        !write_checked(&p->registers->CR3, 0U, UINT32_MAX))
        return NX_ERROR_IO;
    nx_result_t irq = NX_F103_IRQ_CONTROL(p->irq, false, 5U);
    if (irq != NX_SUCCESS)
        return irq;
    guard = nx_arch_irq_save();
    if (p->active != NULL || p->wake_publishers != 0U) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_BUSY;
    }
    p->initialized = false;
    p->wake = NULL;
    p->retained = false;
    (void)NX_F103_READ(&p->registers->SR);
    (void)NX_F103_UART_RECEIVE(p->registers);
    nx_arch_irq_restore(guard);
    return NX_SUCCESS;
}
static nx_result_t attach_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t ceiling) {
    nx_stm32f103_uart_state_t* p = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (p == NULL)
        return NX_ERROR_INVALID;
    if (wake != NULL && wake != p->wake && output_alias(p, wake, sizeof(*wake)))
        return NX_ERROR_INVALID;
    if (ceiling != p->irq_policy.syscall_ceiling)
        return NX_ERROR_PERMISSION;
    nx_irq_source_t source = {(int16_t)p->irq,
                              (uint8_t)NX_F103_IRQ_PRIORITY(p->irq),
                              (uint8_t)NX_F103_IRQ_GROUP()};
    nx_result_t result = nx_irq_wake_validate(wake, &p->irq_policy, &source);
    if (result != NX_SUCCESS)
        return result;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!p->initialized || p->closing || p->active != NULL ||
        p->wake_publishers != 0U)
        result = NX_ERROR_BUSY;
    else
        p->wake = wake;
    nx_arch_irq_restore(guard);
    return result;
}
nx_result_t nx_stm32f103_uart_initialize(nx_stm32f103_uart_state_t* p,
                                         uint32_t clock, uint32_t limit) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (p == NULL || p->registers == NULL || p->baud == 0U || limit == 0U ||
        !span(p->rx_storage, p->rx_capacity))
        return NX_ERROR_INVALID;
    if (p->profile != NX_UART_RX_BYTES || clock != 8000000U)
        return NX_ERROR_UNSUPPORTED;
    if (p->irq != USART2_IRQn)
        return NX_ERROR_UNSUPPORTED;
    if (p->initialized || p->retained || p->active != NULL || p->wake != NULL ||
        NX_F103_READ(&p->registers->CR1) != 0U ||
        NX_F103_READ(&p->registers->CR3) != 0U ||
        !NX_F103_IRQ_AVAILABLE(p->irq))
        return NX_ERROR_BUSY;
    if (!g_nx_stm32f103_system.started ||
        g_nx_stm32f103_system.core_hz != clock ||
        (NX_F103_READ(&g_nx_stm32f103_system.rcc->APB1ENR) &
         RCC_APB1ENR_USART2EN) == 0U)
        return NX_ERROR_STATE;
    if (overlap(p->rx_storage, p->rx_capacity, p, sizeof(*p)) ||
        overlap(p->rx_storage, p->rx_capacity, p->registers,
                sizeof(*p->registers)))
        return NX_ERROR_INVALID;
    nx_irq_source_t source = {(int16_t)p->irq, 5U,
                              (uint8_t)NX_F103_IRQ_GROUP()};
    nx_result_t policy = nx_irq_wake_validate(NULL, &p->irq_policy, &source);
    if (policy != NX_SUCCESS)
        return policy;
    uint32_t divisor = (clock + p->baud / 2U) / p->baud;
    if (divisor < 16U || divisor > 0xffffU)
        return NX_ERROR_UNSUPPORTED;
    p->retained = true;
    p->closing = false;
    p->tx_position = 0U;
    p->tx_complete = false;
    p->rx_head = p->rx_tail = p->rx_count = 0U;
    p->rx_loss = 0U;
    p->wake_publishers = 0U;
    bool valid =
        write_checked(&p->registers->CR2, 0U, UINT32_MAX) &&
        write_checked(&p->registers->CR3, USART_CR3_EIE, UINT32_MAX) &&
        write_checked(&p->registers->BRR, (uint32_t)divisor, UINT32_MAX);
    (void)NX_F103_READ(&p->registers->SR);
    (void)NX_F103_UART_RECEIVE(p->registers);
    valid = write_checked(&p->registers->CR1,
                          USART_CR1_UE | USART_CR1_TE | USART_CR1_RE |
                              USART_CR1_RXNEIE | USART_CR1_PEIE,
                          UINT32_MAX) &&
            valid;
    if (!valid) {
        p->closing = true;
        (void)stop(p);
        return NX_ERROR_IO;
    }
    p->initialized = true;
    nx_result_t result = NX_F103_IRQ_CONTROL(p->irq, true, 5U);
    if (result == NX_SUCCESS)
        return NX_SUCCESS;
    p->closing = true;
    (void)stop(p);
    return result;
}
const nx_uart_ops_t nx_stm32f103_uart_ops = {
    submit,     start_admitted, cancel,      service, NULL,
    read_bytes, stop,           attach_wake, NULL,    NULL};
