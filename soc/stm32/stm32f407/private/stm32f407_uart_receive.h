/**
 * \file            stm32f407_uart_receive.h
 * \brief           Selected USART1 DMA receive targets and producer loans
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_UART_RECEIVE_H
#define NEXUS_STM32F407_UART_RECEIVE_H

#include "stm32f407_dma.h"

/** \brief Only selected receive modes own these exact hardware targets. */
typedef struct {
    nx_stm32_uart_state_t uart;
    nx_stm32_uart_dma_resources_t resources;
    DMA_Stream_TypeDef* receive;
    nx_stream_t* stream;
    nx_stream_producer_t producer;
    nx_stream_fill_t fill[3];
    uint32_t boundary;
    unsigned expected_bank;
    size_t loans;
    bool continuous;
    bool pending;
    bool fault;
    bool stopping;
} nx_stm32_uart_receive_state_t;

/** \brief Duplex reuses the compact RX owner and adds only selected TX DMA. */
typedef struct {
    nx_stm32_uart_receive_state_t receive;
    nx_stm32_uart_tx_dma_extension_t transmit;
} nx_stm32_uart_duplex_state_t;

#ifdef __cplusplus
extern "C" {
#endif

extern const nx_uart_ops_t nx_stm32_uart_receive_ops;
extern const nx_uart_ops_t nx_stm32_uart_duplex_ops;

/**
 * \brief           Initialize exact USART1/DMA2 stream2 receive facts.
 * \param[in,out]   state: Fresh selected state; caller blocks arrive at
 *                  rx_start and remain borrowed through successful rx_stop.
 * \param[in]       clock_hz: Exact USART1 peripheral clock.
 * \return          Success or INVALID/UNSUPPORTED/BUSY without block borrow.
 * \note            Task-only; optional TX DMA uses stream7/channel4. Assembly
 *                  owns reviewed pins, clocks and all vector priorities.
 */
nx_result_t
nx_stm32_uart_receive_initialize(nx_stm32_uart_receive_state_t* state,
                                 uint32_t clock_hz);
/**
 * \brief           Observe bounded USART IDLE/error and independent TX facts.
 * \param[in,out]   state: Live selected instance, retained through stop.
 * \note            Never reads DATA while an RX DMA engine may still write.
 */
void nx_stm32_uart_receive_irq(nx_stm32_uart_receive_state_t* state);
/**
 * \brief           Observe only DMA2 stream2 receive completion or faults.
 * \param[in,out]   state: Same selected instance as the USART vector.
 * \note            Publication requires removal of every hardware write
 *                  reference; failure to disable keeps the producer loan.
 */
void nx_stm32_uart_receive_dma_irq(nx_stm32_uart_receive_state_t* state);

/**
 * \brief           Side-effect-free checks shared with this exact TX start.
 * \param[in]       context: Exact immutable face's selected provider state.
 * \param[in]       request: Live descriptor, not admitted or retained here.
 * \return          Same payload/domain/context/state/deadline rejection.
 * \note            Task-only; reads facts but performs no MMIO store or borrow.
 *                  RS485 binding must use this provider's matching validator.
 */
nx_result_t
nx_stm32_uart_receive_tx_validate(void* context,
                                  const nx_uart_tx_request_t* request);

/**
 * \brief           Initialize selected TX-DMA plus independent RX DMA.
 * \param[in,out]   state: Same selected caller-owned receive storage.
 * \param[in]       clock_hz: Exact UART peripheral clock.
 * \return          Same cold route and ownership checks as receive initialize.
 * \note            Only this constructor retains TX-DMA initialization code.
 */
nx_result_t nx_stm32_uart_duplex_initialize(nx_stm32_uart_duplex_state_t* state,
                                            uint32_t clock_hz);
/** \brief UART vector for selected TX-DMA and independent RX DMA state. */
void nx_stm32_uart_duplex_irq(nx_stm32_uart_duplex_state_t* state);
/** \brief Exact TX-DMA vector decodes the independent duplex extension. */
void nx_stm32_uart_duplex_dma_irq(nx_stm32_uart_duplex_state_t* state);
/** \brief Matching pure validator for the selected duplex transport face. */
nx_result_t
nx_stm32_uart_duplex_tx_validate(void* context,
                                 const nx_uart_tx_request_t* request);

/** \brief Private shared RX algorithm used by both selected owner layouts. */
nx_result_t nx_stm32_uart_receive_prepare(nx_stm32_uart_receive_state_t* state);
void nx_stm32_uart_receive_finish_initialize(
    nx_stm32_uart_receive_state_t* state, unsigned priority);
void nx_stm32_uart_receive_service(nx_stm32_uart_receive_state_t* state);
nx_result_t nx_stm32_uart_receive_validate(nx_stm32_uart_receive_state_t* state,
                                           const nx_uart_tx_request_t* request);
nx_result_t nx_stm32_uart_receive_start(nx_stm32_uart_receive_state_t* state,
                                        nx_stream_t* stream, size_t owner_size);
nx_result_t nx_stm32_uart_receive_stop(void* context);
nx_result_t
nx_stm32_uart_receive_attach_wake(nx_stm32_uart_receive_state_t* state,
                                  const nx_irq_wake_t* wake, uint8_t ceiling,
                                  bool duplex);
bool nx_stm32_uart_receive_irq_facts(nx_stm32_uart_receive_state_t* state,
                                     uint32_t status);
void nx_stm32_uart_receive_vector_stop(void);

#ifdef __cplusplus
}
#endif
#endif
