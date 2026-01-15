/**
 * \file            hal_uart_stm32f4.c
 * \brief           STM32F4 UART HAL Implementation (ST HAL Wrapper)
 * \author          Nexus Team
 * \version         2.0.0
 * \date            2026-01-15
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * This implementation wraps ST HAL UART functions to provide the Nexus HAL
 * interface. It uses HAL_UART_Init(), HAL_UART_Transmit(), HAL_UART_Receive(),
 * HAL_UART_Transmit_IT(), HAL_UART_Receive_IT(), and HAL_UART_IRQHandler()
 * from the ST HAL library.
 */

#include "hal/hal_uart.h"
#include "stm32f4xx.h"
#include "stm32f4xx_hal_conf.h"

/*===========================================================================*/
/* Local definitions                                                          */
/*===========================================================================*/

/**
 * \brief           Maximum number of UART instances
 */
#define UART_MAX_INSTANCES 3

/**
 * \brief           Default timeout for single byte operations (ms)
 */
#define UART_DEFAULT_TIMEOUT 1000

/**
 * \brief           UART instance data structure - wraps ST HAL Handle
 */
typedef struct {
    UART_HandleTypeDef huart;           /**< ST HAL UART Handle */
    hal_uart_config_t config;           /**< Nexus configuration */
    hal_uart_rx_callback_t rx_callback; /**< RX callback */
    hal_uart_tx_callback_t tx_callback; /**< TX callback */
    void* rx_context;                   /**< RX callback context */
    void* tx_context;                   /**< TX callback context */
    uint8_t rx_byte;  /**< Single byte RX buffer for IT mode */
    bool initialized; /**< Initialization flag */
} uart_data_t;

/**
 * \brief           UART instance data array
 */
static uart_data_t uart_data[UART_MAX_INSTANCES];

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
 * \brief           Get USART peripheral pointer by instance
 * \param[in]       instance: UART instance
 * \return          USART peripheral pointer
 */
static USART_TypeDef* uart_get_instance(hal_uart_instance_t instance) {
    switch (instance) {
        case HAL_UART_0:
            return USART1;
        case HAL_UART_1:
            return USART2;
        case HAL_UART_2:
            return USART3;
        default:
            return NULL;
    }
}

/**
 * \brief           Map Nexus word length to ST HAL word length
 */
static uint32_t map_wordlen(hal_uart_wordlen_t wordlen) {
    switch (wordlen) {
        case HAL_UART_WORDLEN_9:
            return UART_WORDLENGTH_9B;
        case HAL_UART_WORDLEN_8:
        default:
            return UART_WORDLENGTH_8B;
    }
}

/**
 * \brief           Map Nexus stop bits to ST HAL stop bits
 */
static uint32_t map_stopbits(hal_uart_stopbits_t stopbits) {
    switch (stopbits) {
        case HAL_UART_STOPBITS_2:
            return UART_STOPBITS_2;
        case HAL_UART_STOPBITS_1:
        default:
            return UART_STOPBITS_1;
    }
}

/**
 * \brief           Map Nexus parity to ST HAL parity
 */
static uint32_t map_parity(hal_uart_parity_t parity) {
    switch (parity) {
        case HAL_UART_PARITY_EVEN:
            return UART_PARITY_EVEN;
        case HAL_UART_PARITY_ODD:
            return UART_PARITY_ODD;
        case HAL_UART_PARITY_NONE:
        default:
            return UART_PARITY_NONE;
    }
}

/**
 * \brief           Map Nexus flow control to ST HAL flow control
 */
static uint32_t map_flowctrl(hal_uart_flowctrl_t flowctrl) {
    switch (flowctrl) {
        case HAL_UART_FLOWCTRL_RTS:
            return UART_HWCONTROL_RTS;
        case HAL_UART_FLOWCTRL_CTS:
            return UART_HWCONTROL_CTS;
        case HAL_UART_FLOWCTRL_RTS_CTS:
            return UART_HWCONTROL_RTS_CTS;
        case HAL_UART_FLOWCTRL_NONE:
        default:
            return UART_HWCONTROL_NONE;
    }
}

/**
 * \brief           Map ST HAL status to Nexus HAL status
 */
static hal_status_t map_hal_status(HAL_StatusTypeDef status) {
    switch (status) {
        case HAL_OK:
            return HAL_OK;
        case HAL_BUSY:
            return HAL_ERROR_BUSY;
        case HAL_TIMEOUT:
            return HAL_ERROR_TIMEOUT;
        case HAL_ERROR:
        default:
            return HAL_ERROR;
    }
}

/**
 * \brief           Map ST HAL UART error to Nexus HAL error
 */
static hal_status_t map_uart_error(uint32_t error) {
    if (error & HAL_UART_ERROR_PE) {
        return HAL_ERROR_PARITY;
    }
    if (error & HAL_UART_ERROR_FE) {
        return HAL_ERROR_FRAMING;
    }
    if (error & HAL_UART_ERROR_NE) {
        return HAL_ERROR_NOISE;
    }
    if (error & HAL_UART_ERROR_ORE) {
        return HAL_ERROR_OVERRUN;
    }
    return HAL_ERROR_IO;
}

/**
 * \brief           Get NVIC IRQ number for UART instance
 */
static IRQn_Type uart_get_irqn(hal_uart_instance_t instance) {
    switch (instance) {
        case HAL_UART_0:
            return USART1_IRQn;
        case HAL_UART_1:
            return USART2_IRQn;
        case HAL_UART_2:
            return USART3_IRQn;
        default:
            return USART1_IRQn;
    }
}

/*===========================================================================*/
/* ST HAL MSP Functions (Clock and GPIO Configuration)                        */
/*===========================================================================*/

/**
 * \brief           UART MSP Initialization
 * \note            This function is called by HAL_UART_Init() to configure
 *                  clocks and GPIO pins for the UART peripheral.
 * \param[in]       huart: UART handle pointer
 */
void HAL_UART_MspInit(UART_HandleTypeDef* huart) {
    GPIO_InitTypeDef gpio_init = {0};

    if (huart->Instance == USART1) {
        /* Enable USART1 clock (APB2) */
        __HAL_RCC_USART1_CLK_ENABLE();
        /* Enable GPIOA clock for TX (PA9) and RX (PA10) */
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* Configure USART1 TX (PA9) */
        gpio_init.Pin = GPIO_PIN_9;
        gpio_init.Mode = GPIO_MODE_AF_PP;
        gpio_init.Pull = GPIO_NOPULL;
        gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        gpio_init.Alternate = GPIO_AF7_USART1;
        HAL_GPIO_Init(GPIOA, &gpio_init);

        /* Configure USART1 RX (PA10) */
        gpio_init.Pin = GPIO_PIN_10;
        gpio_init.Mode = GPIO_MODE_AF_PP;
        gpio_init.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(GPIOA, &gpio_init);

    } else if (huart->Instance == USART2) {
        /* Enable USART2 clock (APB1) */
        __HAL_RCC_USART2_CLK_ENABLE();
        /* Enable GPIOA clock for TX (PA2) and RX (PA3) */
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* Configure USART2 TX (PA2) */
        gpio_init.Pin = GPIO_PIN_2;
        gpio_init.Mode = GPIO_MODE_AF_PP;
        gpio_init.Pull = GPIO_NOPULL;
        gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        gpio_init.Alternate = GPIO_AF7_USART2;
        HAL_GPIO_Init(GPIOA, &gpio_init);

        /* Configure USART2 RX (PA3) */
        gpio_init.Pin = GPIO_PIN_3;
        gpio_init.Mode = GPIO_MODE_AF_PP;
        gpio_init.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(GPIOA, &gpio_init);

    } else if (huart->Instance == USART3) {
        /* Enable USART3 clock (APB1) */
        __HAL_RCC_USART3_CLK_ENABLE();
        /* Enable GPIOB clock for TX (PB10) and RX (PB11) */
        __HAL_RCC_GPIOB_CLK_ENABLE();

        /* Configure USART3 TX (PB10) */
        gpio_init.Pin = GPIO_PIN_10;
        gpio_init.Mode = GPIO_MODE_AF_PP;
        gpio_init.Pull = GPIO_NOPULL;
        gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        gpio_init.Alternate = GPIO_AF7_USART3;
        HAL_GPIO_Init(GPIOB, &gpio_init);

        /* Configure USART3 RX (PB11) */
        gpio_init.Pin = GPIO_PIN_11;
        gpio_init.Mode = GPIO_MODE_AF_PP;
        gpio_init.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(GPIOB, &gpio_init);
    }
}

/**
 * \brief           UART MSP De-Initialization
 * \note            This function is called by HAL_UART_DeInit() to release
 *                  resources used by the UART peripheral.
 * \param[in]       huart: UART handle pointer
 */
void HAL_UART_MspDeInit(UART_HandleTypeDef* huart) {
    if (huart->Instance == USART1) {
        __HAL_RCC_USART1_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9 | GPIO_PIN_10);
    } else if (huart->Instance == USART2) {
        __HAL_RCC_USART2_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_2 | GPIO_PIN_3);
    } else if (huart->Instance == USART3) {
        __HAL_RCC_USART3_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOB, GPIO_PIN_10 | GPIO_PIN_11);
    }
}

/*===========================================================================*/
/* Public functions                                                           */
/*===========================================================================*/

hal_status_t hal_uart_init(hal_uart_instance_t instance,
                           const hal_uart_config_t* config) {
    uart_data_t* data;
    HAL_StatusTypeDef status;

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

    /* Configure ST HAL UART_HandleTypeDef */
    data->huart.Instance = uart_get_instance(instance);
    data->huart.Init.BaudRate = config->baudrate;
    data->huart.Init.WordLength = map_wordlen(config->wordlen);
    data->huart.Init.StopBits = map_stopbits(config->stopbits);
    data->huart.Init.Parity = map_parity(config->parity);
    data->huart.Init.HwFlowCtl = map_flowctrl(config->flowctrl);
    data->huart.Init.Mode = UART_MODE_TX_RX;
    data->huart.Init.OverSampling = UART_OVERSAMPLING_16;

    /* Call ST HAL UART Init */
    status = HAL_UART_Init(&data->huart);
    if (status != HAL_OK) {
        return map_hal_status(status);
    }

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
    HAL_StatusTypeDef status;

    if (data == NULL) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (!data->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Disable NVIC interrupt */
    HAL_NVIC_DisableIRQ(uart_get_irqn(instance));

    /* Call ST HAL UART DeInit */
    status = HAL_UART_DeInit(&data->huart);
    if (status != HAL_OK) {
        return map_hal_status(status);
    }

    /* Clear state */
    data->initialized = false;
    data->rx_callback = NULL;
    data->tx_callback = NULL;
    data->rx_context = NULL;
    data->tx_context = NULL;

    return HAL_OK;
}

hal_status_t hal_uart_transmit(hal_uart_instance_t instance,
                               const uint8_t* data_buf, size_t len,
                               uint32_t timeout_ms) {
    uart_data_t* data = uart_get_data(instance);
    HAL_StatusTypeDef status;

    if (data == NULL) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (data_buf == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }
    if (!data->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Use ST HAL blocking transmit */
    status = HAL_UART_Transmit(&data->huart, (uint8_t*)data_buf, (uint16_t)len,
                               timeout_ms);

    if (status != HAL_OK) {
        /* Check for specific UART errors */
        uint32_t error = HAL_UART_GetError(&data->huart);
        if (error != HAL_UART_ERROR_NONE) {
            return map_uart_error(error);
        }
        return map_hal_status(status);
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
    HAL_StatusTypeDef status;

    if (data == NULL) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (data_buf == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }
    if (!data->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Use ST HAL blocking receive */
    status =
        HAL_UART_Receive(&data->huart, data_buf, (uint16_t)len, timeout_ms);

    if (status != HAL_OK) {
        /* Check for specific UART errors */
        uint32_t error = HAL_UART_GetError(&data->huart);
        if (error != HAL_UART_ERROR_NONE) {
            return map_uart_error(error);
        }
        return map_hal_status(status);
    }

    /* Invoke RX callback for each byte if registered */
    if (data->rx_callback != NULL) {
        for (size_t i = 0; i < len; i++) {
            data->rx_callback(instance, data_buf[i], data->rx_context);
        }
    }

    return HAL_OK;
}

hal_status_t hal_uart_putc(hal_uart_instance_t instance, uint8_t byte) {
    return hal_uart_transmit(instance, &byte, 1, UART_DEFAULT_TIMEOUT);
}

hal_status_t hal_uart_getc(hal_uart_instance_t instance, uint8_t* byte,
                           uint32_t timeout_ms) {
    if (byte == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }
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

    if (callback != NULL) {
        /* Enable NVIC interrupt */
        HAL_NVIC_SetPriority(uart_get_irqn(instance), 5, 0);
        HAL_NVIC_EnableIRQ(uart_get_irqn(instance));

        /* Start interrupt-based receive for single byte */
        HAL_UART_Receive_IT(&data->huart, &data->rx_byte, 1);
    } else {
        /* Abort any ongoing IT receive */
        HAL_UART_AbortReceive_IT(&data->huart);
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

    if (callback != NULL) {
        /* Enable NVIC interrupt */
        HAL_NVIC_SetPriority(uart_get_irqn(instance), 5, 0);
        HAL_NVIC_EnableIRQ(uart_get_irqn(instance));
    }

    return HAL_OK;
}

/*===========================================================================*/
/* ST HAL Callback Implementations                                            */
/*===========================================================================*/

/**
 * \brief           ST HAL UART RX Complete Callback
 * \note            Called by HAL_UART_IRQHandler() when RX is complete
 * \param[in]       huart: UART handle pointer
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart) {
    /* Find which instance triggered the callback */
    for (hal_uart_instance_t i = 0; i < UART_MAX_INSTANCES; i++) {
        uart_data_t* data = &uart_data[i];
        if (data->initialized && &data->huart == huart) {
            /* Invoke user callback with received byte */
            if (data->rx_callback != NULL) {
                data->rx_callback(i, data->rx_byte, data->rx_context);
            }
            /* Re-enable receive for next byte */
            HAL_UART_Receive_IT(&data->huart, &data->rx_byte, 1);
            break;
        }
    }
}

/**
 * \brief           ST HAL UART TX Complete Callback
 * \note            Called by HAL_UART_IRQHandler() when TX is complete
 * \param[in]       huart: UART handle pointer
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart) {
    /* Find which instance triggered the callback */
    for (hal_uart_instance_t i = 0; i < UART_MAX_INSTANCES; i++) {
        uart_data_t* data = &uart_data[i];
        if (data->initialized && &data->huart == huart) {
            /* Invoke user callback */
            if (data->tx_callback != NULL) {
                data->tx_callback(i, data->tx_context);
            }
            break;
        }
    }
}

/**
 * \brief           ST HAL UART Error Callback
 * \note            Called by HAL_UART_IRQHandler() when an error occurs
 * \param[in]       huart: UART handle pointer
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef* huart) {
    /* Find which instance triggered the callback */
    for (hal_uart_instance_t i = 0; i < UART_MAX_INSTANCES; i++) {
        uart_data_t* data = &uart_data[i];
        if (data->initialized && &data->huart == huart) {
            /* Clear error flags and re-enable receive if callback is set */
            if (data->rx_callback != NULL) {
                HAL_UART_Receive_IT(&data->huart, &data->rx_byte, 1);
            }
            break;
        }
    }
}

/*===========================================================================*/
/* IRQ Handlers - Using ST HAL UART Handler                                   */
/*===========================================================================*/

/**
 * \brief           USART1 IRQ Handler
 */
void USART1_IRQHandler(void) {
    HAL_UART_IRQHandler(&uart_data[HAL_UART_0].huart);
}

/**
 * \brief           USART2 IRQ Handler
 */
void USART2_IRQHandler(void) {
    HAL_UART_IRQHandler(&uart_data[HAL_UART_1].huart);
}

/**
 * \brief           USART3 IRQ Handler
 */
void USART3_IRQHandler(void) {
    HAL_UART_IRQHandler(&uart_data[HAL_UART_2].huart);
}
