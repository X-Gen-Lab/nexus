/**
 * \file            adapter.h
 * \brief           Private finite UART HAL lifecycle evaluation boundary
 * \author          Nexus Team
 */
#ifndef NEXUS_HAL_REFERENCE_ADAPTER_H
#define NEXUS_HAL_REFERENCE_ADAPTER_H

#include "nexus/core/request.h"
#include "stm32f4xx_hal.h"

/**
 * \brief           Caller-owned reference state, never a production provider.
 * \note            One serialized executor owns the Handle and all IRQ entry
 *                  calls. Both Handles outlive the adapter. No RX, concurrent
 *                  host thread, callback registration or hardware claim is
 *                  provided. Only finite USART1/DMA2 Stream7 TX is evaluated.
 *                  Cold Handles are configured as normal channel-4 byte DMA,
 *                  memory-to-peripheral, incrementing memory and fixed UART
 *                  data register; UART is 8-bit without parity. The caller
 *                  owns that exact configuration and both Handles exclusively.
 */
typedef struct {
    UART_HandleTypeDef* uart;
    DMA_HandleTypeDef* dma;
    nx_request_t* active;
    nx_time_us_t drain_deadline;
    nx_result_t terminal;
    uint16_t length;
    unsigned hal_depth;
    bool started;
    bool draining;
} nx_hal_reference_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Admit one finite transfer before invoking the real HAL.
 * \param[in,out]   adapter: Zeroed owner with exact cold Handle pointers.
 * \param[in,out]   request: Prepared request retained until observed SETTLED.
 * \param[in]       data: Caller-owned byte payload in the reviewed SRAM range.
 * \param[in]       length: Nonzero byte count, at most UINT16_MAX.
 * \param[in]       now: Absolute monotonic time in the request deadline domain.
 * \return          SUCCESS accepts the borrow even if HAL startup then fails;
 *                  other results reject without retaining the new request.
 * \note            Task executor only. Deadline is finite. Service, IRQ calls
 *                  and cancellation are explicit; no worker advances the HAL.
 */
nx_result_t nx_hal_reference_submit(nx_hal_reference_t* adapter,
                                    nx_request_t* request, const uint8_t* data,
                                    size_t length, nx_time_us_t now);

/**
 * \brief           Request abort without returning the borrowed storage.
 * \param[in,out]   adapter: Admitted owner, not inside a HAL callback.
 * \param[in]       now: Finite absolute time in the same monotonic domain.
 * \return          SUCCESS requests cancellation; STATE has no active borrow;
 *                  INVALID rejects the reserved NX_DEADLINE_NEVER clock value.
 * \note            Task executor only. Service retains QUARANTINED storage
 *                  when drain exceeds its finite 1000 us observation budget.
 */
nx_result_t nx_hal_reference_cancel(nx_hal_reference_t* adapter,
                                    nx_time_us_t now);

/**
 * \brief           Observe deadline, withdrawal and wire completion once.
 * \param[in,out]   adapter: Live owner and Handles.
 * \param[in]       now: Current finite monotonic time, never a HAL tick.
 * \note            Single executor. Reentry from a HAL callback does nothing.
 *                  Settlement clears payload, IRQ and register references
 *                  before the Core release publication. No wait loop is added.
 *                  The reserved NX_DEADLINE_NEVER clock value makes no
 * progress.
 */
void nx_hal_reference_service(nx_hal_reference_t* adapter, nx_time_us_t now);

/**
 * \brief           Execute the real DMA handler only for an admitted owner.
 * \param[in,out]   adapter: Live serialized reference owner.
 * \note            The reviewed TE path polls EN with a count budget derived
 *                  from SystemCoreClock/9600. HAL Init/Abort use another tick.
 *                  This entry does not establish a production ISR budget.
 */
void nx_hal_reference_dma_irq(nx_hal_reference_t* adapter);

/**
 * \brief           Execute the real UART handler only for an admitted owner.
 * \param[in,out]   adapter: Live serialized reference owner.
 * \note            No request is settled from the callback call stack.
 */
void nx_hal_reference_uart_irq(nx_hal_reference_t* adapter);

#ifdef __cplusplus
}
#endif
#endif
