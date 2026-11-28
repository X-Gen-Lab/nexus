/**
 * \file            uart.c
 * \brief           F1 USART2 IRQ data path and checked hardware withdrawal.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "dispatch_checks.h"
#include "instance.h"
#include "nexus_config.h"
#include "private/hardware.h"
#include "stm32f1xx_ll_gpio.h"
#include "stm32f1xx_ll_usart.h"
#include "system/platform.h"

#ifndef NX_STM32F1_UART_STATUS
#define NX_STM32F1_UART_STATUS(reg)  ((reg)->SR)
#define NX_STM32F1_UART_RECEIVE(reg) LL_USART_ReceiveData8(reg)
#define NX_STM32F1_UART_TRANSMIT(reg, data)                                    \
    LL_USART_TransmitData8((reg), (data))
#endif

#ifndef NX_STM32F1_IRQ_ENABLED
#define NX_STM32F1_IRQ_ENABLED(irq)             NVIC_GetEnableIRQ(irq)
#define NX_STM32F1_IRQ_PENDING(irq)             NVIC_GetPendingIRQ(irq)
#define NX_STM32F1_IRQ_ACTIVE(irq)              NVIC_GetActive(irq)
#define NX_STM32F1_IRQ_PRIORITY(irq)            NVIC_GetPriority(irq)
#define NX_STM32F1_IRQ_SET_PRIORITY(irq, value) NVIC_SetPriority((irq), (value))
#define NX_STM32F1_IRQ_ENABLE(irq)              NVIC_EnableIRQ(irq)
#define NX_STM32F1_IRQ_DISABLE(irq)             NVIC_DisableIRQ(irq)
#define NX_STM32F1_IRQ_CLEAR(irq)               NVIC_ClearPendingIRQ(irq)
#endif

#if defined(NEXUS_CLOCK_APB1_HZ)
#define UART_CLOCK_HZ NEXUS_CLOCK_APB1_HZ
#else
#define UART_CLOCK_HZ 8000000U
#endif

#define RX_ERRORS                                                              \
    ((uint32_t)(USART_SR_PE | USART_SR_FE | USART_SR_NE | USART_SR_ORE))
#define RX_STATUS      (RX_ERRORS | USART_SR_RXNE)
#define TX_INTERRUPTS  ((uint32_t)(USART_CR1_TXEIE | USART_CR1_TCIE))
#define RX_CONTROLS    ((uint32_t)(USART_CR1_RXNEIE | USART_CR1_PEIE))
#define UART_CONTROLS  ((uint32_t)(USART_CR1_UE | USART_CR1_RE | USART_CR1_TE))
#define FRAME_CONTROLS ((uint32_t)(USART_CR1_M | USART_CR1_PCE | USART_CR1_PS))

#ifdef NEXUS_STM32F1_UART_WIRE
#define UART_WIRE true
#define UART_WIRE_ENABLED(protocol)                                            \
    ((NEXUS_STM32F1_UART_WIRE & (UINT32_C(1) << (protocol))) != 0U)
#else
#define UART_WIRE                   false
#define UART_WIRE_ENABLED(protocol) false
#endif
#define RX_DIRECTION   (RX_CONTROLS | USART_CR1_RE)
#define DIRECTION_MASK (RX_DIRECTION | USART_CR1_TE)
#define LIN_CONTROLS                                                           \
    ((uint32_t)(USART_CR2_LINEN | USART_CR2_LBDIE | USART_CR2_LBDL))

#define CTS_PAD       (UINT32_C(1) << 0U)
#define RTS_PAD       (UINT32_C(1) << 1U)
#define RTS_CRL_MASK  (UINT32_C(0xf) << 4U)
#define RTS_HIGH_MODE ((uint32_t)LL_GPIO_SPEED_FREQ_LOW << 4U)
#define RTS_AF_MODE   ((uint32_t)(GPIO_CRL_CNF0_1 | LL_GPIO_SPEED_FREQ_LOW) << 4U)

static bool hardware_flow(const nx_stm32f1_uart_config_t* config) {
    return UART_WIRE_ENABLED(NX_UART_WIRE_ASYNCHRONOUS) &&
           config->wire == NX_UART_WIRE_ASYNCHRONOUS;
}

/** \brief Switch the owned nRTS pad between its parked latch and USART. */
static bool rts_hold(bool hold) {
    const uint32_t mode = hold ? RTS_HIGH_MODE : RTS_AF_MODE;
    return (!hold || (GPIOA->ODR & RTS_PAD) != 0U) &&
           ((GPIOA->CRL & RTS_CRL_MASK) == mode ||
            nx_stm32f1_write_masked(&GPIOA->CRL, RTS_CRL_MASK, mode));
}

static bool half_duplex(const nx_stm32f1_uart_config_t* config) {
    return UART_WIRE_ENABLED(NX_UART_WIRE_HALF_DUPLEX) &&
           config->wire == NX_UART_WIRE_HALF_DUPLEX;
}

#define IRDA_TX_CRL_MASK (UINT32_C(0xf) << 8U)
#define IRDA_TX_PARKED   ((uint32_t)LL_GPIO_SPEED_FREQ_LOW << 8U)
#define IRDA_TX_AF       ((uint32_t)(GPIO_CRL_CNF0_1 | LL_GPIO_SPEED_FREQ_LOW) << 8U)
#define IRDA_RX_GUARD_US UINT32_C(10000)

static bool irda(const nx_stm32f1_uart_config_t* config) {
    return UART_WIRE_ENABLED(NX_UART_WIRE_IRDA) &&
           config->wire == NX_UART_WIRE_IRDA;
}

#define SYNC_CONTROLS                                                          \
    ((uint32_t)(USART_CR2_CLKEN | USART_CR2_CPOL | USART_CR2_CPHA |            \
                USART_CR2_LBCL))
#define CK_PAD      (UINT32_C(1) << 4U)
#define CK_CRL_MASK (UINT32_C(0xf) << 16U)
#define CK_PARKED   ((uint32_t)LL_GPIO_SPEED_FREQ_LOW << 16U)
#define CK_AF       ((uint32_t)(GPIO_CRL_CNF0_1 | LL_GPIO_SPEED_FREQ_LOW) << 16U)

static bool synchronous(const nx_stm32f1_uart_config_t* config) {
    return UART_WIRE_ENABLED(NX_UART_WIRE_SYNCHRONOUS) &&
           config->wire == NX_UART_WIRE_SYNCHRONOUS;
}

/** \brief Park CK at its preloaded idle polarity during cold transitions. */
static bool sync_clock_pad(bool enabled) {
    return nx_stm32f1_write_masked(&GPIOA->CRL, CK_CRL_MASK,
                                   enabled ? CK_AF : CK_PARKED);
}

static bool directional(const nx_stm32f1_uart_config_t* config) {
    return half_duplex(config) || irda(config);
}

/** \brief Keep the SIR output low while configuring or removing its encoder. */
static bool irda_tx_pad(bool encoder) {
    const uint32_t mode = encoder ? IRDA_TX_AF : IRDA_TX_PARKED;
    return nx_stm32f1_write_masked(&GPIOA->CRL, IRDA_TX_CRL_MASK, mode);
}

static uint32_t cr3_controls(const nx_stm32f1_uart_config_t* config) {
    return USART_CR3_EIE | (half_duplex(config) ? USART_CR3_HDSEL : 0U) |
           (irda(config) ? USART_CR3_IREN : 0U) |
           (hardware_flow(config) ? USART_CR3_CTSE | USART_CR3_RTSE : 0U);
}

static bool lin(const nx_stm32f1_uart_config_t* config) {
    return UART_WIRE_ENABLED(NX_UART_WIRE_LIN) &&
           config->wire == NX_UART_WIRE_LIN;
}

static bool lin_break(const nx_stm32f1_uart_t* state,
                      const nx_uart_tx_request_t* request) {
    return lin(state->config) && request->length == 0U && request->data == NULL;
}

static uint32_t receive_status(const nx_stm32f1_uart_config_t* config) {
    return RX_STATUS | (lin(config) ? USART_SR_LBD : 0U);
}

/** \brief Offer a new RX slot only after software and RDR are ready. */
static bool release_rts(nx_stm32f1_uart_t* state) {
    if (state->closing || state->loss ||
        state->rx_count >= state->config->rx_capacity ||
        (GPIOA->CRL & RTS_CRL_MASK) == RTS_AF_MODE ||
        (NX_STM32F1_UART_STATUS(state->config->registers) & RX_STATUS) != 0U)
        return true;
    return rts_hold(false);
}

/** \brief Size the selected RX profile without allocating the other storage. */
static size_t rx_stride(const nx_stm32f1_uart_config_t* config) {
    return config->profile == NX_UART_RX_BYTES ? 1U
                                               : sizeof(nx_uart_rx_event_t);
}

static bool uses_remap(const nx_stm32f1_uart_config_t* config) {
#if defined(STM32F107xC)
    return config != NULL && config->tx_registers == GPIOD &&
           config->tx_pin == 5U && config->rx_registers == GPIOD &&
           config->rx_pin == 6U;
#else
    (void)config;
    return false;
#endif
}

/** \brief Reject invalid or overlapping storage before a hardware loan. */
nx_result_t nx_stm32f1_uart_admit(const nx_stm32f1_uart_t* state,
                                  const nx_stm32f1_uart_config_t* config) {
    const bool remap = uses_remap(config);
    if (state == NULL || config == NULL || config->registers != USART2 ||
        config->irq != USART2_IRQn || config->clock_hz != UART_CLOCK_HZ ||
        (half_duplex(config)
             ? (config->rx_registers != NULL || config->rx_pin != 0U ||
                config->profile != NX_UART_RX_BYTES || config->frame != 0U)
             : (!remap &&
                (config->rx_registers != GPIOA || config->rx_pin != 3U))) ||
        config->baud < 1200U || config->baud > 500000U ||
        config->priority >= (1U << __NVIC_PRIO_BITS) ||
        (config->frame & 0xF0U) != 0U ||
        ((config->frame & (NX_STM32F1_UART_WORD9 | NX_STM32F1_UART_ODD)) !=
             0U &&
         (config->frame & NX_STM32F1_UART_PARITY) == 0U)) {
        return NX_ERROR_INVALID;
    }
    if (UART_WIRE &&
        ((lin(config) &&
          (config->profile != NX_UART_RX_EVENTS || config->frame != 0U ||
           config->baud > 20000U ||
           (config->wire_options != 10U && config->wire_options != 11U))) ||
         (half_duplex(config) && config->wire_options != 0U) ||
         (hardware_flow(config) &&
          (config->profile != NX_UART_RX_EVENTS || config->frame != 0U ||
           config->wire_options != 0U)) ||
         (irda(config) &&
          (config->profile != NX_UART_RX_EVENTS || config->frame != 0U ||
           config->baud > 115200U || config->wire_options != 0U)) ||
         (synchronous(config) &&
          (config->profile != NX_UART_RX_EVENTS || config->frame != 0U ||
           config->wire_options >
               (NX_STM32F1_UART_SYNC_CPOL | NX_STM32F1_UART_SYNC_CPHA |
                NX_STM32F1_UART_SYNC_LBCL))) ||
         config->wire > NX_UART_WIRE_IRDA ||
         !UART_WIRE_ENABLED(config->wire))) {
        return NX_ERROR_INVALID;
    }
    if (config->profile == NX_UART_RX_BLOCKS) {
        if (config->tx_registers != NULL || config->tx_pin != 0U ||
            config->rx_storage != NULL || config->rx_capacity != 0U ||
            config->frame != 0U) {
            return NX_ERROR_INVALID;
        }
    } else {
        const size_t stride = rx_stride(config);
        const size_t alignment = config->profile == NX_UART_RX_BYTES
                                     ? 1U
                                     : _Alignof(nx_uart_rx_event_t);
        if ((!remap &&
             (config->tx_registers != GPIOA || config->tx_pin != 2U)) ||
            (config->profile != NX_UART_RX_BYTES &&
             config->profile != NX_UART_RX_EVENTS) ||
            config->rx_capacity == 0U ||
            config->rx_capacity > SIZE_MAX / stride ||
            !nx_io_dispatch_span(config->rx_storage,
                                 config->rx_capacity * stride, alignment) ||
            nx_io_dispatch_overlap(config->rx_storage,
                                   config->rx_capacity * stride, state,
                                   sizeof(*state)) ||
            nx_io_dispatch_overlap(config->rx_storage,
                                   config->rx_capacity * stride, config,
                                   sizeof(*config))) {
            return NX_ERROR_INVALID;
        }
    }
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->config != NULL) {
        return NX_ERROR_STATE;
    }
    /* ES096 2.3.2/2.3.3: USART2 CK depends on SPI1. An external owner
     * may have left SPI1 clocked even when the assembly excludes it. */
    return (synchronous(config) && (RCC->APB2ENR & RCC_APB2ENR_SPI1EN) != 0U) ||
                   NX_STM32F1_IRQ_ENABLED(config->irq) ||
                   NX_STM32F1_IRQ_PENDING(config->irq) ||
                   NX_STM32F1_IRQ_ACTIVE(config->irq)
               ? NX_ERROR_BUSY
               : NX_SUCCESS;
}

/** \brief Omit MAPR writes and temporarily clock just its readable route bit.
 */
static nx_result_t check_default_route(nx_stm32f1_uart_t* state) {
    const uint32_t gate = RCC_APB2ENR_AFIOEN;
    state->afio_gate_added = (RCC->APB2ENR & gate) == 0U;
    if (state->afio_gate_added &&
        !nx_stm32f1_write_masked(&RCC->APB2ENR, gate, gate)) {
        return NX_ERROR_IO;
    }
    const bool remapped = (AFIO->MAPR & AFIO_MAPR_USART2_REMAP) != 0U;
    if (state->afio_gate_added &&
        !nx_stm32f1_write_masked(&RCC->APB2ENR, gate, 0U)) {
        return NX_ERROR_IO;
    }
    state->afio_gate_added = false;
    return remapped ? NX_ERROR_BUSY : NX_SUCCESS;
}

#if defined(STM32F107xC)
/** \brief Commit only USART2_REMAP; RM0008 and ST LL use SWJ=111 for no effect.
 */
static bool set_usart2_remap(bool enabled) {
    const uint32_t remap = enabled ? AFIO_MAPR_USART2_REMAP : 0U;
    const nx_arch_irq_state_t irq = nx_arch_irq_save();
    const uint32_t previous = AFIO->MAPR;
    if ((previous & AFIO_MAPR_USART2_REMAP) != remap) {
        NX_STM32F1_WRITE(&AFIO->MAPR,
                         (previous & ~(uint32_t)AFIO_MAPR_USART2_REMAP) |
                             AFIO_MAPR_SWJ_CFG | remap);
        nx_arch_dsb();
    }
    const bool applied = (AFIO->MAPR & AFIO_MAPR_USART2_REMAP) == remap;
    nx_arch_irq_restore(irq);
    return applied;
}

/** \brief Acquire a reversible field lease after USART2 is inactive. */
static nx_result_t acquire_remap(nx_stm32f1_uart_t* state) {
    const uint32_t gate = RCC_APB2ENR_AFIOEN;
    state->afio_gate_added = (RCC->APB2ENR & gate) == 0U;
    if (state->afio_gate_added &&
        !nx_stm32f1_write_masked(&RCC->APB2ENR, gate, gate)) {
        return NX_ERROR_IO;
    }
    state->remap_owned = (AFIO->MAPR & AFIO_MAPR_USART2_REMAP) == 0U;
    return !state->remap_owned || set_usart2_remap(true) ? NX_SUCCESS
                                                         : NX_ERROR_IO;
}
#endif

/** \brief Establish the selected wire framing before enabling the USART. */
nx_result_t nx_stm32f1_uart_prepare(nx_stm32f1_uart_t* state,
                                    const nx_stm32f1_uart_config_t* config) {
    nx_result_t result = nx_stm32f1_uart_admit(state, config);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (!nx_stm32f1_system_live()) {
        return NX_ERROR_STATE;
    }
    state->config = config;
    const bool remap = uses_remap(config);
    if (!remap) {
        result = check_default_route(state);
        if (result != NX_SUCCESS) {
            if (!state->afio_gate_added) {
                state->config = NULL;
            }
            return result;
        }
    }
    const uint32_t gate = RCC_APB1ENR_USART2EN;
    state->usart_gate_added = (RCC->APB1ENR & gate) == 0U;
    if (state->usart_gate_added &&
        !nx_stm32f1_write_masked(&RCC->APB1ENR, gate, gate)) {
        return NX_ERROR_IO;
    }
    USART_TypeDef* registers = config->registers;
    if (registers->CR1 != 0U || registers->CR2 != 0U || registers->CR3 != 0U ||
        registers->BRR != 0U || registers->GTPR != 0U ||
        (NX_STM32F1_UART_STATUS(registers) & receive_status(config)) != 0U) {
        return NX_ERROR_BUSY;
    }
    const bool rx_only = config->profile == NX_UART_RX_BLOCKS;
    const bool flow = hardware_flow(config);
    const bool sir = irda(config);
    const bool clocked = synchronous(config);
    const unsigned tx = config->tx_pin;
    const unsigned rx = config->rx_pin;
    const uint16_t pins =
        (uint16_t)((half_duplex(config) ? 0U : 1U << rx) |
                   (rx_only ? 0U : 1U << tx) | (flow ? CTS_PAD | RTS_PAD : 0U) |
                   (clocked ? CK_PAD : 0U));
    result = nx_stm32f1_pads_capture(&state->pads, remap ? GPIOD : GPIOA, pins);
    if (result != NX_SUCCESS) {
        return result;
    }
#if defined(STM32F107xC)
    if (remap) {
        result = acquire_remap(state);
        if (result != NX_SUCCESS) {
            return result;
        }
    }
#endif
    /* RM0008 27.3.10: PA2 remains open-drain, with an external pull-up.
     * HDSEL connects the internal receiver to this pad; PA3 stays untouched. */
    const uint32_t tx_mode = GPIO_CRL_CNF0_1 | LL_GPIO_SPEED_FREQ_LOW |
                             (half_duplex(config) ? LL_GPIO_OUTPUT_OPENDRAIN
                                                  : LL_GPIO_OUTPUT_PUSHPULL);
    const uint32_t rx_mode = sir ? GPIO_CRL_CNF0_1 : LL_GPIO_MODE_FLOATING;
    const uint32_t crl = (half_duplex(config) ? 0U : rx_mode << (4U * rx)) |
                         (rx_only ? 0U
                          : sir   ? IRDA_TX_PARKED
                                  : tx_mode << (4U * tx)) |
                         (flow ? GPIO_CRL_CNF0_1 | RTS_HIGH_MODE : 0U) |
                         (clocked ? CK_PARKED : 0U);
    /* The hardware flow pins park inactive. SIR TX idles low and RX high. */
    uint32_t latch = state->pads.latch | (rx_only ? 0U : 1U << tx) |
                     (flow ? CTS_PAD | RTS_PAD : 0U);
    if (sir)
        latch = (latch & ~(UINT32_C(1) << tx)) | (UINT32_C(1) << rx);
    if (clocked)
        latch =
            (latch & ~CK_PAD) |
            ((config->wire_options & NX_STM32F1_UART_SYNC_CPOL) != 0U ? CK_PAD
                                                                      : 0U);
    result = nx_stm32f1_pads_apply(&state->pads, crl, 0U, latch);
    if (result != NX_SUCCESS) {
        return result;
    }
    state->controls_owned = true;
    const uint32_t divisor =
        (config->clock_hz + (sir ? config->baud - 1U : config->baud / 2U)) /
        config->baud;
    const uint32_t controls = rx_only ? USART_CR1_UE
                              : directional(config)
                                  ? USART_CR1_UE | RX_DIRECTION
                                  : UART_CONTROLS | RX_CONTROLS;
    const uint32_t frame =
        ((config->frame & NX_STM32F1_UART_WORD9) ? USART_CR1_M : 0U) |
        ((config->frame & NX_STM32F1_UART_PARITY) ? USART_CR1_PCE : 0U) |
        ((config->frame & NX_STM32F1_UART_ODD) ? USART_CR1_PS : 0U);
    uint32_t cr2 = (config->frame & NX_STM32F1_UART_STOP2) != 0U
                       ? LL_USART_STOPBITS_2
                       : 0U;
    if (lin(config))
        cr2 |= USART_CR2_LINEN | USART_CR2_LBDIE |
               (config->wire_options == 11U ? USART_CR2_LBDL : 0U);
    /* RM0008 27.3.9: configure clock with both directions disabled,
     * then enable TE and RE together before exposing CK on PA4. */
    if (clocked)
        cr2 |= USART_CR2_CLKEN |
               ((uint32_t)config->wire_options << USART_CR2_LBCL_Pos);
    const uint32_t cr3 = cr3_controls(config);
    if (!nx_stm32f1_write_masked(&registers->BRR, UINT16_MAX, divisor) ||
        (cr2 != 0U && !nx_stm32f1_write_masked(&registers->CR2, cr2, cr2)) ||
        (sir &&
         !nx_stm32f1_write_masked(&registers->GTPR, USART_GTPR_PSC, 1U)) ||
        (!rx_only && !nx_stm32f1_write_masked(&registers->CR3, cr3, cr3)) ||
        !nx_stm32f1_write_masked(&registers->CR1,
                                 UART_CONTROLS | RX_CONTROLS | FRAME_CONTROLS,
                                 controls | frame) ||
        (sir && !irda_tx_pad(true)) || (clocked && !sync_clock_pad(true))) {
        return NX_ERROR_IO;
    }
    state->previous_priority = (uint8_t)NX_STM32F1_IRQ_PRIORITY(config->irq);
    state->vector_owned = true;
    NX_STM32F1_IRQ_SET_PRIORITY(config->irq, config->priority);
    if (NX_STM32F1_IRQ_PRIORITY(config->irq) != config->priority) {
        return NX_ERROR_IO;
    }
    const nx_arch_irq_state_t mask = nx_arch_irq_save();
    state->live = true;
    NX_STM32F1_IRQ_ENABLE(config->irq);
    const bool ready =
        NX_STM32F1_IRQ_ENABLED(config->irq) && (!flow || release_rts(state));
    if (!ready) {
        state->live = false;
        NX_STM32F1_IRQ_DISABLE(config->irq);
    }
    nx_arch_irq_restore(mask);
    return ready ? NX_SUCCESS : NX_ERROR_IO;
}

/** \brief Prove a borrow remains outside the provider's mutable storage. */
static bool independent(const nx_stm32f1_uart_t* state, const void* address,
                        size_t bytes) {
    return !nx_io_dispatch_overlap(address, bytes, state, sizeof(*state)) &&
           !nx_io_dispatch_overlap(address, bytes, state->config,
                                   sizeof(*state->config)) &&
           !nx_io_dispatch_overlap(address, bytes, state->config->rx_storage,
                                   state->config->rx_capacity *
                                       rx_stride(state->config));
}

/** \brief Apply the same pure checks to the exact UART face and its adapter. */
nx_result_t nx_stm32f1_uart_validate(void* context,
                                     const nx_uart_tx_request_t* request) {
    const nx_stm32f1_uart_t* state = context;
    if (state == NULL || !nx_io_dispatch_span(request, sizeof(*request),
                                              _Alignof(nx_uart_tx_request_t))) {
        return NX_ERROR_INVALID;
    }
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->live || state->closing) {
        return NX_ERROR_STATE;
    }
    if (state->active != NULL) {
        return NX_ERROR_BUSY;
    }
    if (!independent(state, request, sizeof(*request)) ||
        (!lin_break(state, request) &&
         (request->length == 0U ||
          !nx_io_dispatch_span(request->data, request->length, 1U) ||
          !independent(state, request->data, request->length) ||
          nx_io_dispatch_overlap(request, sizeof(*request), request->data,
                                 request->length)))) {
        return NX_ERROR_INVALID;
    }
    const nx_request_state_t phase = nx_request_state(&request->base);
    if (phase != NX_REQUEST_READY && phase != NX_REQUEST_QUEUED) {
        return NX_ERROR_STATE;
    }
    return nx_deadline_expired(request->base.deadline, nx_time_now_us())
               ? NX_ERROR_TIMEOUT
               : NX_SUCCESS;
}

/** \brief A hardware break has no DR write to clear an old TC. */
static void start_break(nx_stm32f1_uart_t* state) {
    USART_TypeDef* registers = state->config->registers;
    /* Match the STM32F1 LL W0C clear, preserving other status flags. */
    NX_STM32F1_WRITE(&registers->SR, ~(uint32_t)USART_SR_TC);
    nx_arch_dsb();
    if ((NX_STM32F1_UART_STATUS(registers) & USART_SR_TC) != 0U) {
        state->wire_done = true;
    } else if (nx_stm32f1_write_masked(&registers->CR1,
                                       TX_INTERRUPTS | USART_CR1_SBK,
                                       USART_CR1_TCIE | USART_CR1_SBK)) {
        return;
    }
    state->terminal = NX_ERROR_IO;
    state->closing = true;
    state->drain_deadline = nx_time_now_us();
}

/** \brief Move a single request into IRQ TX after successful admission. */
static nx_result_t begin_tx(nx_stm32f1_uart_t* state,
                            nx_uart_tx_request_t* request, bool admitted) {
    nx_result_t result = nx_stm32f1_uart_validate(state, request);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    USART_TypeDef* registers = state->config->registers;
    const nx_stm32f1_uart_config_t* config = state->config;
    if (directional(config) &&
        ((GPIOA->IDR &
          (1U << (irda(config) ? config->rx_pin : config->tx_pin))) == 0U ||
         (NX_STM32F1_UART_STATUS(registers) & RX_STATUS) != 0U)) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_BUSY;
    }
    result = admitted ? nx_request_transition(&request->base, NX_REQUEST_ACTIVE)
                      : nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result == NX_SUCCESS) {
        state->active = request;
        state->sent = 0U;
        state->wire_done = false;
        state->terminal = NX_SUCCESS;
        state->drain_deadline = NX_DEADLINE_NEVER;
        const uint32_t controls =
            TX_INTERRUPTS | (directional(config) ? DIRECTION_MASK : 0U);
        const uint32_t transmit =
            USART_CR1_TXEIE | (directional(config) ? USART_CR1_TE : 0U);
        if (lin_break(state, request)) {
            start_break(state);
        } else if (!nx_stm32f1_write_masked(&registers->CR1, controls,
                                            transmit)) {
            /* The loan was accepted. Service will verify source isolation. */
            state->terminal = NX_ERROR_IO;
            state->wire_done = true;
            state->closing = true;
        }
    }
    nx_arch_irq_restore(mask);
    return result;
}

/** \brief Admit a READY descriptor directly. */
static nx_result_t uart_submit(void* context, nx_uart_tx_request_t* request) {
    return begin_tx(context, request, false);
}

/** \brief Start an already admitted QUEUED descriptor. */
static nx_result_t uart_start_admitted(void* context,
                                       nx_uart_tx_request_t* request) {
    return begin_tx(context, request, true);
}

/** \brief Isolate payload reads, preserving already written wire ownership. */
static void drain(nx_stm32f1_uart_t* state, nx_result_t terminal,
                  nx_time_us_t now) {
    state->terminal = terminal;
    state->wire_done = state->sent == 0U && !lin_break(state, state->active);
    (void)nx_request_transition(&state->active->base, NX_REQUEST_DRAINING);
    const uint32_t frame = state->config->frame;
    const uint32_t frame_bits = 10U + ((frame & NX_STM32F1_UART_WORD9) != 0U) +
                                ((frame & NX_STM32F1_UART_STOP2) != 0U);
    const uint32_t grace_us =
        2000000U * frame_bits / state->config->baud + 100U;
    if (nx_deadline_finite_after(now, grace_us, &state->drain_deadline) !=
        NX_SUCCESS) {
        state->drain_deadline = 0U;
    }
    if (!nx_stm32f1_write_masked(&state->config->registers->CR1, TX_INTERRUPTS,
                                 state->wire_done ? 0U : USART_CR1_TCIE)) {
        state->closing = true;
    }
}

/** \brief Preserve the first observed terminal fact across later cancellation.
 */
static nx_result_t uart_cancel(void* context, nx_uart_tx_request_t* request) {
    nx_stm32f1_uart_t* state = context;
    if (request == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    nx_result_t result = NX_SUCCESS;
    const nx_request_state_t phase = nx_request_state(&request->base);
    if (state->active == request) {
        if (phase == NX_REQUEST_ACTIVE && !state->wire_done) {
            drain(state, NX_ERROR_CANCELLED, nx_time_now_us());
        }
    } else if (phase != NX_REQUEST_SETTLED) {
        result = NX_ERROR_STATE;
    }
    nx_arch_irq_restore(mask);
    return result;
}

/** \brief Map F1's single SR observation into portable RX facts. */
static uint32_t rx_flags(uint32_t sr) {
    return ((sr & USART_SR_PE) != 0U ? NX_UART_EVENT_PARITY : 0U) |
           ((sr & USART_SR_FE) != 0U ? NX_UART_EVENT_FRAMING : 0U) |
           ((sr & USART_SR_NE) != 0U ? NX_UART_EVENT_NOISE : 0U) |
           ((sr & USART_SR_ORE) != 0U ? NX_UART_EVENT_OVERRUN : 0U) |
           ((sr & USART_SR_LBD) != 0U ? NX_UART_EVENT_BREAK : 0U) |
           ((sr & USART_SR_RXNE) == 0U || (sr & USART_SR_LBD) != 0U
                ? NX_UART_EVENT_NO_BYTE
                : 0U);
}

/** \brief Read DR once after SR, even when the software buffer is full. */
static void receive(nx_stm32f1_uart_t* state, uint32_t sr, uint8_t payload) {
    const nx_stm32f1_uart_config_t* config = state->config;
    USART_TypeDef* registers = config->registers;
    const bool flow = hardware_flow(config);
    /* ES096 2.9.6: take nRTS high before reading DR after overrun.
     * Do the same before freeing RDR into the last software event slot. */
    if (flow &&
        ((sr & USART_SR_ORE) != 0U ||
         state->rx_count >= config->rx_capacity - 1U) &&
        !rts_hold(true)) {
        state->closing = true;
        NX_STM32F1_IRQ_DISABLE(config->irq);
        return;
    }
    uint8_t byte = (sr & RX_STATUS) != 0U
                       ? NX_STM32F1_UART_RECEIVE(registers) & payload
                       : 0U;
    if ((sr & USART_SR_LBD) != 0U) {
        NX_STM32F1_WRITE(&registers->SR, ~(uint32_t)USART_SR_LBD);
        byte = 0U;
    }
    if (state->closing) {
        return;
    }
    const bool event = config->profile == NX_UART_RX_EVENTS;
    if (state->rx_count == config->rx_capacity || (event && state->loss)) {
        if (event && !state->loss) {
            state->loss_time = nx_time_now_us();
        }
        state->loss = true;
        return;
    }
    if (!event && (sr & RX_ERRORS) != 0U) {
        state->loss = true;
    }
    if (!event && (sr & USART_SR_RXNE) == 0U) {
        return;
    }
    /* Avoid division and arithmetic beyond one full ring. */
    const size_t until_end = config->rx_capacity - state->rx_head;
    const size_t tail = state->rx_count < until_end
                            ? state->rx_head + state->rx_count
                            : state->rx_count - until_end;
    if (event) {
        nx_uart_rx_event_t* events = config->rx_storage;
        events[tail] =
            (nx_uart_rx_event_t){nx_time_now_us(), rx_flags(sr), byte};
    } else {
        uint8_t* bytes = config->rx_storage;
        bytes[tail] = byte;
    }
    ++state->rx_count;
    if (flow && (sr & USART_SR_ORE) == 0U)
        (void)release_rts(state);
}

/** \brief Change direction only after TX retirement. */
static bool release_tx(nx_stm32f1_uart_t* state, bool listen) {
    USART_TypeDef* registers = state->config->registers;
    const uint32_t controls =
        TX_INTERRUPTS | (directional(state->config) ? DIRECTION_MASK : 0U);
    const uint32_t direction =
        directional(state->config) && listen && !state->closing ? RX_DIRECTION
                                                                : 0U;
    return (registers->CR1 & controls) == direction ||
           nx_stm32f1_write_masked(&registers->CR1, controls, direction);
}

/** \brief Record wire completion before later task time can replace it. */
static void mark_complete(nx_stm32f1_uart_t* state, nx_time_us_t now) {
    if (state->terminal == NX_SUCCESS &&
        nx_deadline_expired(state->active->base.deadline, now)) {
        state->terminal = NX_ERROR_TIMEOUT;
    }
    state->wire_done = true;
    const bool optical_turnaround = irda(state->config) && !state->closing;
    if (optical_turnaround &&
        nx_deadline_finite_after(now, IRDA_RX_GUARD_US,
                                 &state->drain_deadline) != NX_SUCCESS) {
        state->closing = true;
        if (state->terminal == NX_SUCCESS)
            state->terminal = NX_ERROR_IO;
    }
    if (!release_tx(state, !optical_turnaround)) {
        if (UART_WIRE) {
            NX_STM32F1_IRQ_DISABLE(state->config->irq);
            if (state->terminal == NX_SUCCESS)
                state->terminal = NX_ERROR_IO;
        }
        state->closing = true;
    }
}

/** \brief Observe one SR, one DR receive and at most one TX byte per IRQ. */
static void uart_irq(nx_stm32f1_uart_t* state, uint8_t payload) {
    if (!state->live) {
        return;
    }
    USART_TypeDef* registers = state->config->registers;
    const uint32_t sr = NX_STM32F1_UART_STATUS(registers);
    const uint32_t cr1 = registers->CR1;
    const uint32_t received = sr & receive_status(state->config);
    const bool rx = received != 0U && (cr1 & RX_CONTROLS) != 0U;
    if (rx) {
        receive(state, received, payload);
    }
    if (state->active == NULL || state->wire_done) {
        return;
    }
    if ((sr & USART_SR_TXE) != 0U && (cr1 & USART_CR1_TXEIE) != 0U &&
        state->terminal == NX_SUCCESS && state->sent < state->active->length) {
        if (rx) {
            /* RM0008 clears TC by SR read followed by DR write. */
            (void)NX_STM32F1_UART_STATUS(registers);
        }
        NX_STM32F1_UART_TRANSMIT(registers, state->active->data[state->sent]);
        if (++state->sent == state->active->length &&
            !nx_stm32f1_write_masked(&registers->CR1, TX_INTERRUPTS,
                                     USART_CR1_TCIE)) {
            drain(state, NX_ERROR_IO, nx_time_now_us());
        }
        /* The old SR cannot describe the byte just written. */
        return;
    }
    if ((sr & USART_SR_TC) != 0U && (cr1 & USART_CR1_TCIE) != 0U &&
        (!lin_break(state, state->active) || (cr1 & USART_CR1_SBK) == 0U)) {
        mark_complete(state, nx_time_now_us());
    }
}

void nx_stm32f1_uart_irq(nx_stm32f1_uart_t* state) {
    uart_irq(state, UINT8_MAX);
}

void nx_stm32f1_uart_parity_irq(nx_stm32f1_uart_t* state) {
    if (state->live) {
        const uint8_t payload =
            (state->config->frame & NX_STM32F1_UART_WORD9) == 0U ? 0x7FU
                                                                 : UINT8_MAX;
        uart_irq(state, payload);
    }
}

/** \brief Settle only after the wire and interrupt source both drained. */
static void uart_service(void* context) {
    nx_stm32f1_uart_t* state = context;
    if (!nx_stm32f1_cold_context()) {
        return;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    nx_uart_tx_request_t* active = state->active;
    if (active == NULL) {
        nx_arch_irq_restore(mask);
        return;
    }
    const nx_time_us_t now = nx_time_now_us();
    USART_TypeDef* registers = state->config->registers;
    const bool breaking = lin_break(state, active);
    if (!state->wire_done && (state->sent != 0U || breaking) &&
        (!breaking || (registers->CR1 & USART_CR1_SBK) == 0U) &&
        (state->terminal != NX_SUCCESS || state->sent == active->length) &&
        (NX_STM32F1_UART_STATUS(registers) & USART_SR_TC) != 0U) {
        mark_complete(state, now);
    }
    if (!state->wire_done && state->terminal == NX_SUCCESS &&
        nx_deadline_expired(active->base.deadline, now)) {
        drain(state, NX_ERROR_TIMEOUT, now);
    }
    /* RM0008 27.3.12: leave the optical receiver disabled for at least 10 ms.
     */
    if (irda(state->config) && state->wire_done && state->sent != 0U &&
        !state->closing && !nx_deadline_expired(state->drain_deadline, now)) {
        nx_arch_irq_restore(mask);
        return;
    }
    if (state->wire_done && release_tx(state, true)) {
        state->active = NULL;
        nx_request_settle(&active->base, state->terminal, state->sent);
    } else if (state->wire_done ||
               nx_deadline_expired(state->drain_deadline, now)) {
        (void)nx_request_transition(&active->base, NX_REQUEST_QUARANTINED);
    }
    nx_arch_irq_restore(mask);
}

/** \brief Validate a bounded destination separate from all borrowed storage. */
static bool valid_destination(const nx_stm32f1_uart_t* state, void* output,
                              size_t bytes, size_t alignment, size_t* count) {
    if (!nx_io_dispatch_span(output, bytes, alignment) ||
        !nx_io_dispatch_span(count, sizeof(*count), _Alignof(size_t)) ||
        nx_io_dispatch_overlap(output, bytes, count, sizeof(*count)) ||
        !independent(state, output, bytes) ||
        !independent(state, count, sizeof(*count))) {
        return false;
    }
    const nx_uart_tx_request_t* active = state->active;
    return active == NULL ||
           (!nx_io_dispatch_overlap(output, bytes, active, sizeof(*active)) &&
            !nx_io_dispatch_overlap(count, sizeof(*count), active,
                                    sizeof(*active)) &&
            !nx_io_dispatch_overlap(output, bytes, active->data,
                                    active->length) &&
            !nx_io_dispatch_overlap(count, sizeof(*count), active->data,
                                    active->length));
}

/** \brief Pop one RX fact under a short IRQ mask, preserving drain ordering. */
static nx_result_t read_rx(nx_stm32f1_uart_t* state, void* output,
                           size_t capacity, size_t* count, bool event) {
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->live || state->closing) {
        return NX_ERROR_STATE;
    }
    if (event != (state->config->profile == NX_UART_RX_EVENTS)) {
        return NX_ERROR_UNSUPPORTED;
    }
    const size_t stride = rx_stride(state->config);
    const size_t alignment = event ? _Alignof(nx_uart_rx_event_t) : 1U;
    if (capacity == 0U || capacity > SIZE_MAX / stride ||
        !valid_destination(state, output, capacity * stride, alignment,
                           count)) {
        return NX_ERROR_INVALID;
    }
    size_t copied = 0U;
    bool loss = false;
    for (; copied < capacity; ++copied) {
        nx_arch_irq_state_t mask = nx_arch_irq_save();
        const size_t head = state->rx_head;
        if (state->rx_count == 0U) {
            if (event && state->loss) {
                const nx_uart_rx_event_t marker = {
                    state->loss_time,
                    NX_UART_EVENT_LOSS | NX_UART_EVENT_NO_BYTE, 0U};
                state->loss = false;
                nx_arch_irq_restore(mask);
                ((nx_uart_rx_event_t*)output)[copied++] = marker;
                break;
            }
            nx_arch_irq_restore(mask);
            break;
        }
        nx_uart_rx_event_t fact;
        uint8_t byte = 0U;
        if (event) {
            fact = ((const nx_uart_rx_event_t*)state->config->rx_storage)[head];
        } else {
            byte = ((const uint8_t*)state->config->rx_storage)[head];
        }
        state->rx_head =
            head + 1U == state->config->rx_capacity ? 0U : head + 1U;
        --state->rx_count;
        nx_arch_irq_restore(mask);
        if (event) {
            ((nx_uart_rx_event_t*)output)[copied] = fact;
        } else {
            ((uint8_t*)output)[copied] = byte;
        }
    }
    if (!event) {
        nx_arch_irq_state_t mask = nx_arch_irq_save();
        loss = state->loss;
        state->loss = false;
        nx_arch_irq_restore(mask);
    }
    *count = copied;
    if (event && hardware_flow(state->config)) {
        nx_arch_irq_state_t mask = nx_arch_irq_save();
        const bool released = release_rts(state);
        nx_arch_irq_restore(mask);
        if (!released)
            return NX_ERROR_IO;
    }
    return loss           ? NX_ERROR_OVERFLOW
           : copied == 0U ? NX_ERROR_EMPTY
                          : NX_SUCCESS;
}

/** \brief Read packed bytes and aggregate lost or faulty input. */
static nx_result_t uart_read_bytes(void* context, uint8_t* bytes,
                                   size_t capacity, size_t* count) {
    return read_rx(context, bytes, capacity, count, false);
}

/** \brief Read ordered status and loss events. */
static nx_result_t uart_read_events(void* context, nx_uart_rx_event_t* events,
                                    size_t capacity, size_t* count) {
    return read_rx(context, events, capacity, count, true);
}

/** \brief Stop request loads and isolate RX, keeping cold resource receipts. */
nx_result_t nx_stm32f1_uart_stop(nx_stm32f1_uart_t* state) {
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->config == NULL) {
        return NX_SUCCESS;
    }
    const nx_stm32f1_uart_config_t* config = state->config;
    USART_TypeDef* registers = config->registers;
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    state->closing = true;
    const bool flow = hardware_flow(config);
    if (flow && state->pads.captured && !rts_hold(true)) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_IO;
    }
    if (state->active != NULL) {
        if (!state->wire_done && state->terminal == NX_SUCCESS) {
            drain(state, NX_ERROR_CANCELLED, nx_time_now_us());
        }
        nx_arch_irq_restore(mask);
        return NX_ERROR_BUSY;
    }
    if (state->vector_owned) {
        NX_STM32F1_IRQ_DISABLE(config->irq);
        nx_arch_dsb();
        if (NX_STM32F1_IRQ_ENABLED(config->irq) ||
            NX_STM32F1_IRQ_ACTIVE(config->irq)) {
            nx_arch_irq_restore(mask);
            return NX_ERROR_IO;
        }
    }
    state->live = false;
    nx_arch_irq_restore(mask);
    if (state->controls_owned) {
        const bool clocked = synchronous(config);
        const uint32_t cr2 =
            (lin(config) ? LIN_CONTROLS : 0U) | (clocked ? SYNC_CONTROLS : 0U) |
            ((config->frame & NX_STM32F1_UART_STOP2) != 0U ? USART_CR2_STOP
                                                           : 0U);
        const uint32_t cr3 = cr3_controls(config);
        const bool sir = irda(config);
        if ((sir && !irda_tx_pad(false)) ||
            (clocked && !sync_clock_pad(false)) ||
            !nx_stm32f1_write_masked(&registers->CR1,
                                     UART_CONTROLS | RX_CONTROLS |
                                         TX_INTERRUPTS | FRAME_CONTROLS,
                                     0U) ||
            (cr2 != 0U && !nx_stm32f1_write_masked(&registers->CR2, cr2, 0U)) ||
            !nx_stm32f1_write_masked(&registers->CR3, cr3, 0U) ||
            (sir &&
             !nx_stm32f1_write_masked(&registers->GTPR, USART_GTPR_PSC, 0U)) ||
            !nx_stm32f1_write_masked(&registers->BRR, UINT16_MAX, 0U)) {
            return NX_ERROR_IO;
        }
        const uint32_t status = NX_STM32F1_UART_STATUS(registers);
        if ((status & RX_STATUS) != 0U)
            (void)NX_STM32F1_UART_RECEIVE(registers);
        if (lin(config) && (status & USART_SR_LBD) != 0U)
            NX_STM32F1_WRITE(&registers->SR, ~(uint32_t)USART_SR_LBD);
        if (flow && (status & USART_SR_CTS) != 0U)
            NX_STM32F1_WRITE(&registers->SR, ~(uint32_t)USART_SR_CTS);
        const uint32_t pending =
            receive_status(config) | (flow ? USART_SR_CTS : 0U);
        if ((NX_STM32F1_UART_STATUS(registers) & pending) != 0U)
            return NX_ERROR_IO;
        state->controls_owned = false;
    }
    if (state->vector_owned) {
        NX_STM32F1_IRQ_CLEAR(config->irq);
        NX_STM32F1_IRQ_SET_PRIORITY(config->irq, state->previous_priority);
        nx_arch_dsb();
        if (NX_STM32F1_IRQ_PENDING(config->irq) ||
            NX_STM32F1_IRQ_PRIORITY(config->irq) != state->previous_priority) {
            return NX_ERROR_IO;
        }
        state->vector_owned = false;
    }
    return NX_SUCCESS;
}

/** \brief Release cold receipts in the Platform's reverse acquisition order. */
nx_result_t nx_stm32f1_uart_close(nx_stm32f1_uart_t* state) {
    nx_result_t stopped = nx_stm32f1_uart_stop(state);
    if (stopped != NX_SUCCESS || state->config == NULL) {
        return stopped;
    }
#if defined(STM32F107xC)
    if (state->remap_owned) {
        if (!set_usart2_remap(false)) {
            return NX_ERROR_IO;
        }
        state->remap_owned = false;
    }
#endif
    nx_result_t restored = nx_stm32f1_pads_restore(&state->pads);
    if (restored != NX_SUCCESS) {
        return restored;
    }
    if (state->usart_gate_added &&
        !nx_stm32f1_write_masked(&RCC->APB1ENR, RCC_APB1ENR_USART2EN, 0U)) {
        return NX_ERROR_IO;
    }
    if (state->afio_gate_added &&
        !nx_stm32f1_write_masked(&RCC->APB2ENR, RCC_APB2ENR_AFIOEN, 0U)) {
        return NX_ERROR_IO;
    }
    *state = (nx_stm32f1_uart_t){0};
    return NX_SUCCESS;
}

static nx_result_t uart_stop(void* context) {
    return nx_stm32f1_uart_stop(context);
}

const nx_uart_ops_t nx_stm32f1_uart_ops = {
    .submit = uart_submit,
    .start_admitted = uart_start_admitted,
    .cancel = uart_cancel,
    .service = uart_service,
    .read_events = uart_read_events,
    .read_bytes = uart_read_bytes,
    .stop = uart_stop,
};
