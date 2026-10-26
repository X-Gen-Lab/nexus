/**
 * \file            uart_v1_channel_dma.h
 * \brief           Private legacy USART and fixed-channel DMA ownership
 * \author          Nexus Team
 */
#ifndef NX_UART_V1_CHANNEL_DMA_H
#define NX_UART_V1_CHANNEL_DMA_H
#include "nexus/io/dma.h"
#include "nexus/io/uart.h"

/** \brief SDK-asserted scalar layout; no vendor types enter public faces. */
typedef struct {
    volatile uint32_t status;
    volatile uint32_t data;
    volatile uint32_t baud;
    volatile uint32_t control1;
    volatile uint32_t control2;
    volatile uint32_t control3;
    volatile uint32_t guard;
} nx_uart_v1_registers_t;
/** \brief One fixed physical channel, not an allocator or stream adapter. */
typedef struct {
    volatile uint32_t control;
    volatile uint32_t count;
    volatile uint32_t peripheral;
    volatile uint32_t memory;
} nx_uart_v1_channel_t;
/** \brief Optional RX extension owned by one exact duplex instance. */
typedef struct nx_uart_v1_dma_receive nx_uart_v1_dma_receive_t;
/**
 * \brief Compact TX prefix with either an IRQ ring or embedded RX owner.
 * \note Caller regions, ring, stream, requests and wake remain live until
 *       successful stop. A failed hardware withdrawal retains each loan.
 *       Cold SoC wrappers validate routes, masks, gates and every IRQ source.
 */
typedef struct {
    nx_time_us_t drain_deadline;
    nx_uart_v1_registers_t* registers;
    nx_uart_v1_channel_t* tx;
    const volatile uint32_t* flags;
    volatile uint32_t* clear;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
    nx_result_t (*stop_irqs)(void*);
    nx_result_t (*validate_wake)(void*, const nx_irq_wake_t*, uint8_t);
    const nx_irq_wake_t* wake;
    nx_uart_tx_request_t* active;
    /** \brief The immutable selected profile determines the active member. */
    union {
        void* rx_storage;
        nx_uart_v1_dma_receive_t* receive;
    };
    size_t rx_capacity;
    size_t rx_head;
    size_t rx_tail;
    size_t rx_count;
    uint32_t rx_loss;
    uint32_t baud;
    nx_irq_policy_t irq_policy;
    uint16_t wake_publishers;
    nx_result_t terminal;
    nx_uart_rx_profile_t profile;
    uint8_t tx_shift;
    uint8_t rx_shift;
    bool initialized;
    bool retained;
    bool closing;
    bool tx_launched;
    bool tx_draining;
    bool dma_complete;
    bool wire_complete;
} nx_uart_v1_dma_state_t;
/** \brief Embedded only for normal-block DMA; never a maximum-instance pool.
 */
struct nx_uart_v1_dma_receive {
    nx_uart_v1_channel_t* channel;
    nx_stream_t* stream;
    nx_stream_fill_t fill;
    uint32_t boundary;
    bool filling;
    bool rx_launched;
    bool rx_draining;
    bool rx_stopping;
    bool rx_quarantined;
    bool gap_error;
};
/** \brief One static duplex instance; the TX prefix owns its RX extension. */
typedef struct {
    nx_uart_v1_dma_state_t uart;
    nx_uart_v1_dma_receive_t receive;
} nx_uart_v1_dma_duplex_state_t;
#ifdef __cplusplus
extern "C" {
#endif
extern const nx_uart_ops_t nx_uart_v1_channel_dma_ops;
extern const nx_uart_ops_t nx_uart_v1_channel_dma_tx_ops;
/** \brief Admit the selected TX-DMA and byte-IRQ mode without RX block state.
 * \param[in,out] state: Unique static TX context and explicit byte ring.
 * \param[in] clock_hz: Exactly reviewed 8MHz peripheral input.
 * \return Success/error; retained ownership requires ordinary stop.
 */
nx_result_t nx_uart_v1_channel_dma_tx_initialize(nx_uart_v1_dma_state_t* state,
                                                 uint32_t clock_hz);
/** \brief Admit an SDK-validated cold duplex instance without hidden buffers.
 * \param[in,out] state: Prefix owning its embedded extension and live domains.
 * \param[in] clock_hz: Reviewed fixed legacy USART input, exactly 8 MHz.
 * \return Success/error; retained means ordinary stop must finish cleanup.
 * \note Privileged unmasked task. This helper never enables an NVIC vector.
 */
nx_result_t nx_uart_v1_channel_dma_initialize(nx_uart_v1_dma_state_t* state,
                                              uint32_t clock_hz);
/** \brief Bounded duplex status latch; never settles a request in the IRQ.
 * \param[in,out] state: Live duplex prefix whose vectors remain owned.
 */
void nx_uart_v1_channel_dma_irq(nx_uart_v1_dma_state_t* state);
/** \brief Selected TX-mode UART vector, including its IRQ byte receiver.
 * \param[in,out] state: Live exact TX-DMA/IRQ-byte instance.
 */
void nx_uart_v1_channel_dma_tx_irq(nx_uart_v1_dma_state_t* state);
/** \brief Selected TX-mode channel vector, independent from actual wire TC.
 * \param[in,out] state: Live exact TX-only DMA instance.
 */
void nx_uart_v1_channel_dma_tx_channel_irq(nx_uart_v1_dma_state_t* state);
/** \brief Latch one channel's terminal facts; wire TC remains independent.
 * \param[in,out] state: Live exact instance.
 * \param[in] receive: Select the retained RX channel rather than TX.
 */
void nx_uart_v1_channel_dma_channel_irq(nx_uart_v1_dma_state_t* state,
                                        bool receive);
#ifdef __cplusplus
}
#endif
#endif
