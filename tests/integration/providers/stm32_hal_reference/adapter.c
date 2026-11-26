/**
 * \file            adapter.c
 * \brief           Finite HAL lifecycle evaluation implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "adapter.h"

/** \brief A borrowed payload cannot include mutable owner metadata. */
static bool overlap(const void* left, size_t left_size, const void* right,
                    size_t right_size) {
    uintptr_t a = (uintptr_t)left;
    uintptr_t b = (uintptr_t)right;
    if (left_size > UINTPTR_MAX - a || right_size > UINTPTR_MAX - b) {
        return true;
    }
    return a < b + right_size && b < a + left_size;
}

/** \brief Reject aliases before admission or HAL writes any retained field. */
static bool separate_payload(const nx_hal_reference_t* adapter,
                             const nx_request_t* request, const uint8_t* data,
                             size_t length) {
    return !overlap(data, length, request, sizeof(*request)) &&
           !overlap(data, length, adapter, sizeof(*adapter)) &&
           !overlap(data, length, adapter->uart, sizeof(*adapter->uart)) &&
           !overlap(data, length, adapter->dma, sizeof(*adapter->dma));
}

/** \brief Reject unsupported facts before admitting or entering vendor code. */
static bool valid(const nx_hal_reference_t* adapter, const uint8_t* data,
                  size_t length) {
    const uintptr_t begin = (uintptr_t)data;
    return adapter != NULL && adapter->uart != NULL && adapter->dma != NULL &&
           adapter->uart->Instance == USART1 &&
           adapter->dma->Instance == DMA2_Stream7 &&
           adapter->uart->hdmatx == adapter->dma &&
           adapter->dma->Parent == adapter->uart && length != 0U &&
           length <= UINT16_MAX && begin >= UINT32_C(0x20000000) &&
           begin < UINT32_C(0x20020000) &&
           length <= UINT32_C(0x20020000) - begin;
}

/** \brief Establish the owner before a HAL entry can call back synchronously.
 */
nx_result_t nx_hal_reference_submit(nx_hal_reference_t* adapter,
                                    nx_request_t* request, const uint8_t* data,
                                    size_t length, nx_time_us_t now) {
    if (!valid(adapter, data, length) || request == NULL ||
        !separate_payload(adapter, request, data, length) ||
        request->deadline == NX_DEADLINE_NEVER || now == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (nx_deadline_expired(request->deadline, now)) {
        return NX_ERROR_TIMEOUT;
    }
    if (adapter->active != NULL || adapter->hal_depth != 0U ||
        adapter->uart->gState != HAL_UART_STATE_READY ||
        (adapter->dma->Instance->CR & DMA_SxCR_EN) != 0U ||
        (adapter->uart->Instance->CR3 & USART_CR3_DMAT) != 0U) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result = nx_request_admit(request, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS) {
        return result;
    }
    adapter->active = request;
    adapter->length = (uint16_t)length;
    adapter->terminal = NX_SUCCESS;
    adapter->draining = false;
    adapter->drain_deadline = 0U;
    ++adapter->hal_depth;
    HAL_StatusTypeDef started =
        HAL_UART_Transmit_DMA(adapter->uart, data, (uint16_t)length);
    --adapter->hal_depth;
    adapter->started = started == HAL_OK;
    if (!adapter->started) {
        /* HAL restores READY but keeps its private payload and DMA callbacks.
         * Admission already happened; only explicit service withdraws them. */
        adapter->terminal = NX_ERROR_IO;
    }
    return NX_SUCCESS;
}

/** \brief Keep the first cancellation cause and bounded drain observation. */
static nx_result_t abort_transfer(nx_hal_reference_t* adapter, nx_time_us_t now,
                                  nx_result_t reason) {
    if (adapter == NULL || adapter->active == NULL ||
        adapter->hal_depth != 0U) {
        return NX_ERROR_STATE;
    }
    if (adapter->draining) {
        return NX_SUCCESS;
    }
    if (nx_request_transition(adapter->active, NX_REQUEST_DRAINING) !=
        NX_SUCCESS) {
        return NX_ERROR_STATE;
    }
    adapter->draining = true;
    adapter->terminal = reason;
    adapter->drain_deadline = nx_deadline_after(now, 1000U);
    if (adapter->drain_deadline == NX_DEADLINE_NEVER) {
        adapter->drain_deadline = NX_DEADLINE_NEVER - 1U;
    }
    ++adapter->hal_depth;
    if (HAL_UART_AbortTransmit_IT(adapter->uart) != HAL_OK) {
        (void)nx_request_transition(adapter->active, NX_REQUEST_QUARANTINED);
    }
    --adapter->hal_depth;
    return NX_SUCCESS;
}

/** \brief Abort is a withdrawal request; a callback does not revoke a borrow.
 */
nx_result_t nx_hal_reference_cancel(nx_hal_reference_t* adapter,
                                    nx_time_us_t now) {
    if (now == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    return abort_transfer(adapter, now, NX_ERROR_CANCELLED);
}

/** \brief Remove every retained TX reference in this serialized reference. */
static void detach(nx_hal_reference_t* adapter) {
    UART_HandleTypeDef* uart = adapter->uart;
    DMA_HandleTypeDef* dma = adapter->dma;
    uart->Instance->CR1 &= ~(uint32_t)(USART_CR1_TXEIE | USART_CR1_TCIE);
    dma->Instance->CR &= ~(uint32_t)(DMA_SxCR_TCIE | DMA_SxCR_HTIE |
                                     DMA_SxCR_TEIE | DMA_SxCR_DMEIE);
    dma->Instance->FCR &= ~(uint32_t)DMA_SxFCR_FEIE;
    DMA2->HIFCR = DMA_HIFCR_CFEIF7 | DMA_HIFCR_CDMEIF7 | DMA_HIFCR_CTEIF7 |
                  DMA_HIFCR_CHTIF7 | DMA_HIFCR_CTCIF7;
    dma->XferCpltCallback = NULL;
    dma->XferHalfCpltCallback = NULL;
    dma->XferM1CpltCallback = NULL;
    dma->XferM1HalfCpltCallback = NULL;
    dma->XferErrorCallback = NULL;
    dma->XferAbortCallback = NULL;
    dma->Instance->M0AR = 0U;
    dma->Instance->M1AR = 0U;
    dma->Instance->PAR = 0U;
    dma->Instance->NDTR = 0U;
    uart->pTxBuffPtr = NULL;
    uart->TxXferCount = 0U;
    uart->TxXferSize = 0U;
}

/** \brief Observe drain once outside the complete vendor callback call stack.
 */
void nx_hal_reference_service(nx_hal_reference_t* adapter, nx_time_us_t now) {
    if (now == NX_DEADLINE_NEVER || adapter == NULL ||
        adapter->active == NULL || adapter->hal_depth != 0U) {
        return;
    }
    if (!adapter->draining && adapter->terminal == NX_SUCCESS) {
        if (adapter->uart->ErrorCode != HAL_UART_ERROR_NONE ||
            adapter->dma->ErrorCode != HAL_DMA_ERROR_NONE) {
            (void)abort_transfer(adapter, now, NX_ERROR_IO);
        } else if (nx_deadline_expired(adapter->active->deadline, now)) {
            (void)abort_transfer(adapter, now, NX_ERROR_TIMEOUT);
        }
    }
    bool idle = (adapter->dma->Instance->CR & DMA_SxCR_EN) == 0U &&
                (adapter->uart->Instance->CR3 & USART_CR3_DMAT) == 0U &&
                adapter->uart->gState == HAL_UART_STATE_READY;
    bool wire =
        !adapter->started || (adapter->uart->Instance->SR & USART_SR_TC) != 0U;
    if (!idle || !wire) {
        if (adapter->draining &&
            nx_deadline_expired(adapter->drain_deadline, now) &&
            nx_request_state(adapter->active) != NX_REQUEST_QUARANTINED) {
            (void)nx_request_transition(adapter->active,
                                        NX_REQUEST_QUARANTINED);
        }
        return;
    }
    const uint32_t remaining = adapter->dma->Instance->NDTR;
    size_t transferred = adapter->started && remaining <= adapter->length
                             ? adapter->length - remaining
                             : 0U;
    nx_request_t* request = adapter->active;
    nx_result_t result = adapter->terminal;
    if (result == NX_SUCCESS && remaining > adapter->length) {
        result = NX_ERROR_IO;
    }
    detach(adapter);
    adapter->active = NULL;
    adapter->started = false;
    adapter->draining = false;
    nx_request_settle(request, result, transferred);
}

/** \brief Preserve borrow across every real HAL DMA callback stack. */
void nx_hal_reference_dma_irq(nx_hal_reference_t* adapter) {
    if (adapter != NULL && adapter->active != NULL &&
        adapter->hal_depth == 0U) {
        ++adapter->hal_depth;
        HAL_DMA_IRQHandler(adapter->dma);
        --adapter->hal_depth;
    }
}

/** \brief Observe real UART completion without publishing from its callback. */
void nx_hal_reference_uart_irq(nx_hal_reference_t* adapter) {
    if (adapter != NULL && adapter->active != NULL &&
        adapter->hal_depth == 0U) {
        ++adapter->hal_depth;
        HAL_UART_IRQHandler(adapter->uart);
        --adapter->hal_depth;
    }
}
