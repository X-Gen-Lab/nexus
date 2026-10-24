/**
 * \file            stm32u575_uart_rtscts.h
 * \brief           Exact optional FIFO 8N1 hardware RTS and CTS owner
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32U575_UART_RTSCTS_H
#define NEXUS_STM32U575_UART_RTSCTS_H
#include "stm32u575_uart_fifo.h"

/** \brief Same selected FIFO storage; no flow flags or worker allocation. */
typedef nx_stm32u575_uart_fifo_state_t nx_stm32u575_uart_rtscts_state_t;
#ifdef __cplusplus
extern "C" {
#endif
extern const nx_uart_ops_t nx_stm32u575_uart_rtscts_ops;
/**
 * \brief           Prepare the exact PA9/PA10/PA11/PA12 AF7 software route.
 * \return          SUCCESS after four-pin readback; CONTEXT/STATE/BUSY/IO
 *                  otherwise.
 * \note            Task-only; generated cold GPIO/gate snapshots are owned
 *                  before this call and restored only after verified stop.
 */
nx_result_t nx_stm32u575_usart1_rtscts_pins_prepare(void);
/**
 * \brief           Cold exact-controller 8N1, FIFO, RTS+CTS initialization.
 * \param[in,out]   port: Zeroed idle selected state, fixed FIFO profile/ring.
 * \param[in]       clock: Existing exact baseline peripheral rate.
 * \param[in]       limit: Finite UE/TEACK/REACK proof bound.
 * \return          SUCCESS only after flow/frame/source/IRQ readback.
 * \note            Task-only with the separately acquired reviewed four-pin
 *                  route. Live changes are forbidden. Failure after effects
 *                  retains uart.retained until successful public stop. CTS
 *                  blocking can retain an accepted TX indefinitely: cancel,
 *                  deadline and QUARANTINED do not revoke its borrow.
 */
nx_result_t
nx_stm32u575_uart_rtscts_initialize(nx_stm32u575_uart_rtscts_state_t* port,
                                    uint32_t clock, uint32_t limit);
/** \brief Direct bounded vector; only task service can settle TX storage. */
void nx_stm32u575_uart_rtscts_irq(nx_stm32u575_uart_rtscts_state_t* port);
#ifdef __cplusplus
}
#endif
#endif
