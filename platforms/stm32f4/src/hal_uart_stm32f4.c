/**
 * \file            hal_uart_stm32f4.c
 * \brief           STM32F4 UART HAL Implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "hal/hal_uart.h"
#include "stm32f4xx.h"

/*===========================================================================*/
/* Local definitions                                                          */
/*===========================================================================*/

/**
 * \brief           Maximum number of UART instances
 */
#define UART_MAX_INSTANCES      3

/**
 * \brief           UART instance data
 */
typedef struct {
    USART_TypeDef*          instance;       /**< USART peripheral */
    hal_uart_rx_callback_t  rx_callback;    /**< RX callback */
    hal_uart_tx_callback_t  tx_callback;    /**< TX callback */
    void*                   rx_context;     /**< RX callback context */
    void*                   tx_context;     /**< TX callback context */
    bool                    initialized;    /**< Initialization flag */
} uart_data_t;

/**
 * \brief           UART instance data array
 */
static uart_data_t uart_data[UART_MAX_INSTANCES] = {
    { USART1, NULL, NULL, NULL, NULL, false },
    { USART2, NULL, NULL, NULL, NULL, false },
    { USART3, NULL, NULL, NULL, NULL, false }
};

/*===========================================================================*/
/* Local functions                                                            */
/*===========================================================================*/

/**
 * \brief           Get UART data by port
 * \param[in]       port: UART port number
 * \return          Pointer to UART data or NULL
 */
static uart_data_t* uart_get_data(hal_uart_port_t port)
{
    if (port >= UART_MAX_INSTANCES) {
        return NULL;
    }
    return &uart_data[port];
}

/**
 * \brief           Enable UART clock
 * \param[in]       port: UART port number
 */
static void uart_enable_clock(hal_uart_port_t port)
{
    switch (port) {
        case 0:  /* USART1 */
            RCC->APB2ENR |= (1UL << 4);
            break;
        case 1:  /* USART2 */
            RCC->APB1ENR |= (1UL << 17);
            break;
        case 2:  /* USART3 */
            RCC->APB1ENR |= (1UL << 18);
            break;
        default:
            break;
    }
    __asm volatile("dsb");
}

/**
 * \brief           Get APB clock for UART
 * \param[in]       port: UART port number
 * \return          APB clock frequency
 */
static uint32_t uart_get_clock(hal_uart_port_t port)
{
    /* Simplified: assume 16MHz HSI, no prescalers */
    (void)port;
    return SystemCoreClock;
}

/*===========================================================================*/
/* Public functions                                                           */
/*===========================================================================*/

hal_status_t hal_uart_init(hal_uart_port_t port, const hal_uart_config_t* config)
{
    uart_data_t* data;
    USART_TypeDef* uart;
    uint32_t brr;
    uint32_t cr1 = 0;
    uint32_t cr2 = 0;

    /* Parameter validation */
    if (config == NULL) {
        return HAL_ERR_PARAM;
    }

    data = uart_get_data(port);
    if (data == NULL) {
        return HAL_ERR_PARAM;
    }

    /* Enable clock */
    uart_enable_clock(port);
    uart = data->instance;

    /* Disable UART during configuration */
    uart->CR1 = 0;

    /* Calculate baud rate register value */
    brr = (uart_get_clock(port) + config->baudrate / 2) / config->baudrate;
    uart->BRR = brr;

    /* Configure data bits */
    if (config->data_bits == HAL_UART_DATA_9BIT) {
        cr1 |= (1UL << 12);  /* M bit */
    }

    /* Configure parity */
    switch (config->parity) {
        case HAL_UART_PARITY_EVEN:
            cr1 |= (1UL << 10);  /* PCE */
            break;
        case HAL_UART_PARITY_ODD:
            cr1 |= (1UL << 10) | (1UL << 9);  /* PCE + PS */
            break;
        default:
            break;
    }

    /* Configure stop bits */
    switch (config->stop_bits) {
        case HAL_UART_STOP_2BIT:
            cr2 |= (2UL << 12);
            break;
        default:
            break;
    }

    /* Enable TX and RX */
    cr1 |= (1UL << 3) | (1UL << 2);  /* TE + RE */

    /* Apply configuration */
    uart->CR2 = cr2;
    uart->CR1 = cr1;

    /* Enable UART */
    uart->CR1 |= (1UL << 13);  /* UE */

    data->initialized = true;

    return HAL_OK;
}

hal_status_t hal_uart_deinit(hal_uart_port_t port)
{
    uart_data_t* data = uart_get_data(port);

    if (data == NULL || !data->initialized) {
        return HAL_ERR_PARAM;
    }

    /* Disable UART */
    data->instance->CR1 = 0;
    data->initialized = false;

    return HAL_OK;
}

hal_status_t hal_uart_write(hal_uart_port_t port,
                            const uint8_t* data_buf,
                            size_t len,
                            uint32_t timeout_ms)
{
    uart_data_t* data = uart_get_data(port);
    USART_TypeDef* uart;
    size_t i;

    (void)timeout_ms;  /* TODO: Implement timeout */

    if (data == NULL || !data->initialized || data_buf == NULL) {
        return HAL_ERR_PARAM;
    }

    uart = data->instance;

    for (i = 0; i < len; i++) {
        /* Wait for TXE (transmit data register empty) */
        while (!(uart->SR & (1UL << 7))) {
            /* TODO: Add timeout check */
        }
        uart->DR = data_buf[i];
    }

    /* Wait for TC (transmission complete) */
    while (!(uart->SR & (1UL << 6))) {
        /* TODO: Add timeout check */
    }

    return HAL_OK;
}

hal_status_t hal_uart_read(hal_uart_port_t port,
                           uint8_t* data_buf,
                           size_t len,
                           size_t* actual_len,
                           uint32_t timeout_ms)
{
    uart_data_t* data = uart_get_data(port);
    USART_TypeDef* uart;
    size_t i;

    (void)timeout_ms;  /* TODO: Implement timeout */

    if (data == NULL || !data->initialized || data_buf == NULL) {
        return HAL_ERR_PARAM;
    }

    uart = data->instance;

    for (i = 0; i < len; i++) {
        /* Wait for RXNE (receive data register not empty) */
        while (!(uart->SR & (1UL << 5))) {
            /* TODO: Add timeout check */
        }
        data_buf[i] = (uint8_t)uart->DR;
    }

    if (actual_len != NULL) {
        *actual_len = len;
    }

    return HAL_OK;
}

hal_status_t hal_uart_write_byte(hal_uart_port_t port, uint8_t byte)
{
    return hal_uart_write(port, &byte, 1, HAL_WAIT_FOREVER);
}

hal_status_t hal_uart_read_byte(hal_uart_port_t port,
                                uint8_t* byte,
                                uint32_t timeout_ms)
{
    return hal_uart_read(port, byte, 1, NULL, timeout_ms);
}

hal_status_t hal_uart_set_callback(hal_uart_port_t port,
                                   hal_uart_rx_callback_t rx_cb,
                                   hal_uart_tx_callback_t tx_cb,
                                   void* rx_ctx,
                                   void* tx_ctx)
{
    uart_data_t* data = uart_get_data(port);

    if (data == NULL) {
        return HAL_ERR_PARAM;
    }

    data->rx_callback = rx_cb;
    data->tx_callback = tx_cb;
    data->rx_context = rx_ctx;
    data->tx_context = tx_ctx;

    return HAL_OK;
}

hal_status_t hal_uart_flush(hal_uart_port_t port)
{
    uart_data_t* data = uart_get_data(port);
    USART_TypeDef* uart;

    if (data == NULL || !data->initialized) {
        return HAL_ERR_PARAM;
    }

    uart = data->instance;

    /* Wait for TC (transmission complete) */
    while (!(uart->SR & (1UL << 6))) {
        /* Wait */
    }

    return HAL_OK;
}
