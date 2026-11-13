/**
 * \file            stm32f4_uart.h
 * \brief           Private STM32F4 byte UART state and transport methods
 * \author          Nexus Team
 */
#ifndef NX_STM32F4_UART_H
#define NX_STM32F4_UART_H

#include "nexus/arch/arch.h"
#include "nexus/io/uart.h"
#include "nexus/io/uart_format.h"
#include "stm32f4_irq_wake.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    USART_TypeDef* registers;
    const nx_irq_wake_t* wake;
    nx_uart_tx_request_t* active;
    size_t tx_position;
    void* rx_storage;
    size_t rx_capacity;
    size_t rx_head;
    size_t rx_tail;
    size_t rx_count;
    uint32_t rx_loss;
    uint32_t baud;
    nx_time_us_t drain_deadline;
    nx_result_t terminal;
    nx_uart_rx_profile_t profile;
    IRQn_Type irq;
    bool initialized;
    bool closing;
    bool tx_complete;
} nx_stm32_uart_state_t;

/** \brief Configure reviewed USART1 PA9/PA10 AF7 and peripheral clocks. */
nx_result_t nx_stm32_uart1_pins_prepare(void);
/** \brief Initialize one fixed 8N1 UART; storage outlives IRQ and stop.
 * \param[in,out] port: Fresh zeroed state with selected config filled in.
 * \note Live or retained ownership rejects reinitialization. Reuse after
 *       successful stop; never zero state to revoke a live borrow.
 */
nx_result_t nx_stm32_uart_initialize(nx_stm32_uart_state_t* port,
                                     uint32_t clock_hz);
/** \brief Cold selected frame is installed before UE and IRQ publication. */
nx_result_t nx_stm32_uart_initialize_format(nx_stm32_uart_state_t* port,
                                            uint32_t clock_hz,
                                            nx_uart_format_t format);
/** \brief Apply a selected byte format only under quiescent cold ownership.
 * \param[in,out] state: Initialized byte UART; no TX/RX loan or queued RX.
 * \param[in] format: 7E/O or 8N/E/O with one/two stop bits and no flow or CTS.
 * \return Success or INVALID/UNSUPPORTED/CONTEXT/STATE/BUSY before writes;
 *         IO attempts rollback after failed register readback. If rollback
 *         cannot be proven, initialized remains true and closing rejects new
 *         work. Retain pins, clock and storage until stop proves withdrawal.
 * \note Startup task must quiesce the peer. Seven-bit receive uses the separate
 *         format_irq entry. Format selection adds no default state or ops slot.
 *         Existing or requested RTS is UNSUPPORTED without effects: this helper
 *         has no pad owner for the ES0182 2.11.9 UE withdrawal sequence. Select
 *         RTS through the static wire cold owner instead.
 */
nx_result_t nx_stm32_uart_format_configure(nx_stm32_uart_state_t* state,
                                           nx_uart_format_t format);
/** \brief IRQ entry for explicitly selected seven/eight-bit parity formats.
 * \param[in,out] port: Exact live binding; preserves RX parity/error facts.
 */
void nx_stm32_uart_format_irq(nx_stm32_uart_state_t* port);

/**
 * \brief           Select independent RTS/CTS only on an idle live controller.
 * \param[in,out]   state: Startup-controlled UART, no active TX or RX DMA.
 * \param[in]       rts: Actual hardware RTS output enable.
 * \param[in]       cts: Actual hardware CTS transmit gate enable.
 * \return          Success or INVALID/STATE/CONTEXT/BUSY before any write.
 * \note            Board owns AF pins/wiring. RTS covers hardware receive
 *                  register capacity, not application stream/ring backpressure.
 *                  Default state has no extra RAM; other CR3 bits are
 * preserved.
 */
nx_result_t nx_stm32_uart_flow_configure(nx_stm32_uart_state_t* state, bool rts,
                                         bool cts);

/** \brief Bounded USART IRQ dispatch for the statically bound instance. */
void nx_stm32_uart_irq(nx_stm32_uart_state_t* port);
/** \brief Advance only TX facts when an independent provider owns RX storage.
 */
bool nx_stm32_uart_tx_irq(nx_stm32_uart_state_t* port, uint32_t status);
/** \brief Consume one RX observation independently of any TX transport.
 * \param[in,out] port: Live byte/event receive owner and its fixed ring.
 * \param[in] status: One retained SR observation before the DR read.
 * \param[in] payload_mask: Exact selected frame payload width.
 * \return True when the owner observed receive or loss information.
 * \note Serialized UART IRQ only; this helper never dispatches a wake sink.
 */
bool nx_stm32_uart_rx_irq(nx_stm32_uart_state_t* port, uint32_t status,
                          uint8_t payload_mask);
extern const nx_uart_ops_t nx_stm32_uart_ops;
nx_result_t nx_stm32_uart_submit(void* context, nx_uart_tx_request_t* request);
nx_result_t nx_stm32_uart_start_admitted(void* context,
                                         nx_uart_tx_request_t* request);
nx_result_t nx_stm32_uart_cancel(void* context, nx_uart_tx_request_t* request);
void nx_stm32_uart_service(void* context);
nx_result_t nx_stm32_uart_read_events(void* context, nx_uart_rx_event_t* events,
                                      size_t capacity, size_t* count);
nx_result_t nx_stm32_uart_read_bytes(void* context, uint8_t* bytes,
                                     size_t capacity, size_t* count);
nx_result_t nx_stm32_uart_stop(void* context);
/**
 * \brief           Side-effect-free checks shared with this exact TX start.
 * \param[in]       context: Exact immutable face's selected provider state.
 * \param[in]       request: Live descriptor, not admitted or retained here.
 * \return          Same payload/domain/context/state/deadline rejection.
 * \note            Task-only; reads facts but performs no MMIO store or borrow.
 *                  RS485 binding must use this provider's matching validator.
 */
nx_result_t nx_stm32_uart_tx_validate(void* context,
                                      const nx_uart_tx_request_t* request);

#ifdef __cplusplus
}
#endif
#endif
