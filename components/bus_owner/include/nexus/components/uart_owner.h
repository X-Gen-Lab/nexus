/**
 * \file            uart_owner.h
 * \brief           Explicit IRQ UART executor with private provider settlement
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_UART_OWNER_H
#define NEXUS_COMPONENTS_UART_OWNER_H
#include "nexus/components/bus_owner.h"
#include "nexus/io/uart.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Caller-owned TX payload descriptor borrowed until outer SETTLED. */
typedef struct {
    const uint8_t* data;
    size_t length;
} nx_uart_owner_operation_t;

/**
 * \brief           One executor's exact private provider request
 * \note            inner is a separate publication object, not another outer
 *                  queue or caller admission. Only this executor accesses it;
 *                  producers retain their outer request and payload until the
 *                  bus owner publishes outer SETTLED. No buffer copy, heap,
 *                  worker or pool. Never pass inner to application observers.
 */
typedef struct {
    const nx_uart_port_t* port;
    nx_uart_tx_request_t inner;
    bool pending;
} nx_uart_owner_executor_t;

/**
 * \brief           Bind a UART controller to one explicit service executor
 * \param[in,out]   executor: Unused caller-owned storage, no concurrent calls
 * \param[in,out]   uart: Initialized UART exclusively owned by this executor
 * \return          Port for nx_bus_owner_init, or an invalid zero port for NULL
 * \note            Startup/task context. Controller and executor remain alive
 *                  through owner stop/drain and external observer quiescence.
 *                  Direct UART users must not share this controller. Queued
 *                  time belongs to the bus owner; after start the UART's first
 *                  terminal TC/error/cancel/deadline fact is authoritative.
 *                  cancel observes provider completion/deadline before asking
 *                  it to abort. Failed drain keeps both requests QUARANTINED;
 *                  service must continue during recovery. Outer SETTLED never
 *                  precedes inner acquire-observed SETTLED and pointer detach.
 *                  The application separately stops hardware and RX consumers
 *                  only after all owner operations and controls quiesce.
 */
nx_owner_executor_port_t
nx_uart_owner_executor_port(nx_uart_owner_executor_t* executor,
                            const nx_uart_port_t* uart);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_COMPONENTS_UART_OWNER_H */
