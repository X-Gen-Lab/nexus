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
#define UART_MAX_INSTANCES 3

/**
 * \brief           USART Status Register bit definitions
 */
#define USART_SR_PE   (1UL << 0) /**< Parity error */
#define USART_SR_FE   (1UL << 1) /**< Framing error */
#define USART_SR_NE   (1UL << 2) /**< Noise error */
#define USART_SR_ORE  (1UL << 3) /**< Overrun error */
#define USART_SR_IDLE (1UL << 4) /**< IDLE line detected */
#define USART_SR_RXNE (1UL << 5) /**< Read data register not empty */
#define USART_SR_TC   (1UL << 6) /**< Transmission complete */
#define USART_SR_TXE  (1UL << 7) /**< Transmit data register empty */

/**
 * \brief           USART Control Register 1 bit definitions
 */
#define USART_CR1_RE     (1UL << 2)  /**< Receiver enable */
#define USART_CR1_TE     (1UL << 3)  /**< Transmitter enable */
#define USART_CR1_IDLEIE (1UL << 4)  /**< IDLE interrupt enable */
#define USART_CR1_RXNEIE (1UL << 5)  /**< RXNE interrupt enable */
#define USART_CR1_TCIE   (1UL << 6)  /**< TC interrupt enable */
#define USART_CR1_TXEIE  (1UL << 7)  /**< TXE interrupt enable */
#define USART_CR1_PEIE   (1UL << 8)  /**< PE interrupt enable */
#define USART_CR1_PS     (1UL << 9)  /**< Parity selection */
#define USART_CR1_PCE    (1UL << 10) /**< Parity control enable */
#define USART_CR1_M      (1UL << 12) /**< Word length */
#define USART_CR1_UE     (1UL << 13) /**< USART enable */

/**
 * \brief           USART Control Register 2 bit definitions
 */
#define USART_CR2_STOP_1 (0UL << 12) /**< 1 stop bit */
#define USART_CR2_STOP_2 (2UL << 12) /**< 2 stop bits */

/**
 * \brief           UART instance data
 */
typedef struct {
    USART_TypeDef* instance;            /**< USART peripheral */
    hal_uart_config_t config;           /**< Configuration */
    hal_uart_rx_callback_t rx_callback; /**< RX callback */
    hal_uart_tx_callback_t tx_callback; /**< TX callback */
    void* rx_context;                   /**< RX callback context */
    void* tx_context;                   /**< TX callback context */
    bool initialized;                   /**< Initialization flag */
} uart_data_t;

/**
 * \brief           UART instance data array
 */
static uart_data_t uart_data[UART_MAX_INSTANCES] = {
    {USART1, {0}, NULL, NULL, NULL, NULL, false},
    {USART2, {0}, NULL, NULL, NULL, NULL, false},
    {USART3, {0}, NULL, NULL, NULL, NULL, false}};

/*===========================================================================*/
/* Local functions                                                            */
/*===========================================================================*/

/**
 * \brief           Get UART data by instance
 * \param[in]       instance: UART instance
 * \return          Pointer to UART data or NULL
 */
static uart_data_t* uart_get_data(hal_uart_instance_t instance) {
    if (instance >= UART_MAX_INSTANCES) {
        return NULL;
    }
    return &uart_data[instance];
}

/**
 * \brief           Enable UART clock
 * \param[in]       instance: UART instance
 */
static void uart_enable_clock(hal_uart_instance_t instance) {
    switch (instance) {
        case HAL_UART_0: /* USART1 on APB2 */
            RCC->APB2ENR |= (1UL << 4);
            break;
        case HAL_UART_1: /* USART2 on APB1 */
            RCC->APB1ENR |= (1UL << 17);
            break;
        case HAL_UART_2: /* USART3 on APB1 */
            RCC->APB1ENR |= (1UL << 18);
            break;
        default:
            break;
    }
    /* Data synchronization barrier */
    __asm volatile("dsb");
}

/**
 * \brief           Get APB clock for UART
 * \param[in]       instance: UART instance
 * \return          APB clock frequency
 */
static uint32_t uart_get_clock(hal_uart_instance_t instance) {
    /* USART1 is on APB2, USART2/3 are on APB1 */
    /* Simplified: assume SystemCoreClock with no prescalers */
    (void)instance;
    return SystemCoreClock;
}

/*===========================================================================*/
/* Public functions                                                           */
/*===========================================================================*/

hal_status_t hal_uart_init(hal_uart_instance_t instance,
                           const hal_uart_config_t* config) {
    uart_data_t* data;
    USART_TypeDef* uart;
    uint32_t brr;
    uint32_t cr1 = 0;
    uint32_t cr2 = 0;

    /* Parameter validation */
    if (config == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }
    if (instance >= UART_MAX_INSTANCES) {
        return HAL_ERROR_INVALID_PARAM;
    }

    /* Validate baudrate range (9600 - 921600) */
    if (config->baudrate < 9600 || config->baudrate > 921600) {
        return HAL_ERROR_INVALID_PARAM;
    }

    data = uart_get_data(instance);
    if (data == NULL) {
        return HAL_ERROR_INVALID_PARAM;
    }

    /* Enable clock */
    uart_enable_clock(instance);
    uart = data->instance;

    /* Disable UART during configuration */
    uart->CR1 = 0;

    /* Calculate baud rate register value */
    /* BRR = fck / baudrate (with rounding) */
    brr = (uart_get_clock(instance) + config->baudrate / 2) / config->baudrate;
    uart->BRR = brr;

    /* Configure word length */
    if (config->wordlen == HAL_UART_WORDLEN_9) {
        cr1 |= USART_CR1_M;
    }

    /* Configure parity */
    switch (config->parity) {
        case HAL_UART_PARITY_EVEN:
            cr1 |= USART_CR1_PCE;
            break;
        case HAL_UART_PARITY_ODD:
            cr1 |= USART_CR1_PCE | USART_CR1_PS;
            break;
        default:
            break;
    }

    /* Configure stop bits */
    switch (config->stopbits) {
        case HAL_UART_STOPBITS_2:
            cr2 |= USART_CR2_STOP_2;
            break;
        default:
            cr2 |= USART_CR2_STOP_1;
            break;
    }

    /* Enable TX and RX */
    cr1 |= USART_CR1_TE | USART_CR1_RE;

    /* Apply configuration */
    uart->CR2 = cr2;
    uart->CR1 = cr1;

    /* Enable UART */
    uart->CR1 |= USART_CR1_UE;

    /* Store configuration */
    data->config = *config;
    data->rx_callback = NULL;
    data->tx_callback = NULL;
    data->rx_context = NULL;
    data->tx_context = NULL;
    data->initialized = true;

    return HAL_OK;
}

hal_status_t hal_uart_deinit(hal_uart_instance_t instance) {
    uart_data_t* data = uart_get_data(instance);

    if (data == NULL) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (!data->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Disable UART */
    data->instance->CR1 = 0;
    data->initialized = false;
    data->rx_callback = NULL;
    data->tx_callback = NULL;

    return HAL_OK;
}

hal_status_t hal_uart_transmit(hal_uart_instance_t instance,
                               const uint8_t* data_buf, size_t len,
                               uint32_t timeout_ms) {
    uart_data_t* data = uart_get_data(instance);
    USART_TypeDef* uart;
    size_t i;
    uint32_t timeout_count;

    if (data == NULL) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (data_buf == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }
    if (!data->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    uart = data->instance;

    for (i = 0; i < len; i++) {
        /* Wait for TXE (transmit data register empty) with timeout */
        timeout_count = timeout_ms * 1000; /* Simple delay counter */
        while (!(uart->SR & USART_SR_TXE)) {
            if (timeout_ms != HAL_WAIT_FOREVER && --timeout_count == 0) {
                return HAL_ERROR_TIMEOUT;
            }
        }
        uart->DR = data_buf[i];
    }

    /* Wait for TC (transmission complete) */
    timeout_count = timeout_ms * 1000;
    while (!(uart->SR & USART_SR_TC)) {
        if (timeout_ms != HAL_WAIT_FOREVER && --timeout_count == 0) {
            return HAL_ERROR_TIMEOUT;
        }
    }

    /* Invoke TX complete callback if registered */
    if (data->tx_callback != NULL) {
        data->tx_callback(instance, data->tx_context);
    }

    return HAL_OK;
}

hal_status_t hal_uart_receive(hal_uart_instance_t instance, uint8_t* data_buf,
                              size_t len, uint32_t timeout_ms) {
    uart_data_t* data = uart_get_data(instance);
    USART_TypeDef* uart;
    size_t i;
    uint32_t timeout_count;

    if (data == NULL) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (data_buf == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }
    if (!data->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    uart = data->instance;

    for (i = 0; i < len; i++) {
        /* Wait for RXNE (receive data register not empty) with timeout */
        timeout_count = timeout_ms * 1000;
        while (!(uart->SR & USART_SR_RXNE)) {
            if (timeout_ms != HAL_WAIT_FOREVER && --timeout_count == 0) {
                return HAL_ERROR_TIMEOUT;
            }
        }

        /* Check for errors */
        if (uart->SR & USART_SR_PE) {
            return HAL_ERROR_PARITY;
        }
        if (uart->SR & USART_SR_FE) {
            return HAL_ERROR_FRAMING;
        }
        if (uart->SR & USART_SR_NE) {
            return HAL_ERROR_NOISE;
        }
        if (uart->SR & USART_SR_ORE) {
            return HAL_ERROR_OVERRUN;
        }

        data_buf[i] = (uint8_t)uart->DR;

        /* Invoke RX callback if registered */
        if (data->rx_callback != NULL) {
            data->rx_callback(instance, data_buf[i], data->rx_context);
        }
    }

    return HAL_OK;
}

hal_status_t hal_uart_putc(hal_uart_instance_t instance, uint8_t byte) {
    return hal_uart_transmit(instance, &byte, 1, HAL_WAIT_FOREVER);
}

hal_status_t hal_uart_getc(hal_uart_instance_t instance, uint8_t* byte,
                           uint32_t timeout_ms) {
    return hal_uart_receive(instance, byte, 1, timeout_ms);
}

hal_status_t hal_uart_set_rx_callback(hal_uart_instance_t instance,
                                      hal_uart_rx_callback_t callback,
                                      void* context) {
    uart_data_t* data = uart_get_data(instance);

    if (data == NULL) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (!data->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    data->rx_callback = callback;
    data->rx_context = context;

    /* Enable RXNE interrupt if callback is set */
    if (callback != NULL) {
        data->instance->CR1 |= USART_CR1_RXNEIE;
    } else {
        data->instance->CR1 &= ~USART_CR1_RXNEIE;
    }

    return HAL_OK;
}

hal_status_t hal_uart_set_tx_callback(hal_uart_instance_t instance,
                                      hal_uart_tx_callback_t callback,
                                      void* context) {
    uart_data_t* data = uart_get_data(instance);

    if (data == NULL) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (!data->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    data->tx_callback = callback;
    data->tx_context = context;

    /* Enable TC interrupt if callback is set */
    if (callback != NULL) {
        data->instance->CR1 |= USART_CR1_TCIE;
    } else {
        data->instance->CR1 &= ~USART_CR1_TCIE;
    }

    return HAL_OK;
}

/*===========================================================================*/
/* Interrupt handlers                                                         */
/*===========================================================================*/

/**
 * \brief           Common UART IRQ handler
 * \param[in]       instance: UART instance
 */
static void uart_irq_handler(hal_uart_instance_t instance) {
    uart_data_t* data = uart_get_data(instance);
    USART_TypeDef* uart;

    if (data == NULL || !data->initialized) {
        return;
    }

    uart = data->instance;

    /* Check for RX data available */
    if ((uart->SR & USART_SR_RXNE) && (uart->CR1 & USART_CR1_RXNEIE)) {
        uint8_t byte = (uint8_t)uart->DR;
        if (data->rx_callback != NULL) {
            data->rx_callback(instance, byte, data->rx_context);
        }
    }

    /* Check for TX complete */
    if ((uart->SR & USART_SR_TC) && (uart->CR1 & USART_CR1_TCIE)) {
        /* Clear TC flag by reading SR then writing DR (or just clear it) */
        uart->SR &= ~USART_SR_TC;
        if (data->tx_callback != NULL) {
            data->tx_callback(instance, data->tx_context);
        }
    }
}

/**
 * \brief           USART1 IRQ handler
 */
void USART1_IRQHandler(void) {
    uart_irq_handler(HAL_UART_0);
}

/**
 * \brief           USART2 IRQ handler
 */
void USART2_IRQHandler(void) {
    uart_irq_handler(HAL_UART_1);
}

/**
 * \brief           USART3 IRQ handler
 */
void USART3_IRQHandler(void) {
    uart_irq_handler(HAL_UART_2);
}
