/**
 * \file            irq_probe.c
 * \brief           Internal exact-provider fixture; absent from normal images.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/cycle_probe.h"
#if defined(FIFO_FIXTURE_H743)
#include "stm32h743_uart_fifo.h"
#define FIFO_STATE nx_stm32h743_uart_fifo_state_t
#define FIFO_REAL  __real_nx_stm32h743_uart_fifo_irq
#define FIFO_WRAP  __wrap_nx_stm32h743_uart_fifo_irq
#elif defined(FIFO_FIXTURE_U575)
#include "stm32u575_uart_fifo.h"
#define FIFO_STATE nx_stm32u575_uart_fifo_state_t
#define FIFO_REAL  __real_nx_stm32u575_uart_fifo_irq
#define FIFO_WRAP  __wrap_nx_stm32u575_uart_fifo_irq
#else
#error "This internal fixture requires one exact FIFO provider"
#endif

/* This sidecar belongs to this one physical vector. The station reads it only
 * after the complete Platform/IRQ join. It contains raw modulo facts, never
 * IRQ arrival latency or a claimed elapsed cycle count. */
nx_io_cycle_probe_t g_fifo_fixture_irq_probe;

void FIFO_REAL(FIFO_STATE* state);
void FIFO_WRAP(FIFO_STATE* state);

#if defined(FIFO_FIXTURE_REVIEWED_MARKERS)
/* No weak/no-op default and no selected GPIO pin. A station supplies these
 * bounded, normally returning leaves after electrical/routing review. No
 * printf/stdio/semihosting, allocation, locks, waits, provider calls or
 * recursion are permitted. A faulting/hanging arbitrary leaf cannot preserve
 * dispatch; exactly-once dispatch assumes these explicit station preconditions.
 */
void fifo_station_dispatch_enter(void);
void fifo_station_dispatch_leave(void);
#endif

/** \brief Observe the provider-dispatch envelope and call the real IRQ once. */
void FIFO_WRAP(FIFO_STATE* state) {
    nx_io_cycle_token_t token = {NULL, 0U, false};
    (void)nx_io_cycle_probe_begin(&g_fifo_fixture_irq_probe, &token);
#if defined(FIFO_FIXTURE_REVIEWED_MARKERS)
    fifo_station_dispatch_enter();
#endif
    FIFO_REAL(state);
#if defined(FIFO_FIXTURE_REVIEWED_MARKERS)
    fifo_station_dispatch_leave();
#endif
    (void)nx_io_cycle_probe_end(&g_fifo_fixture_irq_probe, &token);
}
