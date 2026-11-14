/**
 * \file            uart_wire.c
 * \brief           Selected F407 USART wire personalities
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_uart_wire.h"

/** \brief DS8626 Table 5 restricts clock, Smartcard and modem to USARTs. */
static bool synchronous_instance(IRQn_Type irq) {
    return irq == USART1_IRQn || irq == USART2_IRQn ||
#ifdef USART3
           irq == USART3_IRQn ||
#endif
           irq == USART6_IRQn;
}

/** \brief Optional asynchronous-only IP exists only on selected devices. */
static bool asynchronous_instance(IRQn_Type irq) {
#ifdef UART4
    if (irq == UART4_IRQn)
        return true;
#endif
#ifdef UART5
    if (irq == UART5_IRQn)
        return true;
#endif
    (void)irq;
    return false;
}

/** \brief Reject irrelevant fields instead of silently changing the tuple. */
nx_result_t
nx_stm32_uart_wire_validate(const nx_stm32_uart_state_t* port,
                            const nx_stm32_uart_wire_config_t* config) {
    if (port == NULL || config == NULL || port->registers == NULL ||
        config->mode > NX_UART_WIRE_SMARTCARD ||
        config->flow > NX_UART_FLOW_RTS_CTS)
        return NX_ERROR_INVALID;
    if (!synchronous_instance(port->irq) && !asynchronous_instance(port->irq))
        return NX_ERROR_INVALID;
    if (port->profile != NX_UART_RX_BYTES && port->profile != NX_UART_RX_EVENTS)
        return NX_ERROR_UNSUPPORTED;
    bool synchronous = config->mode == NX_UART_WIRE_SYNCHRONOUS;
    bool smartcard = config->mode == NX_UART_WIRE_SMARTCARD;
    bool irda = config->mode == NX_UART_WIRE_IRDA;
    bool lin = config->mode == NX_UART_WIRE_LIN;
    if ((synchronous || smartcard || config->flow != NX_UART_FLOW_NONE) &&
        !synchronous_instance(port->irq))
        return NX_ERROR_UNSUPPORTED;
    if ((!synchronous && (config->clock_phase || config->clock_polarity ||
                          config->last_bit_clock)) ||
        (!smartcard && (config->guard_time != 0U || config->smartcard_nack)) ||
        (!irda && config->irda_low_power) ||
        (!irda && !smartcard && config->prescaler != 0U) ||
        (!lin && config->lin_break_bits != 0U) ||
        (config->mode != NX_UART_WIRE_ASYNCHRONOUS &&
         config->flow != NX_UART_FLOW_NONE))
        return NX_ERROR_INVALID;
    if (lin && config->lin_break_bits != 10U && config->lin_break_bits != 11U)
        return NX_ERROR_INVALID;
    if ((irda || smartcard) && config->prescaler == 0U)
        return NX_ERROR_INVALID;
    if (smartcard && config->prescaler > 31U)
        return NX_ERROR_INVALID;
    if (irda && port->baud > 115200U)
        return NX_ERROR_UNSUPPORTED;
    return NX_SUCCESS;
}

/** \brief The complete wire tuple precedes the first UE and publisher. */
nx_result_t
nx_stm32_uart_wire_initialize(nx_stm32_uart_state_t* port, uint32_t clock_hz,
                              const nx_stm32_uart_wire_config_t* config) {
    nx_result_t checked = nx_stm32_uart_wire_validate(port, config);
    if (checked != NX_SUCCESS)
        return checked;
    if (config->irda_low_power &&
        ((uint64_t)clock_hz < (uint64_t)1420000U * config->prescaler ||
         (uint64_t)clock_hz > (uint64_t)2120000U * config->prescaler))
        return NX_ERROR_UNSUPPORTED;
    uint32_t frame = 0U;
    uint32_t control2 = 0U;
    uint32_t control3 = 0U;
    uint32_t prescaler = 0U;
    switch (config->mode) {
        case NX_UART_WIRE_ASYNCHRONOUS:
            if (config->flow == NX_UART_FLOW_RTS ||
                config->flow == NX_UART_FLOW_RTS_CTS)
                control3 |= USART_CR3_RTSE;
            if (config->flow == NX_UART_FLOW_CTS ||
                config->flow == NX_UART_FLOW_RTS_CTS)
                control3 |= USART_CR3_CTSE;
            break;
        case NX_UART_WIRE_SYNCHRONOUS:
            control2 = USART_CR2_CLKEN |
                       (config->clock_polarity ? USART_CR2_CPOL : 0U) |
                       (config->clock_phase ? USART_CR2_CPHA : 0U) |
                       (config->last_bit_clock ? USART_CR2_LBCL : 0U);
            break;
        case NX_UART_WIRE_HALF_DUPLEX:
            control3 = USART_CR3_HDSEL;
            break;
        case NX_UART_WIRE_LIN:
            control2 = USART_CR2_LINEN | USART_CR2_LBDIE |
                       (config->lin_break_bits == 11U ? USART_CR2_LBDL : 0U);
            break;
        case NX_UART_WIRE_IRDA:
            control3 =
                USART_CR3_IREN | (config->irda_low_power ? USART_CR3_IRLP : 0U);
            prescaler = config->prescaler;
            break;
        case NX_UART_WIRE_SMARTCARD:
            frame = USART_CR1_M | USART_CR1_PCE;
            control2 = USART_CR2_STOP | USART_CR2_CLKEN;
            control3 =
                USART_CR3_SCEN | (config->smartcard_nack ? USART_CR3_NACK : 0U);
            prescaler =
                config->prescaler | ((uint32_t)config->guard_time << 8U);
            break;
        default:
            return NX_ERROR_INVALID;
    }
    return nx_stm32_uart_initialize_controls(port, clock_hz, frame, control2,
                                             control3, prescaler);
}
