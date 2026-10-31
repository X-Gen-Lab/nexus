/**
 * \file            uart_hal.c
 * \brief           Finite vendor HAL TX and disjoint LL RX
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-30
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_hal_profile.h"
#include "stm32f407_uart_hal.h"
#include "stm32f4xx_ll_usart.h"
#include <stddef.h>
#include <string.h>

#define NX_HAL_UART_VECTOR       1U
#define NX_HAL_DMA_VECTOR        2U
#define NX_HAL_DMA_DRAIN_PENDING 4U
#define NX_HAL_ABORTING          1U
#define NX_HAL_MEMORY_COMPLETE   2U
#define NX_HAL_DEINITIALIZING    4U
#define NX_HAL_RX_CR1            (USART_CR1_RXNEIE | USART_CR1_PEIE)

/** \brief The registered Handle belongs to this exact static owner. */
static nx_stm32_uart_hal_state_t* owner(UART_HandleTypeDef* handle) {
    return (nx_stm32_uart_hal_state_t*)((uint8_t*)handle -
                                        offsetof(nx_stm32_uart_hal_state_t,
                                                 handle));
}

static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

/** \brief Keep every HAL receive path outside this disjoint RX profile. */
static bool owned_route(const nx_stm32_uart_hal_state_t* state) {
    return state->config != NULL &&
           state->handle.Instance == state->uart.registers &&
           state->handle.Instance == state->config->uart.registers &&
           state->handle.hdmatx == &state->dma &&
           state->dma.Instance == state->config->transmit.registers &&
           state->dma.Parent == &state->handle;
}

static bool owned_domain(const nx_stm32_uart_hal_state_t* state) {
    return owned_route(state) &&
           state->handle.RxState == HAL_UART_STATE_READY &&
           state->handle.ReceptionType == HAL_UART_RECEPTION_STANDARD &&
           state->handle.hdmarx == NULL && state->handle.pRxBuffPtr == NULL &&
           state->handle.RxXferCount == 0U && state->handle.RxXferSize == 0U &&
           (state->uart.registers->CR3 & USART_CR3_DMAR) == 0U;
}

/** \brief Isolate our publishers without adopting any foreign RX DMA owner. */
static void quarantine_domain(nx_stm32_uart_hal_state_t* state) {
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_stm32_uart_state_t* uart = &state->uart;
    uart->closing = true;
    if (uart->terminal == NX_SUCCESS)
        uart->terminal = NX_ERROR_IO;
    NVIC_DisableIRQ(uart->irq);
    NVIC_DisableIRQ(state->config->transmit.irq);
    if (uart->active != NULL &&
        nx_request_state(&uart->active->base) != NX_REQUEST_QUARANTINED)
        (void)nx_request_transition(&uart->active->base,
                                    NX_REQUEST_QUARANTINED);
    state->hal_depth = 0U;
    /* A later valid task HAL entry restores these sources for its own drain. */
    state->masked_vectors = NX_HAL_UART_VECTOR | NX_HAL_DMA_VECTOR;
    nx_arch_dsb();
    nx_arch_irq_restore(saved);
}

/** \brief A callback latches facts without publishing the request. */
static void tx_complete(UART_HandleTypeDef* handle) {
    nx_stm32_uart_state_t* uart = &owner(handle)->uart;
    if (uart->initialized && uart->active != NULL && !uart->tx_complete) {
        if (uart->terminal == NX_SUCCESS &&
            nx_deadline_expired(uart->active->base.deadline, nx_time_now_us()))
            uart->terminal = NX_ERROR_TIMEOUT;
        uart->tx_complete = true;
    }
}

static void transfer_error(UART_HandleTypeDef* handle) {
    nx_stm32_uart_state_t* uart = &owner(handle)->uart;
    if (uart->initialized && uart->active != NULL && !uart->tx_complete &&
        (handle->ErrorCode & HAL_UART_ERROR_DMA) != 0U &&
        uart->terminal == NX_SUCCESS)
        uart->terminal = NX_ERROR_IO;
}

/** \brief Neither abort readiness nor MSP calls transfer resource ownership. */
static void callback_noop(UART_HandleTypeDef* handle) {
    (void)handle;
}

/** \brief Prevent a level source from tail-chaining inside a task HAL call. */
static bool irq_enter(nx_stm32_uart_hal_state_t* state, uint8_t bit) {
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (state->hal_depth != 0U) {
        state->masked_vectors |= bit;
        NVIC_DisableIRQ(bit == NX_HAL_UART_VECTOR
                            ? state->uart.irq
                            : state->config->transmit.irq);
        nx_arch_dsb();
        nx_arch_irq_restore(saved);
        return false;
    }
    state->hal_depth = 1U;
    nx_arch_irq_restore(saved);
    return true;
}

/** \brief Restore deferred vectors only for the still-retained lifecycle. */
static void leave_hal(nx_stm32_uart_hal_state_t* state) {
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    state->hal_depth = 0U;
    uint8_t masked = state->masked_vectors;
    state->masked_vectors = masked & NX_HAL_DMA_DRAIN_PENDING;
    if ((masked & NX_HAL_DMA_DRAIN_PENDING) != 0U &&
        (state->dma.Instance->CR & DMA_SxCR_EN) != 0U) {
        masked &= (uint8_t)~NX_HAL_DMA_VECTOR;
        state->masked_vectors |= NX_HAL_DMA_VECTOR;
    }
    if (state->uart.initialized && owned_domain(state) &&
        (!state->uart.closing || state->uart.active != NULL)) {
        if ((masked & NX_HAL_UART_VECTOR) != 0U)
            NVIC_EnableIRQ(state->uart.irq);
        if ((masked & NX_HAL_DMA_VECTOR) != 0U)
            NVIC_EnableIRQ(state->config->transmit.irq);
    }
    nx_arch_dsb();
    nx_arch_irq_restore(saved);
}

/** \brief Reject unsupported static data before acquiring any resource. */
static nx_result_t configuration(const nx_stm32_uart_dma_cold_config_t* config,
                                 nx_uart_format_t* format) {
    if (config == NULL || config->uart.vectors == NULL ||
        config->uart.vector_count != 2U || config->uart.rx_storage == NULL ||
        config->uart.rx_capacity == 0U || config->uart.baud == 0U ||
        config->uart.priority > 15U ||
        (config->uart.profile != NX_UART_RX_BYTES &&
         config->uart.profile != NX_UART_RX_EVENTS) ||
        config->resources.regions == NULL ||
        config->resources.region_count == 0U)
        return NX_ERROR_INVALID;
    bool primary = config->uart.registers == USART1;
    if ((!primary && config->uart.registers != USART2) ||
        config->uart.clock_hz != (primary ? 84000000U : 42000000U) ||
        config->uart.baud > config->uart.clock_hz / 16U ||
        nx_stm32_hal_brr16(config->uart.clock_hz, config->uart.baud) >
            UINT16_MAX ||
        config->resources.dma != (primary ? DMA2 : DMA1) ||
        config->transmit.registers != (primary ? DMA2_Stream7 : DMA1_Stream6) ||
        config->transmit.irq !=
            (primary ? DMA2_Stream7_IRQn : DMA1_Stream6_IRQn) ||
        config->transmit.stream != (primary ? 7U : 6U) ||
        config->transmit.channel != 4U ||
        config->uart.vectors[0] != (primary ? USART1_IRQn : USART2_IRQn) ||
        config->uart.vectors[1] != config->transmit.irq)
        return NX_ERROR_UNSUPPORTED;
    *format = config->uart.format == NULL
                  ? (nx_uart_format_t){8U, NX_UART_PARITY_NONE, NX_UART_STOP_1,
                                       NX_UART_FLOW_NONE}
                  : *config->uart.format;
    if (nx_uart_format_validate(*format) != NX_SUCCESS ||
        format->data_bits == 9U)
        return NX_ERROR_UNSUPPORTED;
    return NX_SUCCESS;
}

/** \brief Read back the exact finite normal byte-DMA configuration. */
static bool configured_dma(const nx_stm32_uart_hal_state_t* state) {
    uint32_t dma = state->dma.Init.Channel | state->dma.Init.Direction |
                   state->dma.Init.MemInc | state->dma.Init.Priority;
    return (state->dma.Instance->CR &
            (DMA_SxCR_CHSEL | DMA_SxCR_MBURST | DMA_SxCR_PBURST | DMA_SxCR_PL |
             DMA_SxCR_MSIZE | DMA_SxCR_PSIZE | DMA_SxCR_MINC | DMA_SxCR_PINC |
             DMA_SxCR_CIRC | DMA_SxCR_DIR | DMA_SxCR_DBM | DMA_SxCR_CT |
             DMA_SxCR_EN | DMA_SxCR_TCIE | DMA_SxCR_HTIE | DMA_SxCR_TEIE |
             DMA_SxCR_DMEIE)) == dma &&
           (state->dma.Instance->FCR &
            (DMA_SxFCR_DMDIS | DMA_SxFCR_FTH | DMA_SxFCR_FEIE)) == 0U;
}

/** \brief A HAL return code cannot replace selected hardware readback. */
static bool initialized_hardware(const nx_stm32_uart_hal_state_t* state) {
    const USART_TypeDef* uart = state->handle.Instance;
    uint32_t cr1 = USART_CR1_UE | state->handle.Init.Mode |
                   state->handle.Init.WordLength | state->handle.Init.Parity;
    return (uart->CR1 &
            (USART_CR1_UE | USART_CR1_M | USART_CR1_PCE | USART_CR1_PS |
             USART_CR1_TE | USART_CR1_RE | USART_CR1_OVER8 | USART_CR1_RXNEIE |
             USART_CR1_PEIE | USART_CR1_TXEIE | USART_CR1_TCIE |
             USART_CR1_IDLEIE)) == cr1 &&
           (uart->CR2 & (USART_CR2_STOP | USART_CR2_LINEN | USART_CR2_CLKEN)) ==
               state->handle.Init.StopBits &&
           (uart->CR3 & (USART_CR3_CTSE | USART_CR3_RTSE | USART_CR3_DMAT |
                         USART_CR3_DMAR | USART_CR3_EIE | USART_CR3_HDSEL |
                         USART_CR3_SCEN | USART_CR3_IREN)) ==
               state->handle.Init.HwFlowCtl &&
           uart->BRR == nx_stm32_hal_brr16(state->config->uart.clock_hz,
                                           state->config->uart.baud) &&
           configured_dma(state) && owned_domain(state);
}

nx_result_t
nx_stm32_uart_hal_initialize(nx_stm32_uart_hal_state_t* state,
                             const nx_stm32_uart_dma_cold_config_t* config) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL)
        return NX_ERROR_INVALID;
    nx_uart_format_t format;
    nx_result_t result = configuration(config, &format);
    if (result != NX_SUCCESS)
        return result;
    if (state->uart.initialized || state->uart.active != NULL ||
        state->hal_depth != 0U || state->masked_vectors != 0U ||
        (config->transmit.registers->CR & DMA_SxCR_EN) != 0U)
        return NX_ERROR_BUSY;
    bool primary = config->uart.registers == USART1;
    if (((primary ? RCC->APB2ENR : RCC->APB1ENR) &
         (primary ? RCC_APB2ENR_USART1EN : RCC_APB1ENR_USART2EN)) == 0U ||
        (RCC->AHB1ENR & (primary ? RCC_AHB1ENR_DMA2EN : RCC_AHB1ENR_DMA1EN)) ==
            0U)
        return NX_ERROR_STATE;
    if ((config->uart.registers->CR2 & USART_CR2_LBDIE) != 0U ||
        (config->uart.registers->CR3 & USART_CR3_CTSIE) != 0U)
        return NX_ERROR_BUSY;
    state->config = config;
    state->uart.registers = config->uart.registers;
    state->uart.irq = config->uart.vectors[0];
    state->uart.profile = config->uart.profile;
    state->uart.rx_storage = config->uart.rx_storage;
    state->uart.rx_capacity = config->uart.rx_capacity;
    state->uart.baud = config->uart.baud;
    state->uart.initialized = true;
    state->uart.closing = true;
    state->handle = (UART_HandleTypeDef){0};
    state->dma = (DMA_HandleTypeDef){0};
    state->handle.Instance = config->uart.registers;
    state->handle.Init.BaudRate = config->uart.baud;
    state->handle.Init.WordLength =
        format.data_bits == 8U && format.parity != NX_UART_PARITY_NONE
            ? UART_WORDLENGTH_9B
            : UART_WORDLENGTH_8B;
    state->handle.Init.Parity =
        format.parity == NX_UART_PARITY_NONE
            ? UART_PARITY_NONE
            : (format.parity == NX_UART_PARITY_ODD ? UART_PARITY_ODD
                                                   : UART_PARITY_EVEN);
    state->handle.Init.StopBits =
        format.stop == NX_UART_STOP_2 ? UART_STOPBITS_2 : UART_STOPBITS_1;
    state->handle.Init.HwFlowCtl =
        (format.flow == NX_UART_FLOW_RTS || format.flow == NX_UART_FLOW_RTS_CTS
             ? UART_HWCONTROL_RTS
             : 0U) |
        (format.flow == NX_UART_FLOW_CTS || format.flow == NX_UART_FLOW_RTS_CTS
             ? UART_HWCONTROL_CTS
             : 0U);
    state->handle.Init.Mode = UART_MODE_TX_RX;
    state->handle.Init.OverSampling = UART_OVERSAMPLING_16;
    state->handle.MspInitCallback = callback_noop;
    state->handle.MspDeInitCallback = callback_noop;
    state->handle.hdmatx = &state->dma;
    state->dma.Instance = config->transmit.registers;
    state->dma.Parent = &state->handle;
    state->dma.Init.Channel = DMA_CHANNEL_4;
    state->dma.Init.Direction = DMA_MEMORY_TO_PERIPH;
    state->dma.Init.PeriphInc = DMA_PINC_DISABLE;
    state->dma.Init.MemInc = DMA_MINC_ENABLE;
    state->dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    state->dma.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    state->dma.Init.Mode = DMA_NORMAL;
    state->dma.Init.Priority = DMA_PRIORITY_HIGH;
    state->dma.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    state->hal_depth = 1U;
    HAL_StatusTypeDef status = HAL_DMA_Init(&state->dma);
    if (status == HAL_OK)
        status = HAL_UART_Init(&state->handle);
    if (status == HAL_OK)
        status = HAL_UART_RegisterCallback(
            &state->handle, HAL_UART_TX_HALFCOMPLETE_CB_ID, callback_noop);
    if (status == HAL_OK)
        status = HAL_UART_RegisterCallback(
            &state->handle, HAL_UART_TX_COMPLETE_CB_ID, tx_complete);
    if (status == HAL_OK)
        status = HAL_UART_RegisterCallback(&state->handle, HAL_UART_ERROR_CB_ID,
                                           transfer_error);
    if (status == HAL_OK)
        status = HAL_UART_RegisterCallback(
            &state->handle, HAL_UART_ABORT_TRANSMIT_COMPLETE_CB_ID,
            callback_noop);
    leave_hal(state);
    if (status != HAL_OK || !initialized_hardware(state))
        return NX_ERROR_IO;
    LL_USART_EnableIT_RXNE(state->handle.Instance);
    LL_USART_EnableIT_PE(state->handle.Instance);
    LL_USART_EnableIT_ERROR(state->handle.Instance);
    NVIC_SetPriority(state->uart.irq, config->uart.priority);
    NVIC_SetPriority(config->transmit.irq, config->uart.priority);
    NVIC_ClearPendingIRQ(state->uart.irq);
    NVIC_ClearPendingIRQ(config->transmit.irq);
    state->uart.closing = false;
    NVIC_EnableIRQ(state->uart.irq);
    NVIC_EnableIRQ(config->transmit.irq);
    nx_arch_dsb();
    return NX_SUCCESS;
}

/** \brief Share the RX producer without calling the legacy TX state machine. */
static bool receive(nx_stm32_uart_hal_state_t* state, uint32_t status) {
    nx_stm32_uart_state_t* uart = &state->uart;
    uint8_t mask =
        (uart->registers->CR1 & (USART_CR1_M | USART_CR1_PCE)) == USART_CR1_PCE
            ? 0x7FU
            : UINT8_MAX;
    return !uart->closing && nx_stm32_uart_rx_irq(uart, status, mask);
}

void nx_stm32_uart_hal_irq(nx_stm32_uart_hal_state_t* state) {
    if (state == NULL || !state->uart.initialized ||
        !irq_enter(state, NX_HAL_UART_VECTOR))
        return;
    if (!owned_domain(state)) {
        quarantine_domain(state);
        return;
    }
    USART_TypeDef* registers = state->handle.Instance;
    uint32_t status = registers->SR;
    bool notify = receive(state, status);
    uint32_t rx1 = registers->CR1 & NX_HAL_RX_CR1;
    uint32_t rx3 = registers->CR3 & USART_CR3_EIE;
    ATOMIC_CLEAR_BIT(registers->CR1, NX_HAL_RX_CR1);
    ATOMIC_CLEAR_BIT(registers->CR3, USART_CR3_EIE);
    HAL_UART_IRQHandler(&state->handle);
    if (state->uart.initialized && !state->uart.closing) {
        ATOMIC_MODIFY_REG(registers->CR1, NX_HAL_RX_CR1, rx1);
        ATOMIC_MODIFY_REG(registers->CR3, USART_CR3_EIE, rx3);
    }
    notify =
        notify || state->uart.tx_complete || state->uart.terminal != NX_SUCCESS;
    leave_hal(state);
    if (notify)
        (void)nx_irq_wake_signal(state->uart.wake);
}

/** \brief Dispatch HAL only after EN0; one attempt never becomes an IRQ poll.
 * \note A fresh error may arrive after the status snapshot. EN1 therefore
 *       always excludes the vendor handler, including HT and spurious entries.
 *       HT has a private no-op callback, so its owned flag can be acknowledged
 *       with official primitives without advancing any transfer state.
 */
static bool progress_dma(nx_stm32_uart_hal_state_t* state) {
    DMA_HandleTypeDef* dma = &state->dma;
    nx_stm32_uart_state_t* uart = &state->uart;
    uint32_t errors = __HAL_DMA_GET_TE_FLAG_INDEX(dma) |
                      __HAL_DMA_GET_DME_FLAG_INDEX(dma) |
                      __HAL_DMA_GET_FE_FLAG_INDEX(dma);
    uint32_t complete = __HAL_DMA_GET_TC_FLAG_INDEX(dma);
    uint32_t half = __HAL_DMA_GET_HT_FLAG_INDEX(dma);
    uint32_t observed = __HAL_DMA_GET_FLAG(dma, errors | complete | half);
    if ((dma->Instance->CR & DMA_SxCR_EN) != 0U) {
        if ((observed & (errors | complete)) != 0U ||
            dma->ErrorCode != HAL_DMA_ERROR_NONE ||
            (state->masked_vectors & NX_HAL_DMA_DRAIN_PENDING) != 0U) {
            if (uart->terminal == NX_SUCCESS)
                uart->terminal = NX_ERROR_IO;
            LL_USART_DisableDMAReq_TX(uart->registers);
            __HAL_DMA_DISABLE(dma);
            nx_arch_dsb();
            if ((dma->Instance->CR & DMA_SxCR_EN) != 0U) {
                nx_arch_irq_state_t saved = nx_arch_irq_save();
                NVIC_DisableIRQ(state->config->transmit.irq);
                state->masked_vectors |=
                    NX_HAL_DMA_VECTOR | NX_HAL_DMA_DRAIN_PENDING;
                if (uart->active != NULL &&
                    nx_request_state(&uart->active->base) !=
                        NX_REQUEST_QUARANTINED)
                    (void)nx_request_transition(&uart->active->base,
                                                NX_REQUEST_QUARANTINED);
                nx_arch_dsb();
                nx_arch_irq_restore(saved);
                return false;
            }
        } else {
            if ((observed & half) != 0U &&
                __HAL_DMA_GET_IT_SOURCE(dma, DMA_IT_HT) != 0U) {
                __HAL_DMA_DISABLE_IT(dma, DMA_IT_HT);
                __HAL_DMA_CLEAR_FLAG(dma, half);
            }
            return false;
        }
    }
    /* Normal-mode hardware only clears EN. hal_depth excludes every other
     * owned CPU writer until the handler returns, so fresh TE cannot poll EN.
     */
    HAL_DMA_IRQHandler(dma);
    state->masked_vectors &= (uint8_t)~NX_HAL_DMA_DRAIN_PENDING;
    if (uart->active != NULL && !(state->flags & NX_HAL_ABORTING) &&
        !(state->flags & NX_HAL_MEMORY_COMPLETE) &&
        dma->State == HAL_DMA_STATE_READY &&
        dma->ErrorCode == HAL_DMA_ERROR_NONE && dma->Instance->NDTR == 0U &&
        (uart->registers->CR3 & USART_CR3_DMAT) == 0U) {
        state->flags |= NX_HAL_MEMORY_COMPLETE;
        uart->drain_deadline =
            nx_deadline_after(nx_time_now_us(), 20000000U / uart->baud + 100U);
    }
    return true;
}

void nx_stm32_uart_hal_dma_irq(nx_stm32_uart_hal_state_t* state) {
    if (state == NULL || !state->uart.initialized ||
        !irq_enter(state, NX_HAL_DMA_VECTOR))
        return;
    if (!owned_domain(state)) {
        quarantine_domain(state);
        return;
    }
    (void)progress_dma(state);
    leave_hal(state);
    if (state->uart.active != NULL)
        (void)nx_irq_wake_signal(state->uart.wake);
}

/** \brief Reject the entire transfer before retaining or entering HAL. */
static bool overlap(const void* left, size_t left_size, const void* right,
                    size_t right_size) {
    uintptr_t a = (uintptr_t)left;
    uintptr_t b = (uintptr_t)right;
    if (left_size > UINTPTR_MAX - a || right_size > UINTPTR_MAX - b)
        return true;
    return a < b + right_size && b < a + left_size;
}

/** \brief Both metadata and payload must be independent of owned storage. */
static bool protected_storage(const nx_stm32_uart_hal_state_t* state,
                              const void* pointer, size_t size) {
    const nx_stm32_uart_dma_cold_config_t* config = state->config;
    const nx_stm32_uart_state_t* uart = &state->uart;
    size_t element = uart->profile == NX_UART_RX_EVENTS
                         ? sizeof(nx_uart_rx_event_t)
                         : sizeof(uint8_t);
    if (uart->rx_capacity > SIZE_MAX / element)
        return true;
    if (config->resources.region_count >
        SIZE_MAX / sizeof(*config->resources.regions))
        return true;
    if (config->uart.pins != NULL) {
        for (size_t i = 0U; i < 2U; ++i) {
            const uint16_t* snapshot = config->uart.pins[i].snapshot;
            if (snapshot != NULL &&
                overlap(pointer, size, snapshot, sizeof(*snapshot)))
                return true;
        }
    }
    return overlap(pointer, size, state, sizeof(*state)) ||
           overlap(pointer, size, config, sizeof(*config)) ||
           overlap(pointer, size, config->resources.regions,
                   config->resources.region_count *
                       sizeof(*config->resources.regions)) ||
           overlap(pointer, size, config->uart.vectors,
                   config->uart.vector_count * sizeof(*config->uart.vectors)) ||
           (config->uart.format != NULL &&
            overlap(pointer, size, config->uart.format,
                    sizeof(*config->uart.format))) ||
           (config->uart.pins != NULL &&
            overlap(pointer, size, config->uart.pins,
                    2U * sizeof(*config->uart.pins))) ||
           overlap(pointer, size, uart->rx_storage,
                   uart->rx_capacity * element);
}

static nx_result_t validate_tx(nx_stm32_uart_hal_state_t* state,
                               const nx_uart_tx_request_t* request) {
    if (state == NULL || request == NULL)
        return NX_ERROR_INVALID;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!state->uart.initialized || state->uart.closing || !owned_domain(state))
        return NX_ERROR_STATE;
    if (protected_storage(state, request, sizeof(*request)) ||
        request->data == NULL || request->length == 0U ||
        request->length > UINT16_MAX ||
        protected_storage(state, request->data, request->length) ||
        overlap(request->data, request->length, request, sizeof(*request)))
        return NX_ERROR_INVALID;
    nx_result_t result = nx_dma_buffer_validate(
        state->config->resources.regions, state->config->resources.region_count,
        request->data, request->length, 1U, NX_DMA_TO_DEVICE);
    if (result != NX_SUCCESS)
        return result;
    if (state->uart.active != NULL || state->hal_depth != 0U ||
        state->handle.gState != HAL_UART_STATE_READY ||
        (state->dma.Instance->CR & DMA_SxCR_EN) != 0U)
        return NX_ERROR_BUSY;
    return nx_deadline_expired(request->base.deadline, nx_time_now_us())
               ? NX_ERROR_TIMEOUT
               : NX_SUCCESS;
}

/** \brief Establish the borrow before HAL can run a synchronous callback. */
static nx_result_t start(nx_stm32_uart_hal_state_t* state,
                         nx_uart_tx_request_t* request, bool admitted) {
    nx_result_t result = validate_tx(state, request);
    if (result != NX_SUCCESS)
        return result;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    result = admitted ? nx_request_transition(&request->base, NX_REQUEST_ACTIVE)
                      : nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS) {
        nx_arch_irq_restore(saved);
        return result;
    }
    state->uart.active = request;
    state->uart.tx_complete = false;
    state->uart.terminal = NX_SUCCESS;
    state->uart.tx_position = 0U;
    state->flags &= (uint8_t)~NX_HAL_ABORTING;
    state->flags &= (uint8_t)~NX_HAL_MEMORY_COMPLETE;
    state->uart.drain_deadline = NX_DEADLINE_NEVER;
    state->hal_depth = 1U;
    NVIC_ClearPendingIRQ(state->config->transmit.irq);
    NVIC_EnableIRQ(state->config->transmit.irq);
    nx_arch_irq_restore(saved);
    HAL_StatusTypeDef started = HAL_UART_Transmit_DMA(
        &state->handle, request->data, (uint16_t)request->length);
    if (started != HAL_OK)
        state->uart.terminal = NX_ERROR_IO;
    leave_hal(state);
    return NX_SUCCESS;
}

static nx_result_t submit(void* context, nx_uart_tx_request_t* request) {
    return start(context, request, false);
}

static nx_result_t start_admitted(void* context,
                                  nx_uart_tx_request_t* request) {
    if (request == NULL ||
        nx_request_state(&request->base) != NX_REQUEST_QUEUED)
        return NX_ERROR_STATE;
    return start(context, request, true);
}

/** \brief Abort readiness never revokes the borrow or proves wire completion.
 */
static nx_result_t abort_transfer(nx_stm32_uart_hal_state_t* state,
                                  nx_result_t reason) {
    nx_stm32_uart_state_t* uart = &state->uart;
    if (!owned_domain(state)) {
        quarantine_domain(state);
        return NX_ERROR_STATE;
    }
    if ((state->flags & NX_HAL_ABORTING) || uart->tx_complete)
        return NX_SUCCESS;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (state->hal_depth != 0U) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    nx_request_state_t previous = nx_request_state(&uart->active->base);
    if (previous != NX_REQUEST_DRAINING && previous != NX_REQUEST_QUARANTINED &&
        nx_request_transition(&uart->active->base, NX_REQUEST_DRAINING) !=
            NX_SUCCESS) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_STATE;
    }
    state->flags |= NX_HAL_ABORTING;
    if (uart->terminal == NX_SUCCESS)
        uart->terminal = reason;
    uart->drain_deadline =
        nx_deadline_after(nx_time_now_us(), 20000000U / uart->baud + 100U);
    state->hal_depth = 1U;
    nx_arch_irq_restore(saved);
    if (HAL_UART_AbortTransmit_IT(&state->handle) != HAL_OK)
        (void)nx_request_transition(&uart->active->base,
                                    NX_REQUEST_QUARANTINED);
    if ((uart->registers->SR & USART_SR_TC) == 0U)
        LL_USART_EnableIT_TC(uart->registers);
    leave_hal(state);
    return NX_SUCCESS;
}

static nx_result_t cancel(void* context, nx_uart_tx_request_t* request) {
    nx_stm32_uart_hal_state_t* state = context;
    if (state == NULL || request == NULL)
        return NX_ERROR_INVALID;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (state->uart.active != request)
        return nx_request_state(&request->base) == NX_REQUEST_SETTLED
                   ? NX_SUCCESS
                   : NX_ERROR_STATE;
    return abort_transfer(state, NX_ERROR_CANCELLED);
}

/** \brief Observe late DMA errors through the official exact-stream helpers. */
static uint32_t dma_errors(nx_stm32_uart_hal_state_t* state) {
    DMA_HandleTypeDef* dma = &state->dma;
    uint32_t mask = __HAL_DMA_GET_TE_FLAG_INDEX(dma) |
                    __HAL_DMA_GET_DME_FLAG_INDEX(dma) |
                    __HAL_DMA_GET_FE_FLAG_INDEX(dma);
    return __HAL_DMA_GET_FLAG(dma, mask);
}

/** \brief No request can be published while a source still retains its address.
 */
static bool detach(nx_stm32_uart_hal_state_t* state) {
    DMA_HandleTypeDef* dma = &state->dma;
    NVIC_DisableIRQ(state->config->transmit.irq);
    LL_USART_DisableIT_TC(state->handle.Instance);
    LL_USART_DisableIT_TXE(state->handle.Instance);
    __HAL_DMA_DISABLE_IT(dma, DMA_IT_TC | DMA_IT_HT | DMA_IT_TE | DMA_IT_DME);
    __HAL_DMA_DISABLE_IT(dma, DMA_IT_FE);
    nx_arch_dsb();
    if ((dma->Instance->CR & (DMA_SxCR_EN | DMA_SxCR_TCIE | DMA_SxCR_HTIE |
                              DMA_SxCR_TEIE | DMA_SxCR_DMEIE)) != 0U ||
        (dma->Instance->FCR & DMA_SxFCR_FEIE) != 0U ||
        (state->handle.Instance->CR1 & (USART_CR1_TXEIE | USART_CR1_TCIE)) !=
            0U ||
        NVIC_GetEnableIRQ(state->config->transmit.irq) != 0U ||
        NVIC_GetActive(state->config->transmit.irq) != 0U)
        return false;
    if (state->uart.terminal == NX_SUCCESS &&
        (dma_errors(state) != 0U || dma->ErrorCode != HAL_DMA_ERROR_NONE))
        state->uart.terminal = NX_ERROR_IO;
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
    state->handle.pTxBuffPtr = NULL;
    state->handle.TxXferCount = 0U;
    state->handle.TxXferSize = 0U;
    /* Error flags remain observable across the final posted completion ACK. */
    uint32_t complete =
        __HAL_DMA_GET_TC_FLAG_INDEX(dma) | __HAL_DMA_GET_HT_FLAG_INDEX(dma);
    __HAL_DMA_CLEAR_FLAG(dma, complete);
    NVIC_ClearPendingIRQ(state->config->transmit.irq);
    nx_arch_dsb();
    if (state->uart.terminal == NX_SUCCESS && dma_errors(state) != 0U)
        state->uart.terminal = NX_ERROR_IO;
    return dma->Instance->M0AR == 0U && dma->Instance->M1AR == 0U &&
           dma->Instance->PAR == 0U && dma->Instance->NDTR == 0U &&
           (dma->Instance->CR & (DMA_SxCR_EN | DMA_SxCR_TCIE | DMA_SxCR_HTIE |
                                 DMA_SxCR_TEIE | DMA_SxCR_DMEIE)) == 0U &&
           (dma->Instance->FCR & DMA_SxFCR_FEIE) == 0U &&
           NVIC_GetPendingIRQ(state->config->transmit.irq) == 0U;
}

static void service(void* context) {
    nx_stm32_uart_hal_state_t* state = context;
    if (state == NULL || !task_context() || !state->uart.initialized ||
        state->hal_depth != 0U || state->uart.active == NULL)
        return;
    if (!owned_domain(state)) {
        quarantine_domain(state);
        return;
    }
    if ((state->masked_vectors & NX_HAL_DMA_DRAIN_PENDING) != 0U) {
        if (!irq_enter(state, NX_HAL_DMA_VECTOR))
            return;
        bool progressed = progress_dma(state);
        leave_hal(state);
        if (!progressed)
            return;
    }
    nx_stm32_uart_state_t* uart = &state->uart;
    nx_time_us_t now = nx_time_now_us();
    if (!uart->tx_complete && !(state->flags & NX_HAL_ABORTING)) {
        if (uart->terminal != NX_SUCCESS ||
            state->handle.ErrorCode != HAL_UART_ERROR_NONE ||
            state->dma.ErrorCode != HAL_DMA_ERROR_NONE)
            (void)abort_transfer(state, NX_ERROR_IO);
        else if (nx_deadline_expired(uart->active->base.deadline, now))
            (void)abort_transfer(state, NX_ERROR_TIMEOUT);
        else if ((state->flags & NX_HAL_MEMORY_COMPLETE) &&
                 nx_deadline_expired(uart->drain_deadline, now)) {
            nx_time_us_t grace = uart->drain_deadline;
            (void)abort_transfer(state, NX_ERROR_IO);
            uart->drain_deadline = grace;
        }
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (state->hal_depth != 0U) {
        nx_arch_irq_restore(saved);
        return;
    }
    bool memory = (state->dma.Instance->CR & DMA_SxCR_EN) == 0U &&
                  (uart->registers->CR3 & USART_CR3_DMAT) == 0U &&
                  state->handle.gState == HAL_UART_STATE_READY;
    bool wire = (uart->registers->SR & USART_SR_TC) != 0U;
    if (!memory || !wire) {
        if ((state->flags & NX_HAL_ABORTING) &&
            nx_deadline_expired(uart->drain_deadline, nx_time_now_us()) &&
            nx_request_state(&uart->active->base) != NX_REQUEST_QUARANTINED)
            (void)nx_request_transition(&uart->active->base,
                                        NX_REQUEST_QUARANTINED);
        nx_arch_irq_restore(saved);
        return;
    }
    nx_uart_tx_request_t* request = uart->active;
    if (uart->terminal == NX_SUCCESS &&
        (state->dma.Instance->NDTR > request->length ||
         dma_errors(state) != 0U))
        uart->terminal = NX_ERROR_IO;
    if (!detach(state)) {
        nx_arch_irq_restore(saved);
        return;
    }
    if (state->dma.State != HAL_DMA_STATE_READY) {
        /* Wire TC can precede the forced-disable DMA TC callback. The complete
         * detach above already proves its address/source lease has ended.
         * Reinitialize the idle HAL transport through its own API, outside the
         * CPU mask, before allowing another request to reuse this Handle.
         */
        state->hal_depth = 1U;
        nx_arch_irq_restore(saved);
        HAL_StatusTypeDef reset = HAL_DMA_Init(&state->dma);
        leave_hal(state);
        saved = nx_arch_irq_save();
        if (reset != HAL_OK || !configured_dma(state) || !detach(state)) {
            if (nx_request_state(&request->base) != NX_REQUEST_QUARANTINED)
                (void)nx_request_transition(&request->base,
                                            NX_REQUEST_QUARANTINED);
            nx_arch_irq_restore(saved);
            return;
        }
    }
    nx_result_t terminal = uart->terminal;
    size_t transferred = terminal == NX_SUCCESS ? request->length : 0U;
    uart->active = NULL;
    nx_request_settle(&request->base, terminal, transferred);
    nx_arch_irq_restore(saved);
}

static nx_result_t stop(void* context) {
    nx_stm32_uart_hal_state_t* state = context;
    if (state == NULL)
        return NX_ERROR_INVALID;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    nx_stm32_uart_state_t* uart = &state->uart;
    if (!uart->initialized)
        return NX_SUCCESS;
    if (!owned_route(state) ||
        (!(state->flags & NX_HAL_DEINITIALIZING) && !owned_domain(state))) {
        quarantine_domain(state);
        return NX_ERROR_BUSY;
    }
    uart->closing = true;
    LL_USART_DisableIT_RXNE(uart->registers);
    LL_USART_DisableIT_PE(uart->registers);
    LL_USART_DisableIT_ERROR(uart->registers);
    if (uart->active != NULL) {
        (void)abort_transfer(state, NX_ERROR_CANCELLED);
        service(state);
        if (uart->active != NULL)
            return NX_ERROR_BUSY;
    }
    NVIC_DisableIRQ(uart->irq);
    NVIC_DisableIRQ(state->config->transmit.irq);
    NVIC_ClearPendingIRQ(uart->irq);
    NVIC_ClearPendingIRQ(state->config->transmit.irq);
    nx_arch_dsb();
    if (state->hal_depth != 0U || NVIC_GetEnableIRQ(uart->irq) != 0U ||
        NVIC_GetEnableIRQ(state->config->transmit.irq) != 0U ||
        NVIC_GetPendingIRQ(uart->irq) != 0U ||
        NVIC_GetPendingIRQ(state->config->transmit.irq) != 0U ||
        NVIC_GetActive(uart->irq) != 0U ||
        NVIC_GetActive(state->config->transmit.irq) != 0U ||
        (state->dma.Instance->CR & DMA_SxCR_EN) != 0U)
        return NX_ERROR_BUSY;
    state->flags |= NX_HAL_DEINITIALIZING;
    LL_USART_DisableDMAReq_TX(uart->registers);
    LL_USART_DisableDMAReq_RX(uart->registers);
    LL_USART_DisableIT_CTS(uart->registers);
    LL_USART_DisableIT_LBD(uart->registers);
    if (HAL_UART_DeInit(&state->handle) != HAL_OK ||
        HAL_DMA_DeInit(&state->dma) != HAL_OK)
        return NX_ERROR_IO;
    nx_arch_dsb();
    if ((uart->registers->CR1 &
         (USART_CR1_UE | USART_CR1_RXNEIE | USART_CR1_PEIE | USART_CR1_TXEIE |
          USART_CR1_TCIE | USART_CR1_IDLEIE)) != 0U ||
        (uart->registers->CR2 & USART_CR2_LBDIE) != 0U ||
        (uart->registers->CR3 & (USART_CR3_DMAT | USART_CR3_DMAR |
                                 USART_CR3_EIE | USART_CR3_CTSIE)) != 0U ||
        (state->dma.Instance->CR &
         (DMA_SxCR_EN | DMA_SxCR_TCIE | DMA_SxCR_HTIE | DMA_SxCR_TEIE |
          DMA_SxCR_DMEIE)) != 0U ||
        (state->dma.Instance->FCR & DMA_SxFCR_FEIE) != 0U ||
        state->dma.Instance->M0AR != 0U || state->dma.Instance->M1AR != 0U ||
        state->dma.Instance->PAR != 0U || state->dma.Instance->NDTR != 0U ||
        NVIC_GetEnableIRQ(uart->irq) != 0U ||
        NVIC_GetEnableIRQ(state->config->transmit.irq) != 0U ||
        NVIC_GetPendingIRQ(uart->irq) != 0U ||
        NVIC_GetPendingIRQ(state->config->transmit.irq) != 0U ||
        NVIC_GetActive(uart->irq) != 0U ||
        NVIC_GetActive(state->config->transmit.irq) != 0U)
        return NX_ERROR_IO;
    uart->wake = NULL;
    uart->rx_head = uart->rx_tail = uart->rx_count = 0U;
    uart->rx_loss = 0U;
    uart->initialized = false;
    state->flags &= (uint8_t)~NX_HAL_DEINITIALIZING;
    state->masked_vectors = 0U;
    return NX_SUCCESS;
}

static nx_result_t attach_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t syscall_ceiling) {
    nx_stm32_uart_hal_state_t* state = context;
    if (state == NULL)
        return NX_ERROR_INVALID;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!state->uart.initialized || (state->uart.closing && wake != NULL))
        return NX_ERROR_STATE;
    nx_result_t result =
        nx_stm32_irq_wake_validate(wake, state->uart.irq, syscall_ceiling);
    if (result == NX_SUCCESS)
        result = nx_stm32_irq_wake_validate(wake, state->config->transmit.irq,
                                            syscall_ceiling);
    if (result == NX_SUCCESS) {
        nx_arch_irq_state_t saved = nx_arch_irq_save();
        state->uart.wake = wake;
        nx_arch_irq_restore(saved);
    }
    return result;
}

const nx_uart_ops_t nx_stm32_uart_hal_ops = {
    .submit = submit,
    .start_admitted = start_admitted,
    .cancel = cancel,
    .service = service,
    .read_events = nx_stm32_uart_read_events,
    .read_bytes = nx_stm32_uart_read_bytes,
    .stop = stop,
    .attach_wake = attach_wake,
};

/** \brief Keep hardware and lease ownership distinct through startup failure.
 */
nx_result_t
nx_stm32_uart_hal_cold_prepare(nx_stm32_uart_hal_state_t* state,
                               const nx_stm32_uart_dma_cold_config_t* config,
                               nx_stm32_uart_cold_receipt_t* receipt) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || config == NULL || receipt == NULL)
        return NX_ERROR_INVALID;
    if (state->uart.initialized || state->uart.active != NULL ||
        state->hal_depth != 0U || state->masked_vectors != 0U)
        return NX_ERROR_BUSY;
    nx_uart_format_t format;
    nx_result_t result = configuration(config, &format);
    if (result != NX_SUCCESS)
        return result;
    if (!nx_stm32_uart_cold_dma_valid(config, false, true))
        return NX_ERROR_INVALID;
    if ((config->transmit.registers->CR & DMA_SxCR_EN) != 0U)
        return NX_ERROR_BUSY;
    result = nx_stm32_uart_cold_acquire(&config->uart, receipt);
    if (result != NX_SUCCESS)
        return result;
    result = nx_stm32_uart_hal_initialize(state, config);
    if (result == NX_SUCCESS)
        result = nx_stm32_uart_cold_verify(&config->uart, receipt);
    if (result != NX_SUCCESS)
        (void)nx_stm32_uart_hal_cold_close(state, config, receipt);
    return result;
}

/** \brief Release resources only after the concrete runtime is fully stopped.
 */
nx_result_t
nx_stm32_uart_hal_cold_close(nx_stm32_uart_hal_state_t* state,
                             const nx_stm32_uart_dma_cold_config_t* config,
                             nx_stm32_uart_cold_receipt_t* receipt) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || config == NULL || receipt == NULL)
        return NX_ERROR_INVALID;
    if (receipt->effects == 0U && receipt->clock_added == 0U)
        return state->uart.initialized || state->uart.active != NULL
                   ? NX_ERROR_BUSY
                   : NX_SUCCESS;
    nx_result_t result = state->uart.initialized ? stop(state) : NX_SUCCESS;
    if (result == NX_SUCCESS)
        result = nx_stm32_uart_cold_release(&config->uart, receipt);
    if (result == NX_SUCCESS)
        memset(state, 0, sizeof(*state));
    return result;
}
